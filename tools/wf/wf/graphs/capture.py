"""Capture the original game's own calls of some functions (the inputs that
difftest and the CPU oracle check conversions on): give each function a
recorded route that executes it (as few routes as possible), replay those
routes on the original in parallel, save the first calls of each function."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow
from wf.tools.capture import capture_calls, routes_executing
from wf.types import CapturedCalls, FrameCount, FunctionName, Result, Route, SnapshotDir

PROJECT = "recomp-capture"


def capture_graph() -> Graph["CaptureState"]:
    return workflow.graph(
        first=assign_routes,
        ends=[write_report],
        transitions=[
            Transition(after=assign_routes, go_to=capture_calls_on_routes, when=some_route_runs_them),
            Transition(after=assign_routes, go_to=write_report, when=no_route_runs_them),
            Transition(after=capture_calls_on_routes, go_to=write_report),
        ],
    )


def run_capture(start: "CaptureState", run_id: str, tracked: bool = True) -> "CaptureReport":
    outcome = capture_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"capture run {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class RouteAssignment(Result):
    """The functions whose calls are captured on one route."""
    route: Route
    functions: tuple[FunctionName, ...]


class CaptureReport(Result):
    snapshots: SnapshotDir
    routes: tuple[RouteAssignment, ...]
    captured: tuple[CapturedCalls, ...]
    never_called: tuple[FunctionName, ...]  # no route runs them, or no call in the window: not checkable


class CaptureState(BaseModel):
    run_dir: Path
    functions: tuple[FunctionName, ...]
    snapshots: SnapshotDir                       # where the calls are saved, one directory per function
    candidate_routes: tuple[Route, ...]          # routes to choose from
    calls_per_function: int = 40
    assignments: tuple[RouteAssignment, ...] | None = None
    captured: tuple[CapturedCalls, ...] = ()
    report: CaptureReport | None = None


workflow = Workflow[CaptureState](CaptureState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class AssignInput(Result):
    functions: tuple[FunctionName, ...]
    candidate_routes: tuple[Route, ...]
    run_dir: Path


class AssignOutput(Result):
    assignments: tuple[RouteAssignment, ...]


@workflow.step
def assign_routes(given: AssignInput) -> AssignOutput:
    """Give each function a route that runs it: repeatedly take the route that runs the most still unassigned."""
    executed = routes_executing(given.functions, given.candidate_routes, given.run_dir)
    by_name = {route.name: route for route in given.candidate_routes}
    left = set(given.functions)
    assignments: list[RouteAssignment] = []
    while left:
        best = max(executed, key=lambda name: len(executed[name] & left), default=None)
        if best is None or not executed[best] & left:
            break
        chosen = executed[best] & left
        assignments.append(RouteAssignment(route=by_name[best],
                                           functions=tuple(f for f in given.functions if f in chosen)))
        left -= chosen
    return AssignOutput(assignments=tuple(assignments))


class CaptureInput(Result):
    snapshots: SnapshotDir
    assignments: tuple[RouteAssignment, ...]
    calls_per_function: int
    run_dir: Path


class CaptureOutput(Result):
    captured: tuple[CapturedCalls, ...]


@workflow.step
def capture_calls_on_routes(given: CaptureInput) -> CaptureOutput:
    """Replay each assigned route on the original game (in parallel) and save the first calls of its functions."""
    def capture(assignment: RouteAssignment) -> tuple[CapturedCalls, ...]:
        log = given.run_dir / f"capture-{assignment.route.name}.log"
        return capture_calls(given.snapshots, assignment.route, FrameCount(0), assignment.route.length,
                             given.calls_per_function, assignment.functions, log)

    with ThreadPoolExecutor(max_workers=max(1, len(given.assignments))) as pool:
        per_route = list(pool.map(capture, given.assignments))
    return CaptureOutput(captured=tuple(calls for route_calls in per_route for calls in route_calls))


class ReportInput(Result):
    functions: tuple[FunctionName, ...]
    snapshots: SnapshotDir
    assignments: tuple[RouteAssignment, ...]
    captured: tuple[CapturedCalls, ...]


class ReportOutput(Result):
    report: CaptureReport


@workflow.step
def write_report(given: ReportInput) -> ReportOutput:
    """Say where the calls are and which functions were never called."""
    called = {entry.function for entry in given.captured if entry.calls > 0}
    return ReportOutput(report=CaptureReport(
        snapshots=given.snapshots, routes=given.assignments, captured=given.captured,
        never_called=tuple(function for function in given.functions if function not in called)))


# ── decisions ──────────────────────────────────────────────────────────

def some_route_runs_them(state: CaptureState) -> bool:
    return bool(state.assignments)


def no_route_runs_them(state: CaptureState) -> bool:
    return not state.assignments
