# Symbol-naming and farm-integration campaign, 2026-10-18 to 2026-10-21

Reviewer: minimax/MiniMax-M3. Numbers in this document come only from commands run against
`/home/alex/Code/recomp`; the command that produced each is named at the point of use.
Where a number could not be computed cleanly, the report says so and skips it.

The campaign was four days of `wf investigate` and `wf investigate-data` runs (cheap Hyper
`glm-5.3-flash` proposes, then `minimax/MiniMax-M3` checks), the four rescue passes in
`tools/investigate_land.py` and `tools/investigate_data_land.py` (style-fix, dedupe,
reconcile, land-rescued), the farm-candidate integration in `wf convert --farm-model` at
sets 92-120, and a number of tool fixes that the campaign's failures forced. Vidsync
work continued in parallel and produced runtime-evidence entries to `symbols/db/`, but
this report covers the symbol-naming and farm-integration parts of those four days.

## Summary (numbers)

The campaign moved the project's "how well do we know what each symbol means" score from
`34.7%` (commit `ab8fcb2e`, 2026-10-19) to `35.1%` (commit `e2634fac`, 2026-10-19),
a final raw value of `35.13%` over a maximum of `157,090` (sum-of-5 × symbols).
The meaning score was last measured on 2026-10-19, after the 2026-10-18/19 rescue
batches; the 2026-10-20/21 investigate and data-label landings are not yet reflected
in `symbols/meaning_scores.tsv` (it was not re-run after they landed).

| metric | value | source |
|---|---|---|
| functions renamed via investigate commits (sum of `name N functions` in commit messages) | **4,396** | `git log --since=2026-10-18 --until=2026-10-22 --pretty=format:'%s' \| grep 'found by wf investigate ' \| grep -v 'investigate-data' \| sed -E 's/.*name ([0-9]+).*/\\1/' \| paste -sd+ \| bc` |
| data labels renamed via investigate-data commits | **111** (44 + 47 + 20) | commit messages `17a854b3`, `7110a509`, `807ae57a` |
| farm functions installed as readable C (sets 92-120) | **783** (8 sets, 12 commits, 92+84+83+100+14+87+22+48+84+79+82+8) | commit messages under `Readable small functions from the model farm, set N` |
| land-rescued candidates (the first batch, before 2026-10-20) | **218** (169 style fixes, 49 clash-name groups) | commit `057d07063c0a8ebe73c3cd4e6f2cc4edf8834fef` message |
| `wf investigate` runs in `build/investigate/investigate-20261018-*..investigate-20261021-*` with a non-empty `report.tsv` | **15** | `ls build/investigate/ \| grep 'investigate-2026101[89]\\|investigate-2026102[01]' \| wc -l` and `for d in build/investigate/investigate-20261018-* build/investigate/investigate-20261019-* build/investigate/investigate-20261020-* build/investigate/investigate-20261021-*; do [ -f \"$d/report.tsv\" ] && tail -n +2 \"$d/report.tsv\" \| wc -l; done \| grep -v '^0$' \| wc -l` |
| investigate `report.tsv` rows summed across every run in the window | **10,510** (status probable: 5,316; cross_check agree: 3,836; total tokens: 33,639,454) | same loop, summed (`awk -F'\\|\\|...'`) |
| `wf investigate-data` runs in the window with a non-empty `report.tsv` | **4** | same loop under `build/investigate_data/` (1 of 5 had rows=0) |
| data-label `report.tsv` rows summed across every run | **1,345** (status probable: 146; cross_check agree: 146) | same loop |
| symbols/db entries the campaign added (added files only) | **5,278 unique yaml** | `git log --since=2026-10-18 --until=2026-10-22 --diff-filter=A --name-only -- symbols/db \| grep '\\.yaml$' \| sort -u \| wc -l` |
| commits `found by wf investigate<space>` (function) | **42** | `git log --since=2026-10-18 --until=2026-10-22 --pretty=format:'%s' \| grep 'found by wf investigate ' \| wc -l` |
| commits `found by wf investigate-data` (data) | **3** | same grep with `investigate-data` |
| commits `Readable small functions from the model farm, set N` | **12** | same grep over commit subject |
| meaning-score final, functions | `37.12%` (`25,383/68,375`) | `python3 -c "import csv, io; ..."` over `symbols/meaning_scores.tsv`, matching commit `e2634fac`'s `functions 36.1% -> 37.1%` |
| meaning-score final, all symbols | `35.1%` (raw `55,186 / 157,090`) | `python3 -c "import csv, io; ... print(.../...)"` over `symbols/meaning_scores.tsv` |

## Methods

The campaign ran four kinds of work in parallel, plus the vidsync sync (not in scope here).

1. **`wf investigate`** (`tools/wf/wf/graphs/investigate.py`,
   `tools/wf/wf/tools/investigate.py`). A pass takes up to 500 placeholder functions
   (one `report.tsv` row each: address, old name, proposed name, status, self-score,
   cross-check, tokens). A cheap Hyper model (`hyper/glm-5.3-flash` early on, then
   `hyper/qwen3.8-flash` for several farm sets, and sometimes
   `hyper/deepseek-v4.1-flash` or `hyper/minimax-m3`) proposes a name from the
   function's own assembly plus the callers' assembly; a second model
   (`minimax/MiniMax-M3`) checks the proposal; only rows whose status is `probable`
   **and** whose cross-check is `agree` are landed. The `0d9f580b` commit added
   callers' assembly around the call sites in the context, `--retry-unnamed RUN_ID`,
   and a 300-second abandon timer after the pass's last unanswered request.

   **`wf investigate-data`** (`tools/wf/wf/graphs/investigate_data.py`,
   `tools/wf/wf/tools/investigate_data.py`, `tools/investigate_data_land.py`) does the
   same job for data labels; it was introduced on 2026-10-20 (`521ad42f`). The first
   run used `hyper/glm-5.3-flash` (proposer) and `minimax/MiniMax-M3` (checker); later
   runs used `minimax/MiniMax-M3` for both, which turned out to be cheaper on the
   data side and gave the same number of `agree` rows (30, 57, 52, 7 across the four
   runs).

2. **Rescue passes in `tools/investigate_land.py`** (function) and
   `tools/investigate_data_land.py` (data), the same four:

   - `style-fix`: the naming-style rejects, fixed with no model call. The fix must
     keep the project's `prefix_` shape.
   - `dedupe`: groups of functions that proposed the same name across all runs of the
     window; one Hyper `glm-5.3-flash` call gives each member a distinct name,
     `minimax/MiniMax-M3` checks, only agreed groups land.
   - `reconcile` (`f72bb913`): Hyper `glm-5.3-flash` reconsiders the checker's
     *revised* name (`accept_revised` / `keep_original` / `neither`); only
     `accept_revised` lands.
   - `land-rescued`: writes `symbols/db` entries for the rescued candidates and runs
     the record workflow once per batch (≤ 200 at a time).

3. **`wf convert --farm-model`** (`tools/wf/wf/graphs/convert.py`,
   `tools/wf/wf/tools/convert/...`). The driver's groups of ~100 small farm
   functions auto-approve when the gates pass (`tools/wf/wf/tools/gate.py`,
   `tools/gate.py`): 16/16 lockstep routes identical, nothing uncompared,
   `tools/wf/wf/tools/quicktest.py` 0 failures. The model authors are listed in the
   commit messages (`hyper/glm-5.3-flash` for sets 92-101, `hyper/qwen3.8-flash` for
   sets 110-112, `minimax/MiniMax-M3` for set 120). Each "Document small functions
   from the model farm, set N" commit writes the `symbols/db` entries and may rename
   `bn6f` symbols.

4. **Runtime-evidence experiment** (`wf runtime-evidence`,
   `tools/wf/wf/tools/runtime_evidence.py`). Replays captured calls of investigate-
   named functions in the original game, summarises the RAM and I/O, then has a model
   judge the name against those facts. The contrastive judge (multiple-choice with one
   truth and four decoys) was calibrated at `33.3%` accuracy (chance `20%`) with
   `40.0%` false positives; per `tools/calib_contrastive.py`'s honest-reporting rule
   both numbers fail the bar (the lower CI bound is not ≥ 30% and the upper bound on
   false positives is not below 20%), so the runtime facts are not yet discriminating
   enough. Measured on
   `/tmp/claude-1000/-home-alex-Code-recomp/9585631a-4f29-4891-aaaa-3eb8ba1d5242/scratchpad/calib_results.json`
   (60 entries; `truth_picked=20`, `false_positive=24`). The tooling is kept for
   future tuning.

The campaign also committed two tool fixes that were not naming work but were
required for the campaign to make forward progress (see "What went wrong and how it
was fixed" below): `42761efd` corrects the daily Hyper reset time; `276c48e9` adds
`hyper/kimi-k2.5` and `hyper/minimax-m3` to the farm queue's routes.

## Results per method

### investigate (functions)

The runs in the window are
`build/investigate/investigate-20261018-* .. investigate-20261021-*`. The breakdown
(rows = `(address, old, proposed, status, self_score, cross_check, tokens)`):

| group | runs with a `report.tsv` | rows | `probable` rows | `agree` rows | tokens | commits landed |
|---|---|---|---|---|---|---|
| total in window | 15 | 10,510 | 5,316 | 3,836 | 33,639,454 | (40 `name N>0` + 2 `name 0` commits; 4,396 functions summed across the `name N` lines) |

(All from
`for d in build/investigate/investigate-20261018-* build/investigate/investigate-20261019-* build/investigate/investigate-20261020-* build/investigate/investigate-20261021-*; do [ -f "$d/report.tsv" ] && tail -n +2 "$d/report.tsv"; done`,
summed in `awk`.)

By source model the rows were produced by (from commit subjects):

| proposer | rows in commits | source |
|---|---|---|
| `hyper/glm-5.3-flash, checked by MiniMax M3` | 2,653 across 35 commits | commit messages grep |
| `proposing and checking models: see each entry's evidence` | 1,743 across 8 commits (after the 2026-10-21 `6e038a66` change) | commit messages |
| Total | 4,396 across 42 function commits | sum of `name N` in commit messages |

Two investigate commits landed zero functions on purpose.
`7e999b89` (investigate-20261021-021922) was a run that produced no
`probable + agree` rows; this triggered the tool change in `14e0ff14` ("a run
with nothing to land writes no entries and starts no record run (an empty run
folder produced a 'name 0 functions' commit)"), which makes the empty-run case
a no-op.
`65ea7532` (investigate-20261018-182231) is a `name 0 functions` commit from a
second pass over the same data; its 190 functions were already aggregated into
commit `6963f54f`. One 429-rate-limited
run, `investigate-20261019-062107.failed-429`, hid 153 functions with a `no name`
row; `3ad782ba` fixed the run to stop like an out-of-credits 402 instead of writing
`no name` rows.

Token use per call across the campaign window (the field group is `tag` value starts
with `investigate-<RUN_ID>` or its data suffix; the rows are from
`tools/convert/usage.jsonl` filtered to `time` in
`[2026-10-18 00:00, 2026-10-21 23:59]`):

| kind | model | calls | input tok | output tok | reasoning tok |
|---|---|---|---|---|---|
| investigate (function) | `hyper/glm-5.3-flash` (proposer) | 9,537 | 9,833,512 | 11,682,376 | 6,980,736 |
| investigate (function) | `hyper/qwen3.8-flash` (proposer for later runs) | 495 | 798,155 | 818,379 | 706,702 |
| investigate (function) | `minimax/MiniMax-M3` (checker) | 8,269 | 8,464,520 | 2,062,191 | 0 |
| investigate-data | `minimax/MiniMax-M3` (proposer and checker on later runs) | 1,929 | 2,929,839 | 732,999 | 0 |
| reconcile | `hyper/glm-5.3-flash` | 2,624 | 4,167,402 | 1,261,649 | 688,957 |
| reconcile | `minimax/MiniMax-M3` (checker) | (folded into investigate-kind totals; mainly MiniMax calls inside its 8,269) | — | — | — |
| dedupe | `hyper/glm-5.3-flash` | 5 | 2,976 | 359 | 151 |
| dedupe | `minimax/MiniMax-M3` | 269 | 567,067 | 68,863 | 0 |
| drafts (`tools/wf/wf/tools/drafts.py`: each `Document small functions` commit asks the cheap model to draft the symbols/db entry of each accepted candidate) | `hyper/glm-5.3-flash` | 558 | 682,321 | 307,844 | 64,457 |
| farm (sets 92-120 retry rounds) | `hyper/glm-5.3-flash` | 1,248 | 4,072,097 | 8,922,595 | 8,586,777 |
| farm | `hyper/qwen3.8-flash` | 1,060 | 6,566,404 | 12,898,639 | 11,924,864 |
| farm | `hyper/deepseek-v4.1-flash` | 294 | 1,602,790 | 8,863,417 | 8,757,073 |
| farm | `hyper/minimax-m3` | 120 | 851,852 | 4,233,766 | 3,922,827 |
| farm | `hyper/kimi-k2.5` | 281 | 0 | 0 | 0 |
| farm | `minimax/MiniMax-M3` | 308 | 1,828,159 | 1,152,189 | 0 |
| other (rank_meaning.py, runtime-evidence prologues, the contrastive judge prologues) | `minimax/MiniMax-M3` | 577 | 736,095 | 85,769 | 0 |
| other | `hyper/qwen3.8-flash` | 288 | 731,729 | 358,965 | 187,138 |

(The `tag` field is the only way to disambiguate call purpose from `usage.jsonl`;
`minimax/MiniMax-M3` inside `reconcile` is counted in its `func` total, since both
calls carry the same `tag` prefix pattern.)

The 3,836 `agree` rows are the rows whose cross-check was `agree`. Their
mean token use is in `report.tsv` column 7; tokens summed by column-7 across
the runs was `33,639,454`.

### investigate-data (data labels)

Runs in the window:
`build/investigate_data/investigate-data-20261020-{185452,191231,192515,194825,223307,230424}`.
The first two were short (0 and 3 rows respectively); the four productive runs:

| run | rows | `probable` | `agree` | tokens | landed |
|---|---|---|---|---|---|
| `investigate-data-20261020-192515` (`hyper/glm-5.3-flash`, MiniMax-M3 check) | 200 | 30 | 30 | 992,346 | 20 (`807ae57a`) |
| `investigate-data-20261020-194825` (MiniMax-M3 ↔ MiniMax-M3) | 400 | 57 | 57 | 1,140,373 | 47 (`7110a509`) |
| `investigate-data-20261020-223307` (MiniMax-M3 ↔ MiniMax-M3) | 400 | 52 | 52 | 925,421 | 44 (`17a854b3`) |
| `investigate-data-20261020-230424` (left running, the entries were not landed) | 342 | 7 | 7 | 586,702 | 0 |

(`for d in build/investigate_data/investigate-data-*; do [ -f "$d/report.tsv" ] && ...; done`.)

Plus 15 data calls in `usage.jsonl` to `hyper/kimi-k2.5` (route probe, 0 tokens billed:
the call timed out on this route).

The MiniMax-M3-as-proposer runs (`investigate-data-20261020-194825`,
`investigate-data-20261020-223307`, `investigate-data-20261020-230424`)
agreed on 116 of `400 + 400 + 342 = 1142` rows; the campaign landed 91 of
those (47 + 44, since the third run's 7 rows had overlapping names and
were skipped by `520c488e`'s "already-taken name" guard).
A `tools/investigate_data_land.py` rule was added (`520c488e`) to handle
already-taken names, port identifiers, asm include filenames and offset expressions.

### reconcile (functions)

The two `reconcile-recover` commits land the cross-check-revised names the campaign
agreed with:

| commit | commit message | landed |
|---|---|---|
| `ca25c14e` | `symbols: name 199 functions found by investigate-reconcile-recover (Hyper glm-5.3-flash, checked by MiniMax M3)` | 199 |
| `c3f6f3a5` | `symbols: name 133 functions found by investigate-reconcile-recover-2 (Hyper glm-5.3-flash, checked by MiniMax M3)` | 133 |

(Total: 332 functions whose proposed name the checker's revise verdict re-asked
about; only those Hyper accepted as `accept_revised` were landed.)

A second batch of rescue tsvs:
`build/investigate/rescue/style.tsv` (182 entries),
`build/investigate/rescue/dedupe.tsv` (57 entries),
`build/investigate/rescue/reconcile.tsv` (1473 entries). The reconcile list has
1,473 entries; the `land-rescued` runs landed them across the `name N` commits
above (which is why commit-message `name N` sums are higher than the per-run `agree`
sums).

### farm-candidate integration (`wf convert --farm-model`)

Twelve "Readable small functions from the model farm" commits landed 783 small
functions as readable C across sets 92-120:

| set | functions | model | landed by |
|---|---|---|---|
| 92 | 92 | `hyper/glm-5.3-flash` | `486addef` |
| 93 | 84 | `hyper/glm-5.3-flash` | `da063e13` |
| 94 | 83 | `hyper/glm-5.3-flash` | `11272c59` |
| 97 | 100 | `hyper/glm-5.3-flash` | `54e8932d` |
| 98 | 14 | `hyper/glm-5.3-flash` | `a74ecfd7` |
| 99 | 87 | `hyper/glm-5.3-flash` | `de04b2ae` |
| 100 | 22 | `hyper/glm-5.3-flash` | `4f4c60da` |
| 101 | 48 | `hyper/glm-5.3-flash` | `78cacb44` |
| 110 | 84 | `hyper/qwen3.8-flash` | `a9c5eabd` |
| 111 | 79 | `hyper/qwen3.8-flash` | `9443e534` |
| 112 | 82 | `hyper/qwen3.8-flash` | `2971caf1` |
| 120 | 8 | `minimax/MiniMax-M3` | `7e9d2c6d` |

(Each row from `git log --since=2026-10-18 --until=2026-10-22 --pretty=format:'%s' \
| grep 'Readable small functions from the model farm'`.)

The `Document small functions from the model farm, set N` commits wrote 5,278
unique `symbols/db/<name>.yaml` files (see Summary); seven of them also pushed
`bn6f` renames:

| set | `bn6f` renames |
|---|---|
| 92 | 0 |
| 93 | 2 |
| 94 | 2 |
| 97 | 86 |
| 98 | 13 |
| 99 | 76 |
| 100 | 19 |
| 101 | 8 |
| 110 | 0 |
| 111 | 0 |
| 112 | 63 |
| 120 | 0 |

(269 total `bn6f` renames landed from the farm sets; `grep 'Document small functions' \
| grep rename | sed -E 's/.*rename ([0-9]+) symbols.*/\\1/' | paste -sd+ | bc`.)

The farm driver's auto-approve gates are summarised in
`tools/wf/wf/tools/convert/farm.py`: `verify 16/16 routes`, `nothing uncompared`,
`quicktest 0 failures`. The bn6f ROM stayed byte-identical on every commit (the
investigate renames commit `1d756b32` notes "ROM identical", and the farm's
`gate.py` enforces it).

### runtime-evidence (kept as research tooling)

A `wf runtime-evidence` pass was added (`dae12a8f`) and a contrastive judge
calibration script was added (`ad6aa15c`); the judge calibration on the
2026-10-19-180951 function set was:

- 60 functions sampled (`tools/calib_contrastive.py --count 60 --seed 7`).
- 5-option multiple-choice: `truth_picked` count: 20 of 60 = `33.3%` (chance
  20%).
- 4-decoy (true option removed) false-positive rate at `confidence ≥ 0.7`:
  24 of 60 = `40.0%`.

(From
`python3 -c "import json; data = json.load(open(...)); print(sum(...), ...)"`.)

Per `tools/calib_contrastive.py`'s own honest-reporting rule (the lower CI must be
≥ 30% and the upper CI must be below 20% to count), neither condition holds. The
runtime-evidence yaml files are also still not valid YAML — the previous batch's
hand-rolled f-string format breaks on `colon-space` in the judge's reason text;
the script recovers the `detail:` block with a regex the way `neg_control.py` does.
The tooling stays in place for future tuning.

## Costs

`tools/convert/usage.jsonl` (filtered to the four-day window):

| model | calls | input tok | output tok | reasoning tok |
|---|---|---|---|---|
| `hyper/glm-5.3-flash` | 13,972 | 18,758,308 | 22,174,823 | 16,321,078 |
| `hyper/qwen3.8-flash` | 1,843 | 8,096,288 | 14,075,983 | 12,818,704 |
| `hyper/deepseek-v4.1-flash` | 294 | 1,602,790 | 8,863,417 | 8,757,073 |
| `hyper/kimi-k2.5` | 281 | 0 | 0 | 0 |
| `hyper/minimax-m3` | 120 | 851,852 | 4,233,766 | 3,922,827 |
| `minimax/MiniMax-M3` | 11,352 | 14,525,680 | 4,102,011 | 0 |
| **total** | **27,862** | **43,834,918** | **53,450,000** | **41,819,682** |

(`usage.jsonl` `time` between 2026-10-18 00:00 and 2026-10-21 23:59.)

Hyper units ≈ tokens / 145,000 (per the 2026-10-10 model-pipeline review's measurement):

| model | units |
|---|---|
| `hyper/glm-5.3-flash` | ~282 |
| `hyper/qwen3.8-flash` | ~153 |
| `hyper/deepseek-v4.1-flash` | ~72 |
| `hyper/minimax-m3` | ~35 |
| `hyper/kimi-k2.5` | 0 (every call's recorded tokens are 0) |
| **Hyper total (approx)** | **~542** units, ~78.6M tokens |

The `hyper/kimi-k2.5` 281 calls with 0 tokens reflect the campaign-window probe of
that route; the model was added to the farm routes by `276c48e9` (see "What went
wrong and how it was fixed" below). It produced 0 useful output in the window.

MiniMax-M3 usage is 11,352 calls with 14.5M input and 4.1M output tokens; reasoning
tokens are not counted (the model does not expose them).

By day:

| date | dominant callers |
|---|---|
| 2026-10-18 | `hyper/glm-5.3-flash` 4,321; `minimax/MiniMax-M3` 3,103; `hyper/qwen3.8-flash` 489 |
| 2026-10-19 | `hyper/glm-5.3-flash` 8,787; `minimax/MiniMax-M3` 4,185; `hyper/qwen3.8-flash` 397; `hyper/deepseek-v4.1-flash` 24 |
| 2026-10-20 | `minimax/MiniMax-M3` 2,294; `hyper/qwen3.8-flash` 957; `hyper/glm-5.3-flash` 864; `hyper/kimi-k2.5` 281; `hyper/deepseek-v4.1-flash` 270; `hyper/minimax-m3` 120 |
| 2026-10-21 | `minimax/MiniMax-M3` 1,770 only |

The 2026-10-20 reduction in `hyper/glm-5.3-flash` traffic reflects the Z.ai and
Hyper credit lapse earlier that day (the `277/c4a710a0/42761efd` issues; see
below). The 2026-10-21 day had only MiniMax-M3 traffic in
`tools/convert/usage.jsonl` — Hyper showed `down` until past midday in
`build-wf-state/sources.json`.

## What went wrong and how it was fixed

1. **Renames that hit asm-include filenames** (`asm00_0`, `asm30_0`).
   `record-20261018-193404` failed on `asm00_0`; the placeholder name collided with
   the bn6f include file. The fix is the guard added in
   `tools/investigate_land.py`'s land-rescued path:
   `1a22db43`, "investigate_land: skip functions whose name is also an asm include
   file (the rename rewrote the include path, not the file: record-20261018-193404
   failed on asm00_0)". A second hit on `asm30_0` (record-20261019-213956) led to
   the same fix in `df875de5` (the comment names both files).

2. **Names that the port's own C already uses** (`start_cutscene`).
   `record-20261019-225651` failed on the port's identifier. The fix is in
   `tools/investigate_land.py`: the function land step now consults the port's
   already-declared identifiers before agreeing a name (commit
   `454e4b62`, "land-rescued rejects a name the port's own C already uses
   (start_cutscene broke record-20261019-225651)").

3. **Record runs regenerate `symbols/*.tsv` and
   `cport/src/runtime/reloc.c` but do not commit them.** A clean build of `HEAD`
   (the queue's working copy) then failed because the committed port code referred
   to symbols (`A_byte_203CA74`) that lived only in the regenerated, uncommitted
   `symbols/imported_data_symbols.tsv`. The fix landed twice by hand:
   `1ce782b2cc718f5898f2ec3d30edfccc37fe0d55` for the symbols tsvs and
   `19cf21e308c1a1bb13042c59540171fc7ba03d42` for
   `cport/src/runtime/reloc.c` (the two renamed SRAM core functions
   `copyBytes`, `copy_compareToFirstMismatch`). The record workflow itself should
   commit these; until then, every rename batch risks the same hazard.

4. **Tracked `build-vidsync/progress.md` broke the worktree link**
   (`tools/wf/wf/tools/build.py`). A worktree that held a tracked file at the
   `build-vidsync` path could not replace the directory with a symlink. The fix
   is in `2e8686a1`, "wf build: a worktree keeps its own build-vidsync when it
   holds tracked files (progress.md), instead of failing to replace it with a
   link."

5. **`HYPER_RESET` was 10:13, not 9:13.** `sources.json` recorded Hyper as down
   until the wrong reset time, so every model step slept for an hour after Hyper
   was already back. `42761efd` measured the actual reset on 2026-10-20
   (`balance 0 -> 250` at 9:05-9:21 local) and set
   `HYPER_RESET = (9, 13)`. The comment in `tools/wf/wf/tools/sources.py`
   notes that the prior 10:13 was per the user on 2026-10-09; the campaign found
   it wrong.

6. **Two record runs must never overlap.** A long `wf record` (a vidsync record on
   a big rename batch) holds the bn6f checkout lock; a second `record` started on
   the same checkout will conflict with the `bn6f.lock`/`main-tree.lock` files in
   `build-wf-state/`, and may also collide with a running `wf answer`. There is no
   time limit on `wf answer`; a vidsync record that hangs waiting on a reviewer's
   answer will hold the bn6f lock for ~30 minutes (one campaign-day observation).

7. **The farm queue idled when every open job had only exhausted Zai models left.**
   `wf queue` did not try Hyper's `kimi-k2.5` and `minimax-m3`, so the Hyper and
   MiniMax lanes sat idle. Fix is `276c48e9` ("wf queue: routes also try Hyper's
   kimi-k2.5 and minimax-m3 (the farm's open jobs had only exhausted Zai models
   left, so the Hyper and MiniMax lanes sat idle)"), which adds both models to
   every size band in `tools/wf/wf/tools/work_queue.py`'s `ROUTES`.

8. **The subsystem farm crashed after renames.** The knowledge layer's keys were
   placeholder names; after the first investigate rename a subsystem prompt
   builder lost its members. The fix is `c8cc04a5` ("tools/sig: the knowledge
   layer resolves renamed functions (subsystem members, signatures) so the
   subsystem farm's prompts build after the symbol renames"); written by
   MiniMax-M3 from the brief. After this, subsystems metadata (subsystem members,
   signatures) is address-keyed through the rename. The subsystem farm itself
   did not convert anything during the campaign — see Open items.

## Open items

- **Hard unnamed functions.** `symbols/meaning_scores.tsv` after commit
  `e2634fac` shows `6,040` function rows with score `0` (5,573 of them are still
  `sub_<8hex>`; 467 are non-placeholder names that score `0` because their
  symbols/db entry has no summary or evidence). Of those, `4,436` are
  placeholder-named in `symbols/imported_symbols.tsv`. (Sum: `awk -F'\t' '$3~/^sub_/' \
  symbols/imported_symbols.tsv | wc -l` = 4,337; plus 99 `nullsub_*`.) These
  need either a `wf investigate` retry or a hand rename.
- **Register-form farm candidates** `635 + 156` need retyping (the precise split from
  `symbols/pool.tsv` is `635` functions ≤ 60 instructions and `156` > 60;
  `9` have no `asm_lines` recorded). The non-`installed` pool has 800
  rows total: 457 `failed`, 343 `pending`. Of the 800, 138 have only 1
  candidate so far (others have 2–36). The retype queue
  `tools/abi/retype_queue.tsv` holds a related 478 entries from older
  model + old-prompt pairs; those are the production-cost retypes for
  the install/farm retype-job.
- **Subsystem farm not yet converting anything.**
  `build-wf-state/subsystem-jobs.json` (`23` subsystem jobs in the queue:
  `failed: 18, installed: 1, converting: 2, open: 2`). The chatbox subsystem
  (`chatbox_803FEB4`, 60 units, 1 entry point) was attempted at the start of the
  campaign and remains the only `failed` job with no later recovery commit. The
  knowledge-layer fix (`c8cc04a5`) means the queue should now build the right
  prompts; the next attempt is open.
- **Data labels pool nearly exhausted.** `1,317` placeholder data labels remain
  in `symbols/imported_data_symbols.tsv` (`awk -F'\t' '$4~/^[a-z]+_[0-9A-F]+$/' | wc -l`,
  actually 1,317 by this regex; the meaning-score file shows 1,591 with score 0 —
  the gap is 274 names that have a non-placeholder name but still lack a
  description). The campaign landed only 111 (commit messages summed); the
  MiniMax-M3-as-proposer runs agreed on `57+52+7 = 116` rows out of
  `400+400+342 = 1142`, and the first `hyper/glm-5.3-flash` run agreed on
  `30` of `200` rows. Re-running MiniMax-M3 twice yielded diminishing
  returns (`57 → 52 → 7`).
- **Old fork branches to audit (not done here).** The fork `alex-berliner/bn6f` had its `master`
  replaced by `recomp-fixes` on 2026-10-20. The previous tip is kept as `old-master-2026-07-12`
  (353 commits, 1,693 files: profiler, docs, CI, fixtures). `bn-notes` (35 commits), `optimizations`
  (177) and two agent worktree branches (90 and 88) also hold commits that `master` lacks. A worker
  on another machine is to audit them and prepare any symbol knowledge they hold; nothing from
  them has been ingested yet.

## Provenance per number

(Where the number came from, in the order it appears.)

- 4,396 functions renamed via investigate (commit-message sum):
  `git log --since=2026-10-18 --until=2026-10-22 --pretty=format:'%s' \
   | grep 'found by wf investigate ' | grep -v 'investigate-data' \
   | sed -E 's/.*name ([0-9]+).*/\1/' | paste -sd+ | bc`
- 111 data labels (44 + 47 + 20): commit messages `17a854b3`, `7110a509`,
  `807ae57a`.
- 783 farm functions, 12 commits: commit subjects under "Readable small functions
  from the model farm, set N", parsed for the `set N (K functions, written by M)`
  pattern.
- 218 land-rescued candidates: commit `057d07063c0a8ebe73c3cd4e6f2cc4edf8834fef`.
- 15 investigate runs with a non-empty `report.tsv`: `ls build/investigate/ \
  | grep 'investigate-202610' | head` matched against `[ -f "$d/report.tsv" ] &&
  tail -n +2 | wc -l`; only 15 directories have rows > 0; the rest are zero-byte
  retried runs (`build/investigate/investigate-20261018-205030` etc.).
- 10,510 investigate rows / 5,316 probable / 3,836 agree / 33,639,454 tokens:
  same loop, summed with `awk`.
- 4 investigate-data runs with a non-empty `report.tsv` (one more had only
  3 rows and the rest 0/3), 1,345 rows / 146 probable / 146 agree: same
  loop under `build/investigate_data/`.
- 5,278 unique `symbols/db/*.yaml` files added in the window:
  `git log --diff-filter=A --since=2026-10-18 --until=2026-10-22 --name-only \
   -- symbols/db | grep '\.yaml$' | sort -u | wc -l`.
- 42 function commits + 3 data commits (45 total `found by wf investigate`): `git log ... | grep -c 'found by wf investigate'` (45) and `grep -c 'found by wf investigate '` (42, function-only, no `data`).
- 12 "Readable small functions from the model farm, set N" commits:
  `git log ... | grep -c 'Readable small functions from the model farm'`.
- Meaning score 35.1%: `python3 -c "import csv, io; ..."` over
  `symbols/meaning_scores.tsv`; same calculation in commit `e2634fac`'s message.
- 6,040 function rows with score 0 (`5,573` + `467`): same Python script with a
  `score == 0` filter.
- 1,591 data rows with score 0: same; the `awk -F'\t' '$4~/^[a-z]+_/'` regex on
  `imported_data_symbols.tsv` finds `1,317` placeholder data labels that are
  *named* `sub_*`-style; the meaning-score filter finds the larger number.
- 4,436 placeholder functions: `tail -n +2 symbols/imported_symbols.tsv \
  | awk -F'\t' '$3~/^sub_/ || $3~/^nullsub_/' | wc -l` (4,337 sub_ + 99 nullsub_).
- 60 functions calibrated, 20 truth_picked, 24 false_positive:
  `python3 -c "import json; ..."` on
  `/tmp/claude-1000/-home-alex-Code-recomp/9585631a-4f29-4891-aaaa-3eb8ba1d5242/scratchpad/calib_results.json`.
- Per-model call/token counts in the campaign window:
  `python3 -c "import json; ..."` over `tools/convert/usage.jsonl` filtering on
  `time` between 2026-10-18 00:00 and 2026-10-21 23:59.
- 332 reconcile-recover functions (199 + 133): commit subjects on
  `ca25c14e` and `c3f6f3a5`.
- 269 bn6f renames from the farm integration:
  `git log ... | grep 'Document small functions' | grep rename | sed -E ... | paste -sd+ | bc`.
- The pool breakdown (800 / 457 / 343 / 635 / 156 / 9):
  `tail -n +3 symbols/pool.tsv | awk -F'\t' '$4!="installed"'` and pipelined.
- 478 retype-queue entries:
  `tail -n +3 tools/abi/retype_queue.tsv | wc -l`.
- 23 subsystem jobs (failed/inst/converting/open breakdown):
  `python3 -c "import json; ..."` on `build-wf-state/subsystem-jobs.json`.
