# Running the workflows: what cost time or tokens, and what changed

Measured on 2026-10-08 running the workflows for real: two subsystem
conversions with strong models (`wf convert`), three capture batches and
Hyper farm batches over about 2,500 small functions (`wf capture`,
`wf farm`), and the drafts backfill (`wf drafts`). Each entry: what happened,
what it cost, and what was done (or is still open).

## Fixed

| Problem | Cost when found | Fix |
|---|---|---|
| mgba_run counted captured calls per function in a byte. A function called over 256 times wrapped to 0 and was captured again: 40 more 429 KB snapshots every 256 calls, overwriting earlier entries and pairing them with older exits. | One route (452 functions) ran over an hour and wrote 9 GB; the batch never finished. Captures of frequently called functions could compare a call's entry against another call's exit. | The count stops at the limit (16-bit counter). The same batch: 4 min 49 s. |
| capture.sh left its temporary snapshots behind when killed. | 9.1 GB in /tmp after one kill. | `trap` removes them on any exit. |
| capture.sh moved snapshots with two forks per file (`basename`, `mkdir`). | About 330 files/s: 2.5 min of a 5.5 min capture for 48,000 files. | Parameter expansion, `mkdir` only for new directories. |
| Calls an interrupt arrived in were dropped from captures. | Long functions had 0 captured calls; 3 deepseek attempts (about 140k tokens) were spent on functions nothing could check. | `--snap-keep-interrupted` keeps their entry state (difftest -S and the oracle need only that); convert refuses to ask a model about functions without captured calls. |
| A lifter "entry" inside sub_8030808 (`loc_803083E`) came only from NPC script bytes `3f 08 03 08` that look like a pointer to it. The verify build then required the C file to define it, while the quick test refused functions outside the subsystem. | No model could pass: 7 attempts, about 300k output tokens, 55 min of model time. | `tools/lifter/not_entries.txt` (with the evidence); real mid-function entries now join the subsystem (prompted, quick-tested, drafted, flagged when nothing compared them). |
| `farm.find_asm` required a label to end its line; 536 bn6f labels carry a signature comment (`GetRNG: // () -> u32?`). | Those functions were silently "nothing to convert" in every farm run so far (46 of batch A's 496). | The pattern accepts a trailing comment. |
| Replies dropped their cfg/yaml blocks on repair, or wrote yaml values containing `: `. | 3 of 7 strong-model attempts failed on metadata alone, with the code untested. | Values with `: ` are quoted and parsed again; the code is tested even when only the yaml is wrong ("the code passed; fix only the other blocks"); repairs name all three blocks. |
| A 48k output budget for strong models. | Reasoning took 35-48k tokens per subsystem attempt: 3 of 7 replies were cut off or empty (kimi-k3, glm-5.3: 20 min for an empty reply). | 64k (with a 48k fallback for providers that refuse), one retry instead of three. |
| Models copied the reference file's "Written by Claude" line into their own files. | A false authorship line reached review. | The line is stripped from the references; the prompt says the commit records the author. |
| Picking a farm batch from every coverage profile, including sweeps the capture cannot replay. | 522 of batch B's 1,200 functions had no route to capture on. | Select from the eight replayable routes' profiles only. |
| A finished farm candidate had no symbols/db entry, so integrating it would make a strong model write the documentation. | One strong-model round per group of small functions. | The farm model drafts each accepted function's entry (`draft_symbol_entry`; `wf drafts` for older ones): about 4 s and 1-2k tokens on Hyper. |
| Stuck background jobs. A background job only reports when it ends, so a stuck one is invisible. | An hour lost on the stuck capture. | Shell watchdogs (no model cost until they fire): a process over its expected time, or a log that stops growing. Match the process, not the `bash -c` wrapper whose command line repeats the pattern (both mistakes were made). |
| `wf status` showed killed runs as "running": the tracking log of a killed run and of a working one both end with a step that began. | Five stale runs listed as running. | Each tracked run records its process (pid and start time) in `owner.json`; a run whose process is gone shows "stopped ... its process ended during this step". |
| OpenCode Go hit its usage limit (429, "retry in 12,520 s"); the local pacer holds such requests until then without telling the client, which sees only a slow request. Two one-word test calls filled the pacer's 2 slots. | A review round's deepseek request sat 20 minutes, and would have waited about 3.5 hours. | `tools/llm.py blocked_until` reads the pacer's log (read-only; its settings untouched); convert passes over a rate-limited provider for the next model in the escalation order. |
| A farm batch built its difftest from the main tree while a conversion's verify had installed a file and re-lifted. | The link failed (`undefined reference to sub_809FACE`); two farm batches tried nothing, and a third crashed on a run id taken in the same second. | The farm lifts and builds in a git worktree of the last commit (`build-wf-worktrees/farm`); run ids get a suffix when taken. |
| The farm compiled candidates with `-w`, which also silenced its `-Werror=return-type` and `-Werror=implicit-function-declaration`. | A candidate missing a return passed the farm and broke the full GBA build: one 8-minute verify round of a farm group. | No `-w`. |
| Conversions ran one at a time: each installed its file into the main tree and verified with shared build directories. | Integration was the bottleneck (15-20 min per small group, 25 groups waiting). | Each convert run works in a git worktree of its own; approved work is copied into the main tree and committed under a lock. Renames wait for one batch record while conversions run (`--defer-record`). Farm candidates go in groups of about 100-250: a verify costs the same for 10 functions or 250. |
| The random-input difftest ran a group's functions one after another. | 252 s of a 337 s verify round for 95 functions. | Functions spread over a process per core; captured-call difftests and the quick test run a function per thread. |
| A record run committed every changed file under symbols, bn6f, cport, tools and docs. | It swept untracked docs and uncommitted workflow code into unrelated commits. | Only the paths record tools change, minus what was uncommitted before the run. |
| Drafted names: a literal `_ADDRESS` copied from the prompt, a RAM address as the suffix, and renames of names someone chose (`TestCutsceneFlag` -> `cutscene_TestFlag`). | 17 names in bn6f to correct with one more record run (with its verify). | Proposed names always end with the function's own address; renames only replace placeholder names (sub_, loc_, a bare module and address). |
| A farm group's oracle differences surface one or two per verify round (the oracle samples 24 states per function). | Group 1 took 6 verify rounds (44 min) to settle on 93 of 99 functions. | Open: more oracle states for farm groups, or a first oracle pass before the full verify. |
| A worktree lacks what git does not check out there: the submodules (bn6f, gbarecomp) and ignored built tools (build-tools/mgba_run, toolchain). | Farm groups' ROM builds and oracle runs failed; three group runs given up or crashed. | Worktrees link them from the main checkout (removed before the worktree is). |
| A farm group gave up when it could not tell which candidate to drop: a stray `Next X(void)` (brace a line down) in another candidate's part, or a compile error inside a header (the candidate's line only in gcc's note). | Three group runs given up. | The part holding the stray definition is dropped; notes into the file are kept; a test for each case. |
| The lifter wrapped its difftest module in one `#if defined(DIFFTEST)` and then split it at 20k lines. | With 743 hand-written functions the guard and its `#endif` landed in different parts: the batch record's verify could not build. | Each part has its own guard. |
| Renaming back to an earlier name left the current name in `previous_names`. | `symdb.py check` stopped the batch record. | rename_symbol.py drops it; 12 entries fixed. |
| Hyper paced at 15 calls a minute (the experiment). | Cache hits fell from about 50% (bursts of 16 concurrent) to 12%; cost per token about the same (5.6 vs 6.9 units per million tokens, inside the balance's whole-unit noise); no hourly limit hit at that pace. | Pacing gives no cache benefit (Hyper's cache seems to stay warm per server only under concurrency); bursts with the 429-wait return results sooner. |
| A farm group's lockstep differed on some routes while difftest and the oracle named no function. | Group 5 gave up twice; at the time nothing could find the function. | Bisection: the later half is set aside (back to the pool) and the rest verified; passing halves are committed. Five such functions found overnight, each in 4-6 rounds, about 900 good functions committed around them: CheckKeyItem (branched on a clobbered r0 instead of the flags its integrity check leaves), sub_813DA94 (read byte-offset steps 0/4/8 as 0/1, so the sequence stalled at step 4), sub_81196A0, sub_8109FD6, sub_8034D7C. Captured calls and the oracle's sampled states mostly missed them; only whole-route lockstep catches them. Small groups bisect faster in parallel (four groups of 7 at once). |
| One oracle run of the original hung inside a single emulator step (interrupts off), past its 200,000-step cap. | A verify sat 21 minutes. | The oracle gives up on a batch after 5 minutes (calls without an exit are not counted anyway). |
| Hyper has an hourly rate limit, not only a daily balance. | After 1,025 calls in 19 minutes (3.3M tokens, about 23 of 217 units), every call failed with HTTP 429 "try again in 42m12s"; 45 farm pairs and the drafts run recorded provider failures. | `tools/llm.py` waits out a 429 that says when it lifts (up to 3 waits per call, not counted as retries), so one long queue uses each hour's allowance as it opens. |

## Measurements

| What | Number |
|---|---|
| Coverage of a route from its profile | 3 min (coverage.py) to 0.5 s (profiles read directly, cached) |
| Capture, 744 small functions over 5 routes in parallel | 4 min 49 s, 11 GB (429 KB per snapshot; 496 functions with calls, median 40 calls) |
| Capture, 1,200 functions | 5 min 26 s, 13 GB (678 with calls) |
| Capture, 1,576 functions (selected from replayable routes) | 8 min 43 s, 32 GB (1,538 with calls) |
| Hyper glm-5.3-flash farm call (16 jobs) | median 5 s, p90 27 s; 1.8k tokens in, 1.4k out (85% reasoning); about 1 call/s |
| Hyper cost | about 0.023 units per farm call (23 units for 1,025 calls; 145k tokens per unit) |
| Hyper hourly limit | about 1,000 farm calls (3.3M tokens) per hour: the 217-unit day needs about 9 hours of steady use |
| Farm batch A (496 small functions with captured calls) | 16 min 20 s: 434 accepted, 14 failed every round, 48 skipped by the label bug; 97% of those tried |
| Strong models, one subsystem attempt (3 functions) | deepseek-v4-pro 280-350 s, 37-48k output (93% reasoning); kimi-k3 66-450 s; glm-5.3 1,195 s |
| Full verify of a converted file | about 5 min (build, difftest, oracle, 8 lockstep routes) |

## Still open

- **Snapshots are full machine states** (429 KB). Most of a farm function's
  calls touch little memory; storing only the regions a call reads, or
  compressing, would cut the 11-13 GB per batch.
- **Recapture:** a convert run captures again what an earlier run captured
  (`--snapshots` reuses one by hand). A cache keyed by function and route
  would make it automatic.
- **Batch selection is a script**, not a workflow step
  (executed on a replayable route, not hand-written, never farmed, by size).
- **Hyper's balance has whole-unit granularity**, so cost per call is only
  measurable over a few hundred calls.
- **Integrating farm candidates** goes through `wf convert --farm-model`, one
  file per group, with one full verify each; review of hundreds of small
  functions is the slowest part left.
- **Conversions run one at a time**: each installs its file into the shared
  tree and verifies with shared build directories. Separate git worktrees
  per run would let verifies run side by side (16 cores; one verify keeps
  about 8 busy).
