"""The verify graph's branches, run end to end with fake tools: which steps
run, and what the report says. Nothing is built or replayed."""
from pathlib import Path

import pytest

from wf.graphs import verify
from wf.graphs.verify import Verdict, VerifyReport, VerifyState, verify_application, write_report
from wf.routes import ALL_ROUTES
from wf.types import (BuildFailure, BuiltRom, CommitHash, DifftestResult, FunctionName, FrameCount, LockstepResult,
                      LogPath, Outcome, RomPath, Route, SourceVersion, ToolMessage)

ROUTES = ALL_ROUTES[:2]
SOURCES = SourceVersion(commit=CommitHash("0123456789abcdef"), has_uncommitted_changes=False)
GOOD_BUILD = BuiltRom(rom=RomPath(Path("port.gba")), seconds=1.0, log=LogPath(Path("build.log")))
FAILED_BUILD = BuildFailure(log=LogPath(Path("build.log")), last_lines=(ToolMessage("error: boom"),))


def test_a_failed_build_goes_straight_to_the_report(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, build=FAILED_BUILD, difftest_outcome=None, lockstep_runs=False)

    report = run_untracked(tmp_path)

    assert report.verdict is Verdict.BUILD_FAILED
    assert report.build_failure == FAILED_BUILD
    assert report.routes_checked == ()


def test_a_failed_difftest_skips_lockstep(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, build=GOOD_BUILD, difftest_outcome=Outcome.FAILED, lockstep_runs=False)

    report = run_untracked(tmp_path)

    assert report.verdict is Verdict.FAILED
    assert report.failed_functions == (FunctionName("someFunction"),)
    assert report.routes_checked == ()


def test_a_function_difftest_could_not_compare_still_reaches_lockstep(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, build=GOOD_BUILD, difftest_outcome=Outcome.COMPARED_NOTHING, lockstep_runs=True)

    report = run_untracked(tmp_path)

    assert report.verdict is Verdict.PASSED_WITH_UNCHECKED_FUNCTIONS
    assert report.unchecked_functions == (FunctionName("someFunction"),)
    assert report.routes_checked == tuple(route.name for route in ROUTES)


def test_everything_passing(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, build=GOOD_BUILD, difftest_outcome=Outcome.PASSED, lockstep_runs=True)

    report = run_untracked(tmp_path)

    assert report.verdict is Verdict.PASSED
    assert report.build_failure is None


# ── helpers ────────────────────────────────────────────────────────────

def run_untracked(run_dir: Path) -> VerifyReport:
    start = VerifyState(run_dir=run_dir, functions=(FunctionName("someFunction"),), routes=ROUTES, trials=10)
    final_state = verify_application(start, run_id="test", tracked=False).run_until_after(write_report)
    assert final_state.report is not None
    return final_state.report


def fake_tools(monkeypatch: pytest.MonkeyPatch, build: BuiltRom | BuildFailure,
               difftest_outcome: Outcome | None, lockstep_runs: bool) -> None:
    """Replace the tools the steps call. `None` / False: that tool must not be reached."""

    def current_sources() -> SourceVersion:
        return SOURCES

    def build_rom(log_dir: Path) -> BuiltRom | BuildFailure:
        return build

    def run_difftest(functions: tuple[FunctionName, ...], trials: int, log_dir: Path) -> tuple[DifftestResult, ...]:
        if difftest_outcome is None:
            raise AssertionError("difftest ran, but this branch should skip it")
        return tuple(DifftestResult(function=function, outcome=difftest_outcome, trials=trials,
                                    failures=0, out_of_domain=0) for function in functions)

    def run_lockstep_routes(rom: RomPath, routes: tuple[Route, ...], log_dir: Path) -> tuple[LockstepResult, ...]:
        if not lockstep_runs:
            raise AssertionError("lockstep ran, but this branch should skip it")
        return tuple(passing_lockstep(route, log_dir) for route in routes)

    monkeypatch.setattr(verify, "current_sources", current_sources)
    monkeypatch.setattr(verify, "build_rom", build_rom)
    monkeypatch.setattr(verify, "run_difftest", run_difftest)
    monkeypatch.setattr(verify, "run_lockstep_routes", run_lockstep_routes)


def passing_lockstep(route: Route, log_dir: Path) -> LockstepResult:
    return LockstepResult(route=route.name, outcome=Outcome.PASSED, checkpoints=10,
                          reached_frame=FrameCount(route.length), summary=ToolMessage("OK"),
                          seconds=0.1, log=LogPath(log_dir / "lockstep.log"))
