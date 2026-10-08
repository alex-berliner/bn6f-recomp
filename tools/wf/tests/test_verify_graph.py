"""The verify graph's branches, run end to end with fake tools: which checks
run, and what the report says. Nothing is built or replayed."""
from dataclasses import dataclass, field
from pathlib import Path

import pytest

from wf.graphs import verify
from wf.graphs.verify import Verdict, VerifyReport, VerifyState, run_verify
from wf.routes import ALL_ROUTES
from wf.types import (BattleSweepResult, BuildFailure, BuiltRom, CapturedDifftestResult, CommitHash, DifftestResult,
                      FrameCount, FunctionName, GuestAddress, LockstepResult, LogPath, NotRun, OracleResult, Outcome,
                      RomPath, Route, SnapshotDir, SourceVersion, ToolMessage)

FUNCTION = FunctionName("someFunction")
ROUTES = ALL_ROUTES[:2]
GOOD_BUILD = BuiltRom(rom=RomPath(Path("port.gba")), difftest=Path("difftest"), seconds=1.0,
                      log=LogPath(Path("build.log")))
FAILED_BUILD = BuildFailure(log=LogPath(Path("build.log")), last_lines=(ToolMessage("error: boom"),))


@dataclass
class FakeTools:
    """What each fake tool returns; None means the branch must not reach that tool."""
    build: BuiltRom | BuildFailure = GOOD_BUILD
    random: Outcome | None = Outcome.PASSED
    captured: Outcome | None = Outcome.PASSED
    oracle_differs: bool | None = False
    lockstep_passes: bool | None = True
    sweep_passes: bool | None = None
    called: list[str] = field(default_factory=list[str])


def test_a_failed_build_goes_straight_to_the_report(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    tools = FakeTools(build=FAILED_BUILD, random=None, captured=None, oracle_differs=None, lockstep_passes=None)
    report = verify_with(tools, monkeypatch, tmp_path)

    assert report.verdict is Verdict.BUILD_FAILED
    assert report.build_failure == FAILED_BUILD
    assert tools.called == ["build"]


def test_a_difftest_failure_skips_the_oracle_and_lockstep(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    tools = FakeTools(captured=Outcome.FAILED, oracle_differs=None, lockstep_passes=None)
    report = verify_with(tools, monkeypatch, tmp_path)

    assert report.verdict is Verdict.FAILED
    assert report.failed_functions == (FUNCTION,)
    assert tools.called == ["build", "random", "captured"]


def test_oracle_differences_need_triage_before_lockstep(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    tools = FakeTools(oracle_differs=True, lockstep_passes=None)
    report = verify_with(tools, monkeypatch, tmp_path)

    assert report.verdict is Verdict.NEEDS_TRIAGE
    assert report.oracle_differences == (FUNCTION,)
    assert "lockstep" not in tools.called


def test_everything_passing_without_battle_code(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    tools = FakeTools()
    report = verify_with(tools, monkeypatch, tmp_path)

    assert report.verdict is Verdict.PASSED
    assert tools.called == ["build", "random", "captured", "oracle", "lockstep"]
    assert isinstance(report.battle_sweep, NotRun)


def test_battle_code_runs_the_battle_sweep(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    tools = FakeTools(sweep_passes=False)
    report = verify_with(tools, monkeypatch, tmp_path, battle_code=True)

    assert report.verdict is Verdict.FAILED
    assert tools.called[-1] == "sweep"


def test_without_captured_calls_only_random_difftest_can_compare(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    tools = FakeTools(random=Outcome.COMPARED_NOTHING, captured=None, oracle_differs=None)
    report = verify_with(tools, monkeypatch, tmp_path, snapshots=None)

    assert report.verdict is Verdict.PASSED_WITH_UNCHECKED_FUNCTIONS
    assert report.never_compared == (FUNCTION,)
    assert tools.called == ["build", "random", "lockstep"]


# ── helpers ────────────────────────────────────────────────────────────

def verify_with(tools: FakeTools, monkeypatch: pytest.MonkeyPatch, run_dir: Path, battle_code: bool = False,
                snapshots: SnapshotDir | None = SnapshotDir(Path("snapshots"))) -> VerifyReport:
    install(tools, monkeypatch)
    start = VerifyState(run_dir=run_dir, functions=(FUNCTION,), snapshots=snapshots, routes=ROUTES,
                        battle_code=battle_code)
    return run_verify(start, run_id="test", tracked=False)


def install(tools: FakeTools, monkeypatch: pytest.MonkeyPatch) -> None:
    def reached(name: str, expected: object) -> None:
        if expected is None:
            raise AssertionError(f"{name} ran, but this branch should skip it")
        tools.called.append(name)

    def current_sources() -> SourceVersion:
        return SourceVersion(commit=CommitHash("0123456789abcdef"), has_uncommitted_changes=False)

    def lift_and_build(log_dir: Path) -> BuiltRom | BuildFailure:
        tools.called.append("build")
        return tools.build

    def run_difftest(difftest: Path, functions: tuple[FunctionName, ...], trials: int,
                     log_dir: Path) -> tuple[DifftestResult, ...]:
        reached("random", tools.random)
        assert tools.random is not None
        return tuple(DifftestResult(function=function, outcome=tools.random, trials=trials, failures=0,
                                    out_of_domain=0) for function in functions)

    def run_captured_difftest(difftest: Path, functions: tuple[FunctionName, ...], snapshots: SnapshotDir,
                              log_dir: Path) -> tuple[CapturedDifftestResult, ...]:
        reached("captured", tools.captured)
        assert tools.captured is not None
        return tuple(CapturedDifftestResult(function=function, outcome=tools.captured, snapshots=40, runs=40,
                                            failures=0, out_of_domain=0) for function in functions)

    def run_oracle(difftest: Path, functions: tuple[FunctionName, ...], snapshots: SnapshotDir, states: int,
                   log_dir: Path) -> tuple[OracleResult, ...]:
        reached("oracle", tools.oracle_differs)
        differed = 1 if tools.oracle_differs else 0
        return tuple(OracleResult(function=function, address=GuestAddress(0x08000000), compared=24,
                                  differed=differed, out_of_domain=0, differences=()) for function in functions)

    def run_lockstep_routes(rom: RomPath, routes: tuple[Route, ...], log_dir: Path) -> tuple[LockstepResult, ...]:
        reached("lockstep", tools.lockstep_passes)
        outcome = Outcome.PASSED if tools.lockstep_passes else Outcome.FAILED
        return tuple(LockstepResult(route=route.name, outcome=outcome, checkpoints=10,
                                    reached_frame=FrameCount(route.length), summary=ToolMessage("OK"),
                                    seconds=0.1, log=LogPath(log_dir / "lockstep.log")) for route in routes)

    def run_battle_sweep(rom: RomPath, jobs: int, log_dir: Path) -> BattleSweepResult:
        reached("sweep", tools.sweep_passes)
        failures = () if tools.sweep_passes else (ToolMessage("0x80aee70 FAIL: save memory differs"),)
        return BattleSweepResult(battles=116, identical=116 - len(failures), failures=failures,
                                 log=LogPath(log_dir / "sweep.log"), seconds=1.0)

    fakes: dict[str, object] = {
        "current_sources": current_sources, "lift_and_build": lift_and_build, "run_difftest": run_difftest,
        "run_captured_difftest": run_captured_difftest, "run_oracle": run_oracle,
        "run_lockstep_routes": run_lockstep_routes, "run_battle_sweep": run_battle_sweep,
    }
    for name, fake in fakes.items():
        monkeypatch.setattr(verify, name, fake)
