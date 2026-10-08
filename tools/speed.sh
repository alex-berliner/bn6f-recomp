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
