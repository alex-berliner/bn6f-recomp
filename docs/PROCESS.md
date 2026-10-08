# How this project is run

Written 2026-10-08 from the working session's context and Claude's memory
notes, so that the process, the decisions behind it and the lessons learned
live in the repository and not only in a conversation. Keep it current; the
"State when written" section at the end is a snapshot.

Two ports share this repository:

- **PC port** (`gbarecomp`, C++): see `README.md`.
- **GBA C port** (`cport/`): the game as C, compiled back to a GBA ROM. First
  *lifted* C (a mechanical translation of every function, done), then
  progressively *readable* C that replaces it function by function, each
  verified against the original game. This is the project's main goal; the
  user chose "lifted C, then readable".

## 1. Decisions and preferences (from the user)

| What | Detail |
|---|---|
| Goal | Readable C. Prioritise conversion work over side quests. |
| Git | Local only. Never push (GitHub "not yet"). Commit on branches. |
| BIOS | Use the real GBA BIOS (`gbarecomp/bios/gba_bios.bin`) for both ports and all tests. |
| Disassembly | Fix bn6f defects on its local branch `recomp-fixes`; the built ROM must stay byte-identical (bn6f's `make` checks `bn6f.sha1`). |
| Evidence | Wants a reliable test method, not "does the title screen boot". |
| Tools | Results over originality: reuse existing tools before writing new ones. |
| Recordings | The user records gameplay sessions; ask for recordings rather than scripting inputs blindly (`tools/traces/SESSIONS.md`). |
| Symbols | **Every discovery about a symbol goes back into the disassembly (names, comments) and into the symbol database with evidence and reproduction steps** (section 6). |
| Coverage | Prefer converting functions the recorded routes execute, so lockstep checks them; mark exceptions "oracle only". |
| Conversion split | Agreed: Claude converts whole subsystems (callers and callees together); outside models handle small leaf functions in the background; one farm process per provider so review is not held up by the slowest provider. |
| Model cost | Cost per converted function is the most important factor; test all models over time. glm-5.3-flash gave the user good results. |
| Pacers | The local API pacers (`pi-pacer@{minimax,zai,opencode}.service`) are the user's setup: leave their settings alone unless requests start erroring. |
| Flash cart | Paused ("done with flash cart stuff for now"); speed is to be looked at holistically later (section 8). |
| Secrets | Never print API keys. |

## 2. The C port

### Build

```sh
tools/build_lifted.sh               # lift bn6f into cport/src/lifted (+ headers) and build the ROM
make -C cport -j$(nproc)            # rebuild only: cport/build/bn6f_lifted.gba (13.5 MB, entry 0x08800000)
make -C cport/host -j$(nproc)       # host difftest build: cport/build-host/difftest
```

Variants: `make BUILD=dir`, `OPT=` (hand-written/runtime code, default `-Os`),
`LIFTED_OPT=` (lifted code, default `-O2`; `-Os` was measured slower at both
cart timings because executed instructions dominate), `-DBOOT_DIAG` (coloured
boot stages for hardware debugging), `LTO=0`.

### Layout

| Path | What |
|---|---|
| `tools/lifter/lift.py` | The lifter: bn6f (assembly + ELF) to C. Each function's header comment gives its register contract (`inputs`, `outputs`, `clobbers`). |
| `cport/src/lifted/` | Generated lifted C (not committed). |
| `cport/src/game/*.c` | Readable C. A `Next NAME(void)` here replaces the lifted function; the lifted original stays as `L_NAME` for testing. |
| `cport/include/game.h`, `cpu.h` | Macros readable code uses (below). `bn6f_syms.h` and `lifted.h` are generated. |
| `cport/src/gba/` | Boot (`boot.c`), IRQ entry, debug (`rt_fatal` turns the screen magenta). |
| `cport/src/runtime/swi.c` | BIOS services: the GBA build calls the **real BIOS**; the host test build uses C versions that follow the BIOS routines register for register. |
| `tools/lifter/iwram.txt` | Lifted functions placed in IWRAM. |

### Writing readable code

- Registers: `R0`..`R12`, `SP`, `LR`; memory: `LD8/16/32`, `ST8/16/32`,
  `LDS16`/`LDS8` (sign-extending). **Use `LDS16` where the original uses
  `ldrsh`**: the ARM7 turns a misaligned `ldrsh` into a sign-extended byte
  load; `(s16)LD16(...)` differs there (the oracle caught this).
- Flags: `SET_FLAGS_NZ(x)`, `SET_FLAGS_CMP(a, b)`, `SET_FLAGS_ADD(a, b)`,
  `cpu.c`, `cpu.v`. Reproduce the flags the original leaves whenever the
  lifted contract lists them as outputs (callers may branch on them).
- Calls into guest code: `GUEST_CALL(fn)`, `GUEST_CALL_ADDR(addr)` (through a
  table); return with `RETURN_TO_CALLER()`.
- **Register mirroring rule.** Until a whole call tree is readable, a
  readable function calls lifted functions whose contracts list (often
  conservatively, because of jump tables) many registers as inputs. Make the
  same calls in the same order, set exactly the registers each callee's
  contract lists as inputs the way the original sets them, touch nothing else
  in between, and restore what the original restores. Mark such lines "as the
  original". Contracts are in the lifted header comments.
- Struct offsets the generated header does not export: local `enum`s taken
  from `bn6f/include/structs/*.inc` (not `#ifndef` fallbacks).
- A hand-written function may promise less than its lifted contract only with
  a justification; record the promise in `cport/host/difftest.cfg`
  (`compare=rA,...,nz,c,v`).

### IWRAM, boot, speed

- IWRAM: port code and bss from 0x03005700; one shared host stack (top
  0x03007970); IRQ-mode area to 0x030079D0; guest stack 0x030079D4-0x03007E00.
  About 1.4 KB free. The guest-visible game RAM below 0x03005700 is the
  original's.
- Boot (`boot.c`) follows the original's own store instructions for WAITCNT
  and the IRQ vector (`orig_store`), so firmware patches to them apply.
- Speed (mGBA, real BIOS): 97-99% of the original on fast carts
  (WAITCNT 0x45B4); 64-71% with the Supercard's 4/2 timing (the original keeps
  99.8% there). In mGBA, 4/1 timing would give 94-97%.

## 3. Verification

Use them in this order when converting; record every result, and **say so
when a check compared nothing** (all trials out of domain).

| Check | What it proves | How |
|---|---|---|
| Host difftest, random | Readable vs lifted original (`L_NAME`) on random inputs: memory, return target, SP, contract registers/flags | `make -C cport/host && (cd cport && BN6F_ROM=../bn6f/bn6f.gba build-host/difftest -n 2000 NAME)`. Random RAM makes functions that follow pointers through game structures **out of domain**; then this compares nothing. `difftest_gen.c` has structured inputs for some. Host builds publish every return target (`RETURN_NR` is `RETURN` under `DIFFTEST`); without that, a lifted function that returns with `RETURN_NR` leaves the callee's stale `cpu.ret` and a wrong return target goes unseen. |
| Host difftest, captured calls | Same, on the original's own calls | `tools/convert/capture.sh DIR tools/traces/ROUTE.trace G0 G1 40 NAME...` then `build-host/difftest -S DIR/NAME NAME`. The best per-function check for pointer-heavy code, but only for the paths the captured calls take: a snapshot mutator in `difftest_gen.c` (`-m N` varied copies per snapshot) reaches the others (e.g. `mut_npc_secondary` gives an NPC a running secondary script). |
| CPU oracle | The **original** (mGBA, real BIOS) vs the port from the same entry states built from real game memory | `ORACLE_TMP=/tmp python3 tools/oracle.py --base SNAPDIR --states 24 --out OUT NAME...` (base: any `capture.sh` output). Holds hand-written functions to their `difftest.cfg` contract; gives the host the I/O mGBA reads back after reset. Known artifact classes: `tools/oracle_triage.md`. `--lifted` runs the lifted originals of hand-written functions instead: if a difference shows there too, the conversion did not introduce it (it comes from a callee). |
| Lockstep | The whole game replayed by the game's frame counter against the original: palette, VRAM, OAM identical at every checkpoint, save memory at the end | `tools/lockstep.sh ROM tools/traces/ROUTE.trace N` for every route below. Accepts one-game-frame VBlank timing skew; stops runs whose game stops counting frames. **Does not compare sound state or EWRAM**, and cannot see a wrong return target on functions only called directly. |
| Battle sweep | 116 battle records via pokes | `JOBS=6 tools/battlesweep.sh ROM` (run when battle code changes) |
| Other sweeps | 108 maps, every chip, random play | `tools/mapsweep.py` + `tools/tracesweep.sh`, `tools/chipsweep.py`, `tools/fuzzsweep.sh` |
| Coverage | Which lifted functions the original executed on the routes/sweeps | `python3 tools/coverage.py tools/traces/coverage/*.prof` (40.6% of lifted functions, 49.9% of code bytes when written) |

Recorded routes (`tools/traces/`, frame counts for lockstep):

| Route | N | | Route | N |
|---|---|---|---|---|
| battles1 | 20111 | | story2 | 33546 |
| battle | 20000 | | story3 | 17576 |
| menus2 | 23042 | | title_mash_a | 8000 |
| story1 | 9700 | | wander | 8000 |

New recordings: `tools/record.sh NAME START.sav` (the user plays; inputs and a
frame counter are shown on screen; start/end saves are stored), checked with
`tools/check_recording.sh NAME`, logged in `tools/traces/SESSIONS.md`.

Other tools: `tools/speed.sh` (speed vs the original), `tools/profile.sh`
(cycle profile per function), `tools/stackuse.sh`, `build-tools/mgba_run`
(headless runner; `--bios-intro` boots through the BIOS logo as hardware does,
`--call-snaps` runs captured calls, `--dump-at-game`, `--stall-frames`).

## 4. Converting functions

### By Claude (subsystems)

1. Pick a subsystem (callers and callees together) from functions the routes
   execute (`tools/coverage.py`; profiles in `tools/traces/coverage/`).
2. Read the assembly and the callees' contracts; write `cport/src/game/X.c`.
3. `tools/build_lifted.sh`; host difftest (random and captured calls); CPU
   oracle; lockstep on all routes (battle sweep for battle code).
4. Commit; then section 6 (database entries, renames, comments).

### By outside models (leaf functions)

1. Pick small functions the routes execute; capture 40 calls each on a route
   that runs them (`tools/convert/capture.sh`).
2. Run one farm per provider as separate background jobs:
   `python3 tools/convert/farm.py --snaps DIR --models PROVIDER/MODEL --rounds 3 --jobs N FUNC...`.
   It rebuilds the difftest first (a stale one once accepted candidates that
   returned to the wrong place), resumes (skips pairs already passed or out
   of rounds), and writes passing versions to `tools/convert/accepted/`.
3. Review: pick the clearest version per function; check `#ifndef` offset
   fallbacks (use enums), `ldrsh` loads (`LDS16`), return targets, flags.
   Specifically: any `LR = ...` before a call (`GUEST_CALL` keeps LR, so the
   later `RETURN_TO_CALLER()` returns there); table dispatch must enter the
   handler with the registers the original sets (`r0`/`r1`...); a second
   entry (`loc_...` reached by a sibling) must take the sibling's frame;
   and which paths the captured calls actually ran (add a mutator for the
   rest). Integrate, verify as above, then section 6.
4. Cost: `tools/convert/quota.py OUT.json` before and after a run;
   `tools/convert/scorecard.py [FUNC...]` for converted/requests/time/tokens.

Providers (config in `~/.pi/agent`, client `tools/llm.py`, which logs tokens
to `tools/convert/usage.jsonl`): Hyper (charm.land; no pacer; resets daily),
MiniMax, Z.ai, OpenCode Go (through the user's pacers; reset weekly).
Each costs about $20/month, so a function's cost is the share of a reset
period's quota it uses (weekly about $4.60, Hyper about $0.66 per day);
unused quota expires. OpenRouter is prepaid and nearly empty.

Results when written (`scorecard.py`):

| Model | Converted | Requests | Median s | Notes |
|---|---|---|---|---|
| hyper/glm-5.3-flash | 27/34 | 79 | 75 | about $0.002 per converted function (8 of 250 daily units for 13) |
| zai/glm-5.3 | 8/10 | 26 | 184 | Z.ai quota: about 200 credits per batch of ~20 calls |
| minimax/MiniMax-M3 | 8/19 | 84 | 10 | fastest |
| opencode/deepseek-v4-pro, qwen3.7-plus, kimi-k3 | 6/6, 6/6, 6/7 | 11-15 | 280-380 | pilot only |
| zai/glm-5.3-flash | 5/7 | 21 | 458 | slow through the pacer |
| minimax/MiniMax-M2.7-highspeed | 4/11 | 41 | 262 | |

No model converted `CopyByEightWords`, `battle_findPlayer`, `sub_800C01C`,
`sub_800C5E0` or `GetBattleNaviStatsAddr`.

Claude's recommendation to the user (not yet acted on): let deterministic
tooling do the mechanical first pass for every function (control-flow
structuring, struct field names from `bn6f/include/structs`, named
constants), resolve static jump tables in the lifter so register contracts
narrow (most of the "as the original" noise disappears), run cheap models for
volume, escalate failures to stronger models, and keep Claude for subsystem
design, hard cases and review.

## 5. Readable code so far

46 functions in `cport/src/game/` when written: sound (m4a mixer, music
player, init), sprites (OAM building and queueing, projection, graphics
residency), palette effects, frame pacing, event flags (all 16), battle
helpers (6, model-written), overworld object layers and interaction areas
(7, model-written), the NPC update loop and sprite pass, and the NPC state
machine core. Each is listed with its evidence in `symbols/SYMBOLS.md`.
Batch 3 of the model farm added 32 more (84 in all): NPC script plumbing
(`npc_commands.c`), the object sprite API (`sprite_api.c`), and leaves
grouped by bn6f file (`leaves_misc.c`, to be moved when their subsystems
are converted).

## 6. The symbol database and the disassembly

- `symbols/db/NAME.yaml`, one per symbol (format: `symbols/db/README.md`):
  meaning, interface, status (confirmed / probable / hypothesis), evidence
  with exact reproduce commands and actual results, changes, open questions.
- `tools/symdb.py check` validates against `bn6f/bn6f.elf`;
  `tools/symdb.py render` writes `symbols/SYMBOLS.md` and `symbols/symdb.sqlite`.
- Renames: `tools/rename_symbol.py OLD NEW [...]` (needs an entry; edits bn6f
  sources like bn6f's `replace.sh`, plus the port's hand-written code,
  `difftest.cfg`, `difftest_gen.c`, `iwram.txt`; rebuilds bn6f and undoes
  everything unless the ROM is identical; commits bn6f; regenerates the
  lifted C). Rename only confirmed/probable meanings; keep the address suffix
  when the game-level meaning is partly inferred; hypotheses get comments only.
- Comments: `tools/symdoc.py --commit` writes `// symdb:` blocks at each
  definition in bn6f from the database.
- After renames: `tools/import_symbols.sh` refreshes the PC port's symbol
  overlay; re-verify the port (renames change its layout, not behaviour).
- Build bn6f by hand: `make -C bn6f AS=$PWD/toolchain/bin/arm-none-eabi-as LD=$PWD/toolchain/bin/arm-none-eabi-ld OBJCOPY=$PWD/toolchain/bin/arm-none-eabi-objcopy OBJDUMP=$PWD/toolchain/bin/arm-none-eabi-objdump`
  (toolchain from `tools/build_toolchain.sh`; bn6f's default path
  `bn6f/tools/binutils` is not built here). Prints `bn6f.gba: OK` when identical.

## 7. Lessons and pitfalls

- A background `&` on a chain that sets a shell variable backgrounds the
  assignment too: set variables on their own line first.
- `pgrep -f`/`pkill -f` can match the calling shell's own command line; use
  `[x]yz` patterns or wait on PIDs.
- The system `awk` is mawk (no `strtonum`); use Python for hex parsing.
- Address-range filters: ROM addresses (0x08...) are also `>= 0x03000000`;
  test IWRAM as `0x03000000 <= a < 0x04000000`.
- libmgba 0.10's `savedataClone` returns garbage; read SRAM through the bus.
- mgba_run skips the BIOS intro by default; hardware does not
  (`--bios-intro`).
- The CPU oracle's artifact classes (BIOS leftovers, live sound/serial I/O,
  documented narrower contracts, misaligned random pointers, IWRAM data read
  from its ROM copy): `tools/oracle_triage.md`. Its one real finding was
  `sprite_queueOamEntry` not leaving the original's flags (fixed).
- Lockstep can be fooled by a run where both games crash early (now
  reported) and cannot see wrong return targets on direct calls; the host
  difftest can.
- The model farm's style filter must ignore comments; a stale difftest build
  accepted bad candidates (fixed: the farm rebuilds first).
- The farm also accepted two drafts that returned to the wrong place
  (`sprite_update`, `npc_runSecondaryScriptMaybe_809ebf8`): the lifted
  originals returned with `RETURN_NR`, which did not publish `cpu.ret` (fixed
  for host builds), and the second one's bad path never ran in the captured
  calls (fixed with a mutator).
- A narrow `compare=` contract in `difftest.cfg` must hold for every caller.
  `palette_addOrSubtractColor` promised only r5/r7 because one caller needs
  no more, but two other callers exist; it now leaves every register as the
  original. Prefer exact registers over a narrow contract when cheap.

## 8. Flash cart (paused)

The user plays on a "flashgbacart plus+" (Supercard SD clone) running
SuperFW. SuperFW recognises the original's game code (BR6E), patches the ROM
from its database (WAITCNT store at 0x108 NOPed, 0x03007FF4 written at 0x1F8)
and installs its in-game menu; the standard port ROM then shows a white
screen on that cart. What works: `tools/mkrom_supercard.py IN OUT [WAITCNT]`
(game code B6CP, WAITCNT literal 0x4000), loaded with **"No patching"**. A ROM
with SuperFW's current-source patch path baked in also works
(`tools/superfw_sim.py --as BR6E`), so the user's SuperFW build likely differs
from current source (version unknown). Hardware probes: `tools/probe/`
(`mkprobe.py`: reads/calls above 8 MB; `mkromcheck.py`: whole-ROM integrity
grid, then boots the port; `mktiming.py`: which WAITCNT settings the cart
handles; results never reported, work paused).

## 9. The self-scheduled loop

During a session the user asked for a recurring prompt (session-only cron,
every 20 minutes while idle) that tells Claude to keep working. The user had
it cancelled on 2026-10-08 ahead of an all-hands refactor; recreate it only
when asked. Its text:

> Recomp keep-working check (self-scheduled loop; the user asked for it). If you are already mid-task, finish it. Otherwise pick the highest-value item with no blocker and do it end to end: implement, verify (host difftest, CPU oracle, lockstep on all tools/traces routes, battle sweep when battle code changes), commit locally (never push), then give the user a 2-4 line summary.
> Focus: readable C (the project goal). Flash-cart/Supercard work is paused by the user. Do not keep polishing the CPU-oracle harness unless a run finds something that could change game behaviour.
> Rule (user, mandatory): every discovery about a symbol goes back into the bn6f disassembly and the symbol database (...).
> Blockers: anything that needs the user (recordings, decisions). If the only remaining work is blocked, say so in one line and stop until the next check.
> Backlog: 1. readable conversion of whole subsystems; 2. model farms for small leaf functions, one per provider, tracking cost per converted function; 3. coverage growth when it lets lockstep check more converted code; 4. keep notes and SESSIONS.md current.
> Constraints: real BIOS, local git only, bn6f fixes on recomp-fixes with the ROM byte-identical, never print API keys, don't change the user's pacer settings without asking.

## State when written (2026-10-08, evening)

- 84 readable functions; 98 symbol-database entries; renames in bn6f
  (22 + 5 + 6, all ROM-identical, latest bn6f d722c690); every route and the
  battle sweep identical.
- Farm batch 3 (Hyper glm-5.3-flash): 34 of 78 passed; 32 integrated, the
  rest of the accepted pool re-tested with the stricter difftest (all pass).
- The self-scheduled loop is cancelled (user, ahead of an all-hands
  refactor).
- Open: the remaining NPC script actions (hop, leap, waits; several not run
  by any route), the user's answer on the lifter improvements, and moving
  `leaves_misc.c` functions into their subsystems.
