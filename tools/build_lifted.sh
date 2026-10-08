#!/usr/bin/env bash
# Lift bn6f to C and build the GBA ROM (and the LIFT_TRACE debug variant).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$ROOT/tools/lifter/lift.py" --bn6f "$ROOT/bn6f" --out "$ROOT/cport/src/lifted" --inc "$ROOT/cport/include" --overrides "$ROOT/cport/src/game" | grep -v iteration
make -C "$ROOT/cport" -j"$(nproc)" | grep -E "error:|\.gba:" || true
if [ "${TRACE:-0}" = 1 ]; then
  make -C "$ROOT/cport" -j"$(nproc)" BUILD=build-trace \
    CFLAGS="-mcpu=arm7tdmi -mthumb -mthumb-interwork -Os -ffreestanding -fno-strict-aliasing -fomit-frame-pointer -DTARGET_GBA -DLIFT_TRACE -Iinclude -w -Werror=return-type" \
    | grep -E "error:|\.gba:" || true
fi
