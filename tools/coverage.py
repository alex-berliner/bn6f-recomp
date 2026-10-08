#!/usr/bin/env python3
"""Which lifted functions a set of runs of the original ROM executed.

  tools/coverage.py PROFILE... [--list FILE]

PROFILE: profile.txt files written by mgba_run --profile-game on
bn6f/bn6f.gba (one line per executed PC). Each PC is mapped to the lifted
unit (cport/src/lifted header comments) containing it. Prints the union
coverage; --list writes the names of the executed units.
"""
import bisect, glob, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
args = sys.argv[1:]
listfile = None
if "--list" in args:
    i = args.index("--list"); listfile = args[i + 1]; del args[i:i + 2]

units = {}
for f in glob.glob(os.path.join(ROOT, "cport/src/lifted/*.c")):
    for m in re.finditer(r"^/\* (\w+): ([0-9A-F]{8})-([0-9A-F]{8}) (?:THUMB|ARM)", open(f).read(), re.M):
        a, b = int(m.group(2), 16), int(m.group(3), 16)
        units[a] = (m.group(1), b if b - a < 0x10000 else a)   # a few headers have bogus ends
starts = sorted(units)

hit = set()
for prof in args:
    for line in open(prof):
        if line[0] == "#":
            continue
        pc = int(line.split()[0], 16)
        i = bisect.bisect_right(starts, pc) - 1
        if i >= 0 and pc < units[starts[i]][1]:
            hit.add(starts[i])

size = {a: units[a][1] - a for a in starts}
tot = sum(size.values())
print(f"{len(hit)} of {len(starts)} lifted units executed ({100 * len(hit) / len(starts):.1f}%), "
      f"{100 * sum(size[a] for a in hit) / tot:.1f}% of lifted code bytes ({len(args)} profiles)")
if listfile:
    with open(listfile, "w") as fh:
        for a in sorted(hit):
            fh.write(f"{units[a][0]}\n")
