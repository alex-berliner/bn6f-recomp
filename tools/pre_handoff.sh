#!/usr/bin/env bash
# pre_handoff.sh ROM TRACE G TO
#
# Replays the ORIGINAL up to TO with --dump-mem-at-when G so we have the
# original's bytes at G (this is what --handoff would copy to the port).
# Then runs the PORT cold with --handoff at G, and with --dump-pre-handoff
# at G, so we capture the port's OWN bytes BEFORE the handoff writes
# anything.  Compare the two side-by-side; the regions that differ between
# the port's bus state and the original's bus state at the same game
# counter value are what the handoff overwrites non-no-op-ly, and that is
# what can drive a divergence immediately after the handoff.

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"

[ "$#" -eq 4 ] || { echo "usage: $0 ROM TRACE G TO" >&2; exit 2; }
ROM=$1; TRACE=$2; G=$3; TO=$4

WORK=$(mktemp -d /tmp/pre_handoff.XXXXXX)
HOFF_DIR="$WORK/handoff"
mkdir -p "$HOFF_DIR"

HW=$(( TO * 2 + 200000 ))

echo "=== run #1: orig, dump at G" >&2
"$MGBA_RUN" "$ORIG_ROM" --frames "$HW" \
    --game-frame-addr "$GAME_FRAME_ADDR" --quiet \
    --bios "$GBA_BIOS" --input "$TRACE" \
    --until-game-frame "$(( TO + 1 ))" \
    --dump-mem-at-when "$G" "$WORK/orig_dump" \
    $(poke_args "$ORIG_ROM" "$TRACE") >"$WORK/orig_digest.txt" 2>"$WORK/orig_stderr.txt"

# Stage handoff files from orig's dump at G.
for r in ewram iwram vram pal oam save ior; do
  for f in "$WORK/orig_dump_$G.$r.bin" "$WORK/orig_dump_$G.ior.bin"; do
    [ -f "$f" ] || continue
  done
  [ -f "$WORK/orig_dump_$G.$r.bin" ] && cp "$WORK/orig_dump_$G.$r.bin" "$HOFF_DIR/$r.bin"
done
ls -la "$WORK" | sed 's/^/  /'
echo

echo "=== run #2: port (cold), --handoff DIR G, --dump-pre-handoff" >&2
"$MGBA_RUN" "$ROM" --frames "$HW" \
    --game-frame-addr "$GAME_FRAME_ADDR" --quiet \
    --bios "$GBA_BIOS" --input "$TRACE" \
    --until-game-frame "$(( TO + 1 ))" \
    --handoff "$HOFF_DIR" "$G" \
    --dump-pre-handoff "$WORK/port_pre" \
    --dump-mem-at-when "$G,$((G+1)),$((G+2))" "$WORK/port_post" \
    $(poke_args "$ROM" "$TRACE") >"$WORK/port_digest.txt" 2>"$WORK/port_stderr.txt"

ls -la "$WORK" | sed 's/^/  /'
echo
echo "Orig stderr (handoff-relevant lines):"
grep -E "handoff|dump" "$WORK/orig_stderr.txt" || true
echo
echo "Port stderr (handoff-relevant lines):"
grep -E "handoff|dump-pre|dump-mem" "$WORK/port_stderr.txt" || true
echo
echo "Work dir: $WORK"
