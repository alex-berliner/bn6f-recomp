"""The lifter's register contracts and call sites as JSON (docs/C_CALLING_CONVENTION.md
3.1), narrowed by observed call edges (3.2) and further refinements, with a proposed C
signature for every readable function (3.3, the mechanical part).

    lift.py --export-abi build/abi/graph.json [--call-edges tools/traces/coverage/*.edges]

Masks are lists of names: r0..r12, lr, and the flags nz, c, v (N and Z share one bit).

Per entry: inputs (read before written, also by callees), may_write (it or its callees
may change), preserved (written but restored), must_write (written on every returning
path), callers_live (what any caller may read after it returns: the unit's), outputs
(callers_live & may_write: the lifted header's "outputs"), may_unwind, needs_ret,
indirect_target, tail_target, readable (a hand-written C replacement exists).

Per call site: caller unit, kind (call: direct bl; tail: direct jump or falling into
the next unit; indirect: call through a register; jump: computed jump through a
register or table), target, return point, and what is live after it (for tail and
jump sites: what the caller's callers read).

The analysis is re-run here (`Engine`) with refinements that each make the contracts
narrower; `Model` switches them on one at a time, so the summary can say what each buys:

- edges: call edges (`mgba_run --call-edges` lines "SITE TARGET LR COUNT") say an
  indirect site went to TARGET with LR set, i.e. TARGET returns to LR. The lifter gives
  every indirect target what is live after *every* indirect site (`indirect_ret`) and
  lets every computed call or jump read and write everything. With edges, a unit whose
  indirect-target entries were all observed gets the liveness at its edges' return
  points, and an observed site reads and writes only what its targets do. Evidence from
  the recorded routes, not proof: an unobserved run may reach other functions.
- swi: BIOS calls read and write what they really do (SWI below) instead of r0-r3.
- per_site: a call site reads what the callee itself reads (its inputs) plus what passes
  through it unchanged, not what the callee's other callers need after it.
- pass_through: saving a register (a push of a register the function restores, or the
  `mov rL, rH` that moves a high register to be pushed) is not a use of it, and a
  register a callee restores passes through the call.

`*_observed` fields use edges alone; `*_final` fields use every refinement, and
`proposal_final` also infers flag results (callers that only test one condition get a
`bool`). `ceiling_blockers` is an upper bound for more route coverage: every computed
call or jump no route ran, and every caller no route showed, touches nothing.
"""
from __future__ import annotations

import json
import os
import re
from collections import defaultdict
from dataclasses import dataclass
from typing import Callable

from .analysis import ALL, ALLF, C, COND_READS, NZ, RB, V, Program, Unit, flag_effects, mask_names, reg_effects
from .elfinfo import ElfInfo

RETURN = "a return through a register (target == lr)"
ARG_REGS = [n for n in range(13)]          # r0..r12 can carry arguments and results
REGS = sum(RB(n) for n in ARG_REGS)


def _m(names: str) -> int:
    return sum(RB(int(n[1:])) for n in names.split())


# BIOS calls BN6 makes: (reads, may write, writes on every path), from
# cport/src/runtime/swi.c, which follows the BIOS routines register for register (the
# CPU oracle checked it against the real BIOS: tools/oracle_triage.md). The lifter
# assumes r0-r3 read and possibly written. LZ77UnCompVram's register results are not
# modelled there, so it keeps "may write r0-r3".
SWI: dict[int, tuple[int, int, int]] = {
    0x06: (_m("r0 r1"), _m("r0 r1 r3"), _m("r0 r1 r3")),          # Div
    0x08: (_m("r0"), _m("r0 r1 r3"), _m("r0 r1 r3")),             # Sqrt
    0x0A: (_m("r0 r1"), _m("r0 r3"), _m("r0 r3")),                # ArcTan2
    0x0B: (_m("r0 r1 r2"), _m("r0 r1 r3"), _m("r3")),             # CpuSet
    0x0C: (_m("r0 r1 r2"), _m("r0 r1 r3"), 0),                    # CpuFastSet
    0x11: (_m("r0 r1"), _m("r0 r1 r3"), _m("r0 r1")),             # LZ77UnCompWram
    0x12: (_m("r0 r1"), _m("r0 r1 r2 r3"), 0),                    # LZ77UnCompVram
}

# condition -> the bool a caller tests (the condition and its negation are one result)
COND_RESULT = {"eq": "z", "ne": "z", "cs": "c", "cc": "c", "mi": "n", "pl": "n", "vs": "v", "vc": "v",
               "hi": "hi", "ls": "hi", "ge": "ge", "lt": "ge", "gt": "gt", "le": "gt"}
RESULT_FLAGS = {"z": NZ, "n": NZ, "c": C, "v": V, "hi": NZ | C, "ge": NZ | V, "gt": NZ | V}


@dataclass
class Analysis:
    """What lift.py computed (its main() locals), handed over for the export."""
    p: Program
    info: ElfInfo
    names: dict[int, str]
    effects: dict[int, dict[int, list]]
    reads_writes: dict[int, tuple[int, int]]
    flags_in: dict[int, int]
    callers_live: dict[int, int]
    indirect_ret: int
    indirect_targets: set[int]
    tail_targets: set[int]
    inputs: dict[int, int]
    may_write: dict[int, int]
    preserved: dict[int, int]
    must_write: dict[int, int]
    may_unwind: dict[int, bool]
    needs_ret: Callable[[Unit], bool]
    unit_liveness: Callable[..., tuple[dict[int, int], dict[int, int]]]
    local_w: dict[int, int]
    overridden: set[str]


@dataclass
class Edge:
    """One observed (site -> target) with the LR it went with."""
    site: int
    target: int        # with the Thumb bit
    lr: int
    count: int


@dataclass(frozen=True)
class Model:
    """Which refinements an Engine run uses (module docstring)."""
    edges: bool = False
    swi: bool = False
    per_site: bool = False
    pass_through: bool = False
    ceiling: bool = False     # what the routes did not show touches nothing: unobserved computed
                              # calls and jumps read and write nothing, unknown callers read nothing


def regs(m: int) -> list[str]:
    s = mask_names(m)
    return [] if s == "-" else s.split()


def h(a: int | None) -> str | None:
    return None if a is None else f"0x{a:08X}"


def read_edges(paths: list[str], rom_sha1: str | None = None) -> list[Edge]:
    """Merge mgba_run --call-edges files (counts add up). With ROM_SHA1 (the original ROM's SHA-1),
    a file observed on another ROM is refused: its addresses would be stale."""
    merged: dict[tuple[int, int, int], int] = defaultdict(int)
    for path in paths:
        for line in open(path):
            seen_on = re.search(r"original ROM ([0-9a-f]+)", line) if line.startswith("# observed on:") else None
            if seen_on and rom_sha1 and not rom_sha1.startswith(seen_on[1]):
                raise SystemExit(f"{path}: observed on original ROM {seen_on[1]}, not on this one ({rom_sha1[:16]}); "
                                 "observe the routes again (tools/abi/call_edges.py)")
            if line.startswith("#") or not line.strip():
                continue
            site, target, lr, n = line.split()
            merged[int(site, 16), int(target, 16), int(lr, 16)] += int(n)
    return [Edge(s, t, lr, n) for (s, t, lr), n in sorted(merged.items())]


def indirect_sites(a: Analysis) -> list[tuple[Unit, int, str, int | None]]:
    """(unit, site address, 'indirect' | 'jump', return point) of every computed branch."""
    out = []
    for u in a.p.units:
        for addr, eff in a.effects[u.start].items():
            for x in eff:
                if x[0] == "call" and x[1] is None:
                    out.append((u, addr, "indirect", x[2]))
                elif x[0] == "jump":
                    out.append((u, addr, "jump", None))
    return out


def observed_targets(a: Analysis, edges: list[Edge]) -> dict[int, list[tuple[str, int]]]:
    """Site -> what the routes saw it reach: ("entry", t), ("local", t) (a jump within
    its unit) or ("return", 0) (a jump to LR). Sites with a target that is no lifted entry
    are left out (they stay unknown)."""
    unit_of = a.p.unit_at
    kind_at = {addr: (u, kind) for u, addr, kind, _ret in indirect_sites(a)}
    out: dict[int, list[tuple[str, int]]] = defaultdict(list)
    bad = set()
    for e in edges:
        if e.site not in kind_at:
            continue
        su, kind = kind_at[e.site]
        t = e.target & ~1
        tu = unit_of(t)
        if tu is None or t not in tu.insns:
            bad.add(e.site)
        elif kind == "jump" and (e.lr & ~1) == t:
            out[e.site].append(("return", 0))
        elif kind == "jump" and tu.start == su.start:
            out[e.site].append(("local", t))
        elif t in a.names:
            out[e.site].append(("entry", t))
        else:
            bad.add(e.site)
    return {k: v for k, v in out.items() if k not in bad}


def edge_plan(a: Analysis, edges: list[Edge]) -> list[tuple[Edge, int | None, Unit | None, str]]:
    """(edge, return point, target unit, how): where each edge returns to. A call site's
    own return point; for a jump, the LR it went with when that is the return point of a
    call to the jumping unit (LR still holds the unit's own return address: a veneer
    `bx r3`, a dispatcher jumping before any call), otherwise the jumping unit's callers."""
    p, effects = a.p, a.effects
    unit_of = p.unit_at
    kind_at = {addr: (u, kind, ret) for u, addr, kind, ret in indirect_sites(a)}
    calls_returning_to: dict[int, set[int]] = defaultdict(set)    # return point -> called units
    for u in p.units:
        for eff in effects[u.start].values():
            for x in eff:
                if x[0] == "call" and x[1] is not None:
                    calls_returning_to[x[2]].add(unit_of(x[1]).start)
    plan = []
    for e in edges:
        site_u, kind, ret = kind_at.get(e.site, (unit_of(e.site), "unlisted", None))
        tu = unit_of(e.target & ~1)
        if tu is None or (e.target & ~1) not in tu.insns:
            plan.append((e, None, None, "target is not lifted code"))
        elif site_u is not None and tu.start == site_u.start:
            plan.append((e, None, tu, "jump within the unit"))
        elif kind == "indirect":
            plan.append((e, ret, tu, "call return point"))
        elif (e.lr & ~1) == (e.target & ~1):
            plan.append((e, None, tu, RETURN))
        elif site_u is not None and site_u.start in calls_returning_to.get(e.lr & ~1, ()):
            plan.append((e, e.lr & ~1, tu, "lr: return point of a call to the jumping unit"))
        else:
            plan.append((e, None, tu, "the jumping unit's callers"))
    return plan


class Engine:
    """lift.py's register analysis (must-write, may-write, inputs, callers' liveness),
    re-run with the refinements a Model switches on. Model() reproduces the lifter."""

    def __init__(self, a: Analysis, model: Model, edges: list[Edge] | None):
        self.a, self.model = a, model
        self.unit_of = a.p.unit_at
        self.order = {u.start: sorted(u.insns, reverse=True) for u in a.p.units}
        self.sites = observed_targets(a, edges) if model.edges and edges else {}
        self.plan = edge_plan(a, edges) if model.edges and edges else []
        self.rw, self.local_w = self._effects()
        self.must_write = self._must_write()
        self.may_write = self._may_write()
        self.inputs = self._inputs()
        self.callers, self.entry_live, self.indirect_ret, self.edge_after, self.fully_observed = \
            self._callers(0 if model.ceiling else None)

    # ── per-instruction effects ─────────────────────────────────────────
    def _effects(self) -> tuple[dict[int, tuple[int, int]], dict[int, int]]:
        a, model = self.a, self.model
        rw = a.reads_writes
        local_w = a.local_w
        if not (model.swi or model.pass_through):
            return rw, local_w
        rw = dict(rw)
        local_w = dict(local_w)
        for u in a.p.units:
            saved = 0
            if model.pass_through and not self._reads_own_frame(u):
                saved = ALL
                for e in u.entries:
                    saved &= a.preserved[e]
            has_swi = False
            for addr, ins in u.insns.items():
                r, w = rw[addr]
                if model.swi and ins.kind == "swi" and ins.imm in SWI:
                    reads, _may, must = SWI[ins.imm]
                    r = (r & ~REGS) | reads
                    w = (w & ~REGS) | (must if ins.cond == "al" else 0)
                    has_swi = True
                if saved:
                    r &= ~self._save_reads(u, addr, ins, saved)
                rw[addr] = (r, w)
            if has_swi:
                local_w[u.start] = self._unit_writes(u)
        return rw, local_w

    @staticmethod
    def _reads_own_frame(u: Unit) -> bool:
        """Whether the unit may load a pushed register back from the stack other than by
        popping it: an sp-relative load at or above its local frame (`sub sp, #n`)."""
        frame = 0
        for ins in u.insns.values():
            o = ins.op2
            if ins.kind == "dp" and ins.op == "sub" and ins.rd == 13 and ins.rn == 13 and o is not None and o.is_imm:
                frame = max(frame, o.imm)
        for ins in u.insns.values():
            if ins.kind == "mem" and ins.load and ins.rn == 13 and ins.pre:
                o = ins.op2
                if o is None or not o.is_imm or not ins.up or o.imm >= frame:
                    return True
            if ins.kind == "block" and ins.load and ins.rn == 13 and not ins.wb:
                return True
        return False

    def _save_reads(self, u: Unit, addr: int, ins, saved: int) -> int:
        """Registers `ins` reads only to save them for a restore: a push of registers the
        unit restores, or `mov rL, rH` of a restored high register into a low register
        that one of the next three instructions pushes."""
        if ins.kind == "block" and not ins.load and ins.rn == 13 and ins.wb:
            return sum(RB(n) for n in ins.regs if n < 13 and saved & RB(n))
        o = ins.op2
        if ins.kind == "dp" and ins.op == "mov" and ins.cond == "al" and o is not None and not o.is_imm \
                and o.rs is None and o.amount == 0 and o.shift == "lsl" and ins.rd < 8 and 8 <= o.rm < 13 \
                and saved & RB(o.rm) and self.a.local_w[u.start] & RB(o.rm):
            nxt = addr
            for _ in range(3):
                nxt += ins.size
                n = u.insns.get(nxt)
                if n is None:
                    break
                if n.kind == "block" and not n.load and n.rn == 13 and n.wb and ins.rd in n.regs:
                    return RB(o.rm)
        return 0

    def _unit_writes(self, u: Unit) -> int:
        """lift.py's local may-write of a unit, with the BIOS calls' real results."""
        m = 0
        for ins in u.insns.values():
            fr, fw = flag_effects(ins)
            _rr, rw_ = reg_effects(ins if ins.cond == "al" else ins.__class__(**{**ins.__dict__, "cond": "al"}))
            m |= fw | rw_
            if ins.kind == "swi":
                m |= SWI[ins.imm][1] if ins.imm in SWI else RB(0) | RB(1) | RB(2) | RB(3)
        return m

    def kill(self, t: int) -> int:
        """What a call to entry t overwrites for sure (restored registers pass through)."""
        m = self.must_write[t]
        return m & ~self.a.preserved[t] if self.model.pass_through else m

    # ── fixpoints (lift.py's, same order and transfer) ─────────────────
    def _must_write(self) -> dict[int, int]:
        a, rw = self.a, self.rw
        must = {e: ALL for e in a.names}

        def unit_mustwrite(u: Unit, e: int) -> int:
            eff = a.effects[u.start]
            din: dict[int, int] = {e: 0}
            work = [e]
            exits = ALL
            any_exit = False
            while work:
                addr = work.pop()
                ins = u.insns[addr]
                d = din[addr]
                w = rw[addr][1] if ins.cond == "al" else 0
                for x in eff[addr]:
                    kx = x[0]
                    succ = []
                    if kx == "call":
                        d2 = d | w | (must[x[1]] if x[1] is not None else 0)
                        if x[2] in u.insns:
                            succ = [(x[2], d2)]
                        else:
                            exits &= d2 | must.get(x[2], 0)
                            any_exit = True
                    elif kx in ("fall", "local"):
                        succ = [(x[1], d | w)]
                    elif kx == "tail":
                        exits &= d | w | must.get(x[1], 0)
                        any_exit = True
                    elif kx in ("return", "jump"):
                        exits &= d | w
                        any_exit = True
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
            for u in a.p.units:
                for e in u.entries:
                    m = unit_mustwrite(u, e)
                    if m != must[e]:
                        must[e] = m
                        changed = True
            if not changed:
                return must

    def _may_write(self) -> dict[int, int]:
        """Computed calls and jumps write what their observed targets may write (else everything)."""
        a, unit_of = self.a, self.unit_of
        mw = {u.start: 0 for u in a.p.units}
        while True:
            changed = False
            for u in a.p.units:
                m = mw[u.start] | self.local_w[u.start]
                for addr, eff in a.effects[u.start].items():
                    for x in eff:
                        if x[0] in ("call", "tail") and x[1] is not None:
                            m |= mw[unit_of(x[1]).start]
                        elif x[0] in ("call", "jump"):
                            targets = self.sites.get(addr)
                            if targets is None and not self.model.ceiling:
                                m |= ALL
                            for kind, t in targets or ():
                                if kind == "entry":
                                    m |= mw[unit_of(t).start]
                if m != mw[u.start]:
                    mw[u.start] = m
                    changed = True
            if not changed:
                return mw

    def _seen(self, site: int, live_in: dict, entry_in: dict, ret_live: int, gen: dict | None, call: bool) -> int:
        """What a computed call or jump reads: everything, or what its observed targets read."""
        targets = self.sites.get(site)
        if targets is None:
            return 0 if self.model.ceiling else ALL
        m = 0
        for kind, t in targets:
            if kind == "entry":
                if gen is None:
                    m |= entry_in.get(t, ALL)
                else:
                    m |= gen.get(t, ALL) | (0 if call else ret_live & ~self.kill(t))
            elif kind == "local":
                m |= live_in[t]
            else:
                m |= ret_live
        return m

    def unit_liveness(self, u: Unit, entry_in: dict, ret_live: int, gen: dict | None = None) -> dict[int, int]:
        """lift.py's unit_liveness. With `gen` (per_site), a call reads gen[callee] plus what
        is live after it and passes through; otherwise entry_in[callee] (the lifter's)."""
        eff = self.a.effects[u.start]
        rw = self.rw
        must = self.must_write
        live_in: dict[int, int] = dict.fromkeys(self.order[u.start], 0)
        changed = True
        while changed:
            changed = False
            for addr in self.order[u.start]:
                ins = u.insns[addr]
                r, w = rw[addr]
                out = extra = prop = 0
                for x in eff[addr]:
                    kx = x[0]
                    if kx == "fall" or kx == "local":
                        v = live_in[x[1]]
                        out |= v
                        prop |= v
                    elif kx == "tail":
                        t = x[1]
                        v = entry_in[t] if gen is None else gen[t] | (ret_live & ~self.kill(t))
                        out |= v
                        prop |= v
                    elif kx == "call":
                        t, ret = x[1], x[2]
                        after = live_in[ret] if ret in u.insns else entry_in.get(ret, ALL)
                        out |= after
                        if t is not None:
                            prop |= after & ~(must[t] if gen is None else self.kill(t))
                            extra |= entry_in[t] if gen is None else gen[t]
                        else:
                            prop |= after
                            extra |= self._seen(addr, live_in, entry_in, ret_live, gen, True)
                    elif kx == "return":
                        out |= ret_live
                        prop |= ret_live
                    elif kx == "jump":
                        v = self._seen(addr, live_in, entry_in, ret_live, gen, False)
                        out |= v
                        prop |= v
                li = r | prop | out | extra if ins.cond != "al" else r | ((prop | extra) & ~w)
                if li != live_in[addr]:
                    live_in[addr] = li
                    changed = True
        return live_in

    def _inputs(self) -> dict[int, int]:
        inputs = {e: 0 for e in self.a.names}
        gen = inputs if self.model.per_site else None
        while True:
            changed = False
            for u in self.a.p.units:
                live_in = self.unit_liveness(u, inputs, 0, gen)
                for e in u.entries:
                    if live_in[e] & ~inputs[e]:
                        inputs[e] |= live_in[e]
                        changed = True
            if not changed:
                return inputs

    def _callers(self, unknown: int | None = None):
        """lift.py's liveness fixpoint (1): what callers may read after each unit returns.
        With edges, indirect targets whose entries were all observed get the liveness at
        their edges' return points; `unknown` (0: the ceiling) replaces indirect_ret for
        the others. Returns (callers_live, entry liveness, indirect_ret, [(plan item, live
        after)], fully_observed(unit))."""
        a, unit_of, effects = self.a, self.unit_of, self.a.effects
        gen = self.inputs if self.model.per_site else None
        entry_live = {e: 0 for e in a.names}
        callers = {u.start: 0 for u in a.p.units}
        ind_ret = 0
        want: dict[int, set[int]] = defaultdict(set)       # unit start -> return points to record
        for _e, r, _tu, _how in self.plan:
            if r is not None and unit_of(r) is not None and r in unit_of(r).insns:
                want[unit_of(r).start].add(r)
        observed: dict[int, list] = defaultdict(list)        # target unit start -> plan items
        seen_entries = set()
        for item in self.plan:
            if item[2] is not None and item[3] != RETURN:
                observed[item[2].start].append(item)
                seen_entries.add(item[0].target & ~1)

        def fully_observed(u: Unit) -> bool:
            return bool(self.plan) and all(e in seen_entries for e in u.entries if e in a.indirect_targets)

        live_at: dict[int, int] = {}

        def after(item) -> int:
            e, r, _tu, how = item
            if how in ("jump within the unit", RETURN):
                return 0
            if r is not None:
                return live_at[r] if r in live_at else entry_live.get(r, ALL)
            su = unit_of(e.site)
            return callers[su.start] if su is not None else ALL

        while True:
            changed = False
            new_callers = dict(callers)
            new_ind = ind_ret
            for u in a.p.units:
                live_in = self.unit_liveness(u, entry_live, callers[u.start], gen)
                for e in u.entries:
                    if live_in[e] & ~entry_live[e]:
                        entry_live[e] |= live_in[e]
                        changed = True
                for r in want.get(u.start, ()):
                    live_at[r] = live_in[r]
                for eff in effects[u.start].values():
                    for x in eff:
                        if x[0] == "call":
                            tgt, ret = x[1], x[2]
                            aft = live_in[ret] if ret in u.insns else entry_live.get(ret, ALL)
                            if tgt is not None:
                                new_callers[unit_of(tgt).start] |= aft
                            else:
                                new_ind |= aft
                        elif x[0] == "tail":
                            new_callers[unit_of(x[1]).start] |= callers[u.start]
                        elif x[0] == "jump":
                            new_ind |= callers[u.start]
            for u in a.p.units:
                if any(e in a.indirect_targets for e in u.entries):
                    if fully_observed(u):
                        for item in observed[u.start]:
                            new_callers[u.start] |= after(item)
                    else:
                        new_callers[u.start] |= new_ind if unknown is None else unknown
                elif u.start in observed:      # reached through a pointer the lifter did not see
                    for item in observed[u.start]:
                        new_callers[u.start] |= after(item)
            if new_callers != callers or new_ind != ind_ret:
                changed = True
            callers, ind_ret = new_callers, new_ind
            if not changed:
                break
        return callers, entry_live, ind_ret, [(item, after(item)) for item in self.plan], fully_observed

    def outs(self, e: int, callers_live: int | None = None) -> tuple[int, int]:
        """(registers, flags) callers read after entry e that it may change."""
        u = self.unit_of(e)
        cl = self.callers[u.start] if callers_live is None else callers_live
        mw = self.may_write[u.start]
        return cl & mw & ~self.a.preserved[e] & REGS, cl & mw & ALLF


# ── flag results ────────────────────────────────────────────────────────
class FlagReaders:
    """Which conditions the known callers of a unit test on the flags it leaves: every
    direct call site, tail-caller (through its own callers) and observed indirect edge,
    walking forward from the return point until the flags are overwritten."""

    def __init__(self, eng: Engine):
        self.eng = eng
        a, unit_of = eng.a, eng.unit_of
        self.calls_into: dict[int, list[tuple[Unit, int]]] = defaultdict(list)
        self.tails_into: dict[int, list[Unit]] = defaultdict(list)
        for u in a.p.units:
            for eff in a.effects[u.start].values():
                for x in eff:
                    if x[0] == "call" and x[1] is not None:
                        self.calls_into[unit_of(x[1]).start].append((u, x[2]))
                    elif x[0] == "tail":
                        self.tails_into[unit_of(x[1]).start].append(u)
        self.edges_into: dict[int, list] = defaultdict(list)
        for (e, r, tu, how), _after in eng.edge_after:
            if tu is not None:
                self.edges_into[tu.start].append((e, r, how))
        self.memo: dict[tuple[int, int], frozenset[str]] = {}

    def of_unit(self, ustart: int, bits: int, depth: int = 0) -> frozenset[str]:
        key = (ustart, bits)
        if key in self.memo:
            return self.memo[key]
        self.memo[key] = frozenset()          # a cycle adds nothing new
        eng, a = self.eng, self.eng.a
        out: set[str] = set()
        u = eng.unit_of(ustart)
        if depth > 8:
            out.add("too deep")
        else:
            if any(e in a.indirect_targets for e in u.entries) and not eng.fully_observed(u):
                out.add("unknown callers")
            for cu, ret in self.calls_into[ustart]:
                if ret in cu.insns:
                    out |= self.walk(cu, ret, bits, depth)
                else:
                    out.add("returns into another unit")
            for cu in self.tails_into[ustart]:
                out |= self.of_unit(cu.start, bits, depth + 1)
            for e, r, how in self.edges_into[ustart]:
                if r is not None:
                    ru = eng.unit_of(r)
                    out |= self.walk(ru, r, bits, depth)
                elif how == "the jumping unit's callers":
                    out |= self.of_unit(eng.unit_of(e.site).start, bits, depth + 1)
        self.memo[key] = frozenset(out)
        return self.memo[key]

    def walk(self, cu: Unit, start: int, bits: int, depth: int) -> set[str]:
        eng = self.eng
        effects = eng.a.effects[cu.start]
        out: set[str] = set()
        stack = [(start, bits)]
        seen = set()
        while stack:
            addr, b = stack.pop()
            if (addr, b) in seen or addr not in cu.insns:
                continue
            seen.add((addr, b))
            ins = cu.insns[addr]
            fr, fw = flag_effects(ins)
            if fr & b:
                cond = COND_READS.get(ins.cond, 0)
                out.add(ins.cond if ins.cond != "al" and not (fr & ~cond & b) else f"used as data ({ins.kind})")
            nb = b & ~fw if ins.cond == "al" else b
            if not nb:
                continue
            for x in effects[addr]:
                k = x[0]
                if k in ("fall", "local"):
                    stack.append((x[1], nb))
                elif k == "call":
                    t, ret = x[1], x[2]
                    if t is None:
                        targets = eng.sites.get(addr)
                        if targets is None:
                            out.add("unobserved indirect call")
                            continue
                        if any(eng.inputs.get(tt, ALL) & nb for kind, tt in targets if kind == "entry"):
                            out.add("a callee reads them")
                        nb2 = nb
                    else:
                        if eng.inputs[t] & nb:
                            out.add("a callee reads them")
                        nb2 = nb & ~eng.must_write[t]
                    if nb2:
                        if ret in cu.insns:
                            stack.append((ret, nb2))
                        else:
                            out.add("returns into another unit")
                elif k == "return":
                    out |= self.of_unit(cu.start, nb, depth + 1)
                elif k == "tail":
                    if eng.inputs.get(x[1], ALL) & nb:
                        out.add("a callee reads them")
                    rest = nb & ~eng.must_write.get(x[1], 0)
                    if rest:
                        out |= self.of_unit(cu.start, rest, depth + 1)
                elif k == "jump":
                    out.add("computed jump")
        return out


UNSEEN = frozenset({"unknown callers", "unobserved indirect call", "computed jump"})


def flag_result(eng: Engine, readers: FlagReaders, e: int, oflags: int) -> dict:
    """The bool a flag result can be: the one condition (with its negation) every known
    caller tests, if that is all they do (a ceiling engine ignores what no route showed)."""
    u = eng.unit_of(e)
    conds = readers.of_unit(u.start, oflags) - (UNSEEN if eng.model.ceiling else frozenset())
    kinds = {COND_RESULT.get(c) for c in conds}
    if not conds:
        return {"conds": [], "result": None, "deterministic": True}
    if None in kinds or len(kinds) != 1:
        return {"conds": sorted(conds), "result": None, "deterministic": False}
    k = kinds.pop()
    need = RESULT_FLAGS[k]
    return {"conds": sorted(conds), "result": f"flag:{k}", "deterministic": eng.must_write[e] & need == need}


# ── proposals ───────────────────────────────────────────────────────────
def propose(eng: Engine, e: int, unobserved_indirect: bool, callers_live: int | None = None,
            readers: FlagReaders | None = None) -> dict:
    """A C signature from the contract (doc 3.3): args = input registers in order (r10 is
    the toolkit global), ret = r0 / r0:r1 / none, keep = other registers callers read.
    Auto-acceptable when no blocker applies: it does not unwind (`unwinds`), reads no
    flags on entry (`flags_in`), no known call site reads flags (`flags_out`: with
    `readers`, unless every one tests the same condition and the function sets it on every
    path: then the result is that bool) or registers other than the result (`keep`) after
    it, and its callers are known (`callers_unknown`: called through pointers no route
    was seen to use). `ret_and_flag`: callers read r0 and test a flag."""
    a = eng.a
    inp = eng.inputs[e]
    outs, oflags = eng.outs(e, callers_live)
    args = [f"r{n}" for n in ARG_REGS if inp & RB(n) and n != 10]
    ret = "r0:r1" if outs & RB(0) and outs & RB(1) else "r0" if outs & RB(0) else "none"
    keep = outs & ~(RB(0) | (RB(1) if ret == "r0:r1" else 0))
    flag = None
    flag_blocker = bool(oflags)
    extra = []
    if oflags and readers is not None:
        flag = flag_result(eng, readers, e, oflags)
        if not flag["conds"]:
            flag_blocker = False                       # no known caller tests them
        elif flag["result"] and flag["deterministic"]:
            flag_blocker = False
            if ret == "none":
                ret = flag["result"]
            else:
                extra.append("ret_and_flag")
    blockers = [b for b, on in (("unwinds", a.may_unwind[e]), ("flags_in", inp & ALLF), ("flags_out", flag_blocker),
                                ("keep", keep), ("callers_unknown", unobserved_indirect)) if on] + extra
    out = {
        "args": args,
        "ret": ret,
        "globals": ["toolkit"] if inp & RB(10) else [],
        "keep": regs(keep),
        "flags_out": regs(oflags),
        "flags_in": regs(inp & ALLF),
        "auto": not blockers,
        "blockers": blockers,
    }
    if flag is not None:
        out["flag_conds"] = flag["conds"]
    return out


# ── the export ──────────────────────────────────────────────────────────
# The contracts the lifted headers carry, so what models are shown, what the difftest compares and
# what stamps record (review R1-A7): the lifter's analysis with every refinement above. lift.py
# writes the inputs and what callers read from it; may-write and preserved stay the lifter's.
REFINED = Model(edges=True, swi=True, per_site=True, pass_through=True)

STEPS = [("edges", Model(edges=True)),
         ("swi", Model(edges=True, swi=True)),
         ("per_site", Model(edges=True, swi=True, per_site=True)),
         ("pass_through", REFINED)]


def refined_contracts(a: Analysis, edges: list[Edge]) -> tuple[dict[int, int], dict[int, int]]:
    """(inputs of each entry, what callers may read after each unit returns) under REFINED."""
    eng = Engine(a, REFINED, edges)
    return eng.inputs, eng.callers


def write(path: str, a: Analysis, edge_files: list[str] | None = None, rom_sha1: str | None = None) -> dict:
    """Write the export to `path` and return its summary."""
    p, names = a.p, a.names
    unit_of = p.unit_at
    edges = read_edges(edge_files, rom_sha1) if edge_files else None
    static = Engine(a, Model(), None)
    if (static.callers != a.callers_live or static.indirect_ret != a.indirect_ret or static.inputs != a.inputs
            or static.may_write != a.may_write or static.must_write != a.must_write):
        raise SystemExit("abi export: re-running the lifter's analysis gave other results")
    engines = {name: Engine(a, model, edges) for name, model in STEPS} if edges is not None else {}
    obs = engines.get("edges")
    final = engines.get("pass_through")
    readers = FlagReaders(final) if final is not None else None
    ceiling = Engine(a, Model(edges=True, swi=True, per_site=True, pass_through=True, ceiling=True), edges) \
        if edges is not None else None
    ceiling_readers = FlagReaders(ceiling) if ceiling is not None else None
    readable_units = {u.start for u in p.units if any(names[e] in a.overridden for e in u.entries)}

    def unobserved(eng: Engine, u: Unit) -> bool:
        return any(x in a.indirect_targets for x in u.entries) and not eng.fully_observed(u)

    entries = {}
    for u in sorted(p.units, key=lambda x: x.start):
        for e in sorted(u.entries):
            loc = a.info.lines.get(e)
            rec = {
                "addr": h(e), "unit": names[u.start], "thumb": u.thumb,
                "src": f"{loc[0]}:{loc[1]}" if loc else None,
                "readable": u.start in readable_units,
                "inputs": regs(a.inputs[e]), "may_write": regs(a.may_write[u.start]),
                "preserved": regs(a.preserved[e] & a.may_write[u.start]),
                "must_write": regs(a.must_write[e] if a.must_write[e] != ALL else 0),
                "callers_live": regs(a.callers_live[u.start]),
                "outputs": regs(a.callers_live[u.start] & a.may_write[u.start]),
                "may_unwind": a.may_unwind[e], "needs_ret": a.needs_ret(u),
                "indirect_target": e in a.indirect_targets, "tail_target": e in a.tail_targets,
            }
            for tag, eng in (("observed", obs), ("final", final)):
                if eng is not None:
                    rec[f"inputs_{tag}"] = regs(eng.inputs[e])
                    rec[f"may_write_{tag}"] = regs(eng.may_write[u.start])
                    rec[f"callers_live_{tag}"] = regs(eng.callers[u.start])
                    rec[f"outputs_{tag}"] = regs(eng.callers[u.start] & eng.may_write[u.start])
            if obs is not None:
                rec["indirect_callers_observed"] = obs.fully_observed(u) if e in a.indirect_targets else None
            if rec["readable"]:
                rec["proposal"] = propose(static, e, e in a.indirect_targets)
                if obs is not None:
                    rec["proposal_observed"] = propose(obs, e, unobserved(obs, u))
                    rec["blockers_by_step"] = {name: propose(eng, e, unobserved(eng, u))["blockers"]
                                               for name, eng in engines.items()}
                    rec["proposal_final"] = propose(final, e, unobserved(final, u), readers=readers)
                    rec["blockers_by_step"]["flag_result"] = rec["proposal_final"]["blockers"]
                    per_site_keep = set(propose(engines["per_site"], e, False)["keep"])
                    rec["keep_claimed_only"] = sorted(per_site_keep - set(rec["proposal_final"]["keep"]),
                                                      key=lambda r: int(r[1:]))
                    rec["ceiling_blockers"] = propose(ceiling, e, False, readers=ceiling_readers)["blockers"]
            entries[names[e]] = rec

    sites = []
    for u in sorted(p.units, key=lambda x: x.start):
        lives = None
        for addr in sorted(a.effects[u.start]):
            for x in a.effects[u.start][addr]:
                kind = {"call": "call", "tail": "tail", "jump": "jump"}.get(x[0])
                if kind is None:
                    continue
                site = {"addr": h(addr), "caller": names[u.start]}
                if obs is not None and (kind == "jump" or (x[0] == "call" and x[1] is None)):
                    site["observed"] = sorted({k if k == "return" else names.get(t, h(t)) if k == "entry" else h(t)
                                               for k, t in obs.sites.get(addr, ())})
                if x[0] == "call":
                    tgt, ret = x[1], x[2]
                    if tgt is None:
                        kind = "indirect"
                    if lives is None:
                        lives = [("", static, static.unit_liveness(u, static.entry_live, static.callers[u.start]))]
                        for tag, eng in (("_observed", obs), ("_final", final)):
                            if eng is not None:
                                gen = eng.inputs if eng.model.per_site else None
                                lives.append((tag, eng, eng.unit_liveness(u, eng.entry_live, eng.callers[u.start], gen)))
                    site.update(kind=kind, target=names.get(tgt) if tgt is not None else None, ret=h(ret))
                    for tag, eng, live_in in lives:
                        site["live_after" + tag] = regs(live_in[ret] if ret in u.insns else eng.entry_live.get(ret, ALL))
                else:
                    site.update(kind=kind, target=names.get(x[1]) if kind == "tail" else None, ret=None)
                    for tag, eng in (("", static), ("_observed", obs), ("_final", final)):
                        if eng is not None:
                            site["live_after" + tag] = regs(eng.callers[u.start])
                sites.append(site)

    out_edges = []
    for (e, r, tu, how), after in (obs.edge_after if obs is not None else []):
        out_edges.append({"site": h(e.site), "site_unit": names.get(unit_of(e.site).start) if unit_of(e.site) else None,
                          "target": names.get(e.target & ~1, h(e.target)), "thumb": bool(e.target & 1),
                          "lr": h(e.lr), "count": e.count, "return_point": h(r), "how": how,
                          "live_after": regs(after)})

    summary = summarize(entries, edges is not None, out_edges, sites)
    doc = {
        "format": "abi-graph 2 (docs/C_CALLING_CONVENTION.md 3.1)",
        "edge_files": [os.path.relpath(f) for f in edge_files or []],
        "steps": ["lifter"] + [name for name, _m in STEPS] + ["flag_result"],
        "indirect_ret": regs(a.indirect_ret),
        "indirect_ret_observed": regs(obs.indirect_ret) if obs is not None else None,
        "summary": summary,
        "entries": entries,
        "sites": sites,
        "edges": out_edges,
    }
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w") as fh:
        json.dump(doc, fh, indent=None, separators=(",", ":"))
        fh.write("\n")
    return summary


def summarize(entries: dict, with_edges: bool, edges: list[dict], sites: list[dict]) -> dict:
    """Counts the doc asks for: readable functions whose callers may read all of r0-r12,
    how many observed edges narrow, and how many proposed signatures are auto-acceptable
    after each refinement."""
    allr = {f"r{n}" for n in ARG_REGS}
    readable = {n: r for n, r in entries.items() if r["readable"]}
    units = {r["unit"]: r for r in readable.values()}
    s: dict = {
        "entries": len(entries),
        "readable_entries": len(readable),
        "readable_units": len(units),
        "indirect_sites": sum(1 for x in sites if x["kind"] in ("indirect", "jump")),
        "readable_units_all_r0_r12_outputs": sorted(n for n, r in units.items() if allr <= set(r["outputs"])),
        "auto_acceptable": sum(r["proposal"]["auto"] for r in readable.values()),
    }
    if with_edges:
        still = sorted(n for n, r in units.items() if allr <= set(r["outputs"]) and allr <= set(r["outputs_observed"]))
        s["readable_units_all_r0_r12_outputs_observed"] = still
        s["narrowed_by_edges"] = len(s["readable_units_all_r0_r12_outputs"]) - len(still)
        s["auto_acceptable_observed"] = sum(r["proposal_observed"]["auto"] for r in readable.values())
        s["auto_by_step"] = {"lifter": s["auto_acceptable"],
                             **{k: sum(not r["blockers_by_step"][k] for r in readable.values())
                                for k in next(iter(readable.values()))["blockers_by_step"]}}
        s["auto_acceptable_ceiling"] = sum(not r["ceiling_blockers"] for r in readable.values())
        s["edges"] = len(edges)
        s["edge_sites"] = len({x["site"] for x in edges})
        s["edge_targets"] = len({x["target"] for x in edges})
    return s
