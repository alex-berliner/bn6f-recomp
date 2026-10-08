"""Claude converts a subsystem (callers and callees together) to readable C
(docs/PROCESS.md §4, "By Claude"): capture the original's calls of the
functions, wait for Claude to write cport/src/game/*.c, verify (a child
verify run), and either go back to Claude with the report or commit.
Afterwards the record workflow puts the findings into the symbol database."""
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Graph, Transition, Workflow, current_run_id
from wf.graphs.capture import CaptureReport, CaptureState, run_capture
from wf.graphs.verify import Verdict, VerifyReport, VerifyState, run_verify
from wf.routes import ALL_ROUTES
from wf.tools.git import commit_files
from wf.types import CommitHash, FunctionName, Result, Route, SnapshotDir

PROJECT = "recomp-convert"
ACCEPTED_VERDICTS = (Verdict.PASSED, Verdict.PASSED_WITH_UNCHECKED_FUNCTIONS)


def convert_graph() -> Graph["ConvertState"]:
    return workflow.graph(
        first=capture_original_calls,
        ends=[commit_conversion],
        transitions=[
            Transition(after=capture_original_calls, go_to=write_readable_c),
            Transition(after=write_readable_c, go_to=verify_conversion),
            Transition(after=verify_conversion, go_to=commit_conversion, when=verified),
            Transition(after=verify_conversion, go_to=write_readable_c, when=not_verified),
        ],
    )


# ── the run's state ────────────────────────────────────────────────────

class WrittenCode(Result):
    """Claude's answer at the pause point: what it wrote and how to commit it."""
    files: tuple[Path, ...]          # the readable C (and anything else) to commit
    battle_code: bool                # the change touches battle code: verify runs the battle sweep
    commit_message: str              # in full, attribution lines included
    notes: str = ""                  # what changed since the last attempt, for the record


class ConvertState(BaseModel):
    run_dir: Path
    subsystem: str
    functions: tuple[FunctionName, ...]
    routes: tuple[Route, ...] = ALL_ROUTES
    capture: CaptureReport | None = None
    written: WrittenCode | None = None
    verification: VerifyReport | None = None
    verify_runs: tuple[str, ...] = ()
    commit: CommitHash | None = None


workflow = Workflow[ConvertState](ConvertState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class CaptureInput(Result):
    run_dir: Path
    functions: tuple[FunctionName, ...]
    routes: tuple[Route, ...]


class CaptureOutput(Result):
    capture: CaptureReport


@workflow.step
def capture_original_calls(given: CaptureInput) -> CaptureOutput:
    """Save the original game's calls of the functions (a child capture run)."""
    run_id = f"{current_run_id()}-capture"
    start = CaptureState(run_dir=given.run_dir, functions=given.functions,
                         snapshots=SnapshotDir(given.run_dir / "snapshots"), candidate_routes=given.routes)
    return CaptureOutput(capture=run_capture(start, run_id))


class CodeRequest(Result):
    """What Claude is shown at the pause point."""
    subsystem: str
    functions: tuple[FunctionName, ...]
    capture: CaptureReport
    verification: VerifyReport | None  # the last verify report, when going round again


class CodeOutput(Result):
    written: WrittenCode


@workflow.pause_point
def write_readable_c(given: CodeRequest, answer: WrittenCode) -> CodeOutput:
    """Claude writes the readable C (cport/src/game) for the functions, or fixes it after a failed verify."""
    return CodeOutput(written=answer)


class VerifyInput(Result):
    run_dir: Path
    functions: tuple[FunctionName, ...]
    capture: CaptureReport
    routes: tuple[Route, ...]
    written: WrittenCode
    verify_runs: tuple[str, ...]


class VerifyOutput(Result):
    verification: VerifyReport
    verify_runs: tuple[str, ...]


@workflow.step
def verify_conversion(given: VerifyInput) -> VerifyOutput:
    """Verify the readable functions with every check (a child verify run)."""
    run_id = f"{current_run_id()}-verify-{len(given.verify_runs) + 1}"
    run_dir = given.run_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    start = VerifyState(run_dir=run_dir, functions=given.functions, snapshots=given.capture.snapshots,
                        routes=given.routes, battle_code=given.written.battle_code)
    return VerifyOutput(verification=run_verify(start, run_id), verify_runs=(*given.verify_runs, run_id))


class CommitInput(Result):
    written: WrittenCode


class CommitOutput(Result):
    commit: CommitHash


@workflow.step
def commit_conversion(given: CommitInput) -> CommitOutput:
    """Commit the readable code locally (never pushed)."""
    return CommitOutput(commit=commit_files(given.written.files, given.written.commit_message))


# ── decisions ──────────────────────────────────────────────────────────

def verified(state: ConvertState) -> bool:
    return state.verification is not None and state.verification.verdict in ACCEPTED_VERDICTS


def not_verified(state: ConvertState) -> bool:
    return not verified(state)
