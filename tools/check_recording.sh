#!/usr/bin/env bash
# check_recording.sh NAME [ROM]: check a tools/record recording, then test
# a C-port ROM (default cport/build/bn6f_lifted.gba) on it.
#   1. Replay tools/traces/NAME.trace on the original from NAME.sav for the
#      recorded number of frames: the state digest and the save memory must
#      equal what the recorder saw (the trace's "# end" line, NAME.end.sav).
#   2. Lockstep the port against the original over the whole trace
#      (including save memory).
#   3. Report how many lifted functions the recording executes, alone and
#      together with the existing routes.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
NAME=${1:?usage: tools/check_recording.sh NAME [ROM]}
ROM=$(realpath "${2:-$ROOT/cport/build/bn6f_lifted.gba}")
T="$TRACES/$NAME.trace"
ORIG="$ORIG_ROM"
TMP=$(mktemp -d)
end=$(grep '^# end ' "$T" || true)
[ -n "$end" ] || { echo "$T has no '# end' line (recorded before the recorder wrote one?)"; exit 1; }
hw=$(sed -E 's/.*hw_frames=([0-9]+).*/\1/' <<< "$end")
game=$(sed -E 's/.*game_frame=([0-9]+).*/\1/' <<< "$end")
echo "$NAME: $(grep -vc '^#' "$T") input changes, $game game frames ($hw hardware frames)"

"$MGBA_RUN" "$ORIG" --quiet --frames "$hw" --bios "$GBA_BIOS" \
  --input "$T" --game-frame-addr "$GAME_FRAME_ADDR" --final-digest --dump-save "$TMP/end.sav" \
  $(poke_args "$ORIG" "$T") | grep '^final' > "$TMP/final.txt"
want=$(grep -oE 'ewram=[0-9a-f]+ pal=[0-9a-f]+ vram=[0-9a-f]+ oam=[0-9a-f]+' <<< "$end")
got=$(grep -oE 'ewram=[0-9a-f]+ pal=[0-9a-f]+ vram=[0-9a-f]+ oam=[0-9a-f]+' "$TMP/final.txt")
if [ "$want" = "$got" ]; then echo "replay: final state matches the recording"
else echo "FAIL replay: final state differs from the recording"; echo "  recorded: $want"; echo "  replayed: $got"; exit 1; fi
if [ -f "$TRACES/$NAME.end.sav" ]; then
  if cmp -s "$TMP/end.sav" "$TRACES/$NAME.end.sav"; then echo "replay: ending save matches"
  else echo "FAIL replay: ending save differs from $NAME.end.sav"; exit 1; fi
fi

OUT="$TMP/lockstep" "$ROOT/tools/lockstep.sh" "$ROM" "$T" "$game" | tail -n 2

(cd "$TMP" && "$MGBA_RUN" "$ORIG" --quiet --frames "$hw" --bios "$GBA_BIOS" \
  --input "$T" --game-frame-addr "$GAME_FRAME_ADDR" --profile-game 0 "$game" $(poke_args "$ORIG" "$T") > /dev/null)
echo -n "coverage alone: "; python3 "$ROOT/tools/coverage.py" "$TMP/profile.txt"
if ls "$TRACES"/coverage/*.prof > /dev/null 2>&1; then
  echo -n "with the existing routes: "; python3 "$ROOT/tools/coverage.py" "$TMP/profile.txt" "$TRACES"/coverage/*.prof
fi
mkdir -p "$TRACES/coverage"
cp "$TMP/profile.txt" "$TRACES/coverage/$NAME.prof"
