#!/usr/bin/env bash
# Build the bn6f disassembly (byte-matching Falzar ROM + ELF) and turn its
# symbol table into the gbarecomp symbol overlay under symbols/.
#
# Requires the agbasm binutils fork in toolchain/bin (see tools/build_toolchain.sh).
# --no-build: use bn6f as last built (the record workflow has just built it).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TC="$ROOT/toolchain/bin"
BN6F="$ROOT/bn6f"
SYM="$ROOT/symbols"

if [ "${1:-}" != --no-build ]; then
  make -C "$BN6F/tools/gbagfx" >/dev/null
  TOOLS=(AS="$TC/arm-none-eabi-as" LD="$TC/arm-none-eabi-ld"
         OBJCOPY="$TC/arm-none-eabi-objcopy" OBJDUMP="$TC/arm-none-eabi-objdump" PY=python3)
  make -C "$BN6F" "${TOOLS[@]}" assets -j"$(nproc)" >/dev/null
  make -C "$BN6F" "${TOOLS[@]}" -j"$(nproc)"
fi

readelf -sW "$BN6F/bn6f.elf" > "$SYM/bn6f_readelf_syms.txt"
readelf -SW "$BN6F/bn6f.elf" > "$SYM/bn6f_readelf_sections.txt"

# Runtime code copies (ROM -> IWRAM), both resolved by symbol name:
#   start.s copies 0x1ED4 bytes of IWRAM routines (IRQ handler etc.) to 0x03005B00.
#   m4a SoundInit CpuSets 0x100 words of SoundMainRAM to byte_3005700.
python3 "$ROOT/gbarecomp/tools/symbol_import/import_decomp_symbols.py" \
    --id BR6E --name "Mega Man Battle Network 6: Cybeast Falzar (USA)" \
    --syms "$SYM/bn6f_readelf_syms.txt" \
    --sections "$SYM/bn6f_readelf_sections.txt" \
    --rom "$BN6F/bn6f.gba" \
    --data-source sections \
    --code-copy-pair "sub_3005B00=IWRAMRoutinesROMLocation:arm:0x1ED4" \
    --code-copy-pair "byte_3005700=SoundMainRAM:thumb:0x400" \
    --rom-end 0x087FFFFF \
    --out "$SYM"
