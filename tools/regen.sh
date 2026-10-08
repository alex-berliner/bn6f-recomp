#!/usr/bin/env bash
# Regenerate the recompiled C++ for BN6 Falzar.
#
#   1. apply local gbarecomp patches (idempotent)
#   2. build gba_recompile
#   3. recompile the GBA BIOS (gbarecomp/bios/gba_bios.bin, user-provided)
#   4. recompile the ROM (roms/bn6f.gba) with the bn6f symbol overlay
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

for p in patches/gbarecomp-*.patch; do
    if git -C gbarecomp apply --check "$ROOT/$p" 2>/dev/null; then
        git -C gbarecomp apply "$ROOT/$p" && echo "applied $p"
    fi
done

ROM="${ROM:-roms/bn6f.gba}"
BIOS="${BIOS:-gbarecomp/bios/gba_bios.bin}"
[ -f "$ROM" ]  || { echo "missing $ROM (sha1 0676ecd4d58a976af3346caebb44b9b6489ad099)"; exit 1; }
[ -f "$BIOS" ] || { echo "missing $BIOS (sha1 300c20df6731a33952ded8c436f7f186d25d3492)"; exit 1; }
echo "0676ecd4d58a976af3346caebb44b9b6489ad099  $ROM" | sha1sum -c --quiet

cmake -S gbarecomp -B build-gbarecomp -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-gbarecomp --target gba_recompile -j"$(nproc)"

mkdir -p generated_bios generated
./build-gbarecomp/gba_recompile --bios "$BIOS" \
    --config gbarecomp/bios/gba_bios.toml --out generated_bios | tail -3

rm -f generated/*.cpp generated/*.h
./build-gbarecomp/gba_recompile --rom "$ROM" \
    --config game.toml --config symbols/BR6E_symbols.toml \
    --symbols symbols/imported_symbols.tsv \
    --data-symbols symbols/imported_data_symbols.tsv \
    --out generated --max-functions 131072 | grep -E "^==>|TOTAL|ERROR"
