"""`wf facts`: the runtime-facts layer -- a record of what the ORIGINAL game actually does at
runtime, for every lifted function, built once from the recordings and with no model judgment
in it. Step 1 of the runtime checking: the later steps (a claim checker, an mGBA intervention
runner) read what this writes and decide what to do with it.

    python -m wf facts [--only NAME]... [--states K=8] [--jobs J=6] [--limit N]

Three steps, in order:

  1. `read_the_recordings` folds every `tools/traces/coverage/*.prof` and `*.edges` into
     per-function coverage (executed yes/no, cycles, the indirect call sites that reached it
     and their callers, calls per 1000 frames with the route lengths read from `wf/routes.py`
     at run time), picks the functions in this run, and writes every record that needs no
     mGBA: the ones whose captured calls have not changed since the last run, and the ones
     with no captured calls at all.
  2. `gather_the_facts` replays K captured entry states per function through mGBA
     (`tools/runtime_evidence.py`'s replay, summarised with its `summarise`) with --jobs
     functions at a time, and folds what the original did into each record. A function whose
     replay crashes or times out is recorded with its `facts_error`; it never stops the run.
  3. `write_the_index` writes `build/facts/coverage.tsv` and says what the run produced.

Output (regenerable, never committed):

    build/facts/by_address/0x08XXXXXX.json    one record per function, keyed by entry address
    build/facts/coverage.tsv                  one row per function
    build/facts/progress.txt                 what the run did, as it did it

Nothing under bn6f or symbols/db is touched and no model is called: a function's name is a
field of its record, never its key.
"""
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.tools import facts as ft
from wf.tools.facts import DEFAULT_JOBS, DEFAULT_LIMIT, DEFAULT_STATES, FactsReport, Target
from wf.types import FunctionName, Result, RouteName, ToolMessage

PROJECT = "recomp-facts"


def facts_graph() -> Graph["FactsState"]:
    return workflow.graph(
        first=read_the_recordings,
        ends=[write_the_index],
        transitions=[
            Transition(after=read_the_recordings, go_to=gather_the_facts),
            Transition(after=gather_the_facts, go_to=write_the_index),
        ],
    )


def run_facts(start: "FactsState", run_id: str, tracked: bool = True) -> FactsReport:
    outcome = facts_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"facts run {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class FactsState(BaseModel):
    facts_dir: Path = ft.FACTS                  # build/facts
    only: tuple[FunctionName, ...] = ()
    states: int = DEFAULT_STATES
    jobs: int = DEFAULT_JOBS
    limit: int = DEFAULT_LIMIT
    targets: tuple[Target, ...] = ()            # every function in the run
    replay: tuple[Target, ...] = ()             # the ones whose facts still have to be gathered
    routes: tuple[RouteName, ...] = ()          # the recorded routes that were read
    planned: int = 0                            # functions read_the_recordings wrote or kept
    facts: int = 0                              # functions whose replay produced facts
    errors: int = 0
    skipped: int = 0                            # functions with no captured calls to replay
    seconds: float = 0.0
    notes: tuple[ToolMessage, ...] = ()
    report: FactsReport | None = None


workflow = Workflow[FactsState](FactsState, project=PROJECT)


# ── step one: the recordings ───────────────────────────────────────────

class RecordingsInput(Result):
    facts_dir: Path
    only: tuple[FunctionName, ...]
    states: int
    limit: int


class RecordingsOutput(Result):
    targets: tuple[Target, ...]
    replay: tuple[Target, ...]
    routes: tuple[RouteName, ...]
    notes: tuple[ToolMessage, ...]


@workflow.step
def read_the_recordings(given: RecordingsInput) -> RecordingsOutput:
    """Every recorded route's `.prof` and `.edges`, folded into per-function coverage, and
    every record that needs no mGBA written: the functions whose captured entry states and
    replay inputs are unchanged since the last run (their facts are kept; the coverage is
    re-read when a recording moved under it), and the functions with no captured calls."""
    _claim(given.facts_dir)
    planned = ft.step_plan(given.facts_dir, given.only, given.states, given.limit)
    return RecordingsOutput(targets=planned.targets, replay=planned.replay, routes=planned.routes,
                            notes=planned.notes)


# ── step two: the captured calls, replayed ─────────────────────────────

class GatherInput(Result):
    facts_dir: Path
    replay: tuple[Target, ...]
    states: int
    jobs: int
    notes: tuple[ToolMessage, ...] = ()


class GatherOutput(Result):
    facts: int
    errors: int
    skipped: int
    seconds: float
    notes: tuple[ToolMessage, ...]


@workflow.step
def gather_the_facts(given: GatherInput) -> GatherOutput:
    """Replay K of each function's captured entry states in the original game (mGBA, the same
    replay `wf runtime-evidence` does) with --jobs functions at once, and fold what the
    original did -- registers in and out, the RAM it wrote by bn6f symbol, the I/O registers
    it wrote, the arguments it was given and the r0 it returned -- into its record.

    Nothing here stops the run: a function whose replay crashes, times out or never returns
    is written with its `facts_error` and the run carries on."""
    gathered = ft.step_gather(given.facts_dir, ft.Planned(targets=(), replay=given.replay,
                                                           routes=(), notes=()),
                              given.states, given.jobs)
    return GatherOutput(facts=gathered.facts, errors=gathered.errors, skipped=gathered.skipped,
                        seconds=gathered.seconds, notes=tuple(given.notes) + gathered.notes)


# ── step three: the index and the report ───────────────────────────────

class WriteInput(Result):
    facts_dir: Path
    targets: tuple[Target, ...]
    routes: tuple[RouteName, ...]
    facts: int
    errors: int
    seconds: float
    notes: tuple[ToolMessage, ...]


class WriteOutput(Result):
    report: FactsReport


@workflow.step
def write_the_index(given: WriteInput) -> WriteOutput:
    """`build/facts/coverage.tsv` (one row per function: address, name, reached, the routes
    that reached it, calls per 1000 frames, captured states, whether it has facts, the
    error) and the run's own report."""
    gathered = ft.Gathered(facts=given.facts, errors=given.errors, skipped=0, seconds=given.seconds,
                           notes=())
    planned = ft.Planned(targets=given.targets, replay=(), routes=given.routes, notes=())
    return WriteOutput(report=ft.step_write(given.facts_dir, planned, gathered))


# ── helpers ────────────────────────────────────────────────────────────

def _claim(facts_dir: Path) -> None:
    """Say in progress.txt that this run started, before anything long begins."""
    facts_dir.mkdir(parents=True, exist_ok=True)
    ft.note_progress(facts_dir, f"run {current_run_id_or('facts')} started")


def current_run_id_or(fallback: str) -> str:
    """The run's id, or FALLBACK when this is called outside a tracked run (a tool run by
    hand, a test)."""
    try:
        return current_run_id()
    except RuntimeError:
        return fallback


def summarise_report(report: FactsReport, seconds: float) -> str:
    """The run's numbers as one block of text (what `wf facts` prints when it finishes)."""
    lines = [f"functions: {report.functions}",
             f"reached by a recorded route: {report.reached}",
             f"with captured calls: {report.captured}",
             f"with runtime facts: {report.with_facts}",
             f"facts_error: {report.facts_errors}",
             f"coverage.tsv rows: {report.rows}",
             f"routes read: {', '.join(str(route) for route in report.routes) or '(none)'}",
             f"wall time: {seconds:.0f}s"]
    if report.error_reasons:
        lines.append("most common error reasons:")
        lines += [f"  {reason}" for reason in report.error_reasons]
    if report.examples:
        lines.append("examples:")
        lines += [f"  {path}" for path in report.examples]
    return "\n".join(lines)