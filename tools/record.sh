#!/usr/bin/env bash
# record.sh NAME [START.sav]: play the original ROM and record
# tools/traces/NAME.trace (and NAME.sav if starting from a save) for the
# test harness. Opens a window on the desktop (DISPLAY=:0 when unset).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
NAME=${1:?usage: tools/record.sh NAME [START.sav]}
cd "$ROOT"
[ -x "$RECORD" ] || make -C "$ROOT/tools" record
OUT="tools/traces/$NAME.trace"
if [ -e "$OUT" ]; then
  echo "$OUT exists; pick another name" >&2
  exit 1
fi
# From a session without a display (e.g. Claude Code), use the desktop's.
if [ -z "${DISPLAY:-}" ]; then
  x=$(ls /tmp/.X11-unix/ 2>/dev/null | sed -n 's/^X\([0-9]*\)$/\1/p' | head -1)
  export DISPLAY=":${x:-0}"
  [ -f "/run/user/$(id -u)/gdm/Xauthority" ] && export XAUTHORITY="/run/user/$(id -u)/gdm/Xauthority"
fi
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}   # sound (PulseAudio/PipeWire)
"$RECORD" "$ORIG_ROM" "$OUT" --bios "$GBA_BIOS" ${2:+--save "$(realpath "$2")"}
echo "Recorded $OUT. Tell Claude: \"recorded $NAME\"."
