#!/usr/bin/env bash
# Per-function cycles per game frame, original ROM vs a C-port ROM, over the
# same game-frame window of a trace (input keyed to the game's frame counter).
#   tools/profcmp.sh ROM [TRACE] [G0] [G1] [TOP]
# Functions are matched by name (lifted C functions keep bn6f's symbol names).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
ROM=$(realpath "$1"); TRACE=$(realpath "${2:-$ROOT/tools/traces/wander.trace}")
G0=${3:-3000}; G1=${4:-4000}; TOP=${5:-40}
ORIG="$ROOT/bn6f/bn6f.gba"
TMP=$(mktemp -d)
run() {  # rom dir
  mkdir -p "$2"
  (cd "$2" && "$ROOT/build-tools/mgba_run" "$1" --quiet --frames 200000 --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
     --input "$TRACE" --game-frame-addr 0x020093D4 --profile-game "$G0" "$G1" $(poke_args "$1" "$TRACE") > /dev/null)
}
run "$ORIG" "$TMP/o" & run "$ROM" "$TMP/t" & wait
python3 - "$TMP/o/profile.txt" "$ROOT/bn6f/bn6f.elf" "$TMP/t/profile.txt" "${ROM%.gba}.elf" "$TOP" <<'PY'
import bisect, collections, subprocess, sys
def syms(elf):
    # FUNC symbols only: bn6f's local code labels (loc_*, etc.) would split
    # each function's cycles.
    out = []
    for l in subprocess.run(["arm-none-eabi-readelf", "-sW", elf], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) >= 8 and p[3] == "FUNC":
            out.append((int(p[1], 16) & ~1, p[7]))
    return sorted(set(out))
def load(prof, elf, original):
    s = syms(elf); addrs = [a for a, _ in s]
    c = collections.Counter(); hdr = None
    for l in open(prof):
        if l.startswith("#"):
            hdr = l.split(); continue
        pc, cyc = l.split(); pc, cyc = int(pc, 16), int(cyc)
        if pc < 0x4000:
            c["<bios>"] += cyc; continue
        if original and 0x03005700 <= pc < 0x03005B00:
            c["SoundMainRAM (mixer)"] += cyc     # the original's IWRAM copy; the port's pieces
            continue
        i = bisect.bisect_right(addrs, pc) - 1
        name = s[i][1] if i >= 0 else f"?{pc:#x}"
        if name.startswith("SoundMainRAM_iwram"):
            name = "SoundMainRAM (mixer)"
        c[name] += cyc
    return c, int(hdr[hdr.index("cycles") + 1])
o, ot = load(sys.argv[1], sys.argv[2], True)
t, tt = load(sys.argv[3], sys.argv[4], False)
top = int(sys.argv[5])
idle = {"main_awaitFrame", "main_pollGeneralLCDStatus_STAT_LYC_"}
def busy(c): return sum(v for k, v in c.items() if k not in idle)
print(f"total cycles: original {ot}, tested {tt}; busy (excl. vblank wait): original {busy(o)}, tested {busy(t)} "
      f"({busy(t) / max(1, busy(o)):.2f}x)")
print(f"{'function':48} {'orig':>10} {'tested':>10} {'ratio':>6} {'extra':>10}")
rows = sorted(set(o) | set(t), key=lambda k: -(t[k] - o[k]))
for k in rows[:top]:
    if k in idle: continue
    r = f"{t[k] / o[k]:6.2f}" if o[k] else "   new"
    print(f"{k:48} {o[k]:10d} {t[k]:10d} {r} {t[k] - o[k]:10d}")
PY
rm -rf "$TMP"
