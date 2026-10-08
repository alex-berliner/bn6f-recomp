#!/usr/bin/env python3
"""Audit the bn6f disassembly for defects the lifter runs into.

Reports:
  code-as-data   reachable instructions inside $d (data) mapping regions,
                 i.e. code the disassembly emits as .word/.byte
  short-size     FUNC symbols whose .size stops before reachable code that
                 still belongs to them (thumb_func_end placed too early)
  raw-pointer    `.word 0x08xxxxxx` literals in the source that equal the
                 address of a label (should be the label itself)
"""
from __future__ import annotations

import bisect
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lifter import elfinfo, analysis  # noqa: E402
from lifter.lift import COPIES, COPY_SEEDS  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def where(info, addr):
    """Nearest preceding symbol + offset, and its source location if known."""
    labs = sorted(info.labels)
    k = bisect.bisect_right(labs, addr) - 1
    nm = info.labels[labs[k]] if k >= 0 else "?"
    off = addr - labs[k] if k >= 0 else 0
    return f"{nm}+{off:#x}"


def main():
    info = elfinfo.load(os.path.join(ROOT, "bn6f"))
    p = analysis.build(info, COPIES, COPY_SEEDS)
    mapping = info.mapping
    mstarts = [a for a, _ in mapping]

    def kind(a):
        k = bisect.bisect_right(mstarts, a) - 1
        return mapping[k][1] if k >= 0 else None

    # ── code as data ────────────────────────────────────────────────────
    rom_insns = sorted(a for (a, _t) in p.insns if 0x08000000 <= a < 0x08800000)
    bad = [a for a in rom_insns if kind(a) == "d"]
    runs = []
    for a in bad:
        if runs and a - runs[-1][1] <= 4:
            runs[-1][1] = a
        else:
            runs.append([a, a])
    print(f"== code-as-data: {len(bad)} instructions in {len(runs)} runs")
    for lo, hi in runs:
        print(f"  {lo:#010x}-{hi:#010x}  {where(info, lo)}")

    # ── short FUNC sizes ────────────────────────────────────────────────
    fs = sorted(info.funcs)
    short = []
    for k, a in enumerate(fs):
        mode, size, name = info.funcs[a]
        nxt = fs[k + 1] if k + 1 < len(fs) else a + 0x100000
        end = a + size
        stray = [x for x in rom_insns if end <= x < nxt] if size else []
        if size and stray:
            short.append((name, a, end, stray[-1]))
    print(f"\n== short-size: {len(short)} functions end before their last instruction")
    for name, a, end, last in short[:40]:
        print(f"  {name} {a:#010x}: .size ends {end:#010x}, code continues to {last:#010x}")

    # ── raw pointers in source ──────────────────────────────────────────
    word_re = re.compile(r"\.word\s+(0x0?8[0-7][0-9A-Fa-f]{5})\b")
    label_addrs = info.labels
    hits = []
    for root, _d, files in os.walk(os.path.join(ROOT, "bn6f")):
        if "/.git" in root or "/tools" in root:
            continue
        for f in files:
            if not f.endswith(".s"):
                continue
            path = os.path.join(root, f)
            for n, line in enumerate(open(path, errors="replace"), 1):
                for m in word_re.finditer(line):
                    v = int(m.group(1), 16)
                    if v == 0x08000000:
                        continue  # a flag bit, not a pointer
                    tgt = label_addrs.get(v & ~1)
                    if tgt:
                        hits.append((os.path.relpath(path, ROOT), n, m.group(1), tgt + ("+1" if v & 1 else "")))
    print(f"\n== raw-pointer: {len(hits)} .word literals equal to a label address")
    for path, n, val, lab in hits[:60]:
        print(f"  {path}:{n}: {val} -> {lab}")


if __name__ == "__main__":
    main()
