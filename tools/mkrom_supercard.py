#!/usr/bin/env python3
"""Make a port ROM for Supercard flashcarts running SuperFW.

  tools/mkrom_supercard.py IN.gba OUT.gba [WAITCNT]

SuperFW recognises the original's game code (BR6E) and patches the ROM
automatically, adding its in-game menu; the port does not boot on hardware
that way (white screen). This ROM uses its own game code (B6CP), which
SuperFW does not know, so it asks how to patch: choose "No patching". The
one patch the Supercard's slow memory needs is applied here instead: the
WAITCNT value the startup code stores (literal at 0x08000204, 0x45B4 in the
original, too fast for the Supercard) becomes WAITCNT (default 0x4000:
slowest waitstates with prefetch, what SuperFW sets for every game it
launches). Nothing in the game reads its header's game code.
"""
import struct, sys

GAME_CODE = b"B6CP"


def main():
    src, dst = sys.argv[1], sys.argv[2]
    waitcnt = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x4000
    rom = bytearray(open(src, "rb").read())
    if rom[0xAC:0xB0] != b"BR6E" or struct.unpack_from("<I", rom, 0x108)[0] != 0xE5801000 \
            or struct.unpack_from("<II", rom, 0x200) != (0x04000204, 0x45B4):
        sys.exit(f"{src}: not an unpatched BN6 Falzar port ROM")
    struct.pack_into("<I", rom, 0x204, waitcnt)
    rom[0xAC:0xB0] = GAME_CODE
    rom[0xBD] = (-sum(rom[0xA0:0xBD]) - 0x19) & 0xFF
    open(dst, "wb").write(rom)
    print(f"{dst}: game code {GAME_CODE.decode()}, WAITCNT {waitcnt:#06x}")


if __name__ == "__main__":
    main()
