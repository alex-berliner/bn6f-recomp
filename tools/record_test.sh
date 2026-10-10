#!/usr/bin/env bash
# record_test.sh [FRAMES] [START.sav]: check that a tools/record recording
# replays identically in mgba_run (from START.sav if given, as the test
# scripts do with a trace's NAME.sav). The recorder (headless, pseudo-random keys that
# change at arbitrary hardware frames) and mgba_run's replay of the trace
# it wrote must print the same state digests at every checkpoint.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
N=${1:-6000}
SAVE=${2:+$(realpath "$2")}
TMP=$(mktemp -d)
cd "$ROOT"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RECORD_TEST_FRAMES=$N \
  "$RECORD" "$ORIG_ROM" "$TMP/t.trace" --bios "$GBA_BIOS" ${SAVE:+--save "$SAVE"} | grep '^digest' > "$TMP/rec.txt"
G=$(tail -n 1 "$TMP/rec.txt" | sed 's/.*game=\([0-9]*\).*/\1/')
"$MGBA_RUN" "$ORIG_ROM" --quiet --frames $((N * 2)) --bios "$GBA_BIOS" \
  --input "$TMP/t.trace" --game-frame-addr "$GAME_FRAME_ADDR" --until-game-frame $((G + 1)) --digest-every 20 \
  $([ -f "$TMP/t.sav" ] && echo --save "$TMP/t.sav") |
  grep '^digest' > "$TMP/rep.txt"
echo "$(grep -c . "$TMP/t.trace") trace lines, $(wc -l < "$TMP/rec.txt") checkpoints to game frame $G"
if diff <(cut -d' ' -f2,4- "$TMP/rec.txt") <(cut -d' ' -f2,4- "$TMP/rep.txt") > "$TMP/diff.txt"; then
  echo "OK: replay matches the recording at every checkpoint"
else
  echo "FAIL: replay differs from the recording"; head -4 "$TMP/diff.txt"; exit 1
fi
