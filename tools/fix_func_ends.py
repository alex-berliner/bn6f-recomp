#!/usr/bin/env python3
"""Classify (and with --apply, fix) bn6f functions whose thumb_func_end sits
before code that still belongs to them.

Category A: the instruction right before the early .size end falls through
into the trailing code, so the end marker was simply misplaced; move it to
just before the next function's start directive.
Category B: anything else (trailing code starts fresh): listed for review.
"""
import bisect
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lifter import elfinfo, analysis  # noqa: E402
from lifter.lift import COPIES, COPY_SEEDS  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APPLY = "--apply" in sys.argv

info = elfinfo.load(os.path.join(ROOT, "bn6f"))
p = analysis.build(info, COPIES, COPY_SEEDS)
rom_insns = sorted(a for (a, t) in p.insns if 0x08000000 <= a < 0x08800000)
fs = sorted(info.funcs)
cat_a, cat_b = [], []
for k, a in enumerate(fs):
    mode, size, name = info.funcs[a]
    nxt = fs[k + 1] if k + 1 < len(fs) else a + 0x100000
    end = a + size
    if not size:
        continue
    lo = bisect.bisect_left(rom_insns, end)
    stray = [x for x in rom_insns[lo:] if x < nxt]
    if not stray:
        continue
    # the last instruction inside the declared size
    prev = max((x for x in rom_insns[bisect.bisect_left(rom_insns, a):lo] if x < end), default=None)
    ins = p.insns.get((prev, mode == "t")) if prev is not None else None
    falls = ins is not None and prev + ins.size == end and not (
        (ins.kind == "b" and not ins.link and ins.cond == "al") or
        (analysis.writes_pc(ins) and ins.kind != "b" and ins.cond == "al"))
    (cat_a if falls else cat_b).append((name, a, end, stray[0], stray[-1], nxt))

print(f"category A (misplaced end, fall-through): {len(cat_a)}")
print(f"category B (needs review): {len(cat_b)}")
for name, a, end, s0, s1, nxt in cat_b:
    print(f"  B {name} {a:#x}: size ends {end:#x}, stray {s0:#x}-{s1:#x}, next func {nxt:#x}")

if APPLY:
    # Edit sources: move `thumb_func_end NAME` to just before the next
    # function start directive in the same file.
    start_re = re.compile(r"^\s*(thumb_func_start|thumb_local_start|arm_func_start|arm_local_start)\b")
    files = {}
    for root, _d, fl in os.walk(os.path.join(ROOT, "bn6f")):
        if "/.git" in root or "/tools" in root:
            continue
        for f in fl:
            if f.endswith(".s"):
                files[os.path.join(root, f)] = None
    by_name = {x[0]: x for x in cat_a}
    for path in files:
        lines = open(path, errors="replace").read().split("\n")
        changed = False
        i = 0
        while i < len(lines):
            m = re.match(r"^\s*thumb_func_end\s+(\S+)", lines[i])
            if m and m.group(1) in by_name:
                j = next((j for j in range(i + 1, len(lines)) if start_re.match(lines[j])), len(lines))
                end_line = lines.pop(i)
                j -= 1
                # keep blank lines/comments directly above the next start with it
                while j - 1 > i and (lines[j - 1].strip() == "" or lines[j - 1].strip().startswith("//")):
                    j -= 1
                lines.insert(j, end_line)
                changed = True
                del by_name[m.group(1)]
                continue
            i += 1
        if changed:
            open(path, "w").write("\n".join(lines))
    print(f"applied; not found in sources: {sorted(by_name)}")
