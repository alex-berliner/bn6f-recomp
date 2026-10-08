"""Outside models convert small lifted functions (docs/PROCESS.md §4, "By
outside models"). One `farm` run is one function and one model: the model
writes readable C, the farm's acceptance test checks it against the lifted
original on the captured calls and random inputs, failures go back to the
model for repair. A `farm batch` run builds the host difftest once, skips
pairs that are already finished, and runs the rest as its child runs."""
import contextvars
from concurrent.futures import ThreadPoolExecutor
from enum import Enum
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.tools.build import build_host_difftest
from wf.tools.models import (CandidateTest, ConversionContext, ModelReply, Prompt, accept, already_accepted,
                             ask_model, conversion_context, first_prompt, log_round, out_of_rounds_before,
                             repair_prompt, test_candidate)
from wf.types import BuildFailure, FunctionName, ModelSpec, NotRun, Result, SnapshotDir, ToolMessage

PROJECT = "recomp-farm"


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
    context: ConversionContext | NotRun | None = None
    attempts: tuple[Attempt, ...] = ()
    accepted: Path | None = None
    outcome: FarmOutcome | None = None


farm = Workflow[FarmState](FarmState, project=PROJECT)


class GatherInput(Result):
    function: FunctionName
    snapshots: SnapshotDir


class GatherOutput(Result):
    context: ConversionContext | NotRun


@farm.step
def gather_context(given: GatherInput) -> GatherOutput:
    """Find the function's assembly, lifted C and register contract, and its captured calls."""
    if not (given.snapshots / given.function).is_dir():
        return GatherOutput(context=NotRun(reason=ToolMessage(f"no captured calls in {given.snapshots}")))
    context = conversion_context(given.function)
    if context is None:
        return GatherOutput(context=NotRun(reason=ToolMessage("no lifted code or assembly found")))
    return GatherOutput(context=context)


class AskInput(Result):
    context: ConversionContext
    model: ModelSpec
    attempts: tuple[Attempt, ...]


class AskOutput(Result):
    attempts: tuple[Attempt, ...]


@farm.step
def ask_model_for_c(given: AskInput) -> AskOutput:
    """Ask the model for readable C; after a failure, show it the failure and its code."""
    if given.attempts:
        last = given.attempts[-1]
        failure = last.test.report if last.test else ToolMessage("the reply had no ```c code block")
        prompt = repair_prompt(given.context, failure, last.reply.candidate)
    else:
        prompt = first_prompt(given.context)
    round_number = len(given.attempts)
    reply = ask_model(given.model, prompt, tag=f"{given.context.function} round {round_number}")
    attempt = Attempt(round_number=round_number, prompt=prompt, reply=reply, test=None)
    return AskOutput(attempts=(*given.attempts, attempt))


class TestInput(Result):
    context: ConversionContext
    model: ModelSpec
    snapshots: SnapshotDir
    difftest: Path
    attempts: tuple[Attempt, ...]


class TestOutput(Result):
    attempts: tuple[Attempt, ...]


@farm.step
def test_reply(given: TestInput) -> TestOutput:
    """Test the model's candidate the way farm.py accepts one, and log the round as farm.py does."""
    latest = given.attempts[-1]
    test = (test_candidate(given.context, latest.reply.candidate, given.snapshots, given.difftest)
            if latest.reply.candidate is not None else None)
    log_round(given.context.function, given.model, latest.round_number, latest.reply, test)
    return TestOutput(attempts=(*given.attempts[:-1], latest.model_copy(update={"test": test})))


class AcceptInput(Result):
    function: FunctionName
    model: ModelSpec
    attempts: tuple[Attempt, ...]


class AcceptOutput(Result):
    accepted: Path
    outcome: FarmOutcome


@farm.step
def accept_candidate(given: AcceptInput) -> AcceptOutput:
    """Keep the passing candidate in tools/convert/accepted for review."""
    candidate = given.attempts[-1].reply.candidate
    assert candidate is not None
    return AcceptOutput(accepted=accept(given.function, given.model, candidate), outcome=FarmOutcome.ACCEPTED)


class GiveUpInput(Result):
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
        return GiveUpOutput(outcome=FarmOutcome.MODEL_ERROR)
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


def it_failed_with_rounds_left(state: FarmState) -> bool:
    return not the_candidate_passed(state) and len(state.attempts) <= state.rounds


def it_failed_with_no_rounds_left(state: FarmState) -> bool:
    return not the_candidate_passed(state) and len(state.attempts) > state.rounds


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
    difftest: Path | BuildFailure | None = None
    pairs: tuple[PairResult, ...] = ()
    tallies: tuple[ModelTally, ...] = ()


batch = Workflow[FarmBatchState](FarmBatchState, project=PROJECT)


class BuildDifftestInput(Result):
    run_dir: Path


class BuildDifftestOutput(Result):
    difftest: Path | BuildFailure


@batch.step
def build_difftest(given: BuildDifftestInput) -> BuildDifftestOutput:
    """Build the host difftest once, from the current sources (a stale one once accepted bad candidates)."""
    return BuildDifftestOutput(difftest=build_host_difftest(given.run_dir))


class ConvertPairsInput(Result):
    functions: tuple[FunctionName, ...]
    models: tuple[ModelSpec, ...]
    snapshots: SnapshotDir
    rounds: int
    jobs: int
    difftest: Path


class ConvertPairsOutput(Result):
    pairs: tuple[PairResult, ...]


@batch.step
def convert_each_pair(given: ConvertPairsInput) -> ConvertPairsOutput:
    """Run one farm run per function and model (in parallel), skipping pairs finished before
    (accepted, or out of rounds: farm.py's resume rule)."""
    batch_run_id = current_run_id()

    def convert(function: FunctionName, model: ModelSpec) -> PairResult:
        accepted = already_accepted(function, model)
        if accepted is not None:
            return PairResult(function=function, model=model, run_id=None, outcome=FarmOutcome.ACCEPTED,
                              accepted=accepted)
        if out_of_rounds_before(function, model, given.rounds):
            return PairResult(function=function, model=model, run_id=None, outcome=FarmOutcome.OUT_OF_ROUNDS,
                              accepted=None)
        run_id = f"{batch_run_id}-{function}-{model.replace('/', '_')}"
        start = FarmState(function=function, model=model, snapshots=given.snapshots, difftest=given.difftest,
                          rounds=given.rounds)
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
