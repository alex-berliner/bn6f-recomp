#!/usr/bin/env bash
# lockstep.sh ROM [TRACE] [GAME_FRAMES] [EVERY]
#
# Whole-game regression test. Runs the original bn6f.gba and ROM on the same
# input, replayed by the game's own frame counter (so speed differences do not
# change what the game sees), and compares the visible machine state —
# palette, VRAM and OAM — at every checkpoint. EWRAM is reported too but is
# expected to differ (sound/RNG/decompression progress depend on real time).
# On a mismatch, screenshots of both are written next to the digests.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
ROM=$1
TRACE=${2:-$ROOT/tools/traces/wander.trace}
N=${3:-8000}
EVERY=${4:-20}
OUT=${OUT:-$(mktemp -d)}
mkdir -p "$OUT"
run() {
  "$ROOT/build-tools/mgba_run" "$1" --frames 200000 --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
      --input "$TRACE" --game-frame-addr 0x020093D4 --until-game-frame "$N" \
      --digest-every "$EVERY" --dump-save "$2.sav" $(poke_args "$1" "$TRACE") --quiet | grep digest > "$2"
}
run "$ROOT/bn6f/bn6f.gba" "$OUT/orig.txt" &
run "$ROM" "$OUT/test.txt"
wait
status=0
python3 - "$OUT" "$N" "$EVERY" <<'PY' || status=$?
import sys
out, want, every = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
o = [l.split() for l in open(f"{out}/orig.txt")]
t = [l.split() for l in open(f"{out}/test.txt")]
n = min(len(o), len(t))
bad = [(a, b) for a, b in zip(o, t) if a[4:] != b[4:]]
print(f"{n} checkpoints to game frame {o[n-1][1].split('=')[1]}; "
      f"hardware frames original {o[n-1][2].split('=')[1]}, tested {t[n-1][2].split('=')[1]}")
if len(o) != len(t):
    print(f"FAIL: tested ROM stopped early ({len(t)} vs {len(o)} checkpoints)")
    sys.exit(1)
if bad:
    a, b = bad[0]
    print(f"FAIL: visible state differs at {len(bad)} checkpoints, first at {a[1]}")
    print("  original:", " ".join(a[4:]))
    print("  tested:  ", " ".join(b[4:]))
    open(f"{out}/first_bad", "w").write(a[1].split("=")[1])
    sys.exit(1)
last = int(o[n-1][1].split("=")[1])
note = ""
if last + every < want:
    # both stopped counting game frames (mgba_run --stall-frames): the original crashed or hung
    note = f" (both games stopped at game frame {last} of {want})"
print("OK: palette, VRAM and OAM identical at every checkpoint" + note)
PY
# Save memory where both stopped (the game's own saves must match too).
if [ "$status" = 0 ] && ! cmp -s "$OUT/orig.txt.sav" "$OUT/test.txt.sav"; then
  echo "FAIL: save memory differs at the end"
  status=1
fi
if [ -f "$OUT/first_bad" ]; then
  g=$(cat "$OUT/first_bad")
  for r in orig:$ROOT/bn6f/bn6f.gba test:$ROM; do
    mkdir -p "$OUT/${r%%:*}"
    "$ROOT/build-tools/mgba_run" "${r#*:}" --frames 200000 --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
        --input "$TRACE" --game-frame-addr 0x020093D4 --until-game-frame "$((g + 1))" \
        --shot-at-game-frame "$g" --shots "$OUT/${r%%:*}" $(poke_args "${r#*:}" "$TRACE") --quiet > /dev/null
  done
  echo "screenshots: $OUT/orig $OUT/test"
fi
exit $status
