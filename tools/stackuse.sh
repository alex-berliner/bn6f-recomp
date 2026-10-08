#!/usr/bin/env bash
# Host stack high-water marks of a C-port ROM after playing a trace.
#   tools/stackuse.sh ROM TRACE GAME_FRAMES
# boot.c paints the main stack (__bss_end..0x3006A00) with 0xA5A5A5A5 and
# the IRQ stack (0x3006A00..0x3007900) with 0x5A5A5A5A.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROM=$1 TRACE=$2 N=$3
ELF="${ROM%.gba}.elf"
TMP=$(mktemp -d)
"$ROOT/build-tools/mgba_run" "$ROM" --quiet --frames $((N * 3)) --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
  --input "$TRACE" --game-frame-addr 0x020093D4 --until-game-frame "$N" --dump-ram "$TMP/ram" >/dev/null
BSS_END=$(arm-none-eabi-nm "$ELF" | awk '$3=="__bss_end"{print $1}')
python3 - "$TMP/ram.iwram.bin" "$BSS_END" <<'PY'
import sys
iw = open(sys.argv[1], "rb").read()
bss_end = (int(sys.argv[2], 16) + 3) & ~3
def low_water(lo, hi, pat):
    a = lo
    while a < hi and iw[a - 0x03000000:a - 0x03000000 + 4] == pat:
        a += 4
    return a
m = low_water(bss_end, 0x03006A00, b"\xa5" * 4)
i = low_water(0x03006A00, 0x03007900, b"\x5a" * 4)
print(f"main stack: {0x03006A00 - m:5d} of {0x03006A00 - bss_end} bytes used (lowest sp {m:#x})")
print(f"IRQ stack:  {0x03007900 - i:5d} of {0x03007900 - 0x03006A00} bytes used (lowest sp {i:#x})")
PY
rm -rf "$TMP"
