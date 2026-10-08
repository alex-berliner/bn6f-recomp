#!/usr/bin/env python3
"""Cycles per source line from an mgba_run --profile-game profile.

  tools/lineprof.py PROFILE ELF [LO HI] [--top N]

Maps each sampled PC in [LO, HI) (default: the IWRAM block) to its source line
with addr2line (build the ELF with OPT="-Os -g"; the code is identical) and
prints, per function, the cycles of each line, so rarely run paths of hot
functions stand out. Lines are attributed to the innermost inlined function.
"""
import collections, subprocess, sys

args = [a for a in sys.argv[1:] if not a.startswith("--")]
top = int(sys.argv[sys.argv.index("--top") + 1]) if "--top" in sys.argv else 1000
if "--top" in sys.argv:
    args.remove(str(top))
prof, elf = args[0], args[1]
lo = int(args[2], 16) if len(args) > 2 else 0x03005700
hi = int(args[3], 16) if len(args) > 3 else 0x03008000

cyc = collections.Counter()
for l in open(prof):
    if l.startswith("#"):
        continue
    pc, c = l.split()
    pc = int(pc, 16)
    if lo <= pc < hi:
        cyc[pc] += int(c)

pcs = sorted(cyc)
# addr2line -i prints a (function, file:line) pair per inline level, so a
# batch query cannot be split back per address: query one at a time.
def locate(pc):
    r = subprocess.run(["arm-none-eabi-addr2line", "-f", "-i", "-e", elf, hex(pc)],
                       capture_output=True, text=True).stdout.splitlines()
    frames = [(r[i], r[i + 1].split("/")[-1].split(" ")[0]) for i in range(0, len(r) - 1, 2)]
    return frames[0], frames[-1][0]     # innermost (function, line), outermost function

total = sum(cyc.values())
by_func = collections.Counter()
by_line = collections.Counter()
for pc in pcs:
    (fn, line), outer = locate(pc)
    by_func[outer] += cyc[pc]
    by_line[(outer, fn, line)] += cyc[pc]

print(f"{total} cycles in [{lo:#x}, {hi:#x})")
for outer, fc in by_func.most_common(top):
    print(f"\n{fc:10d} {100 * fc / total:5.1f}%  {outer}")
    lines = sorted(((k, v) for k, v in by_line.items() if k[0] == outer),
                   key=lambda kv: (kv[0][2].split(":")[0], int(kv[0][2].split(":")[-1]) if kv[0][2].split(":")[-1].isdigit() else 0))
    for (o, fn, line), v in lines:
        print(f"    {v:10d} {100 * v / fc:5.1f}%  {line:24s} {fn if fn != outer else ''}")
