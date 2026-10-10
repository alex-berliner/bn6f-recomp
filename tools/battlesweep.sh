#!/usr/bin/env bash
# battlesweep.sh ROM [GAME_FRAMES] [SETTINGS_ADDR...]
#
# Lockstep a C-port ROM against the original in many different battles:
# battle.trace's input, with its StartBattle poke (battle.poke) pointed at
# other battle-settings records (bn6f data/BattleSettings.s), so other
# enemies, their AI and attacks run. Default records: every 4th of both
# lists (STEP=n to change). JOBS parallel lockstep runs (default 8).
# COVERAGE=1 also profiles the original over each battle and reports how
# many lifted functions the sweep executed (tools/coverage.py).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
ROM=$(realpath "$1")
N=${2:-20000}
shift $(( $# < 2 ? $# : 2 ))
if [ $# -eq 0 ]; then
  set -- $(python3 -c "
step = ${STEP:-4}
print(' '.join([hex(0x080AEE70 + 16 * k) for k in range(0, 269, step)] +
               [hex(0x080B0D88 + 16 * k) for k in range(0, 192, step)]))")
fi
OUT=${OUT:-$(mktemp -d)}
mkdir -p "$OUT"
G0=$(awk '{print $1}' "$TRACES/battle.poke")
for s in "$@"; do
  ln -sf "$TRACES/battle.trace" "$OUT/b_$s.trace"
  sed -E "s/0x2001b9c=0x[0-9A-Fa-f]+/0x2001b9c=$s/" "$TRACES/battle.poke" > "$OUT/b_$s.poke"
done
one() {  # settings address
  local s=$1 r
  r=$(OUT="$OUT/ls_$s" "$ROOT/tools/lockstep.sh" "$ROM" "$OUT/b_$s.trace" "$N" 2>&1 | tail -n 1)
  if [ "${COVERAGE:-0}" = 1 ]; then
    . "$ROOT/tools/common.sh"
    mkdir -p "$OUT/cov_$s"
    (cd "$OUT/cov_$s" && "$MGBA_RUN" "$ORIG_ROM" --quiet --frames 200000 \
        --bios "$GBA_BIOS" --input "$OUT/b_$s.trace" --game-frame-addr "$GAME_FRAME_ADDR" \
        --profile-game "$G0" "$N" $(poke_args "$ORIG_ROM" "$OUT/b_$s.trace") > /dev/null)
  fi
  echo "$s $r"
}
export -f one
export ROOT ROM N OUT G0
printf '%s\n' "$@" | xargs -P "${JOBS:-8}" -I{} bash -c 'one {}' | tee "$OUT/summary.txt"
echo "$(grep -c ' OK:' "$OUT/summary.txt") of $# battles identical; results in $OUT"
if [ "${COVERAGE:-0}" = 1 ]; then
  python3 "$ROOT/tools/coverage.py" "$OUT"/cov_*/profile.txt
fi
grep -q ' FAIL' "$OUT/summary.txt" && exit 1 || exit 0
