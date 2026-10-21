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