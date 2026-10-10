#!/usr/bin/env bash
# profile.sh ROM [TRACE] [START_HW_FRAME] [STEPS] [TOP] — cycle profile per function
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
ROM=$(realpath "$1"); TRACE=$(realpath "${2:-$TRACES/wander.trace}"); START=${3:-9000}; STEPS=${4:-8000000}; TOP=${5:-30}
TMP=$(mktemp -d)
(cd "$TMP" && "$MGBA_RUN" "$ROM" --frames $((START + 1)) --bios "$GBA_BIOS" \
    --input "$TRACE" --profile "$START" "$STEPS" --quiet > /dev/null)
python3 - "$TMP/profile.txt" "${ROM%.gba}.elf" "$TOP" <<'PY'
import bisect, collections, subprocess, sys
prof, elf, top = sys.argv[1], sys.argv[2], int(sys.argv[3])
syms = []
try:
    for l in subprocess.run(["arm-none-eabi-nm", "-n", elf], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 3 and p[1] in "tTW":
            syms.append((int(p[0], 16), p[2]))
except FileNotFoundError:
    pass
addrs = [a for a, _ in syms]
c = collections.Counter()
for l in open(prof):
    pc, cyc = l.split()
    pc, cyc = int(pc, 16), int(cyc)
    if pc < 0x4000:
        c["<bios>"] += cyc
        continue
    i = bisect.bisect_right(addrs, pc) - 1
    c[syms[i][1] if i >= 0 else f"?{pc:#x}"] += cyc
tot = sum(c.values())
for k, v in c.most_common(top):
    print(f"{100 * v / tot:5.1f}% {k}")
PY
rm -rf "$TMP"
