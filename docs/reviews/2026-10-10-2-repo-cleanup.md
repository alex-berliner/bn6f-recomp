# Repo cleanup and streamlining: debate report

Moderator: C2 (Opus 5.5, medium). Debaters: A (Opus 5.5, xhigh) and B (Fable 5.1, xhigh). Both worked read-only on 2026-10-10 while a farm run was live, so some counts may have moved slightly. This report lists only what the first review (`2026-10-10-1-non-llm-cleanup.md`) missed, plus corrections to it.

## Summary (top recommendations, in priority order)

1. **Don't run the first review's cleanup of `build-wf-runs` (57 GB) as written.** 56 GB of it is the live farm's input: `snapshots-latest` is 2,726 symlinks into 9 capture runs. 93% of db entries cite a run id, and the folder also holds the quota state and lock files. Only about 1.7 GB is safe to delete today.
2. **Make sure nothing irreplaceable is deleted.** Write a "never delete" list for ignored data that exists nowhere else (`usage.jsonl`, `coverage/*.prof`, `accepted/`, `log/`). Before pruning any run, archive the text logs (about 14 MB) of the runs the db cites.
3. **Fix the rename tool before the planned commit gate lands.** It does not update `lint_baseline.json`, so the first rename would break the gate.
4. **Halve the symbol db.** About 6 of its 11 MB is copies of whole farm-group name lists (in `reproduce:` and `related:`), and about 48% of those names are stale. Do this in one script, before the `leaves_farm` regroup.
5. **Ask the user two questions:**
   - Is the old PC port (gbarecomp) still wanted? It takes half the repo root and about 700 MB of build output.
   - May `master` be moved up to the current work? It is 158 commits behind `wf-burr-demo`.
6. **Put shared paths in one file (`tools/paths.env`) and add a build rule for `mgba_run`.** Every check depends on that binary, yet no script builds it.
7. **Docs pass:**
   - `README.md` becomes a "start here" page with a table of every doc, marked hand-written, generated or dated.
   - `PROCESS.md` keeps rules only; dated material moves to `docs/log/`.
   - One spec for a db entry.
8. **Small cleanups:**
   - Delete stray files, then run `git gc`.
   - Use SVG instead of PNG for the graphs.
   - Store experiment data compressed.
   - Remove 54-67 duplicated `#define`s.
   - Stop giving models two reference files that fail the lint.

A and B converged on every area. The one point left differs only in timing: whether `master` may be fast-forwarded before the user answers (Area 4).

Terms used below:
- **Snapshot:** a saved machine state captured for one function call; it is used as a test input.
- **Run dir:** one workflow run's folder under `build-wf-runs/`.
- **db:** `symbols/db/`, the symbol database.
- **Lint:** `tools/abi/lint.py`, the check that rejects register-form code.

---

## Area 1. Run folders, snapshots, state files, Burr logs

**Ideas:**
- A1 / C1 (B): most of the 57 GB is in use.
- B1: one fixed snapshot store.
- B10: move the state files out of the runs folder.
- C4 (B): keep the logs of runs the db cites.
- A8: Burr logs store every model reply after every step.
- B11: document `~/.burr` and clean it up.
- A3: a "never delete" list for ignored data.

**Debate in brief:**
- B first wanted to merge snapshots per function and add a cap. A showed:
  - capture already stops at 40 calls per function per route (`capture.py:56`);
  - only 8% of 4 KB pages are unique across 1,323 sampled snapshots, so storing pages by content (first review Area 6, step 5) saves about 12x without dropping inputs, while a cap gives at most 2x.
  - B withdrew the cap.
- On timing, the two crossed over twice and then settled on two separate steps:
  - The cheap layout move (a rename on the same disk, instant) happens early.
  - Storing pages by content comes later.
- A corrected B's archive size. The cited runs hold 415-430 MB without snapshots, but their readable text (`.md`/`.log`/`.json`/`.cfg`) is about 14 MB. Keep the text and drop the binary digests.

**Agreed approach and order:**
1. A3: the "never delete" list (`tools/convert/usage.jsonl` with 5,835 calls, `accepted/`, `log/`, `tools/traces/coverage/*.prof`). Also commit a daily usage summary, and the `.prof` files or a compact form of them.
2. Layout move, between farm batches and before the next capture run:
   - move the link targets into `build-snapshots/FUNCTION/`;
   - turn `snapshots-latest` into one symlink to that store;
   - capture then writes into the store.
3. State folder `build-wf-state/` for `sources.json` and the three lock files. One constant changes, in `wf/repo.py`. Delete `cport/.farm-build.lock`; only the old farm command-line driver uses it.
4. Archive the text logs of the 36-38 cited runs (about 14 MB). From now on, `sources:` also records the commit a run produced.
5. `wf gc --dry-run` that lists, per run, only runs that are not cited, not linked and have no live owner (about 1.7 GB today). It refuses anything on the step 1 list.
6. After the farm-pool ledger backfill (first review Area 1):
   - A8: save replies as files and keep only their paths in Burr's state. One `log.jsonl` is 1.19 GB; `~/.burr` is 2.9 GB.
   - B11: gc for `~/.burr`.
7. Storing snapshot pages by content: at the latest when free disk drops below about 150 GB. Today 330 GB is free, and each capture run adds 10-30 GB.

**Effort:** steps 1-5 S each; A8 S-M; the content store M.

**First steps:** the "never delete" list; the dry-run gc; the `snapshots-latest` swap between batches.

**Relation to the first review:** this replaces the order of Area 8, step 1, and the age-based retention rule of Area 6. The first review's content-addressed snapshot store (Area 6, step 5) stays, but is no longer a prerequisite for cleanup.

---

## Area 2. The old PC port (gbarecomp) and shared paths

**Ideas:**
- A4 / B2: freeze or retire the PC port.
- B3: split `import_symbols.sh` and stop tracking the overlay files.
- A5 / B4: one place for the shared paths.
- A24: tidy `.gitignore`.

**Evidence:**
- The PC port is 7 root entries plus 2 submodules: gbarecomp (80 MB, plus 160 MB in `.git/modules`) and recomp-ui. Its ignored build output is about 690-740 MB. Nothing has touched it since Oct 7.
- The C port uses it for only two things: the BIOS file, and a record step (`record.py:152`) that rewrites 4 tracked generated files (about 1.9 MB) in 18 commits. One of those files, `symbols/function_boundaries.tsv`, is read by nothing.
- Hard-coded paths:
  - the BIOS path in 19-21 files;
  - `build-tools/mgba_run` in 30 references;
  - the frame address `0x020093D4` in 14-15 files.

**Debate in brief:**
- B accepted A's "ask the user first", and that a git tag is a better archive than an `attic/pcport/` folder.
- For the paths, both moved to one KEY=VALUE file, `tools/paths.env`. Shell scripts source it; `wf/repo.py` and the system-Python scripts parse it. Values are relative to the repo, so worktrees work. `common.sh` keeps only functions.
- The BIOS goes to `roms/`, which is already ignored, with a symlink at the old path for now.
- They differed on how many copies of the bn6f make command exist: B counts 2 in shell. The Python copies in `rename_symbol.py:91` and `symdoc.py:72` should also call `build_bn6f.sh`.

**Agreed order:**
1. `tools/paths.env`.
2. B3: `build_bn6f.sh` becomes the only copy of the make command. Stop tracking the three `.tsv` files and `BR6E_symbols.toml`, and drop `record.py:152`.
3. Ask the user. If the PC port is dead: tag it `pcport-final` and delete it. If it is dormant: move it into `pcport/`. Either way, `build.py:21` stops linking gbarecomp into worktrees.
4. `.gitignore` tidy (A24).

**Effort:** S, S, M, S.

**Relation to the first review:** none directly. This must come before the one-output-root step in Area 3.

---

## Area 3. Generated output, stray files, git history, tool binaries

**Ideas:**
- A13 / B13: one ignored root for all generated output.
- A16: stray files, then `git gc`.
- A17 / B7: SVG instead of PNG.
- A17 / B8: experiment raw data.
- A12 / B15: a build rule for `mgba_run` and `record`.

**Debate in brief:**
- Both agreed fully, except on experiment data. B first wanted it in an ignored folder. A pointed out that it backs a committed report and would become data with no other copy. B accepted keeping it tracked, compressed: `results.jsonl` goes from 987 KB to 152 KB as `.jsonl.zst`.
- Both flagged that `roms/out` (136 MB) holds the ROM the user's flash cart runs (`bn6f_falzar_cport_17b3891_supercard.gba`). Keep the newest `_supercard` and `_diag` ROMs, and ask before deleting the rest.

**Agreed order:**
1. `tools/Makefile` for `build-tools/mgba_run` and `record`, called from the verify build step, with the libmgba package documented. Today no script contains `mgba_run`'s compile command.
2. One commit removing stray files, then `git gc` (5,064 loose objects). The strays:
   - `.playwright-mcp/`, `saves/`, `recomp_cache/`, `recomp_coverage_BR6E.json`;
   - two `profile.txt` files and `cport/build.log`;
   - `.claude/scheduled_tasks.lock`, `tools/convert/rejected/`, `tools/traces/menus.trace`;
   - `cport/src/game/README` and `build-wf-runs/farm-paused-20261009.cmdline`.
3. Graphs exported as SVG, and PNGs no longer tracked. 48 PNG/HTML blobs add up to about 5 MB of history.
4. Experiment raw data stored as `.jsonl.zst`. The rule for any experiment: keep the `.md`, the script and the compressed data.
5. One output root (`build/{gba,host,abi,snapshots,wf-runs,worktrees,tools,state}`), together with Area 2. Paths are hard-coded today: `build-tools/` in 23 files, `src/lifted` in 13, `build/abi` in 6. Cheap first step: move `build/abi/graph.json` (31 MB) out of the PC port's CMake `build/` folder (6 references), so cleaning the PC port doesn't delete it.

**Effort:** S, S, S, S, M.

**Relation to the first review:** this extends Area 8's "prune build directories" with a naming rule, and sits on top of Area 1's retention changes.

---

## Area 4. Docs, entry point, branches

**Ideas:**
- A6: a "start here" README.
- C3 / B6: split the docs into rules, a dated log and generated status.
- B9: misplaced generated reports.
- B18: one spec for a db entry.
- A14: undocumented lifter switches.
- A15 / B14: a `./wf` launcher.
- A7 / B5: the branch names.

**Evidence:**
- `README.md` still describes the Oct 7 PC port.
- `PROCESS.md` has stale prose:
  - §5 says "46 functions … 84 in all";
  - §9 is a copy of a cancelled cron prompt;
  - the cost line is wrong.
- Three descriptions of the repo layout disagree with each other.
- The `abi:` block is specified only in `C_CALLING_CONVENTION.md` §2, not in the db spec.
- `lift.py` reads 7 environment variables; one hides an unproven assumption (`lift.py:702`).
- 5,404 db lines say `cd tools/wf && .venv/bin/python -m wf`.

**Debate in brief:**
- B withdrew moving `SESSIONS.md`; it becomes `tools/traces/README.md`, beside the files it describes.
- A won on keeping PROCESS §8 (the flash-cart notes) as `tools/flashcart/README.md` rather than cutting it.
- Moving `oracle_triage.md` to `docs/` must update its 5 code references in the same commit.
- **Branch (the one point left open):** both agree a fast-forward of `master` is risk-free (`master` has 0 commits of its own, and nothing names either branch). In the last round, A said the whole change waits for the user's yes, because the user keeps git decisions. B said `git branch -f master wf-burr-demo` may go first, with only the checkout switch and branch deletion waiting. Moderator's note: given the user's preference rules, ask first. It costs nothing.

**Agreed order:**
1. Ask about the branch, in the same message as the PC-port question.
2. Add `./wf` (before Area 6, so the new `reproduce:` lines are short).
3. One docs pass:
   - README "start here": 5 commands, and a table of the 21 `.md` files outside the db, each marked hand-written, generated or dated.
   - `PROCESS.md` keeps rules only: delete §9, turn §5 into a pointer, move §8 to `tools/flashcart/README.md`.
   - Dated material moves to `docs/log/`: `EFFICIENCY.md`, `C_CALLING_CONVENTION.md` §3, and the model results table.
   - Status is generated (first review A31).
   - `UNSEEN_SITES.md` either gets a generator and a "made by" header, or is deleted.
4. The full db entry spec, including `abi:`, goes in `symbols/db/README.md`.
5. The lifter's environment variables become `--flags`, with the unwind assumption written down.

**Effort:** S each, about half a day in total.

**Relation to the first review:** extends A31 (generated counts), which by itself does not fix the stale prose.

---

## Area 5. The `tools/` layout, retiring scripts, Python inside shell scripts

**Ideas:**
- A2 / C2 (B): "unreferenced" is the wrong test for retiring a script.
- B17 / A18: group `tools/` by purpose.
- A11 / B12: one farm driver.
- B16 / A23: move the Python embedded in shell scripts into modules.

**Evidence:**
- db lines cite tools by path: symdoc 1,512 times, rename_symbol 762, lockstep.sh 609.
- `bisect_readable.py` has no code references but was used on Oct 9.
- bn6f's commits cite the `fix_*.py` scripts.
- `models.py:18-21` imports `farm.py` by adding its folder to `sys.path`.
- `lockstep.sh`, the project's main judge, holds its 64-line pass/fail rule as untested Python inside the shell script.

**Debate in brief:**
- Both agreed the retirement rule is a "cited by" scan over the db, bn6f's git log and the docs. The same scanner then powers Area 6's path check.
- Retired scripts are deleted outright, with a restore note (the commit to restore from) in `tools/README.md`. No attic folder.
- The bn6f repair scripts move to `bn6f/tools/recomp/` on `recomp-fixes`: `fix_func_ends`, `fix_ptr_tables`, `fix_rfu_blob`, `label_data`, `bn6f_audit` and `rawdis.sh`.
- B corrected its own classification: `compare_roms.sh` isn't from the PC port. Retire it anyway, because lockstep already saves screenshots on a mismatch.
- A first opposed regrouping cited tools. A withdrew this once B showed that 2,273 of the citations are `changes: tool:` fields, which a sed rewrite can fix in the same commit as Area 6's rewrite.
- `farm.py`: delete only its command-line driver now, between batches. Move its library parts into typed `wf` code later, at a quiet point, after checking that the rendered prompts are byte-identical and the `usage.jsonl` line format is unchanged. The user compares pass rates per model, so prompt drift would spoil the comparison.

**Agreed order:**
1. Delete the `farm.py` CLI driver together with `.farm-build.lock`.
2. Move lockstep's pass/fail logic into a Python module with JSON output and tests (using two saved `mgba_run` outputs), then battlesweep. Keep the thin `.sh` wrappers. The other 8 scripts with embedded Python change only when touched.
3. Write the cited-by scanner, then retire uncited scripts and move the bn6f repair scripts.
4. Regroup uncited families now (`tools/flashcart/`, profiling). Cited families move only inside Area 6's db rewrite commit.
5. Later: the typed move of `farm.py`'s library parts.

**Effort:** S, S-M, S, M, M.

**Relation to the first review:** this corrects Area 8, step 5: no attic folder and no "unreferenced" test. It complements B15's shared parser library.

---

## Area 6. Symbol database hygiene

**Ideas:**
- A9: `reproduce:` lines repeat the whole farm-group command.
- A19: `related:` lists the whole farm group.
- A20: the rename tool skips the lint baseline.
- A10: overlong rename commit subjects.
- A2: check that paths in `reproduce:` lines exist.
- C5 (B): the `/tmp` citations are a small job.

**Where the size goes (B's measurement):** the db is 11 MB. `evidence` is 5.4 MB, of which `reproduce:` text is 3.6 MB, and `related:` is 3 MB. The farm-group copies are about 160,000 name mentions, and 48% of them are stale.

**Debate in brief:**
- `reproduce:` lines must be runnable and must not depend on an ignored cache. The agreed form is the self-contained pair already used: `./wf capture --function NAME`, then `./wf verify --function NAME --snapshots DIR`.
- Readable-C evidence cites the commit that installed the code.
- The run id and commit go in `sources:`.
- `related:` lists only direct callers and callees from `graph.json`, and is empty for farm groups.
- A showed the rewrite needs no record run (it would cost about 5 minutes for nothing). symdoc writes only summary, interface and status into bn6f (`symdoc.py:40-43`), so `symdb check` plus a re-render of `SYMBOLS.md` is enough proof.
- Placeholder-key counts (183 vs 198) differ only in how each agent searched.

**Agreed order:**
1. A20, alone and first: `rename_symbol.py` also rewrites `tools/abi/lint_baseline.json` and `tools/lifter/not_entries.txt`.
2. One script for A9, A19 and C5. The 81 entries with dead `/tmp` lines get the per-function capture command.
3. `symdb check` verifies that paths in `reproduce:` lines exist, using Area 5's scanner.
4. A10: rename commit subjects become "Rename N symbols (record RUNID)", with the pairs in the body. The longest subject today is 11,193 characters.
5. All of this happens before the `leaves_farm` regroup. 1,324 entries mention `leaves_farm_N.c` about 4 times each.

**Effort:** S, M, S, S.

**Relation to the first review:**
- A20 must land before the commit gate (Area 8, step 2).
- The rewrite must come before the `leaves_farm` regroup (Area 8, step 4).
- It replaces Area 6, step 6's `/tmp` path fix: those folders are gone, so rewriting the paths would not work.

---

## Area 7. Leftovers in cport code and model prompts

**Ideas:**
- A21: duplicated `#define`s and private memory accessors.
- A22: reference files that fail the lint.

**Evidence:**
- 54-67 local `#define`s in `cport/src/game` repeat names from `bn6f_equates.h` and `bn6f_syms.h`. Every one has the same value; the two counts differ only in search scope.
- `m4a_mplay.c`, `m4a_mixer.c`, `sprite_gfx.c` and `sprite_oam.c` each define their own U8/U16/U32 accessors. These use plain pointers, without the volatile access or ROM-write guard of the shared accessors.
- The prompts still ask models to write local defines and enums: `subsystem.py:73` and `farm.py:123`.
- Both reference files fail the lint: `bg_scroll.c` has 8 findings, `event_flags.c` 23. No file passes; only 51 of 89 typed functions are clean.

**Debate in brief:**
- They agreed on the order and the proofs.
- Deleting same-value defines must leave the ROM and the host binary byte-identical. Comparing bytes is enough; no lockstep run is needed.
- Switching the accessors changes the code in the hot sound mixer, so it needs `tools/speed.sh` before and after, plus a verify.
- On the reference files, the two crossed over and ended agreed: drop both now.

**Agreed order:**
1. Delete the duplicate defines and confirm the outputs are byte-identical.
2. In the same commit, between farm batches, change the prompts to "use the `bn6f_equates.h`/`bn6f_syms.h` names, never redefine".
3. Drop REFERENCE_FILES. Add one rule line for what the files used to show: a header comment naming the bn6f source, and names taken from the shared headers. This saves about 14 KB per strong-model attempt.
4. Switch the four files to the shared accessors, with the speed check.
5. Later: an outside model converts `event_flags.c` through `wf convert`, Claude reviews it, and it becomes the reference.

**Effort:** S, S, S, S, M.

**Relation to the first review:** this updates Area 3, step 2. The "export the offsets" half is done (8549708); only deleting the local copies remains.

---

## Appendix: stage 1 idea lists

**A: 24 ideas, exhausted after 2 rounds.** Round 1 gave 18; round 2 gave 6 more and ended "after A24 I have no more ideas".
- Corrections to the first review:
  - A1: most of `build-wf-runs` is in use.
  - A2: "unreferenced" is not "unused"; check reproduce paths; no attic.
  - A3: ignored data that exists nowhere else.
- A4: freeze or fence off the PC port
- A5: one place for the BIOS path and replay constants
- A6: README as a "start here" page
- A7: honest branch names
- A8: Burr logs store every reply
- A9: `reproduce:` cites the run, not the group
- A10: readable rename commit subjects
- A11: one farm driver
- A12: a build rule for `mgba_run` and `record`
- A13: one root for generated output
- A14: undocumented lifter switches
- A15: a `./wf` launcher
- A16: stray files and `git gc`
- A17: large generated files out of history
- A18: a flash-cart folder
- A19: `related:` is the whole group
- A20: the rename tool skips the baseline
- A21: redundant `#define`s and accessors
- A22: reference files fail the lint
- A23: lockstep logic into Python
- A24: `.gitignore` tidy

**B: 18 ideas plus 5 corrections, exhausted after 2 rounds.** Round 1 gave C1-C3 and B1-B14; round 2 gave C4-C5 and B15-B18, then "no further new ideas".
- Corrections to the first review:
  - C1: `build-wf-runs` is live input.
  - C2: the wrong attic criterion.
  - C3: generated counts are not enough for the docs.
  - C4: keep the logs of cited runs.
  - C5: the `/tmp` problem is small.
- B1: a snapshot store with a cap (cap later withdrawn)
- B2: retire the PC port
- B3: split `import_symbols.sh`
- B4: one place for shared paths
- B5: make the trunk branch obvious
- B6: docs split into rules, log and status
- B7: stop committing PNGs
- B8: experiment write-ups, not raw data
- B9: misplaced generated reports
- B10: state files mixed with runs
- B11: document and prune `~/.burr`
- B12: farm script used as a library via a path hack
- B13: one root for generated output
- B14: a `wf` launcher
- B15: a build rule for `build-tools`
- B16: Python inside shell scripts
- B17: group `tools/` by purpose
- B18: one db entry spec

**Overlaps** (the same idea found independently): A1/C1, A2/C2, A4/B2, A5/B4, A7/B5, A6/B6, A11/B12, A12/B15, A13/B13, A15/B14, A17/B7+B8, A18/B17, A23/B16.

**Debate:** 4 exchanges per area, all areas in parallel:
1. Proposals.
2. Critiques.
3. Open points.
4. Tie-break.
