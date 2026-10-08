# BN6FalzarRecomp

Static recompilation of **Mega Man Battle Network 6: Cybeast Falzar** (GBA, USA)
to a native PC executable, built on the [gbarecomp](https://github.com/mstan/gbarecomp)
framework.

The game's ARM7TDMI ARM/Thumb code is translated ahead of time into C++ and
linked against gbarecomp's GBA hardware runtime (PPU, APU, DMA, timers, IRQs,
SRAM, input). Function boundaries, names, the code/data split and the
ROM→IWRAM code copies come from the byte-matching
[bn6f disassembly](https://github.com/dism-exe/bn6f) (submodule `bn6f/`).

No ROM, BIOS, or ROM-derived generated code is included in this repository.

## Requirements

- Linux (tested), CMake ≥ 3.20, GCC/Clang with C++20, SDL2 dev headers, Python 3, libpng (for gbagfx)
- A GBA BIOS dump: `gbarecomp/bios/gba_bios.bin`
  (SHA-1 `300c20df6731a33952ded8c436f7f186d25d3492`)
- The Falzar ROM: `roms/bn6f.gba`
  (SHA-1 `0676ecd4d58a976af3346caebb44b9b6489ad099`).
  `tools/import_symbols.sh` builds this exact ROM from the disassembly at `bn6f/bn6f.gba`.

## Build

```sh
git submodule update --init --recursive
cp /path/to/gba_bios.bin gbarecomp/bios/gba_bios.bin

tools/build_toolchain.sh      # agbasm binutils fork needed by bn6f (once)
tools/import_symbols.sh       # build bn6f + regenerate symbols/ from its ELF
cp bn6f/bn6f.gba roms/bn6f.gba  # or supply your own dump
tools/regen.sh                # recompile BIOS + ROM into generated*/
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/BN6FalzarRecomp game.toml
```

Run from the repository root so `game.toml` paths resolve.

Headless smoke test:

```sh
./build/BN6FalzarRecomp --no-window --frames 600 --dump-png title.png game.toml
```

The exit banner reports static coverage: `self_heal_coverage=FULLY_STATIC`
means every executed guest instruction ran as recompiled native code. Misses
are bridged through gbarecomp's interpreter, logged to
`recomp_master_misses_BR6E.toml.frag`, and should be reviewed and merged into
`game.toml`.

## Controls (gbarecomp defaults)

| GBA | Keyboard |
|---|---|
| D-Pad | Arrow keys |
| A / B | Z / X |
| L / R | A / S |
| Start / Select | Enter / Right Shift |
| Fast-forward | Hold Tab |

Save states: Shift+F1–F9 save, F1–F9 load. SRAM saves go to `saves/bn6f.sav`.

## Layout

| Path | Purpose |
|---|---|
| `game.toml` | Hand-reviewed recompiler + runtime config (wins over the overlay) |
| `symbols/` | Overlay generated from the bn6f ELF by `tools/import_symbols.sh` |
| `patches/` | Local fixes to gbarecomp, applied by `tools/regen.sh` |
| `generated/`, `generated_bios/` | Recompiled C++ (local only, not committed) |

## Local gbarecomp patches

- `gbarecomp-0001`: the function finder treated the even return address left
  by Thumb `mov lr, pc; bx rN` as an ARM resume point (and word-aligned the
  PC read), seeding bogus ARM functions inside Thumb code. BN6 uses this call
  idiom everywhere.

## Status

- Boots through the recompiled real BIOS (intro skipped) to the title screen.
- New game → opening cutscene in ACDC Town runs **fully static** (7,000 frames,
  zero interpreted instructions).
