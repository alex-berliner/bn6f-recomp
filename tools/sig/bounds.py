#!/usr/bin/env python3
"""Upper and lower bounds of every function's arguments and results, from the lifter's analysis.

    python3 tools/sig/bounds.py [--out build/sig/bounds.json] [lift.py options]

Runs tools/lifter/lift.py's analysis (as `lift.py --export-abi` does; nothing is lifted or
written into the tree) and, for every entry (graph.json proposes signatures for readable ones only),
two proposals (abi_export.propose):
  final    the refined contract (abi_export.REFINED: observed call edges, BIOS calls, per-site
           liveness, saves pass through, flag results). An upper bound: an argument it lacks is
           never read, a result it lacks is never read by a known caller.
  ceiling  the same with `ceiling` on: calls and jumps the routes never showed read and write
           nothing, unknown callers read nothing. A lower bound in practice: what the function and
           its known callees and callers need.
Where the two agree, the contract settles the registers; where they differ, the difference comes
only from code the routes never reached (jump tables, pointers), and a reader decides.
The lifter's sources are not changed (their hash is part of every stamp).
"""
from __future__ import annotations

import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from lifter import abi_export, lift  # noqa: E402

OUT = os.path.join(ROOT, "build", "sig", "bounds.json")


def write(path: str, a: "abi_export.Analysis", edge_files=None, rom_sha1=None) -> dict:
    edges = abi_export.read_edges(edge_files, rom_sha1)
    final = abi_export.Engine(a, abi_export.REFINED, edges)
    ceiling = abi_export.Engine(a, abi_export.Model(edges=True, swi=True, per_site=True, pass_through=True,
                                                    ceiling=True), edges)
    readers = abi_export.FlagReaders(final)
    creaders = abi_export.FlagReaders(ceiling)
    out = {}
    for u in a.p.units:
        unobserved = any(x in a.indirect_targets for x in u.entries) and not final.fully_observed(u)
        for e in u.entries:
            out[a.names[e]] = {
                "final": abi_export.propose(final, e, unobserved, readers=readers),
                "ceiling": abi_export.propose(ceiling, e, False, readers=creaders),
                "inputs_final": abi_export.regs(final.inputs[e]),
                "inputs_ceiling": abi_export.regs(ceiling.inputs[e]),
            }
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w") as fh:
        json.dump(out, fh, separators=(",", ":"))
    return {"entries": len(out)}


def main() -> None:
    argv = sys.argv[1:]
    out = OUT
    if "--out" in argv:
        i = argv.index("--out")
        out = argv[i + 1]
        del argv[i:i + 2]
    abi_export.write = write
    sys.argv = ["lift.py", "--export-abi", out] + argv
    lift.main()


if __name__ == "__main__":
    main()
