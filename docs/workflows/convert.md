# convert

Claude converts a subsystem (callers and callees together) to readable C
(docs/PROCESS.md §4, "By Claude"): capture the original's calls of the
functions, wait for Claude to write cport/src/game/*.c, verify (a child
verify run), and either go back to Claude with the report or commit.
Afterwards the record workflow puts the findings into the symbol database.

![convert](convert.png)

Runs are tracked in the Burr UI under project `recomp-convert`.

## Steps

### `capture_original_calls` (step)

Save the original game's calls of the functions (a child capture run).

- reads: `run_dir: Path`, `functions: tuple[FunctionName, ...]`, `routes: tuple[Route, ...]`
- produces: `capture: CaptureReport`
- then: `write_readable_c`

### `write_readable_c` (pause point)

Claude writes the readable C (cport/src/game) for the functions, or fixes it after a failed verify.

- reads: `subsystem: str`, `functions: tuple[FunctionName, ...]`, `capture: CaptureReport`, `verification: VerifyReport | None`
- waits for: `WrittenCode` (`files: tuple[Path, ...]`, `battle_code: bool`, `commit_message: str`, `notes: str`)
- produces: `written: WrittenCode`
- then: `verify_conversion`

### `verify_conversion` (step)

Verify the readable functions with every check (a child verify run).

- reads: `run_dir: Path`, `functions: tuple[FunctionName, ...]`, `capture: CaptureReport`, `routes: tuple[Route, ...]`, `written: WrittenCode`, `verify_runs: tuple[str, ...]`
- produces: `verification: VerifyReport`, `verify_runs: tuple[str, ...]`
- then: `commit_conversion` when `verified`; `write_readable_c` when `not_verified`

### `commit_conversion` (step)

Commit the readable code locally (never pushed).

- reads: `written: WrittenCode`
- produces: `commit: CommitHash`
- then: the run ends
