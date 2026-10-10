#!/usr/bin/env python3
"""Find code the disassembly treats as data: Thumb code pointers in the ROM
(odd words, 0x08000000-0x08800000) that point outside every lifted unit and
at a function prologue (push {..., lr}). Such code is never lifted, so the
port stops with "dispatch miss" when the game calls it.

  tools/find_hidden_code.py
"""
import bisect, glob, os, re, subprocess
import gamepaths  # tools/gamepaths.py: the game profile (tools/paths.env)
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rom = open(gamepaths.ORIG_ROM, "rb").read()
# Every instruction the lifter translated has a "/* ADDR: insn */" comment.
# (Unit ranges are not enough: the disassembly can put a data block, and
# code hidden in it, inside a function's start/end markers.)
lifted = set()
for f in glob.glob(os.path.join(ROOT, "cport/src/lifted/*.c")):
    lifted.update(int(a, 16) for a in re.findall(r"^\s*/\* ([0-9A-F]{8}): ", open(f).read(), re.M))
def in_code(t):
    return t in lifted
syms = {}
for l in subprocess.run(["arm-none-eabi-nm", "-n", gamepaths.ORIG_ELF],
                        capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3:
        syms.setdefault(int(p[0], 16), p[2])
sym_addrs = sorted(syms)
def label(t):
    i = bisect.bisect_right(sym_addrs, t) - 1
    a = sym_addrs[i]
    return syms[a] + (f"+{t - a:#x}" if t != a else "")
found = {}
for off in range(0, len(rom) - 3, 2):     # literal pools and tables are word aligned; halfword for safety
    v = int.from_bytes(rom[off:off + 4], "little")
    if not (v & 1) or not (0x08000000 <= v < 0x08800000):
        continue
    t = v - 1
    if in_code(t):
        continue
    hw = int.from_bytes(rom[t - 0x08000000:t - 0x08000000 + 2], "little")
    if hw & 0xFF00 == 0xB500:             # push {..., lr}
        found.setdefault(t, []).append(0x08000000 + off)
for t, refs in sorted(found.items()):
    print(f"{t:#010x} {label(t):40s} referenced from {', '.join(hex(r) for r in refs[:4])}"
          f"{' ...' if len(refs) > 4 else ''}")
print(f"{len(found)} candidate functions hidden as data")
