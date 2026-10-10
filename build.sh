#!/usr/bin/env bash
# Build the recomp ROM (the game as C, compiled back to a GBA ROM) from a fresh clone:
#   ./build.sh            -> cport/build/bn6f_lifted.gba
#   ./build.sh supercard  -> also roms/out/bn6f_falzar_cport_supercard.gba (for SuperCard-style flash carts)
#
# Needs: git, make, a host C compiler, python3, libpng (dev headers), bison, flex, texinfo
# (for the bn6f binutils fork) and arm-none-eabi-gcc (e.g. Debian/Ubuntu: gcc-arm-none-eabi).
# No ROM or BIOS is needed: the bn6f disassembly builds the original game from source.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
JOBS="$(nproc 2>/dev/null || echo 4)"
step() { printf '\n== %s\n' "$*"; }

need() { command -v "$1" >/dev/null || { echo "missing: $1 ($2)"; exit 1; }; }
need git "git"; need make "make"; need cc "a host C compiler"; need python3 "Python 3"
need readelf "binutils"; need arm-none-eabi-gcc "the ARM GCC toolchain, e.g. gcc-arm-none-eabi"

step "1/5 disassembly (bn6f submodule)"
git submodule update --init bn6f

step "2/5 the binutils fork bn6f assembles with (once)"
[ -x toolchain/bin/arm-none-eabi-as ] || tools/build_toolchain.sh
TC="$ROOT/toolchain/bin"

step "3/5 the original game from the disassembly"
make -C bn6f/tools/gbagfx >/dev/null
TOOLS=(AS="$TC/arm-none-eabi-as" LD="$TC/arm-none-eabi-ld"
       OBJCOPY="$TC/arm-none-eabi-objcopy" OBJDUMP="$TC/arm-none-eabi-objdump" PY=python3)
make -C bn6f "${TOOLS[@]}" assets -j"$JOBS" >/dev/null
make -C bn6f "${TOOLS[@]}" -j"$JOBS" >/dev/null
echo "$(cut -c1-40 bn6f/bn6f.sha1)  bn6f/bn6f.gba" | sha1sum -c --quiet \
    && echo "bn6f/bn6f.gba matches the original (SHA-1 $(cut -c1-40 bn6f/bn6f.sha1))"

step "4/5 lift the game to C"
python3 tools/lifter/lift.py --bn6f bn6f --out cport/src/lifted --inc cport/include --overrides cport/src/game \
    | grep -v iteration

step "5/5 compile the C port to a GBA ROM"
make -C cport -j"$JOBS" >/dev/null
echo "built cport/build/bn6f_lifted.gba"

if [ "${1:-}" = supercard ]; then
    mkdir -p roms/out
    python3 tools/mkrom_supercard.py cport/build/bn6f_lifted.gba roms/out/bn6f_falzar_cport_supercard.gba
    echo "built roms/out/bn6f_falzar_cport_supercard.gba"
fi
