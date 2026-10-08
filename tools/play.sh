#!/usr/bin/env bash
# Build input traces step by step on the original ROM.
#   tools/play.sh STATE_IN TRACE GAME_FRAME STATE_OUT PNG
# Plays TRACE (game-frame keyed) from STATE_IN (or from boot if "-") until
# GAME_FRAME, saves STATE_OUT and a 2x screenshot of the last frame.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IN=$1 TRACE=$2 G=$3 OUT=$4 PNG=$5
TMP=$(mktemp -d)
ARGS=()
[ "$IN" != "-" ] && ARGS+=(--load-state "$IN")
"$ROOT/build-tools/mgba_run" "$ROOT/bn6f/bn6f.gba" --quiet --frames 200000 --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
  --input "$TRACE" --game-frame-addr 0x020093D4 --until-game-frame "$G" \
  --shots "$TMP" --shot-at-game-frame $((G - 1)) --save-state "$OUT" "${ARGS[@]}" > /dev/null
python3 -c "
from PIL import Image; import glob
Image.open(glob.glob('$TMP/*.ppm')[0]).resize((480, 320), Image.NEAREST).save('$PNG')"
rm -rf "$TMP"
