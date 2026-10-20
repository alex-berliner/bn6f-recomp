"""Cheap models investigate placeholder-named DATA labels of the bn6f disassembly in volume and
write *candidate* symbols/db entries for review (docs/PROCESS.md §4's farm, pointed at data instead
of code): pick the labels worth reading first (referenced by many functions that already have
descriptive names, ROM tables and RAM variables alike), show each to one model with the code that
uses it, have a second call cross-check the proposal against the same code, and write the
candidates and a report under build/investigate_data/RUN_ID. Nothing is renamed, nothing is
installed: the orchestrator reviews the candidates and takes what holds up through `record`."""
from pathlib import Path

from pydantic import BaseModel
from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.tools import investigate_data
from wf.tools.investigate_data import (DataCandidate, DataContext, DataProposal, DataTarget, Report,
                                      ToolMessage, Verdict, check_all, elf_symbols, gather_all,
                                      guard_problem, investigate_all, name_conventions, select_targets,
                                      symdb_check)
from wf.types import ModelSpec, Result, SymbolName

PROJECT = "recomp-investigate-data"

from wf.tools.investigate_data import (DEFAULT_CHECKER, DEFAULT_COUNT, DEFAULT_JOBS,  # noqa: E402
                                       DEFAULT_MODEL)


def investigate_data_graph() -> Graph["InvestigateDataState"]:
    return workflow.graph(
        first=select_the_labels,
        ends=[write_the_candidates],
        transitions=[
            Transition(after=select_the_labels, go_to=gather_the_contexts, when=some_selected),
            Transition(after=select_the_labels, go_to=write_the_candidates, when=none_selected),
            Transition(after=gather_the_contexts, go_to=investigate_each),
            Transition(after=investigate_each, go_to=cross_check_each),
            Transition(after=cross_check_each, go_to=write_the_candidates),
        ],
    )


def run_investigate_data(start: "InvestigateDataState", run_id: str,
                         tracked: bool = True) -> "Report":
    outcome = investigate_data_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"investigate-data run {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class InvestigateDataState(BaseModel):
    run_dir: Path                                        # build/investigate_data/RUN_ID
    count: int = DEFAULT_COUNT
    only: tuple[SymbolName, ...] = ()
    model: ModelSpec = DEFAULT_MODEL
    checker: ModelSpec = DEFAULT_CHECKER
    jobs: int = DEFAULT_JOBS
    targets: tuple[DataTarget, ...] = ()
    contexts: tuple[DataContext, ...] = ()
    proposals: tuple[DataProposal, ...] = ()
    verdicts: tuple[Verdict, ...] = ()
    candidates: tuple[DataCandidate, ...] = ()
    notes: tuple[ToolMessage, ...] = ()
    report: Report | None = None


workflow = Workflow[InvestigateDataState](InvestigateDataState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class SelectInput(Result):
    count: int
    only: tuple[SymbolName, ...]


class SelectOutput(Result):
    targets: tuple[DataTarget, ...]
    notes: tuple[ToolMessage, ...]


@workflow.step
def select_the_labels(given: SelectInput) -> SelectOutput:
    """The placeholder data labels worth investigating first, and which --only names were not
    eligible."""
    targets, notes = select_targets(given.count, given.only)
    return SelectOutput(targets=targets, notes=notes)


class GatherInput(Result):
    targets: tuple[DataTarget, ...]
    run_dir: Path


class GatherOutput(Result):
    contexts: tuple[DataContext, ...]


@workflow.step
def gather_the_contexts(given: GatherInput) -> GatherOutput:
    """Each label's brief: its address, region and size, its bytes when it is ROM data, and up to
    eight of the sites bn6f references it at — the owning function, its symbols/db summary and a
    plain reading of the use — with the code around each site (the snippets stay under the run's
    directory)."""
    return GatherOutput(contexts=gather_all(given.targets, given.run_dir))


class InvestigateInput(Result):
    contexts: tuple[DataContext, ...]
    model: ModelSpec
    jobs: int
    run_dir: Path


class InvestigateOutput(Result):
    proposals: tuple[DataProposal, ...]


@workflow.step
def investigate_each(given: InvestigateInput) -> InvestigateOutput:
    """Ask the investigating model about each label, several at a time."""
    run_id = current_run_id()
    return InvestigateOutput(proposals=investigate_all(
        given.model, given.contexts, given.run_dir, run_id, given.jobs, name_conventions()))


class CheckInput(Result):
    contexts: tuple[DataContext, ...]
    proposals: tuple[DataProposal, ...]
    checker: ModelSpec
    jobs: int
    run_dir: Path


class CheckOutput(Result):
    verdicts: tuple[Verdict, ...]


@workflow.step
def cross_check_each(given: CheckInput) -> CheckOutput:
    """Have a second call check each proposal against the same code (a proposal without a name is
    not asked: it stays a hypothesis with its open question)."""
    run_id = current_run_id()
    return CheckOutput(verdicts=check_all(given.checker, given.contexts, given.proposals,
                                          given.run_dir, run_id, given.jobs, name_conventions()))


class WriteInput(Result):
    contexts: tuple[DataContext, ...]
    proposals: tuple[DataProposal, ...]
    verdicts: tuple[Verdict, ...]
    run_dir: Path
    model: ModelSpec
    checker: ModelSpec
    notes: tuple[ToolMessage, ...]


class WriteOutput(Result):
    report: Report
    candidates: tuple[DataCandidate, ...]


@workflow.step
def write_the_candidates(given: WriteInput) -> WriteOutput:
    """A candidate symbols/db entry per label whose name is free (validated with symdb's checks)
    and report.tsv."""
    by_name = {context.target.name: context for context in given.contexts}
    verdict_of = {verdict.name: verdict for verdict in given.verdicts}
    syms = elf_symbols(frozenset(proposal.name for proposal in given.proposals))
    taken = investigate_data.taken_names()
    written = agreed = revised = disagreed = nulls = rejected = invalid = retries = stragglers = 0
    problems: list[ToolMessage] = list(given.notes)
    guarded: dict[SymbolName, ToolMessage] = {}
    pairs: list[tuple[DataProposal, Verdict]] = []
    kept: list[DataCandidate] = []
    for proposal in given.proposals:
        context = by_name[proposal.name]
        verdict = verdict_of[proposal.name]
        if proposal.finished:
            continue
        pairs.append((proposal, verdict))
        retries += proposal.retries + verdict.retries
        if investigate_data.abandoned(proposal.problem):
            stragglers += 1
        if investigate_data.abandoned(verdict.problem):
            stragglers += 1
        if investigate_data.out_of_credits(proposal.problem) \
                or investigate_data.out_of_credits(verdict.problem):
            problems.append(ToolMessage(f"{proposal.name}: {verdict.problem or proposal.problem}"))
            continue
        answer = proposal.answer
        proposed = (verdict.reply.revised_name if verdict.reply is not None
                    and verdict.reply.verdict == "revise" and verdict.reply.revised_name else None) \
            or (answer.proposed_name if answer else None)
        candidate = investigate_data.make_candidate(context, proposal, verdict, given.model,
                                                    given.checker, current_run_id())
        if proposed is None:
            nulls += 1
            write_candidate(candidate, given.run_dir)   # the label stays documented even unnamed
            continue
        refused = guard_problem(str(proposed), taken)
        if refused is not None:
            rejected += 1
            guarded[proposal.name] = refused
            problems.append(ToolMessage(f"{proposal.name}: the proposed name {proposed} was "
                                        f"rejected: {refused}"))
            continue
        check = verdict.reply.verdict if verdict.reply else "none"
        if check == "agree":
            agreed += 1
        elif check == "revise":
            revised += 1
        elif check == "disagree":
            disagreed += 1
        investigate_data.write_candidate(candidate, given.run_dir)
        found = symdb_check(given.run_dir / f"{candidate.name}.yaml", syms)
        if found:
            invalid += 1
            problems += [ToolMessage(f"{candidate.name}: {problem}") for problem in found]
            continue
        kept.append(candidate)
        written += 1
    report_path = given.run_dir / "report.tsv"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(investigate_data.report_rows(tuple(pairs), guarded))
    stopped = next((problem for problem in problems if investigate_data.out_of_credits(problem)), None)
    if stopped is not None:
        stopped = ToolMessage(f"the checker or the model ran out of credits (HTTP 402); the run kept "
                              f"what was done and stopped cleanly ({stopped})")
    return WriteOutput(
        report=Report(selected=len(given.proposals), written=written, agreed=agreed, revised=revised,
                      disagreed=disagreed, nulls=nulls, rejected=rejected, invalid=invalid,
                      retries=retries, stragglers=stragglers,
                      tokens=sum(proposal.tokens for proposal in given.proposals)
                      + sum(verdict.tokens for verdict in given.verdicts),
                      problems=tuple(problems), stopped=stopped),
        candidates=tuple(kept))


def write_candidate(candidate: DataCandidate, run_dir: Path) -> Path:
    """The candidate as NAME.yaml beside the run's report."""
    return investigate_data.write_candidate(candidate, run_dir)


# ── decisions ──────────────────────────────────────────────────────────

def some_selected(state: InvestigateDataState) -> bool:
    return bool(state.targets)


def none_selected(state: InvestigateDataState) -> bool:
    return not state.targets