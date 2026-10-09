# farm

Outside models convert small lifted functions in volume (docs/PROCESS.md
§4). One `farm` run is one function and one model: the model
writes readable C, the farm's acceptance test checks it against the lifted
original on the captured calls and random inputs, failures go back to the
model for repair, and an accepted function gets its symbols/db entry drafted
by the same model. A `farm batch` run builds the host difftest once, skips
pairs that are already finished, and runs the rest as its child runs.

![farm](farm.png)

Runs are tracked in the Burr UI under project `recomp-farm`.

## Steps

### `gather_context` (step)

Find the function's assembly, lifted C and register contract, and its captured calls.

- reads: `function: FunctionName`, `snapshots: SnapshotDir`
- produces: `context: ConversionContext | NotRun`
- then: `ask_model_for_c` when `there_is_something_to_convert`; `give_up` when `there_is_nothing_to_convert`

### `ask_model_for_c` (step)

Ask the model for readable C; after a failure, show it the failure and its code.

- reads: `context: ConversionContext`, `model: ModelSpec`, `attempts: tuple[Attempt, ...]`
- produces: `attempts: tuple[Attempt, ...]`
- then: `test_reply` when `the_model_answered`; `give_up` when `the_model_failed`

### `give_up` (step)

Record why there is no accepted candidate.

- reads: `context: ConversionContext | NotRun`, `attempts: tuple[Attempt, ...]`
- produces: `outcome: FarmOutcome`
- then: the run ends

### `test_reply` (step)

Test the model's candidate the way farm.py accepts one, and log the round as farm.py does.

- reads: `context: ConversionContext`, `model: ModelSpec`, `snapshots: SnapshotDir`, `difftest: Path`, `attempts: tuple[Attempt, ...]`
- produces: `attempts: tuple[Attempt, ...]`
- then: `accept_candidate` when `the_candidate_passed`; `ask_model_for_c` when `it_failed_with_rounds_left`; `give_up` when `it_failed_with_no_rounds_left`

### `accept_candidate` (step)

Keep the passing candidate in tools/convert/accepted for review.

- reads: `function: FunctionName`, `model: ModelSpec`, `attempts: tuple[Attempt, ...]`
- produces: `accepted: Path`, `outcome: FarmOutcome`
- then: `draft_symbol_entry`

### `draft_symbol_entry` (step)

Have the same model draft the function's symbols/db entry, beside the candidate.

- reads: `model: ModelSpec`, `accepted: Path`
- produces: `draft: Draft`
- then: the run ends
