#!/usr/bin/env python3
"""List asm sites where a Thumb shift/rotate sets carry and bcc/bcs/blo/bhs (within the next two
instructions) consumes it, inside functions that have a readable C body, with the C lines that
shift. The OK/WRONG judgement is manual (docs/reviews/shift-carry-sweep.md).

Carry rules: lsr/asr #N -> C = bit N-1 of Rm (#32 -> bit 31); lsl #N -> C = bit 32-N;
ror #N -> bit N-1; register amount -> bit (n-1) / (32-n) for the runtime n.

usage: tools/check_shift_carry.py [ASM_DIR] [--all]   (--all also lists functions with no readable C)
"""
from __future__ import annotations

import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funcindex  # noqa: E402

SHIFT = re.compile(r"^(lsr|asr|lsl|ror)\s+(\w+),\s*(\w+)(?:,\s*#(\d+))?\s*(?://.*)?$")
BR = re.compile(r"^(bcc|bcs|blo|bhs)\s+(\S+)")
START = re.compile(r"^\s*(?:thumb|arm)_(?:func|local)_start\b")
LABEL = re.compile(r"^(\w+):")
END = re.compile(r"^\s*(?:thumb|arm)_func_end\s+(\w+)")
# instructions that leave C alone (movs only sets N,Z; loads/stores/push/pop touch no flags)
KEEP = re.compile(r"^(ldr|ldrb|ldrh|ldrsb|ldrsh|str|strb|strh|mov|push|pop|nop)\b")
CSHIFT = re.compile(r">>|<<|ror|rotate|carry|& *(?:1|2|4|8|0x[0-9a-fA-F]+)u?\\b|\\bbit\\b", re.I)


def bit(op: str, rd: str, rm: str, n: str | None) -> str:
    if n is None:  # register amount (two-operand form: Rd = Rd shifted by Rm)
        return f"{op} by {rm}: " + ("bit (n-1)" if op != "lsl" else "bit (32-n)") + f" of {rd}"
    k = int(n)
    if op == "lsl":
        return f"bit {32 - k} of {rm}" if k else "C unchanged"
    return f"bit {k - 1} of {rm}"


def c_body(path: str, name: str) -> tuple[int, list[tuple[int, str]]]:
    lines = open(path, encoding="utf-8", errors="replace").read().split("\n")
    pat = re.compile(r"\b" + re.escape(name) + r"\s*\(")
    start = next((i for i, l in enumerate(lines) if pat.search(l) and l.rstrip().endswith("{")), None)
    if start is None:
        start = next((i for i, l in enumerate(lines) if re.search(r"\b" + re.escape(name) + r"\b", l)), 0)
    out = []
    hdr = next((i for i, l in enumerate(lines) if l.startswith("/* ── " + name + " ──")), None)
    if hdr is not None:  # the generated section: typed body and wrapper up to the next section
        end = next((i for i in range(hdr + 1, len(lines)) if lines[i].startswith("/* ── ")), len(lines))
        rng = range(hdr, end)
        start = hdr
    else:
        end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("}")), len(lines))
        rng = range(start, end)
    for i in rng:
        if CSHIFT.search(lines[i]):
            out.append((i + 1, lines[i].strip()))
    return start + 1, out


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    show_all = "--all" in sys.argv
    asmdir = args[0] if args else None
    if not asmdir:
        root = funcindex.ROOT
        asmdir = os.path.join(root, "bn6f", "asm")
        if not os.path.isdir(asmdir):  # worktrees: the main checkout holds the (untracked) asm
            asmdir = os.path.join(root, "..", "..", "bn6f", "asm")
    readable = funcindex.readable_functions()
    sites = 0
    funcs = set()
    for f in sorted(glob.glob(os.path.join(asmdir, "*.s"))):
        func = None
        ins: list[tuple[int, str]] = []
        for no, raw in enumerate(open(f, encoding="utf-8", errors="replace"), 1):
            if START.match(raw):
                func, ins = "", []  # named by the label that follows
                continue
            m = LABEL.match(raw)
            if m and func == "":
                func = m.group(1)
                continue
            s = raw.split("//")[0].strip() if not raw.strip().startswith("//") else ""
            if not s or s.endswith(":") or s.startswith("."):
                continue
            ins.append((no, s))
            ins = ins[-4:]
            b = BR.match(s)
            if not b or not func:
                continue
            # walk back over at most two instructions (before the branch) to the shift
            for back in (1, 2, 3):
                if len(ins) <= back:
                    break
                ln, prev = ins[-1 - back]
                m2 = SHIFT.match(prev)
                if m2:
                    op, rd, rm, n = m2.groups()
                    two = prev.count(",") == 1  # `lsl Rd, Rs`: register amount
                    desc = bit(op, rd, rm, None if two else n)
                    cpath = readable.get(func)
                    if cpath or show_all:
                        sites += 1
                        funcs.add(func)
                        print(f"{func}  {os.path.basename(f)}:{ln}  `{prev}` ... `{s}`  asm tests: {desc} "
                              f"({'==0' if b.group(1) in ('bcc', 'blo') else '==1'} -> branch)")
                        if cpath:
                            cl, body = c_body(cpath, func)
                            print(f"    C {os.path.relpath(cpath, funcindex.ROOT)}:{cl}")
                            for l, t in body:
                                print(f"      {l}: {t}")
                    break
                if not KEEP.match(prev):
                    break
    print(f"# {sites} sites in {len(funcs)} readable functions", file=sys.stderr)


if __name__ == "__main__":
    main()
