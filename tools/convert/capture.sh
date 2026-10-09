#!/usr/bin/env bash
# capture.sh OUTDIR TRACE G0 G1 N FUNC...: run the original ROM over game
# frames G0..G1 of TRACE and save the entry and exit machine state of the
# first N calls of each FUNC (ROM functions), as OUTDIR/FUNC/snap_*.bin
# (+ .exit.bin): the inputs for checking candidate conversions.
# KEEP_INTERRUPTED=1: also keep the entry of calls an interrupt arrived in
# (no .exit.bin); long functions are nearly always interrupted, and difftest
# -S and the CPU oracle need only the entry.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
. "$ROOT/tools/common.sh"
OUT=$(realpath -m "$1"); TRACE=$(realpath "$2"); G0=$3; G1=$4; N=$5; shift 5
ORIG="$ROOT/bn6f/bn6f.gba"
RAW=$(mktemp -d)
trap 'rm -rf "$RAW"' EXIT   # snapshots are large: never leave them behind
declare -A NAME
arm-none-eabi-nm "$ROOT/bn6f/bn6f.elf" > "$RAW/syms.txt"
for f in "$@"; do
  a=$(awk -v n="$f" '$3==n{print $1}' "$RAW/syms.txt" | head -n 1)
  [ -n "$a" ] || { echo "no symbol $f" >&2; continue; }
  echo "$a" >> "$RAW/funcs.txt"
  NAME[$(printf '%08x' $((16#$a & ~1)))]=$f
done
mkdir -p "$RAW/snaps"
"$ROOT/build-tools/mgba_run" "$ORIG" --quiet --frames $(( G1 * 3 )) --bios "$ROOT/gbarecomp/bios/gba_bios.bin" \
  --input "$TRACE" --game-frame-addr 0x020093D4 --profile-game "$G0" "$G1" \
  ${KEEP_INTERRUPTED:+--snap-keep-interrupted} \
  --snap-funcs "$RAW/funcs.txt" "$RAW/snaps" "$N" $(poke_args "$ORIG" "$TRACE") > /dev/null
tag=$(basename "$TRACE" .trace)
for f in "$RAW"/snaps/snap_*.bin; do
  [ -e "$f" ] || continue
  case "$f" in *.exit.bin) continue ;; esac
  b=$(basename "$f" .bin); id=${b#snap_}; addr=${id%_*}; n=${id#*_}
  fn=${NAME[$addr]:-}
  [ -n "$fn" ] || continue
  # without KEEP_INTERRUPTED only calls with an exit are kept (mgba_run removed the rest)
  [ -e "$RAW/snaps/$b.exit.bin" ] || [ -n "${KEEP_INTERRUPTED:-}" ] || continue
  mkdir -p "$OUT/$fn"
  mv "$f" "$OUT/$fn/snap_${tag}_$n.bin"
  if [ -e "$RAW/snaps/$b.exit.bin" ]; then mv "$RAW/snaps/$b.exit.bin" "$OUT/$fn/snap_${tag}_$n.exit.bin"; fi
done
for f in "$@"; do
  calls=$(ls "$OUT/$f"/snap_*.bin 2>/dev/null | grep -vc '\.exit\.bin$' || true)
  echo "$f: $calls calls"
done
