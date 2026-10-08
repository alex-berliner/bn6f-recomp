#!/usr/bin/env python3
"""Build a ROM-check ROM: tools/probe/mkromcheck.py PORT.gba OUT.gba

PORT.gba (13.5 MB, its _start at 0x08800000) with romcheck.c appended at
0x08D80000 and the header's first instruction pointed at it; the hash of
every 64 KB block of PORT.gba is stored at 0x08D81000 (see romcheck.c)."""
import os, struct, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
subprocess.run(["arm-none-eabi-gcc", "-mcpu=arm7tdmi", "-mthumb", "-mthumb-interwork", "-O2", "-nostdlib",
                "-nostartfiles", "-T", "romcheck.ld", "romcheck.s", "romcheck.c", "-o", "romcheck.elf"], cwd=here, check=True)
subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "romcheck.elf", "romcheck.bin"], cwd=here, check=True)
code = open(os.path.join(here, "romcheck.bin"), "rb").read()
for f in ("romcheck.elf", "romcheck.bin"):
    os.remove(os.path.join(here, f))
assert len(code) <= 0x1000, len(code)
rom = bytearray(open(sys.argv[1], "rb").read())
assert len(rom) == 0xD80000 and struct.unpack_from("<I", rom, 0)[0] == 0xEA1FFFFE
def h(data):
    x = 0x12345678
    for (w,) in struct.iter_unpack("<I", data):
        x = (((x << 5) | (x >> 27)) & 0xFFFFFFFF) ^ w
    return x
n = len(rom) // 0x10000
table = [n] + [h(rom[i * 0x10000 + (0x200 if i == 0 else 0):(i + 1) * 0x10000]) for i in range(n)]
rom += code + b"\xFF" * (0x1000 - len(code)) + struct.pack("<%dI" % len(table), *table)
struct.pack_into("<I", rom, 0, 0xEA000000 | ((0xD80000 - 8) >> 2))
rom[0xBD] = (-sum(rom[0xA0:0xBD]) - 0x19) & 0xFF
open(sys.argv[2], "wb").write(rom)
print(f"{n} blocks, checker {len(code)} bytes")
