#!/usr/bin/env python3
"""Check claimed leftovers against the original's captured calls (docs/C_CALLING_CONVENTION.md 3.3).

    tools/abi/captures.py build/abi/graph.json [-o build/abi/captures.json] [--dirs DIR...]

A readable function blocked only by `keep` (callers read registers it may change) needs no
`keep:` when it leaves those registers as it found them: the generated adapter preserves
every register it does not return, so it is exact for them. The same holds for flags
callers read (`flags_out`): the adapter preserves them too. Checked are the functions
whose blockers are all in --blockers (default: keep only; `keep,flags_out` adds flags). This reads the entry and exit
snapshots the capture workflow took of the original game (default: every
build-wf-runs/capture-*/snapshots/NAME and build-wf-runs/snapshots-latest/NAME;
snap_*.bin with its snap_*.exit.bin, header "BN6S", version, r0-r14, pc, cpsr) and counts,
per function and kept register, the calls that returned it changed. Functions are matched
by entry address (the pc of their snapshots), so renames do not matter.

Prints a JSON summary; writes per function {calls, changed: {reg: n}} to -o.
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import struct
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_DIRS = ["build-wf-runs/capture-*/snapshots/*", "build-wf-runs/snapshots-latest/*"]
HEADER = struct.Struct("<19I")     # magic, version, r0..r14, pc, cpsr
FLAG_BITS = {"nz": 0xC0000000, "c": 0x20000000, "v": 0x10000000}   # CPSR N|Z, C, V


def header(path: str) -> tuple[int, ...] | None:
    try:
        with open(path, "rb") as fh:
            raw = fh.read(HEADER.size)
    except OSError:
        return None
    if len(raw) != HEADER.size:
        return None
    h = HEADER.unpack(raw)
    return h if h[0] == 0x53364E42 else None


def index(patterns: list[str]) -> dict[int, set[str]]:
    """Entry address -> capture directories of that function."""
    out: dict[int, set[str]] = defaultdict(set)
    for pattern in patterns:
        for d in glob.glob(os.path.join(ROOT, pattern)):
            real = os.path.realpath(d)
            first = next((f for f in sorted(os.listdir(real)) if f.endswith(".bin") and not f.endswith(".exit.bin")),
                         None) if os.path.isdir(real) else None
            h = header(os.path.join(real, first)) if first else None
            if h is not None:
                out[h[17] & ~1].add(real)
    return out


def calls(dirs: set[str]):
    """(entry header, exit header) of every captured call that returned to its LR."""
    for d in sorted(dirs):
        for f in sorted(os.listdir(d)):
            if not f.endswith(".exit.bin"):
                continue
            entry, exit_ = header(os.path.join(d, f.removesuffix(".exit.bin") + ".bin")), header(os.path.join(d, f))
            if entry is not None and exit_ is not None and exit_[17] & ~1 == entry[16] & ~1 and exit_[15] == entry[15]:
                yield entry, exit_


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("graph")
    ap.add_argument("-o", "--out", default=os.path.join(ROOT, "build", "abi", "captures.json"))
    ap.add_argument("--dirs", nargs="+", default=DEFAULT_DIRS, help="glob patterns of capture directories")
    ap.add_argument("--blockers", default="keep", help="check functions all of whose blockers are in this list "
                                                         "(comma-separated: keep, flags_out)")
    args = ap.parse_args()
    allowed = set(args.blockers.split(","))
    g = json.load(open(args.graph))
    key = "proposal_final" if any("proposal_final" in r for r in g["entries"].values()) else "proposal_observed"
    wanted = {n: r for n, r in g["entries"].items()
              if r["readable"] and r.get(key, {}).get("blockers") and set(r[key]["blockers"]) <= allowed}
    idx = index(args.dirs)
    result: dict[str, dict] = {}
    for name, r in sorted(wanted.items()):
        regs = [int(x[1:]) for x in r[key]["keep"]]
        flags = r[key]["flags_out"] if "flags_out" in r[key]["blockers"] else []
        dirs = idx.get(int(r["addr"], 16), set())
        n = 0
        changed = {**{f"r{k}": 0 for k in regs}, **{f: 0 for f in flags}}
        for entry, exit_ in calls(dirs):
            n += 1
            for k in regs:
                if exit_[2 + k] != entry[2 + k]:
                    changed[f"r{k}"] += 1
            for f in flags:
                if (exit_[18] ^ entry[18]) & FLAG_BITS[f]:
                    changed[f] += 1
        result[name] = {"calls": n, "changed": changed, "blockers": r[key]["blockers"],
                        "dirs": [os.path.relpath(d, ROOT) for d in sorted(dirs)]}
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w") as fh:
        json.dump({"proposals": key, "blockers": sorted(allowed), "functions": result}, fh, indent=1)
    summary = {
        "checked": len(result),
        "no_captures": sum(1 for x in result.values() if x["calls"] == 0),
        "unchanged_in_every_call": sum(1 for x in result.values() if x["calls"] and not any(x["changed"].values())),
        "changed_in_some_call": sum(1 for x in result.values() if any(x["changed"].values())),
        "captured_functions_indexed": len(idx),
        "out": os.path.relpath(args.out, ROOT),
    }
    json.dump(summary, sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main()
