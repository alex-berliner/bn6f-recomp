#!/usr/bin/env python3
"""Evidence for choosing C signatures (docs/C_CALLING_CONVENTION.md 3.3), from an export
of `lift.py --export-abi` (with --call-edges for the observed narrowing).

    tools/abi/report.py build/abi/graph.json NAME...     evidence per function (JSON)
    tools/abi/report.py build/abi/graph.json --summary   counts over the readable functions

Per function: its contract, the proposed signature (static: what the lifter assumes for
indirect calls; observed: narrowed by the routes' call edges) with its blockers, and
every known call site: direct calls and tail calls (with what the caller reads after),
observed indirect edges (with their return point), each marked readable or lifted.

The summary counts the readable functions whose callers may read all of r0-r12, how
many the observed edges narrow and why the others stay (callers unknown, or a known
caller really reads them), and how many proposals are auto-acceptable (no output other
than r0/r0:r1 read at any known call site, no flags read, does not unwind, callers
known), with the blockers of the others.
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
        out = {k: s[k] for k in ("addr", "caller", "kind", "target", "live_after") if k in s}
        if "live_after_observed" in s:
            out["live_after_observed"] = s["live_after_observed"]
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


def summary(g: dict) -> dict:
    s = dict(g["summary"])
    readable = {n: r for n, r in g["entries"].items() if r["readable"]}
    for key in ("proposal", "proposal_observed"):
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
    args = ap.parse_args()
    g = load(args.graph)
    out: dict | list = summary(g) if args.summary or not args.names else [evidence(g, n) for n in args.names]
    json.dump(out, sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main()
