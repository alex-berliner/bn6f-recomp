#!/usr/bin/env python3
"""ELF -> .gba for the lifted build: objcopy to binary, then patch the ROM
header's entry instruction (word 0) to branch to the C runtime's _start."""
import subprocess
import sys

elf, out = sys.argv[1], sys.argv[2]
subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", elf, out], check=True)
syms = subprocess.run(["arm-none-eabi-nm", elf], check=True, capture_output=True, text=True).stdout
start = next(int(l.split()[0], 16) for l in syms.splitlines() if l.split()[-1] == "_start")
off = (start - 0x08000008) >> 2
assert 0 <= off < (1 << 23), hex(start)
rom = bytearray(open(out, "rb").read())
rom[0:4] = (0xEA000000 | off).to_bytes(4, "little")
# Pad to a power-of-two-friendly size (multiple of 256 KB).
pad = (-len(rom)) % 0x40000
rom += b"\xff" * pad
open(out, "wb").write(rom)
print(f"{out}: {len(rom):#x} bytes, entry -> {start:#010x}")
