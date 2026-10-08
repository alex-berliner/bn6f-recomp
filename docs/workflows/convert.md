# convert

A strong outside model converts a subsystem (callers and callees together)
to readable C with C call and return semantics (docs/PROCESS.md §4): capture
the original's calls, ask the model for the whole file, quick-test it per
function against the lifted originals, install it and run the full verify,
then stop for the orchestrator's review. Failures and review comments go back
to the model (3 attempts per model, then the next model). Approved work is
committed, its symbols/db entries are written from the model's drafts plus the
run's own evidence, and a child record run renames and documents it.
The orchestrator never writes the code: it chooses the work and reviews.

![convert](convert.png)

Runs are tracked in the Burr UI under project `recomp-convert`.

## Steps

### `capture_original_calls` (step)

Save the original game's calls of the functions (a child capture run), unless given a capture.

- reads: `run_dir: Path`, `functions: tuple[FunctionName, ...]`, `routes: tuple[Route, ...]`, `snapshots: typing.Optional[SnapshotDir]`
- produces: `capture: CaptureReport | NotRun`, `snapshots: SnapshotDir`
- then: `gather_context`

### `gather_context` (step)

Each function's assembly, lifted C and register contract; build the host difftest for quick tests.

- reads: `run_dir: Path`, `file_name: str`, `functions: tuple[FunctionName, ...]`
- produces: `contexts: tuple[ConversionContext, ...] | NotRun`, `difftest: Path | BuildFailure`
- then: `ask_model_for_file_step` when `ready_to_convert`; `give_up` when `cannot_convert`

### `ask_model_for_file_step` (step)

Ask the next model for the whole file; after a failure or review, show it what went wrong.

- reads: `run_dir: Path`, `subsystem: str`, `file_name: str`, `contexts: tuple[ConversionContext, ...]`, `models: tuple[ModelSpec, ...]`, `rounds: int`, `candidate: Path | None`, `attempts: tuple[Attempt, ...]`
- produces: `attempts: tuple[Attempt, ...]`
- then: `quick_test`

### `give_up` (step)

Take the file out of the tree again and say why there is nothing to commit.

- reads: `file_name: str`, `contexts: tuple[ConversionContext, ...] | NotRun`, `difftest: Path | BuildFailure`, `attempts: tuple[Attempt, ...]`
- produces: `outcome: ConvertOutcome`
- then: the run ends

### `quick_test` (step)

Each function of the file against its lifted original on the captured calls and random inputs.

- reads: `contexts: tuple[ConversionContext, ...]`, `snapshots: SnapshotDir`, `difftest: Path`, `attempts: tuple[Attempt, ...]`
- produces: `attempts: tuple[Attempt, ...]`
- then: `install_and_verify` when `quick_test_passed`; `ask_model_for_file_step` when `quick_test_failed_with_attempts_left`; `give_up` when `quick_test_failed_with_no_attempts_left`

### `install_and_verify` (step)

Put the file in the tree and run every check (a child verify run); take it out again if it fails.

- reads: `run_dir: Path`, `file_name: str`, `contexts: tuple[ConversionContext, ...]`, `snapshots: SnapshotDir`, `routes: tuple[Route, ...]`, `battle_code: bool`, `attempts: tuple[Attempt, ...]`
- produces: `attempts: tuple[Attempt, ...]`, `checks: tuple[FunctionChecks, ...]`
- then: `review_conversion` when `verified`; `ask_model_for_file_step` when `not_verified_with_attempts_left`; `give_up` when `not_verified_with_no_attempts_left`

### `review_conversion` (pause point)

The orchestrator reviews: approve, request changes (they go back to the model) or reject.

- reads: `subsystem: str`, `file_name: str`, `attempts: tuple[Attempt, ...]`
- waits for: `Review` (`decision: ReviewDecision`, `comments: str`, `attribution: str`)
- produces: `attempts: tuple[Attempt, ...]`
- then: `commit_conversion` when `approved`; `ask_model_for_file_step` when `changes_requested_with_attempts_left`; `give_up` when `rejected_or_no_attempts_left`

### `commit_conversion` (step)

Commit the file and its difftest.cfg lines locally (never pushed).

- reads: `subsystem: str`, `file_name: str`, `functions: tuple[FunctionName, ...]`, `attempts: tuple[Attempt, ...]`
- produces: `commit: CommitHash`
- then: `write_symbol_entries`

### `write_symbol_entries` (step)

symbols/db entries from the model's drafts plus this run's evidence; renames for confirmed/probable meanings.

- reads: `subsystem: str`, `file_name: str`, `attempts: tuple[Attempt, ...]`, `checks: tuple[FunctionChecks, ...]`, `commit: CommitHash`
- produces: `entries: tuple[Path, ...]`, `renames: tuple[Rename, ...]`
- then: `record_symbols`

### `record_symbols` (step)

Check the entries, rename in bn6f, write the comments, re-verify, commit (a child record run).

- reads: `run_dir: Path`, `subsystem: str`, `functions: tuple[FunctionName, ...]`, `renames: tuple[Rename, ...]`, `attempts: tuple[Attempt, ...]`
- produces: `record: RecordReport`, `outcome: ConvertOutcome`
- then: the run ends
