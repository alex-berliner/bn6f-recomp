#!/usr/bin/env python3
"""Lift the bn6f disassembly (via its byte-matching ELF) to C.

    python3 -m lifter.lift --bn6f bn6f --out cport/src/lifted
"""
from __future__ import annotations

import argparse
import re
import os
import sys
import time
from collections import defaultdict

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    __package__ = "lifter"

from . import abi_export, elfinfo  # noqa: E402
from .analysis import (ALL, ALLF, ALLR, NZ, C, V, RB, CodeCopy, Program, Unit, build,  # noqa: E402
                       flag_effects, mask_names, reg_effects, writes_pc)
from .emit import Ctx, UnitEmitter, cname, h  # noqa: E402

COPIES = [
    # m4a SoundInit CpuSets 0x100 words of SoundMainRAM into IWRAM and runs it
    # there; its ARM half is reached PC-relatively, so lift it at the RAM VMA.
    CodeCopy("SoundMainRAM_iwram", 0x03005700, 0x0814DC88, 0x400),
    # librfu STWI_init_all DMA-copies its ARM serial-interrupt handler
    # (byte_814CF54, 0x4B0 halfwords) to gRfuState+0xE8 and installs it as an
    # IRQ callback.
    CodeCopy("rfu_iwram", 0x030004D4, 0x0814CF54, 0x960),
]
COPY_SEEDS = [(0x030004D4, False)]


# ═══════════════════════════════════════════════════════════════════════
# Control-flow classification shared by liveness and emission
# ═══════════════════════════════════════════════════════════════════════

def classify(em: UnitEmitter, ins) -> list[tuple]:
    """List of control effects: ('fall', a) ('local', a) ('tail', a)
    ('call', target|None, ret) ('return',) ('jump',) ('stop',)."""
    u = em.u
    nxt = ins.addr + ins.size
    out = []
    k = ins.kind
    cond = ins.cond != "al"
    if k == "undef":
        return [("stop",)]
    if k == "b":
        if ins.link:
            return [("call", ins.target, nxt)]
        out.append(("local", ins.target) if ins.target in u.insns else ("tail", ins.target))
        if cond:
            out.append(("fall", nxt))
        return out
    pc_src = None
    if k == "bx":
        pc_src = ins.rm
    elif k == "dp" and ins.rd == 15 and ins.op not in ("tst", "teq", "cmp", "cmn"):
        o = ins.op2
        if ins.s:
            out.append(("stop",))
        elif ins.op == "mov" and o and not o.is_imm and o.rs is None and o.amount == 0 and o.shift == "lsl":
            pc_src = o.rm
        else:
            out.append(("jump",))
    elif k == "mem" and ins.load and ins.rd == 15:
        lit = em.p.literal(ins)
        if lit is not None:
            out.append(("local", lit & ~1) if (lit & ~1) in u.insns else ("tail", lit & ~1))
        elif ins.rn == 13 and not ins.pre:
            out.append(("return",))
        else:
            out.append(("jump",))
    elif k == "block" and ins.load and 15 in ins.regs:
        out.append(("stop",) if ins.user else ("return",))
    else:
        return [("fall", nxt)]
    if pc_src is not None:
        if pc_src == 14:
            out.append(("return",))
        else:
            ct = em.p.const_targets.get(ins.addr)
            ret = em.call_ret(ins)
            if ret is not None:
                ret &= ~1
                out.append(("call", (ct & ~1) if ct is not None else None, ret))
                return out + ([("fall", nxt)] if cond else [])
            if ct is not None:
                t = ct & ~1
                out.append(("local", t) if t in u.insns else ("tail", t))
            elif k == "bx" and em.is_pop_return(ins):
                out.append(("return",))
            else:
                out.append(("jump",))
    if cond:
        out.append(("fall", nxt))
    return out


COMMENT_RE = re.compile(r"/\*.*?\*/")
REG_RE = re.compile(r"\b(r1[0-2]|r[0-9]|lr)\b")
WRITE_RE = re.compile(r"\b(r1[0-2]|r[0-9]|lr)\s*=(?!=)")
FLAG_TOKEN = {"nz": re.compile(r"\bnz\b|COND_|FLAG_[NZ]"),
              "c": re.compile(r"\bc\b|COND_|ADDS|SUBS|ADCS|SBCS|_REG_C"),
              "v": re.compile(r"\bv\b|COND_|ADDS|SUBS|ADCS|SBCS")}
FLAG_WRITE = {"nz": re.compile(r"\bnz\s*=(?!=)"),
              "c": re.compile(r"\bc\s*=(?!=)|ADDS|SUBS|ADCS|SBCS|_REG_C"),
              "v": re.compile(r"\bv\s*=(?!=)|ADDS|SUBS|ADCS|SBCS")}
REG_ORDER = [f"r{i}" for i in range(13)] + ["lr"]
GLOBAL = {**{f"r{i}": f"R{i}" for i in range(13)}, "lr": "LR", "nz": "cpu.nz", "c": "cpu.c", "v": "cpu.v"}


RETURN_RE = re.compile(r"\bRETURN\(")
CALLM_RE = re.compile(r"CALLM\((\w+), (.+?), (\w+), (0x[0-9a-f]+), (0x[0-9a-f]+), (0x[0-9a-f]+), (0x[0-9a-f]+), ([01])\);")
BIT = {"nz": 1, "c": 2, "v": 4, **{f"r{i}": 1 << (3 + i) for i in range(13)}, "lr": 1 << 17}


def expand_calls(body: list[str], regs: list[str], flags: list[str], written: set, fwritten: set):
    """Replace CALLM placeholders with the call plus the register sync this
    site needs: store locally-modified values the callee may read or change,
    reload what it may change. Everything else stays in host registers."""
    names = regs + flags
    def sync(m):
        kind, fn, ra, save, load, mw, clob, chk = m.groups()
        save, load, mw, clob = int(save, 16), int(load, 16), int(mw, 16), int(clob, 16)
        # If the callee unwinds past us, registers it may write (including
        # ones it restores) already hold the right values in `cpu`; publish
        # only the others.
        unwind = [f"{GLOBAL[x]} = {x};" for x in names
                  if (x in written or x in fwritten) and not (mw & BIT[x])]
        st = [f"{GLOBAL[x]} = {x};" for x in names
              if (x in written or x in fwritten) and save & BIT[x]]
        # LR is always reloaded: the call itself sets it (to the return
        # address), whatever the callee does.
        ld = [f"{x} = {GLOBAL[x]};" for x in names if load & BIT[x] or x == "lr"]
        # Registers kept in host registers across the call (neither synced
        # nor reloaded) that the callee must leave alone: LIFT_TRACE builds
        # check it really did. (A dead register it clobbers is not reloaded
        # either, and may change.)
        kept = [x for x in regs if not (save & BIT[x]) and not (load & BIT[x]) and not (clob & BIT[x])
                and x != "lr"]
        chk_pre = chk_post = ""
        if kept:
            chk_pre = "SYNC_SNAP(" + ", ".join(f"{GLOBAL[x]}" for x in kept[:6]) + ")"
            chk_post = f"SYNC_CHECK({ra}, " + ", ".join(f"{GLOBAL[x]}" for x in kept[:6]) + ");"
        if kind == "X":
            call = (f"{{ u32 x_ = {fn}; {' '.join(st)} {chk_pre} GuestFn f_ = dispatch_fast(x_); "
                    f"if (f_) CALL_DIRECT(f_); else dispatch(x_);")
        else:
            st = [x for x in st if not x.startswith("LR = ")]     # the call sets LR
            lr = f"(u32){ra} | 1u" if kind[0] == "T" else f"(u32){ra}"
            run = f"CALL_LEAF({fn}, {ra})" if kind.endswith("L") else f"CALL_DIRECT({fn})"
            call = f"{{ {' '.join(st)} {chk_pre} LR = {lr}; {run};"
        if chk == "0":      # the callee provably returns here (LIFT_TRACE builds verify)
            return f"{call} {chk_post} {' '.join(ld)} RET_ASSERT({ra}); }}"
        return (f"{call} {chk_post} {' '.join(ld)} "
                f"if (__builtin_expect(cpu.ret != (u32){ra}, 0)) {{ {' '.join(unwind)} RET_MISMATCH({ra}); }} }}")
    return [CALLM_RE.sub(sync, ln) for ln in body]


def reg_locals(body: list[str]):
    """Guest registers/flags a function uses become C locals; SAVE writes the
    modified ones back to `cpu`, LOAD refreshes all of them after a call."""
    text = COMMENT_RE.sub("", "\n".join(body))
    used = set(REG_RE.findall(text))
    written = set(WRITE_RE.findall(text))
    fused = {f for f, rx in FLAG_TOKEN.items() if rx.search(text)}
    fwritten = {f for f, rx in FLAG_WRITE.items() if rx.search(text)}
    if "c" in fused or "v" in fused:
        fused |= {"c", "v"} & (fused | ({"c", "v"} if re.search("ADDS|SUBS|ADCS|SBCS", text) else set()))
    regs = [r for r in REG_ORDER if r in used]
    flags = [f for f in ("nz", "c", "v") if f in fused]
    body[:] = expand_calls(body, regs, flags, written, fwritten)
    save = [f"{GLOBAL[r]} = {r};" for r in regs if r in written] + \
           [f"{GLOBAL[f]} = {f};" for f in flags if f in fwritten]
    load = [f"{r} = {GLOBAL[r]};" for r in regs] + [f"{f} = {GLOBAL[f]};" for f in flags]
    decls = []
    allv = regs + flags
    if allv:
        decls.append("    u32 " + ", ".join(f"{x} = {GLOBAL[x]}" for x in allv) + ";")
    save_def = "#define SAVE do { " + " ".join(save) + " } while (0)"
    load_def = "#define LOAD do { " + " ".join(load) + " } while (0)"
    return save_def, load_def, decls


GENERATED: set[str] = set()
# hand-written C in --overrides replacing a lifted function: `Next NAME(void) {`
OVERRIDE_RE = re.compile(r"^(?:IWRAM_\w+\s+|ARM_CODE\s+)*Next\s+(\w+)\s*\(\s*void\s*\)\s*\{", re.M)


def overridden_names(directory: str | None) -> set[str]:
    out: set[str] = set()
    if directory and os.path.isdir(directory):
        for f in sorted(os.listdir(directory)):
            if f.endswith(".c"):
                out |= set(OVERRIDE_RE.findall(open(os.path.join(directory, f)).read()))
    return out


def write_if_changed(path: str, text: str):
    GENERATED.add(os.path.abspath(path))
    try:
        with open(path) as fh:
            if fh.read() == text:
                return False
    except FileNotFoundError:
        pass
    with open(path, "w") as fh:
        fh.write(text)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bn6f", default="bn6f")
    ap.add_argument("--out", default="cport/src/lifted")
    ap.add_argument("--inc", default="cport/include")
    ap.add_argument("--overrides", default="cport/src/game",
                    help="directory of hand-written C replacing lifted functions")
    ap.add_argument("--export-abi", metavar="OUT.json",
                    help="write register contracts and call sites as JSON (docs/C_CALLING_CONVENTION.md 3.1) "
                         "and stop without writing C")
    ap.add_argument("--call-edges", nargs="+", metavar="EDGES",
                    help="with --export-abi: call edges observed by mgba_run --call-edges (tools/abi/call_edges.py)")
    args = ap.parse_args()
    t0 = time.time()
    info = elfinfo.load(args.bn6f)
    p = build(info, COPIES, COPY_SEEDS)
    print(f"[lift] {len(p.units)} units, {len(p.insns)} insns ({time.time() - t0:.1f}s)")
    for w in p.warnings[:20]:
        print("  warn:", w)

    # ── entries & labels ────────────────────────────────────────────────
    entries: dict[int, set[int]] = defaultdict(set)   # unit start -> entry addrs
    by_start = {u.start: u for u in p.units}

    def unit_of(a):
        return p.unit_at(a)

    def add_entry(a):
        uu = unit_of(a)
        if uu is None:
            raise SystemExit(f"entry {a:#x} not in any unit")
        if a not in uu.insns:
            raise SystemExit(f"entry {a:#x} not a decoded instruction of {uu.name}")
        entries[uu.start].add(a)

    for u in p.units:
        entries[u.start].add(u.start)
    ems = {}
    for u in p.units:
        em = UnitEmitter(None, u, {})  # ctx filled later
        em.p = p
        em.info = info
        ems[u.start] = em
    for a in p.data_refs:
        if unit_of(a) is not None and a in unit_of(a).insns:
            add_entry(a)
    for a, t in p.entries.items():
        uu = unit_of(a)
        if uu is not None and a in uu.insns:
            add_entry(a)
    effects: dict[int, dict[int, list]] = {}
    for u in p.units:
        em = ems[u.start]
        eff = {}
        for a, ins in u.insns.items():
            e = classify(em, ins)
            # fall off the end of the decoded unit body
            e2 = []
            for x in e:
                if x[0] == "fall" and x[1] not in u.insns:
                    e2.append(("return",) if em.pop_into_bx_veneer(ins) is not None else ("tail", x[1]))
                elif x[0] == "local":
                    u.labels.add(x[1])
                    e2.append(x)
                else:
                    e2.append(x)
                if x[0] == "call" and x[2] != ins.addr + ins.size:
                    if x[2] in u.insns:
                        u.labels.add(x[2])
            eff[a] = e2
            for x in e2:
                if x[0] == "tail":
                    add_entry(x[1])
                elif x[0] == "call":
                    if x[1] is not None:
                        add_entry(x[1])
                    if x[2] != ins.addr + ins.size and x[2] not in u.insns:
                        add_entry(x[2])
        effects[u.start] = eff
    for u in p.units:
        u.entries = entries[u.start]
        if len(u.entries) > 1:
            u.labels |= u.entries
        u.labels |= {a for a in p.data_refs if a in u.insns}

    # ── names ───────────────────────────────────────────────────────────
    names: dict[int, str] = {}
    used: set[str] = set()
    for u in sorted(p.units, key=lambda x: x.start):
        for a in sorted(u.entries):
            base = u.name if a == u.start else (info.labels.get(a) or f"loc_{a:08X}")
            nm = cname(base)
            if nm in used:
                nm = f"{nm}_{a:08X}"
            used.add(nm)
            names[a] = nm

    # ── flag liveness (inter-procedural fixpoint) ───────────────────────
    flags_in = {a: 0 for a in names}
    callers_live = {u.start: 0 for u in p.units}
    indirect_targets = {a for a in names if a in p.data_refs}
    for site, ct in p.const_targets.items():
        indirect_targets.add(ct & ~1)
    # Code pointers can also sit unaligned inside byte streams (e.g. the
    # cutscene script command "call native routine" embeds one after its
    # command byte). Any entry whose address (with its mode bit) appears
    # anywhere in the ROM is treated as possibly called indirectly; a
    # coincidental match only makes the analysis more conservative.
    want = {}
    for a in names:
        if a >= 0x08000000:
            want[(a | 1) if p.entries.get(a, True) else a] = a
    rom = p.info.rom if os.environ.get("LIFT_NO_BYTESCAN") != "1" else b""
    found = 0
    for off in range(0, len(rom) - 3):
        v = rom[off] | rom[off + 1] << 8 | rom[off + 2] << 16 | rom[off + 3] << 24 if rom[off + 3] == 0x08 else 0
        if v in want and want[v] not in indirect_targets:
            indirect_targets.add(want[v])
            found += 1
    print(f"[lift] {found} more entries referenced by unaligned/byte data")
    indirect_in = 0
    indirect_ret = 0
    def _rw(ins):
        fr, fw = flag_effects(ins)
        rr, rw = reg_effects(ins)
        return fr | rr, fw | rw
    reads_writes = {a: _rw(ins) for (a, _t), ins in p.insns.items()}
    live_out_all: dict[int, dict[int, int]] = {}

    # (0) Must-write: flags/registers every returning path of a function
    #     (from each entry) definitely writes. A call kills liveness of
    #     those; without this, flags live after any call stay live back
    #     through every earlier call.
    must_write = {a: ALL for a in names}

    def unit_mustwrite(u: Unit, e: int) -> int:
        eff = effects[u.start]
        din: dict[int, int] = {e: 0}
        work = [e]
        exits = ALL
        any_exit = False
        while work:
            a = work.pop()
            ins = u.insns[a]
            d = din[a]
            _r, w = reads_writes[a]
            if ins.cond != "al":
                w = 0
            for x in eff[a]:
                kx = x[0]
                if kx == "call":
                    d2 = d | w | (must_write[x[1]] if x[1] is not None else 0)
                    nxt = x[2]
                    if nxt in u.insns:
                        succ = [(nxt, d2)]
                    else:
                        exits &= d2 | must_write.get(nxt, 0)
                        any_exit = True
                        succ = []
                elif kx in ("fall", "local"):
                    succ = [(x[1], d | w)]
                elif kx == "tail":
                    exits &= d | w | must_write.get(x[1], 0)
                    any_exit = True
                    succ = []
                elif kx in ("return", "jump"):
                    exits &= d | w
                    any_exit = True
                    succ = []
                else:
                    succ = []
                for t, v in succ:
                    if t not in din:
                        din[t] = v
                        work.append(t)
                    elif din[t] & ~v:
                        din[t] &= v
                        work.append(t)
        return exits if any_exit else ALL

    while True:
        changed = False
        for u in p.units:
            for e in u.entries:
                m = unit_mustwrite(u, e)
                if m != must_write[e]:
                    must_write[e] = m
                    changed = True
        if not changed:
            break

    def unit_liveness(u: Unit, entry_in: dict, ret_live: int, seen=None):
        # seen(site, live_in, entry_in, ret_live): what the targets observed at a computed
        # call or jump read (--export-abi with --call-edges); without it, everything
        eff = effects[u.start]
        addrs = sorted(u.insns, reverse=True)
        live_in: dict[int, int] = {a: 0 for a in addrs}
        live_out: dict[int, int] = {}
        changed = True
        while changed:
            changed = False
            for a in addrs:
                ins = u.insns[a]
                r, w = reads_writes[a]
                out = 0
                extra = 0
                prop = 0      # what flows backward past this instruction
                for x in eff[a]:
                    kx = x[0]
                    if kx in ("fall", "local"):
                        v = live_in[x[1]]
                        out |= v; prop |= v
                    elif kx == "tail":
                        v = entry_in[x[1]]
                        out |= v; prop |= v
                    elif kx == "call":
                        tgt, ret = x[1], x[2]
                        after = live_in[ret] if ret in u.insns else entry_in.get(ret, ALL)
                        out |= after
                        prop |= after & ~(must_write[tgt] if tgt is not None else 0)
                        extra |= entry_in[tgt] if tgt is not None else \
                            ALL if seen is None else seen(a, live_in, entry_in, ret_live)
                    elif kx == "return":
                        out |= ret_live; prop |= ret_live
                    elif kx == "jump":
                        v = ALL if seen is None else seen(a, live_in, entry_in, ret_live)
                        out |= v; prop |= v
                if ins.cond != "al":
                    li = r | prop | out | extra
                else:
                    li = r | ((prop | extra) & ~w)
                live_out[a] = out
                if li != live_in[a]:
                    live_in[a] = li
                    changed = True
        return live_in, live_out

    # (1) Liveness for code generation: flags/registers live after each
    #     instruction, given what callers need after each function returns.
    it = 0
    while True:
        it += 1
        changed = False
        new_callers = {s_: callers_live[s_] for s_ in callers_live}
        new_ind_ret = indirect_ret
        for u in p.units:
            live_in, live_out = unit_liveness(u, flags_in, callers_live[u.start])
            live_out_all[u.start] = live_out
            for e in u.entries:
                if live_in[e] & ~flags_in[e]:
                    flags_in[e] |= live_in[e]
                    changed = True
            for a, eff in effects[u.start].items():
                for x in eff:
                    if x[0] == "call":
                        tgt, ret = x[1], x[2]
                        after = live_in[ret] if ret in u.insns else flags_in.get(ret, ALL)
                        if tgt is not None:
                            new_callers[unit_of(tgt).start] |= after
                        else:
                            new_ind_ret |= after
                    elif x[0] == "tail":
                        new_callers[unit_of(x[1]).start] |= callers_live[u.start]
                    elif x[0] == "jump":
                        new_ind_ret |= callers_live[u.start]
        for u in p.units:
            if any(e in indirect_targets for e in u.entries):
                new_callers[u.start] |= new_ind_ret
        if new_callers != callers_live or new_ind_ret != indirect_ret:
            changed = True
        callers_live, indirect_ret = new_callers, new_ind_ret
        if not changed:
            break
    print(f"[lift] liveness converged after {it} iterations")
    why = os.environ.get("LIFT_WHY")
    if why:
        tgt_addr = next((a for a, n in names.items() if n == why), None)
        for u in p.units:
            live_in, live_out = unit_liveness(u, flags_in, callers_live[u.start])
            for a, eff in effects[u.start].items():
                for x in eff:
                    if x[0] == "call" and x[1] == tgt_addr:
                        print(f"  call from {names.get(u.start, hex(u.start))} at {a:#x}: live after = {mask_names(live_out[a])}")
                    if x[0] == "tail" and x[1] == tgt_addr:
                        print(f"  tail from {names.get(u.start, hex(u.start))} at {a:#x}: callers need {mask_names(callers_live[u.start])}")
        print(f"  indirect target: {tgt_addr in indirect_targets}; indirect_ret = {mask_names(indirect_ret)}")

    # (2) Signatures. Inputs: what a function (or its callees) reads before
    #     writing, ignoring values merely passed through to its callers.
    #     May-write: registers it or its callees can modify. Outputs:
    #     may-write registers that callers read after it returns.
    inputs = {a: 0 for a in names}
    while True:
        changed = False
        for u in p.units:
            live_in, _ = unit_liveness(u, inputs, 0)
            for e in u.entries:
                if live_in[e] & ~inputs[e]:
                    inputs[e] |= live_in[e]
                    changed = True
        if not changed:
            break
    may_write = {u.start: 0 for u in p.units}
    local_w = {}
    for u in p.units:
        m = 0
        for a, ins in u.insns.items():
            fr, fw = flag_effects(ins)
            rr, rw = reg_effects(ins._replace() if False else ins) if ins.cond == "al" else \
                reg_effects(ins.__class__(**{**ins.__dict__, "cond": "al"}))
            m |= fw | rw
            if ins.kind == "swi":
                m |= RB(0) | RB(1) | RB(2) | RB(3)     # BIOS results come back in r0-r3
        local_w[u.start] = m
    while True:
        changed = False
        for u in p.units:
            m = may_write[u.start] | local_w[u.start]
            for eff in effects[u.start].values():
                for x in eff:
                    if x[0] in ("call", "tail"):
                        t = x[1]
                        m |= may_write[unit_of(t).start] if t is not None else ALL
                    elif x[0] == "jump":
                        m |= ALL
            if m != may_write[u.start]:
                may_write[u.start] = m
                changed = True
        if not changed:
            break
    # (3) Preserved registers: written but provably restored before every
    #     return (push/pop save-restore, including hi registers moved through
    #     low ones). Symbolic stack tracking per entry; callees' preserved
    #     sets come from the same fixpoint (optimistic start).
    preserved = {a: ALLR for a in names}

    # may_unwind: some path can return somewhere other than the entry LR
    # (popping a caller's frame, returning through a computed address,
    # indirect jumps/calls, or calling something that may unwind). Calls to
    # such functions keep full register sync (see call_masks).
    may_unwind = {a: False for a in names}
    em_of = {u.start: ems[u.start] for u in p.units}

    def unit_preserved(u: Unit, e: int) -> int:
        nonlocal_unwind[0] = False
        eff = effects[u.start]
        # state: (regs tuple of 15 symbols or None, sp offset or None, stack dict)
        init = (tuple(range(15)), 0, {})
        states: dict[int, tuple] = {e: init}
        work = [e]
        result = ALLR
        seen_ret = False
        def merge(a, b):
            if a[1] != b[1] or a[1] is None:
                return (tuple(x if x == y else None for x, y in zip(a[0], b[0])), None, {})
            regs = tuple(x if x == y else None for x, y in zip(a[0], b[0]))
            stack = {k: v for k, v in a[2].items() if b[2].get(k) == v}
            return (regs, a[1], stack)
        def kept(regs):
            m = 0
            for n in range(15):
                if n != 13 and regs[n] == n:
                    m |= RB(n)
            return m
        steps = 0
        while work and steps < 20000:
            steps += 1
            a = work.pop()
            ins = u.insns[a]
            regs, spo, stack = list(states[a][0]), states[a][1], dict(states[a][2])
            k = ins.kind
            ret_regs = None
            regs0, spo0, stack0 = states[a]
            if ins.cond != "al":
                # conditional (ARM): may or may not execute; forget what it may touch
                _r2, wr = reg_effects(ins.__class__(**{**ins.__dict__, "cond": "al"}))
                if k == "swi":
                    wr |= RB(0) | RB(1) | RB(2) | RB(3)
                for n in range(15):
                    if n != 13 and wr & RB(n):
                        regs[n] = None
                if (k in ("block", "mem") and ins.rn == 13 and (ins.wb or not ins.pre)) or \
                        (k == "dp" and ins.rd == 13) or (k == "mem" and not ins.load and ins.rn == 13):
                    spo = None
                    stack = {}
            elif k == "block" and ins.rn == 13 and ins.wb and spo is not None:
                n = len(ins.regs)
                if not ins.load and ins.mode == "db":
                    spo -= 4 * n
                    for i, r in enumerate(ins.regs):
                        stack[spo + 4 * i] = regs[r] if r < 15 else None
                elif ins.load and ins.mode == "ia":
                    for i, r in enumerate(ins.regs):
                        if r < 15:
                            regs[r] = stack.get(spo + 4 * i)
                    spo += 4 * n
                    if 15 in ins.regs:
                        ret_regs = regs
                else:
                    spo = None
            elif k == "dp" and ins.rd == 13:
                o = ins.op2
                if ins.rn == 13 and o.is_imm and ins.op in ("add", "sub") and spo is not None:
                    spo += o.imm if ins.op == "add" else -o.imm
                else:
                    spo = None
            elif k == "mem" and ins.rn == 13 and ins.op2.is_imm and ins.pre and not ins.wb and spo is not None and ins.width == 4:
                off = spo + (ins.op2.imm if ins.up else -ins.op2.imm)
                if ins.load:
                    if ins.rd < 15:
                        regs[ins.rd] = stack.get(off)
                else:
                    stack[off] = regs[ins.rd] if ins.rd < 15 else None
            elif k == "dp" and ins.op == "mov" and ins.op2 and not ins.op2.is_imm and ins.op2.rs is None \
                    and ins.op2.amount == 0 and ins.op2.shift == "lsl" and ins.rd < 15 and ins.op2.rm < 15 \
                    and ins.cond == "al":
                regs[ins.rd] = regs[ins.op2.rm]
            else:
                _r, w = reads_writes[a]
                if k == "swi":
                    w |= RB(0) | RB(1) | RB(2) | RB(3)   # BIOS results (reg_effects claims none definite)
                if ins.cond != "al":
                    _r2, wr = reg_effects(ins.__class__(**{**ins.__dict__, "cond": "al"}))
                    w = wr
                for n in range(15):
                    if n != 13 and w & RB(n):
                        regs[n] = None
                if k == "mem" and (ins.wb or not ins.pre) and ins.rn == 13:
                    spo = None
                if k == "block" and ins.rn == 13 and ins.wb:
                    spo = None
            succs = []
            for x in effects[u.start][a]:
                kx = x[0]
                if kx in ("fall", "local"):
                    succs.append(x[1])
                elif kx == "call":
                    tgt, retp = x[1], x[2]
                    if (tgt is None and not IND_NO_UNWIND) or (tgt is not None and may_unwind.get(tgt, True)) \
                            or retp not in u.insns:
                        nonlocal_unwind[0] = True
                    keep = (preserved[tgt] if tgt is not None else 0) & ~RB(14)   # the call writes LR
                    regs2 = [regs[n] if (n == 13 or keep & RB(n)) else None for n in range(15)]
                    if retp in u.insns:
                        st = (tuple(regs2), spo, stack)
                        if retp not in states:
                            states[retp] = st; work.append(retp)
                        else:
                            m = merge(states[retp], st)
                            if m != states[retp]:
                                states[retp] = m; work.append(retp)
                    else:
                        result &= kept(regs2) & preserved.get(retp, 0)
                        seen_ret = True
                elif kx == "return":
                    result &= kept(ret_regs if ret_regs is not None else regs)
                    seen_ret = True
                    rt = return_target(ins, regs0, spo0, stack0)
                    if rt != 14:
                        nonlocal_unwind[0] = True
                        own_unwind.add(u.start)
                        if os.environ.get("LIFT_DEBUG_UNWIND") == names.get(u.start):
                            print(f"    {a:#x} {ins.kind} rt={rt} spo={spo0} stack={stack0} regs={regs0}")
                elif kx == "tail":
                    result &= kept(regs) & preserved.get(x[1], 0)
                    seen_ret = True
                    if regs[14] != 14 or may_unwind.get(x[1], True):
                        nonlocal_unwind[0] = True
                elif kx == "jump":
                    result = 0
                    seen_ret = True
                    nonlocal_unwind[0] = True
            st = (tuple(regs), spo, stack)
            for t in succs:
                if t not in states:
                    states[t] = st; work.append(t)
                else:
                    m = merge(states[t], st)
                    if m != states[t]:
                        states[t] = m; work.append(t)
        if work:
            nonlocal_unwind[0] = True
            return 0          # gave up
        return result if seen_ret else ALLR

    nonlocal_unwind = [False]
    own_unwind = set()
    # Indirect calls are assumed not to unwind (none does on the test routes;
    # LIFT_TRACE builds log any return-address mismatch). LIFT_IND_UNWIND=1
    # restores the fully conservative assumption.
    IND_NO_UNWIND = os.environ.get("LIFT_IND_UNWIND") != "1"

    def return_target(ins, regs0, spo0, stack0):
        """Symbol (14 = entry LR) of the address a returning instruction goes to."""
        k = ins.kind
        if k == "block" and ins.load and ins.rn == 13 and spo0 is not None:
            rx = 15 if 15 in ins.regs else em_of_cur[0].pop_into_bx_veneer(ins)
            if rx is None:
                return None
            return stack0.get(spo0 + 4 * sorted(ins.regs).index(rx))
        if k == "bx" and ins.rm < 15:
            return regs0[ins.rm]
        if k == "dp" and ins.rd == 15 and ins.op2 is not None and not ins.op2.is_imm and ins.op2.rm < 15:
            return regs0[ins.op2.rm]
        if k == "mem" and ins.load and ins.rd == 15 and ins.rn == 13 and not ins.pre and spo0 is not None:
            return stack0.get(spo0)
        return None

    em_of_cur = [None]
    while True:
        changed = False
        for u in p.units:
            em_of_cur[0] = em_of[u.start]
            for e in u.entries:
                m = unit_preserved(u, e) & preserved[e]
                if m != preserved[e]:
                    preserved[e] = m
                    changed = True
                if nonlocal_unwind[0] and not may_unwind[e]:
                    may_unwind[e] = True
                    changed = True
        if not changed:
            break
    print(f"[lift] {sum(may_unwind.values())} of {len(may_unwind)} entries may unwind past their caller "
          f"({len(own_unwind)} units through their own returns)")

    # Units whose callers may check the return address (cpu.ret): those that
    # may unwind, can be called through a pointer (indirect call sites always
    # check) or are tail-called (they return on behalf of their tail-caller).
    # The others return without storing it (RETURN_NR; LIFT_TRACE builds
    # still store it so RET_ASSERT can verify the analysis).
    tail_targets = set()
    for u in p.units:
        for a, effs in effects[u.start].items():
            for x in effs:
                if x[0] == "tail" or (x[0] == "fall" and x[1] not in u.insns):
                    tail_targets.add(x[1])
    # Units that can only return DONE: no tail call, computed jump or fall
    # into the next function. Calls to them need no trampoline.
    tail_free = {}
    for u in p.units:
        free = True
        for a, effs in effects[u.start].items():
            for x in effs:
                if x[0] in ("tail", "jump") or (x[0] == "fall" and x[1] not in u.insns) \
                        or (x[0] == "call" and x[2] not in u.insns):
                    free = False
        for e in u.entries:
            tail_free[e] = free
    print(f"[lift] {sum(tail_free.values())} of {len(tail_free)} entries never tail-call")
    def needs_ret(u):
        return any(may_unwind.get(e, True) or e in indirect_targets or e in tail_targets for e in u.entries)
    print(f"[lift] {sum(not needs_ret(u) for u in p.units)} of {len(p.units)} units return without "
          f"publishing their return address")
    if os.environ.get("LIFT_DEBUG_UNWIND"):
        for st in sorted(own_unwind)[:40]:
            print(f"  own unwind: {names.get(st, hex(st))}")
    unit_preserved_mask = {u.start: min((preserved[e] for e in u.entries), key=lambda m: bin(m).count("1"))
                           for u in p.units}
    outputs = {u.start: callers_live[u.start] & may_write[u.start] for u in p.units}
    use_preserved = os.environ.get("LIFT_NO_PRESERVED") != "1"
    def pres(t):
        return preserved[t] if use_preserved else 0

    use_live_sync = os.environ.get("LIFT_NO_LIVE_SYNC") != "1"

    if args.export_abi:
        summary = abi_export.write(args.export_abi, abi_export.Analysis(
            p=p, info=info, names=names, effects=effects, reads_writes=reads_writes, flags_in=flags_in,
            callers_live=callers_live,
            indirect_ret=indirect_ret, indirect_targets=indirect_targets, tail_targets=tail_targets,
            inputs=inputs, may_write=may_write, preserved=preserved, must_write=must_write,
            may_unwind=may_unwind, needs_ret=needs_ret, unit_liveness=unit_liveness, local_w=local_w,
            overridden=overridden_names(args.overrides)), args.call_edges)
        print(f"[lift] wrote {args.export_abi}: " + ", ".join(
            f"{k} {len(v) if isinstance(v, list) else v}" for k, v in summary.items()))
        return

    def call_masks(t, call_addr, unit_start):
        """(save, load, may_write, clobbers) for a call site: registers/flags
        to store before the call, to reload after it, that the callee may
        change, and that it may leave changed."""
        if t is None:
            return ALL, ALL, ALL, ALL
        mw = may_write[unit_of(t).start]
        mwp = mw & ~pres(t)
        if may_unwind.get(t, True) or not use_live_sync:
            return inputs[t] | mwp, mwp, mw, mwp
        # The callee returns here: only what is live afterwards matters.
        live = live_out_all[unit_start].get(call_addr, ALL)
        return inputs[t] | (mwp & ~must_write[t] & live), mwp & live, mw, mwp

    # ── emission ────────────────────────────────────────────────────────
    addr_names: dict[int, str] = {}
    for a, nm in sorted(info.labels.items()):
        addr_names.setdefault(a, "A_" + cname(nm).removeprefix("bn_"))
    for nm, v in sorted(info.equates.items()):
        if 0x02000000 <= v < 0x10000000:
            addr_names.setdefault(v, "A_" + cname(nm).removeprefix("bn_"))
    # Dirty registers: those whose local copy may differ from `cpu` at each
    # call site (forward may-analysis per unit). Locals are loaded from
    # `cpu` on entry, so a call site need not store an input that has not
    # changed since the last sync. LIFT_NO_DIRTY=1 stores every written one.
    use_dirty = os.environ.get("LIFT_NO_DIRTY") != "1"
    dirty_cache: dict[int, dict[int, int]] = {}

    def unit_dirty(u: Unit) -> dict[int, int]:
        eff = effects[u.start]
        din = {e: 0 for e in u.entries}
        work = list(u.entries)
        while work:
            a = work.pop()
            d = din[a]
            ins = u.insns[a]
            _r, w = reads_writes[a]
            if ins.kind == "swi":
                d_next = 0                  # SWI(): SAVE, BIOS call, LOAD of every local
            else:
                d_next = d | w
            for x in eff[a]:
                kx = x[0]
                if kx == "call":
                    tgt, nxt = x[1], x[2]
                    save, load, _mw, clob = call_masks(tgt, a, u.start)
                    v = ((d & ~save) | (clob & ~load)) & ~load & ~RB(14)   # LR is reloaded
                elif kx in ("fall", "local"):
                    nxt, v = x[1], d_next
                else:
                    continue
                if nxt not in u.insns:
                    continue
                if nxt not in din:
                    din[nxt] = v
                    work.append(nxt)
                elif v & ~din[nxt]:
                    din[nxt] |= v
                    work.append(nxt)
        return din

    def dirty_at(u: Unit, call_addr: int) -> int:
        if not use_dirty:
            return ALL
        if u.start not in dirty_cache:
            dirty_cache[u.start] = unit_dirty(u)
        return dirty_cache[u.start].get(call_addr, ALL)

    ctx = Ctx(p=p, names=names, addr_names=addr_names, used_addr_syms=set(),
              used_equates={}, flags_in=flags_in, callers_live=callers_live,
              indirect_in=ALLF,
              call_masks=call_masks, may_unwind=may_unwind, tail_free=tail_free,
              dirty_at=dirty_at)
    iwram_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "iwram.txt")
    iwram_names: dict[str, str] = {}      # name -> "thumb" | "arm"
    if os.path.exists(iwram_path):
        for line in open(iwram_path):
            f = line.split("#")[0].split()
            if f:
                mode = f[1] if len(f) > 1 else "thumb"
                if mode not in ("thumb", "arm"):
                    sys.exit(f"iwram.txt: {f[0]}: unknown mode {mode!r}")
                iwram_names[f[0]] = mode
    overridden = overridden_names(args.overrides)
    files: dict[str, list[str]] = defaultdict(list)
    protos: list[str] = []
    fn_names = set(names.values())
    t1 = time.time()
    stats = defaultdict(int)
    n_over = 0
    over_pairs: list[str] = []
    for u in sorted(p.units, key=lambda x: x.start):
        ent_names = {names[e] for e in u.entries}
        is_over = bool(ent_names & overridden)
        if is_over:
            missing = ent_names - overridden
            if missing:
                raise SystemExit(f"override of {u.name} must define all its entries: missing {sorted(missing)}")
            for e in sorted(u.entries):
                protos.append(f"Next {names[e]}(void); /* {e:08X}, hand-written */")
                protos.append(f"Next L_{names[e]}(void); /* lifted original, DIFFTEST builds */")
                over_pairs.append(names[e])
            n_over += 1
        em = UnitEmitter(ctx, u, live_out_all[u.start])
        loc = info.lines.get(u.start)
        if any(c.vma <= u.start < c.vma + c.size for c in COPIES):
            fname = "ram_copies"
        else:
            fname = os.path.splitext(loc[0])[0] if loc else "misc"
        body = em.body()
        if not is_over and not needs_ret(u):      # (difftest compares cpu.ret of overridden ones)
            body = [RETURN_RE.sub("RETURN_NR(", ln) for ln in body]
        save_def, load_def, decls = reg_locals(body)
        hdr = f"/* {u.name}: {u.start:08X}-{u.end:08X} {'THUMB' if u.thumb else 'ARM'}"
        if loc:
            hdr += f", bn6f {loc[0]}:{loc[1]}"
        hdr += " */"
        hdr += (f"\n/*   inputs:  {mask_names(inputs[u.start])}"
                f"\n *   outputs: {mask_names(outputs[u.start])}"
                f"\n *   clobbers (unused by callers): {mask_names(may_write[u.start] & ~outputs[u.start] & ~preserved[u.start])}"
                f"\n *   preserved: {mask_names(preserved[u.start] & may_write[u.start])} */")
        out = files["difftest_lifted" if is_over else fname]
        out.append(hdr)
        out.append(save_def)
        out.append(load_def)
        ents = sorted(u.entries)
        pfx = "L_" if is_over else ""
        # hot units listed in iwram.txt run from IWRAM (Thumb, or ARM when
        # listed so), like the game's own IWRAM routines
        modes = [iwram_names[names[e]] for e in ents if names[e] in iwram_names]
        attr = "" if is_over or not modes else ("IWRAM_LIFTED_ARM " if "arm" in modes else "IWRAM_LIFTED ")
        stats["iwram_units"] += bool(attr)
        if len(ents) == 1:
            out.append(f"{attr}Next {pfx}{names[u.start]}(void) {{")
            out.extend(decls)
            out.extend(body)
            out.append("}")
        else:
            out.append(f"static {attr}Next {pfx}U_{u.start:08X}(u32 entry) {{")
            out.extend(decls)
            out.append("    switch (entry) {")
            for e in ents:
                if e != u.start:
                    out.append(f"        case {h(e)}: goto L_{e:08X};")
            out.append("    }")
            out.extend(body)
            out.append("}")
            for e in ents:
                out.append(f"Next {pfx}{names[e]}(void) {{ return {pfx}U_{u.start:08X}({h(e)}); }}")
            stats["multi_entry_units"] += 1
        out.append("#undef SAVE")
        out.append("#undef LOAD")
        out.append("")
        for e in ents:
            protos.append(f"Next {names[e]}(void); /* {e:08X} */")
    os.makedirs(args.out, exist_ok=True)
    written: set[str] = set()
    # Large modules are split into ~20k-line parts (at unit boundaries) so the
    # build parallelises and no single translation unit dominates.
    _buf = []
    _buf.append("/* Generated by tools/lifter: hand-written functions and their lifted originals. */\n")
    for nm in over_pairs:
        _buf.append(f"PAIR({nm})\n")
    write_if_changed(os.path.join(args.inc, "difftest_pairs.h"), "".join(_buf))
    for fname, lines in files.items():
        parts, cur = [], []
        for ln in lines:
            if len(cur) > 20000 and ln.startswith("/* ") and ln.endswith(" */") and "bn6f " in ln:
                parts.append(cur)
                cur = []
            cur.append(ln)
        parts.append(cur)
        for k, part in enumerate(parts):
            out_name = fname if len(parts) == 1 else f"{fname}_{k:02d}"
            if fname == "difftest_lifted":   # each part guarded on its own (one guard split across parts broke the build)
                part = ["#if defined(DIFFTEST)", *part, "#endif"]
            write_if_changed(os.path.join(args.out, out_name + ".c"),
                             f"/* Lifted from bn6f {fname}.s by tools/lifter. */\n"
                             '#include "lifted.h"\n\n' + "\n".join(part) + "\n")
            written.add(out_name + ".c")
    os.makedirs(args.inc, exist_ok=True)
    rom_lo, rom_hi = 0x08000000, max(a for a in names if a >= 0x08000000) + 2
    _buf = []
    _buf.append("/* Generated by tools/lifter: prototypes of all lifted entry points. */\n")
    _buf.append("#pragma once\n#include \"cpu.h\"\n#include \"bn6f_syms.h\"\n\n")
    _buf.append("\n".join(protos))
    _buf.append("\n\n/* Inline fast path of dispatch_lookup for ROM code addresses. */\n")
    _buf.append(f"#define DISPATCH_ROM_END 0x{rom_hi:08X}u\n")
    _buf.append("extern const GuestFn g_dispatch_rom[];\n")
    _buf.append("static inline GuestFn dispatch_fast(u32 a) {\n"
                "    a = (a & ~1u) - 0x08000000u;\n"
                "    return a < DISPATCH_ROM_END - 0x08000000u ? g_dispatch_rom[a >> 1] : 0;\n}\n")
    write_if_changed(os.path.join(args.inc, "lifted.h"), "".join(_buf))
    _buf = []
    _buf.append("/* Generated by tools/lifter: bn6f addresses and assembler constants used by lifted code. */\n")
    _buf.append("#pragma once\n\n")
    inv = {v: k for k, v in addr_names.items()}
    for nm in sorted(ctx.used_addr_syms):
        _buf.append(f"#define {nm} 0x{inv[nm]:08X}u\n")
    _buf.append("\n")
    for nm, v in sorted(ctx.used_equates.items()):
        if nm in fn_names:
            continue
        _buf.append(f"#define {nm} 0x{v & 0xFFFFFFFF:X}u\n")
    write_if_changed(os.path.join(args.inc, "bn6f_syms.h"), "".join(_buf))
    # Every other assembler constant (struct offsets such as oToolkit_JoypadPtr, flag
    # values, ...), for hand-written code: models copy these names from the assembly,
    # and bn6f_syms.h holds only those the lifted code uses. Included by game.h only.
    _buf = []
    _buf.append("/* Generated by tools/lifter: the other assembler constants of bn6f, for hand-written code. */\n")
    _buf.append("#pragma once\n\n")
    for nm, v in sorted(info.equates.items()):
        if nm in ctx.used_equates or nm in fn_names or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", nm):
            continue
        _buf.append(f"#ifndef {nm}\n#define {nm} 0x{v & 0xFFFFFFFF:X}u\n#endif\n")
    write_if_changed(os.path.join(args.inc, "bn6f_equates.h"), "".join(_buf))
    # Direct-mapped dispatch: one slot per halfword of the ROM code region and
    # of IWRAM, so an indirect branch is a single table load.
    _buf = []
    _buf.append("/* Generated by tools/lifter: guest address -> lifted entry point. */\n")
    _buf.append('#include "lifted.h"\n#include "dispatch.h"\n\n')
    _buf.append(f"const u32 g_dispatch_rom_end = 0x{rom_hi:08X}u;\n")
    _buf.append(f"const GuestFn g_dispatch_rom[0x{(rom_hi - rom_lo) >> 1:X}] = {{\n")
    for a in sorted(names):
        if rom_lo <= a < rom_hi:
            _buf.append(f"    [0x{(a - rom_lo) >> 1:X}] = {names[a]},\n")
    _buf.append("};\n\n")
    _buf.append("const GuestFn g_dispatch_iwram[0x4000] = {\n")
    for a in sorted(names):
        if 0x03000000 <= a < 0x03008000:
            _buf.append(f"    [0x{(a - 0x03000000) >> 1:X}] = {names[a]},\n")
    _buf.append("};\n")
    other = [a for a in names if not (rom_lo <= a < rom_hi or 0x03000000 <= a < 0x03008000)]
    if other:
        raise SystemExit(f"entries outside dispatch regions: {[hex(a) for a in other]}")
    write_if_changed(os.path.join(args.out, "dispatch_table.c"), "".join(_buf))
    # Files from an earlier run that this run did not produce (e.g. after the
    # split points moved) would duplicate definitions.
    for fn in os.listdir(args.out):
        path = os.path.abspath(os.path.join(args.out, fn))
        if fn.endswith(".c") and path not in GENERATED:
            os.remove(path)
            print(f"[lift] removed stale {fn}")
    print(f"[lift] wrote {len(files)} files, {len(names)} entry points, "
          f"{stats['multi_entry_units']} multi-entry units, {stats['iwram_units']} in IWRAM, {n_over} hand-written "
          f"({time.time() - t1:.1f}s)")


if __name__ == "__main__":
    main()
