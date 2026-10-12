"""Outside models convert small lifted functions in volume (docs/PROCESS.md
§4). One `farm` run is one function and one model: the model
writes readable C, the farm's acceptance test checks it against the lifted
original on the captured calls and random inputs, failures go back to the
model for repair. `--prompt old` asks with the prompt before the typed one (register-form C, no
register check; docs/reviews/2026-10-10-benchmark.md): its rounds and candidates are filed under the
model tagged `+old` (FUNC.minimax_MiniMax-M3+old.c), a pair of its own. With the typed prompt (typed-2),
a round starts from the best attempt so far, another model's included (the hand-over,
wf/tools/handover.py), and a passing candidate is kept with the generated wrappers it calls. (Entries for accepted functions are drafted afterwards by `wf drafts --drafter`, on a
plan's quota rather than the farm model's.) Earlier: an accepted function got its symbols/db entry drafted
by the same model. A `farm batch` run builds the host difftest once, skips
pairs that are already finished, and runs the rest as its child runs."""
import contextvars
from concurrent.futures import ThreadPoolExecutor
from enum import Enum
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.tools.budgets import reasoning_budget
from wf.tools.build import build_host_difftest, committed_tree
from wf.tools.handover import Tried, best_start, earlier_attempts, tried
from wf.tools.models import (CandidateTest, ConversionContext, ModelReply, Prompt, PromptKind, ReviewedCandidate,
                             accept, already_accepted, ask_model, conversion_context, finished, first_prompt, log_round,
                             logged_rounds, out_of_rounds_before, rejected_at_review, repair_prompt, test_candidate,
                             with_prompt, written_by_model)
from wf.tools.sources import first_up, models_for, out_for_long
from wf.types import BuildFailure, FunctionName, ModelSpec, NotRun, Result, SnapshotDir, ToolMessage

PROJECT = "recomp-farm"
# a retry's round 0 after a review rejected the model's candidate: the reason stands in for the test's report
REVIEWED = ("A reviewer read this candidate against the original's assembly and rejected it (it passed the "
            "automatic checks):\n{review}\nFix what the review found; keep what it did not question.")


def farm_graph() -> Graph["FarmState"]:
    return farm.graph(
        first=gather_context,
        ends=[accept_candidate, give_up],
        transitions=[
            Transition(after=gather_context, go_to=ask_model_for_c, when=there_is_something_to_convert),
            Transition(after=gather_context, go_to=give_up, when=there_is_nothing_to_convert),
            Transition(after=ask_model_for_c, go_to=test_reply, when=the_model_answered),
            Transition(after=ask_model_for_c, go_to=give_up, when=the_model_failed),
            Transition(after=test_reply, go_to=accept_candidate, when=the_candidate_passed),
            Transition(after=test_reply, go_to=give_up, when=the_reply_had_no_code),
            Transition(after=test_reply, go_to=ask_model_for_c, when=it_failed_with_rounds_left),
            Transition(after=test_reply, go_to=give_up, when=it_failed_with_no_rounds_left),
        ],
    )


def farm_batch_graph() -> Graph["FarmBatchState"]:
    return batch.graph(
        first=build_difftest,
        ends=[summarise],
        transitions=[
            Transition(after=build_difftest, go_to=convert_each_pair, when=difftest_built),
            Transition(after=build_difftest, go_to=summarise, when=difftest_did_not_build),
            Transition(after=convert_each_pair, go_to=summarise),
        ],
    )


# ── one function, one model ────────────────────────────────────────────

class FarmOutcome(Enum):
    ACCEPTED = "accepted"
    OUT_OF_ROUNDS = "failed every round"
    MODEL_ERROR = "the model's provider failed"
    RUNAWAY = "the reply had no code (all reasoning): left for another model"
    SOURCES_OUT = "every source was out of quota (left for a later batch)"
    REJECTED_AT_REVIEW = "a review rejected this model's candidate (left for the queue's retry with the review)"
    NOTHING_TO_CONVERT = "no lifted code, assembly or captured calls"


class Attempt(Result):
    round_number: int
    prompt: Prompt
    reply: ModelReply
    test: CandidateTest | None  # None when the reply had no code


class FarmState(BaseModel):
    function: FunctionName
    model: ModelSpec
    snapshots: SnapshotDir
    difftest: Path
    rounds: int                                  # repair rounds after the first attempt
    prompt: PromptKind = PromptKind.TYPED        # which prompt it is asked with
    source_wait: float = 7200                    # seconds to wait for a provider that is down (the queue: 0,
                                                 # it gives the job to another provider instead)
    reviewed: ReviewedCandidate | None = None    # a retry after a review rejected this model's candidate: round 0
                                                 # starts from it, with the reviewer's reason (the queue sets it)
    context: ConversionContext | NotRun | None = None
    earlier: tuple[Tried, ...] = ()              # what other models (and runs) wrote, tested today, best first
    attempts: tuple[Attempt, ...] = ()
    accepted: Path | None = None
    outcome: FarmOutcome | None = None


farm = Workflow[FarmState](FarmState, project=PROJECT)


class GatherInput(Result):
    function: FunctionName
    snapshots: SnapshotDir
    difftest: Path
    prompt: PromptKind


class GatherOutput(Result):
    context: ConversionContext | NotRun
    earlier: tuple[Tried, ...] = ()


@farm.step
def gather_context(given: GatherInput) -> GatherOutput:
    """Find the function's assembly, lifted C and register contract, and its captured calls; for the
    typed prompt, also every earlier attempt at it, tested today (the hand-over)."""
    if not (given.snapshots / given.function).is_dir():
        return GatherOutput(context=NotRun(reason=ToolMessage(f"no captured calls in {given.snapshots}")))
    context = conversion_context(given.function)
    if context is None:
        return GatherOutput(context=NotRun(reason=ToolMessage("no lifted code or assembly found")))
    earlier = (earlier_attempts(context, given.difftest, snapshots=given.snapshots)
               if given.prompt is PromptKind.TYPED else ())
    return GatherOutput(context=context, earlier=earlier)


class AskInput(Result):
    context: ConversionContext
    model: ModelSpec
    prompt: PromptKind
    source_wait: float
    reviewed: ReviewedCandidate | None
    earlier: tuple[Tried, ...]
    attempts: tuple[Attempt, ...]


class AskOutput(Result):
    attempts: tuple[Attempt, ...]


@farm.step
def ask_model_for_c(given: AskInput) -> AskOutput:
    """Ask the model (or the first source of a tier that is up) for readable C. The typed prompt starts from
    the best attempt so far (another model's, or one of this run's) with every problem its test found and a
    history line of the others; the old prompt shows the last failure and its code."""
    start = best_start((*given.earlier, *own_attempts(given.attempts))) if given.prompt is PromptKind.TYPED else None
    if given.reviewed is not None and not given.attempts:   # a review rejected this model's candidate: fix that
        prompt = repair_prompt(given.context, ToolMessage(REVIEWED.format(review=given.reviewed.review)),
                               written_by_model(given.context.function, given.reviewed.code), given.prompt,
                               whose="your earlier candidate, which a reviewer rejected")
    elif start is not None:
        prompt = repair_prompt(given.context, start.best.report, start.best.code, given.prompt, whose=start.best.whose,
                               history=start.history)
    elif given.attempts and given.prompt is not PromptKind.TYPED:
        last = given.attempts[-1]
        failure = last.test.report if last.test else ToolMessage("the reply had no ```c code block")
        prompt = repair_prompt(given.context, failure, last.reply.candidate, given.prompt)
    else:
        prompt = first_prompt(given.context, given.prompt)
    round_number = len(given.attempts)
    models = models_for(given.model)   # one model, or a tier: the first source that is up answers
    tag = f"{given.context.function} round {round_number}"
    waits = len(models) == 1 and given.source_wait > 0

    def ask(model: ModelSpec) -> ModelReply:
        """A reply whose reasoning passes the model's budget with no code begun is stopped (a runaway)."""
        budget = reasoning_budget(model, given.prompt, given.context.function).tokens
        return ask_model(model, prompt, tag, wait_out_limits=waits, kind=given.prompt, budget=budget)

    _, reply = first_up(models, ask, lambda reply: reply.error, horizon=given.source_wait)
    attempt = Attempt(round_number=round_number, prompt=prompt, reply=reply, test=None)
    return AskOutput(attempts=(*given.attempts, attempt))


def own_attempts(attempts: tuple[Attempt, ...]) -> tuple[Tried, ...]:
    """This run's tested attempts, ranked (a reply without code ends the run, so every earlier one has a test)."""
    return tuple(tried(attempt.reply.candidate, attempt.test.passed, attempt.test.report,
                       "your last attempt" if i == len(attempts) - 1 else f"your round {attempt.round_number}")
                 for i, attempt in enumerate(attempts)
                 if attempt.reply.candidate is not None and attempt.test is not None)


class TestInput(Result):
    context: ConversionContext
    model: ModelSpec
    prompt: PromptKind
    reviewed: ReviewedCandidate | None
    snapshots: SnapshotDir
    difftest: Path
    attempts: tuple[Attempt, ...]


class TestOutput(Result):
    attempts: tuple[Attempt, ...]


@farm.step
def test_reply(given: TestInput) -> TestOutput:
    """Test the model's candidate the way farm.py accepts one (the register check only for the typed
    prompt), and log the round as farm.py does."""
    latest = given.attempts[-1]
    test = (test_candidate(given.context, latest.reply.candidate, given.snapshots, given.difftest,
                           check_registers=given.prompt.checks_registers,
                           first_failure_only=given.prompt.first_failure_only)
            if latest.reply.candidate is not None else None)
    log_round(given.context.function, latest.reply.model or given.model, latest.round_number, latest.reply, test,
              given.difftest, given.prompt, after_review=given.reviewed is not None)
    return TestOutput(attempts=(*given.attempts[:-1], latest.model_copy(update={"test": test})))


class AcceptInput(Result):
    function: FunctionName
    model: ModelSpec
    prompt: PromptKind
    reviewed: ReviewedCandidate | None
    attempts: tuple[Attempt, ...]


class AcceptOutput(Result):
    accepted: Path
    outcome: FarmOutcome


@farm.step
def accept_candidate(given: AcceptInput) -> AcceptOutput:
    """Keep the passing candidate in tools/convert/accepted for review (filed under the model tagged with
    its prompt when that is not the typed one; a typed one with the generated wrappers it calls and the
    tool's register-form notes, so it stands on its own)."""
    reply = given.attempts[-1].reply
    assert reply.candidate is not None
    code = finished(given.function, reply.candidate) if given.prompt is PromptKind.TYPED else reply.candidate
    return AcceptOutput(accepted=accept(given.function, with_prompt(reply.model or given.model, given.prompt), code,
                                        after_review=given.reviewed),
                        outcome=FarmOutcome.ACCEPTED)




class GiveUpInput(Result):
    function: FunctionName
    model: ModelSpec
    prompt: PromptKind
    context: ConversionContext | NotRun
    attempts: tuple[Attempt, ...]


class GiveUpOutput(Result):
    outcome: FarmOutcome


@farm.step
def give_up(given: GiveUpInput) -> GiveUpOutput:
    """Record why there is no accepted candidate."""
    if isinstance(given.context, NotRun):
        return GiveUpOutput(outcome=FarmOutcome.NOTHING_TO_CONVERT)
    if given.attempts and given.attempts[-1].reply.error is not None:
        last = given.attempts[-1]
        log_round(given.function, last.reply.model or given.model, last.round_number, last.reply, None,
                  prompt=given.prompt)
        return GiveUpOutput(outcome=FarmOutcome.MODEL_ERROR)
    if given.attempts and given.attempts[-1].reply.candidate is None:
        return GiveUpOutput(outcome=FarmOutcome.RUNAWAY)
    return GiveUpOutput(outcome=FarmOutcome.OUT_OF_ROUNDS)


def there_is_something_to_convert(state: FarmState) -> bool:
    return isinstance(state.context, ConversionContext)


def there_is_nothing_to_convert(state: FarmState) -> bool:
    return isinstance(state.context, NotRun)


def the_model_answered(state: FarmState) -> bool:
    return state.attempts[-1].reply.error is None


def the_model_failed(state: FarmState) -> bool:
    return state.attempts[-1].reply.error is not None


def the_candidate_passed(state: FarmState) -> bool:
    test = state.attempts[-1].test
    return test is not None and test.passed


def the_reply_had_no_code(state: FarmState) -> bool:
    """A runaway: never sent the same prompt again (review 3, B3); another model gets the function later."""
    return state.attempts[-1].reply.candidate is None and state.attempts[-1].reply.error is None


def it_failed_with_rounds_left(state: FarmState) -> bool:
    return (not the_candidate_passed(state) and not the_reply_had_no_code(state)
            and len(state.attempts) <= state.rounds)


def it_failed_with_no_rounds_left(state: FarmState) -> bool:
    return (not the_candidate_passed(state) and not the_reply_had_no_code(state)
            and len(state.attempts) > state.rounds)


# ── a batch: many functions, many models ───────────────────────────────

class PairResult(Result):
    function: FunctionName
    model: ModelSpec
    run_id: str | None                # None when the pair was finished before this batch
    outcome: FarmOutcome
    accepted: Path | None


class ModelTally(Result):
    model: ModelSpec
    accepted: int
    tried: int


class FarmBatchState(BaseModel):
    run_dir: Path
    functions: tuple[FunctionName, ...]
    models: tuple[ModelSpec, ...]
    snapshots: SnapshotDir
    rounds: int = 3
    jobs: int = 4
    prompt: PromptKind = PromptKind.TYPED
    difftest: Path | BuildFailure | None = None
    pairs: tuple[PairResult, ...] = ()
    tallies: tuple[ModelTally, ...] = ()


batch = Workflow[FarmBatchState](FarmBatchState, project=PROJECT)


class BuildDifftestInput(Result):
    run_dir: Path
    prompt: PromptKind


class BuildDifftestOutput(Result):
    difftest: Path | BuildFailure


@batch.step
def build_difftest(given: BuildDifftestInput) -> BuildDifftestOutput:
    """Build the host difftest once, from the last commit in a worktree of its own (a stale one once
    accepted bad candidates; one built from the main tree saw a conversion's verify half done). Each
    prompt's batches have a tree of their own, so batches of the two can run side by side."""
    name = "farm" if given.prompt is PromptKind.TYPED else f"farm-{given.prompt.value}"
    return BuildDifftestOutput(difftest=build_host_difftest(given.run_dir, build_dir="build-wf-farm-host",
                                                            tree=committed_tree(name)))


class ConvertPairsInput(Result):
    functions: tuple[FunctionName, ...]
    models: tuple[ModelSpec, ...]
    snapshots: SnapshotDir
    rounds: int
    prompt: PromptKind
    jobs: int
    difftest: Path


class ConvertPairsOutput(Result):
    pairs: tuple[PairResult, ...]


@batch.step
def convert_each_pair(given: ConvertPairsInput) -> ConvertPairsOutput:
    """Run one farm run per function and model (in parallel), skipping pairs finished before
    (accepted, or out of rounds: farm.py's resume rule). A pair is a model and a prompt: a model that
    tried a function with the typed prompt may still try it with the old one."""
    batch_run_id = current_run_id()

    def convert(function: FunctionName, model: ModelSpec) -> PairResult:
        sources = models_for(model)   # a tier's pair is finished by any of its models
        filed = tuple(with_prompt(source, given.prompt) for source in sources)   # as its logs and files are named
        accepted = next((path for path in (already_accepted(function, source) for source in filed) if path), None)
        if accepted is not None:
            return PairResult(function=function, model=model, run_id=None, outcome=FarmOutcome.ACCEPTED,
                              accepted=accepted)
        if any(rejected_at_review((function,), source) for source in filed):   # a reviewed verdict: never refilled
            return PairResult(function=function, model=model, run_id=None, outcome=FarmOutcome.REJECTED_AT_REVIEW,
                              accepted=None)
        if (out_of_rounds_before(function, filed[0], given.rounds) if len(sources) == 1
                else sum(logged_rounds(function, source) for source in filed) > given.rounds):
            return PairResult(function=function, model=model, run_id=None, outcome=FarmOutcome.OUT_OF_ROUNDS,
                              accepted=None)
        if out_for_long(sources):
            return PairResult(function=function, model=model, run_id=None, outcome=FarmOutcome.SOURCES_OUT,
                              accepted=None)
        run_id = f"{batch_run_id}-{function}-{model.replace('/', '_')}"   # tier/cheap -> tier_cheap
        start = FarmState(function=function, model=model, snapshots=given.snapshots, difftest=given.difftest,
                          rounds=given.rounds, prompt=given.prompt)
        outcome = farm_graph().start(start, run_id)
        if not isinstance(outcome, Finished) or outcome.state.outcome is None:
            raise RuntimeError(f"farm run {run_id} did not finish")
        return PairResult(function=function, model=model, run_id=run_id, outcome=outcome.state.outcome,
                          accepted=outcome.state.accepted)

    with ThreadPoolExecutor(max_workers=given.jobs) as pool:
        # each child run is started in a copy of this step's context, so the UI links it to this step
        runs = [pool.submit(contextvars.copy_context().run, convert, function, model)
                for model in given.models for function in given.functions]
        return ConvertPairsOutput(pairs=tuple(run.result() for run in runs))


class SummariseInput(Result):
    models: tuple[ModelSpec, ...]
    pairs: tuple[PairResult, ...]


class SummariseOutput(Result):
    tallies: tuple[ModelTally, ...]


@batch.step
def summarise(given: SummariseInput) -> SummariseOutput:
    """How many functions each model converted (tools/convert/scorecard.py has the cost)."""
    return SummariseOutput(tallies=tuple(
        ModelTally(model=model,
                   accepted=sum(1 for pair in given.pairs if pair.model == model and pair.outcome is FarmOutcome.ACCEPTED),
                   tried=sum(1 for pair in given.pairs if pair.model == model))
        for model in given.models))


def difftest_built(state: FarmBatchState) -> bool:
    return isinstance(state.difftest, Path)


def difftest_did_not_build(state: FarmBatchState) -> bool:
    return isinstance(state.difftest, BuildFailure)
