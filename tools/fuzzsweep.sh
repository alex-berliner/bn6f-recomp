#!/usr/bin/env bash
# fuzzsweep.sh ROM [GAME_FRAMES] [SEEDS] [START.sav...]
#
# Random play (tools/fuzzplay.py) from recorded sessions' ending saves:
# for every save and seed, lockstep a C-port ROM against the original over
# the random trace and profile the original for coverage. Default saves:
# every tools/traces/*.end.sav; SEEDS (default 4) runs per save; JOBS
# parallel (default 8). The union of the profiles is added to the coverage
# cache as tools/traces/coverage/fuzz.prof.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROM=$(realpath "$1"); N=${2:-12000}; SEEDS=${3:-4}
shift $(( $# < 3 ? $# : 3 ))
[ $# -gt 0 ] || set -- "$ROOT"/tools/traces/*.end.sav
OUT=${OUT:-$(mktemp -d)}
mkdir -p "$OUT"
jobs_list=()
for sav in "$@"; do
  base=$(basename "$sav" .end.sav)
  for s in $(seq 1 "$SEEDS"); do
    python3 "$ROOT/tools/fuzzplay.py" "$OUT/${base}_$s.trace" "$sav" "$s" "$N"
    jobs_list+=("${base}_$s")
  done
done
one() {
  local t=$1 r
  . "$ROOT/tools/common.sh"
  r=$(OUT="$OUT/ls_$t" "$ROOT/tools/lockstep.sh" "$ROM" "$OUT/$t.trace" "$N" 2>&1 | tail -n 1)
  mkdir -p "$OUT/cov_$t"
  (cd "$OUT/cov_$t" && "$ROOT/build-tools/mgba_run" "$ROOT/bn6f/bn6f.gba" --quiet --frames $((N * 3)) \
      --bios "$ROOT/gbarecomp/bios/gba_bios.bin" --input "$OUT/$t.trace" --game-frame-addr 0x020093D4 \
      --profile-game 0 "$N" $(poke_args "$ROOT/bn6f/bn6f.gba" "$OUT/$t.trace") > /dev/null)
  echo "$t $r"
}
export -f one
export ROOT ROM N OUT
printf '%s\n' "${jobs_list[@]}" | xargs -P "${JOBS:-8}" -I{} bash -c 'one {}' | tee "$OUT/summary.txt"
echo "$(grep -c ' OK:' "$OUT/summary.txt") of ${#jobs_list[@]} random runs identical; results in $OUT"
python3 - "$OUT" "$ROOT/tools/traces/coverage/fuzz.prof" <<'PY'
import glob, sys
pcs = set()
for p in glob.glob(sys.argv[1] + "/cov_*/profile.txt"):
    pcs.update(l.split()[0] for l in open(p) if l[0] != "#")
old = set()
try:
    old = {l.split()[0] for l in open(sys.argv[2]) if l[0] != "#"}
except FileNotFoundError:
    pass
with open(sys.argv[2], "w") as f:
    f.write("# union of random-play runs (tools/fuzzsweep.sh), PCs only\n")
    for pc in sorted(pcs | old):
        f.write(pc + " 1\n")
PY
python3 "$ROOT/tools/coverage.py" "$ROOT"/tools/traces/coverage/*.prof
grep -q ' FAIL' "$OUT/summary.txt" && exit 1 || exit 0
