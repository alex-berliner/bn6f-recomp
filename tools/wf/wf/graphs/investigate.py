"""Cheap models investigate placeholder-named functions of the bn6f disassembly in volume and write
*candidate* symbols/db entries for review (docs/PROCESS.md §4's farm, pointed at documentation
instead of code): pick the functions worth reading first (captured calls, named neighbours, small),
show each to one Hyper model with its context, have a different one cross-check the proposal, and
write the candidates and a report under build/investigate/RUN_ID. Nothing is renamed, nothing is
installed: the orchestrator reviews the candidates and takes what holds up through `record`."""
from pathlib import Path

from pydantic import BaseModel
from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.tools import investigate
from wf.tools.investigate import (Context, Proposal, Report, Target, ToolMessage, Verdict,
                                  investigate_all, check_all, gather_all, name_prefixes, not_selectable,
                                  select_targets)
from wf.types import FunctionName, ModelSpec, Result

PROJECT = "recomp-investigate"

from wf.tools.investigate import (DEFAULT_CHECKER, DEFAULT_COUNT, DEFAULT_JOBS, DEFAULT_MODEL)  # noqa: E402


def investigate_graph() -> Graph["InvestigateState"]:
    return workflow.graph(
        first=select_the_targets,
        ends=[write_the_candidates],
        transitions=[
            Transition(after=select_the_targets, go_to=gather_the_contexts, when=some_selected),
            Transition(after=select_the_targets, go_to=write_the_candidates, when=none_selected),
            Transition(after=gather_the_contexts, go_to=investigate_each),
            Transition(after=investigate_each, go_to=cross_check_each),
            Transition(after=cross_check_each, go_to=write_the_candidates),
        ],
    )


def run_investigate(start: "InvestigateState", run_id: str, tracked: bool = True) -> "Report":
    outcome = investigate_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"investigate run {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class InvestigateState(BaseModel):
    run_dir: Path                                        # build/investigate/RUN_ID
    count: int = DEFAULT_COUNT
    only: tuple[FunctionName, ...] = ()
    retry_unnamed: str | None = None                     # an earlier run's ID: re-ask its unnamed ones
    gather: bool = True                                  # --facts: show what the original game did
                                                          # (default since v3: the contradiction check
                                                          # rejects names that contradict the facts)
    model: ModelSpec = DEFAULT_MODEL
    checker: ModelSpec = DEFAULT_CHECKER
    jobs: int = DEFAULT_JOBS
    targets: tuple[Target, ...] = ()
    contexts: tuple[Context, ...] = ()
    proposals: tuple[Proposal, ...] = ()
    verdicts: tuple[Verdict, ...] = ()
    notes: tuple[ToolMessage, ...] = ()
    report: Report | None = None


workflow = Workflow[InvestigateState](InvestigateState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class SelectInput(Result):
    run_dir: Path
    count: int
    only: tuple[FunctionName, ...]
    retry_unnamed: str | None = None


class SelectOutput(Result):
    targets: tuple[Target, ...]
    notes: tuple[ToolMessage, ...]


@workflow.step
def select_the_targets(given: SelectInput) -> SelectOutput:
    """The placeholder functions worth investigating first, and which --only names were not eligible."""
    retry = investigate.unnamed_names(given.retry_unnamed) if given.retry_unnamed \
        else frozenset[str]()
    if retry:
        eligible = select_targets(given.run_dir, len(retry), given.only, retry=retry)
        targets = eligible[:given.count]
    else:
        eligible = targets = select_targets(given.run_dir, given.count, given.only)
    notes = not_selectable(given.only, targets)
    if retry:
        have = {target.name for target in eligible}
        notes += tuple(ToolMessage(f"{name}: run {given.retry_unnamed} left it unnamed, but it is not "
                                   "an eligible placeholder any more (renamed, or no lifted unit)")
                       for name in sorted(retry) if name not in have)
    return SelectOutput(targets=targets, notes=notes)


class GatherInput(Result):
    targets: tuple[Target, ...]
    run_dir: Path
    gather: bool = False          # --facts: every context carries the runtime-facts block too


class GatherOutput(Result):
    contexts: tuple[Context, ...]


@workflow.step
def gather_the_contexts(given: GatherInput) -> GatherOutput:
    """Each target's brief: its assembly, bn6f's comment, its callers and callees with what the
    project knows about them, up to three callers' code around their call sites (how the callers
    set the call up and use the result), the data it touches (the assembly and the snippets
    are kept under the run's directory), and, with --facts, what the original game did at runtime
    (`wf facts`): the scenes it runs in, its runtime callers, the RAM and I/O it writes and the
    arguments and returns of the calls replayed in mGBA."""
    return GatherOutput(contexts=gather_all(given.targets, given.run_dir, given.gather))


class InvestigateInput(Result):
    contexts: tuple[Context, ...]
    model: ModelSpec
    jobs: int
    run_dir: Path


class InvestigateOutput(Result):
    proposals: tuple[Proposal, ...]


@workflow.step
def investigate_each(given: InvestigateInput) -> InvestigateOutput:
    """Ask the investigating model about each function, several at a time."""
    run_id = current_run_id()
    return InvestigateOutput(
        proposals=investigate_all(given.model, given.contexts, given.run_dir, run_id, given.jobs,
                                  name_prefixes()))


class CheckInput(Result):
    contexts: tuple[Context, ...]
    proposals: tuple[Proposal, ...]
    checker: ModelSpec
    jobs: int
    run_dir: Path


class CheckOutput(Result):
    verdicts: tuple[Verdict, ...]


@workflow.step
def cross_check_each(given: CheckInput) -> CheckOutput:
    """Have a different model check each proposal against the code (a proposal without a name is
    not asked: it stays a hypothesis with its open question)."""
    run_id = current_run_id()
    return CheckOutput(
        verdicts=check_all(given.checker, given.contexts, given.proposals, given.run_dir, run_id, given.jobs))


class WriteInput(Result):
    contexts: tuple[Context, ...]
    proposals: tuple[Proposal, ...]
    verdicts: tuple[Verdict, ...]
    run_dir: Path
    model: ModelSpec
    checker: ModelSpec
    notes: tuple[ToolMessage, ...]


class WriteOutput(Result):
    report: Report


@workflow.step
def write_the_candidates(given: WriteInput) -> WriteOutput:
    """A candidate symbols/db entry per function (validated with symdb's checks) and report.tsv."""
    by_name = {context.target.name: context for context in given.contexts}
    verdict_of = {verdict.name: verdict for verdict in given.verdicts}
    syms = investigate.elf_symbols(frozenset(proposal.name for proposal in given.proposals))
    written = agreed = revised = disagreed = nulls = invalid = retries = stragglers = rejected = 0
    problems: list[ToolMessage] = list(given.notes)
    pairs: list[tuple[Proposal, Verdict]] = []
    for proposal in given.proposals:
        context = by_name[proposal.name]
        verdict = verdict_of[proposal.name]
        if proposal.finished:
            continue
        pairs.append((proposal, verdict))
        retries += proposal.retries + verdict.retries
        if investigate.abandoned(proposal.problem):
            stragglers += 1
        if investigate.abandoned(verdict.problem):
            stragglers += 1
        if investigate.out_of_credits(proposal.problem) or investigate.out_of_credits(verdict.problem):
            problems.append(ToolMessage(f"{proposal.name}: {verdict.problem or proposal.problem}"))
            continue
        if proposal.rejection is not None:
            rejected += 1
            continue                    # recorded in report.tsv, but not landed as a candidate
        if proposal.answer is None or proposal.answer.proposed_name is None:
            nulls += 1
        else:
            check = verdict.reply.verdict if verdict.reply else "none"
            if check == "agree":
                agreed += 1
            elif check == "revise":
                revised += 1
            elif check == "disagree":
                disagreed += 1
        candidate = investigate.make_candidate(context, proposal, verdict, given.model, given.checker,
                                               current_run_id())
        investigate.write_candidate(candidate, given.run_dir)
        found = investigate.symdb_check(given.run_dir / f"{candidate.name}.yaml", syms)
        if found:
            invalid += 1
            problems += [ToolMessage(f"{candidate.name}: {problem}") for problem in found]
            continue
        written += 1
    report_path = given.run_dir / "report.tsv"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(investigate.report_rows(tuple(pairs)))
    stopped = next((problem for problem in problems if investigate.out_of_credits(problem)), None)
    if stopped is not None:
        stopped = ToolMessage(f"the checker or the model ran out of credits (HTTP 402); the run kept "
                              f"what was done and stopped cleanly ({stopped})")
    return WriteOutput(report=Report(selected=len(given.proposals), written=written, agreed=agreed,
                                     revised=revised, disagreed=disagreed, nulls=nulls, invalid=invalid,
                                     retries=retries, stragglers=stragglers, rejected=rejected,
                                     tokens=sum(proposal.tokens for proposal in given.proposals)
                                     + sum(verdict.tokens for verdict in given.verdicts),
                                     problems=tuple(problems), stopped=stopped))


# ── decisions ──────────────────────────────────────────────────────────

def some_selected(state: InvestigateState) -> bool:
    return bool(state.targets)


def none_selected(state: InvestigateState) -> bool:
    return not state.targets