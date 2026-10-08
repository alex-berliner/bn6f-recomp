#!/usr/bin/env bash
# speed.sh ROM [GAME_FRAMES] — hardware frames needed to reach a game frame,
# with input replayed by game frame (same work for every build). Prints the
# speed relative to the original ROM.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
N=${2:-2500}
TRACE=${TRACE_FILE:-$ROOT/tools/traces/title_mash_a.trace}
# toolkit (r10) = 0x020093B0; [toolkit + oToolkit_CurFramePtr (0x24)] -> u16 counter
run() {
  "$ROOT/build-tools/mgba_run" "$1" --frames 40000 --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
      --input "$TRACE" --game-frame-addr 0x020093D4 --until-game-frame "$N" --quiet |
      awk '/reached game frame/ {print $NF}'
}
base=$(run "$ROOT/bn6f/bn6f.gba")
hw=$(run "$1")
echo "$1: game frame $N at hw frame $hw (original: $base) -> $(( 100 * base / hw ))% speed"
# CPU load per game step, from the C port's frame statistics (if present)
addr=$(arm-none-eabi-nm "${1%.gba}.elf" 2>/dev/null | awk '$3=="g_frame_stats"{print "0x"$1}')
if [ -n "$addr" ]; then
  "$ROOT/build-tools/mgba_run" "$1" --frames 40000 --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
      --input "$TRACE" --game-frame-addr 0x020093D4 --until-game-frame "$N" --guest "$addr" --quiet |
    python3 -c '
import sys
for l in sys.stdin:
    if l.startswith("guest:"):
        steps, lines, missed = (int(x, 16) for x in l.split()[1:4])
        print(f"  CPU load: {lines / steps:.0f} scanlines per game step ({100 * lines / steps / 228:.0f}% of a frame), "
              f"{missed} of {steps} steps overran")'
fi
