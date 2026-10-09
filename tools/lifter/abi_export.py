"""The lifter's register contracts and call sites as JSON (docs/C_CALLING_CONVENTION.md
3.1), narrowed by observed call edges (3.2), with a proposed C signature for every
readable function (3.3, the mechanical part).

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

Call edges (`mgba_run --call-edges` lines "SITE TARGET LR COUNT"): each says that an
indirect site went to TARGET with LR set, i.e. TARGET returns to LR. The lifter gives
every indirect target the union of what is live after *every* indirect site
(`indirect_ret`), and assumes every computed call or jump reads and writes every
register. With edges (the `*_observed` fields), a unit whose indirect-target entries
were all observed gets only what is live at the return points of its observed edges
(plus its direct and tail callers, as before), and an observed site reads and writes
only what its observed targets do; unobserved targets and sites keep the lifter's
assumptions. That is evidence from the recorded routes, not proof: an indirect site
the routes never ran, or ran with other data, may reach other functions.
`ceiling_blockers` assumes the indirect calls no route reached read nothing after them.
"""
from __future__ import annotations

import json
import os
from collections import defaultdict
from dataclasses import dataclass
from typing import Callable

from .analysis import ALL, ALLF, RB, Program, Unit, mask_names
from .elfinfo import ElfInfo

RETURN = "a return through a register (target == lr)"
ARG_REGS = [n for n in range(13)]          # r0..r12 can carry arguments and results
REGS = sum(RB(n) for n in ARG_REGS)


@dataclass
class Analysis:
    """What lift.py computed (its main() locals), handed over for the export."""
    p: Program
    info: ElfInfo
    names: dict[int, str]
    effects: dict[int, dict[int, list]]
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


def regs(m: int) -> list[str]:
    s = mask_names(m)
    return [] if s == "-" else s.split()


def h(a: int | None) -> str | None:
    return None if a is None else f"0x{a:08X}"


def read_edges(paths: list[str]) -> list[Edge]:
    """Merge mgba_run --call-edges files (counts add up)."""
    merged: dict[tuple[int, int, int], int] = defaultdict(int)
    for path in paths:
        for line in open(path):
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


def seen_hook(sites: dict[int, list[tuple[str, int]]]):
    """unit_liveness's `seen`: what the observed targets of a computed call or jump read."""
    def seen(site: int, live_in: dict, entry_in: dict, ret_live: int) -> int:
        targets = sites.get(site)
        if targets is None:
            return ALL
        m = 0
        for kind, t in targets:
            m |= entry_in.get(t, ALL) if kind == "entry" else live_in[t] if kind == "local" else ret_live
        return m
    return seen


def observed_may_write(a: Analysis, sites: dict[int, list[tuple[str, int]]]) -> dict[int, int]:
    """lift.py's may-write fixpoint, with computed calls and jumps writing what their
    observed targets may write (instead of everything)."""
    unit_of = a.p.unit_at
    mw = {u.start: 0 for u in a.p.units}
    while True:
        changed = False
        for u in a.p.units:
            m = mw[u.start] | a.local_w[u.start]
            for addr, eff in a.effects[u.start].items():
                for x in eff:
                    if x[0] in ("call", "tail") and x[1] is not None:
                        m |= mw[unit_of(x[1]).start]
                    elif x[0] in ("call", "jump"):
                        targets = sites.get(addr)
                        if targets is None:
                            m |= ALL
                        for kind, t in targets or ():
                            if kind == "entry":
                                m |= mw[unit_of(t).start]
            if m != mw[u.start]:
                mw[u.start] = m
                changed = True
        if not changed:
            return mw


def observed_inputs(a: Analysis, seen) -> dict[int, int]:
    """lift.py's inputs fixpoint (2), with computed calls and jumps reading what their
    observed targets read."""
    inputs = {e: 0 for e in a.names}
    while True:
        changed = False
        for u in a.p.units:
            live_in, _ = a.unit_liveness(u, inputs, 0, seen)
            for e in u.entries:
                if live_in[e] & ~inputs[e]:
                    inputs[e] |= live_in[e]
                    changed = True
        if not changed:
            return inputs


def liveness(a: Analysis, edges: list[Edge] | None, unknown: int | None = None, seen=None):
    """lift.py's liveness fixpoint (1); with `edges`, indirect targets whose entries were
    all observed get the liveness at their edges' return points instead of indirect_ret.
    `unknown` replaces indirect_ret for the others (0: the ceiling, as if the indirect
    calls no route reached read nothing after them).
    Returns (callers_live, flags_in, indirect_ret, live_in at every return point used,
    [((edge, return point, target unit, how), live after)], fully_observed(unit))."""
    p, effects = a.p, a.effects
    unit_of = p.unit_at
    flags_in = {e: 0 for e in a.names}
    callers = {u.start: 0 for u in p.units}
    ind_ret = 0
    # Where each edge returns to: a call site's own return point; for a jump, the LR it
    # went with when that is the return point of a call to the jumping unit (LR still
    # holds the unit's own return address: a veneer `bx r3`, a dispatcher jumping before
    # any call), otherwise the jumping unit's callers (the lifter's rule).
    kind_at = {addr: (u, kind, ret) for u, addr, kind, ret in indirect_sites(a)}
    calls_returning_to: dict[int, set[int]] = defaultdict(set)    # return point -> called units
    for u in p.units:
        for eff in effects[u.start].values():
            for x in eff:
                if x[0] == "call" and x[1] is not None:
                    calls_returning_to[x[2]].add(unit_of(x[1]).start)
    plan: list[tuple[Edge, int | None, Unit | None, str]] = []   # edge, return point, target unit, how
    for e in edges or []:
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
    want: dict[int, set[int]] = defaultdict(set)       # unit start -> return points to record
    for u in p.units:
        for eff in effects[u.start].values():
            for x in eff:
                if x[0] == "call" and x[2] in u.insns:
                    want[u.start].add(x[2])
    for _e, r, _tu, _how in plan:
        if r is not None and unit_of(r) is not None and r in unit_of(r).insns:
            want[unit_of(r).start].add(r)
    observed: dict[int, list] = defaultdict(list)        # target unit start -> plan items
    seen_entries = set()
    for item in plan:
        if item[2] is not None and item[3] != RETURN:
            observed[item[2].start].append(item)
            seen_entries.add(item[0].target & ~1)

    def fully_observed(u: Unit) -> bool:
        return edges is not None and all(e in seen_entries for e in u.entries if e in a.indirect_targets)

    live_at: dict[int, int] = {}
    while True:
        changed = False
        new_callers = dict(callers)
        new_ind = ind_ret
        for u in p.units:
            live_in, _ = a.unit_liveness(u, flags_in, callers[u.start], seen)
            for e in u.entries:
                if live_in[e] & ~flags_in[e]:
                    flags_in[e] |= live_in[e]
                    changed = True
            for r in want.get(u.start, ()):
                live_at[r] = live_in[r]
            for eff in effects[u.start].values():
                for x in eff:
                    if x[0] == "call":
                        tgt, ret = x[1], x[2]
                        after = live_in[ret] if ret in u.insns else flags_in.get(ret, ALL)
                        if tgt is not None:
                            new_callers[unit_of(tgt).start] |= after
                        else:
                            new_ind |= after
                    elif x[0] == "tail":
                        new_callers[unit_of(x[1]).start] |= callers[u.start]
                    elif x[0] == "jump":
                        new_ind |= callers[u.start]

        def after(item) -> int:
            e, r, _tu, how = item
            if how in ("jump within the unit", RETURN):
                return 0
            if r is not None:
                return live_at[r] if r in live_at else flags_in.get(r, ALL)
            su = unit_of(e.site)
            return callers[su.start] if su is not None else ALL

        for u in p.units:
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
    edge_after = [(item, after(item)) for item in plan]
    return callers, flags_in, ind_ret, live_at, edge_after, fully_observed


def propose(a: Analysis, e: int, u: Unit, callers_live: int, unobserved_indirect: bool,
            inputs: dict[int, int] | None = None, may_write: dict[int, int] | None = None) -> dict:
    """A C signature from the contract (doc 3.3): args = input registers in order (r10 is
    the toolkit global), ret = r0 / r0:r1 / none, keep = other registers callers read.
    Auto-acceptable when no blocker applies: it does not unwind (`unwinds`), reads no
    flags on entry (`flags_in`), no known call site reads flags (`flags_out`: a flag
    result needs a choice of z/!z/c) or registers other than the result (`keep`) after
    it, and its callers are known (`callers_unknown`: called through pointers no route
    was seen to use)."""
    inp = (inputs or a.inputs)[e]
    mw = (may_write or a.may_write)[u.start]
    outs = callers_live & mw & ~a.preserved[e] & REGS
    oflags = callers_live & mw & ALLF
    args = [f"r{n}" for n in ARG_REGS if inp & RB(n) and n != 10]
    ret = "r0:r1" if outs & RB(0) and outs & RB(1) else "r0" if outs & RB(0) else "none"
    keep = outs & ~(RB(0) | (RB(1) if ret == "r0:r1" else 0))
    blockers = [b for b, on in (("unwinds", a.may_unwind[e]), ("flags_in", inp & ALLF), ("flags_out", oflags),
                                ("keep", keep), ("callers_unknown", unobserved_indirect)) if on]
    return {
        "args": args,
        "ret": ret,
        "globals": ["toolkit"] if inp & RB(10) else [],
        "keep": regs(keep),
        "flags_out": regs(oflags),
        "flags_in": regs(inp & ALLF),
        "auto": not blockers,
        "blockers": blockers,
    }


def write(path: str, a: Analysis, edge_files: list[str] | None = None) -> dict:
    """Write the export to `path` and return its summary."""
    p, names = a.p, a.names
    unit_of = p.unit_at
    edges = read_edges(edge_files) if edge_files else None
    static = liveness(a, None)
    if static[0] != a.callers_live or static[2] != a.indirect_ret or observed_may_write(a, {}) != a.may_write:
        raise SystemExit("abi export: re-running the lifter's analysis gave other results")
    obs = ceiling = None
    if edges is not None:
        sites_seen = observed_targets(a, edges)
        seen = seen_hook(sites_seen)
        mw_obs = observed_may_write(a, sites_seen)
        in_obs = observed_inputs(a, seen)
        obs = liveness(a, edges, seen=seen)
        ceiling = liveness(a, edges, unknown=0, seen=seen)
    readable_units = {u.start for u in p.units if any(names[e] in a.overridden for e in u.entries)}

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
            if obs is not None:
                rec["inputs_observed"] = regs(in_obs[e])
                rec["may_write_observed"] = regs(mw_obs[u.start])
                rec["callers_live_observed"] = regs(obs[0][u.start])
                rec["outputs_observed"] = regs(obs[0][u.start] & mw_obs[u.start])
                rec["indirect_callers_observed"] = obs[5](u) if e in a.indirect_targets else None
            if rec["readable"]:
                rec["proposal"] = propose(a, e, u, a.callers_live[u.start], e in a.indirect_targets)
                if obs is not None:
                    unobs = any(x in a.indirect_targets for x in u.entries) and not obs[5](u)
                    rec["proposal_observed"] = propose(a, e, u, obs[0][u.start], unobs, in_obs, mw_obs)
                    rec["ceiling_blockers"] = propose(a, e, u, ceiling[0][u.start], False, in_obs, mw_obs)["blockers"]
            entries[names[e]] = rec

    sites = []
    for u in sorted(p.units, key=lambda x: x.start):
        live_in = obs_live_in = None
        for addr in sorted(a.effects[u.start]):
            for x in a.effects[u.start][addr]:
                kind = {"call": "call", "tail": "tail", "jump": "jump"}.get(x[0])
                if kind is None:
                    continue
                site = {"addr": h(addr), "caller": names[u.start]}
                if obs is not None and (kind == "jump" or (x[0] == "call" and x[1] is None)):
                    site["observed"] = sorted({k if k == "return" else names.get(t, h(t)) if k == "entry" else h(t)
                                               for k, t in sites_seen.get(addr, ())})
                if x[0] == "call":
                    tgt, ret = x[1], x[2]
                    if tgt is None:
                        kind = "indirect"
                    if live_in is None:
                        live_in = a.unit_liveness(u, static[1], static[0][u.start])[0]
                        if obs is not None:
                            obs_live_in = a.unit_liveness(u, obs[1], obs[0][u.start], seen)[0]
                    site.update(kind=kind, target=names.get(tgt) if tgt is not None else None, ret=h(ret),
                                live_after=regs(live_in[ret] if ret in u.insns else static[1].get(ret, ALL)))
                    if obs_live_in is not None:
                        site["live_after_observed"] = regs(obs_live_in[ret] if ret in u.insns
                                                           else obs[1].get(ret, ALL))
                elif x[0] == "tail":
                    site.update(kind=kind, target=names.get(x[1]), ret=None,
                                live_after=regs(static[0][u.start]))
                    if obs is not None:
                        site["live_after_observed"] = regs(obs[0][u.start])
                else:
                    site.update(kind=kind, target=None, ret=None, live_after=regs(static[0][u.start]))
                    if obs is not None:
                        site["live_after_observed"] = regs(obs[0][u.start])
                sites.append(site)

    out_edges = []
    for (e, r, tu, how), after in (obs[4] if obs is not None else []):
        out_edges.append({"site": h(e.site), "site_unit": names.get(unit_of(e.site).start) if unit_of(e.site) else None,
                          "target": names.get(e.target & ~1, h(e.target)), "thumb": bool(e.target & 1),
                          "lr": h(e.lr), "count": e.count, "return_point": h(r), "how": how,
                          "live_after": regs(after)})

    summary = summarize(entries, edges is not None, out_edges, sites)
    doc = {
        "format": "abi-graph 1 (docs/C_CALLING_CONVENTION.md 3.1)",
        "edge_files": [os.path.relpath(f) for f in edge_files or []],
        "indirect_ret": regs(a.indirect_ret),
        "indirect_ret_observed": regs(obs[2]) if obs is not None else None,
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
    how many observed edges narrow, and how many proposed signatures are auto-acceptable."""
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
        s["auto_acceptable_ceiling"] = sum(not r["ceiling_blockers"] for r in readable.values())
        s["edges"] = len(edges)
        s["edge_sites"] = len({x["site"] for x in edges})
        s["edge_targets"] = len({x["target"] for x in edges})
    return s
