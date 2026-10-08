"""Verify the main checkout: build the port ROM, difftest the changed
functions against their lifted originals, then replay recorded routes in
lockstep with the original game (docs/PROCESS.md §3)."""
from enum import Enum
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, ConfigDict

from wf.burr_typed import Transition, TypedApplication, Workflow
from wf.repo import current_sources
from wf.tools.build import build_rom
from wf.tools.difftest import run_difftest
from wf.tools.lockstep import run_lockstep_routes
from wf.types import (BuildFailure, BuiltRom, DifftestResult, FunctionName, LockstepResult, Outcome,
                      Result, Route, RouteName, SourceVersion)

PROJECT = "recomp-verify"


def verify_application(start: "VerifyState", run_id: str) -> TypedApplication["VerifyState"]:
    return workflow.application(
        start=start,
        run_id=run_id,
        first=note_sources,
        transitions=[
            Transition(after=note_sources, go_to=build),
            Transition(after=build, go_to=difftest_changed_functions, when=build_succeeded),
            Transition(after=build, go_to=write_report),
            Transition(after=difftest_changed_functions, go_to=lockstep_routes, when=no_difftest_failed),
            Transition(after=difftest_changed_functions, go_to=write_report),
            Transition(after=lockstep_routes, go_to=write_report),
        ],
    )


def run_verify(start: "VerifyState", run_id: str) -> "VerifyReport":
    final_state = verify_application(start, run_id).run_until_after(write_report)
    if final_state.report is None:
        raise RuntimeError("verify finished without a report")
    return final_state.report


# ── state ──────────────────────────────────────────────────────────────

class Verdict(Enum):
    PASSED = "passed"
    PASSED_WITH_UNCHECKED_FUNCTIONS = "passed, but some functions were never compared"
    FAILED = "failed"
    BUILD_FAILED = "build failed"


class VerifyReport(Result):
    sources: SourceVersion
    verdict: Verdict
    failed_functions: tuple[FunctionName, ...]
    unchecked_functions: tuple[FunctionName, ...]  # difftest compared nothing: every trial out of domain
    failed_routes: tuple[RouteName, ...]
    routes_checked: tuple[RouteName, ...]


class VerifyState(BaseModel):
    model_config = ConfigDict(validate_assignment=True)

    run_dir: Path
    functions: tuple[FunctionName, ...]
    routes: tuple[Route, ...]
    trials: int
    sources: SourceVersion | None = None
    build: BuiltRom | BuildFailure | None = None
    difftests: tuple[DifftestResult, ...] = ()
    locksteps: tuple[LockstepResult, ...] = ()
    report: VerifyReport | None = None


VerifyField = Literal["run_dir", "functions", "routes", "trials", "sources",
                      "build", "difftests", "locksteps", "report"]

workflow = Workflow[VerifyField, VerifyState](VerifyState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

@workflow.step(reads=[], writes=["sources"])
def note_sources(state: VerifyState) -> VerifyState:
    """Record which commit is being verified, and whether it has uncommitted changes."""
    state.sources = current_sources()
    return state


@workflow.step(reads=["run_dir"], writes=["build"])
def build(state: VerifyState) -> VerifyState:
    """Build the port ROM into the workflow's own build directory."""
    state.build = build_rom(state.run_dir)
    return state


@workflow.step(reads=["functions", "trials", "run_dir"], writes=["difftests"])
def difftest_changed_functions(state: VerifyState) -> VerifyState:
    """Compare each changed function with its lifted original on random inputs."""
    state.difftests = run_difftest(state.functions, state.trials, state.run_dir)
    return state


@workflow.step(reads=["build", "routes", "run_dir"], writes=["locksteps"])
def lockstep_routes(state: VerifyState) -> VerifyState:
    """Replay each recorded route on the original game and the port side by side."""
    state.locksteps = run_lockstep_routes(_built_rom(state).rom, state.routes, state.run_dir)
    return state


@workflow.step(reads=["sources", "build", "difftests", "locksteps", "routes"], writes=["report"])
def write_report(state: VerifyState) -> VerifyState:
    """Sum up what passed, what failed and what was never compared."""
    if state.sources is None:
        raise RuntimeError("write_report ran before note_sources")
    state.report = VerifyReport(
        sources=state.sources,
        verdict=_verdict(state),
        failed_functions=tuple(result.function for result in state.difftests
                               if result.outcome is Outcome.FAILED),
        unchecked_functions=tuple(result.function for result in state.difftests
                                  if result.outcome is Outcome.COMPARED_NOTHING),
        failed_routes=tuple(result.route for result in state.locksteps if not result.passed),
        routes_checked=tuple(result.route for result in state.locksteps),
    )
    return state


def _built_rom(state: VerifyState) -> BuiltRom:
    if not isinstance(state.build, BuiltRom):
        raise RuntimeError("lockstep_routes ran without a built ROM")
    return state.build


def _verdict(state: VerifyState) -> Verdict:
    if not isinstance(state.build, BuiltRom):
        return Verdict.BUILD_FAILED
    difftest_failed = any(result.outcome is Outcome.FAILED for result in state.difftests)
    lockstep_failed = any(not result.passed for result in state.locksteps)
    if difftest_failed or lockstep_failed or len(state.locksteps) < len(state.routes):
        return Verdict.FAILED
    if any(result.outcome is Outcome.COMPARED_NOTHING for result in state.difftests):
        return Verdict.PASSED_WITH_UNCHECKED_FUNCTIONS
    return Verdict.PASSED


# ── decisions ──────────────────────────────────────────────────────────

def build_succeeded(state: VerifyState) -> bool:
    return isinstance(state.build, BuiltRom)


def no_difftest_failed(state: VerifyState) -> bool:
    """Functions difftest could not compare do not stop the run; the report lists them."""
    return not any(result.outcome is Outcome.FAILED for result in state.difftests)
