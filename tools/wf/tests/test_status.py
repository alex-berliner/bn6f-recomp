"""`wf status` tells a run that is working from one whose process died: both
end their tracking log with a step that began and never ended."""
import json
from pathlib import Path

import pytest

from wf.burr_typed import RunOwner, burr_dir
from wf.graphs import capture
from wf.graphs.capture import PROJECT, CaptureState, run_capture
from wf.routes import ALL_ROUTES
from wf.status import Phase, all_runs
from wf.types import FunctionName, Route, RouteName, SnapshotDir


def test_a_run_whose_process_died_mid_step_is_stopped(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    def no_route_runs_them(functions: tuple[FunctionName, ...], routes: tuple[Route, ...],
                           log_dir: Path) -> dict[RouteName, frozenset[FunctionName]]:
        return {}

    monkeypatch.setattr(capture, "routes_executing", no_route_runs_them)
    _ = run_capture(CaptureState(run_dir=tmp_path, functions=(FunctionName("f"),), snapshots=SnapshotDir(tmp_path),
                                 candidate_routes=ALL_ROUTES), run_id="capture-died")
    run_dir = burr_dir() / PROJECT / "capture-died"
    with (run_dir / "log.jsonl").open("a") as log:  # as if killed in the middle of a step
        _ = log.write(json.dumps({"type": "begin_entry", "action": "capture_calls_on_routes"}) + "\n")

    assert phase_of("capture-died") is Phase.RUNNING   # this process still runs it
    _ = (run_dir / "owner.json").write_text(RunOwner(pid=2**22 + 7, process_started=1).model_dump_json())
    assert phase_of("capture-died") is Phase.STOPPED


def phase_of(run_id: str) -> Phase:
    return next(status.phase for status in all_runs() if status.run_id == run_id)
