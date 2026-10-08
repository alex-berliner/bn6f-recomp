"""Capture the original game's own calls of some functions (the inputs that
difftest and the CPU oracle check conversions on): pick a recorded route
that executes them, replay it on the original, save the first calls of each.
(docs/PROCESS.md §4: "capture 40 calls each on a route that runs them".)"""
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow
from wf.tools.capture import capture_calls, routes_executing
from wf.types import CapturedCalls, FrameCount, FunctionName, NotRun, Result, Route, RouteName, SnapshotDir, ToolMessage

PROJECT = "recomp-capture"


def capture_graph() -> Graph["CaptureState"]:
    return workflow.graph(
        first=choose_route,
        ends=[write_report],
        transitions=[
            Transition(after=choose_route, go_to=capture_calls_on_route, when=a_route_runs_them),
            Transition(after=choose_route, go_to=write_report, when=no_route_runs_them),
            Transition(after=capture_calls_on_route, go_to=write_report),
        ],
    )


def run_capture(start: "CaptureState", run_id: str, tracked: bool = True) -> "CaptureReport":
    outcome = capture_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"capture run {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class RouteCoverage(Result):
    """Which of the wanted functions a route's recorded profile shows executing."""
    route: RouteName
    functions: tuple[FunctionName, ...]


class CaptureReport(Result):
    snapshots: SnapshotDir
    route: Route | NotRun
    captured: tuple[CapturedCalls, ...]
    never_called: tuple[FunctionName, ...]  # no call in the window: difftest and the oracle cannot check them


class CaptureState(BaseModel):
    run_dir: Path
    functions: tuple[FunctionName, ...]
    snapshots: SnapshotDir                       # where the calls are saved, one directory per function
    candidate_routes: tuple[Route, ...]          # routes to choose from (a single one to force it)
    calls_per_function: int = 40
    route: Route | NotRun | None = None
    coverage: tuple[RouteCoverage, ...] = ()
    captured: tuple[CapturedCalls, ...] = ()
    report: CaptureReport | None = None


workflow = Workflow[CaptureState](CaptureState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class ChooseRouteInput(Result):
    functions: tuple[FunctionName, ...]
    candidate_routes: tuple[Route, ...]
    run_dir: Path


class ChooseRouteOutput(Result):
    route: Route | NotRun
    coverage: tuple[RouteCoverage, ...]


@workflow.step
def choose_route(given: ChooseRouteInput) -> ChooseRouteOutput:
    """Pick the route whose coverage profile executes the most of the functions."""
    if len(given.candidate_routes) == 1:
        return ChooseRouteOutput(route=given.candidate_routes[0], coverage=())
    executed = routes_executing(given.functions, given.candidate_routes, given.run_dir)
    coverage = tuple(RouteCoverage(route=route, functions=tuple(sorted(functions)))
                     for route, functions in executed.items())
    best = max(coverage, key=lambda entry: len(entry.functions), default=None)
    if best is None or not best.functions:
        return ChooseRouteOutput(route=NotRun(reason=ToolMessage("no recorded route executes these functions")),
                                 coverage=coverage)
    chosen = next(route for route in given.candidate_routes if route.name == best.route)
    return ChooseRouteOutput(route=chosen, coverage=coverage)


class CaptureInput(Result):
    functions: tuple[FunctionName, ...]
    snapshots: SnapshotDir
    route: Route
    calls_per_function: int
    run_dir: Path


class CaptureOutput(Result):
    captured: tuple[CapturedCalls, ...]


@workflow.step
def capture_calls_on_route(given: CaptureInput) -> CaptureOutput:
    """Replay the route on the original game and save the first calls of each function."""
    return CaptureOutput(captured=capture_calls(
        given.snapshots, given.route, FrameCount(0), given.route.length, given.calls_per_function,
        given.functions, given.run_dir / "capture.log"))


class ReportInput(Result):
    functions: tuple[FunctionName, ...]
    snapshots: SnapshotDir
    route: Route | NotRun
    captured: tuple[CapturedCalls, ...]


class ReportOutput(Result):
    report: CaptureReport


@workflow.step
def write_report(given: ReportInput) -> ReportOutput:
    """Say where the calls are and which functions were never called."""
    called = {entry.function for entry in given.captured if entry.calls > 0}
    return ReportOutput(report=CaptureReport(
        snapshots=given.snapshots, route=given.route, captured=given.captured,
        never_called=tuple(function for function in given.functions if function not in called)))


# ── decisions ──────────────────────────────────────────────────────────

def a_route_runs_them(state: CaptureState) -> bool:
    return isinstance(state.route, Route)


def no_route_runs_them(state: CaptureState) -> bool:
    return isinstance(state.route, NotRun)
