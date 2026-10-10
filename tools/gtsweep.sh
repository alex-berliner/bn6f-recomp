#!/usr/bin/env bash
# Ground-truth sweep: capture the first call of every function run in a
# window of game frames on the original ROM, replay each through the lifted
# code on the host and list the calls whose RAM results differ.
#   tools/gtsweep.sh TRACE G0 G1 [OUTDIR]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
TRACE=$(realpath "$1"); G0=$2; G1=$3
OUT=${4:-$(mktemp -d)}
mkdir -p "$OUT/snaps"
arm-none-eabi-readelf -sW "$ORIG_ELF" | awk '$4=="FUNC" {print $2}' | sort -u > "$OUT/funcs.txt"
"$MGBA_RUN" "$ORIG_ROM" --quiet --frames 400000 --bios "$GBA_BIOS" \
  --input "$TRACE" --game-frame-addr "$GAME_FRAME_ADDR" --until-game-frame $((G0 - 2)) --save-state "$OUT/start.ss" > /dev/null
"$MGBA_RUN" "$ORIG_ROM" --quiet --frames 400000 --bios "$GBA_BIOS" \
  --input "$TRACE" --game-frame-addr "$GAME_FRAME_ADDR" --load-state "$OUT/start.ss" --profile-game "$G0" "$G1" \
  --snap-funcs "$OUT/funcs.txt" "$OUT/snaps" 1 > /dev/null
(cd "$ROOT/cport" && BN6F_ROM="$ORIG_ROM" ./build-host/difftest -G "$OUT/snaps" > "$OUT/result.txt" 2>&1 || true)
echo "$(grep -c ': MEM' "$OUT/result.txt" || true) MEM / $(grep -c ': REG' "$OUT/result.txt" || true) REG / $(grep -c ': ok' "$OUT/result.txt" || true) ok; details: $OUT/result.txt"
grep ': MEM\|crashed' "$OUT/result.txt" | sed "s|$OUT/snaps/||" | head -40 || true
