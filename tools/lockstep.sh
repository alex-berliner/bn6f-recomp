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
# ROM's memory equals the original's one game frame before or after (or the
# tested ROM equals the original one game frame before and after: it was in
# the middle of rewriting VRAM when the frame started).
# On any other mismatch, screenshots of both are written next to the digests.
#
# The original game's side of a replay depends only on its inputs, so its digests
# and end-of-run save memory are cached (build-wf-cache/lockstep next to the bn6f
# the original ROM comes from), keyed by the hashes of the original ROM, mgba_run,
# the BIOS, the trace and its .sav and .poke, and the frame counts: a replay then
# runs only the tested ROM. LOCKSTEP_CACHE=off replays the original every time;
# LOCKSTEP_CACHE=DIR keeps the cache there.
#
#   lockstep.sh --cached TRACE [GAME_FRAMES] [EVERY]   exit 0 if the original's side is cached
#   lockstep.sh --from G [--to G2] ROM [TRACE] [GAME_FRAMES] [EVERY]
#     Section mode: the port is started cold and resumed at game frame --from,
#     using the original's checkpointed state as the resume point.  The
#     original is replayed once to extract that state; thereafter the same
#     cache file lets the port be replayed in pieces up to --to (default
#     end of trace) without retiming the original.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
ORIG="$ORIG_ROM"
MGBA="$MGBA_RUN"
BIOS="$GBA_BIOS"
QUERY=0
SECTION_FROM=        # game frame to handoff at; --from=<game-frame> / --from N
SECTION_TO=          # game frame to stop at; --to N (defaults to N below)
if [ "${1:-}" = --cached ]; then QUERY=1; shift; set -- "$ORIG" "$@"; fi
while [ "${1:-}" = --from ] || [ "${1:-}" = --to ]; do
  case "$1" in
    --from) SECTION_FROM=$2; shift 2 ;;
    --to)   SECTION_TO=$2;   shift 2 ;;
  esac
done
ROM=$1
TRACE=${2:-$TRACES/wander.trace}
N=${3:-8000}
EVERY=${4:-20}
HW=$(( N * 2 > 200000 ? N * 2 : 200000 ))   # hardware frames a replay may take (long routes need more)
RUN_ARGS="--frames $HW --game-frame-addr $GAME_FRAME_ADDR --quiet"   # part of the cache key: change both together
case "${LOCKSTEP_CACHE:-}" in
  off) CACHE= ;;
  "") CACHE="$(cd -P "$BN6F_DIR/.." && pwd)/build-wf-cache/lockstep" ;;
  *) CACHE=$LOCKSTEP_CACHE ;;
esac
orig_entry() {   # the cache directory for this replay of the original
  local key f
  key=$({ for f in "$ORIG" "$MGBA" "$BIOS" "$TRACE" "${TRACE%.trace}.sav" "${TRACE%.trace}.poke"; do
            if [ -f "$f" ]; then sha1sum < "$f"; else echo none; fi
          done
          echo "$RUN_ARGS $N $EVERY"; } | sha1sum | cut -c1-16)
  echo "$CACHE/$(basename "$TRACE" .trace)-$key"
}
if [ "$QUERY" = 1 ]; then
  [ -n "$CACHE" ] && [ -f "$(orig_entry)/orig.txt" ]
  exit
fi
OUT=${OUT:-$(mktemp -d)}
mkdir -p "$OUT"
run() {
  "$MGBA" "$1" $RUN_ARGS --bios "$BIOS" --input "$TRACE" --until-game-frame "$N" \
      --digest-every "$EVERY" --dump-save "$2.sav" $(poke_args "$1" "$TRACE") | grep digest > "$2"
}
if [ -n "$SECTION_FROM" ]; then
  # Section mode: replay the original up to a section end and dump its state
  # at --from; replay the port, cold, with --handoff from --from --from up
  # to --to.  Only the comparators in [from, to] are checked (digests before
  # --from on the port match the original because the port's run is short,
  # so they're not informative, and the original run's full [0,to] tail is
  # what we compare against). The original can be cached by adding the
  # section to the cache key.
  SEC_TO=${SECTION_TO:-$N}
  SEC_KEY=$({ for f in "$ORIG" "$MGBA" "$BIOS" "$TRACE" "${TRACE%.trace}.sav" "${TRACE%.trace}.poke"; do
               if [ -f "$f" ]; then sha1sum < "$f"; else echo none; fi
             done
             echo "$RUN_ARGS $N $EVERY $SECTION_FROM $SEC_TO" ; } | sha1sum | cut -c1-16)
  SHA=section-$(basename "$TRACE" .trace)-${SECTION_FROM}-${SEC_TO}-${SEC_KEY}
  ENTRY=$([ -n "$CACHE" ] && echo "$CACHE/$SHA" || true)
  mkdir -p "$OUT/land"
  if [ -n "$ENTRY" ] && [ -f "$ENTRY/orig.txt" ] && [ -f "$ENTRY/orig_${SECTION_FROM}_${SECTION_FROM}.ewram.bin" ]; then
    echo "original: cached section ($ENTRY)"
    cp "$ENTRY/orig.txt" "$OUT/orig.txt"
    # handoff files: mgba_run writes "<prefix>_<frame>.<region>.bin" so the
    # names are "orig_<from>_<from>.ewram.bin" etc.
    for r in ewram iwram vram pal oam save ior; do
      if [ -f "$ENTRY/orig_${SECTION_FROM}_${SECTION_FROM}.$r.bin" ]; then
        cp "$ENTRY/orig_${SECTION_FROM}_${SECTION_FROM}.$r.bin" "$OUT/"
      fi
    done
    [ -f "$ENTRY/orig.txt.sav" ] && cp "$ENTRY/orig.txt.sav" "$OUT/orig.txt.sav"
  else
    echo "original: 0..$SEC_TO replay with state dump at $SECTION_FROM"
    # --until-game-frame SEC_TO+1 so the digest at game=sec_to is included.
    "$MGBA" "$ORIG" $RUN_ARGS --bios "$BIOS" --input "$TRACE" --until-game-frame "$((SEC_TO + 1))" \
        --digest-every "$EVERY" --dump-save "$OUT/orig.txt.sav" \
        --dump-mem-at-when "$SECTION_FROM" "$OUT/orig_$SECTION_FROM" \
        $(poke_args "$ORIG" "$TRACE") | grep digest > "$OUT/orig.txt"
    if [ -n "$ENTRY" ] && [ -s "$OUT/orig.txt" ] && [ -f "$OUT/orig.txt.sav" ]; then
      mkdir -p "$CACHE" "$ENTRY.part.$$"
      cp "$OUT/orig.txt" "$OUT/orig.txt.sav" "$ENTRY.part.$$/"
      for r in ewram iwram vram pal oam save ior; do
        [ -f "$OUT/orig_${SECTION_FROM}_${SECTION_FROM}.$r.bin" ] && \
          cp "$OUT/orig_${SECTION_FROM}_${SECTION_FROM}.$r.bin" "$ENTRY.part.$$/"
      done
      mv -T "$ENTRY.part.$$" "$ENTRY" 2>/dev/null || rm -rf "$ENTRY.part.$$"
    fi
  fi
  echo "port: cold boot, --handoff at $SECTION_FROM (orig state), replay to $SEC_TO"
  "$MGBA" "$ROM" --frames $HW --game-frame-addr $GAME_FRAME_ADDR --quiet --bios "$BIOS" --input "$TRACE" --until-game-frame "$((SEC_TO + 1))" --digest-every "$EVERY" --dump-save "$OUT/port.txt.sav" --handoff "$OUT" "$SECTION_FROM" $(poke_args "$ROM" "$TRACE") | grep digest > "$OUT/port.txt"
  # Filter/comparison: only the [SECTION_FROM, SEC_TO] window.
  STATUS=0
  export LS_HW="$HW" LS_ORIG="$ORIG" LS_MGBA="$MGBA" LS_BIOS="$BIOS" LS_FRAME_ADDR="$GAME_FRAME_ADDR" LS_ROM="$ROM" LS_TRACE="$TRACE"
  printf '%s\n' "$(poke_args "$ORIG" "$TRACE")" >"$OUT/poke_orig.args"
  printf '%s\n' "$(poke_args "$ROM"  "$TRACE")" >"$OUT/poke_test.args"
  export LS_POKE_ORIG="$OUT/poke_orig.args" LS_POKE_TEST="$OUT/poke_test.args"
  python3 - "$OUT" "$EVERY" "$SECTION_FROM" "$SEC_TO" <<'PY' || STATUS=$?
import os, sys
out, every, sec_from, sec_to = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
def load(p):
    rows = []
    for l in open(p):
        a = l.split()
        if not a or a[0] != 'digest': continue
        rows.append((int(a[1].split('=')[1]), a[3:]))
    return rows
o, t = load(f"{out}/orig.txt"), load(f"{out}/port.txt")
o2 = [r for r in o if sec_from <= r[0] <= sec_to]
t2 = [r for r in t if sec_from <= r[0] <= sec_to]
print(f"section [{sec_from},{sec_to}]: orig {len(o2)} digests, port {len(t2)} digests")
def index(rows): return {g: tuple(d) for g, d in rows}
oi, ti = index(o2), index(t2)
# Same as non-section lockstep: visible state is pal/vram/oam (ewram is
# reporting-only; m4a mix buffers, real-time RNG, decompression progress).
def vis(row):
    return tuple(v for v in row if v.split('=', 1)[0] in ('pal', 'vram', 'oam'))
common = sorted(set(oi) & set(ti))
expected = set(range(sec_from, sec_to + 1, every))
miss_o = sorted(g for g in expected if g not in oi)
miss_t = sorted(g for g in expected if g not in ti)
if miss_o:
    print(f"note: orig missing digests at {miss_o[:10]}{'...' if len(miss_o) > 10 else ''}"
          " (likely the original crashed before reaching this section end)")
if miss_t:
    print(f"note: port missing digests at {miss_t[:10]}{'...' if len(miss_t) > 10 else ''}"
          " (likely the port had already left its handoff window)")
bad = [g for g in common if vis(oi[g]) != vis(ti[g])]
print(f"section result: {len(common)} checkpoints (pal/vram/oam only), {len(bad)} mismatches")
if bad:
    print(f"first visible-state mismatch at game={bad[0]}:")
    print(f"  orig: {vis(oi[bad[0]])}")
    print(f"  port: {vis(ti[bad[0]])}")
    sys.exit(1)
PY
  if [ "$STATUS" = 0 ] && ! cmp -s "$OUT/orig.txt.sav" "$OUT/port.txt.sav"; then
    echo "FAIL: save memory differs at the end of section"
    STATUS=1
  fi
  exit $STATUS
fi
ENTRY=$([ -n "$CACHE" ] && orig_entry || true)
if [ -n "$ENTRY" ] && [ -f "$ENTRY/orig.txt" ] && [ -f "$ENTRY/orig.txt.sav" ]; then
  cp "$ENTRY/orig.txt" "$ENTRY/orig.txt.sav" "$OUT/"
  echo "original: cached replay ($ENTRY)"
  run "$ROM" "$OUT/test.txt"
else
  run "$ORIG" "$OUT/orig.txt" &
  orig_pid=$!
  run "$ROM" "$OUT/test.txt"
  # only a complete replay of the original is kept
  if wait "$orig_pid" && [ -n "$ENTRY" ] && [ -s "$OUT/orig.txt" ] && [ -f "$OUT/orig.txt.sav" ]; then
    mkdir -p "$CACHE" "$ENTRY.part.$$"
    cp "$OUT/orig.txt" "$OUT/orig.txt.sav" "$ENTRY.part.$$/"
    mv -T "$ENTRY.part.$$" "$ENTRY" 2>/dev/null || rm -rf "$ENTRY.part.$$"   # another run stored it first
  fi
fi
status=0
export LS_HW="$HW" LS_ORIG="$ORIG" LS_MGBA="$MGBA" LS_BIOS="$BIOS" LS_FRAME_ADDR="$GAME_FRAME_ADDR" LS_ROM="$ROM" LS_TRACE="$TRACE"
# the --poke-at arguments go through files: a long route's (beastsp: ~1,000 writes) are over the 128 KB
# limit on one environment variable
# (inside $(...), as above: poke_args's nm | awk ends nm with SIGPIPE, which set -e would take for a failure)
printf '%s\n' "$(poke_args "$ORIG" "$TRACE")" > "$OUT/poke_orig.args"
printf '%s\n' "$(poke_args "$ROM" "$TRACE")" > "$OUT/poke_test.args"
export LS_POKE_ORIG="$OUT/poke_orig.args" LS_POKE_TEST="$OUT/poke_test.args"
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
    subprocess.run([os.environ["LS_MGBA"], rom, "--frames", os.environ["LS_HW"], "--bios", os.environ["LS_BIOS"],
                    "--input", os.environ["LS_TRACE"], "--game-frame-addr", os.environ["LS_FRAME_ADDR"],
                    "--until-game-frame", str(max(frames) + 1), "--dump-at-game", ",".join(map(str, frames)), prefix,
                    "--quiet"] + shlex.split(open(poke).read()), check=True, stdout=subprocess.DEVNULL)


def timing_only(gs):
    """Every differing 32-byte-aligned range of the tested ROM's palette/VRAM/OAM
    at game frame g equals the original's at g - 1 or g + 1; or the tested ROM
    equals the original at g - 1 and at g + 1, all of it (at g it was caught in
    the middle of rewriting VRAM when the frame started)."""
    around = sorted({g + d for g in gs for d in (-1, 0, 1)})
    dump(os.environ["LS_ORIG"], os.environ["LS_POKE_ORIG"], around, f"{out}/skew_orig")
    dump(os.environ["LS_ROM"], os.environ["LS_POKE_TEST"], around, f"{out}/skew_test")
    for g in gs:
        a, b = (open(f"{out}/skew_{w}_{g}.bin", "rb").read() for w in ("orig", "test"))
        near = [open(f"{out}/skew_orig_{g + d}.bin", "rb").read() for d in (-1, 1)]
        if all(open(f"{out}/skew_orig_{g + d}.bin", "rb").read() == open(f"{out}/skew_test_{g + d}.bin", "rb").read()
               for d in (-1, 1)):
            continue
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
    note += f" (game frame{'s' if skewed > 1 else ''} {', '.join(map(str, gs))} one frame early or late, or caught mid-update)"
print("OK: palette, VRAM and OAM identical at every checkpoint" + note)
PY
# Save memory where both stopped (the game's own saves must match too).
if [ "$status" = 0 ] && ! cmp -s "$OUT/orig.txt.sav" "$OUT/test.txt.sav"; then
  echo "FAIL: save memory differs at the end"
  status=1
fi
if [ -f "$OUT/first_bad" ]; then
  g=$(cat "$OUT/first_bad")
  for r in orig:$ORIG test:$ROM; do
    mkdir -p "$OUT/${r%%:*}"
    "$MGBA" "${r#*:}" --frames "$HW" --bios "$BIOS" \
        --input "$TRACE" --game-frame-addr "$GAME_FRAME_ADDR" --until-game-frame "$((g + 1))" \
        --shot-at-game-frame "$g" --shots "$OUT/${r%%:*}" $(poke_args "${r#*:}" "$TRACE") --quiet > /dev/null
  done
  echo "screenshots: $OUT/orig $OUT/test"
fi
exit $status
