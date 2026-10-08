#!/usr/bin/env bash
# speed.sh ROM [FRAMES] — game frames completed in FRAMES hardware frames
# (real BIOS, scripted input). The original reaches 2924 at 3000.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
N=${2:-3000}
TRACE=${TRACE_FILE:-$ROOT/tools/traces/title_mash_a.trace}
v=$("$ROOT/build-tools/mgba_run" "$1" --frames "$N" --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
    --input "$TRACE" --guest 0x0200a210 --quiet | awk '/guest/ {print $2}')
echo "$1: $((0x${v:4:4})) game frames in $N"
