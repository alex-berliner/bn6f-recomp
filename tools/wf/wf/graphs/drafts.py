"""A farm model drafts the symbols/db entries of its accepted candidates that
have none yet (tools/convert/accepted/NAME.MODEL.yaml), in parallel: the
documentation small functions bring into the convert workflow's review and
record. New farm runs draft as they accept; this catches up older ones."""
import contextvars
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.tools.drafts import Draft, draft_entry, undrafted
from wf.types import FunctionName, ModelSpec, Result

PROJECT = "recomp-drafts"


def drafts_graph() -> Graph["DraftsState"]:
    return workflow.graph(
        first=find_undrafted_candidates,
        ends=[summarise_drafts],
        transitions=[
            Transition(after=find_undrafted_candidates, go_to=draft_each_candidate, when=some_need_drafts),
            Transition(after=find_undrafted_candidates, go_to=summarise_drafts, when=none_need_drafts),
            Transition(after=draft_each_candidate, go_to=summarise_drafts),
        ],
    )


def run_drafts(start: "DraftsState", run_id: str, tracked: bool = True) -> "DraftsReport":
    outcome = drafts_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"drafts run {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class DraftsReport(Result):
    written: int
    failed: tuple[Draft, ...]


class DraftsState(BaseModel):
    model: ModelSpec
    functions: tuple[FunctionName, ...] | None = None   # None: every accepted candidate of the model
    jobs: int = 16
    candidates: tuple[Path, ...] = ()
    drafts: tuple[Draft, ...] = ()
    report: DraftsReport | None = None


workflow = Workflow[DraftsState](DraftsState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class FindInput(Result):
    model: ModelSpec
    functions: tuple[FunctionName, ...] | None


class FindOutput(Result):
    candidates: tuple[Path, ...]


@workflow.step
def find_undrafted_candidates(given: FindInput) -> FindOutput:
    """The model's accepted candidates without a drafted entry."""
    return FindOutput(candidates=undrafted(given.model, given.functions))


class DraftInput(Result):
    model: ModelSpec
    candidates: tuple[Path, ...]
    jobs: int


class DraftOutput(Result):
    drafts: tuple[Draft, ...]


@workflow.step
def draft_each_candidate(given: DraftInput) -> DraftOutput:
    """Ask the model for each candidate's entry, several at a time."""
    tag = current_run_id()
    with ThreadPoolExecutor(max_workers=given.jobs) as pool:
        runs = [pool.submit(contextvars.copy_context().run, draft_entry, given.model, candidate, tag)
                for candidate in given.candidates]
        return DraftOutput(drafts=tuple(run.result() for run in runs))


class SummariseInput(Result):
    drafts: tuple[Draft, ...]


class SummariseOutput(Result):
    report: DraftsReport


@workflow.step
def summarise_drafts(given: SummariseInput) -> SummariseOutput:
    """How many entries were written, and which candidates got none (and why)."""
    return SummariseOutput(report=DraftsReport(written=sum(1 for draft in given.drafts if draft.path is not None),
                                               failed=tuple(draft for draft in given.drafts if draft.path is None)))


# ── decisions ──────────────────────────────────────────────────────────

def some_need_drafts(state: DraftsState) -> bool:
    return bool(state.candidates)


def none_need_drafts(state: DraftsState) -> bool:
    return not state.candidates
