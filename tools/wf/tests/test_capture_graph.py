"""The capture graph's branches with fake tools: choosing a route by coverage."""
from pathlib import Path

import pytest

from wf.graphs import capture
from wf.graphs.capture import CaptureReport, CaptureState, run_capture
from wf.routes import ALL_ROUTES, route_named
from wf.types import CapturedCalls, FrameCount, FunctionName, NotRun, Route, RouteName, SnapshotDir

CALLED = FunctionName("calledFunction")
NEVER_CALLED = FunctionName("neverCalledFunction")


def test_the_route_executing_the_most_functions_is_chosen(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, executed={RouteName("wander"): {CALLED}, RouteName("story1"): {CALLED, NEVER_CALLED}})

    report = capture_with(ALL_ROUTES, tmp_path)

    assert isinstance(report.route, Route) and report.route.name == "story1"


def test_functions_without_calls_are_listed(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, executed={RouteName("wander"): {CALLED}})

    report = capture_with(ALL_ROUTES, tmp_path)

    assert report.never_called == (NEVER_CALLED,)


def test_no_route_running_them_ends_without_capturing(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, executed={}, capture_allowed=False)

    report = capture_with(ALL_ROUTES, tmp_path)

    assert isinstance(report.route, NotRun)
    assert report.never_called == (CALLED, NEVER_CALLED)


def test_a_single_route_given_is_used_without_coverage(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, executed=None)

    report = capture_with((route_named(RouteName("battle")),), tmp_path)

    assert isinstance(report.route, Route) and report.route.name == "battle"


def capture_with(routes: tuple[Route, ...], run_dir: Path) -> CaptureReport:
    start = CaptureState(run_dir=run_dir, functions=(CALLED, NEVER_CALLED), snapshots=SnapshotDir(run_dir / "s"),
                         candidate_routes=routes)
    return run_capture(start, run_id="test", tracked=False)


def fake_tools(monkeypatch: pytest.MonkeyPatch, executed: dict[RouteName, set[FunctionName]] | None,
               capture_allowed: bool = True) -> None:
    def routes_executing(functions: tuple[FunctionName, ...], routes: tuple[Route, ...],
                         log_dir: Path) -> dict[RouteName, frozenset[FunctionName]]:
        assert executed is not None, "coverage was consulted, but one route was given"
        return {route: frozenset(found) for route, found in executed.items()}

    def capture_calls(snapshots: SnapshotDir, route: Route, first_frame: FrameCount, last_frame: FrameCount,
                      calls_per_function: int, functions: tuple[FunctionName, ...],
                      log: Path) -> tuple[CapturedCalls, ...]:
        assert capture_allowed, "capture ran, but no route runs the functions"
        return tuple(CapturedCalls(function=function, calls=40 if function == CALLED else 0,
                                   directory=snapshots / function if function == CALLED else None)
                     for function in functions)

    monkeypatch.setattr(capture, "routes_executing", routes_executing)
    monkeypatch.setattr(capture, "capture_calls", capture_calls)
