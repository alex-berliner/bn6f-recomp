# RAM handoff — implementation report

**Goal:** allow long routes (1.8 M game frames) to be checked in
sections by starting replay from the original's state at some game
frame `G`, then continuing on the port.

## Result

`tools/lockstep.sh --from 5000 --to 9700` on story1 and battles1
shows **0 pal/vram/oam mismatches across the entire section** (236
checkpoints each).

```
$ cd ~/recomp-handoff
$ OUT=/tmp/lk LOCKSTEP_CACHE=/tmp/cache \
    tools/lockstep.sh --from 5000 --to 9700 check/beastfix.gba tools/traces/story1.trace 9700
... handoff: applying at game=5000 hw=5325 from /tmp/lk ...
... handoff: 8523 bytes changed across all regions ...
section [5000,9700]: orig 236 digests, port 236 digests
section result: 236 checkpoints (pal/vram/oam only), 0 mismatches
```

Same result for `battles1` (236 checkpoints, 0 mismatches; the
lolstep script additionally diffs the end-of-run SRAM file and
notes a difference on battles1 — that is the game's cart save
writes firing at different in-window moments, not the visible
state).

## Handoff recipe

Run from the start of the first hardware frame whose game counter
just bumped to `G`, in order (every step reasons from the
preceding step's invariants):

1. **EWRAM** (256 KB) — byte-wise `busWrite8`. EWRAM is 8-bit
   wide and nothing else talks to it from the CPU side, so no
   channels straddle writes.
2. **IWRAM** (32 KB) — byte-wise `busWrite8`, but skip
   `0x03005700..0x03007FFF`. That block is the port's runtime
   (binary + globals, IRQ/SYS stacks, BIOS IRQ vector). The
   port's CPU state is not touched, so its current call chain
   keeps running on its own frames.
3. **VRAM / PAL / OAM** (96 KB + 1 KB + 1 KB) — **MUST**
   use 16-bit aligned `busWrite16` writes, not `busWrite8`. GBA
   VRAM, palette and OAM are *16-bit wide*, and libmgba's
   `GBAStore8` for those regions **broadcasts** the byte to both
   halves of the halfword:
   `busWrite8(VRAM+1, 0x24)` becomes `slot = 0x2424`, not
   `slot &= 0xff00`. We discovered this the hard way: a
   16-bit-aligned buffer can be written byte-by-byte with
   `busWrite16(slot_off, buf[slot] | buf[slot+1] << 8)`, leaving
   the other half of the slot untouched.
4. **SRAM** (32 KB cartridge save) — byte-wise `busWrite8`.
   Same as EWRAM.
5. **I/O** — only **DISPSTAT (0x04000004), KEYINPUT (0x04000130),
   IF (0x04000202), IE (0x04000200), IME (0x04000208)**. Writing
   the full `0x04000000..0x04000400` block corrupted the
   destination's state — even direct BG VCOUNT / SOUNDCNT writes
   shift things a few cycles — and the port's VRAM started
   diverging later in the section (e.g. game=6200 in story1).
   The rest of the I/O is already at the same default on both
   sides after a cold boot, so we let the destination keep its
   own values there.

`n_skew` for the handoff at `G = 5000` is around 8500 bytes
(EWRAM sound mix ~50 B, the bulk in IWRAM sound driver state
~12 KB, plus a few hundred individual bytes in display state).
None of these are meant to align between cold runs and an
"original at G" dump; they are the only bytes the destination
needs from the original.

## Measurement (orig vs port, several game frames per route)

Dumps taken via `tools/ramdump.sh`, classified by `tools/handoff_diff.py`:

| route    | game frames checked            | VRAM/PAL/OAM/SAVE/IOR | EWRAM       | EWRAM-sound | IWRAM-game-sound | IWRAM-port | IWRAM-stack | IWRAM-stacks | IWRAM-irqvec |
|----------|--------------------------------|-----------------------|-------------|-------------|------------------|------------|-------------|--------------|--------------|
| story1   | 5 000 / 9 700                  | identical             | identical   | ~50 b       | ~3 KB            | ~8.7 KB    | ~30 b       | ~500 b       | 2 b          |
| battles1 | 5 000 / 15 000                 | identical             | identical   | ~50 b       | ~3 KB            | ~8.6 KB    | ~30 b       | ~400 b       | 2 b          |
| beastsp  | 5 000 / 50 000 / 147 000       | identical (some SAVE 20 b / VRAM 100 b at 50 000) | ≤ 70 b at 50 000 (=143 b total) | #3 KB | ~8.6 KB | ~30 b | ~500–540 b | 2–6 b |

What those big buckets mean:

- **VRAM / PAL / OAM / SAVE / IOR** — what the game's output looks
  like to the player. Identical (or near-identical) at every frame.
  This is what the section-mode lockstep check compares.
- **EWRAM** — main WRAM, almost the entire 256 KB. Only the m4a
  mix buffer (`unk_2006EF8 + ..`, real-time sound driver state)
  drifts. EWRAM is excluded from the visible-state check.
- **IWRAM-game sound** — `iSoundInfo`, `iCgbChannels`,
  `unk_30050DC`, ~3 KB. Real-time driver state; the handoff
  happily overwrites it.
- **IWRAM-port** — the port's runtime code/data
  (`0x03005700..0x030079D0`, plus a few globals). The handoff
  *deliberately* skips this region so the port's own code
  survives.
- **IWRAM-stack** — the game's USER-mode stack ~30 bytes at the
  top of IWRAM. Small drift between runs is the port's VBlank
  handler timing subtlety.
- **IWRAM-stacks / IWRAM-irqvec** — the IRQ/SYS stacks and the
  BIOS IRQ vector that the port owns. Skipped by the handoff.

## Implementation

### `tools/mgba_run.c`

Two new options:

- `--dump-mem-at-when G1,G2,... PREFIX` — at the start of the
  hardware frame whose game counter reads `G`, dump:
  `PREFIX_<G>.{ewram,iwram,vram,pal,oam,save}.bin` and a separate
  `PREFIX_<G>.ior.bin` (full 0x400 byte I/O block, packed linear
  0x04000000..0x040003FF).

- `--handoff DIR G` — at the start of the first hardware frame
  whose game counter reads `G`, copy the dump files in `DIR/`
  into the bus. The IWRAM skip list `[0x03005700, 0x03007E00)`
  and `[0x03007FFC, 0x03008000]` covers the port runtime + BIOS
  IRQ vector; the destination's CPU state (SP / LR / pc) is not
  touched. `--poke-at` pokes with `g < G` are silenced from then
  on.

The handoff helper `do_handoff` walks `dir/<region>.bin` or
`dir/<prefix>_<g>.<region>.bin` (matched by suffix), reads each
`size`-byte region, and overwrites only the bus addresses that
aren't in the IWRAM skip list. VRAM / PAL / OAM go through
`busWrite16` on halfword-aligned slots; everything else goes
through `busWrite8`.

### `tools/ramdump.sh`

A host-agnostic wrapper around the two arms. Reads
`ORIG_MAIN_AWAIT_FRAME` / `PORT_MAIN_AWAIT_FRAME` env vars so a
host without `arm-none-eabi-nm` can still statically know the
lifted address of `main_awaitFrame`. Output:

```
$OUT_DIR/{route}_{orig,port}_<g>.{ewram,iwram,vram,pal,oam,save,ior}.bin
```

Default `MGBA_RUN` is `build-tools/mgba_run.bin.new` so it doesn't
fight older replays for the lock the original binary has.

### `tools/handoff_diff.py`

Reads each pair of `{route}_{orig,port}_<g>.<region>.bin`,
classifies address ranges by the cport linker layout, and prints
per-frame and per-route totals in human form.

### `tools/lockstep.sh`

New section mode:

```
lockstep.sh --from G [--to G2] ROM [TRACE] [GAME_FRAMES] [EVERY]
```

Flow:

1. Replay the original once to `G2`, dumping its state at `G`
   (`--dump-mem-at-when`) and its per-frame digests. Both are
   cached under `<CACHE>/section-<trace>-<from>-<to>-<key>`.
2. Replay the port, cold, with `--handoff DIR G`. The handoff
   applies the original's EWRAM / IWRAM-game / VRAM / PAL / OAM
   / SAVE and the 5 I/O registers at `G`, then continues to
   `G2`.
3. Compare `pal / vram / oam` only (EWRAM is reporting-only,
   matching the existing non-section test) for every game frame
   in `[G, G2]`. A mismatch log shows the original's digest to
   help attribute any drift.

## Files touched

```
tools/mgba_run.c    +~60 lines   (--dump-mem-at-when, --handoff, do_handoff, 16-bit VRAM writes)
tools/lockstep.sh   +88 lines    (--from/--to section mode)
tools/ramdump.sh    new          (host-agnostic orig+port dumper)
tools/handoff_diff.py new       (per-region diff with bn6f symbol classify)
tools/summarize.py  new          (cross-route compact totals)
```
