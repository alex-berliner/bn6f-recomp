#!/usr/bin/env python3
"""Replace raw `.word 0x08xxxxxx` data pointers in a bn6f source file with
labels, inserting `byte_XXXXXXX:` labels inside data blocks (splitting
`.byte` lines) where the target has none yet.

    label_data.py FILE ADDR [ADDR ...]

Only `.byte` / `.word` / `.hword` lines and labels may lie between the
nearest preceding labelled address and the target. The ROM must stay
identical (check with `make`).
"""
import bisect
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lifter import elfinfo  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
path = sys.argv[1]
targets = [int(x, 16) for x in sys.argv[2:]]
info = elfinfo.load(os.path.join(ROOT, "bn6f"))
lines = open(path).read().split("\n")
LABEL = re.compile(r"^([A-Za-z_]\w*)::?\s*(//.*)?$")


def line_size(l):
    s = l.split("//")[0].strip()
    if s.startswith(".byte"):
        return len([x for x in s[5:].split(",") if x.strip()])
    if s.startswith(".word"):
        return 4 * len([x for x in s[5:].split(",") if x.strip()])
    if s.startswith(".hword"):
        return 2 * len([x for x in s[6:].split(",") if x.strip()])
    if s == "" or LABEL.match(l.strip()):
        return 0
    return None


for t in sorted(targets, reverse=True):
    name = f"byte_{t:X}"
    if t in info.labels:
        name = info.labels[t]
    else:
        # nearest preceding labelled address in this file
        cand = [(info.label_names.get(m.group(1)), i) for i, l in enumerate(lines)
                if (m := LABEL.match(l.strip())) and info.label_names.get(m.group(1)) is not None]
        cand = [(a, i) for a, i in cand if a <= t and t - a < 0x10000]
        a, i = max(cand)
        addr = a
        k = i + 1
        while True:
            sz = line_size(lines[k])
            if sz is None:
                raise SystemExit(f"{t:#x}: cannot walk over line {k + 1}: {lines[k]!r}")
            if addr + sz > t:
                break
            addr += sz
            k += 1
        off = t - addr
        l = lines[k]
        if off == 0:
            lines.insert(k, f"{name}:")
        else:
            assert l.strip().startswith(".byte"), (hex(t), l)
            vals = [x.strip() for x in l.split("//")[0].strip()[5:].split(",") if x.strip()]
            lines[k:k + 1] = ["\t.byte " + ", ".join(vals[:off]), f"{name}:",
                              "\t.byte " + ", ".join(vals[off:])]
    text = "\n".join(lines)
    text = re.sub(rf"\.word\s+0x0?{t:X}\b", f".word {name}", text, flags=re.I)
    # drop the "would mess up" notes directly above the fixed pointer
    lines = text.split("\n")
    print(f"{t:#x} -> {name}")
text = "\n".join(lines)
text = re.sub(r"\n\s*// This pointer would mess up[^\n]*(?=\n\s*\.word [A-Za-z_])", "", text)
open(path, "w").write(text)
