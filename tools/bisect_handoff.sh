#!/usr/bin/env bash
# bisect_handoff.sh — given a ROM, trace, source frame G, and bisect bounds,
# replay the original up to G + a list of intermediate frames with state dumps,
# replay the port cold with --handoff at G, dump the port at the same frames,
# and report the FIRST intermediate frame whose state (EWRAM + IWRAM-game, not
# the sound mix buffers / port runtime) differs from the original's.
#
# Usage:  bisect_handoff.sh ROM TRACE G FRAME [FRAME FRAME ...] [N]
#   -- the 4th arg onwards is the list of game frames at which to dump/compare.
#   -- N (the last arg, optional) is the until-game-frame for both replays.
#   -- output: per-frame "first differing address" report, plus bn6f symbol.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"

if [ "$#" -lt 4 ]; then
  echo "usage: $0 ROM TRACE G FRAME [FRAME ...] [N]" >&2
  exit 2
fi
ROM=$1
TRACE=$2
G=$3
shift 3
N_LAST=52712
FRAMES=()
while [ "$#" -gt 0 ]; do
  if [[ "$1" =~ ^[0-9]+$ ]]; then FRAMES+=("$1"); else N_LAST="$1"; fi
  shift
done

CACHE_DIR=$(mktemp -d /tmp/bisect.XXXXXX)
echo "bisect: G=$G cache=$CACHE_DIR frames=${FRAMES[*]} N=$N_LAST" >&2
HOFF_DIR="$CACHE_DIR/handoff"
mkdir -p "$HOFF_DIR"

# Original replay: dump state at G (handoff) and at every listed FRAME, plus
# digest per frame for sanity.  A single replay covers the whole range.
ORIG_PREFIX="$CACHE_DIR/orig"
"$MGBA_RUN" "$ORIG_ROM" --frames $(( N_LAST * 2 + 100000 )) \
    --game-frame-addr $GAME_FRAME_ADDR --quiet \
    --bios "$GBA_BIOS" --input "$TRACE" \
    --until-game-frame "$(( N_LAST + 1 ))" \
    --digest-every $EVERY_DEFAULT \
    --dump-save "$CACHE_DIR/orig.txt.sav" \
    --dump-mem-at-when "$(IFS=,; echo "${FRAMES[*]}"),$G" "$ORIG_PREFIX" \
    $(poke_args "$ORIG_ROM" "$TRACE") >/dev/null
# Stage the handoff dump for the port (mgba_run's --handoff looks for
# <prefix>_<g>.{ewram,iwram,...}.bin in DIR/, or simply DIR/<region>.bin).
for r in ewram iwram vram pal oam save ior; do
  cp "$ORIG_PREFIX"_"$G"."$r".bin "$HOFF_DIR/" 2>/dev/null || true
  cp "$ORIG_PREFIX"_"$G"."$r".bin "$HOFF_DIR/$r.bin" 2>/dev/null || true
done
echo "bisect: orig + handoff done." >&2

# Port replay (cold) with --handoff at G, then dumps at each FRAME.
PORT_PREFIX="$CACHE_DIR/port"
"$MGBA_RUN" "$ROM" --frames $(( N_LAST * 2 + 100000 )) \
    --game-frame-addr $GAME_FRAME_ADDR --quiet \
    --bios "$GBA_BIOS" --input "$TRACE" \
    --until-game-frame "$(( N_LAST + 1 ))" \
    --digest-every $EVERY_DEFAULT \
    --dump-save "$CACHE_DIR/port.txt.sav" \
    --dump-mem-at-when "$(IFS=,; echo "${FRAMES[*]}")" "$PORT_PREFIX" \
    --handoff "$HOFF_DIR" "$G" \
    $(poke_args "$ROM" "$TRACE") >/dev/null
echo "bisect: port done." >&2

# Diff each frame's regions: skip sound-mix ranges and the port runtime as
# the lockstep comparison does.
python3 - "$CACHE_DIR" "${FRAMES[@]}" <<'PY'
import sys, os
from collections import defaultdict
cache = sys.argv[1]
frames = list(map(int, sys.argv[2:]))
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from lifter import elfinfo

REGIONS = [
    ("ewram", 0x02000000, 0x40000, lambda a: "EWRAM" if not (0x02007E00 <= a < 0x02033600) else "EWRAM-sound"),
    ("iwram", 0x03000000, 0x8000, lambda a: classify_iwram(a, sound_only=True)),
    ("vram",  0x06000000, 0x18000, lambda v: "VRAM"),
    ("pal",   0x05000000, 0x400, lambda v: "PAL"),
    ("oam",   0x07000000, 0x400, lambda v: "OAM"),
    ("save",  0x0E000000, 0x8000, lambda v: "SAVE"),
]
def classify_iwram(a, sound_only=False):
    if 0x030045C0 <= a < 0x03005700: return "IWRAM-game sound"
    if a < 0x03005700: return "IWRAM-game"
    if a < 0x030079D0: return "IWRAM-port"
    if a < 0x03007E00: return "IWRAM-stacks"
    if a < 0x03007FFC: return "IWRAM-stack"
    return "IWRAM-irqvec"

info = elfinfo.load(os.path.join(ROOT, "bn6f"))
SYMS = []
for n, v in info.equates.items(): SYMS.append((v, n))
for n, v in info.labels.items(): SYMS.append((v, n))
SYMS.sort()

def name_at(addr):
    pick = None
    for v, n in SYMS:
        if v <= addr: pick = (v, n)
        else: break
    if pick is None: return "(no symbol)"
    off = addr - pick[0]
    return f"{pick[1]}+0x{off:x}" if off else pick[1]

found = False
for fr in frames:
    diffs_by_class = defaultdict(list)
    for (suf, base, size, classify) in REGIONS:
        op = f"{cache}/orig_{fr}.{suf}.bin"
        pp = f"{cache}/port_{fr}.{suf}.bin"
        if not (os.path.exists(op) and os.path.exists(pp)):
            continue
        a = open(op, "rb").read(); b = open(pp, "rb").read()
        if len(a) != len(b):
            print(f"  @game={fr} {suf}: SIZE DIFF orig={len(a)} port={len(b)}")
            continue
        for i, (x, y) in enumerate(zip(a, b)):
            if x != y:
                addr = base + i
                cls = classify(addr)
                diffs_by_class[cls].append((addr, x, y))
    nd = sum(len(v) for v in diffs_by_class.values())
    print(f"@game={fr}: {nd} game-state byte diffs (excluding sound/port-runtime)")
    if nd == 0:
        if found: continue
        # All previous frames also had zero; report nothing.
        continue
    if not found:
        print(f"  *** FIRST diff at game={fr}:")
        for k in ("EWRAM", "IWRAM-game", "VRAM", "PAL", "OAM", "SAVE"):
            if not diffs_by_class.get(k): continue
            v = diffs_by_class[k]
            print(f"  class={k}  count={len(v)}  first 8:")
            for (addr, x, y) in v[:8]:
                print(f"    0x{addr:08x} {name_at(addr)}: orig=0x{x:02x} port=0x{y:02x}")
            if len(v) > 8: print(f"    ... {len(v) - 8} more")
        found = True
    nd_snd = sum(len(v) for k, v in diffs_by_class.items() if k.endswith("sound"))
    print(f"  sound-class totals: {', '.join(f'{k}={len(v)}' for k, v in diffs_by_class.items() if k.endswith('sound'))}")
PY
