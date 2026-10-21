"""Every workflow, by name: what `wf status`, `wf answer` and `wf docs` go through."""
from collections.abc import Callable
from dataclasses import dataclass
from types import ModuleType

from wf.burr_typed import GraphView
from wf.graphs import (capture, convert, drafts, farm, facts, intervene, investigate, investigate_data, queue,
                       record, runtime_evidence, verify, vidsync_ready, vidsync_repair, vidsync_run,
                       vidsync_tick)


@dataclass(frozen=True)
class WorkflowEntry:
    name: str
    project: str           # the Burr UI project its runs are tracked under
    graph: Callable[[], GraphView]
    module: ModuleType     # its docstring says what the workflow is for


WORKFLOWS: tuple[WorkflowEntry, ...] = (
    WorkflowEntry("verify", verify.PROJECT, verify.verify_graph, verify),
    WorkflowEntry("capture", capture.PROJECT, capture.capture_graph, capture),
    WorkflowEntry("farm", farm.PROJECT, farm.farm_graph, farm),
    WorkflowEntry("farm-batch", farm.PROJECT, farm.farm_batch_graph, farm),
    WorkflowEntry("queue", queue.PROJECT, queue.queue_graph, queue),
    WorkflowEntry("drafts", drafts.PROJECT, drafts.drafts_graph, drafts),
    WorkflowEntry("investigate", investigate.PROJECT, investigate.investigate_graph, investigate),
    WorkflowEntry("investigate-data", investigate_data.PROJECT, investigate_data.investigate_data_graph,
                  investigate_data),
    WorkflowEntry("intervene", intervene.PROJECT, intervene.intervene_graph, intervene),
    WorkflowEntry("intervene-pilot", intervene.PROJECT_PILOT, intervene.intervene_pilot_graph, intervene),
    WorkflowEntry("runtime-evidence", runtime_evidence.PROJECT, runtime_evidence.runtime_evidence_graph,
                  runtime_evidence),
    WorkflowEntry("facts", facts.PROJECT, facts.facts_graph, facts),
    WorkflowEntry("convert", convert.PROJECT, convert.convert_graph, convert),
    WorkflowEntry("record", record.PROJECT, record.record_graph, record),
    WorkflowEntry("vidsync-run", vidsync_run.PROJECT, vidsync_run.vidsync_run_graph, vidsync_run),
    WorkflowEntry("vidsync-repair", vidsync_repair.PROJECT, vidsync_repair.vidsync_repair_graph, vidsync_repair),
    WorkflowEntry("vidsync-ready", vidsync_ready.PROJECT, vidsync_ready.vidsync_ready_graph, vidsync_ready),
    WorkflowEntry("vidsync-tick", vidsync_tick.PROJECT, vidsync_tick.vidsync_tick_graph, vidsync_tick),
)


def workflow_named(name: str) -> WorkflowEntry:
    for entry in WORKFLOWS:
        if entry.name == name:
            return entry
    raise ValueError(f"no workflow named {name!r}; there are {', '.join(entry.name for entry in WORKFLOWS)}")
