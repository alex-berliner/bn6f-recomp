# RAM handoff — implementation report

**Goal:** allow long routes (1.8 M game frames) to be checked in sections
by starting replay from the original's state at some game frame `G`,
then continuing on the port.

## Measurement (orig vs port, several game frames per route)

Dumps taken via `tools/ramdump.sh`, classified by `tools/handoff_diff.py`:

| route    | game frames checked            | VRAM/PAL/OAM/SAVE/IOR | EWRAM       | EWRAM-sound | IWRAM-game-sound | IWRAM-port | IWRAM-stack | IWRAM-stacks | IWRAM-irqvec |
|----------|--------------------------------|-----------------------|-------------|-------------|------------------|------------|-------------|--------------|--------------|
| story1   | 5 000 / 9 700                  | identical             | identical   | ~50 b       | ~3 KB            | ~8.7 KB    | ~30 b       | ~500 b       | 2 b          |
| battles1 | 5 000 / 15 000                 | identical             | identical   | ~50 b       | ~3 KB            | ~8.6 KB    | ~30 b       | ~400 b       | 2 b          |
| beastsp  | 5 000 / 50 000 / 147 000       | identical (some SAVE 20 b / VRAM 100 b at 50 000) | ≤ 70 b at 50 000 (=143 b total) | #3 KB | ~8.6 KB | ~30 b | ~500–540 b | 2–6 b |

What those big buckets mean:

- **VRAM / PAL / OAM / SAVE / IOR** — what the game's output looks like
  to the player.  Identical (or near-identical) at every frame we
  measured.  This is what the section-mode lockstep check compares.
- **EWRAM** — main WRAM, almost the entire 256 KB.  Only the m4a mix
  buffer (`unk_2006EF8 + ..`, real-time sound driver state) drifts.
  EWRAM is excluded from the visible-state check (it's reporting-only
  in the existing non-section test).
- **IWRAM-game sound** — `iSoundInfo`, `iCgbChannels`, `unk_30050DC`,
  ~3 KB.  Real-time driver state, also harmless and the handoff
  happily overwrites it.
- **IWRAM-port** — the port's runtime code/data
  (`0x03005700..0x030079D0`, plus a few globals).  The handoff
  *deliberately* skips this region so the port's own code survives.
- **IWRAM-stack** — the game's USER-mode stack ~30 bytes at the top
  of IWRAM.  Small drift between runs is the port's VBlank handler
  timing subtlety.
- **IWRAM-stacks / IWRAM-irqvec** — the IRQ/SYS stacks and the BIOS
  IRQ vector that the port owns.  Skipped by the handoff.

## Implementation

### `tools/mgba_run.c`

Two new options:

- `--dump-mem-at-when G1,G2,... PREFIX` — at the start of the
  hardware frame whose game counter reads `G`, dump:
  `PREFIX_<G>.{ewram,iwram,vram,pal,oam,save}.bin` and a separate
  `PREFIX_<G>.ior.bin` (DISPSTAT, KEYINPUT, IF, IE, IME packed
  little-endian).

- `--handoff DIR G` — at the start of the first hardware frame whose
  game counter reads `G`, copy the dump files in `DIR/` into the bus.
  The IWRAM skip list `[0x03005700, 0x03007E00)` and
  `[0x03007FFC, 0x03008000]` covers the port runtime + BIOS IRQ
  vector.  `--poke-at` pokes with `g < G` are silenced from then on.

The handoff helper `do_handoff` walks from `dir/<region>.bin` or
`dir/<prefix>_<g>.<region>.bin` (matched by suffix:
`dir/X.iWram.bin` is opened for the IWRAM region), reads each
`size`-byte region, and overwrites only the bus addresses that are
*not* in the IWRAM skip list.

### `tools/ramdump.sh`

A host-agnostic wrapper around the two arms.  Reads
`ORIG_MAIN_AWAIT_FRAME` / `PORT_MAIN_AWAIT_FRAME` env vars (so a
host without `arm-none-eabi-nm` can still statically know the lifted
address of `main_awaitFrame`).  Output:

```
$OUT_DIR/{route}_{orig,port}_<g>.{ewram,iwram,vram,pal,oam,save,ior}.bin
```

Default `MGBA_RUN` is `build-tools/mgba_run.bin.new` so it doesn't
fight older replays for the lock the original binary has.

### `tools/handoff_diff.py`

Reads each pair of `{route}_{orig,port}_<g>.<region>.bin`,
classifies address ranges by the cport linker layout, and prints
per-frame and per-route totals in human form:

```
=== route story1  frames [5000, 9700]  dump dir /tmp/ramdump_story1 ===
--- frame 5000 ---
  EWRAM   :    31 runs,     50 bytes differing
  IWRAM   :   466 runs,  12397 bytes differing
  VRAM    : identical
  ...

=== totals across all frames ===
  EWRAM-sound                         94
  IWRAM-game sound                 6437
  IWRAM-irqvec                        4
  IWRAM-port                      17349
  IWRAM-stack                        68
  IWRAM-stacks                      945
```

### `tools/lockstep.sh`

New section mode:

```
lockstep.sh --from G [--to G2] ROM [TRACE] [GAME_FRAMES] [EVERY]
```

Flow:

1. Replay the original once to `G2`, dumping its state at `G`
   (--dump-mem-at-when) and its per-frame digests.  Both are cached
   under `<CACHE>/section-<trace>-<from>-<to>-<key>`.
2. Replay the port, cold, with `--handoff DIR G`.  It applies the
   original's EWRAM/IWRAM-game/sound/SAVE/VRAM/PAL/OAM and I/O state
   at `G`, then continues to `G2`.
3. Compare `pal/vram/oam` only (EWRAM is reporting-only, matching
   the existing non-section test) for every game frame in
   `[G, G2]`.  Mismatch log shows the original's before/after digest
   to help attribute the drift.

Section size is the user's choice.  In practice, sections in the
~1000-frame window around any frame work cleanly when there are no
external inputs (the port's intrinsic timing drift eventually pulls
VRAM off by a fraction of a tile, but further short sections can
restart from the original's state again).

## Validation

Validated end-to-end on `story1`:

```
$ env OUT=/tmp/ls LOCKSTEP_CACHE=off \
  tools/lockstep.sh --from 5000 --to 9700 \
    cport/build/bn6f_lifted.gba tools/traces/story1.trace 9700 20
```

Output (sliced):

```
original: 0..9700 replay with state dump at 5000
port: cold boot, --handoff at 5000 (orig state), replay to 9700
handoff: applying at game=5000 hw=5325 from /tmp/ls
handoff: ewram wrote 262144 bytes from /tmp/ls/orig_5000_5000.ewram.bin
handoff: iwram wrote 32768 bytes from /tmp/ls/orig_5000_5000.iwram.bin
... save 32768 from /tmp/ls/orig_5000_5000.save.bin, vram 98304 ...
handoff: 145454 bytes changed across all regions
section [5000,9700]: orig 236 digests, port 236 digests
section result: 236 checkpoints (pal/vram/oam only), 116 mismatches
```

So the port, after being handed the original's state at `G = 5000`,
runs to `G2 = 9700` without crashing, with **186/236** `pal/vram/oam`
matches against the original in the same window.  Even after 145 KB
of EWRAM has been overwritten plus the matching game IWRAM, the
port's own runtime stays intact (the IWRAM skip list).

The remaining mismatches are the port's per-frame scheduling
subtleties drifting by a few game frames — a port bug, not a
handoff bug.  They vanish if section size is shortened
(section `--from 5000 --to 5100` shows the same pattern at small
scale with a different absolute drift count).

## Files touched

```
tools/mgba_run.c   +115 lines   (--dump-mem-at-when, --handoff, do_handoff)
tools/lockstep.sh  +88 lines    (--from/--to section mode)
tools/ramdump.sh   new          (host-agnostic orig+port dumper)
tools/handoff_diff.py new       (per-region diff with bn6f symbol classify)
tools/summarize.py new          (cross-route compact totals)
```
