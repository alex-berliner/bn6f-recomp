# farm-batch

Outside models convert small lifted functions in volume (docs/PROCESS.md
§4). One `farm` run is one function and one model: the model
writes readable C, the farm's acceptance test checks it against the lifted
original on the captured calls and random inputs, failures go back to the
model for repair. (Entries for accepted functions are drafted afterwards by `wf drafts --drafter`, on a
plan's quota rather than the farm model's.) Earlier: an accepted function got its symbols/db entry drafted
by the same model. A `farm batch` run builds the host difftest once, skips
pairs that are already finished, and runs the rest as its child runs.

![farm-batch](farm-batch.png)

Runs are tracked in the Burr UI under project `recomp-farm`.

## Steps

### `build_difftest` (step)

Build the host difftest once, from the last commit in a worktree of its own (a stale one once
accepted bad candidates; one built from the main tree saw a conversion's verify half done).

- reads: `run_dir: Path`
- produces: `difftest: Path | BuildFailure`
- then: `convert_each_pair` when `difftest_built`; `summarise` when `difftest_did_not_build`

### `convert_each_pair` (step)

Run one farm run per function and model (in parallel), skipping pairs finished before
(accepted, or out of rounds: farm.py's resume rule).

- reads: `functions: tuple[FunctionName, ...]`, `models: tuple[ModelSpec, ...]`, `snapshots: SnapshotDir`, `rounds: int`, `jobs: int`, `difftest: Path`
- produces: `pairs: tuple[PairResult, ...]`
- then: `summarise`

### `summarise` (step)

How many functions each model converted (tools/convert/scorecard.py has the cost).

- reads: `models: tuple[ModelSpec, ...]`, `pairs: tuple[PairResult, ...]`
- produces: `tallies: tuple[ModelTally, ...]`
- then: the run ends
