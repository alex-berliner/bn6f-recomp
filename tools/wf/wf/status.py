"""Where every workflow run is, read from Burr's tracking logs: running,
waiting at a pause point (for what), finished, or failed (where and why)."""
import json
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, ValidationError

from wf.burr_typed import burr_dir, owner_alive
from wf.registry import WORKFLOWS, WorkflowEntry
from wf.types import Result


class Phase(Enum):
    RUNNING = "running"
    PAUSED = "waiting"
    FINISHED = "finished"
    FAILED = "failed"
    STOPPED = "stopped"  # between steps with nothing waiting: the process was interrupted


class RunStatus(Result):
    workflow: str
    run_id: str
    phase: Phase
    step: str | None
    detail: str
    last_activity: datetime


class _TrackedEntry(BaseModel):
    """The fields of a Burr tracking-log line that say where a run is."""
    type: Literal["begin_entry", "end_entry"]
    action: str
    exception: str | None = None


def all_runs() -> tuple[RunStatus, ...]:
    statuses: list[RunStatus] = []
    for project in sorted({entry.project for entry in WORKFLOWS}):
        project_dir = burr_dir() / project
        if not project_dir.is_dir():
            continue
        for run_dir in sorted(path for path in project_dir.iterdir() if (path / "log.jsonl").exists()):
            workflow = _workflow_of(run_dir, project)
            if workflow is not None:
                statuses.append(_status(workflow, run_dir))
    return tuple(sorted(statuses, key=lambda status: status.last_activity))


def workflow_of_run(run_id: str) -> WorkflowEntry:
    """The workflow a saved run belongs to."""
    for project in sorted({entry.project for entry in WORKFLOWS}):
        run_dir = burr_dir() / project / run_id
        workflow = _workflow_of(run_dir, project) if run_dir.is_dir() else None
        if workflow is not None:
            return workflow
    raise SystemExit(f"no run {run_id} in {burr_dir()}")


def _workflow_of(run_dir: Path, project: str) -> WorkflowEntry | None:
    """The registered workflow whose steps are exactly this run's steps."""
    graph_file = run_dir / "graph.json"
    if not graph_file.exists():
        return None
    actions = {str(action["name"]) for action in json.loads(graph_file.read_text())["actions"]}  # pyright: ignore[reportAny]
    for entry in WORKFLOWS:
        transitions = entry.graph().transitions
        steps = {step.__name__ for transition in transitions for step in (transition.after, transition.go_to)}
        if entry.project == project and steps == actions:
            return entry
    return None


def _status(workflow: WorkflowEntry, run_dir: Path) -> RunStatus:
    log = run_dir / "log.jsonl"
    entries = [_entry(line) for line in log.read_text().splitlines() if line.strip()]
    tracked = [entry for entry in entries if entry is not None]
    last_activity = datetime.fromtimestamp(log.stat().st_mtime)

    def status(phase: Phase, step: str | None, detail: str) -> RunStatus:
        return RunStatus(workflow=workflow.name, run_id=run_dir.name, phase=phase, step=step, detail=detail,
                         last_activity=last_activity)

    process_gone = owner_alive(workflow.project, run_dir.name) is False
    if not tracked:
        return status(Phase.STOPPED, None, "its process ended") if process_gone else status(Phase.RUNNING, None, "started")
    last = tracked[-1]
    if last.type == "begin_entry":
        if process_gone:
            return status(Phase.STOPPED, last.action, "its process ended during this step")
        return status(Phase.RUNNING, last.action, "")
    if last.exception:
        return status(Phase.FAILED, last.action, last.exception.strip().splitlines()[-1][:200])
    graph = workflow.graph()
    if last.action in {end.__name__ for end in graph.ends}:
        return status(Phase.FINISHED, last.action, "")
    paused = graph.waiting(run_dir.name)
    if paused is not None:
        return status(Phase.PAUSED, paused.step, f"needs a {paused.answer_type.__name__}")
    return status(Phase.STOPPED, last.action, "interrupted after this step")


def _entry(line: str) -> _TrackedEntry | None:
    try:
        return _TrackedEntry.model_validate_json(line)
    except ValidationError:
        return None  # other kinds of tracking lines (spans, attributes)
