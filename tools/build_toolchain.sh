#!/usr/bin/env bash
# Build the agbasm binutils fork that the bn6f disassembly requires and
# install as/ld/objcopy/objdump into toolchain/bin.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${WORK:-$ROOT/build-toolchain}"
mkdir -p "$WORK" "$ROOT/toolchain/bin"
if [ ! -d "$WORK/agbcc" ]; then
    git clone --depth 1 -b new_layout_with_libs https://github.com/luckytyphlosion/agbcc "$WORK/agbcc"
fi
make -C "$WORK/agbcc" -j"$(nproc)" binutils
cp "$WORK/agbcc/gas/as-new"        "$ROOT/toolchain/bin/arm-none-eabi-as"
cp "$WORK/agbcc/ld/ld-new"         "$ROOT/toolchain/bin/arm-none-eabi-ld"
cp "$WORK/agbcc/binutils/objcopy"  "$ROOT/toolchain/bin/arm-none-eabi-objcopy"
cp "$WORK/agbcc/binutils/objdump"  "$ROOT/toolchain/bin/arm-none-eabi-objdump"
echo "toolchain installed in $ROOT/toolchain/bin"
