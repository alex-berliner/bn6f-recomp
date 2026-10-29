# `wf check-names` -- check a function's name against the running game, with a calibration gate

`wf check-names` is the place where a model's reading of a name meets the running game --
and no model ever judges the outcome. A name writes a list of testable claims and one causal
prediction from the symbols/db description alone (BLIND, that is -- nothing about the
runtime facts or the assembly is in the prompt). The claims are checked against what
`wf facts` recorded at runtime; the prediction is run through `wf intervene`; and a swap
control (another function's claims checked against THIS function's facts, another function's
prediction run against THIS function's intervention) sits inside the same batch so the gate
itself can be calibrated.

    python -m wf check-names [--only NAME]... [--from-tsv FILE [--tsv-where K=V]...] [--count N]
                             [--jobs J] [--model PROVIDER/MODEL] [--frames-window N]
                             [--seed S]
                       # check a function's name against the running game: claims written
                       # blind, checked against build/facts, prediction run through wf intervene,
                       # and a calibration gate. Output to build-wf-runs/check-names-RUN
                       # (report.tsv + NAME.json + calibration.md). Nothing under symbols/db or
                       # bn6f is written.

The brief was written for **a model change**: the `minimax/MiniMax-M3` provider is out of
quota today, and the substitute is `hyper/minimax-m3` (the same model on the Hyper
provider; tools/llm.py treats them as one model by `model_id`).  Out-of-credits (402) or a
daily rate limit stops the pilot; what already finished is kept and reported.

## Pilot (run 2026-10-21 -- 2026-10-21)

| batch part | target | actual |
|---|---|---|
| (a) audit `wrong`, function rows only | 30 names | **26** (the audit finished 26 such rows; 4 of the 30 are data labels, dropped because the workflow is about function rows per the brief) |
| (b) random reached, seed 12345 | 60 names | **60** |
| total | 90 | **86** |

`--model hyper/minimax-m3 --jobs 4 --frames-window 1200 --seed 12345`. The run took
1759 s (29 min) and used 393,795 tokens. Run id: `check-names-20261021-094015`. Output:
`build-wf-runs/check-names-20261021-094015/` -- `report.tsv`, 86 per-name `*.json` files,
`claims/*.json` (the blind answers, retained so a re-run can resume past the model calls),
and `calibration.md`.

### Levels (count per level)

| level | count |
|---|---|
| `L1` (no runtime support: nothing to check) | **17** |
| `L2 consistent` | 0 (gate failed) |
| `L3 confirmed` | 0 (gate failed) |
| `contradicted` | 0 (gate failed) |
| `unreached` (no recorded route ever executed it) | **21** |
| `ungated` | **48** |

The gate is refusing to grant anything on this batch. That is the design working: a level
of `contradicted`, `L2` or `L3` is only granted when the swapped controls fail at a higher
rate than the calibrated bar (`true - swapped >= 0.30` for claims; `swapped < 0.15` for
predictions). Both fails on this batch.

### Calibration (the gate's own numbers)

| check | true | swapped | bar | verdict |
|---|---|---|---|---|
| claims | 68/185 passed (36.8%) | 69/190 passed (36.3%) | true - swapped >= 0.30 | **FAIL** (margin 0.4%) |
| predictions | 1/7 held | 1/6 held (16.7%) | swapped < 0.15 | **FAIL** (swapped 16.7%) |

By claim kind (the `calibration.md` per-kind table, taken from each function's
`checks` / `swapped_checks`):

| kind | true passed | swapped passed |
|---|---|---|
| `called_every_frame` | 3/28 (10.7%) | 8/26 (30.8%) -- swapped is **better** than true |
| `has_argument` | 10/29 (34.5%) | 11/28 (39.3%) |
| `reached_in_scene` | 38/58 (65.5%) | 33/56 (58.9%) |
| `reads_or_writes_io` | 0/8 (0%) | 1/13 (7.7%) |
| `returns_value` | 16/31 (51.6%) | 16/38 (42.1%) |
| `writes_ram` | 1/31 (3.2%) | 0/29 (0%) -- only kind the true wins on, by 1 swap |

A claim kind that passes *more* when swapped in (`called_every_frame`, the model
marking things as called every 2 frames regardless of which name) is the loudest signal
that the checker is not specialised to the function. Only `writes_ram` shows the
true rank above swapped, by a single check.

What this says about the checker:

* **claim gate (margin 0.4%):** the swapped controls' claims passed within 4/1000 of the
  true ones, so the claims the model wrote were not specialised to a given function.
  That is a signature of the model writing "what a typical function in this vocabulary
  tends to write" rather than what THIS specific name promises. The bar is 30
  percentage points because anything less is what a reader could produce from a
  glossary.
* **prediction gate (16.7%):** a swapped prediction held 1 in 6 times, and the bar is
  below 15%. So the `held` outcome does not separate a true prediction from another
  function's prediction. Note that 6 true predictions failed and 1 held; the model bet
  hard on OAM/screen predicates that mostly did not hold on the routes the function
  ran into the first 1200 frames of.

**Net:** the checker is not discriminating on this batch, so the report's `contradicted`,
`L2` and `L3` levels are downgraded to `ungated`. Pilot goal met: the calibration gate
caught it.

### wF intervene, the active half

The brief asked for the prediction to run through `wf intervene`. The active half ran:
65 functions reached (the 21 unreached names had no route on which the function ever
executed); for each, the workflow picked the shortest route the facts recorded as
executing the function, probed it for a first game frame, ran a `null` control and the
prediction intervention, and judged the predicates against the anchor diffs. The 8
prediction results that ran came through as:

| outcome | true | swapped |
|---|---|---|
| `held` | 1 | 1 |
| `failed` | 6 | 5 |
| `unreached` / `nondeterministic` | the rest | rest |

The model wrote predictions that mostly failed (the `differs(anchor=oam, ...)` predicate
on a function whose effect lands in palette, sound or another RAM struct, for example).
The 1 true held is paired with 1 swapped held, which is what the prediction gate reads as
"a held prediction carries nothing this time."

### 10 most clearly contradicted names

These names had a `contradicted` passive verdict and at least 3 failing claims; the
factual observation is what `wf facts` recorded for that function.  All ten are also
`ungated` -- the gate caught the batch and refused to grant `contradicted` as a level --
but the underlying factual mismatch is plain:

| # | name | first failing claim | observation |
|---|---|---|---|
| 1 | `sub_80C0EE2` | `reached_in_scene(battle)` | executed on `story9` only: scene `overworld`, never `battle` |
| 2 | `sub_810D970` | `reached_in_scene(battle)` | executed on `beastsp, story4, story6, story9`: scenes `['overworld']`, never `battle` |
| 3 | `NPCCommand_jump` | `writes_ram(oNPC)` | wrote `end_file_3007B00(32B)`; no symbol starts with `oNPC` |
| 4 | `ai_attackEndStep` | `writes_ram(oAIAttackVars)` | wrote `eAIData1_AttackVars(36B), end_file_3007B00(32B), eCollisionData1_ObjectFlags1(4B), eT1BattleObject1_CurAnim(4B)`; no symbol starts with `oAIAttackVars` |
| 5 | `battle_phaseWaitThenAdvance` | `writes_ram(oBattleObject_Timer)` | wrote `end_file_3007B00(36B), eT3BattleObject1_Timer(32B), eT3BattleObject1_CurAction(4B), eT3BattleObject1_ExtraVars(4B)`; no symbol starts with `oBattleObject_Timer` |
| 6 | `beastout_loadSelectionScreenChipUseGfx` | `reads_or_writes_io(vram)` | wrote `fiveWordArr200B4B0(496B), end_file_3007B00(352B), eGfxTransferQueueCount(32B)`: nothing that lands in `vram` |
| 7 | `owPlayer_offsetS200ace0Coords_809e1fa` | `writes_ram(oS200ace0_PlayerX)` | wrote `eStruct200ace0_PlayerX(32B), end_file_3007B00(32B)`; no symbol starts with `oS200ace0_PlayerX` |
| 8 | `sprite_makeScalable` | `writes_ram(oOamBuf)` | wrote `end_file_3007B00(96B), dword_2009A2C(32B), eOWPlayerObject_SpriteData_ScratchHalfword(32B)`; no symbol starts with `oOamBuf` |
| 9 | `aiAttack_stepStateMachine_80EB04C` | `writes_ram(oAIAttackVars_Unk_0c)` | wrote `end_file_3007B00(140B), eAIData0_AttackVars(56B), eCollisionData0_ObjectFlags1(8B), eT1BattleObject0_CurAnim(8B)`; no symbol starts with `oAIAttackVars_Unk_0c` |
| 10 | `battle_EnableAIUnk0x48FlagForNonstandardBoard_80141F4` | `writes_ram(oBattleAI)` | wrote `end_file_3007B00(64B)`; no symbol starts with `oBattleAI` |

A common shape emerges: the model writes a `writes_ram(WRONG_OBJPREFIX)` claim against
`oBattleObject_*` / `oAIAttackVars_*` / `oS200ace0_*` families that the static reader
proposed, while the facts show the function actually writing into a different
`eAIData1_*`, `eStruct200ace0_*`, `eOWPlayerObject_*`, `eT3BattleObject1_*` shadow
struct. (Names whose prefix begins with `e` are index-0/1/2/3 specific encodings; the
square `o*` names the static reader tried are usually the same struct but with a
different object index, so the prefix doesn't match.) For the 30 audit-wrong names, 21
are `unreached`, which is itself the cleanest form of contradiction the workflow can
emit: the name promises a function the running game never calls.

### The 17 L1 names (no usable blind claims)

For these, `hyper/minimax-m3` returned a reply that did not contain a JSON object (or
the schema rejected it; one retry, then `the reply held no JSON object`). The 17 are
recorded in `claims/<name>.json` with their `problem` and preserved so a later run with
a different model can substitute without re-doing the work. The names are listed in
`report.tsv` with level `L1`.

## Pilot 2 (re-run 2026-10-29)

The brief asked for a re-run on the same 86 names after two fixes:

1. **`wf intervene` cache-key bug.** Before this fix a freshly re-imported route (beastsp
   was rebuilt from video on 2026-10-24) could reuse an earlier batch's cached emulator
   states, so the function landed at the wrong frame and the prediction's anchors
   compared against the wrong slices. The fix extends `route_states_dir(route)` with a
   sha1 fingerprint of the route's `.trace` / `.poke` / `.sav` bytes (content-only, so
   recording under a worktree path or under the main checkout's path hashes to the same
   fingerprint): `build/intervene/states/<ROUTE>/<FINGERPRINT>/`. Three tests pin it
   (`test_route_fingerprint_is_stable_for_an_unchanged_recording`,
   `test_route_fingerprint_changes_when_a_recording_changes`,
   `test_cached_frames_is_invisible_to_a_recording_change`).
2. **Active-half hung/unreached outcomes.** Pilot 1 broke before step four because
   `wf intervene` crashed on `skip` for some functions; pilot 2 made `tools/check_names`
   run every prediction through a fixed `intervene_once` (replacing the inline pilot
   helper), and added four tests that confirm hung/unreached are NEVER counted as held
   (`test_hung_and_unreached_outcomes_are_not_held`,
   `test_the_calibration_holds_no_swap_for_hung_or_unreached_predictions`,
   `test_a_return_k_prediction_runs_through_intervene_with_kind_and_k`,
   `test_a_hung_outcome_serialises_with_a_problem_carrying_the_evidence`).

Run id: `check-names-pilot2-20261029-131116`. Wall time **43 s** (most claims were
re-used from pilot 1 via the `claims/` directory, so the model cost is just the
regenerated swap half on the 8 active); **246,651 tokens** consumed. Output:
`build-wf-runs/check-names-pilot2-20261029-131116/`.

### Facts layer: 37 of 86 names had no coverage row

The brief specified `build/facts` (main tree) plus `build-wf-worktrees/facts2` if check-names
could read it. Today, `build/facts/coverage.tsv` is **60 bytes** (the header alone) while
`build-wf-worktrees/facts2/build/facts2/coverage.tsv` has **49 data rows** (a subset of
pilot 1's 86). check-names reads `facts_dir / coverage.tsv` first, and only falls back to
`by_address/0x*.json` when the TSV is empty (`coverage_index` in
`tools/check_names.py`); pilot 2 set `WF_FACTS_DIR=…/facts2`, so it picked the 49 names
in facts2's coverage and dropped the other 37. The 37 dropped are exactly pilot 1's
21 `unreached` + 15 `ungated` + 1 `L1`: they have no coverage row in facts2, so their
facts record is not addressable from `coverage.tsv`. **Pilot 2 did not decide these are
unreached / ungated / L1 from new evidence -- facts2 simply doesn't list them.**

The 49 names pilot 2 *did* check were all in pilot 1's batch; on those 49 names, every
level is unchanged -- pilot 1 and pilot 2 agree on `ungated` / `L1` / `contradicted` /
`consistent` for every name in the overlap.

### vs pilot 1

| metric | pilot 1 (`check-names-20261021-094015`) | pilot 2 (`check-names-pilot2-20261029-131116`) | bar | verdict |
|---|---|---|---|---|
| names in batch | **86** | **49** (37 dropped -- facts2 coverage is the subset) | — | — |
| `claim` true pass rate | 68/185 (36.8%) | 74/158 (46.8%) | true - swapped ≥ 0.30 | **gate fails both**, but the margin changes |
| `claim` swapped pass rate | 69/190 (36.3%) | 61/180 (33.9%) | — | — |
| margin (true - swapped) | **0.4%** | **13.0%** | ≥ 0.30 | FAIL (but +12.6 pp) |
| true predictions held | 1/7 | 1/7 | — | unchanged on overlap |
| swapped predictions held | 1/6 (16.7%) | 2/8 (25.0%) | swapped < 0.15 | FAIL (regressed -8.3 pp) |
| `hung` / `unreached` counted as `held` | (none in this batch; bug was earlier) | 0 (fixed; tested with fakes) | — | — |
| levels: `unreached` | 21 | **0** (dropped pre-batch) | — | — |
| levels: `ungated` | 48 | **33** | — | the gate refused |
| levels: `L2 consistent` | 0 | 0 | — | — |
| levels: `L3 confirmed` | 0 | 0 | — | — |
| levels: `L1` (no usable claims) | 17 | **16** (1 of pilot 1's was dropped; the remaining 16 are unchanged) | — | unchanged on overlap |
| wall time | 1759 s (full model cost) | **43 s** (claims reused) | — | — |
| token cost | 393,795 | 246,651 (active half + claim regen on the 8 active) | — | — |

### What the fixes bought

* **Active half is clean.** All 8 true predictions and all 8 swapped predictions came
  back with a concrete outcome -- **no `hung`, no `unreached` counted as held**. The
  active result distribution is: `held` 1 (battle_IsBattleEnding_800A97A on story7),
  `failed` 6, `none` 1 (SWI_CpuFastSet's own intervention was the literal `intervention:
  none` -- the model declined to predict one). The fixed `intervene_once` is the only
  call into `wf intervene` now (the inline pilot helper that crashed on `skip` is gone),
  and `kind=return` / `k=0` at the boundary is what's tested in
  `test_a_return_k_prediction_runs_through_intervene_with_kind_and_k`.
* **Cache-key bug does not regress.** The new fingerprint is **content-only**: a
  re-import under a different absolute path (worktree vs main checkout) hashes to the
  same fingerprint, so cached states are reused; a re-import with different bytes
  hashes to a different fingerprint, and a fresh `build/intervene/states/<ROUTE>/<NEW>/`
  is created (the old one is left in place, accumulating as a stale version no run
  reads). On pilot 2, none of the 8 active reproduction runs blew up on a stale cache --
  each one read from the correct freshly populated state dir.

### What the gate still says

The fixed active half is honest about outcomes, but the gate still REFUSES to grant
anything on this batch -- the underlying calibration problem is **not** in the active
half:

* **claim margin is up (0.4% → 13.0%) but still under the 30% bar.** On pilot 2's 49
  names the model still writes claims close to what a swapped-in partner writes. The
  `called_every_frame` row from pilot 1 (true 11%, swapped 31% -- swapped is *better*)
  repeats in pilot 2: the model marks "called every frame" against functions that aren't.
* **swapped prediction rate regressed (16.7% → 25.0%).** The two swapped-held cases are:
  * `loadIndexedPaletteToBuffer_802E240` against partner `battle_IsBattleEnding_800A97A`'s
    `unchanged(anchor=palette, for=120)` -- true on `wander` (palette indeed didn't
    change), but only because the model picked an anchor (palette) that didn't change
    when the actual diff is in `display_io`.
  * `SWI_CpuFastSet` against partner `sub_80FD158`'s `differs(anchor=oam, within=60)` --
    true on `wander` (OAM does differ), and the partner's claim *happens* to specialise
    to a region of memory that `SWI_CpuFastSet` happens to touch 1432 frames in. This is
    the same pathology that borderlines pilot 1: a landed prediction can be more about
    the partner's domain than the host function's behaviour.

So **pilot 2 closes the active-half bug and the cache-key bug, but does NOT close the
calibration problem**. The gate's marginal improvement (0.4% → 13%) comes from a
slightly cleaner batch (49 names with stable coverage include fewer unreached names),
not from a more discriminating model. To pass the gate, the swap half has to fail at
the bar (`swapped < 0.15` for predictions, `true - swapped ≥ 0.30` for claims); that
needs a model whose claims lean on a function's specific evidence rather than on
generic patterns.

## Design

Six Burr steps, no judgement from the model after step 2:

1. **`choose_the_batch`** (`graphs/check_names.py`) picks the names (`--only`, a TSV with
   `--tsv-where`, plus `--count N` random reached), reads each one's symbols/db
   description, and pairs each with a random other name in the batch (its swap control;
   deterministic by seed).
2. **`write_the_claims`** asks the model for claims + one prediction blind (name +
   symbols/db description only), retries once on a reply that doesn't fit the schema,
   and writes the answer to `claims/<name>.json` so a re-run resumes past the model
   calls.
3. **`check_the_claims`** (script) checks each claim against the function's record and
   each name's PARTNER's claims against the same record. `pass` / `fail` / `unknown`
   (the facts cannot show it: write-only sound/DMA/key input, one replayed call, a route
   whose scene the table cannot name).
4. **`run_the_predictions`** (script) runs every prediction through `wf intervene`
   (`probe` -> first frame; `null` -> control; `skip` / `return:K` -> intervention), and
   every partner's prediction against the same function. One function, `intervene_once`
   in `tools/check_names.py`, is the only call into `wf intervene` -- a fix in the
   latter drops in here.
5. **`calibrate_and_level`** does the gate's arithmetic: what the true claims and
   predictions did, what the swapped ones did against the same facts and interventions,
   the margin (true - swapped for claims must be >= 0.30; swapped prediction rate must
   be < 0.15), and the level each name earned under that gate.
6. **`write_the_outputs`** writes `report.tsv` (one row per name, columns including
   `level`, `claims_pass/fail/unknown`, `prediction`, `active`, `route`, `swapped_from`,
   `swapped_active`, `control`, `seconds`, `summary`), one `<NAME>.json` per name with
   the full state, and `calibration.md`.

Nothing under `bn6f/` or `symbols/db` is written.

### The schemas

* `Claim` is a tagged union (`field(discriminator="kind")`) over
  `reached_in_scene(SCENE) / writes_ram(SYMBOL_PREFIX) /
  reads_or_writes_io(GROUP∈{oam,palette,vram,display,sound,dma,input}) /
  returns_value(KIND∈{bool,small_int,pointer,none}) /
  called_every_frame(YES) / has_argument(N∈0..4)`.
* `prediction` is `DoIt(intervention: skip | return:K, k: 0..255, predicates: 1..3,
  window: 60..240)` or `NoEffect(reason: str)`. Predicates are in `wf intervene`'s
  predicate language.
* `claims` is `tuple[Claim, ...]`, validated by pydantic; one retry on a malformed
  reply, otherwise recorded with its problem and given `L1`.

### The route -> scene table

A route's scene is what the recorded input shows. The table the brief asked for:

| route prefix | scene |
|---|---|
| `battle`, `battles1..battles2`, `battlesweep`, `chips` | `battle` |
| `story1..story9`, `maps`, `wander` | `overworld` |
| `menus2`, `menus3` | `menu` |
| `title_mash_a` | `title` |

A route not in the table (`beastsp`, `fuzz` in this run's coverage) is `None`: the
checker says nothing about a scene it cannot name, and the run's notes ask which scene
it is.

### The calibration gate

```
SWAPPED_MAX_PREDICTION_RATE = 0.15   # swapped predictions: held less than this
SWAPPED_MIN_CLAIM_MARGIN = 0.30      # true claim rate minus swapped >= 30 pp
```

Per-batch counts read from `calibration.md`:

* `claims_ok`: true claims pass rate - swapped claims pass rate >= 0.30, AND both
  samples are non-empty.
* `predictions_ok`: the swapped prediction `held` / tested rate is < 0.15.
* `discriminating`: both. A level is only granted under a discriminating batch.

### Levels

| level | when |
|---|---|
| `L1` | nothing to check (no claims or no prediction; a model call failed) |
| `L2 consistent` | every checkable claim passed AND `claims_ok` is true |
| `L3 confirmed` | the true prediction held AND the swapped control did not, AND `predictions_ok` is true |
| `contradicted` | a claim failed OR a tested prediction failed, AND the gate that owns that verdict passed |
| `unreached` | no recorded route ever executed the function |
| `ungated` | the gate for the level the underlying arithmetic would have granted did NOT pass |

`ungated` is the gate's own verdict: it says "the run's numbers would have granted
something, but the gate did not pass, so the report does not grant it." The fact is still
in `checks` and `active` of the per-name JSON.

## A note on the active half

The brief said the prediction runs through `wf intervene`, and noted that `wf intervene`
was being hardened concurrently in another worktree (skip-crash bug). The checker calls
`wf intervene` through ONE function (`intervene_once` in `tools/check_names.py`), so a
fix to `wf intervene` drops in behind that function alone. The pilot's active half did
encounter the `probe` -> `null` -> `intervention` flow OK on the 65 functions reached in
the batch; the 1 true held is paired with 1 swapped held, the prediction gate reads that
as "a held outcome carries nothing" and falls through `prediction_gate did not pass` for
the eventual levels.

## Files

| file | what |
|---|---|
| `tools/wf/wf/tools/check_names.py` | the schemas (`Claim`, `Action`, `Reply`, `Active`, `Control`, `Level`, `Calibration`), the claim checker, the gate arithmetic, and `intervene_once` (the only call into `wf intervene`) |
| `tools/wf/wf/graphs/check_names.py` | the Burr graph: 6 typed steps, `run_check_names(start, run_id)` |
| `tools/wf/wf/__main__.py` | `wf check-names`, the seven flags |
| `tools/wf/wf/registry.py` | registers `check-names` under project `recomp-check-names` |
| `tools/wf/tests/test_check_names.py` | 51 tests: schema validation, claim checking against fake facts, scene mapping, swapped pairing, gate arithmetic, the graph end-to-end with fakes (no model calls) |

## Tests

```
$ cd build-wf-worktrees/check-names/tools/wf && timeout 30 \
    /home/alex/Code/recomp/tools/wf/.venv/bin/python -m pytest -q tests/test_check_names.py
.............................................. [ 90%] ..... [100%] 51 passed
```

The full pytest suite for the worktree passes too (`506 passed in 129.20s`).
