# From docs/PROCESS.md to the workflows

Where each part of `docs/PROCESS.md` went when the process moved into the
typed workflows in `tools/wf` (pages and diagrams: `docs/workflows/`, live
runs: the Burr UI). **For review:** once this is agreed, the sections marked
"moved" are replaced in PROCESS.md by links to their workflow pages; sections
marked "stays" keep their prose.

| PROCESS.md | What it says | Where it is now |
|---|---|---|
| §1 Decisions: Goal, Conversion split | readable C first; Claude converts subsystems, outside models leaf functions | **stays** (rationale). Enacted by the two workflows: [convert](workflows/convert.md) (Claude) and [farm](workflows/farm.md) (models) |
| §1 Git | local only, never push, commit on branches | **enforced**: `wf/tools/git.py` can only commit locally (there is no push) |
| §1 BIOS | real BIOS everywhere | **unchanged**: the wrapped tools (lockstep.sh, capture.sh, oracle.py) already use it |
| §1 Disassembly | bn6f fixes on `recomp-fixes`, ROM byte-identical | **enforced** by the tools [record](workflows/record.md) runs (`rename_symbol.py`, `symdoc.py` check the ROM) |
| §1 Evidence | a reliable test method | **moved**: [verify](workflows/verify.md) |
| §1 Recordings | ask the user for recordings | **stays** (recording is interactive: `tools/record.sh`) |
| §1 Symbols | every discovery into bn6f and symbols/db | **moved**: [record](workflows/record.md); [convert](workflows/convert.md) ends by pointing to it |
| §1 Coverage | prefer functions the routes execute | **moved**: [capture](workflows/capture.md) `choose_route` picks the route whose profile runs them |
| §1 Model cost, Pacers, Secrets | cost per function; leave pacers alone; never print keys | **stays**; farm keeps logging to `usage.jsonl` / `tools/convert/log` so `scorecard.py` still counts |
| §1 Flash cart | paused | **stays** |
| §2 Build | build_lifted.sh, make, host make | **moved**: verify `lift_and_build_everything` (own build dirs `cport/build-wf`, `cport/build-wf-host`); the commands stay listed for hand use |
| §2 Layout | where things live | **stays** |
| §2 Writing readable code | macros, LDS16, mirroring rule, compare= | **stays** (it is what the person or model writing code reads; farm's prompt still takes it from `farm.py`); future: `docs/C_CALLING_CONVENTION.md` |
| §2 IWRAM, boot, speed | memory layout, speed numbers | **stays** |
| §3 order of checks | difftest random, captured, oracle, lockstep, battle sweep | **moved**: [verify](workflows/verify.md) steps in that order, each later check only after the earlier ones pass |
| §3 "say so when a check compared nothing" | report out-of-domain runs | **enforced**: `Outcome.COMPARED_NOTHING`, report field `never_compared`, verdict "passed, but some functions were never compared" |
| §3 host difftest captured calls | capture.sh then `difftest -S` | **moved**: [capture](workflows/capture.md), then verify `difftest_captured_calls` |
| §3 CPU oracle + `tools/oracle_triage.md` | known artifact classes | **moved** (the run): verify `run_cpu_oracle`; differences give verdict "needs triage". The triage knowledge **stays** in `oracle_triage.md` (Claude reads it; not yet a classifier) |
| §3 lockstep, battle sweep | all routes; battle sweep for battle code | **moved**: verify `lockstep_routes`, `sweep_battles` (when `--battle-code`) |
| §3 other sweeps | mapsweep, tracesweep, chipsweep, fuzzsweep | **not yet in a workflow** |
| §3 coverage | coverage.py | **moved** (for choosing routes): capture `choose_route` |
| §3 recorded routes table | names and frame counts | **moved**: `tools/wf/wf/routes.py` (`ALL_ROUTES`) |
| §3 new recordings | record.sh, check_recording.sh, SESSIONS.md | **stays** (interactive with the user) |
| §4 By Claude, steps 1–4 | pick subsystem, write, verify, commit, then §6 | **moved**: [convert](workflows/convert.md) (pause point `write_readable_c`, child verify, commit), then [record](workflows/record.md) |
| §4 By outside models, steps 1–2 | capture 40 calls; one farm per provider | **moved**: [capture](workflows/capture.md), [farm-batch](workflows/farm-batch.md) (one run per function and model, as child runs) |
| §4 By outside models, step 3 | review: pick the clearest, check fallbacks, LDS16, flags; integrate | **not yet in a workflow** (Claude does it by hand; a review pause point is the natural next step) |
| §4 cost: quota.py before and after | quota per provider | **not yet in a workflow** |
| §4 providers, results tables | model results | **stays** (data; `scorecard.py` is the live version) |
| §5 Readable code so far | what exists | **stays** (a snapshot; `symbols/SYMBOLS.md` is the live list) |
| §6 symbol database and disassembly | symdb check/render, rename_symbol, symdoc, import_symbols, re-verify | **moved**: [record](workflows/record.md) (renames only for confirmed/probable, re-verify after renames). Building bn6f by hand **stays** as reference |
| §7 Lessons and pitfalls | shell, mawk, mGBA quirks, oracle and lockstep limits | **stays**; the ones about tools are now handled in the wrappers (exit codes not trusted where the tools do not report failure) |
| §8 Flash cart | paused | **stays** |
| §9 The self-scheduled loop | keep-working prompt | **stays**; it can now start with `python -m wf status` |
| State when written | snapshot | **replaced** by `python -m wf status` and the Burr UI |
