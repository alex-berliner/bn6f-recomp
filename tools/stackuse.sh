#!/usr/bin/env bash
# Host stack high-water marks of a C-port ROM after playing a trace.
#   tools/stackuse.sh ROM TRACE GAME_FRAMES
# boot.c paints the main stack (__bss_end..__sys_stack_top) with 0xA5A5A5A5
# and the IRQ stack (__sys_stack_top..__irq_stack_top) with 0x5A5A5A5A.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROM=$1 TRACE=$2 N=$3
ELF="${ROM%.gba}.elf"
TMP=$(mktemp -d)
"$ROOT/build-tools/mgba_run" "$ROM" --quiet --frames $((N * 3)) --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
  --input "$TRACE" --game-frame-addr 0x020093D4 --until-game-frame "$N" --dump-ram "$TMP/ram" >/dev/null
sym() { arm-none-eabi-nm "$ELF" | awk -v n="$1" '$3==n{print $1}'; }
python3 - "$TMP/ram.iwram.bin" "$(sym __bss_end)" "$(sym __sys_stack_top)" "$(sym __irq_stack_top)" "$(sym __sys_stack_limit)" <<'PY'
import sys
iw = open(sys.argv[1], "rb").read()
bss_end = (int(sys.argv[2], 16) + 3) & ~3
SYS, IRQ, LIMIT = (int(x, 16) for x in sys.argv[3:6])
def low_water(lo, hi, pat):
    a = lo
    while a < hi and iw[a - 0x03000000:a - 0x03000000 + 4] == pat:
        a += 4
    return a
m = low_water(bss_end, SYS, b"\xa5" * 4)
i = low_water(SYS, IRQ, b"\x5a" * 4)
print(f"main stack: {SYS - m:5d} of {SYS - LIMIT} bytes used (lowest sp {m:#x}; {LIMIT - bss_end} spare below)")
print(f"IRQ stack:  {IRQ - i:5d} of {IRQ - SYS} bytes used (lowest sp {i:#x})")
print(f"IWRAM code + bss: {bss_end - 0x03005700} bytes; free for code: {LIMIT - bss_end}")
PY
rm -rf "$TMP"
