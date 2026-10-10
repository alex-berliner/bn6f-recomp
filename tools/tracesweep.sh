#!/usr/bin/env bash
# tracesweep.sh ROM DIR GAME_FRAMES PROFILE_FROM COVNAME
#
# Lockstep a C-port ROM against the original over every DIR/*.trace (with
# its .poke/.sav sidecars) up to GAME_FRAMES, in parallel (JOBS, default 8),
# and profile the original from game frame PROFILE_FROM for coverage. The
# union of the profiles goes to tools/traces/coverage/COVNAME.prof.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
ROM=$(realpath "$1"); DIR=$(realpath "$2"); N=$3; G0=$4; COV=$5
one() {
  local t=$1 r
  . "$ROOT/tools/common.sh"
  r=$(OUT="$DIR/ls_$t" "$ROOT/tools/lockstep.sh" "$ROM" "$DIR/$t.trace" "$N" 2>&1 | tail -n 1)
  mkdir -p "$DIR/cov_$t"
  (cd "$DIR/cov_$t" && "$MGBA_RUN" "$ORIG_ROM" --quiet --frames $((N * 3)) \
      --bios "$GBA_BIOS" --input "$DIR/$t.trace" --game-frame-addr "$GAME_FRAME_ADDR" \
      --profile-game "$G0" "$N" $(poke_args "$ORIG_ROM" "$DIR/$t.trace") > /dev/null)
  echo "$t $r"
}
export -f one
export ROOT ROM N DIR G0
ls "$DIR"/*.trace | xargs -n1 basename | sed 's/\.trace$//' | xargs -P "${JOBS:-8}" -I{} bash -c 'one {}' | tee "$DIR/summary.txt"
echo "$(grep -c ' OK:' "$DIR/summary.txt") of $(ls "$DIR"/*.trace | wc -l) runs identical; results in $DIR"
python3 - "$DIR" "$TRACES/coverage/$COV.prof" <<'PY'
import glob, sys
pcs = set()
for p in glob.glob(sys.argv[1] + "/cov_*/profile.txt"):
    pcs.update(l.split()[0] for l in open(p) if l[0] != "#")
try:
    pcs |= {l.split()[0] for l in open(sys.argv[2]) if l[0] != "#"}
except FileNotFoundError:
    pass
with open(sys.argv[2], "w") as f:
    f.write("# union of tools/tracesweep.sh runs, PCs only\n")
    for pc in sorted(pcs):
        f.write(pc + " 1\n")
PY
python3 "$ROOT/tools/coverage.py" "$TRACES"/coverage/*.prof
grep -q ' FAIL' "$DIR/summary.txt" && exit 1 || exit 0
