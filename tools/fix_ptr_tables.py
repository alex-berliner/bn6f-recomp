#!/usr/bin/env python3
"""Fix bn6f data tables that were disassembled as Thumb instructions.

Pattern: a function ends with `off_X: .word loc_Y` and `thumb_func_end F`;
`loc_Y` is the byte table F indexes (the pointer has no Thumb bit), but it was
emitted as instructions, sometimes followed by a real, unlabelled function.
The table becomes `byte_Y: .byte ...` (exact ROM bytes); a following function
(first `push {..., lr}` line) gets `thumb_local_start` / `sub_<addr>` /
`thumb_func_end`. The ROM must stay identical (check with `make`).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lifter import elfinfo  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
info = elfinfo.load(os.path.join(ROOT, "bn6f"))
START_RE = re.compile(r"^\s*(thumb_func_start|thumb_local_start|arm_func_start|arm_local_start)\b")

# line -> address for assembled instructions, per file basename
line_addr: dict[tuple[str, int], int] = {}
for a, (f, ln) in info.lines.items():
    line_addr.setdefault((f, ln), a)

fixed = []
for root, _d, files in os.walk(os.path.join(ROOT, "bn6f")):
    if "/.git" in root or "/tools" in root:
        continue
    for fname in files:
        if not fname.endswith(".s"):
            continue
        path = os.path.join(root, fname)
        lines = open(path, errors="replace").read().split("\n")
        orig_n = len(lines)
        changed = False
        # walk from the bottom so line numbers above stay valid for DWARF lookups
        i = len(lines) - 4
        while i > 0:
            m_end = re.match(r"^\s*thumb_func_end\s+(\S+)", lines[i])
            m_word = re.match(r"^\s*\.word\s+(loc_([0-9A-F]+))\s*$", lines[i - 1]) if i else None
            if not (m_end and m_word):
                i -= 1
                continue
            loc, yhex = m_word.group(1), m_word.group(2)
            y = int(yhex, 16)
            # find `loc_Y:` after the end marker
            j = next((k for k in range(i + 1, min(i + 4, len(lines))) if lines[k].startswith(loc + ":")), None)
            if j is None:
                i -= 1
                continue
            nxt = next((k for k in range(j + 1, len(lines)) if START_RE.match(lines[k])), None)
            if nxt is None:
                i -= 1
                continue
            # first prologue line inside (j, nxt)
            pro = None
            for k in range(j + 1, nxt):
                if re.match(r"^\s*push\s*\{[^}]*lr\}", lines[k]):
                    pro = k
                    break
                if re.match(r"^[A-Za-z_]\w*:", lines[k]):
                    break   # another label: leave this one for manual review
            end_line = pro if pro is not None else nxt
            if pro is not None:
                p_addr = line_addr.get((fname, pro + 1))
            else:
                # data runs up to the next function's first instruction
                p_addr = next((line_addr.get((fname, k + 1)) for k in range(nxt, nxt + 4)
                               if line_addr.get((fname, k + 1))), None)
            if p_addr is None or p_addr <= y:
                print(f"skip {loc} in {fname}: cannot size table")
                i -= 1
                continue
            # only instruction lines (or blank/comment) may be converted
            body = lines[j + 1:end_line]
            if any(re.match(r"^\s*\.", l) or re.match(r"^[A-Za-z_]\w*:", l) for l in body):
                print(f"skip {loc} in {fname}: table body has directives/labels")
                i -= 1
                continue
            data = info.rom[y - 0x08000000:p_addr - 0x08000000]
            new = [f"byte_{yhex}:"]
            for k in range(0, len(data), 12):
                new.append("\t.byte " + ", ".join(f"{b:#x}" if b > 9 else str(b) for b in data[k:k + 12]))
            end_marker = lines[i]
            repl = new + [end_marker, ""]
            if pro is not None:
                repl += ["\tthumb_local_start", f"sub_{p_addr:X}:"] + lines[pro:nxt]
                repl += [f"\tthumb_func_end sub_{p_addr:X}"]
            else:
                repl += lines[end_line:nxt]
            # lines[i] (end marker) .. nxt-1 replaced
            lines[i:nxt] = repl
            fixed.append((loc, f"byte_{yhex}", len(data), f"sub_{p_addr:X}" if pro is not None else None))
            changed = True
            i -= 1
        if changed:
            text = "\n".join(lines)
            for loc, byte, _n, _s in fixed:
                text = re.sub(rf"\b{loc}\b", byte, text)
            open(path, "w").write(text)

for loc, byte, n, sub in fixed:
    print(f"{loc} -> {byte} ({n} bytes){'; then ' + sub if sub else ''}")
print(f"{len(fixed)} tables fixed")
