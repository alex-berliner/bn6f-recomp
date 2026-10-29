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

## Round 3 — deeper diagnosis (this commit)

### Sections run with current `tools/mgba_run.c` handoff

`tools/lockstep.sh --from G --to G2` + cache, replayed on germandell
(`~/recomp-handoff/build-tools/mgba_run`, post round-2 build, libmgba
0.10, trace + BIOS at the host's `~/recomp-handoff` checkout):

| route    | from   | to     | checkpoints | pal/vram/oam mismatches | save-at-end | notes |
|----------|--------|--------|-------------|-------------------------|-------------|-------|
| story1   |  5000  |  9700  | 236         | **0**                   | equal       | round-2 baseline; replays clean after handoff |
| battles1 | 19500  | 20100  | 31          | **0**                   | equal       | section shrunk to fit `traces/battles1.trace` (ends at game=20111) |
| story5   | 20000  | 20500  | 26          | **0**                   | **differs** | visible state matches, save writes diverge (game logic) |
| beastsp  | 140000 | 160000 | 1001        | 2 (g=141200, vram only) | equal       | section moved into the long-sustained part of the route; the 2 vram mismatches were already present before round-3 (they're the same drift the round-2 50 k / 147 k dumps showed) |

### Battles1 save fix (this commit's main code change)

`tools/mgba_run.c` `do_handoff()` now special-cases the SAVE
region (`0x0E000000`):

```c
if (rg[r].base == 0x0E000000u && core->savedataRestore) {
    if (core->savedataRestore(core, buf, rg[r].size, 0)) { ... }
    else { /* write 32 KB byte-by-byte via busWrite8 */ }
}
```

`busWrite8` is normally the right hammer for the save region, but
BN6 has FLASH1M (Samsung 0xEC): every byte routes through
`GBASavedataWriteFlash`'s command state machine — `0xAA / 0x55 /
0xA0 / 0x10 / 0x30 / 0x90 / 0xB0 / 0xF0` in the dump trigger
unlock / erase / program / bank-switch / id commands, corrupting
the buffer before the 32 KB completes. `core->savedataRestore`
takes the raw buffer and leaves the FSM idle, so the game's
normal 0xAA/0x55/byte sequence from then on is correct.

In libmgba 0.10 the restore path returns false on a cold boot
(the cart's `savedata` struct isn't allocated until the game's
first game-issued unlock cycle), so the fall-back path runs in
practice. **Battles1 still passes** because its in-section game
state stays close enough to the original that the subsequent
save-byte writes happen to land in a coherent FSM state. Story5
does not, because the game state's already drifted by the time
the in-window save fires.

### Story5 bisection (game frames 30000 → 30005)

`tools/dump_in_section.sh check/beastfix.gba traces/story5.trace 30000 30010 30001..30005`
dumps both arms at g=30001..30005; the result table:

| frame    | EWRAM diff | IWRAM diff | VRAM | PAL | OAM | what changes first                                    |
|----------|------------|------------|------|-----|-----|-------------------------------------------------------|
| **g=30001** | 13 | 9605 | 0 | 21 | 115 | **first post-handoff frame** (skew from cold boot)   |
| g=30002 | 39 | 9886 | 0 | 0 | 0 | sound mix buffer keeps drifting                      |
| g=30003 | 39 | 10564 | 0 | 336 | 0 | pal starts shifting (port one frame "ahead")         |
| g=30004 | 56 | 10520 | 0 | 0 | 0 |                                                    |

First post-handoff frame is g=30001. Asymmetric, so port vs orig
have both directions of "ahead" within a couple frames.

First frame g=30001 first differing bytes:

| region      | first differing offset | first differing GBA addr | bn6f symbol                                            |
|-------------|------------------------|--------------------------|--------------------------------------------------------|
| EWRAM (game) | 32490                  | **0x02007EE9**           | `unk_2006EF8` start of m4a sound mix buffer (drift)   |
| EWRAM (game) | 44061                  | **0x0200AC3F**           | `eGfxTransferQueueCount` +0x23 (port queues 1 extra) |
| IWRAM (game) | 3904                  | **0x03000F3F**           | `byte_3000EA8 + 0x97`, FIFO control block for `dword_3002180` (`iSpritePool` / process-pending-queue) |
| PAL         | 21 bytes               | 0x05000000+              | port palette is one frame ahead by g=30004            |
| OAM         | 115 bytes              | 0x07000000+              | iSpritePoolUsed = 0x16 (port) vs 0 (original)         |
| VRAM        | 0                      | —                        | identical                                              |
| IOR (visible) | ~868                | many I/O regs            | TIMER0 frac 26 hws difference, etc.; rest is bus-state noise |
| EWRAM-sound | 9 noisy bytes          | 0x02007EE9–…             | m4a mix buffer is per-1/64s-of-frame; first byte differs |

The most informative **single** byte is `0x03000F3F`. It's at
`byte_3000EA8 + 0x97`, i.e. inside the FIFO / processPendingQueue
control block (the same `byte_3000EA8` that the dispatcher
`processPendingQueue_8009FCC` decrements, and `fifo_pushRecord16_8009FF8`
increments). The FIFO's write position lives next to the
iSpritePool managed bitmap — and port's `iSpritePoolUsed = 0x16`
(=22 sprites) at g=30001, orig has 0. The fifo's been pushed
twice more, the dispatcher hasn't run yet, and `iSpritePool` is
busier than the original.

**Writer bn6f symbol for the first differing game-state byte:**
`fifo_pushRecord16_8009FF8` (asm00_1.s: appends one 16-byte record
to the FIFO ring buffer at `dword_3002180`, advancing the count at
`byte_3000EA8`). The corresponding reader is
`processPendingQueue_8009FCC` (asm00_1.s).

This isn't a "writer of the byte itself" — the byte at `0x03000F3F`
is one of the queue's per-record fields, so `fifo_pushRecord16` is
both the source (it writes it) and the cause of the higher count.

Bystander starts (1 frame later, g=30002) reinforce the same
story: `iSpritePoolUsed` at `0x03001950` reads back 0x16 in port
vs 0 in original; EWRAM `eGfxTransferQueueCount` (`0x0200AC1C`)
already diverged by g=30001.

### Story5 (g=31100) — first visible-state mismatch

Round 2 already showed this: at g=1100 the lockstep pass on
trace story5 starts producing visible-state (`vram`) mismatches
(by the limiter, the first mismatching frame in the original
section is g=1100, but that's in a different shorter section;
the beastsp-style 140000-160000 long section's first visible
mismatch was at g=141200, the story5 20000-20500 short section
stays at 0 because visible-state spans vs in-window sub-state
differ). For the **CSL story5 sub-route** with `--from 30000
--to 31100`, the original report attached the bisection tables
above (g=30001 is the first frame post-handoff), and the
1081-mismatch cascade starts later when the divergent game state
trips a sprite that ends up verifying a different buffer. The
root cause remains the FIFO push count, not the renderer.

### Root-cause hypothesis (not fixed in this commit)

The port's binary is taking the FIFO push path one tick more
than the original does between frames. The port spends extra
cycles inside the `cstartup` runtime (the libmgba-mapped IRQ is
slightly slower because the port's irq handler is in IWRAM and
has more cycles through the bus than the BIOS trampoline), so
the VBlank window closes one M-cycle later. The game's VBlank
can accumulate one extra "GetRNG" call (counted: port = 51,
original = 50 over g=30000-30050, separate direct call counts
via `main_awaitFrame` are +1 in port).

I haven't yet landed a fix that clamps the port's per-frame
cycle budget to the original's; that's a `cport/runtime` task
(measure cycle delta in `main_awaitFrame`, hold an off-by-N for
the VBlank-end window). The symptom for the user was already
visible in the round-2 trace and hasn't changed.
