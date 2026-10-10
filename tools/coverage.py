#!/usr/bin/env python3
"""Which lifted functions a set of runs of the original ROM executed.

  tools/coverage.py PROFILE... [--list FILE]

PROFILE: profile.txt files written by mgba_run --profile-game on
bn6f/bn6f.gba (one line per executed PC). Each PC is mapped to the lifted
unit (cport/src/lifted header comments) containing it. Prints the union
coverage; --list writes the names of the executed units.
"""
import sys

import funcindex  # tools/funcindex.py: the function index (lifted units and their ranges)

args = sys.argv[1:]
listfile = None
if "--list" in args:
    i = args.index("--list"); listfile = args[i + 1]; del args[i:i + 2]

ranges = funcindex.unit_ranges()          # a few headers have bogus ends: those units count as empty
unit_at = funcindex.unit_finder(ranges)

hit = set()
for prof in args:
    for line in open(prof):
        if line[0] == "#":
            continue
        name = unit_at(int(line.split()[0], 16))
        if name is not None:
            hit.add(name)

size = {name: hi - lo for name, (lo, hi) in ranges.items()}
tot = sum(size.values())
print(f"{len(hit)} of {len(ranges)} lifted units executed ({100 * len(hit) / len(ranges):.1f}%), "
      f"{100 * sum(size[n] for n in hit) / tot:.1f}% of lifted code bytes ({len(args)} profiles)")
if listfile:
    with open(listfile, "w") as fh:
        for name in sorted(hit, key=lambda n: ranges[n][0]):
            fh.write(f"{name}\n")
