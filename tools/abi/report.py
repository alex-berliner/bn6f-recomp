#!/usr/bin/env python3
"""Evidence for choosing C signatures (docs/C_CALLING_CONVENTION.md 3.3), from an export
of `lift.py --export-abi` (with --call-edges for the observed narrowing).

    tools/abi/report.py build-abi/graph.json NAME...     evidence per function (JSON)
    tools/abi/report.py build-abi/graph.json --summary [--captures build-abi/captures.json]
                                                         counts over the readable functions

Per function: its contract, the proposed signature (static: what the lifter assumes for
indirect calls; observed: narrowed by the routes' call edges) with its blockers, and
every known call site: direct calls and tail calls (with what the caller reads after),
observed indirect edges (with their return point), each marked readable or lifted.

The summary counts the readable functions whose callers may read all of r0-r12, how
many the observed edges narrow and why the others stay (callers unknown, or a known
caller really reads them), and how many proposals are auto-acceptable (no output other
than r0/r0:r1 read at any known call site, no flags read, does not unwind, callers
known), with the blockers of the others; with an export that has them, after each
refinement step (lift.py's abi_export: edges, swi, per_site, pass_through, flag_result)
and, with --captures (tools/abi/captures.py), after clearing `keep` where every captured
call of the original left the kept registers unchanged. It also lists the blocked
readable functions with the most call sites.
"""
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter

ALLR = {f"r{n}" for n in range(13)}


def load(path: str) -> dict:
    return json.load(open(path))


def callers_of(g: dict, name: str) -> dict:
    """Every known call site of NAME's unit (callers_live is per unit)."""
    e = g["entries"][name]
    unit_entries = {n for n, r in g["entries"].items() if r["unit"] == e["unit"]}
    readable = {n for n, r in g["entries"].items() if r["readable"]}

    def site(s: dict) -> dict:
        out = {k: s[k] for k in ("addr", "caller", "kind", "target", "live_after", "live_after_observed",
                                 "live_after_final") if k in s}
        out["caller_readable"] = s["caller"] in readable
        return out

    direct = [site(s) for s in g["sites"] if s["target"] in unit_entries]
    edges = [dict(x, site_readable=x["site_unit"] in readable) for x in g["edges"] if x["target"] in unit_entries]
    return {"direct_and_tail": direct, "observed_indirect": edges}


def evidence(g: dict, name: str) -> dict:
    e = g["entries"].get(name)
    if e is None:
        sys.exit(f"{name}: not an entry of the export")
    return {"name": name, **e, **callers_of(g, name)}


def why_all(g: dict, unit: str) -> tuple[str, str]:
    """(reason, detail): why a readable unit's observed callers may still read all of r0-r12."""
    entries = {n: r for n, r in g["entries"].items() if r["unit"] == unit}
    if any(r["indirect_target"] and not r.get("indirect_callers_observed") for r in entries.values()):
        return "callers unknown", "called through pointers the routes never reached"
    for s in g["sites"]:
        if s["target"] in entries and ALLR <= set(s.get("live_after_observed", s["live_after"])):
            what = "tail-called by" if s["kind"] == "tail" else "called by"
            return "a caller reads everything", f"{what} {s['caller']} at {s['addr']}"
    for x in g["edges"]:
        if x["target"] in entries and ALLR <= set(x["live_after"]):
            return "an observed edge returns where everything is live", f"from {x['site_unit']} at {x['site']} ({x['how']})"
    return "several callers, each reading some", ""


def cleared_by_captures(r: dict, captures: dict | None, allowed: tuple[str, ...] = ("keep",)) -> bool:
    """Blocked only by `allowed` (keep, flags_out), and every captured call of the original
    left the kept registers (and flags) as it found them."""
    c = (captures or {}).get(r["name"]) if r.get("proposal_final") else None
    blockers = r["proposal_final"]["blockers"] if c else []
    return bool(c and blockers and set(blockers) <= set(allowed) and set(blockers) <= set(c["blockers"])
                and c["calls"] and not any(c["changed"].values()))


def steps(g: dict, captures: dict | None) -> dict:
    """Auto-acceptable and blockers after each refinement step."""
    readable = [dict(r, name=n) for n, r in g["entries"].items() if r["readable"]]
    names = ["lifter"] + list(readable[0]["blockers_by_step"])
    table = {}
    for step in names:
        bl = [r["proposal"]["blockers"] if step == "lifter" else r["blockers_by_step"][step] for r in readable]
        table[step] = {"auto": sum(not b for b in bl), "blockers": dict(Counter(x for b in bl for x in b).most_common())}
    if captures is not None:
        for step, allowed in (("captures", ("keep",)), ("captures_with_flags", ("keep", "flags_out"))):
            bl = [[] if cleared_by_captures(r, captures, allowed) else r["proposal_final"]["blockers"] for r in readable]
            table[step] = {"auto": sum(not b for b in bl),
                           "blockers": dict(Counter(x for b in bl for x in b).most_common())}
    return table


def flag_stats(g: dict) -> dict:
    """What flag-result inference finds: conditions callers test, results proposed."""
    readable = [r for r in g["entries"].values() if r["readable"] and "flag_conds" in r.get("proposal_final", {})]
    conds: Counter[str] = Counter(c for r in readable for c in r["proposal_final"]["flag_conds"])
    before = sum("flags_out" in r["blockers_by_step"]["pass_through"] for r in readable)
    after = sum("flags_out" in r["proposal_final"]["blockers"] for r in readable)
    return {
        "flag_blocked_before": before,
        "cleared": before - after,
        "results": dict(Counter(r["proposal_final"]["ret"] for r in readable
                                if r["proposal_final"]["ret"].startswith("flag"))),
        "ret_and_flag": sum("ret_and_flag" in r["proposal_final"]["blockers"] for r in readable),
        "conditions_and_obstacles": dict(conds.most_common()),
    }


def keep_stats(g: dict, captures: dict | None) -> dict:
    """Where kept registers come from: claimed only by later callees' saves (cleared by
    pass_through), still read, and what the captured calls say about the rest."""
    readable = [dict(r, name=n) for n, r in g["entries"].items() if r["readable"] and "proposal_final" in r]
    out = {
        "keep_blocked_per_site": sum("keep" in r["blockers_by_step"]["per_site"] for r in readable),
        "keep_blocked_final": sum("keep" in r["proposal_final"]["blockers"] for r in readable),
        "functions_with_registers_claimed_only_by_callee_saves": sum(1 for r in readable if r["keep_claimed_only"]),
        "registers_claimed_only": dict(Counter(x for r in readable for x in r["keep_claimed_only"]).most_common()),
        "registers_still_read": dict(Counter(x for r in readable for x in r["proposal_final"]["keep"]).most_common()),
        "keep_cleared_at_ceiling": sum("keep" in r["proposal_final"]["blockers"] and "keep" not in r["ceiling_blockers"]
                                       for r in readable),
    }
    if captures is not None:
        only = [r for r in readable if r["proposal_final"]["blockers"] == ["keep"]]
        calls = sorted(captures[r["name"]]["calls"] for r in only if r["name"] in captures and cleared_by_captures(r, captures))
        out["keep_only"] = len(only)
        out["keep_only_without_captures"] = sum(1 for r in only if not captures.get(r["name"], {}).get("calls"))
        out["keep_only_unchanged_in_every_capture"] = len(calls)
        out["unchanged_calls_min_median"] = [calls[0], calls[len(calls) // 2]] if calls else None
        out["keep_only_changed_in_some_capture"] = sum(1 for r in only if any(captures.get(r["name"], {}).get("changed", {}).values()))
    return out


def top_blocked(g: dict, captures: dict | None, n: int = 20) -> list[dict]:
    """The blocked readable functions with the most known call sites."""
    count: Counter[str] = Counter()
    unit_of = {name: r["unit"] for name, r in g["entries"].items()}
    for site in g["sites"]:
        if site["target"]:
            count[unit_of.get(site["target"], "")] += 1
    for x in g["edges"]:
        count[unit_of.get(x["target"], "")] += 1
    rows = []
    for name, r in g["entries"].items():
        p = r.get("proposal_final")
        if not r["readable"] or p is None or p["auto"] or \
                cleared_by_captures(dict(r, name=name), captures, ("keep", "flags_out")):
            continue
        rows.append({"name": name, "call_sites": count[r["unit"]], "blockers": p["blockers"], "keep": p["keep"],
                     "flag_conds": p.get("flag_conds", []), "src": r["src"]})
    return sorted(rows, key=lambda x: -x["call_sites"])[:n]


def summary(g: dict, captures: dict | None = None) -> dict:
    s = dict(g["summary"])
    readable = {n: r for n, r in g["entries"].items() if r["readable"]}
    if any("blockers_by_step" in r for r in readable.values()):
        s["steps"] = steps(g, captures)
        s["flag_results"] = flag_stats(g)
        s["keep"] = keep_stats(g, captures)
        s["top_blocked"] = top_blocked(g, captures)
    for key in ("proposal", "proposal_observed", "proposal_final"):
        if not any(key in r for r in readable.values()):
            continue
        props = [r[key] for r in readable.values()]
        auto = [p for p in props if p["auto"]]
        s[key] = {
            "auto_acceptable": len(auto),
            "of": len(props),
            "auto_needing_toolkit_r10": sum(1 for p in auto if p["globals"]),
            "auto_ret": dict(Counter(p["ret"] for p in auto)),
            "blockers": dict(Counter(b for p in props for b in p["blockers"]).most_common()),
            "blocked_by_only": dict(Counter(p["blockers"][0] for p in props if len(p["blockers"]) == 1).most_common()),
            "keep_only_r1_r2_r3": sum(1 for p in props if p["blockers"] == ["keep"]
                                      and set(p["keep"]) <= {"r1", "r2", "r3"}),
        }
    if any("ceiling_blockers" in r for r in readable.values()):
        s["ceiling"] = {
            "auto_acceptable": sum(not r["ceiling_blockers"] for r in readable.values()),
            "blockers": dict(Counter(b for r in readable.values() for b in r["ceiling_blockers"]).most_common()),
        }
    if "readable_units_all_r0_r12_outputs_observed" in s:
        s["still_all_r0_r12"] = {u: ": ".join(filter(None, why_all(g, u)))
                                 for u in s["readable_units_all_r0_r12_outputs_observed"]}
        s["still_all_r0_r12_reasons"] = dict(Counter(why_all(g, u)[0]
                                                     for u in s["readable_units_all_r0_r12_outputs_observed"]))
    return s


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("graph")
    ap.add_argument("names", nargs="*")
    ap.add_argument("--summary", action="store_true")
    ap.add_argument("--captures", help="tools/abi/captures.py output")
    args = ap.parse_args()
    g = load(args.graph)
    captures = load(args.captures)["functions"] if args.captures else None
    out: dict | list = summary(g, captures) if args.summary or not args.names else [evidence(g, n) for n in args.names]
    json.dump(out, sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main()
