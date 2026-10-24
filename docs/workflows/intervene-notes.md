# intervene

Causal checks of function names. A name like `drawHpBar` is a claim: *this function
updates the HP digits' OAM entries each frame*. `wf investigate` asks a model what
the code looks like it does. `wf intervene` does not ask — it breaks the function on
purpose and watches what the machine does instead.

Nothing is renamed and nothing is installed. The output is a table plus one JSON per
run; the orchestrator reads the table and takes what holds up through `record`.

Runs are tracked in the Burr UI under project `recomp-intervene` (and
`recomp-intervene-pilot` for the pilot).

## Usage

```bash
python -m wf intervene --function NAME_OR_ADDR --intervention skip|null|return:K \
                       [--route R | --route auto] [--frames N]
python -m wf intervene-pilot [--route R | auto] [--frames N] [--random N] \
                             [--only NAME]... [--jobs J]
```

`--route auto` (the default) picks the shortest recorded route that *enters* the
function (facts first, execution map to confirm — §2). `--intervention` is one
validated string (`skip`, `null`, `return:K` with K in 0..255), not 258 argparse
choices. Interventions are ROM patching — no emulator C changes, no savestate
editing, nothing under `bn6f/` or `symbols/db` touched.

All outputs (`build/intervene/`, `build-wf-runs/`) go to the **main checkout**
(`wf.repo.REPO`, override with `WF_REPO`): the workflow code can run from a worktree
while the ROMs, the recordings and the cached states are shared.

## What a run does

### 1. Resolve the function

`find_function` maps a name or address onto bn6f.elf and **refuses** anything it
cannot patch honestly:

| refusal | why |
|---|---|
| ARM mode (`mode == "a"`) | the patch encodings are THUMB (`bx lr`, `movs r0,#k`) |
| RAM-resident (`0x02…`/`0x03…`) | there is no ROM byte to patch; `patch_rom` has no offset |
| shorter than 4 bytes | a 4-byte `return:K` patch would not fit |

A name with duplicates (ELF aliases, or a static name reused per translation unit)
resolves to the **most-called** address, since that is the one the game is actually
dispatching to.

### 2. Find the first frame it executes

`first_game_frame` searches the **whole route** — not a 1200-frame window — in two
steps.

**Which segment.** `ensure_route_states` caches an mGBA state every
`state_stride(route)` game frames (32, 64, 256, 2048 … whatever keeps a route down to
~400 states) in ONE whole-route pass. Those states cut the route into segments, and
`ensure_route_exec_map` profiles each segment once (`mgba_run --profile-game`, one
`profile.txt` per segment) to record *which addresses executed there*. One pass
answers that for every function on the route at once: 34 s of emulated frames for
`wander`, 85 s for `battles1`, ~7 min (4 threads) for `beastsp`, cached in
`build/intervene/exec-maps/ROUTE.json` and shared by every function, every
intervention and every future run on that route. `first_execution_segment` then
returns the first segment that runs the function, or an authoritative "the route
never enters it".

That map is also what confirms the route: the facts layer
(`build/facts/by_address/0xADDR.json`, absolute path, `WF_FACTS_DIR` overridable for a
worktree) lists which routes reach a function and `routes_executing` takes them
shortest first, but the facts' `executed` flag is loose — it comes from a coverage
profile, which credits a function's whole range, so a symbol whose tail is reached by
a jump into the middle reads as executed although nothing ever branches to its entry
(`sound_dispatchTask_8000630`: 0 entries at 0x08000630 in a whole-route
`--count-entry` on `wander`). `confirm_route` walks the facts' order and keeps the
first route whose execution map agrees.

**Which frame.** Inside the segment, `first_game_frame` bisects the *cumulative*
`--count-entry` count from that segment's own cached state down to the exact frame.
The count's start frame stays pinned to the segment start: only grid frames have a
state, and a probe anchored anywhere else silently reports "no hit".

If the execution map cannot be built (the pass fails), `first_game_frame` falls back
to galloping over segment ranges 1, 2, 4, … chunks wide and binary-searching the hit
range — ~`2 × first frame` of emulated frames per function instead of one shared pass.

**A `None` here means "nowhere on this route", never "never".** The search covers the
whole route; `frames_window` is the upper bound if a caller wants to clip it.

Two things the grid has to get right, both of which silently cost minutes when they
do not:

- `mgba_run --save-state-every N PREFIX` writes `PREFIX_%07u.state` at every multiple
  of N. `state_path_at` therefore uses seven digits, and the grid is frame 1 plus
  those multiples. When the names/offsets did not line up (6-digit names, and a grid
  asked for at 1, 33, 65 … against states written at 32, 64, 96 …), nothing was reused:
  `ensure_route_states` reported one cached state and every probe re-ran the route from
  the beginning. A one-function pilot took minutes; it now takes seconds.
- The chunk-level predicate "hit in segment i" is **not** monotone (a function can
  run in one segment and never again), so it is a gallop, not a plain binary search.

### 3. Cache an emulator state just before it

The state is taken at the cached grid point at or before `first_frame -
lead_in_frames` (default 8, clamped to ≥ 1 — `mgba_run --until-game-frame 0` exits
immediately). Snapping down to the grid is not just cheaper than saving a state at
the exact frame (that means replaying the route from its first frame); it also gives
*more* head-room, never less. Both the baseline and the patched ROM then load *the
same machine state* and replay *the same trace*, so the only variable is the patch.
States live under `build/intervene/states/ROUTE/`, shared by every function that
starts on that frame.

### 4. Patch and observe

The patched ROM is a full copy written into a `tempfile.mkdtemp` under
`build/intervene/patched-roms/`; the temp dir is `shutil.rmtree`'d on every exit
path so the patched ROM never out-lives the run (945 MB off disk by the v1 pilot).
The original is never touched. Both ROMs run in **one** `mgba_run` invocation each,
carrying every observation flag at once so the two logs are frame-aligned by
construction:

| flag | feeds |
|---|---|
| `--digest-every 1` | the memory anchors (`oam`, `palette`, `vram_bg`, `vram_obj`), one hash per frame |
| `--watch 0x04000000 0xe0` | the register-set anchors (`display_io`, `dma_control`, `sound_io`), raw bytes per frame |
| `--shots … --every k` | the `screen` anchor, PPM frames sampled every k-th game frame |

The diff is then per-anchor and per-frame: memory anchors compare hashes (a VRAM
block hash so a 64 KB region is one token per frame), register anchors compare raw
bytes, and `screen` compares raw pixel bytes.

### 5. Validate the start point

**Every verdict is gated on a `null` run.** `null` rewrites the function's entry with
*its own bytes* (whichever prefix the kind-aware skip patch consumes), producing a
byte-identical ROM; if that run shows any anchor difference, the route/window is
nondeterministic and no `skip`/`return:K` verdict from this window means anything.
The null row is written next to the real row in `report.tsv` so a reader can see
the gate, not take it on faith.

## Interventions

The patch's *return* encoding is chosen per function by `return_kind_for`: a
classification that decodes the function's own instructions and emits one of
"pop_pc" / "mov_pc_lr" / "mov_pc" / "bx_lr" / "bx_other" / "unknown". Per the brief:

| function returns with | `skip` patch | reasoning |
|---|---|---|
| `pop {…pc}` (the SDK shape) | `mov pc, lr` (`f7 46`) | mode-preserving: ARMv4T `pop {pc}` ignores bit 0 |
| `mov pc, X` (high reg) | `mov pc, lr` | mode-preserving: ARMv4T `mov pc, X` ignores bit 0 |
| `bx <other reg>` | `mov pc, lr` | the function returns to a *computed* address; `mov pc, lr` returns to the caller |
| unknown (default) | `mov pc, lr` | the brief's safe default |
| `bx lr` (and only `bx lr`) | `bx lr` (`70 47`) | the function's own kind; callers used a normal Thumb call so LR has bit 0 set |

`return:K` mirrors `skip`: it's `movs r0,#K` followed by the same 2-byte return
(`mov pc, lr` or `bx lr`), 4 bytes total. `null` rewrites the function's own bytes
back over the prefix the kind-aware skip patch consumes — same length, byte
identical to baseline.

`skip` alone cannot tell you whether a function matters because of its writes or
because of what its caller does with the result. `return:K` with K=0 keeps the
control flow intact while zeroing the result, which is how you separate the two.

### How the return kind is detected (instructions, not bytes)

The kind has to come from the function's *instructions*. The first implementation
scanned the function's trailing bytes for the byte patterns of the four exits, and
that is wrong in a way that matters: a literal pool inside the function looks
exactly like an epilogue.

`cgbSoundInit_814EB84` (0x0814eb84, 284 bytes) ends with the **data** word
`0x4770df2a` at 0x0814ec9c — its low half is `bx lr` (0x4770). A byte-pattern scan
reads that as the function's epilogue and hands the function the `bx lr` patch,
i.e. the one encoding that crashes when the caller's LR has bit 0 clear.

So the detection decodes the function instead:

1. the ELF's `$t` / `$a` / `$d` **mapping symbols** split the function's address
   range into THUMB code, ARM code and data (`ElfInfo.mapping`), and only the `$t`
   ranges are decoded — that is what keeps `cgbSoundInit`'s literal pool out;
2. the repo's own **ARMv4T decoder** (`tools/lifter/isa.py`, the one the lifter
   uses) turns those bytes into instructions, 32-bit `bl`s included;
3. the exits are read off the decoded instructions: `pop {…,pc}` → `pop_pc`,
   `mov pc, <reg>` → `mov_pc` / `mov_pc_lr`, `bx <reg>` → `bx_lr` / `bx_other`.

A function with several exits is only allowed `bx lr` when **every** exit is
`bx lr`: `mov pc, lr` is correct whether the caller left LR's bit 0 set or clear,
`bx lr` is not, so one `pop {…,pc}` anywhere in the function decides the whole
patch. A function whose range also holds ARM code (`umul3232H32` jumps into an ARM
`umull` and returns from there) is "unknown" → `mov pc, lr`.

Checked against an `arm-none-eabi-objdump` ground truth over **every** THUMB
function in bn6f.elf (13,497 of them, ROM-resident, ≥ 4 bytes):

| detection | agreement with objdump |
|---|---|
| same patch bytes | 13,496 / 13,497 |
| same label | 13,479 / 13,497 |

The one patch-byte difference is `umul3232H32`, the mixed THUMB/ARM function above:
the tool picks the safe `mov pc, lr` where objdump's last instruction says `bx lr`.
The 18 label-only differences are functions with several exits where the
mode-preserving one wins (the tool's label is the conservative one).

The ELF's function *sizes* can also overlap the next symbol (35 THUMB functions
have this: `IsScreenFadeActive`'s 18 bytes cover the `isScreenFadeActive_80062FC`
variant that starts 4 bytes into it). The scan therefore sees the neighbour's
epilogue too — harmless, because the mode-preserving return wins whenever one is
present, and "unknown" maps to the same `mov pc, lr`.

### Why `bx lr` is not always safe (problem 1)

A naive `skip` writes `bx lr` at the function's entry and lets the patched
function return immediately. That works for almost every ROM function on almost
every route — the caller's LR has bit 0 set (Thumb call convention), so `bx lr`
stays Thumb — but it crashes hard on a specific shape that the game uses a lot:
the IWRAM trampoline.

The IWRAM sound trampoline (`bn6f/asm/asm38.s:239 sound_vcountCallback`) is the
path `call_m4aSoundMain` is reached from on battles1:

```
sound_vcountCallback:                       @ 0x03005d78, IWRAM
        push    {lr}                       @ (A) pushes the *real* return
        ldr     r0, off_3005D94            @ r0 = call_m4aSoundVSync+1
        mov     lr, pc                     @ LR = 0x03005d82, bit 0 clear
        bx      r0                         @ call
        pop     {r0}                       @ (B) restores that real return
        mov     lr, r0                     @ LR = the real return, bit 0 set
        ldr     r0, off_3005D90            @ r0 = call_m4aSoundMain+1
        mov     lr, pc                     @ LR = 0x03005d8c, bit 0 clear
        bx      r0                         @ call
        pop     {r0}                       @ (C)
        bx      r0                         @ back to interrupt dispatcher
```

`mov lr, pc` writes the next instruction's *even* address — bit 0 clear, ARM-flavoured.
The callee is entered with LR = an even address. Its own original epilogue is
`push {lr}; bl m4aSoundMain; pop {pc}`: the `push {lr}` records that even LR; the
`pop {pc}` returns to it via THUMB `pop {pc}`, which on ARMv4T ignores bit 0 and
stays Thumb — so the trampoline's `pop {r0}; bx r0` at (C) decodes correctly.

The original v1 patch replaced the callee's first two bytes with `bx lr` (0x4770,
bytes `70 47`). That works ONLY when LR has bit 0 set; with LR bit 0 clear, `bx lr`
switches the CPU to ARM state. The CPU lands in ARM state at 0x03005d8c, decodes
the THUMB `pop {r0}; bx r0` *(at (C))* as ARM bytes (2.4 M illegal opcodes per
second of game time), the frame counter stops advancing inside mgba_run's frame
window, and the pilot reports `hung` — for **six unrelated functions**: all of
`call_m4aSoundMain`, `SetScreenFade`, `IsScreenFadeActive`, `LoadGFXAnims`,
`LoadGFXAnim`, and `startScreen_AnimatePressStart_803E938` share the cached-state
at `battles1_000001.state` because their `first_frame` is the VCount interrupt at
game frame 1, and the syscall that breaks the count is the same syscall regardless
of which function we stubbed out.

The fix is the patch's return encoding: pick an instruction that, like the
callee's own `pop {pc}`, ignores bit 0 of the loaded value. Both `pop {pc}` and
(more compactly) `mov pc, lr` qualify on ARMv4T. The brief picks
`mov pc, lr` per-detected-kind because each function's epilogue choice tells us
which LR-shapes they were designed for: a `bx lr` function is one whose callers
use the normal Thumb call convention (LR bit 0 set), so `bx lr` is safe there.
The kind is read off the function's *decoded instructions* (see *How the return
kind is detected* above), not off a byte pattern.
Regression tests: `test_skip_bytes_for_integration_detects_pop_pc_from_real_function`,
`test_skip_bytes_for_integration_detects_bx_lr_from_real_function`, and
`test_skip_does_not_stop_the_game[skip-None|return-0|return-1]` — every relation of
the v1 crash on `battles1` now keeps the counter running. `test_bx_lr_skip_does_stop_the_game`
forces the OLD encoding on the same function and confirms the crash is still
reproducible, so the explanation above is the actual cause and not a coincidence.

`IsScreenFadeActive` returns a value (its third instruction is `mov pc, lr` after
a `cmp r0, #1`), so `return:K` shows whether the *caller* reads the value or not:
when the patched run is `return:0` instead of `skip`, the game continues — the
caller's wait-loop terminates because the fade-active flag flipped back to 0.
That distinction is real and is what makes `return:K` worth running on top of
`skip`.

### `skip` also drops the flags

`skip` is `mov pc, lr` — two bytes that touch nothing but PC. `return:K` is
`movs r0,#K` followed by that return, and `movs` **sets the flags** (Z = (K == 0)).
So the two interventions do not only differ in r0: they differ in the condition
flags the caller sees.

The v3 pilot has one row where that is the whole difference:
`CutsceneCmd_wait_chatbox` (0x08037790) hangs under `skip` and under `return:1`
but **plays** under `return:0`. Its caller is the cutscene script dispatcher
(`RunCutscene`, 0x0803851c):

```asm
RunCutscene.cutsceneCommandLoop:
        mov     r6, ip
        ldrb    r0, [r7, #0]        @ the opcode
        lsls    r0, r0, #2          @ <- sets Z (Z = 0 for a non-zero opcode)
        ldr     r0, [r6, r0]
        mov     lr, pc              @ <- the same trampoline shape as the sound one
        bx      r0                  @ call the command
        bne     RunCutscene.cutsceneCommandLoop   @ Z clear -> run the next command
```

The dispatcher calls the command and then tests the flags the command left
behind. The command's own exit (`add r7,#2` / `mov r0,#1` / `pop {pc}`) leaves Z
set, so the dispatcher falls out of the loop. `skip` leaves the flags as the
dispatcher had them (Z = 0 from the `lsls`), so the dispatcher calls the same
command again — forever, because the skipped command never advances the script
cursor. `return:0` sets Z (`movs r0,#0`) and the dispatcher falls out of the loop
as it did originally: the cutscene proceeds.

Read `skip` and `return:K` as two different experiments: `skip` removes the call
*and* the flags it would have set, `return:K` keeps the call and replaces its
result (and, as a side effect, its flags).

## Working in a worktree

This pass ran in `build-wf-worktrees/intervene2`. Two things the orchestrator should
know:

- The worktree links `build-checkpoints`, `build-tools`, `gbarecomp`, `roms` and
  `toolchain` to the main checkout so the tool tests can find the ROM, the BIOS and the
  emulator binary. The `.gitignore` entries for those paths all end in `/`, which does
  not match a symlink, so every one of them showed up as untracked. `.gitignore` now
  has one entry per symlink (following the `/build-vidsync` precedent) — **the symlinks
  are still there, they are just not offered to git**. If a future worktree adds
  another one, add the matching line rather than committing the link.
- The `bn6f` submodule entry is untouched: the index still holds the gitlink
  (`160000 3b86ef82`), matching the superproject.

## What this pass fixed

| # | problem | fix | proof |
|---|---|---|---|
| 1 | `skip` wrote `bx lr`, which switches to ARM state when the caller set LR with `mov lr, pc` — six "hung" functions on `battles1` | the patch's return is chosen per function from its own epilogue (`return_kind_for`): `pop {pc}` / `mov pc, X` / unknown → `mov pc, lr`, `bx lr` → `bx lr` | `test_bx_lr_skip_does_stop_the_game` forces the old bytes and still wedges; `test_skip_does_not_stop_the_game` and `test_the_six_previously_hung_functions_run_to_completion` show the new bytes play the whole window |
| 2 | a `hung` verdict was one `stalled:` line, and it could be the *baseline* that stalled | `hung` needs the frame counter to have stopped on a side, and reports `hung_side` + mGBA's illegal-opcode count + a note | `test_hung_is_not_inferred_from_the_stalled_line_alone`, `test_hung_records_which_side_stopped`, `test_illegal_opcodes_come_from_mgba_run_stderr` |
| 3 | the first-frame search covered 1200 frames of one route: 17 of 24 pilot functions were "not reached" | whole-route search on the facts layer's routes + per-route execution map (§2) | `test_first_frame_search_finds_a_function_past_the_old_window`; this pilot reached 44 of 48 in one shot |
| 4 | every run kept its own 8 MB patched ROM (945 MB) | the copy goes in a `tempfile.mkdtemp` that is `rmtree`'d on every exit path; the run's result JSON keeps the hash and the patch bytes | `test_patched_roms_do_not_survive_a_run` |
| 5 | `--intervention` spelled out 258 argparse choices | one validated string (`InterventionKind.parse`), with the three forms named in the help | `test_intervention_argument_is_a_validated_string`, `test_intervention_subparser_does_not_enumerate_k` |

## Reading `hung`

`hung` is **not** a measurement failure. It means the run stopped making game
frames: mgba_run exhausted its hardware-frame budget without ever printing
`reached game frame N`, which is what a wedged CPU does (an infinite loop, or
execution that has fallen out of THUMB into ARM). Skipping a function the game calls
on a hot path *can and does* wedge it — and that is the finding.

But a "hung" verdict needs evidence, not just a single `stalled:` line. The
diagnostics are:

- **`hung_side`**: `''` (no hang), `'baseline'` (the baseline hung), `'patched'`
  (only the patched run hung), or `'baseline|patched'` (both).
- **`illegal_opcodes`**: the count of illegal instructions mGBA's own code
  reported on stderr for each side (`'2.4M illegal opcodes executed'` is the
  signature of the registers-state-was-corrupted path; an empty stderr means the
  CPU is alive but in a wait loop, e.g. an `IsScreenFadeActive`-style
  indefinitely-true return value).
- **`notes`**: a row-level explanation. A baseline hung row carries the warning
  "the unpatched baseline hung too: this window is not usable for comparing the
  intervention".

A `hung: True` row where `hung_side='patched|baseline'` is just nondeterminism
in the route/window; flip the route, or shorten the window, and re-run. A
`hung: True` row where `hung_side='patched'` and `illegal_opcodes > 0` is the
real ARM-mode-via-bx-lr signature described in *Why `bx lr` is not always safe*
above. A `hung: True` row with `illegal_opcodes == 0` is the wait-loop signature
— usually a `return:K` row the caller never reads the value of, only the side
effect, which is intact.

`deterministic` is a property of the **route/window**, so it is gated on the
*baseline* alone. A hung patched run leaves `deterministic` intact and sets
`hung`.

## Predicates

A small typed predicate language reads a `RunRecord` and returns
`True` / `False` / `None` (indeterminate — e.g. asked about an anchor that was not
observed). Each is a frozen dataclass with an `evaluate(run)` method, and
`make_predicate(spec)` parses the DSL string form:

| spec | true when |
|---|---|
| `differs(anchor=A, within=N)` | anchor A differs in some frame ≤ N |
| `unchanged(anchor=A, for=N)` | anchor A shows no difference in the first N frames |
| `oam_entries_missing(min=K, first_after=F)` | ≥ K OAM bytes differ, first at frame ≥ F; `None` if OAM wasn't observed |
| `screen_differs(region=x,y,w,h)` | the screen anchor changed |
| `sound_write_missing(reg=R)` | the named sound register does **not** differ (the write is missing) |
| `ram_anchor(name=N) baseline_minus_patched >= K` | the RAM anchor's difference magnitude clears the threshold |

The point of `None` is that a predicate must not silently pass on an anchor the run
never watched.

## Outputs

```
build-wf-runs/intervene-RUN_ID/
  report.tsv                      one row per (function, intervention) + its null row
  FUNCTION/result.json            the full RunRecord: per-anchor diffs, notes, hashes
build-wf-runs/intervene-pilot-RUN_ID/
  pilot.tsv                       one row per (function, intervention): 24 named + 24
                                  random reached functions x {null, skip, return:0, return:1}
  FUNCTION_KIND_ADDR_ROUTE/
    result.json
    baseline/{log.txt,shots/}     mgba_run stdout for the unpatched ROM
    patched/{log.txt,shots/}      mgba_run stdout for the patched ROM
build/intervene/patched-roms/     a temp dir per RUN, deleted when the run returns
build/intervene/states/ROUTE/     cached mgba_run states, one per stride-sized segment
build/intervene/exec-maps/        ROUTE.json: which addresses each segment executes
```

`build/intervene/` is under the main checkout (`wf.repo.REPO`, `WF_REPO`): the
workflow code can run from any worktree, but the ROMs, the recordings and the cached
states are shared. `patched-roms/` is only ever a scratch parent for
`tempfile.mkdtemp`, and the 945 MB of ROM copies an earlier pilot left there are gone.

## Pilot

`wf intervene-pilot` runs 24 named functions (drawing/OAM/palette/fade/sound/text/menu
plus battle logic) plus 24 drawn at random from the **functions the facts layer says
some route reaches**, each as `null`, `skip`, `return:0` and `return:1` — 192
emulator runs, 120 frames each. The random tail is drawn from the reached set rather
than from a keyword-matching lifted set, because a function no recording executes can
only ever produce a "not reached" row; it is filtered through `find_function` at
selection time, because the lifted unit names are a superset of the ELF symbols and a
name that cannot be patched can only ever produce an `unsupported` row. Each
function's route is the shortest one the facts list *and the route's execution map
confirm* (§2). Before any trial runs, each route in play gets its cached states and
its execution map — one whole-route pass per route, shared by every function on it.
Trials are independent subprocesses and fan out across a thread pool; results are
re-ordered to the selection order so `pilot.tsv` does not depend on completion order.

### Pilot run v1 (2026-10-21, `battles1`, 60 frames, 1200-frame bisect window) — superseded

Kept because every "hung" below is now known to be the `bx lr` crash of problem 1,
and the reader should be able to check that claim against the old table. The rows were
24 functions × {`skip`, `null`}, a 1200-frame window and one route; 17 of 24 were
never reached; all six executions were reported `hung`.

| function | source | first frame | `skip` result | first anchor diff |
|---|---|---|---|---|
| `call_m4aSoundMain` | named | 1 | **hung** (43 anchor frames differ) | `display_io@2` |
| `SetScreenFade` | named | 1 | **hung** (43) | `display_io@2` |
| `IsScreenFadeActive` | named | 1 | **hung** (43) | `display_io@2` |
| `LoadGFXAnims` | named | 233 | **hung** (27) | `screen@18` |
| `LoadGFXAnim` | named | 233 | **hung** (27) | `screen@18` |
| `startScreen_AnimatePressStart_803E938` | random | 1 | **hung** (43) | `display_io@2` |
| `palette_copyTableRow_80AA104` | named | 1 | **no observable effect** (0) | — |
| `PlaySoundEffect`, `PlayMusic`, `LoadBGAnimData`, `LoadBGAnimForMapGroup`, `screenFade_80062C8`, `owPlayer_setPalette_809e2a0` | named | — | not reached in 1200 frames | — |
| 11 further random functions | random | — | not reached in 1200 frames | — |

What the v1 pilot actually established:

- **The window is sound.** All 7 executed functions had `deterministic=True` null
  runs with 0 anchor differences over 60 frames. The verdicts are real. (The other 17
  null rows also show 0 differences, but trivially: the function never ran, so there was
  nothing to differ.)
- **Six functions are load-bearing.** Skipping any of them wedges the game within a
  second. `call_m4aSoundMain`'s patched run ends at `pc=0x080005ae` with
  `cpsr=0x2000003f` — the T bit clear, i.e. the game is executing ARM garbage two
  halfwords into the function we just stubbed out.
- **The three `first_frame=1` named cases share one cached state**
  (`battles1_000001.state`), so their identical diffs are the same divergence, not
  three coincidences. `startScreen_AnimatePressStart_803E938` lands on that same state
  and the same diff for the same reason. That shared state is also why all six rows
  said `hung`: one crash, six reports.
- **`palette_copyTableRow_80AA104` is a genuine no-op on this route.** Its `skip` run
  reaches the same frame with the same register state as the baseline. Its callers
  ignore the return value *and* nothing it writes survives to an anchor within 60
  frames. This is the kind of result the tool exists to produce: a name that promises
  palette work with no visible consequence here.
- **17 of 24 were never reached** inside 1200 frames of a battle route. That is the
  honest limit of one route — a function that matters on the overworld says nothing
  here, and a `None` row is not a negative result.

### The same six on `battles1` after the fix (problem 2)

120 frames each, route fixed to `battles1`, `anchors=()` (the hang evidence does not
need anchors):

| function | first frame | `null` | `skip` | `return:0` | `return:1` |
|---|---|---|---|---|---|
| `call_m4aSoundMain` | 1 | clean | plays on, 0 illegal | plays on | plays on |
| `SetScreenFade` | 1 | clean | plays on | plays on | plays on |
| `IsScreenFadeActive` | 1 | clean | plays on | plays on | plays on |
| `LoadGFXAnims` | 232 | clean | plays on | plays on | plays on |
| `LoadGFXAnim` | 232 | clean | plays on | plays on | plays on |
| `startScreen_AnimatePressStart_803E938` | 1 | clean | plays on | plays on | plays on |

**None of the six is really hung.** All six `hung` verdicts were the `bx lr` crash:
`hung_side=''` and `illegal_opcodes=(0, 0)` on every row, patched and baseline.

The one that could have been real is `IsScreenFadeActive` — it returns a status, and
under `skip` r0 keeps whatever the caller left in it, so a fade wait loop
`while IsScreenFadeActive():` could spin forever. It does not, and the reason is
visible in the anchors: `skip` diverges from the baseline at `display_io@2` and keeps
diverging (`palette` follows at frame 4), i.e. the fade never starts, so the loop is
never entered. The same is true of `return:0`/`return:1`, which is a stronger result
than "it did not hang": the caller's decision changed, and the game took the other
branch. `test_the_six_previously_hung_functions_run_to_completion` pins this.

### Pilot run v2 (2026-10-21) — superseded by v3

The v2 run (`intervene-pilot-20261021-151503`) is the same code path with a
different draw of the random half (the seed is per run; `--seed` now makes it
explicit). It reached 47 of 48 functions in one shot, all 47 `null` runs
deterministic, one hang (`NPCCommand_set_layer_priority_override_to_2`, the same
NPC-cursor shape as v3's first hang) and the same headline numbers
(`skip` vs `return:0`: same first anchor 37/46, same magnitude 7/46). The patch
bytes are identical in both runs (`f746` for every function), because the
instruction-level detection of the return kind does not change what any of these
functions is patched with.

### Pilot run v3 (2026-10-24, 24 named + 24 random, 120 frames, auto routes)

`intervene-pilot-20261024-070934` (`--route auto --frames 120 --random 24 --jobs 8
--seed 20261024`): 48 functions x {`null`, `skip`, `return:0`, `return:1`} = 192
emulator runs of 120 frames each, 2m23s with the routes' states and execution maps
already cached. Every cell reads *what happened to the game*, then *how many anchor
frames differ from the baseline*, then *the first anchor to differ*. `plays` means
the run reached the end of the window on both sides with no illegal opcodes.

| # | function | src | route | first frame | `null` deterministic | `skip` | `return:0` | `return:1` |
|---|---|---|---|---|---|---|---|---|
| 1 | `call_m4aSoundMain` | named | wander | 1 | yes | plays, 966f, display_io@2 | plays, 1036f, display_io@2 | plays, 1036f, display_io@2 |
| 2 | `SoundMain` | named | wander | 1 | yes | plays, 1050f, display_io@2 | plays, 1050f, display_io@2 | plays, 1050f, display_io@2 |
| 3 | `sound_dispatchTask_8000630` | named | story1 | 8093 | yes | plays, 1341f, display_io@8094 | plays, 1341f, display_io@8094 | plays, 1341f, display_io@8094 |
| 4 | `SetScreenFade` | named | wander | 1 | yes | plays, 1086f, palette@4 | plays, 1380f, palette@4 | plays, 1380f, palette@4 |
| 5 | `IsScreenFadeActive` | named | wander | 1 | yes | plays, 1064f, display_io@2 | plays, 1134f, display_io@2 | plays, 1218f, display_io@2 |
| 6 | `screenFade_InitFadeBlock_800634C` | named | wander | 1 | yes | plays, 1330f, display_io@2 | plays, 1288f, display_io@2 | plays, 1288f, display_io@2 |
| 7 | `LoadGFXAnims` | named | wander | 232 | yes | plays, 1276f, palette@268 | plays, 1136f, palette@268 | plays, 1136f, palette@268 |
| 8 | `LoadGFXAnim` | named | wander | 232 | yes | plays, 464f, palette@268 | plays, 464f, palette@268 | plays, 464f, palette@268 |
| 9 | `startScreen_AnimatePressStart_803E938` | named | wander | 1 | yes | plays, 896f, display_io@2 | plays, 1134f, display_io@2 | plays, 1134f, display_io@2 |
| 10 | `sprite_gfxAnimState_GotoNext_8002310` | named | wander | 650 | yes | plays, 1136f, palette@652 | plays, 296f, palette@652 | plays, 296f, palette@652 |
| 11 | `drawBGTileMapLeftColumns_803066E` | named | wander | 740 | yes | plays, 978f, vram_bg@742 | plays, 866f, vram_bg@742 | plays, 866f, vram_bg@742 |
| 12 | `owObject_lineDraw_8030CAC` | named | wander | 739 | yes | plays, 672f, display_io@741 | plays, 784f, display_io@740 | plays, 784f, display_io@740 |
| 13 | `drawChipNameTileRow` | named | menus3 | 6357 | yes | plays, 997f, vram_bg@6359 | plays, 227f, vram_bg@6359 | plays, 227f, vram_bg@6359 |
| 14 | `battle_drawChipCountsOAM_801C082` | named | story7 | 1181 | yes | plays, 826f, display_io@1182 | plays, 770f, display_io@1182 | plays, 770f, display_io@1182 |
| 15 | `sprite_submitOamSet_802F994` | named | wander | 651 | yes | plays, 953f, oam@653 | plays, 1149f, oam@653 | plays, 1149f, oam@653 |
| 16 | `copyOamEntry_80AA004` | named | wander | 738 | yes | plays, 910f, display_io@739 | plays, **no diff** | plays, **no diff** |
| 17 | `chatbox_emitTextGridOAM_804021C` | named | wander | 833 | yes | plays, 800f, oam@835 | plays, 842f, oam@835 | plays, 842f, oam@835 |
| 18 | `sprite_getPalette` | named | -- | -- | -- | -- no route -- | -- no route -- | -- no route -- |
| 19 | `copyPalletesToIWRAM_8002650` | named | wander | 1 | yes | plays, 1110f, palette@2 | plays, 1152f, palette@2 | plays, 1152f, palette@2 |
| 20 | `chatbox_stepTextPrinter_801CE92` | named | story7 | 1949 | yes | plays, 1016f, oam@1951 | plays, 1030f, oam@1951 | plays, 1030f, oam@1951 |
| 21 | `pet_menuUpdateAndHandleExit_8005462` | named | story1 | 6099 | yes | plays, 1471f, oam@6101 | plays, 1429f, oam@6101 | plays, 1429f, oam@6101 |
| 22 | `submenuDrawHpGauges` | named | story7 | 1193 | yes | plays, 1225f, vram_bg@1195 | plays, 1197f, vram_bg@1195 | plays, 1197f, vram_bg@1195 |
| 23 | `RunBattleObjectLogic` | named | story7 | 1130 | yes | plays, 1584f, oam@1132 | plays, 1556f, oam@1132 | plays, 1556f, oam@1132 |
| 24 | `checkThenStartBattle_8005A8C` | named | wander | 739 | yes | plays, 644f, display_io@741 | plays, 644f, display_io@741 | plays, 644f, display_io@741 |
| 25 | `battle_stepWaitForStateRunUpdate` | random | menus3 | 9055 | yes | plays, 837f, oam@9057 | plays, 963f, oam@9057 | plays, 963f, oam@9057 |
| 26 | `sub_8132FB8` | random | story1 | 9010 | yes | plays, 1294f, vram_bg@9011 | plays, 944f, vram_bg@9011 | plays, 944f, vram_bg@9011 |
| 27 | `battle_setupAllianceEntryKind3_802DFC8` | random | story7 | 1130 | yes | plays, 1162f, display_io@1132 | plays, 938f, display_io@1133 | plays, 938f, display_io@1133 |
| 28 | `playOamSeq_802F9EC` | random | story1 | 1273 | yes | plays, 816f, oam@1275 | plays, 970f, oam@1275 | plays, 970f, oam@1275 |
| 29 | `startScreen_updateFrame_802F8D8` | random | wander | 651 | yes | plays, 757f, oam@653 | plays, 1023f, oam@653 | plays, 1023f, oam@653 |
| 30 | `NPCCommand_give_attached_shadow` | random | wander | 5969 | yes | **hung** (patched), 64f, display_io@5970 | **hung** (patched), 64f, display_io@5970 | **hung** (patched), 64f, display_io@5970 |
| 31 | `bg_LoadFullScreenTilesAndSetup_812AFB0` | random | menus3 | 8568 | yes | plays, 755f, vram_bg@8570 | plays, 811f, vram_bg@8570 | plays, 811f, vram_bg@8570 |
| 32 | `battleObject_aiGetUnk4cPtr_801442C` | random | story7 | 2004 | yes | plays, 1212f, palette@2006 | plays, 686f, display_io@2005 | plays, 686f, display_io@2005 |
| 33 | `CopyBytes` | random | wander | 738 | yes | plays, 891f, display_io@739 | plays, 723f, display_io@739 | plays, 723f, display_io@739 |
| 34 | `returnZero_809E228` | random | wander | 739 | yes | plays, 976f, oam@742 | plays, **no diff** | plays, 990f, oam@742 |
| 35 | `object_highlightCurrentCollisionPanels` | random | battles1 | 6006 | yes | plays, 818f, vram_bg@6008 | plays, 636f, vram_bg@6008 | plays, 636f, vram_bg@6008 |
| 36 | `owPlayer_isJumptableIndex09At0x10_0809DF28` | random | wander | 739 | yes | plays, 700f, display_io@740 | plays, 770f, display_io@741 | plays, 504f, display_io@741 |
| 37 | `CutsceneCmd_wait_chatbox` | random | wander | 830 | yes | **hung** (patched), 59f, display_io@831 | plays, 546f, display_io@832 | **hung** (patched), 59f, display_io@831 |
| 38 | `chip_class_lookup_800FC9E` | random | story7 | 1130 | yes | plays, 1162f, display_io@1132 | plays, **no diff** | plays, **no diff** |
| 39 | `camera_callModeHandler` | random | wander | 739 | yes | plays, 994f, display_io@740 | plays, 882f, display_io@741 | plays, 882f, display_io@741 |
| 40 | `checkOWObjectInteractions_80037f4` | random | wander | 739 | yes | plays, 602f, display_io@740 | plays, 714f, display_io@741 | plays, 714f, display_io@741 |
| 41 | `GetRenderInfoLCDControl` | random | story1 | 8313 | yes | plays, 1177f, display_io@8314 | plays, 1191f, display_io@8314 | plays, 1191f, display_io@8314 |
| 42 | `battle_aiAddTotalDamageTaken_8010548` | random | story7 | 2249 | yes | plays, 1106f, display_io@2250 | plays, 1316f, display_io@2250 | plays, 1316f, display_io@2250 |
| 43 | `battle_pickChipAttr_80AA910` | random | story7 | 6440 | yes | plays, 1055f, vram_bg@6496 | plays, 1490f, oam@6525 | plays, 1148f, palette@6488 |
| 44 | `encryption_xorBufferWithKey_8006df6` | random | story1 | 9065 | yes | **hung** (patched), 61f, screen@22 | **hung** (patched), 61f, screen@22 | **hung** (patched), 61f, screen@22 |
| 45 | `sub_8029F70` | random | story7 | 1183 | yes | plays, 770f, display_io@1184 | plays, **no diff** | plays, **no diff** |
| 46 | `t1_0x54_80C3EE0` | random | menus3 | 4705 | yes | plays, 1064f, oam@4707 | plays, 994f, oam@4707 | plays, 994f, oam@4707 |
| 47 | `battle_getStateTableEntryPtr_802C8EA` | random | story7 | 6446 | yes | plays, 840f, display_io@6449 | plays, 798f, display_io@6447 | plays, 798f, display_io@6447 |
| 48 | `sub_8132380` | random | menus3 | 10016 | yes | plays, 714f, display_io@10018 | plays, **no diff** | plays, **no diff** |

The 48th function, `sprite_getPalette` (named), is the honest gap: the facts layer
lists it on `battle` and `beastsp`, and a whole-route `--count-entry` on `battle`
says **0 entries** at 0x08002d8c, so no recording ever branches to its entry. It
gets four empty rows rather than a fabricated one. The same check moved
`sound_dispatchTask_8000630` off `wander` (0 entries) onto `story1`, where it first
runs at game frame 8093 — a hit no 1200-frame window on a battle route would ever
have found.

Summary:

| question | answer |
|---|---|
| `null` deterministic (i.e. is the window trustworthy)? | **47 / 47** — every route/window gate passed |
| `skip` changes at least one anchor? | **47 / 47** — every function the pilot could run moves an anchor |
| `skip` hangs? | **3 / 47** — all three real, all three explained below |
| `return:0` hangs? | **2 / 47** — `CutsceneCmd_wait_chatbox` plays under `return:0` but hangs under `skip` |
| `skip` vs `return:0`: same first differing anchor? | 33 / 47 |
| `skip` vs `return:0`: same *number* of differing frames? | 6 / 47 — the magnitude almost always differs |
| where the divergence starts | `display_io` 23 rows, `oam` 10, `vram_bg` 7, `palette` 6, `screen` 1 |
| routes the facts chose | wander 24, story7 11, story1 6, menus3 5, battles1 1 |
| first frame | min 1, median 1130, max 10016; **16 of 47 first run after frame 2000** |
| the patch bytes used | `f746` (`mov pc, lr`) on all 47 — every one of them returns with `pop {…pc}` or `mov pc, …` |

Reading it:

- **`skip` is nearly always observable, and almost always quickly.** The very first
  anchor to move is `display_io` (DISPCNT/BG/OAM/…) on most rows, within 1-3 frames of
  the first execution: these are per-frame OAM/palette/VRAM writers on the main loop,
  so removing one shifts the very next frame's hardware state.
- **`return:0` is not `skip` with a smaller number.** Same first anchor 33 times out
  of 47, but the same magnitude only 6 times. The two answers differ by hundreds of
  anchor-frames in either direction, which is exactly the separation the tool is for:
  `skip` destroys the call, `return:0` keeps the call and hands back a wrong answer.
- **The no-diff cells are the interesting ones.** Five of the 47 functions show
  `return:0` and/or `return:1` with **no anchor difference at all**:
  `copyOamEntry_80AA004` (both), `returnZero_809E228` (return:0), `chip_class_lookup_800FC9E`
  (both), `sub_8029F70` (both), `sub_8132380` (both) — their callers never read the
  return value, so only removing the call itself has an effect. The mirror image
  (a function whose *value* matters but whose call does not) did not appear in this
  draw; the v2 pilot found one (`split9BitsFromBitfield_8021AE0`: `skip` changed no
  anchor at all while `return:0` changed hundreds).
- **All three hangs are real, and all three are the same shape.** In every one the
  patched side stopped and the baseline did not, with **0 illegal opcodes** — the CPU
  was executing valid code in a loop that never comes back, not running off into
  garbage. That is the difference from problem 1 (which showed 2.4 M illegal opcodes
  and an ARM-state PC).

  | function | route | first frame | `skip` | `return:0` | `return:1` | why |
  |---|---|---|---|---|---|---|
  | `NPCCommand_give_attached_shadow` | wander | 5969 | hung | hung | hung | the NPC script command's cursor advance (`add r6,#1`) never runs, so the dispatcher re-reads the same opcode forever |
  | `CutsceneCmd_wait_chatbox` | wander | 830 | hung | **plays** | hung | the cutscene dispatcher loops on the *flags* the command left; `skip` leaves them alone, `return:0` sets Z (see *`skip` also drops the flags* above) |
  | `encryption_xorBufferWithKey_8006df6` | story1 | 9065 | hung | hung | hung | the save buffer is never XORed, so the checksum never matches and the caller retries forever |

  The first one is the v2 pilot's hang again, one function over: the NPC script
  dispatcher reaches commands through a table and stores the cursor with `str r6,
  [r5, #76]`, so a command that does not advance `r6` is re-read forever. The run
  ends at the dispatch site itself (`pc=0x0809ebe4 lr=0x0809ebf2 cpsr=0x4000003f`
  — still Thumb, still valid code, just the wrong code).
- **What a `hung` row is worth here: small.** A hang stops the run a few dozen anchor
  frames into the window (35-64 frames out of a possible 960). Read
  `first_anchor_diff` on a hung row, never the count.

### `skip` and `return:K` are not interchangeable

`skip` answers two questions at once: *does the call matter* and *does the result
matter*. `return:K` answers only the second. Two rows from the v2 pilot, same route,
same window, same function:

| function | `skip` | `return:0` / `return:1` | what it means |
|---|---|---|---|
| `copyOamEntry_80AA004` (wander, frame 738) | diverges in 910 anchor frames, `display_io@739` | **no anchor differs at all** | the call matters, the return value does not: every caller ignores it |
| `split9BitsFromBitfield_8021AE0` (wander, frame 738) | **no anchor differs at all** | diverges in 1008 / 910 anchor frames, `display_io@739` | the return value matters, the call does not: a pure helper feeding a computation |

The first is the case a name gets wrong — the function *is* load-bearing (its writes
reach OAM) but its status is decoration. The second is the case where a rename or a
`symbols/db` entry saying "does nothing here" would be right about the anchors and
wrong about the function: skip it and the game is unchanged for 120 frames, because the
caller re-derives everything from the corrupted value instead.

So on a `skip` row, `differing_frames` is a measure of "the call matters", not of "how
much this function does". To ask the second question you have to run `return:K`, and
`return:0` (a falsy answer) is the specific test for "does the caller branch on it".

One more asymmetry worth knowing: `skip` and `return:K` produce the same
`first_anchor_diff` on 37 of 46 rows but the same magnitude on only 7. Reading
`differing_frames` off a `skip` row and concluding "this function writes N frames of
VRAM" is not supported by anything.

## Limits

- **One route, one window.** A `None` (not reached) row means "no recorded route
  enters this function"; it is never evidence that the function is dead. The facts
  layer and the execution map can also be *too generous*: both credit a function's
  whole address range, so a symbol whose tail is shared with something else reads as
  reached although nothing branches to its entry (`sprite_getPalette`: 0 entries in a
  whole-route `--count-entry` on both routes the facts list).
- **Anchor-limited.** A function can do real work that none of the eight anchors
  observe and still report zero differences — `split9BitsFromBitfield_8021AE0` in the
  v2 pilot is exactly that, and the `return:K` rows on the same function are what
  reveal it.
- **`skip` measures the call, not the function.** See *`skip` and `return:K` are not
  interchangeable* above; `copyOamEntry_80AA004` is the mirror-image case.
- **A hang costs the rest of the window.** The diffs up to the wedge are real, but
  `differing_frames` under-reports (35 frames out of a possible 960 on the v2 pilot's
  one hang). Read `first_anchor_diff` on a hung row.
- **The state grid is the search's floor.** Everything here assumes
  `build/intervene/states/ROUTE/` holds a state per stride-sized segment. A route
  whose whole-route state pass timed out has a partial grid; the search then covers
  only the prefix that exists and says so through the segment it returns.
- **`ram:` in `symbols/anchors.yaml` is empty by policy.** A RAM anchor is only
  trustworthy once a `null` run has shown the window is deterministic; until one has,
  a RAM difference is indistinguishable from emulator noise. Candidates go in the
  file's `#ram-pending` block and move up only with a passing null.
- **A hang masks what came before it.** The diffs up to the wedge are real, but the
  window after it is empty, so `differing_frames` under-reports. Read
  `first_anchor_diff` on a hung row, and follow a hang with a `return:K` run (see
  above) to recover the magnitude.