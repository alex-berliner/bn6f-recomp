"""Verify the main checkout: build the port ROM, difftest the changed
functions against their lifted originals, then replay recorded routes in
lockstep with the original game (docs/PROCESS.md §3)."""
from enum import Enum
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Transition, TypedApplication, Workflow
from wf.repo import current_sources
from wf.tools.build import build_rom
from wf.tools.difftest import run_difftest
from wf.tools.lockstep import run_lockstep_routes
from wf.types import (BuildFailure, BuiltRom, DifftestResult, FunctionName, LockstepResult, Outcome,
                      Result, Route, RouteName, SourceVersion)

PROJECT = "recomp-verify"


def verify_application(start: "VerifyState", run_id: str, tracked: bool = True) -> TypedApplication["VerifyState"]:
    return workflow.application(
        start=start,
        run_id=run_id,
        tracked=tracked,
        first=note_sources,
        transitions=[
            Transition(after=note_sources, go_to=build),
            Transition(after=build, go_to=difftest_changed_functions, when=build_succeeded),
            Transition(after=build, go_to=write_report, when=build_failed),
            Transition(after=difftest_changed_functions, go_to=lockstep_routes, when=no_difftest_failed),
            Transition(after=difftest_changed_functions, go_to=write_report, when=some_difftest_failed),
            Transition(after=lockstep_routes, go_to=write_report),
        ],
    )


def run_verify(start: "VerifyState", run_id: str) -> "VerifyReport":
    final_state = verify_application(start, run_id).run_until_after(write_report)
    if final_state.report is None:
        raise RuntimeError("verify finished without a report")
    return final_state.report


# ── the run's state ────────────────────────────────────────────────────

class Verdict(Enum):
    PASSED = "passed"
    PASSED_WITH_UNCHECKED_FUNCTIONS = "passed, but some functions were never compared"
    FAILED = "failed"
    BUILD_FAILED = "build failed"


class VerifyReport(Result):
    sources: SourceVersion
    verdict: Verdict
    build_failure: BuildFailure | None  # why the build failed, when it did
    failed_functions: tuple[FunctionName, ...]
    unchecked_functions: tuple[FunctionName, ...]  # difftest compared nothing: every trial out of domain
    failed_routes: tuple[RouteName, ...]
    routes_checked: tuple[RouteName, ...]


class VerifyState(BaseModel):
    """Everything a verify run knows; each step sees only the part its input model names."""
    run_dir: Path
    functions: tuple[FunctionName, ...]
    routes: tuple[Route, ...]
    trials: int
    sources: SourceVersion | None = None
    build: BuiltRom | BuildFailure | None = None
    difftests: tuple[DifftestResult, ...] = ()
    locksteps: tuple[LockstepResult, ...] = ()
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
def build(given: BuildInput) -> BuildOutput:
    """Build the port ROM into the workflow's own build directory."""
    return BuildOutput(build=build_rom(given.run_dir))


class DifftestInput(Result):
    functions: tuple[FunctionName, ...]
    trials: int
    run_dir: Path


class DifftestOutput(Result):
    difftests: tuple[DifftestResult, ...]


@workflow.step
def difftest_changed_functions(given: DifftestInput) -> DifftestOutput:
    """Compare each changed function with its lifted original on random inputs."""
    return DifftestOutput(difftests=run_difftest(given.functions, given.trials, given.run_dir))


class LockstepInput(Result):
    build: BuiltRom  # only a successful build reaches this step
    routes: tuple[Route, ...]
    run_dir: Path


class LockstepOutput(Result):
    locksteps: tuple[LockstepResult, ...]


@workflow.step
def lockstep_routes(given: LockstepInput) -> LockstepOutput:
    """Replay each recorded route on the original game and the port side by side."""
    return LockstepOutput(locksteps=run_lockstep_routes(given.build.rom, given.routes, given.run_dir))


class ReportInput(Result):
    sources: SourceVersion
    build: BuiltRom | BuildFailure
    difftests: tuple[DifftestResult, ...]
    locksteps: tuple[LockstepResult, ...]
    routes: tuple[Route, ...]


class ReportOutput(Result):
    report: VerifyReport


@workflow.step
def write_report(given: ReportInput) -> ReportOutput:
    """Sum up what passed, what failed and what was never compared."""
    return ReportOutput(report=VerifyReport(
        sources=given.sources,
        verdict=_verdict(given),
        build_failure=given.build if isinstance(given.build, BuildFailure) else None,
        failed_functions=tuple(result.function for result in given.difftests
                               if result.outcome is Outcome.FAILED),
        unchecked_functions=tuple(result.function for result in given.difftests
                                  if result.outcome is Outcome.COMPARED_NOTHING),
        failed_routes=tuple(result.route for result in given.locksteps if not result.passed),
        routes_checked=tuple(result.route for result in given.locksteps),
    ))


def _verdict(given: ReportInput) -> Verdict:
    if isinstance(given.build, BuildFailure):
        return Verdict.BUILD_FAILED
    difftest_failed = any(result.outcome is Outcome.FAILED for result in given.difftests)
    lockstep_failed = any(not result.passed for result in given.locksteps)
    if difftest_failed or lockstep_failed or len(given.locksteps) < len(given.routes):
        return Verdict.FAILED
    if any(result.outcome is Outcome.COMPARED_NOTHING for result in given.difftests):
        return Verdict.PASSED_WITH_UNCHECKED_FUNCTIONS
    return Verdict.PASSED


# ── decisions ──────────────────────────────────────────────────────────

def build_succeeded(state: VerifyState) -> bool:
    return isinstance(state.build, BuiltRom)


def build_failed(state: VerifyState) -> bool:
    return isinstance(state.build, BuildFailure)


def some_difftest_failed(state: VerifyState) -> bool:
    return any(result.outcome is Outcome.FAILED for result in state.difftests)


def no_difftest_failed(state: VerifyState) -> bool:
    """Functions difftest could not compare do not stop the run; the report lists them."""
    return not some_difftest_failed(state)
