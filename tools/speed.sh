#!/usr/bin/env bash
# speed.sh ROM [GAME_FRAMES] — hardware frames needed to reach a game frame,
# with input replayed by game frame (same work for every build). Prints the
# speed relative to the original ROM.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
N=${2:-2500}
TRACE=${TRACE_FILE:-$ROOT/tools/traces/title_mash_a.trace}
# toolkit (r10) = 0x020093B0; [toolkit + oToolkit_CurFramePtr (0x24)] -> u16 counter
run() {
  "$ROOT/build-tools/mgba_run" "$1" --frames 40000 --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
      --input "$TRACE" --game-frame-addr 0x020093D4 --until-game-frame "$N" $(poke_args "$1" "$TRACE") --quiet |
      awk '/reached game frame/ {print $NF}'
}
base=$(run "$ROOT/bn6f/bn6f.gba")
hw=$(run "$1")
echo "$1: game frame $N at hw frame $hw (original: $base) -> $(( 100 * base / hw ))% speed"
# CPU load per game step, from the C port's frame statistics (if present)
addr=$(arm-none-eabi-nm "${1%.gba}.elf" 2>/dev/null | awk '$3=="g_frame_stats"{print "0x"$1}')
# FROM=G0: report the CPU load of game frames G0..N only (e.g. a battle)
stats() {  # game frame -> "steps lines missed"
  "$ROOT/build-tools/mgba_run" "$1" --frames 100000 --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
      --input "$TRACE" --game-frame-addr 0x020093D4 --until-game-frame "$2" --guest "$addr" $(poke_args "$1" "$TRACE") --quiet |
    python3 -c 'import sys
for l in sys.stdin:
    if l.startswith("guest:"): print(*(int(x, 16) for x in l.split()[1:4]))'
}
if [ -n "$addr" ]; then
  read -r s1 l1 m1 < <(stats "$1" "$N")
  s0=0 l0=0 m0=0
  [ -n "${FROM:-}" ] && read -r s0 l0 m0 < <(stats "$1" "$FROM")
  python3 -c "
steps, lines, missed = $s1 - $s0, $l1 - $l0, $m1 - $m0
print(f'  CPU load{\" (game frames ${FROM:-0}..$N)\" if \"${FROM:-}\" else \"\"}: {lines / steps:.0f} scanlines per game step '
      f'({100 * lines / steps / 228:.0f}% of a frame), {missed} of {steps} steps overran')"
fi
