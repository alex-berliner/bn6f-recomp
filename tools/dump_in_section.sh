#!/usr/bin/env bash
# dump_in_section.sh ROM TRACE G TO FRAMES...
#   G: game frame of handoff
#   TO: until-game-frame for the replay
#   FRAMES: list of additional frames whose state to dump on the same replay.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"

[ "$#" -ge 4 ] || { echo "usage: $0 ROM TRACE G TO FRAME..." >&2; exit 2; }
ROM=$1; TRACE=$2; G=$3; TO=$4; shift 4
FRAMES=("$@")

WORK=$(mktemp -d /tmp/dinsec.XXXXXX)
HOFF_DIR="$WORK/handoff"
mkdir -p "$HOFF_DIR"

# Compute N (hardware-frame budget): allow generous margin so total slowdown
# doesn't trip the --until-game-frame stall timer.
HW=$(( TO * 2 + 200000 ))

# Run 1: Orig.  Dump memory at G (for the port's handoff) AND at every extra
# frame we want to compare at.
ORIG_PREFIX="$WORK/orig"
echo "run #1: orig (harness up to game=$TO; dumps at $G, ${FRAMES[*]})" >&2
"$MGBA_RUN" "$ORIG_ROM" --frames "$HW" \
    --game-frame-addr "$GAME_FRAME_ADDR" --quiet \
    --bios "$GBA_BIOS" --input "$TRACE" \
    --until-game-frame "$(( TO + 1 ))" \
    --digest-every 200 --dump-save "$WORK/orig.sav" \
    --dump-mem-at-when "$(IFS=,; echo "${FRAMES[*]}"),$G" "$ORIG_PREFIX" \
    $(poke_args "$ORIG_ROM" "$TRACE") >"$WORK/orig.digest"

# Stage the handoff files for the port: --handoff reads DIR/<region>.bin.
for r in ewram iwram vram pal oam save ior; do
  [ -f "$ORIG_PREFIX"_"$G"."$r".bin ] && cp "$ORIG_PREFIX"_"$G"."$r".bin "$HOFF_DIR/$r.bin"
done

# Run 2: Port (cold).  --handoff at G from $HOFF_DIR, then dumps at the same FRAMES.
PORT_PREFIX="$WORK/port"
echo "run #2: port (cold, handoff at $G; dumps at ${FRAMES[*]})" >&2
"$MGBA_RUN" "$ROM" --frames "$HW" \
    --game-frame-addr "$GAME_FRAME_ADDR" --quiet \
    --bios "$GBA_BIOS" --input "$TRACE" \
    --until-game-frame "$(( TO + 1 ))" \
    --digest-every 200 --dump-save "$WORK/port.sav" \
    --dump-mem-at-when "$(IFS=,; echo "${FRAMES[*]}")" "$PORT_PREFIX" \
    --handoff "$HOFF_DIR" "$G" \
    $(poke_args "$ROM" "$TRACE") >"$WORK/port.digest"

echo "Work dir: $WORK"
echo "Files:"
ls -la "$WORK/"
