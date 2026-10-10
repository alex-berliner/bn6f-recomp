# BN6 Falzar Recomp

**Mega Man Battle Network 6: Cybeast Falzar** (GBA, USA) rebuilt as C, then as readable C.

1. **Lift.** Every function of the original ARM/Thumb code, taken from the byte-matching
   [bn6f disassembly](https://github.com/dism-exe/bn6f), is translated into exact C and
   compiled back into a GBA ROM that plays the same game.
2. **Rewrite.** The lifted functions are replaced one by one with readable, typed C. Each
   replacement is tested against the original and kept only if it behaves identically.
   Most are written by language models in volume and checked automatically.
3. **Record.** Everything learned along the way goes back into the disassembly (names,
   comments) and into a symbol database with the evidence behind each finding.

The goals: a version of the game that is readable C rather than assembly, that stays
faithful to the original frame by frame, could eventually run faster than it, and whose
method could be reused for other GBA games with a disassembly.

**Status (October 2026):** the C ROM plays identically to the original on all 16 recorded
gameplay routes (43% of the game's code exercised). About 1,435 of 14,000 functions are
readable C, most of them still being moved from register-level C to typed C. On an
emulator the port runs at about 90-99% of the original's speed.

## Build the ROM

No ROM or BIOS is needed: the disassembly assembles the original game from source, and
the build checks it against the original's SHA-1 before lifting it.

**Linux or WSL:** install git, make, a C compiler, python3, libpng
(dev headers), bison, flex, texinfo and `arm-none-eabi-gcc` (Debian/Ubuntu:
`sudo apt install build-essential python3 libpng-dev bison flex texinfo gcc-arm-none-eabi`), then

```sh
git clone https://github.com/alex-berliner/bn6f-recomp && cd bn6f-recomp
./build.sh             # -> cport/build/bn6f_lifted.gba
./build.sh supercard   # also a copy for SuperCard-style flash carts, in roms/out/
```

**Windows:** run `build.bat`. It runs `build.sh` under WSL (install it with `wsl --install`),
or under another bash (MSYS2, Git Bash) that has the tools above.

The first build also compiles the binutils fork the disassembly needs (a few minutes).

## What's where

| Path | What |
|---|---|
| `bn6f/` | the disassembly (submodule; a fork with this project's names and comments) |
| `tools/lifter/` | the ARM/Thumb-to-C lifter |
| `cport/` | the port: generated lifted C, readable functions in `src/game/`, the test harness in `host/` |
| `tools/wf/` | the workflows that capture test inputs, convert functions, verify and record ([map](docs/orchestration.html)) |
| `tools/convert/` | the model farm that converts small functions |
| `tools/abi/` | calling-convention tools ([target convention](docs/C_CALLING_CONVENTION.md)) |
| `symbols/db/` | one record per discovered symbol: meaning, evidence, how to reproduce it |
| `tools/traces/` | recorded gameplay routes, keyed to the game's own frame counter |
| `docs/` | [process and decisions](docs/PROCESS.md), [workflows](docs/workflows/), [plans and reviews](docs/reviews/) |

## How changes are checked

Every readable function is compared with its lifted original on real recorded game
states and random inputs (`cport/host` difftest). The whole port is then replayed beside
the original on every recorded route, comparing palette, VRAM and OAM at each checkpoint
(lockstep), and a CPU oracle compares functions directly. The test tools need your own
GBA BIOS dump (`gbarecomp/bios/gba_bios.bin` for now) and the `mgba_run` harness; their
setup is still being consolidated.

## Legal

This repository contains no ROM, BIOS or game assets beyond what the bn6f disassembly
already publishes. Mega Man Battle Network is a trademark of Capcom; this is an
unofficial fan project.
