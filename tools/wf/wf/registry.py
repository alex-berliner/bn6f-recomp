"""Every workflow, by name: what `wf status`, `wf answer` and `wf docs` go through."""
from collections.abc import Callable
from dataclasses import dataclass
from types import ModuleType

from wf.burr_typed import GraphView
from wf.graphs import capture, convert, drafts, farm, record, verify


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
    WorkflowEntry("drafts", drafts.PROJECT, drafts.drafts_graph, drafts),
    WorkflowEntry("convert", convert.PROJECT, convert.convert_graph, convert),
    WorkflowEntry("record", record.PROJECT, record.record_graph, record),
)


def workflow_named(name: str) -> WorkflowEntry:
    for entry in WORKFLOWS:
        if entry.name == name:
            return entry
    raise ValueError(f"no workflow named {name!r}; there are {', '.join(entry.name for entry in WORKFLOWS)}")
