#!/usr/bin/env python3
"""Rewrite librfu's IntrSIO32 block in bn6f/asm/libs.s (0x0814CF54-0x0814D8B4)
from `.byte` data into ARM instructions + literal pools.

Code is found by recursive descent from the block start (it is copied to RAM
and run there; branches are PC-relative so the structure is the same at the
ROM address). Words never reached as code stay `.word`. Output must assemble
to the identical ROM; check with `make` (bn6f.sha1).
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lifter import elfinfo  # noqa: E402
from lifter.isa import decode_arm  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
START, END = 0x0814CF54, 0x0814D8B4
SRC = os.path.join(ROOT, "bn6f/asm/libs.s")

info = elfinfo.load(os.path.join(ROOT, "bn6f"))
u32 = info.u32

# ── recursive descent ───────────────────────────────────────────────────
code, literals, targets, calls = set(), set(), set(), set()
work = [START]
while work:
    a = work.pop()
    while START <= a < END and a not in code:
        ins = decode_arm(a, u32(a))
        if ins.kind == "undef":
            break
        code.add(a)
        if ins.kind == "mem" and ins.rn == 15 and ins.op2.is_imm and ins.pre:
            lit = a + 8 + (ins.op2.imm if ins.up else -ins.op2.imm)
            literals.add(lit)
        if ins.kind == "b":
            targets.add(ins.target)
            if ins.link:
                calls.add(ins.target)
            work.append(ins.target)
            if ins.cond == "al" and not ins.link:
                break
        elif ins.kind == "bx" or (ins.kind == "block" and ins.load and 15 in ins.regs) or \
                (ins.kind == "dp" and ins.rd == 15) or (ins.kind == "mem" and ins.load and ins.rd == 15):
            if ins.cond == "al":
                break
        a += 4

# ── objdump the raw block (unified syntax) ──────────────────────────────
blob = info.rom[START - 0x08000000:END - 0x08000000]
tmp = "/tmp/claude-1000/rfu_blob.bin"
os.makedirs(os.path.dirname(tmp), exist_ok=True)
open(tmp, "wb").write(blob)
dis = subprocess.run(["arm-none-eabi-objdump", "-D", "-b", "binary", "-m", "armv4t",
                      f"--adjust-vma={START:#x}", tmp], capture_output=True, text=True).stdout
text = {}
for line in dis.splitlines():
    m = re.match(r"^\s+([0-9a-f]+):\s+[0-9a-f]{8}\s+(.*)$", line)
    if m:
        text[int(m.group(1), 16)] = m.group(2).split("@")[0].split(";")[0].strip()

# ── labels ──────────────────────────────────────────────────────────────
existing = {a: n for a, n in info.labels.items() if START <= a < END}
existing[START] = "IntrSIO32"
labels = dict(existing)
for t in sorted(targets | literals):
    if START <= t < END and t not in labels:
        prefix = "sub" if t in calls else ("dword" if t in literals else "loc")
        labels[t] = f"{prefix}_{t:X}"


def lab(a):
    return labels.get(a)


def addr_operand(m):
    v = int(m.group(1), 16)
    return lab(v) or m.group(0)


out = ["\tthumb_func_end Sio32IDIntr", "",
       "// librfu IntrSIO32 and its helpers (ARM). STWI_init_all DMA-copies this",
       "// whole block to RAM and installs it as the serial interrupt handler.",
       "\tarm_func_start IntrSIO32", "\t.syntax unified"]
a = START
while a < END:
    if a in labels:
        out.append(f"{labels[a]}:")
    w = u32(a)
    if a in code:
        t = text[a]
        mnem, _, ops = t.partition("\t")
        ops = ops.strip()
        m = re.match(r"^(r\d+|ip|fp|sl|sp|lr|pc), \[pc, #(-?\d+)\]$", ops)
        if mnem.startswith("ldr") and m and mnem in ("ldr",) + tuple("ldr" + c for c in
                ("eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le")):
            lit = a + 8 + int(m.group(2))
            ops = f"{m.group(1)}, {labels[lit]}"
        elif re.match(r"^b(l)?([a-z]{2})?$", mnem):
            ops = re.sub(r"0x([0-9a-f]+)", addr_operand, ops)
        out.append(f"\t{mnem} {ops}".rstrip())
    else:
        nm = info.labels.get(w & ~1) if 0x02000000 <= w < 0x0A000000 else None
        if nm and not nm.startswith("$"):
            out.append(f"\t.word {nm}{'+1' if w & 1 and (w & ~1) in info.funcs else ''}"
                       if (w & ~1) == w or (w & ~1) in info.funcs else f"\t.word {w:#010x}")
        else:
            out.append(f"\t.word {w:#010x}")
    a += 4
out += ["\t.syntax divided", "\tarm_func_end IntrSIO32", "\t.thumb", ""]

src = open(SRC).read().split("\n")
i0 = next(i for i, l in enumerate(src) if l.startswith("byte_814CF54:"))
i1 = next(i for i, l in enumerate(src) if "thumb_func_end Sio32IDIntr" in l)
assert i0 < i1
src[i0:i1 + 1] = out
open(SRC, "w").write("\n".join(src))
print(f"code words: {len(code)}, literal words: {len(literals)}, "
      f"data words: {(END - START) // 4 - len(code)}, labels: {len(labels)}")
