"""Verify the main checkout (docs/PROCESS.md §3): lift and build, difftest the
changed functions against their lifted originals on random inputs and on the
original game's captured calls, run them on the CPU oracle, replay every
recorded route in lockstep, and the battle sweep when battle code changed."""
from enum import Enum
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow
from wf.repo import current_sources
from wf.tools.battlesweep import run_battle_sweep
from wf.tools.build import lift_and_build
from wf.tools.difftest import run_captured_difftest, run_difftest
from wf.tools.lockstep import run_lockstep_routes
from wf.tools.oracle import run_oracle
from wf.types import (BattleSweepResult, BuildFailure, BuiltRom, CapturedDifftestResult, DifftestResult,
                      FunctionName, LockstepResult, NotRun, OracleResult, Outcome, Result, Route, RouteName,
                      SnapshotDir, SourceVersion, ToolMessage)

PROJECT = "recomp-verify"


def verify_graph() -> Graph["VerifyState"]:
    return workflow.graph(
        first=note_sources,
        ends=[write_report],
        transitions=[
            Transition(after=note_sources, go_to=lift_and_build_everything),
            Transition(after=lift_and_build_everything, go_to=difftest_random_inputs, when=build_succeeded),
            Transition(after=lift_and_build_everything, go_to=write_report, when=build_failed),
            Transition(after=difftest_random_inputs, go_to=difftest_captured_calls),
            Transition(after=difftest_captured_calls, go_to=run_cpu_oracle, when=no_difftest_failed),
            Transition(after=difftest_captured_calls, go_to=write_report, when=some_difftest_failed),
            Transition(after=run_cpu_oracle, go_to=lockstep_routes, when=oracle_saw_no_difference),
            Transition(after=run_cpu_oracle, go_to=write_report, when=oracle_saw_differences),
            Transition(after=lockstep_routes, go_to=sweep_battles, when=battle_code_changed),
            Transition(after=lockstep_routes, go_to=write_report, when=battle_code_unchanged),
            Transition(after=sweep_battles, go_to=write_report),
        ],
    )


def run_verify(start: "VerifyState", run_id: str, tracked: bool = True) -> "VerifyReport":
    outcome = verify_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"verify run {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class Verdict(Enum):
    PASSED = "passed"
    PASSED_WITH_UNCHECKED_FUNCTIONS = "passed, but some functions were never compared"
    NEEDS_TRIAGE = "the CPU oracle saw differences (artifact or real? see tools/oracle_triage.md)"
    FAILED = "failed"
    BUILD_FAILED = "build failed"


class VerifyReport(Result):
    sources: SourceVersion
    verdict: Verdict
    build_failure: BuildFailure | None
    failed_functions: tuple[FunctionName, ...]       # difftest found a difference
    oracle_differences: tuple[FunctionName, ...]     # the CPU oracle found a difference
    never_compared: tuple[FunctionName, ...]         # no check compared anything for these
    routes_checked: tuple[RouteName, ...]
    failed_routes: tuple[RouteName, ...]
    battle_sweep: BattleSweepResult | NotRun | None


class VerifyState(BaseModel):
    """Everything a verify run knows; each step sees only the part its input model names."""
    run_dir: Path
    functions: tuple[FunctionName, ...]
    snapshots: SnapshotDir | None             # captured calls of the functions (tools/convert/capture.sh)
    routes: tuple[Route, ...]
    battle_code: bool                         # the change touches battle code: run the battle sweep
    trials: int = 300                         # random difftest inputs per function
    oracle_states: int = 24                   # entry states per function on the CPU oracle
    sources: SourceVersion | None = None
    build: BuiltRom | BuildFailure | None = None
    difftests: tuple[DifftestResult, ...] = ()
    captured_difftests: tuple[CapturedDifftestResult, ...] | NotRun | None = None
    oracle: tuple[OracleResult, ...] | NotRun | None = None
    locksteps: tuple[LockstepResult, ...] = ()
    battle_sweep: BattleSweepResult | NotRun | None = None
    report: VerifyReport | None = None


workflow = Workflow[VerifyState](VerifyState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class Nothing(Result):
    pass


class SourcesOutput(Result):
    sources: SourceVersion


@workflow.step
def note_sources(given: Nothing) -> SourcesOutput:
    """Record which commit is being verified, and whether it has uncommitted changes."""
    return SourcesOutput(sources=current_sources())


class BuildInput(Result):
    run_dir: Path


class BuildOutput(Result):
    build: BuiltRom | BuildFailure


@workflow.step
def lift_and_build_everything(given: BuildInput) -> BuildOutput:
    """Lift bn6f with the readable functions in place; build the port ROM and the host difftest."""
    return BuildOutput(build=lift_and_build(given.run_dir))


class RandomDifftestInput(Result):
    build: BuiltRom
    functions: tuple[FunctionName, ...]
    trials: int
    run_dir: Path


class RandomDifftestOutput(Result):
    difftests: tuple[DifftestResult, ...]


@workflow.step
def difftest_random_inputs(given: RandomDifftestInput) -> RandomDifftestOutput:
    """Compare each changed function with its lifted original on random inputs."""
    return RandomDifftestOutput(
        difftests=run_difftest(given.build.difftest, given.functions, given.trials, given.run_dir))


class CapturedDifftestInput(Result):
    build: BuiltRom
    functions: tuple[FunctionName, ...]
    snapshots: SnapshotDir | None
    run_dir: Path


class CapturedDifftestOutput(Result):
    captured_difftests: tuple[CapturedDifftestResult, ...] | NotRun


@workflow.step
def difftest_captured_calls(given: CapturedDifftestInput) -> CapturedDifftestOutput:
    """Compare each changed function with its lifted original on the original game's own calls."""
    if given.snapshots is None:
        return CapturedDifftestOutput(captured_difftests=NotRun(reason=ToolMessage("no captured calls given")))
    return CapturedDifftestOutput(captured_difftests=run_captured_difftest(
        given.build.difftest, given.functions, given.snapshots, given.run_dir))


class OracleInput(Result):
    build: BuiltRom
    functions: tuple[FunctionName, ...]
    snapshots: SnapshotDir | None
    oracle_states: int
    run_dir: Path


class OracleOutput(Result):
    oracle: tuple[OracleResult, ...] | NotRun


@workflow.step
def run_cpu_oracle(given: OracleInput) -> OracleOutput:
    """Run each changed function on the original game and on the port from the same real states."""
    if given.snapshots is None:
        return OracleOutput(oracle=NotRun(reason=ToolMessage("no captured calls to build entry states from")))
    return OracleOutput(oracle=run_oracle(given.build.difftest, given.functions, given.snapshots,
                                          given.oracle_states, given.run_dir))


class LockstepInput(Result):
    build: BuiltRom
    routes: tuple[Route, ...]
    run_dir: Path


class LockstepOutput(Result):
    locksteps: tuple[LockstepResult, ...]


@workflow.step
def lockstep_routes(given: LockstepInput) -> LockstepOutput:
    """Replay each recorded route on the original game and the port side by side."""
    return LockstepOutput(locksteps=run_lockstep_routes(given.build.rom, given.routes, given.run_dir))


class BattleSweepInput(Result):
    build: BuiltRom
    run_dir: Path


class BattleSweepOutput(Result):
    battle_sweep: BattleSweepResult


@workflow.step
def sweep_battles(given: BattleSweepInput) -> BattleSweepOutput:
    """Replay the 116 recorded battles in lockstep (battle code changed)."""
    return BattleSweepOutput(battle_sweep=run_battle_sweep(given.build.rom, jobs=6, log_dir=given.run_dir))


class ReportInput(Result):
    sources: SourceVersion
    build: BuiltRom | BuildFailure
    functions: tuple[FunctionName, ...]
    difftests: tuple[DifftestResult, ...]
    captured_difftests: tuple[CapturedDifftestResult, ...] | NotRun | None
    oracle: tuple[OracleResult, ...] | NotRun | None
    routes: tuple[Route, ...]
    locksteps: tuple[LockstepResult, ...]
    battle_code: bool
    battle_sweep: BattleSweepResult | NotRun | None


class ReportOutput(Result):
    report: VerifyReport


@workflow.step
def write_report(given: ReportInput) -> ReportOutput:
    """Sum up what passed, what failed, what needs triage and what was never compared."""
    captured = given.captured_difftests if isinstance(given.captured_difftests, tuple) else ()
    oracle = given.oracle if isinstance(given.oracle, tuple) else ()
    failed = {result.function for result in (*given.difftests, *captured) if result.outcome is Outcome.FAILED}
    compared = {result.function for result in (*given.difftests, *captured, *oracle)
                if result.outcome is Outcome.PASSED}
    battle_sweep = given.battle_sweep if given.battle_code else NotRun(reason=ToolMessage("no battle code changed"))
    report = VerifyReport(
        sources=given.sources,
        verdict=Verdict.PASSED,
        build_failure=given.build if isinstance(given.build, BuildFailure) else None,
        failed_functions=tuple(function for function in given.functions if function in failed),
        oracle_differences=tuple(result.function for result in oracle if result.outcome is Outcome.FAILED),
        never_compared=tuple(function for function in given.functions
                             if function not in compared and function not in failed),
        routes_checked=tuple(result.route for result in given.locksteps),
        failed_routes=tuple(result.route for result in given.locksteps if not result.passed),
        battle_sweep=battle_sweep,
    )
    return ReportOutput(report=report.model_copy(update={"verdict": _verdict(given, report)}))


def _verdict(given: ReportInput, report: VerifyReport) -> Verdict:
    if report.build_failure is not None:
        return Verdict.BUILD_FAILED
    if report.failed_functions:
        return Verdict.FAILED
    if report.oracle_differences:
        return Verdict.NEEDS_TRIAGE
    sweep_failed = isinstance(given.battle_sweep, BattleSweepResult) and not given.battle_sweep.passed
    if report.failed_routes or len(report.routes_checked) < len(given.routes) or sweep_failed:
        return Verdict.FAILED
    if report.never_compared:
        return Verdict.PASSED_WITH_UNCHECKED_FUNCTIONS
    return Verdict.PASSED


# ── decisions ──────────────────────────────────────────────────────────

def build_succeeded(state: VerifyState) -> bool:
    return isinstance(state.build, BuiltRom)


def build_failed(state: VerifyState) -> bool:
    return isinstance(state.build, BuildFailure)


def some_difftest_failed(state: VerifyState) -> bool:
    captured = state.captured_difftests if isinstance(state.captured_difftests, tuple) else ()
    return any(result.outcome is Outcome.FAILED for result in (*state.difftests, *captured))


def no_difftest_failed(state: VerifyState) -> bool:
    """Functions difftest could not compare do not stop the run; the report lists them."""
    return not some_difftest_failed(state)


def oracle_saw_differences(state: VerifyState) -> bool:
    return isinstance(state.oracle, tuple) and any(result.outcome is Outcome.FAILED for result in state.oracle)


def oracle_saw_no_difference(state: VerifyState) -> bool:
    return not oracle_saw_differences(state)


def battle_code_changed(state: VerifyState) -> bool:
    return state.battle_code


def battle_code_unchanged(state: VerifyState) -> bool:
    return not state.battle_code
