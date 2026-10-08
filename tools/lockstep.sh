#!/usr/bin/env bash
# lockstep.sh ROM [TRACE] [GAME_FRAMES] [EVERY]
#
# Whole-game regression test. Runs the original bn6f.gba and ROM on the same
# input, replayed by the game's own frame counter (so speed differences do not
# change what the game sees), and compares the visible machine state —
# palette, VRAM and OAM — at every checkpoint. EWRAM is reported too but is
# expected to differ (sound/RNG/decompression progress depend on real time).
# A mismatch is accepted as timing (work the game does in its VBlank
# interrupt, e.g. copying tiles to VRAM, landing one game frame earlier or
# later because the port runs at a different speed) when it is isolated, the
# next checkpoint matches again, and every differing range of the tested
# ROM's memory equals the original's one game frame before or after.
# On any other mismatch, screenshots of both are written next to the digests.
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
export LS_ROOT="$ROOT" LS_ROM="$ROM" LS_TRACE="$TRACE"
export LS_POKE_ORIG="$(poke_args "$ROOT/bn6f/bn6f.gba" "$TRACE")" LS_POKE_TEST="$(poke_args "$ROM" "$TRACE")"
python3 - "$OUT" "$N" "$EVERY" <<'PY' || status=$?
import os, shlex, subprocess, sys
out, want, every = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
o = [l.split() for l in open(f"{out}/orig.txt")]
t = [l.split() for l in open(f"{out}/test.txt")]
n = min(len(o), len(t))
game = lambda i: int(o[i][1].split("=")[1])
print(f"{n} checkpoints to game frame {game(n-1)}; "
      f"hardware frames original {o[n-1][2].split('=')[1]}, tested {t[n-1][2].split('=')[1]}")
if len(o) != len(t):
    print(f"FAIL: tested ROM stopped early ({len(t)} vs {len(o)} checkpoints)")
    sys.exit(1)
bad = [i for i in range(n) if o[i][4:] != t[i][4:]]


def dump(rom, poke, frames, prefix):
    root = os.environ["LS_ROOT"]
    subprocess.run([f"{root}/build-tools/mgba_run", rom, "--frames", "200000", "--bios", f"{root}/gbarecomp/bios/gba_bios.bin",
                    "--input", os.environ["LS_TRACE"], "--game-frame-addr", "0x020093D4",
                    "--until-game-frame", str(max(frames) + 1), "--dump-at-game", ",".join(map(str, frames)), prefix,
                    "--quiet"] + shlex.split(poke), check=True, stdout=subprocess.DEVNULL)


def timing_only(gs):
    """Every differing 32-byte-aligned range of the tested ROM's palette/VRAM/OAM
    at game frame g equals the original's at g - 1 or g + 1."""
    dump(f"{os.environ['LS_ROOT']}/bn6f/bn6f.gba", os.environ["LS_POKE_ORIG"],
         sorted({g + d for g in gs for d in (-1, 0, 1)}), f"{out}/skew_orig")
    dump(os.environ["LS_ROM"], os.environ["LS_POKE_TEST"], gs, f"{out}/skew_test")
    for g in gs:
        a, b = (open(f"{out}/skew_{w}_{g}.bin", "rb").read() for w in ("orig", "test"))
        near = [open(f"{out}/skew_orig_{g + d}.bin", "rb").read() for d in (-1, 1)]
        chunks = [k for k in range(0, len(a), 32) if a[k:k + 32] != b[k:k + 32]]
        ranges = []
        for k in chunks:
            if ranges and ranges[-1][1] == k:
                ranges[-1][1] = k + 32
            else:
                ranges.append([k, k + 32])
        if not all(any(b[s:e] == m[s:e] for m in near) for s, e in ranges):
            return False
    return True


skewed = 0
if bad:
    gs = [game(i) for i in bad]
    # isolated, healed by the next checkpoint, and nothing but timing
    if all(i + 1 < n and i + 1 not in bad for i in bad) and min(gs) > 0 and len(gs) <= 60 and timing_only(gs):
        skewed = len(bad)
    else:
        a, b = o[bad[0]], t[bad[0]]
        print(f"FAIL: visible state differs at {len(bad)} checkpoints, first at {a[1]}")
        print("  original:", " ".join(a[4:]))
        print("  tested:  ", " ".join(b[4:]))
        open(f"{out}/first_bad", "w").write(a[1].split("=")[1])
        sys.exit(1)
last = game(n - 1)
note = ""
if last + every < want:
    # both stopped counting game frames (mgba_run --stall-frames): the original crashed or hung
    note = f" (both games stopped at game frame {last} of {want})"
if skewed:
    note += f" (game frame{'s' if skewed > 1 else ''} {', '.join(map(str, gs))} one frame early or late)"
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
