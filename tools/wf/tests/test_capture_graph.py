"""The capture graph with fake tools: functions spread over as few routes as
possible, each route captured once, functions no route runs reported."""
from pathlib import Path

import pytest

from wf.graphs import capture
from wf.graphs.capture import CaptureReport, CaptureState, run_capture
from wf.routes import ALL_ROUTES
from wf.types import CapturedCalls, FrameCount, FunctionName, Route, RouteName, SnapshotDir

A, B, C, NOWHERE = (FunctionName(name) for name in ("funcA", "funcB", "funcC", "funcNowhere"))


def test_the_fewest_routes_cover_the_functions(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    captured_on = fake_tools(monkeypatch, {RouteName("wander"): {A, B}, RouteName("story1"): {B, C},
                                           RouteName("battle"): {C}})

    report = capture_with((A, B, C), tmp_path)

    assert [(assignment.route.name, assignment.functions) for assignment in report.routes] == [
        ("wander", (A, B)), ("story1", (C,))]
    assert sorted(captured_on) == ["story1", "wander"]
    assert report.never_called == ()


def test_functions_no_route_runs_are_reported(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    _ = fake_tools(monkeypatch, {RouteName("wander"): {A}})

    report = capture_with((A, NOWHERE), tmp_path)

    assert report.never_called == (NOWHERE,)


def test_nothing_is_replayed_when_no_route_runs_them(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    captured_on = fake_tools(monkeypatch, {})

    report = capture_with((A, B), tmp_path)

    assert report.routes == () and report.never_called == (A, B) and captured_on == []


def capture_with(functions: tuple[FunctionName, ...], run_dir: Path) -> CaptureReport:
    start = CaptureState(run_dir=run_dir, functions=functions, snapshots=SnapshotDir(run_dir / "s"),
                         candidate_routes=ALL_ROUTES)
    return run_capture(start, run_id="test", tracked=False)


def fake_tools(monkeypatch: pytest.MonkeyPatch, executed: dict[RouteName, set[FunctionName]]) -> list[str]:
    captured_on: list[str] = []

    def routes_executing(functions: tuple[FunctionName, ...], routes: tuple[Route, ...],
                         log_dir: Path) -> dict[RouteName, frozenset[FunctionName]]:
        return {route: frozenset(found & set(functions)) for route, found in executed.items()}

    def capture_calls(snapshots: SnapshotDir, route: Route, first_frame: FrameCount, last_frame: FrameCount,
                      calls_per_function: int, functions: tuple[FunctionName, ...],
                      log: Path) -> tuple[CapturedCalls, ...]:
        captured_on.append(route.name)
        return tuple(CapturedCalls(function=function, calls=40, directory=snapshots / function)
                     for function in functions)

    monkeypatch.setattr(capture, "routes_executing", routes_executing)
    monkeypatch.setattr(capture, "capture_calls", capture_calls)
    return captured_on
