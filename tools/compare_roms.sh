#!/usr/bin/env bash
# Run the original and lifted ROMs headless in mGBA with the same input and
# write side-by-side screenshots (original | lifted) every N frames.
#   tools/compare_roms.sh FRAMES EVERY OUTDIR [INPUT_TRACE]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FRAMES=$1; EVERY=$2; OUT=$3; INPUT=${4:-}
RUN="$ROOT/build-tools/mgba_run"
MARKER=$(arm-none-eabi-nm "$ROOT/cport/build/bn6f_lifted.elf" | awk '$3=="g_fatal_marker"{print "0x"$1}')
mkdir -p "$OUT/orig" "$OUT/lifted"
ARGS=(--frames "$FRAMES" --every "$EVERY" --quiet)
[ -n "$INPUT" ] && ARGS+=(--input "$INPUT")
"$RUN" "$ROOT/bn6f/bn6f.gba" "${ARGS[@]}" --shots "$OUT/orig" > "$OUT/orig.log" 2>&1 &
"$RUN" "$ROOT/cport/build/bn6f_lifted.gba" "${ARGS[@]}" --shots "$OUT/lifted" --marker "$MARKER" > "$OUT/lifted.log" 2>&1 || true
wait
cat "$OUT/lifted.log" | tail -5
python3 - "$OUT" <<'PY'
import os, sys
from PIL import Image
out = sys.argv[1]
names = sorted(f for f in os.listdir(f"{out}/orig") if f.endswith(".ppm"))
rows = []
for n in names:
    a = Image.open(f"{out}/orig/{n}")
    lp = f"{out}/lifted/{n}"
    b = Image.open(lp) if os.path.exists(lp) else Image.new("RGB", a.size, (255, 0, 255))
    diff = sum(1 for p, q in zip(a.getdata(), b.getdata()) if p != q)
    rows.append((n, a, b, diff))
    print(f"{n}: {diff} differing pixels")
W = Image.new("RGB", (484, 164 * len(rows)), (255, 255, 255))
for i, (n, a, b, d) in enumerate(rows):
    W.paste(a, (0, i * 164)); W.paste(b, (244, i * 164))
W.save(f"{out}/compare.png")
PY
