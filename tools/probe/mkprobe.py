#!/usr/bin/env python3
"""Build cart_probe.gba: probe code plus markers and tiny Thumb functions at
4, 8, 12 and 13.5 MB, padded to 13.5 MB like the port; header from bn6f.gba
with its own title and game code."""
import os, struct, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(os.path.dirname(here))
code = open(os.path.join(here, "probe.bin"), "rb").read()
rom = bytearray(0xD80000)
rom[:len(code)] = code
orig = open(os.path.join(root, "bn6f/bn6f.gba"), "rb").read()
rom[4:0xC0] = orig[4:0xC0]                         # logo and header fields
rom[0xA0:0xAC] = b"CARTPROBE\0\0\0"
rom[0xAC:0xB0] = b"PRBE"
rom[0xBD] = (-sum(rom[0xA0:0xBD]) - 0x19) & 0xFF   # header complement
for off, val in ((0x400000, 0x11111111), (0x800000, 0x22222222), (0xC00000, 0x33333333), (0xD7FF00, 0x44444444)):
    struct.pack_into("<I", rom, off, val)
for off, ret in ((0x800100, 0x5A), (0xC00100, 0x6B), (0xD7FF80, 0x7C)):
    struct.pack_into("<HH", rom, off, 0x2000 | ret, 0x4770)   # movs r0, #ret; bx lr
open(sys.argv[1], "wb").write(rom)
