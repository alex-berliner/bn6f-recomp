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

from . import elfinfo  # noqa: E402
from .analysis import (ALL, ALLF, NZ, C, V, CodeCopy, Program, Unit, build, flag_effects,  # noqa: E402
                       mask_names, reg_effects, writes_pc)
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bn6f", default="bn6f")
    ap.add_argument("--out", default="cport/src/lifted")
    ap.add_argument("--inc", default="cport/include")
    ap.add_argument("--overrides", default="cport/src/game",
                    help="directory of hand-written C replacing lifted functions")
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
                    e2.append(("tail", x[1]))
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
    indirect_targets = {a for a in names if a in p.data_refs or any(
        (ct & ~1) == a for ct in ())}
    for site, ct in p.const_targets.items():
        indirect_targets.add(ct & ~1)
    indirect_in = 0
    indirect_ret = 0
    def _rw(ins):
        fr, fw = flag_effects(ins)
        rr, rw = reg_effects(ins)
        return fr | rr, fw | rw
    reads_writes = {a: _rw(ins) for (a, _t), ins in p.insns.items()}
    live_out_all: dict[int, dict[int, int]] = {}

    def unit_liveness(u: Unit, entry_in: dict, ret_live: int):
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
                for x in eff[a]:
                    kx = x[0]
                    if kx in ("fall", "local"):
                        out |= live_in[x[1]]
                    elif kx == "tail":
                        out |= entry_in[x[1]]
                    elif kx == "call":
                        tgt, ret = x[1], x[2]
                        out |= live_in[ret] if ret in u.insns else entry_in.get(ret, ALL)
                        extra |= entry_in[tgt] if tgt is not None else ALL
                    elif kx == "return":
                        out |= ret_live
                    elif kx == "jump":
                        out |= ALL
                if ins.cond != "al":
                    li = r | out | extra
                else:
                    li = r | ((out | extra) & ~w)
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
    outputs = {u.start: callers_live[u.start] & may_write[u.start] for u in p.units}

    # ── emission ────────────────────────────────────────────────────────
    addr_names: dict[int, str] = {}
    for a, nm in sorted(info.labels.items()):
        addr_names.setdefault(a, "A_" + cname(nm).removeprefix("bn_"))
    for nm, v in sorted(info.equates.items()):
        if 0x02000000 <= v < 0x10000000:
            addr_names.setdefault(v, "A_" + cname(nm).removeprefix("bn_"))
    ctx = Ctx(p=p, names=names, addr_names=addr_names, used_addr_syms=set(),
              used_equates={}, flags_in=flags_in, callers_live=callers_live,
              indirect_in=ALLF)
    overridden = set()
    if args.overrides and os.path.isdir(args.overrides):
        for f in sorted(os.listdir(args.overrides)):
            if f.endswith(".c"):
                txt = open(os.path.join(args.overrides, f)).read()
                overridden |= set(re.findall(r"^(?:IWRAM_CODE\s+|ARM_CODE\s+)*Next\s+(\w+)\s*\(\s*void\s*\)\s*\{",
                                             txt, re.M))
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
        save_def, load_def, decls = reg_locals(body)
        hdr = f"/* {u.name}: {u.start:08X}-{u.end:08X} {'THUMB' if u.thumb else 'ARM'}"
        if loc:
            hdr += f", bn6f {loc[0]}:{loc[1]}"
        hdr += " */"
        hdr += (f"\n/*   inputs:  {mask_names(inputs[u.start])}"
                f"\n *   outputs: {mask_names(outputs[u.start])}"
                f"\n *   clobbers (unused by callers): {mask_names(may_write[u.start] & ~outputs[u.start])} */")
        out = files["difftest_lifted" if is_over else fname]
        out.append(hdr)
        out.append(save_def)
        out.append(load_def)
        ents = sorted(u.entries)
        pfx = "L_" if is_over else ""
        if len(ents) == 1:
            out.append(f"Next {pfx}{names[u.start]}(void) {{")
            out.extend(decls)
            out.extend(body)
            out.append("}")
        else:
            out.append(f"static Next {pfx}U_{u.start:08X}(u32 entry) {{")
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
    for f in os.listdir(args.out):
        if f.endswith(".c"):
            os.remove(os.path.join(args.out, f))
    # Large modules are split into ~20k-line parts (at unit boundaries) so the
    # build parallelises and no single translation unit dominates.
    if "difftest_lifted" in files:
        files["difftest_lifted"] = ["#if defined(DIFFTEST)"] + files["difftest_lifted"] + ["#endif"]
    with open(os.path.join(args.inc, "difftest_pairs.h"), "w") as fh:
        fh.write("/* Generated by tools/lifter: hand-written functions and their lifted originals. */\n")
        for nm in over_pairs:
            fh.write(f"PAIR({nm})\n")
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
            with open(os.path.join(args.out, out_name + ".c"), "w") as fh:
                fh.write(f"/* Lifted from bn6f {fname}.s by tools/lifter. */\n")
                fh.write('#include "lifted.h"\n\n')
                fh.write("\n".join(part))
                fh.write("\n")
    os.makedirs(args.inc, exist_ok=True)
    with open(os.path.join(args.inc, "lifted.h"), "w") as fh:
        fh.write("/* Generated by tools/lifter: prototypes of all lifted entry points. */\n")
        fh.write("#pragma once\n#include \"cpu.h\"\n#include \"bn6f_syms.h\"\n\n")
        fh.write("\n".join(protos))
        fh.write("\n")
    with open(os.path.join(args.inc, "bn6f_syms.h"), "w") as fh:
        fh.write("/* Generated by tools/lifter: bn6f addresses and assembler constants used by lifted code. */\n")
        fh.write("#pragma once\n\n")
        inv = {v: k for k, v in addr_names.items()}
        for nm in sorted(ctx.used_addr_syms):
            fh.write(f"#define {nm} 0x{inv[nm]:08X}u\n")
        fh.write("\n")
        for nm, v in sorted(ctx.used_equates.items()):
            if nm in fn_names:
                continue
            fh.write(f"#define {nm} 0x{v & 0xFFFFFFFF:X}u\n")
    # Direct-mapped dispatch: one slot per halfword of the ROM code region and
    # of IWRAM, so an indirect branch is a single table load.
    rom_lo, rom_hi = 0x08000000, max(a for a in names if a >= 0x08000000) + 2
    with open(os.path.join(args.out, "dispatch_table.c"), "w") as fh:
        fh.write("/* Generated by tools/lifter: guest address -> lifted entry point. */\n")
        fh.write('#include "lifted.h"\n#include "dispatch.h"\n\n')
        fh.write(f"const u32 g_dispatch_rom_end = 0x{rom_hi:08X}u;\n")
        fh.write(f"const GuestFn g_dispatch_rom[0x{(rom_hi - rom_lo) >> 1:X}] = {{\n")
        for a in sorted(names):
            if rom_lo <= a < rom_hi:
                fh.write(f"    [0x{(a - rom_lo) >> 1:X}] = {names[a]},\n")
        fh.write("};\n\n")
        fh.write("const GuestFn g_dispatch_iwram[0x4000] = {\n")
        for a in sorted(names):
            if 0x03000000 <= a < 0x03008000:
                fh.write(f"    [0x{(a - 0x03000000) >> 1:X}] = {names[a]},\n")
        fh.write("};\n")
        other = [a for a in names if not (rom_lo <= a < rom_hi or 0x03000000 <= a < 0x03008000)]
        if other:
            raise SystemExit(f"entries outside dispatch regions: {[hex(a) for a in other]}")
    print(f"[lift] wrote {len(files)} files, {len(names)} entry points, "
          f"{stats['multi_entry_units']} multi-entry units, {n_over} hand-written "
          f"({time.time() - t1:.1f}s)")


if __name__ == "__main__":
    main()
