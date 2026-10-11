# How this project is run

The decisions behind the project, the conventions for readable code, what
each check proves, and the lessons learned. **The steps themselves live in
typed workflows** (`tools/wf`, run as `./wf ...` from the repo root, which is
`python -m wf` with tools/wf's venv): their pages and diagrams are in [docs/workflows](workflows/README.md),
generated from the code, and every run can be followed in the Burr UI
(`tools/wf/.venv/bin/burr --no-open --no-copy-demo_data --host 0.0.0.0`,
port 7241). `./wf status` says where every run is.

| Workflow | Does |
|---|---|
| [verify](workflows/verify.md) | lift and build, difftest (random and captured calls), CPU oracle, lockstep on every route, battle sweep (§3) |
| [capture](workflows/capture.md) | the original's own calls of functions, on as few routes as possible, in parallel |
| [farm](workflows/farm.md), [farm-batch](workflows/farm-batch.md) | cheap models convert small functions one at a time and draft their symbol entries (§4) |
| [drafts](workflows/drafts.md) | symbol entries for accepted farm candidates that have none |
| [convert](workflows/convert.md) | strong models write a subsystem's file (or farm candidates go in as a group); quick test, full verify, review, commit (§4) |
| [record](workflows/record.md) | symbols/db, renames and comments into bn6f, re-verify, commit (§6) |

What running them taught about cost and speed: [EFFICIENCY.md](workflows/EFFICIENCY.md).

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
| BIOS | Use the real GBA BIOS (`roms/gba_bios.bin`, `GBA_BIOS` in `tools/paths.env`) for both ports and all tests. |
| Disassembly | Fix bn6f defects on its local branch `recomp-fixes`; the built ROM must stay byte-identical (bn6f's `make` checks `bn6f.sha1`). |
| Evidence | Wants a reliable test method, not "does the title screen boot" (the verify workflow). |
| Tools | Results over originality: reuse existing tools before writing new ones. |
| Recordings | The user records gameplay sessions; ask for recordings rather than scripting inputs blindly (`tools/traces/SESSIONS.md`). |
| Symbols | **Every discovery about a symbol goes back into the disassembly (names, comments) and into the symbol database with evidence and reproduction steps** (section 6; the record workflow). |
| Coverage | Prefer converting functions the recorded routes execute, so lockstep checks them; mark exceptions "oracle only". |
| Conversion split | Outside models write all the code: strong models (deepseek-v4-pro, then kimi-k3, then glm-5.3, 3 attempts each) whole subsystems, cheap models (Hyper glm-5.3-flash) small functions in volume. Claude orchestrates: picks the work, reviews (approve, request changes, reject) and decides what to do when every model gives up; it does not edit the code. |
| C semantics | Models move each function out of its registers-as-arguments origin: typed C functions with real parameters and results, `Next NAME(void)` only as a thin register adapter. Where C cannot work they say so in a `/* register form: why */` comment, which review and symbols/db carry forward (future work: `docs/C_CALLING_CONVENTION.md`). |
| Workflows | The process lives in small typed Burr graphs (pydantic models, NewTypes, readable step names), observable in the Burr UI on the LAN; not in prose. |
| Model cost | Cost per converted function is the most important factor; test all models over time. glm-5.3-flash gave the user good results. |
| Pacers | The local API pacers (`pi-pacer@{minimax,zai,opencode}.service`) are the user's setup. Change their settings when the work needs it (the user, 2026-10-09: "whatever works, I don't care"); keep a backup of `api_pacer.py` and note the change. |
| Flash cart | Paused ("done with flash cart stuff for now"); speed is to be looked at holistically later (section 8). |
| Secrets | Never print API keys. |

## 2. The C port

### Build

The verify workflow builds in its own directories (`cport/build-wf`,
`cport/build-wf-host`). By hand:

```sh
tools/build_lifted.sh               # lift bn6f into cport/src/lifted (+ headers) and build the ROM
make -C cport -j$(nproc)            # rebuild only: cport/build/bn6f_lifted.gba (13.5 MB, entry 0x08800000)
make -C cport/host -j$(nproc)       # host difftest build: cport/build-host/difftest
```

An install rebuilds little: `lifted.h` declares only the plain names, so it
stays the same when a function is installed (the lifted originals' `L_NAME`
declarations are in `lifted_originals.h`, which only the host difftest reads);
large modules are split into parts at fixed addresses (`tools/lifter/splits.txt`),
so an install changes only the part that held the function; and both Makefiles
use real header dependencies (`-MMD`). An install recompiles 3 files for the ROM
(the LTO link then takes most of the time) and 4 for the host difftest, instead of
all 149. ccache holds 20 GB (`ccache -M 20G`); workflow builds log how many files
they compiled and how many ccache answered (the build log, and
`build-wf-state/ccache.tsv`). When the lifter warns that a part has grown too big
(`difftest_lifted` grows with every install), add an address to `splits.txt`.

Variants: `make BUILD=dir`, `OPT=` (hand-written/runtime code, default `-Os`),
`LIFTED_OPT=` (lifted code, default `-O2`; `-Os` was measured slower at both
cart timings because executed instructions dominate), `-DBOOT_DIAG` (coloured
boot stages for hardware debugging), `LTO=0`.

The lifter's analysis switches are flags (`python3 tools/lifter/lift.py --help`;
the old `LIFT_*` environment variables still work). One default rests on an
unproven assumption: no call through a pointer unwinds past its caller (true on
every recorded route; `LIFT_TRACE` builds log any return-address mismatch).
`--indirect-calls-may-unwind` drops it.

**Names in lifted code.** The lifter writes each constant as bn6f's source
writes it (`tools/lifter/srcnames.py`, review R1-A22), checked against the
instruction's value: a literal-pool load as its `.word` (`A_eOWPlayerObjectEnd`,
`(A_sub_80F6FEC + 1)`), an immediate as its operand (`oObjectSprite_Unk_06`,
`(OBJECT_FLAG_ACTIVE | OBJECT_FLAG_STOP_SPRITE_UPDATE)`), and a bare offset whose
comment names the field as that field (`(A_byte_200A00B - 0x200A008)`). A number
the source writes stays a number. Labels and address-valued constants (I/O
registers) are `A_NAME`, other constants keep their names; `bn6f_syms.h` defines
the ones lifted code uses. Readable code uses the name its lifted original uses.
`bn6f_equates.h` (hand-written code only) also keeps the names lifts before
2026-10-11 gave these addresses (the first label at each), so code and farm
candidates written against them still compile.

**Golden test.** `python3 tools/lifter/golden.py check` lifts into a temporary
directory and compares every generated file with `tools/lifter/golden/manifest.tsv`
(hashes; the inputs are pinned: the bn6f build, and the readable functions listed
in `golden/overrides.txt`, so installs don't move it). `golden.py diff` shows what
a lifter change changes, against the committed lifter on the same inputs;
`golden.py update` records an intended change, and re-records after a bn6f rename
or comments (check exits 2 then).

### Layout

| Path | What |
|---|---|
| `tools/lifter/lift.py` | The lifter: bn6f (assembly + ELF) to C. Each function's header comment gives its register contract (`inputs`, `outputs`, `clobbers`). |
| `cport/src/lifted/` | Generated lifted C (not committed). |
| `cport/src/game/*.c` | Readable C. A `Next NAME(void)` here replaces the lifted function; the lifted original stays as `L_NAME` for testing. A subsystem converted as a whole has its own file (`npc_update.c`); a function converted on its own (the model farm) is in the file of the bn6f source that defines it (`object.c` for `bn6f/asm/object.s`; big sources in parts named by their first address, `asm00_2_8012BA2.c`), in address order. `tools/layout.py` lays the files out (the split addresses: `tools/layout.txt`), the convert workflow puts a farm group's functions there (its `--file` is only an inbox), and the commit gate checks that laying out the tree gives back the same tree. |
| `cport/include/game.h`, `cpu.h` | Macros readable code uses (below). `bn6f_syms.h`, `bn6f_equates.h`, `lifted.h` and (for the difftest) `lifted_originals.h` and `difftest_pairs.h` are generated. |
| `cport/src/gba/` | Boot (`boot.c`), IRQ entry, debug (`rt_fatal` turns the screen magenta). |
| `cport/src/runtime/swi.c` | BIOS services: the GBA build calls the **real BIOS**; the host test build uses C versions that follow the BIOS routines register for register. |
| `tools/lifter/iwram.txt` | Lifted functions placed in IWRAM. |
| `tools/lifter/splits.txt` | Where large lifted modules are split into parts (fixed addresses). |
| `tools/lifter/golden/` | The golden test's reference (`golden.py`). |

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
  original". Contracts are in the lifted header comments: the lifter's
  analysis refined by the call edges the routes showed (`abi_export.REFINED`;
  `lift.py --static-contracts` writes the lifter's own).
  Models are shown them, the difftest compares them (`difftest_pairs.h`), stamps
  record them, and `tools/abi/leftovers.py` checks readable callers against them.
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

The [verify](workflows/verify.md) workflow runs the first five in this
order, each only after the earlier ones pass, and **says so when a check
compared nothing** (all trials out of domain: verdict "passed, but some
functions were never compared"). What each proves, and what it cannot see:

| Check | What it proves | Limits |
|---|---|---|
| Host difftest, random | Readable vs lifted original (`L_NAME`) on random inputs: memory, return target, SP, contract registers/flags (every r0-r12 the contract does not leave free and the flags callers read, or the function's `compare=` line; before 2026-10-10 only functions with a `compare=` line had registers compared) | Random RAM makes functions that follow pointers through game structures **out of domain** (the original faults, or returns elsewhere though the lifter proves it never unwinds: the input overwrote its own frame): then it compares nothing (`difftest_gen.c` has structured inputs for some: a list with a terminator, a table index the game uses). A random input on which the original made a misaligned halfword or word load and the two differ is out of domain too, as on the CPU oracle (the ARM7 rotates the value; typed C reads its type); captured calls stay compared. Host builds publish every return target (`RETURN_NR` is `RETURN` under `DIFFTEST`), or a wrong return target would go unseen. |
| Host difftest, captured calls | Same, on the original's own calls ([capture](workflows/capture.md)) | The best per-function check for pointer-heavy code, but only for the paths the captured calls take; a snapshot mutator in `difftest_gen.c` (`-m N`) reaches the others (e.g. `mut_npc_secondary`). |
| CPU oracle | The **original** (mGBA, real BIOS) vs the port from the same entry states built from real game memory | Holds hand-written functions to their `difftest.cfg` contract. Known artifact classes: `tools/oracle_triage.md` (differences give the verdict "needs triage"). `--lifted` runs the lifted originals instead: a difference there too comes from a callee. |
| Lockstep | The whole game replayed by the game's frame counter on every route (`wf/routes.py`): palette, VRAM, OAM identical at every checkpoint, save memory at the end | Accepts one-game-frame VBlank timing skew. **Does not compare sound state or EWRAM**, and cannot see a wrong return target on functions only called directly. |
| Battle sweep | 116 battle records via pokes | Run when battle code changes (`--battle-code`). |
| Other sweeps (not in a workflow yet) | 108 maps, every chip, random play | `tools/mapsweep.py` + `tools/tracesweep.sh`, `tools/chipsweep.py`, `tools/fuzzsweep.sh` |

By hand, for triage: `(cd cport && BN6F_ROM=../bn6f/bn6f.gba build-host/difftest -n 2000 NAME)`,
`build-host/difftest -S DIR/NAME NAME`, `tools/oracle.py --base SNAPDIR --states 24 --out OUT NAME...`,
`tools/lockstep.sh ROM tools/traces/ROUTE.trace N`, `JOBS=6 tools/battlesweep.sh ROM`.
Lockstep caches the original game's side of each replay (digests and end save, in
`build-wf-cache/lockstep`, keyed by the hashes of the original ROM, `mgba_run`, the
BIOS, the trace with its `.sav`/`.poke`, and the frame counts), so after the first
run only the tested ROM is replayed and verify runs all routes at once;
`LOCKSTEP_CACHE=off` replays both.

New recordings: `tools/record.sh NAME START.sav` (the user plays; inputs and a
frame counter are shown on screen; start/end saves are stored), checked with
`tools/check_recording.sh NAME`, logged in `tools/traces/SESSIONS.md`.

Other tools: `tools/speed.sh` (speed vs the original), `tools/profile.sh`
(cycle profile per function), `tools/stackuse.sh`, `build-tools/mgba_run`
(headless runner; `--bios-intro` boots through the BIOS logo as hardware does,
`--call-snaps` runs captured calls, `--dump-at-game`, `--stall-frames`).

## 4. Converting functions

- **Subsystems:** `wf convert --subsystem WHAT --file X.c FUNCTION...` (the
  [convert](workflows/convert.md) workflow). Claude picks the subsystem from
  functions the routes execute and answers the review pause; the review sheet
  carries the reviewer's checklist.
- **Small functions in volume:** `wf capture` then `wf farm --model
  hyper/glm-5.3-flash --snapshots DIR FUNCTION...` (one batch per provider, so
  review is not held up by the slowest); accepted candidates land in
  `tools/convert/accepted/` with drafted entries, and go into the tree as a
  group through `wf convert --farm-model MODEL --snapshots DIR`, which tests,
  verifies and stops for the same review.
- Cost: `tools/convert/quota.py OUT.json` before and after a run;
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
grouped by bn6f file. The farm's later groups (1,340 functions in
`leaves_farm_*.c` and `leaves_misc.c` by 2026-10-10) were regrouped by
bn6f source on 2026-10-10 (`tools/layout.py`, §2 Layout).

## 6. The symbol database and the disassembly

- `symbols/db/NAME.yaml`, one per symbol (format: `symbols/db/README.md`):
  meaning, interface, status (confirmed / probable / hypothesis), evidence
  with exact reproduce commands and actual results, changes, open questions.
- The [record](workflows/record.md) workflow does the rest: checks the entries
  (`tools/symdb.py check`), renames only confirmed/probable meanings
  (`tools/rename_symbol.py`: bn6f sources and the port's hand-written code,
  bn6f rebuilt and everything undone unless the ROM is identical), refreshes
  the PC port's symbols, writes `// symdb:` comments into bn6f
  (`tools/symdoc.py`; before the renames, so one bn6f build checks both),
  renders `symbols/SYMBOLS.md`, re-verifies the port after renames (only when
  the renamed port's ROM differs from the one built before them: renames
  normally leave it byte-identical), and commits. Keep the address suffix when the game-level meaning
  is partly inferred; hypotheses get comments only.
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
- A byte counter in mgba_run wrapped after 256 calls and captured again forever;
  a lifter "entry" made of NPC script bytes made a conversion impossible to
  pass; labels with a trailing comment hid 536 functions from the farm. Each
  is in [EFFICIENCY.md](workflows/EFFICIENCY.md) with its cost.
- A background job reports only when it ends: watch long ones for progress
  (and match the process, not the shell wrapper whose command line repeats
  the pattern).
- A narrow `compare=` contract in `difftest.cfg` must hold for every caller.
  `palette_addOrSubtractColor` promised only r5/r7 because one caller needs
  no more, but two other callers exist; it now leaves every register as the
  original. Prefer exact registers over a narrow contract when cheap.
- Passing every check is not enough for farm code. On 2026-10-11 review agents
  read 444 farm functions that had passed the quick test, full verify and
  lockstep against their bn6f assembly: about 1 in 7 differed on paths no
  captured call or route ran (a callee's input register left unset, a value
  not re-read after a call, an inverted condition, a mistyped table, a
  missing store). 56 installed ones were taken out (51d348b); candidates the
  review rejects go to `tools/convert/rejected/` with a `.review.txt` saying
  why. Review reads each function against its assembly (the review sheet's
  checklist), before the commit.

## 8. Flash cart (paused)

The user plays on a "flashgbacart plus+" (Supercard SD clone) running
SuperFW. SuperFW recognises the original's game code (BR6E), patches the ROM
from its database (WAITCNT store at 0x108 NOPed, 0x03007FF4 written at 0x1F8)
and installs its in-game menu; the standard port ROM then shows a white
screen on that cart. What works: `tools/flashcart/mkrom_supercard.py IN OUT [WAITCNT]`
(game code B6CP, WAITCNT literal 0x4000), loaded with **"No patching"**. A ROM
with SuperFW's current-source patch path baked in also works
(`tools/flashcart/superfw_sim.py --as BR6E`), so the user's SuperFW build likely differs
from current source (version unknown). Hardware probes: `tools/flashcart/probe/`
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
> Constraints: real BIOS, local git only, bn6f fixes on recomp-fixes with the ROM byte-identical, never print API keys. (Pacers: changed when the work needs it, since 2026-10-09.)

## State

`python -m wf status` and the Burr UI show every run; `symbols/SYMBOLS.md`
lists the readable functions with their evidence. Open: the NPC script
actions no route runs, the lifter improvements in §4's results, and moving
small functions from their bn6f-source files into subsystem files as whole
subsystems are converted.
