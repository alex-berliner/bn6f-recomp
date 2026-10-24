# `wf investigate --facts` — the runtime-facts block, and the A/B that measures it

`wf investigate` asks a model what one placeholder function (`sub_*`) of bn6f is for, from its
assembly, its callers' call sites and the data it touches. Everything it shows the model is read
**out of the disassembly**: the code says what the function *could* be for, never what it *is*.

`wf facts` ([facts-notes.md](facts-notes.md)) records what the original game actually did at
runtime, per function, from the project's own recordings — no model in it. This note is about
showing a compact piece of that to the proposer and the checker, and about what happened when the
two were measured against each other.

Nothing here renames anything: `--facts` changes what the models are shown, and the runs write
candidates under `build/investigate/RUN_ID` exactly as before.

## The flag

```bash
python -m wf investigate --only-file pilot/ab-armA.txt --facts   \
    --model minimax/MiniMax-M3 --checker minimax/MiniMax-M3 --jobs 12
```

* `--facts` / `--no-facts` — off by default. **Without the flag the brief is byte for byte what it
  was before this change** (`facts_rule()` returns nothing and `context_text()` adds no section),
  so the A/B below differs in one thing only.
* `--only-file FILE` — a file of function names, one per line (`#` comments allowed), taken like
  `--only`. A run over a chosen set does not need the list repeated sixty times on the command
  line; a pilot's two halves are exactly this.
* A name given by `--only`/`--only-file` is taken even when an earlier run already wrote a
  candidate for it (`select_targets`): an earlier candidate that held no name is a reason to ask
  again, and asking the same two prompts about the same functions is what an A/B is. Without an
  explicit list the old rule stands (an earlier candidate keeps a function out).

## The block

`runtime_facts_text()` builds it from **one** `wf facts` record, looked up by entry address
(`build/facts/by_address/0xADDR.json`, so a rename never moves it — `$WF_FACTS_DIR` moves it).
It is capped at `FACTS_LINES` (25); over the pilot's 120 functions the longest block was 10 lines
and the mean 8. It goes into the context after the code sections, and both prompts (proposer and
checker) carry the same block.

```
Runtime facts (observed by running the ORIGINAL game, not inferred: the recorded routes in
build/facts, and captured calls of it replayed in mGBA):
  scenes it runs in: battle (battlesweep), mixed (beastsp)
  reached by 3 of 21 recorded routes; 6 calls at the recorded indirect call sites
  runtime callers, with the calls they made: ai_dispatchElementAttackStep_80FFC82 (6 calls)
  RAM symbols it wrote: eAIData1_AttackVars (60 bytes, 9 values), eCollisionData1_ObjectFlags1
    (8 bytes, 1 value), eT1BattleObject1_CurAnim (8 bytes, 1 value), ... (+3 more)
  I/O it wrote: none seen (write-only registers are never listed)
  arguments: r0 = 0x00000000 (8 calls, one value), r1 = 0x080FFCA9 (8 calls, one value), ...
  returns: r0 = 0x00000001..0x00000004 (8 calls, 3 values)
  argument registers left unchanged: sp
  Blind spots: only values that CHANGED are listed (reads and same-value writes are invisible);
    write-only I/O registers read back as junk and are never listed, so an empty I/O line does NOT
    rule out display, scrolling, DMA or sound work.
```

(one line per row in the prompt; wrapped here to fit the page)

What is in it, and why:

| line | from | why it is there |
|---|---|---|
| scenes it runs in | `routes[]`, mapped to a scene (`battle*`/`chips` → battle, `story*`/`maps`/`wander` → overworld, `menus*` → menu, `title_mash_a` → title, `beastsp`/`fuzz` → mixed) | a function the recordings only reach in battles is not a menu function |
| reached by N of M routes | `reached`, `routes[]` | how thin the observation is |
| runtime callers | `routes[].callers`, aggregated by name, a named caller first | who calls it **while the game plays**, which the static call graph does not say |
| RAM symbols written | `facts.memory_writes` (≤ 10, widest first, with distinct-value counts) | the strongest single hint a name can have: `eEventFlags`, `eOWPlayerObject_*`, `oBattleState_*` |
| I/O written | `facts.io_writes`, grouped by register (≤ 8) | rare (write-only registers read back as junk), but decisive when it happens |
| arguments / returns | `arguments`, `returns` as ranges | pointer vs. small integer, one value or many |
| left unchanged | `facts.registers_written` against the argument registers | a function that changes nothing in r0..r3 returns nothing |

Two cases are lines of their own:

* **never reached** — "never executed by the recordings: no route reached it and no call of it was
  captured, so what it does is not observed at all — say so in `open_questions` rather than reading
  a purpose into it." Silence is information, but it argues for nothing either way.
* **no record** — `wf facts` has not been run for that address. One line, and nothing is guessed.

A bare region name (`IWRAM`, the `end_file_…` marker of a write past the last symbol) sorts below a
real symbol: "it wrote somewhere" is worth less than `eT1BattleObject1_CurAnim`.

### What the prompts are told

With a block in the context, both prompts get one paragraph (`facts_rule()`): the block is
evidence rather than a guess, the name, summary and interface must be consistent with it, and a
contradiction is not something to argue past — propose what the code and the facts support
together, or answer `"proposed_name": null` and put the contradiction in `open_questions`.

## The A/B pilot

**Question.** Does the runtime-facts block make `wf investigate` propose more usable names, and
make more of them hold up under the cross-check?

**Set.** 120 `sub_*` functions that (a) `build/facts/by_address` says a recorded route reached,
(b) are still `sub_*` in `symbols/meaning_scores.tsv` (so still unnamed in bn6f), and (c) no
earlier `wf investigate` run proposed a name for (read over every `build/investigate/*/report.tsv`:
235 functions qualified, 120 were drawn at random with `seed 20261021` and split in two halves of
60). `pilot/pick_ab.py` picks them; `pilot/summarise_ab.py` reads the four runs back.

Both proposer and checker were `minimax/MiniMax-M3` in every run, `--jobs 12`, one run at a time,
nothing landed (the candidates are in the run folders, for the orchestrator to review).

| run id | arm | functions | proposed | probable+agree | tokens |
|---|---|---|---|---|---|
| `investigate-20261021-100724` | A **with** facts | 60 | 31 | **14** | 177,555 |
| `investigate-20261021-101004` | A without facts | 60 | 18 | **6** | 109,963 |
| `investigate-20261021-101229` | B **with** facts | 60 | 40 | **19** | 159,712 |
| `investigate-20261021-101527` | B without facts | 60 | 37 | **20** | 128,010 |

Both halves together, 120 functions:

| arm | proposed | probable+agree | tokens | per function |
|---|---|---|---|---|
| with facts | **71** | **33** | 337,267 | 2,810 |
| without facts | 55 | 26 | 237,973 | 1,983 |

* **+16 names proposed (+29%)**, **+7 probable+agree (+27%)**, for **+42% tokens**.
* Named in both arms: 45 of the 120; named in one arm only: 26 with facts, 10 without.
* Of those 45, **44 got a different name** — the block does not merely confirm a name the other arm
  would have found, it changes which name is proposed.

### Where the gain is

Half of the 120 have no captured calls, so their record carries route coverage only; the other half
has captured calls that `wf facts` replayed in mGBA (arguments, RAM and I/O writes). Split on that:

| group | n | with facts: proposed / probable | without: proposed / probable |
|---|---|---|---|
| captured calls replayed | 60 | 35 / **16** | 24 / 8 |
| route coverage only | 60 | 36 / 17 | 31 / 18 |

The whole effect sits in the first group: **twice the probable names** (16 vs 8) where there are
replayed calls, and nothing at all where the block can only say *which scenes reached this*.
Half of the pilot set is uncaptured because the capture step (`wf capture`) is what fills
`build-snapshots/`; **the facts block pays for itself in proportion to what has been captured.**

### The 15 side-by-side pairs

The orchestrator judges these: same function, same model, same assembly, the only difference the
block. `facts:` is what the facts arm was shown (scenes, runtime callers, RAM symbols).

| function | with facts | without facts | what the facts said |
|---|---|---|---|
| `sub_800D668` | `object_canClaimPanelAlliance` | `object_checkColumnAlliance` | mixed (beastsp), menu (menus2, menus3), overworld (story2); no named RAM (only `end_file_3007B00`, 372 bytes, 15 values) |
| `sub_800EABA` | `battle_clearBusterChargeFlag` | `battle_sweepCompleteAction` | battle (battlesweep) |
| `sub_801DCCC` | `battle_assignLandingLink` | `sprite_setLinkOffsetByOwner` | battle (battlesweep) |
| `sub_802A974` | `battleStepDispatcher` | `scene_dispatchStepByState` | battle (battlesweep) |
| `sub_803DD60` | `e_checkLinkStateReady` | `checkLinkStateReady` | battle (battlesweep) |
| `sub_8083F34` | `cutscene_resetSceneFlagsAndGfx` | `clearPetStoryFlags` | mixed (beastsp), overworld (story5, story6); `CutsceneCmd_call_native_with_return_value` (14 calls); `byt_3007F78`, `eGFXAnimState13_End`, `eGFXAnimState14_CommandPos`, … |
| `sub_8084180` | `cutscene_rollIntroSelection` | `cutscene_pickIntroPetSlot` | mixed (beastsp), overworld (story5, story6); `ePrimaryRngSeed` (8 values), `byte_2013965`, `end_file_3007B00` |
| `sub_8084214` | `ow_placeOffsetFromPlayer` | `cameraSetIntroTargetOffset` | mixed (beastsp), overworld (story5, story6); `word_2013974` (64 bytes, 16 values) |
| `sub_80871F4` | `battle_copyBeastSpStateData` | `copy_publishStagedStateToActive` | mixed (beastsp), overworld (maps); `byte_2000ED6`, `dword_2000F88` |
| `sub_8089F70` | `owBeastspAdvanceSwingFrame` | `owPlayer_applyBeastSpinStep` | mixed (beastsp); `eCutsceneState` (8 values), `eStruct200ace0_PlayerX/Y/Z`, `eOWPlayerObject_FacingDirection` |
| `sub_808A054` | `battleUpdateBeastAim` | `object_facePlayerAndActivate` | mixed (beastsp); `dispatch_808A028` (3 calls); `dword_200A498`, `dword_200A490`, `eCutsceneState*` |
| `sub_808A0A6` | `ow_exitBeastSpState` | `ow_finishBeastOut` | mixed (beastsp); `dispatch_808A028` (3 calls); `eEventFlags`, `eGameState_ChatboxMessageSlot`, `eOWPlayerObject_*` |
| `sub_809B9EC` | `submenu_applyCursorDelta` | `ow_applyZoneOffset` | mixed (beastsp); `eCutsceneState_owMapObjectPtrsEnd_70`, `end_file_3007B00` |
| `sub_80A59B8` | `owPlayer_selectEncounterTile` | `owPlayer_initDirectionAnim` | mixed (beastsp); `eOverworldMapObject0_Z`, `byte_2013946` |
| `sub_80A8674` | `battle_beastSP_initAction` | `battle_setupBeastSPSprite` | mixed (beastsp); `battleObject_dispatchUpdateByCurAction_80A8654` (186 calls); `byte_2013946`, `dword_200A498`, `dword_200F3D0`, … |

Reading them: where the facts name the RAM the function writes, the facts-arm name is the one that
mentions it (`battle_clearBusterChargeFlag` over `battle_sweepCompleteAction`; the `eOWPlayerObject_*`
writer is a facing/position function, not a sprite-offset one). Where the facts could only say
`battle (battlesweep)` (`sub_800EABA`, `sub_801DCCC`, `sub_802A974`, `sub_803DD60`) the two names
are near-synonyms and the scene is all the block had to offer — which is why that group shows no
measurable gain. `sub_803DD60` also shows the naming rules at work: `e_checkLinkStateReady` keeps
the project's data-symbol prefix, plain `checkLinkStateReady` does not.

## Where the four runs are

`build/investigate/investigate-20261021-100724`, `-101004`, `-101229`, `-101527` in the main
checkout (report.tsv, `NAME.yaml` candidates, `contexts/`, `proposals/`, `checkers/` each).
**Nothing was landed**: no rename, no `symbols/db` entry, no commit. Reviewing and landing is the
orchestrator's, through the normal path.

## Reading it

* The block is worth its tokens: +29% names proposed and +27% probable, at +42% tokens, on a
  placeholder set the earlier runs could not name.
* The gain is entirely in the functions whose calls were captured and replayed. Where only route
  coverage exists, the block costs tokens and changes nothing — a reason to capture more functions
  (`wf capture`) rather than a reason to distrust the block.
* The costs are honest ones: I/O writes are almost always empty (write-only registers read back as
  junk), "0 calls" means "not at a recorded indirect site", and a `never executed` line says
  nothing either way.
* Not measured here: whether the facts arm's names land more often once reviewed. The pilot can
  only say that more names were proposed and that more of them survived their own cross-check.
  Landing them, and comparing what sticks, is the next measurement.
---

# v2: the block again -- scenes per call, struct fields, CPU entry rates

The pilot above is the baseline this section is measured against. Nothing of it changed: the same
120 functions (`pilot/ab-armA.txt`, `pilot/ab-armB.txt`), the same two arms, the same model
(`minimax/MiniMax-M3`) as proposer and checker, `--jobs 12`, one run at a time.

## What the block says now

`runtime_facts_text()` (`tools/wf/wf/tools/investigate.py`) builds the same short block from a
richer `wf facts` record. Three of its lines changed:

| line | v1 | v2 |
|---|---|---|
| scenes | route-level: `scenes it runs in: battle (battlesweep), mixed (beastsp)` | **per captured call**: `scenes it runs in (per captured call): overworld (8 calls)` -- the two mode bytes `main_` dispatches on (`oToolkit_MainJumptableIndexPtr` and `GameState.SubsystemIndex`), read out of each captured call's *own* entry snapshot (`tools/wf/wf/tools/scenes.py`). A route is an input tape, not a scene: `story2` runs a battle, opens a menu and comes back to the map. When there is no captured call the route-level line is still used, now with the caveat **`(route-level, routes mix scenes)`** attached, so the model is told which kind of claim it is reading. A mode the tables cannot place (`other`) is reported as `unidentified`, never as evidence of a scene. |
| RAM written | the bare RAM symbol that contained the write (`eT3BattleObject1_Timer`) | **the struct field, in bn6f's own words** (`oBattleObject_Timer`), listed before the RAM symbols (`tools/wf/wf/tools/structfields.py`). Only *attributed* writes are quoted -- the base register's struct family has to agree with the field's family -- and at most 8 field names are listed, with `(+N more)` after that. The bare RAM symbols stay in the block, one line lower. |
| call rate | `N calls at the recorded indirect call sites` (the `.edges` count, which only covers indirect sites) | **the CPU entry count**: `CPU entry rate: 2 entries; 0.001 calls per 1,000 frames` -- how often the CPU actually entered the function, measured over the recorded routes (`tools/wf/wf/tools/entry_counts.py`). A route whose entries were not measured is left out; a measured zero is shown as a measurement. |

The rest of the block is unchanged (reached-by, runtime callers, I/O, arguments, returns, unchanged
registers, the blind-spots caveat), and it is still capped at `FACTS_LINES` (25): over the 120
functions the longest block was **12 lines**, the mean **9.2**.

The block for `sub_808A054` -- the function the v1 arm named `battleUpdateBeastAim` while it runs
in the overworld -- reads:

```
Runtime facts (observed by running the ORIGINAL game, not inferred: the recorded routes in
build/facts, and captured calls of it replayed in mGBA):
  scenes it runs in (per captured call): overworld (8 calls)
  reached by 1 of 21 recorded routes; 3 calls at the recorded indirect call sites
  CPU entry rate: 2 entries; 0.001 calls per 1,000 frames
  runtime callers, with the calls they made: dispatch_808A028 (3 calls)
  struct fields it wrote: oCutsceneState_CutsceneScript3LongPauseTimer
  RAM symbols it wrote: byt_3007F78 (96 bytes, 3 values), dword_200A498 (64 bytes, 2 values), ...
  I/O it wrote: none seen (write-only registers are never listed)
  arguments: r0 = 0x0808A055 (8 calls, one value), r1 = 0x0808A048 (8 calls, one value), ...
  returns: r0 = 0x00000001 (8 calls, one value)
  argument registers left unchanged: sp
  Blind spots: only values that CHANGED are listed ...
```

The v1 block for the same function said `scenes it runs in: mixed (beastsp)` and quoted
`eCutsceneState*` as a RAM symbol. The v2 block says *overworld*, names the field
`oCutsceneState_CutsceneScript3LongPauseTimer`, and gives the caller.

### Where the records came from

The richer record format (`scenes_at_call`, `field_writes`, `entries`) is not in this worktree's
`wf facts` yet: it exists in the object DB as commit `27d0bd87` ("wf facts: struct-field writes
... scene per captured call ... exact entry counts per route", branch `wf-burr-demo`), and the
`facts2` worktree carries the matching uncommitted source. The 120 A/B targets were therefore
**re-generated** with that source (no edit to it):

```
WF_FACTS_DIR=.../inv-facts2/build/facts-v2 \
WF_COUNTS_DIR=.../facts2/build/counts2 \
python -m wf facts --only <the 120 names> --states 8 --jobs 6 --limit 0
```

120 records, **0 errors**, 60 captured (60 with per-call scenes), 40 with field writes of which
**18 have attributed ones** (the ones the block quotes), and measured entries for all 120. The
captured calls fall in three scenes: overworld 288, battle 136, menu 56 (480 captured calls of 8
states each). The records live in `build/facts-v2/` (git-ignored, regenerable); the two v2 runs
read them through `$WF_FACTS_DIR`.

## The three arms

The same 120 functions, three arms: **without** facts (the 2026-10-21 runs), **v1** facts (the
2026-10-21 runs) and **v2** facts (the 2026-10-24 runs).

| run id | arm | functions | proposed | probable+agree | tokens |
|---|---|---|---|---|---|
| `investigate-20261021-101004` | A without facts | 60 | 18 | 6 | 109,963 |
| `investigate-20261021-101527` | B without facts | 60 | 37 | 20 | 128,010 |
| `investigate-20261021-100724` | A facts v1 | 60 | 31 | 14 | 177,555 |
| `investigate-20261021-101229` | B facts v1 | 60 | 40 | 19 | 159,712 |
| `investigate-20261024-064512` | A facts **v2** | 60 | 35 | 13 | 175,841 |
| `investigate-20261024-064746` | B facts **v2** | 60 | 47 | 20 | 181,730 |

Both halves together, 120 functions:

| arm | proposed | probable+agree | tokens | per function |
|---|---|---|---|---|
| without facts | 55 | 26 | 237,973 | 1,983 |
| facts v1 | 71 | **33** | 337,267 | 2,810 |
| facts **v2** | **82** | **33** | 357,571 | 2,979 |

* v2 names **11 more functions** than v1 (+15%) for **+6% tokens** (+20,304), and **27 more** than
  no facts (+49%) for +50% tokens.
* v2 and v1 agree on **33 probable+agree** -- the same. The extra names v2 proposed did **not**
  survive their own cross-check as probable ones.
* **32 of v2's 33 probable+agree names are names the no-facts arm did not produce**: 13 are for
  functions the no-facts arm left unnamed, 19 are a different name for a function it did name
  (one name is the same). The two facts arms are unstable with each other too: only 14 of the 33
  probable functions are probable in both v1 and v2, and **none** of those 14 carries the same
  name in the two arms.
* Split on whether the function's calls were captured (so the block can say more than route
  coverage):

| arm | captured calls: named / probable+agree | route coverage only: named / probable+agree |
|---|---|---|
| without facts | 24 / 8 | 31 / 18 |
| facts v1 | 35 / 16 | 36 / 17 |
| facts v2 | **38** / 12 | **44** / **21** |

  v2 names more of the captured half (38 of 60, against 35 for v1 and 24 without facts) but
  **fewer** of them are probable+agree (12 against 16 for v1): the extra names it proposes there
  are the less certain ones. The route-coverage half gains in both arms, and v2's gain there
  (+8 named, +4 probable over v1) comes from the new entry-rate line -- that half has no per-call
  scenes and no field writes, only the route-level line with its caveat.
* **No name survives unchanged between v1 and v2**: of the 62 functions both arms named, **0**
  got the same name (without vs v1: 1 of 45; without vs v2: 1 of 47). The block decides the name,
  so the v2 names are facts-driven by construction.

## Names side by side, against the facts

`pilot/name_quality_ab.py` prints the whole table (95 functions named in at least one arm, each
with the v2 facts hint: per-call scenes, attributed field writes, runtime callers) and counts the
contradictions:

```
python pilot/name_quality_ab.py            # --facts-dir defaults to build/facts-v2
```

**How a contradiction is counted.** A *scene* contradiction: the name's leading word names a
scene of the vocabulary the per-call scenes use (`battle`, `ow`/`overworld`, `menu`/`submenu`,
`cutscene`, `title`) and the captured calls were never in that scene. `battleObject_`/
`battleState_` are struct families, not scenes, and are not counted as scene claims. A *struct*
contradiction: the name's leading word names a struct family (`battleObject`, `owPlayer`,
`chatbox`, `aiAttack`, `gameState`, ...) and none of the attributed field writes belongs to that
family. `CUTSCENE` is counted apart: a `cutscene` name whose calls the mode bytes called
`overworld` -- the cutscene commands are dispatched from the map handler, so the mode byte says
`overworld` while the code is plainly cutscene code. That is a limit of the scene vocabulary, not
a wrong name, so it is reported separately.

Only functions with captured calls can be checked (60 of the 120; the rest have route coverage
only, which says nothing about a scene).

| arm | names proposed | checkable (named & captured) | scene contradictions | CUTSCENE | struct contradictions | any |
|---|---|---|---|---|---|---|
| without facts | 55 | 24 | 2 | 1 | 0 | **3** (12.5% of checkable) |
| facts v1 | 71 | 35 | 5 | 8 | 2 | **15** (42.9%) |
| facts **v2** | 82 | 38 | 5 | 3 | 0 | **8** (21.1%) |

* **v2 halves v1's contradiction rate** (15 -> 8, 42.9% -> 21.1% of the checkable names) and
  removes both struct contradictions. The eight v1 CUTSCENE marks are the bulk of v1's count:
  the v1 block could only say `mixed (beastsp), overworld (story5, story6)` for those functions
  -- a route-level line that names no single scene and contradicts no prefix -- so nothing
  steered the proposer away from a `cutscene_`/`battle_` prefix. The v2 block says
  `overworld (8 calls)` and names the field the call wrote, and the names follow.
* The five v2 scene contradictions are all real in the same sense the v1 pilot's own table used:
  a scene prefix the captured calls never showed.

| function | v2 name | what the facts say |
|---|---|---|
| `sub_80A8674` | `battle_beastSP_init` | overworld; fields `oOverworldMapObject_End/Size/SpriteData_Size`; caller `battleObject_dispatchUpdateByCurAction_80A8654` (276 calls) -- the fields say *overworld map object*, the name says *battle* |
| `sub_80871F4` | `battle_beastSpRefreshState` | overworld (8 calls), never a battle |
| `sub_80A7AE0` | `battle_storeBeastSpAIDataOffset` | overworld (8 calls); caller `battle_beastSpStepper_80A7912` (59 calls) |
| `sub_81261FC` | `battle_drawBcdNumberGrid` | menu (8 calls) -- a chip-number grid, seen only in a menu |
| `sub_81375AC` | `battleHandleBeastSp` | menu (8 calls); fields `oJoypad_Held`, `oJoypad_LowSensitivityHeld` |

* v2 **fixed** the flagship scene error the earlier pilot showed: `sub_808A054` was
  `battleUpdateBeastAim` (v1) against a function that runs in the overworld; v2's checker revised
  it to `cutsceneUpdateFacingDistanceAndLongPause`, citing the field write
  `oCutsceneState_CutsceneScript3LongPauseTimer` in the block. It also dropped the menu claim
  on `sub_809B9EC` (v1 `submenu_applyCursorDelta` -> v2 `object_applyCutsceneMovement`; the
  function is overworld, and the name no longer claims a scene).
* v2's three CUTSCENE marks are `sub_8084214` (`cutscene_getIntroPlayerCoords`), `sub_808B9A0`
  (`cutsceneSetDefaultScriptTableEntry`, writing `oCutsceneState_DefaultScriptTable`) and
  `sub_808A054` (`cutsceneUpdateFacingDistanceAndLongPause`, writing
  `oCutsceneState_CutsceneScript3LongPauseTimer`). In all three the **field family supports the
  name** and only the mode byte disagrees (the cutscene commands run under the overworld gamestate
  byte), which is why they are counted apart from the five scene contradictions.
* The no-facts arm contradicts the facts least (3 of 24) simply because its names are vaguer
  (`object_...`, `sprite_...`, `get_countAdjacentBeastSpEntries`): a generic name cannot name a
  scene that was never seen. Facts make names specific, and a specific name can be wrong.

## Recommendation: **facts on by default -- in the v2 form**

Reasons, in order of weight:

1. **It buys names and probable names.** Over the same 120 functions: 82 names proposed and 33
   probable+agree with facts, against 55 and 26 without. The extra names are concentrated where
   the evidence is (38 of the 60 captured functions named, against 24), and 32 of the 33 probable
   names are names the no-facts arm never produced.
2. **v2 is strictly better than v1 for the same tokens.** +11 names for +6% tokens, the same 33
   probable+agree, less than half the contradictions, no struct contradictions, and the flagship
   scene error is fixed. The v1 block's route-level line was the worst of it: it named no
   single scene, so a wrong scene prefix could not be caught against it; the per-call scenes and
   the struct-field names replace it.
3. **The contradictions that remain are checkable, not silent.** The block shows the model the
   same per-call scene and the same field family a reviewer would check a name against, so a
   contradiction is visible in the block (and the v2 checker did use it, twice). The failure mode
   of a facts-driven name is a wrong *prefix*, which the next step (`wf check-names`) is built to
   catch.
4. **The cost is the same one v1 was accepted at.** ~2,980 tokens per function against ~1,980
   without facts (+50%), for +49% names. The pilot's earlier reading -- "the block is worth its
   tokens" -- survives the v2 measurement.

Caveats to keep in mind when it is switched on:

* The gain still needs captured calls. In the uncaptured half the block is route-level only, and
  its scene line now carries the `(route-level, routes mix scenes)` caveat; the honest thing
  there is to capture more functions (`wf capture`), not to trust the scene line.
* The contradiction rate (21% of checkable names) is still above the no-facts arm's (12.5%):
  facts make names specific, and specific names can be wrong. The cross-check (and later
  `wf check-names`) has to stay in the loop; the block is evidence for the checker, not a verdict.
* `CUTSCENE` marks (3 of v2's 8) are the scene vocabulary's limit, not the model's error: the
  cutscene commands run under the overworld gamestate byte. Naming those functions
  `cutscene_...` is the project's convention and should not be counted against a name.
* Nothing here is landed: no rename, no `symbols/db` entry, no commit. Reviewing and landing is
  the orchestrator's, through the normal path.

## Where the runs and the records are

* the six runs: `build/investigate/investigate-20261021-{100724,101004,101229,101527}` and
  `build/investigate/investigate-20261024-{064512,064746}` in the **main checkout** (`wf.repo.REPO`
  points there, so that is where the run folders are written; they are generated output, and no
  source in the main tree was touched).
* the v2 facts records: `build-wf-worktrees/inv-facts2/build/facts-v2/by_address/*.json` (120
  records, regenerable, git-ignored).
* the analysis: `tools/wf/pilot/name_quality_ab.py` (new, in this worktree).

## Housekeeping

`cport/src/lifted` and `cport/src/lifted_t` in this worktree are **symlinks** into the main
checkout (`/home/alex/Code/recomp/cport/src/lifted`): the worktree's own build output, untracked.
They are build output, not source: they stay untracked and are not part of this change.

The changed files in this worktree are `tools/wf/wf/tools/investigate.py` (the v2 block),
`tools/wf/wf/tools/facts.py` (the richer record fields), `tools/wf/tests/test_investigate.py`
(the fake-record tests for the v2 block), `tools/wf/pilot/name_quality_ab.py` (the table above)
and this note. The full suite passes: **527 passed**.

# v3: a mechanical facts check before the cross-check

The pilot above (v2) found 8 names that contradict the facts (21.1% of the 38 checkable
ones), reported apart from the cutscene-mode-byte caveat. The contradictions have a clear
shape -- a `battle_` name for a function whose captured calls were 8/8 in the overworld, an
`owPlayer_` name for a function that did not write any `oOWPlayerObject_*` field. The v2 block
shows the model the evidence; nothing in v2 stops a contradicting name from being written. v3
adds the missing step: a typed, mechanical check (`wf.tools.facts_check`) that flags the
contradiction, gives the proposer one retry with the contradiction stated, and rejects the
proposal if the retry still contradicts (recorded in `report.tsv`, never landed).

## The check, narrowly

`facts_check.contradictions(name, record)` returns a tuple of `Contradiction(kind, claim,
observed, reason)`. Two kinds:

* `scene`: the name's leading word claims one of the per-call scenes (`battle`, `overworld`,
  `ow`, `menu`, `submenu`, `cutscene`, `title`) and the captured calls have a strict scene
  majority in a different scene. The majority needs `count(claimed) * 2 > total`; a tie has no
  majority and therefore no contradiction, and the `other` mode is never a majority (it's
  reported as `unidentified` in the block).
* `struct`: the name's leading words claim a struct family (recognised prefixes:
  `battleObject`, `battleState`, `owPlayer`, `chatbox`, `joypad`, `overworldMapObject`,
  `aiAttack`, `gameState`, `cutsceneState`, `collisionData`, `camera`, `panelData`,
  `mapScript`) and none of the *attributed* field writes belongs to that family. Only
  attributed writes count (`wf.tools.facts.FieldWrite.attributed` is the gate; un-attributed
  offsets are shared by 30+ structs and so cannot pin a family).

Two carve-outs:

* Recognised struct prefixes take precedence over scene prefixes: `battleObject_` and
  `owPlayer_` are struct families, not scene claims (the `battleObject_` family covers
  `oBattleObject`, `oT1BattleObject`, `oT3BattleObject`, `oT4BattleObject`).
* The cutscene mode-byte caveat stays. A `cutscene_` name's contradiction is held back when
  the field family supports the cutscene prefix (the cutscene commands run under the
  overworld gamestate byte, so the mode byte reads `overworld` while the code is plainly
  cutscene code -- the field is the stronger evidence).

Route coverage never contradicts a name. It says where a recording, not what scene each call
of this function, ran in.  The check only reads `scenes_at_call` (per-call) and attributed
`field_writes`; the v2 block's `scenes it runs in: battle (battlesweep)` route-level line
(with its `(route-level, routes mix scenes)` caveat) cannot be refuted, and is not used.

## What the run does

`ask_investigate` (`tools/wf/wf/tools/investigate.py`) checks the proposal's name against
`facts_check.contradictions(name, facts_record(address))`. A context whose `facts` field is
empty (a `--no-facts` run, or any sub-record that never landed) is never checked; neither is
a function with no record. When the check sees a contradiction:

1. `retries += 1` (the run's `retries` column counts both an unusable-reply retry and a
   facts-retry now).
2. The retry prompt states the contradiction verbatim and asks the model to propose a
   different name (or `proposed_name: null` with the contradiction in `open_questions`).
3. If the retry's name still contradicts the facts, the proposal is marked
   `rejection = ToolMessage("rejected by the runtime-facts check: ...")` and never sent to
   the checker. The candidate `*.yaml` is not written; a row in `report.tsv` says
   `status = rejected` with the reason. The run's `Report` adds a `rejected` field; the
   totals line says `N rejected by runtime facts`.

The proposal cache (`run_dir/proposals/NAME.json`) keeps `facts_checked: bool`; with
`--facts` an old cached proposal is only reused when the previous run also went through the
check (which means a restart of a partial run keeps the contradiction-aware proposals and
does not re-ask them).

## The four arms

The same 120 functions (`pilot/ab-armA.txt`, `pilot/ab-armB.txt`); v3 with `wf`'s test
infrastructure, the other three unchanged. The model is `hyper/minimax-m3` for both roles in
v3 (the brief: MiniMax direct is rate-limited today); the other three used
`minimax/MiniMax-M3`. `--jobs 12`.

| arm | both | proposed | probable+agree | tokens | per function |
|---|---|---|---|---|---|
| without | — | 55 | 26 | 237,973 | 1,983 |
| facts v1 | — | 71 | 33 | 337,267 | 2,810 |
| facts v2 | — | 82 | 33 | 357,571 | 2,979 |
| facts **v3** (this run) | — | **63** | **48** | **1,034,248** | 8,618 |

| run id | arm | selected | proposed | probable+agree | rejected | tokens |
|---|---|---|---|---|---|---|
| `investigate-20261021-101004` | A without | 60 | 18 | 6 | — | 109,963 |
| `investigate-20261021-101527` | B without | 60 | 37 | 20 | — | 128,010 |
| `investigate-20261021-100724` | A v1 | 60 | 31 | 14 | — | 177,555 |
| `investigate-20261021-101229` | B v1 | 60 | 40 | 19 | — | 159,712 |
| `investigate-20261024-064512` | A v2 | 60 | 35 | 13 | — | 175,841 |
| `investigate-20261024-064746` | B v2 | 60 | 47 | 20 | — | 181,730 |
| `investigate-v3-armA` | A v3 | 60 | 32 | 21 | 0 | 583,728 |
| `investigate-v3-armB` | B v3 | 60 | 31 | 27 | 1 | 450,520 |

The v3 tokens are higher partly because the model response is longer
(`hyper/minimax-m3` vs `minimax/MiniMax-M3`), partly because 8 retries (5 in armA, 3 in armB,
8 successes + 1 last-resort rejection) added a second round of investigation per contradiction,
and partly because the hyper route is more verbose. The flat inefficiency, ~8,600
tokens/fn, outweighs v2's ~3,000 but the gain is real: 48 probable+agree vs 33, and 0–1
contradictions out of 41 checkable.

Split on whether the function's calls were captured:

| arm | captured: named / probable+agree | uncaptured: named / probable+agree |
|---|---|---|
| without | 24 / 8 | 31 / 18 |
| v1 | 35 / 16 | 36 / 17 |
| v2 | 38 / 12 | 44 / 21 |
| **v3** | **41** / **30** | 22 / 18 |

The captured half is where the block has something to contradict a name against. v3 names
more of the captured half (41 of 60, against 38 for v2 and 24 without) and **most of those
land probable+agree** (30 of 41, against 12 for v2 and 8 without). The uncaptured half drops
in v3: with a check that flags `scene` claims against the per-call majority, the model now
predictably hedges when the block carries no `scenes_at_call` (only the route-level line).
That is the right behaviour: a `cutscene_` prefix on a route-level-only function is a guess;
before v3 the guess was uncaught.

## Name quality: contradictions

| arm | named | checkable (named & captured) | `scene` | `struct` | any |
|---|---|---|---|---|---|
| without | 55 | 24 | 3 | 0 | **3** (12.5%) |
| v1 | 71 | 35 | 10 | 2 | **12** (34.3%) |
| v2 | 82 | 38 | 6 | 0 | **6** (15.8%) |
| **v3** | 63 | 41 | 1 | 0 | **1** (2.4%) |

The single v3 mark is `sub_81375AC battleBeast_spMenuAutoSelect`: the function's
`scenes_at_call` is 8 of 8 menu, the name's leading word claims `battle`. The proposer
proposed the same prefix twice; `facts_check.contradictions` caught it both times; the
second attempt was rejected, recorded in `report.tsv` with the reason, and never landed.
The other 8 contradiction hits in v3 (5 in armA, 3 in armB aside from the rejected one)
were recovered from -- the model saw the contradiction in its retry prompt, proposed a
different prefix, and the checker agreed.

The eight v2 marks are gone:

* `sub_80A8674` (v2: `battle_beastSP_init`, overworld) -- v3 armA: `owMapObject_writeAndCommitENtryAndSize`. The retry gave the model the contradiction (overworld map object fields, 8/8 overworld calls), and the new name no longer claims a scene.
* `sub_80871F4` (v2: `battle_beastSpRefreshState`, overworld) -- v3 armA: `owMapObject_packBeastSpState`.
* `sub_80A7AE0` (v2: `battle_storeBeastSpAIDataOffset`, overworld, caller `battle_beastSpStepper_80A7912`) -- v3 armA: same example as retry path; the retry dropped the `battle_` prefix.
* `sub_81261FC` (v2: `battle_drawBcdNumberGrid`, menu) -- v3 armB: `menu_drawBcdNumberGrid` (the retry kept the `menu_` prefix and dropped the `battle_`).
* `sub_8084214`, `sub_808B9A0`, `sub_808A054` (v2's CUTSCENE marks) -- v3 had the field family supporting the cutscene prefix on these, so they are not flagged.

`sub_80A8674`, the flagship error in both v1 and v2, finally names itself correctly: the
fields `oOverworldMapObject_End/Size/SpriteData_Size` and the per-call overworld majority
agree.

## The same function across four arms

| pair | both named | same name | different |
|---|---|---|---|
| without vs v1 | 45 | 1 | 44 |
| without vs v2 | 47 | 1 | 46 |
| without vs v3 | 32 | 0 | 32 |
| v1 vs v2 | 62 | 0 | 62 |
| v1 vs v3 | 45 | 1 | 44 |
| v2 vs v3 | 50 | 0 | 50 |

The 0-same-name pairs (without/v3, v1/v3, v2/v3) say what v3 is: the check is decisive, and
the proposer now picks a name the checker agrees on, so the run's names are systematically
different from any earlier arm's. The single 1-same-name in v1/v3 is `sub_80A8674`
(`owMapObject_writeAndCommitENtryAndSize` -- v1's name was `owMap_packingRoutines`);
otherwise every v3 name is a v3-only decision.

## Recommendation: **facts on by default -- in the v3 form**

The v2 recommendation stood; the v3 measurement strengthens the case on the two metrics the
brief asked for (probable+agree ≥ 30, contradictions ≤ no-facts' rate) and adds a third --
1 contradiction across 41 checkable names, recoverable on one retry, recorded when it isn't.

1. **probable+agree ≥ 30**: 48. The bare minimum (33) was already met by v2; v3 adds 15 on
   top with the same 60 functions and the same input evidence.
2. **contradictions ≤ no-facts' rate (12.5%)**: 2.4%. v3 is below the no-facts rate by a
   fifth, not just below v2's 15.8%.

The cost goes up: ~8,600 tokens/fn against v2's ~3,000. But the 4 arm comparison vs the
no-facts arm is now +85% probable+agree for ~+330% tokens, vs v2's +27% for +50%. The
higher absolute cost buys a higher absolute gain. Where the earlier pilot could only say
"the block is worth its tokens", v3 can say "the block plus the check is worth much more
than the block alone".

The pre-check recovery rate (8 of 9 hits landed a non-contradicting name) means a future
model with better priors can probably do even better; the check itself is the bottleneck
now that the noise is gone.

Caveats, unchanged from v2:

* The gain is still in the captured-calls half. Functions whose calls were never captured
  still get the route-level scenes line with `(route-level, routes mix scenes)`; the prose
  still says nothing about which calls ran in which scene. The honest move is to capture
  more functions (`wf capture`), not to weaken the check.
* A `cutscene_` name whose `scenes_at_call` is overworld stays unflagged when the
  attributed field family supports the cutscene prefix -- the cutscene commands run under
  the overworld gamestate byte, and the field is the stronger evidence.
* Nothing here was landed: no rename, no `symbols/db` entry, no commit. Reviewing the
  candidates and landing them is the orchestrator's, through the normal path. The
  rejection (`sub_81375AC`) did not produce a candidate `*.yaml`.

## Decision

`--facts` becomes the default. `wf investigate --no-facts` still opts out (kept for A/B
pilots and for functions whose block would be misleading, e.g. before a `wf capture` pass).

| change | file | why |
|---|---|---|
| `gather: bool = True` (was `False`) | `wf/graphs/investigate.py` | run state default |
| `--facts` default `True`, `--no-facts` still `False` | `wf/__main__.py` | CLI default + help text updated |
| the check itself | `wf/tools/facts_check.py` (new) | mechanical, typed, in the wf package; tests in `tests/test_facts_check.py` |
| `ask_investigate` retry on contradiction, `Report.rejected`, `report_rows` column | `wf/tools/investigate.py` | the run does the check and records rejections |
| `write_the_candidates` skips rejected proposals (recorded in `report.tsv`, not landed) | `wf/graphs/investigate.py` | the rejection is visible to the reviewer without polluting the candidates dir |
| `investigate.record_rows` adds a `rejection` column | `wf/tools/investigate.py` | the reviewer sees which name was refused and why |
| pilot reads the four arms (without/v1/v2/v3); v3 lives in this worktree's `build/investigate` | `pilot/name_quality_ab.py` | the table above |
| tests for the check | `tests/test_facts_check.py` (new) | seven cases over fake records (route-level facts never contradict; per-call majority; unattributed writes don't contradict; struct precedence over scene; cutscene mode-byte caveat) |
| tests for the retry path and the rejected proposal | `tests/test_investigate.py` | two cases (`test_a_contradicting_name_gets_one_retry_with_the_contradiction`, `test_a_name_that_still_contradicts_is_rejected_and_not_checked`, `test_a_facts_rejection_is_recorded_but_not_written_as_a_candidate`) |
| `test_the_graph_gathers_the_facts_only_when_asked` updated | `tests/test_investigate.py` | the default flipped |
| section above | `docs/workflows/investigate-facts-notes.md` | measurement, recommendation and the carve-outs |

## Where the runs are

* the eight runs: `build/investigate/investigate-20261021-{100724,101004,101229,101527}` and
  `investigate-20261024-{064512,064746}` in the **main checkout**, `build/investigate/investigate-v3-armA`
  and `investigate-v3-armB` in this worktree (`wf.repo.REPO` was set to it so the candidates
  stay out of the main tree).
* the v2 facts records: `build/facts-v2/by_address/` (120 records, regenerable, git-ignored).
* the analysis: `pilot/name_quality_ab.py`.

## Housekeeping

`cport/src/lifted`, `cport/src/lifted_t`, `bn6f/`, and `build-snapshots/` in this worktree
are symlinks into the main checkout (where the lifter's output, the disassembly, the ELF,
and the captured snapshots live); they are not source under git, and they are not part of
this change. They were needed because `wf.repo.REPO` was pointed here so the run's
candidates stay in this worktree.

The v3 changes are `tools/wf/wf/tools/facts_check.py` (new, the typed check),
`tools/wf/wf/tools/investigate.py` (the retry-on-contradiction in `ask_investigate`; the
`rejection` fields on `Proposal` and `Report`; the `report_rows` column),
`tools/wf/wf/graphs/investigate.py` (the `rejected` count in the write step; the default
flip on `InvestigateState.gather`), `tools/wf/wf/__main__.py` (the `--facts`/`--no-facts`
default and the docstring), `tools/wf/tests/test_facts_check.py` (new), additional tests
in `tools/wf/tests/test_investigate.py`, and `tools/wf/pilot/name_quality_ab.py`
(refactored to read the four arms, with v3 living under this worktree). The full suite
passes: **548 passed**.
