#!/usr/bin/env python3
"""Build a cart timing probe: tools/probe/mktiming.py PORT.gba OUT.gba

PORT.gba (13.5 MB) with timing.c appended at 0x08D80000 and the header's
first instruction pointed at it; hashes of PORT.gba's 64 KB blocks at
0x08D88000 (see timing.c). exec_test is generated here: straight-line
Thumb code (adds/lsls/eors, a branch every 32 instructions) whose result
is computed here too."""
import os, random, struct, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
M = 0xFFFFFFFF
rng = random.Random(6)
r0 = r1 = 0
lines = [".syntax unified", ".thumb", ".section .text.exec, \"ax\"", ".global exec_test", ".thumb_func",
         "exec_test:", "    movs r0, #0", "    movs r1, #0"]
for i in range(6000):
    if i % 32 == 31:
        lines += [f"    b 1f", "1:"]
        continue
    op = rng.randrange(3)
    if op == 0:
        k = rng.randrange(256); lines.append(f"    adds r0, #{k}"); r0 = (r0 + k) & M
    elif op == 1:
        s = rng.randrange(1, 32); lines.append(f"    lsls r1, r0, #{s}"); r1 = (r0 << s) & M
    else:
        lines.append("    eors r0, r1"); r0 ^= r1
lines += ["    bx lr", ".align 2", ".global exec_expected", f"exec_expected: .word {r0:#x}"]
open(os.path.join(here, "exec_test.s"), "w").write("\n".join(lines) + "\n")
try:
    subprocess.run(["arm-none-eabi-gcc", "-mcpu=arm7tdmi", "-mthumb", "-mthumb-interwork", "-O2", "-nostdlib",
                    "-nostartfiles", "-T", "timing.ld", "timing.s", "exec_test.s", "timing.c", "-o", "timing.elf"],
                   cwd=here, check=True)
    subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "timing.elf", "timing.bin"], cwd=here, check=True)
    code = open(os.path.join(here, "timing.bin"), "rb").read()
finally:
    for f in ("exec_test.s", "timing.elf", "timing.bin"):
        if os.path.exists(os.path.join(here, f)):
            os.remove(os.path.join(here, f))
assert len(code) <= 0x8000, hex(len(code))
rom = bytearray(open(sys.argv[1], "rb").read())
assert len(rom) == 0xD80000 and struct.unpack_from("<I", rom, 0)[0] == 0xEA1FFFFE
def h(data):
    x = 0x12345678
    for (w,) in struct.iter_unpack("<I", data):
        x = (((x << 5) | (x >> 27)) & M) ^ w
    return x
n = len(rom) // 0x10000
table = [n] + [h(rom[i * 0x10000 + (0x200 if i == 0 else 0):(i + 1) * 0x10000]) for i in range(n)]
rom += code + b"\xFF" * (0x8000 - len(code)) + struct.pack("<%dI" % len(table), *table)
struct.pack_into("<I", rom, 0, 0xEA000000 | ((0xD80000 - 8) >> 2))
rom[0xBD] = (-sum(rom[0xA0:0xBD]) - 0x19) & 0xFF
open(sys.argv[2], "wb").write(rom)
print(f"{n} blocks, probe {len(code)} bytes, exec_test expects {r0:#010x}")
