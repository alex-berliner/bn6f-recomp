"""A strong outside model converts a subsystem (callers and callees together)
to readable C with C call and return semantics (docs/PROCESS.md §4): capture
the original's calls, ask the model for the whole file, quick-test it per
function against the lifted originals, install it and run the full verify,
then stop for the orchestrator's review. Failures and review comments go back
to the model (3 attempts per model, then the next model). Approved work is
committed, its symbols/db entries are written from the model's drafts plus the
run's own evidence, and a child record run renames and documents it.
The orchestrator never writes the code: it chooses the work and reviews."""
from enum import Enum
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Graph, Transition, Workflow, current_run_id
from wf.graphs.capture import CaptureReport, CaptureState, run_capture
from wf.graphs.record import RecordReport, RecordState, run_record
from wf.graphs.verify import Verdict, VerifyReport, VerifyState, run_verify_in_full
from wf.routes import ALL_ROUTES
from wf.tools.build import build_host_difftest
from wf.tools.drafts import candidate_from_farm
from wf.tools.evidence import FunctionChecks, Provenance, renames, write_entries
from wf.tools.git import commit_files
from wf.tools.install import DIFFTEST_CFG, game_file, install_file, uninstall_file
from wf.tools.models import ConversionContext, conversion_context, other_entries, provider_blocked_until
from wf.tools.subsystem import (FileReply, FileTest, TokenUse, ask_model_for_file, candidate_from_file, repair_prompt,
                                subsystem_prompt, test_candidate_file, tokens_for)
from wf.types import (BuildFailure, CommitHash, FunctionName, ModelSpec, NotRun, Rename, Result, Route, SnapshotDir,
                      SymbolName, ToolMessage)

PROJECT = "recomp-convert"
DEFAULT_MODELS = (ModelSpec("opencode/deepseek-v4-pro"), ModelSpec("opencode/kimi-k3"), ModelSpec("zai/glm-5.3"))
GIVEN_CANDIDATE = ModelSpec("given candidate")
ACCEPTED_VERDICTS = (Verdict.PASSED, Verdict.PASSED_WITH_UNCHECKED_FUNCTIONS)


def convert_graph() -> Graph["ConvertState"]:
    return workflow.graph(
        first=capture_original_calls,
        ends=[record_symbols, give_up],
        transitions=[
            Transition(after=capture_original_calls, go_to=gather_context),
            Transition(after=gather_context, go_to=ask_model_for_file_step, when=ready_to_convert),
            Transition(after=gather_context, go_to=give_up, when=cannot_convert),
            Transition(after=ask_model_for_file_step, go_to=quick_test),
            Transition(after=quick_test, go_to=install_and_verify, when=quick_test_passed),
            Transition(after=quick_test, go_to=ask_model_for_file_step, when=quick_test_failed_with_attempts_left),
            Transition(after=quick_test, go_to=give_up, when=quick_test_failed_with_no_attempts_left),
            Transition(after=install_and_verify, go_to=review_conversion, when=verified),
            Transition(after=install_and_verify, go_to=ask_model_for_file_step, when=not_verified_with_attempts_left),
            Transition(after=install_and_verify, go_to=give_up, when=not_verified_with_no_attempts_left),
            Transition(after=review_conversion, go_to=commit_conversion, when=approved),
            Transition(after=review_conversion, go_to=ask_model_for_file_step, when=changes_requested_with_attempts_left),
            Transition(after=review_conversion, go_to=give_up, when=rejected_or_no_attempts_left),
            Transition(after=commit_conversion, go_to=write_symbol_entries),
            Transition(after=write_symbol_entries, go_to=record_symbols),
        ],
    )


# ── the run's state ────────────────────────────────────────────────────

class ReviewDecision(Enum):
    APPROVE = "approve"
    REQUEST_CHANGES = "request changes"   # the comments go to the model, which edits the file
    REJECT = "reject"


class Review(Result):
    """The orchestrator's answer at the review pause point."""
    decision: ReviewDecision
    comments: str = ""
    attribution: str = ""   # lines appended to the commit messages (who reviewed, the session)


class Attempt(Result):
    number: int
    model: ModelSpec          # who wrote the file
    given: bool = False       # a ready file (--candidate, --farm-model): not one of the models' attempts
    reply: FileReply
    tokens: TokenUse | None = None
    test: FileTest | None = None
    verification: VerifyReport | None = None
    verify_run: str | None = None
    review: Review | None = None


class ConvertOutcome(Enum):
    RECORDED = "committed and recorded"
    REJECTED = "rejected at review"
    OUT_OF_ATTEMPTS = "every model ran out of attempts"
    NOTHING_TO_CONVERT = "nothing to convert (no lifted code, or the file exists)"
    NO_CAPTURED_CALLS = "some functions have no captured calls, so they could not be checked"
    DIFFTEST_DID_NOT_BUILD = "the host difftest did not build"


class ConvertState(BaseModel):
    run_dir: Path
    subsystem: str
    file_name: str                                   # the new file under cport/src/game
    functions: tuple[FunctionName, ...]
    models: tuple[ModelSpec, ...] = DEFAULT_MODELS   # escalation order
    rounds: int = 3                                  # attempts per model
    routes: tuple[Route, ...] = ALL_ROUTES
    battle_code: bool = False
    snapshots: SnapshotDir | None = None             # reuse a capture instead of capturing
    candidate: Path | None = None                    # a ready file tried first
    farm_model: ModelSpec | None = None              # or: the farm's accepted candidates by this model, tried first
    capture: CaptureReport | NotRun | None = None
    contexts: tuple[ConversionContext, ...] | NotRun | None = None
    difftest: Path | BuildFailure | None = None
    without_calls: tuple[FunctionName, ...] = ()
    attempts: tuple[Attempt, ...] = ()
    checks: tuple[FunctionChecks, ...] = ()
    commit: CommitHash | None = None
    entries: tuple[Path, ...] = ()
    renames: tuple[Rename, ...] = ()
    record: RecordReport | None = None
    outcome: ConvertOutcome | None = None


workflow = Workflow[ConvertState](ConvertState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class CaptureInput(Result):
    run_dir: Path
    functions: tuple[FunctionName, ...]
    routes: tuple[Route, ...]
    snapshots: SnapshotDir | None


class CaptureOutput(Result):
    capture: CaptureReport | NotRun
    snapshots: SnapshotDir


@workflow.step
def capture_original_calls(given: CaptureInput) -> CaptureOutput:
    """Save the original game's calls of the functions (a child capture run), unless given a capture."""
    if given.snapshots is not None:
        return CaptureOutput(capture=NotRun(reason=ToolMessage(f"reusing {given.snapshots}")), snapshots=given.snapshots)
    start = CaptureState(run_dir=given.run_dir, functions=given.functions,
                         snapshots=SnapshotDir(given.run_dir / "snapshots"), candidate_routes=given.routes)
    report = run_capture(start, f"{current_run_id()}-capture")
    return CaptureOutput(capture=report, snapshots=report.snapshots)


class GatherInput(Result):
    run_dir: Path
    file_name: str
    functions: tuple[FunctionName, ...]
    snapshots: SnapshotDir


class GatherOutput(Result):
    contexts: tuple[ConversionContext, ...] | NotRun
    difftest: Path | BuildFailure
    without_calls: tuple[FunctionName, ...]


@workflow.step
def gather_context(given: GatherInput) -> GatherOutput:
    """Each function's assembly, lifted C and register contract, and that it has captured calls (no model is
    asked to write what nothing could check); labels callers enter in the middle of a function join the
    subsystem (a replacement must define them); build the host difftest for quick tests."""
    difftest = build_host_difftest(given.run_dir)
    without_calls = tuple(function for function in given.functions if not _has_calls(given.snapshots, function))
    if game_file(given.file_name).exists():
        return GatherOutput(contexts=NotRun(reason=ToolMessage(f"cport/src/game/{given.file_name} already exists")),
                            difftest=difftest, without_calls=without_calls)
    found = {function: conversion_context(function) for function in given.functions}
    missing = [function for function, context in found.items() if context is None]
    if missing:
        return GatherOutput(contexts=NotRun(reason=ToolMessage(f"no lifted code or assembly: {', '.join(missing)}")),
                            difftest=difftest, without_calls=without_calls)
    contexts = tuple(context for context in found.values() if context is not None)
    with_entries = tuple(each for context in contexts for each in (context, *other_entries(context)))
    return GatherOutput(contexts=with_entries, difftest=difftest, without_calls=without_calls)


class AskInput(Result):
    run_dir: Path
    subsystem: str
    file_name: str
    contexts: tuple[ConversionContext, ...]
    models: tuple[ModelSpec, ...]
    rounds: int
    candidate: Path | None
    farm_model: ModelSpec | None
    attempts: tuple[Attempt, ...]


class AskOutput(Result):
    attempts: tuple[Attempt, ...]


@workflow.step
def ask_model_for_file_step(given: AskInput) -> AskOutput:
    """Ask the next model for the whole file (passing over a rate-limited provider); after a failure or
    review, show it what went wrong."""
    number = len(given.attempts) + 1
    if not given.attempts and given.candidate is not None:
        attempt = Attempt(number=number, model=GIVEN_CANDIDATE, given=True, reply=candidate_from_file(given.candidate))
        return AskOutput(attempts=(attempt,))
    if not given.attempts and given.farm_model is not None:
        functions = tuple(context.function for context in given.contexts)
        reply = candidate_from_farm(functions, given.farm_model, given.subsystem, given.file_name)
        return AskOutput(attempts=(Attempt(number=number, model=given.farm_model, given=True, reply=reply),))
    escalation = given.models[min(_model_attempts(given.attempts) // given.rounds, len(given.models) - 1):]
    # a model whose provider is rate-limited (for hours, at times) is passed over for the next one
    model = next((model for model in escalation if provider_blocked_until(model) is None), escalation[0])
    prompt = subsystem_prompt(given.subsystem, given.file_name, given.contexts)
    if given.attempts:
        what_failed, report = _what_went_wrong(given.attempts[-1])
        prompt = repair_prompt(prompt, what_failed, report, given.attempts[-1].reply)
    tag = f"{current_run_id()} round {number}"
    reply = ask_model_for_file(model, prompt, tag, given.run_dir / "prompts")
    return AskOutput(attempts=(*given.attempts, Attempt(number=number, model=model, reply=reply, tokens=tokens_for(tag))))


class QuickTestInput(Result):
    contexts: tuple[ConversionContext, ...]
    snapshots: SnapshotDir
    difftest: Path
    attempts: tuple[Attempt, ...]


class QuickTestOutput(Result):
    attempts: tuple[Attempt, ...]


@workflow.step
def quick_test(given: QuickTestInput) -> QuickTestOutput:
    """Each function of the file against its lifted original on the captured calls and random inputs."""
    latest = given.attempts[-1]
    test = test_candidate_file(given.contexts, latest.reply, given.snapshots, given.difftest)
    return QuickTestOutput(attempts=(*given.attempts[:-1], latest.model_copy(update={"test": test})))


class VerifyInput(Result):
    run_dir: Path
    file_name: str
    contexts: tuple[ConversionContext, ...]
    snapshots: SnapshotDir
    routes: tuple[Route, ...]
    battle_code: bool
    attempts: tuple[Attempt, ...]


class VerifyOutput(Result):
    attempts: tuple[Attempt, ...]
    checks: tuple[FunctionChecks, ...]


@workflow.step
def install_and_verify(given: VerifyInput) -> VerifyOutput:
    """Put the file in the tree and run every check (a child verify run); take it out again if it fails."""
    latest = given.attempts[-1]
    assert latest.reply.file is not None
    install_file(given.file_name, latest.reply.file, latest.reply.cfg_lines)
    run_id = f"{current_run_id()}-verify-{latest.number}"
    run_dir = given.run_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    functions = tuple(context.function for context in given.contexts if context.entry_of is None)
    state = run_verify_in_full(VerifyState(run_dir=run_dir, functions=functions, snapshots=given.snapshots,
                                           routes=given.routes, battle_code=given.battle_code), run_id)
    assert state.report is not None
    if state.report.verdict not in ACCEPTED_VERDICTS:
        uninstall_file(given.file_name)
    checks = tuple(_checks(context, state) for context in given.contexts)
    updated = latest.model_copy(update={"verification": state.report, "verify_run": run_id})
    return VerifyOutput(attempts=(*given.attempts[:-1], updated), checks=checks)


class ReviewRequest(Result):
    """What the orchestrator is shown: the file, its promises and drafts, the evidence, the attempts."""
    subsystem: str
    file_name: str
    attempts: tuple[Attempt, ...]


class ReviewOutput(Result):
    attempts: tuple[Attempt, ...]


@workflow.pause_point
def review_conversion(given: ReviewRequest, answer: Review) -> ReviewOutput:
    """The orchestrator reviews: approve, request changes (they go back to the model) or reject."""
    latest = given.attempts[-1]
    return ReviewOutput(attempts=(*given.attempts[:-1], latest.model_copy(update={"review": answer})))


class CommitInput(Result):
    subsystem: str
    file_name: str
    functions: tuple[FunctionName, ...]
    attempts: tuple[Attempt, ...]


class CommitOutput(Result):
    commit: CommitHash


@workflow.step
def commit_conversion(given: CommitInput) -> CommitOutput:
    """Commit the file and its difftest.cfg lines locally (never pushed)."""
    latest = given.attempts[-1]
    assert latest.review is not None and latest.verification is not None
    message = (f"Readable {given.subsystem} ({len(given.functions)} functions, written by {latest.model})\n\n"
               f"{', '.join(given.functions)}.\nWritten by {latest.model} through the convert workflow "
               f"(run {current_run_id()}, attempt {latest.number}); quick test and full verify passed "
               f"(verify run {latest.verify_run}: {latest.verification.verdict.value}); reviewed and approved"
               + (f": {latest.review.comments}" if latest.review.comments else "") + ".\n\n"
               + latest.review.attribution)
    return CommitOutput(commit=commit_files((game_file(given.file_name), DIFFTEST_CFG), message.strip() + "\n"))


class EntriesInput(Result):
    subsystem: str
    file_name: str
    attempts: tuple[Attempt, ...]
    checks: tuple[FunctionChecks, ...]
    commit: CommitHash


class EntriesOutput(Result):
    entries: tuple[Path, ...]
    renames: tuple[Rename, ...]


@workflow.step
def write_symbol_entries(given: EntriesInput) -> EntriesOutput:
    """symbols/db entries from the model's drafts plus this run's evidence; renames for confirmed/probable meanings."""
    latest = given.attempts[-1]
    assert latest.verify_run is not None
    provenance = Provenance(subsystem=given.subsystem, file=game_file(given.file_name), model=latest.model,
                            attempts=latest.number, convert_run=current_run_id(), verify_run=latest.verify_run,
                            commit=given.commit)
    return EntriesOutput(entries=write_entries(latest.reply.drafts, given.checks, provenance),
                         renames=renames(latest.reply.drafts, given.checks))


class RecordInput(Result):
    run_dir: Path
    subsystem: str
    functions: tuple[FunctionName, ...]
    renames: tuple[Rename, ...]
    attempts: tuple[Attempt, ...]


class RecordOutput(Result):
    record: RecordReport
    outcome: ConvertOutcome


@workflow.step
def record_symbols(given: RecordInput) -> RecordOutput:
    """Check the entries, rename in bn6f, write the comments, re-verify, commit (a child record run)."""
    review = given.attempts[-1].review
    message = (f"Document {given.subsystem}: symbols/db entries"
               + (f"; rename {len(given.renames)} symbols in bn6f" if given.renames else "")
               + f"\n\nEntries drafted by {given.attempts[-1].model}, evidence from convert run {current_run_id()}.\n\n"
               + (review.attribution if review else ""))
    run_id = f"{current_run_id()}-record"
    run_dir = given.run_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    report = run_record(RecordState(run_dir=run_dir, symbols=tuple(SymbolName(f) for f in given.functions),
                                    renames=given.renames,
                                    commit_message=message.strip() + "\n"), run_id)
    return RecordOutput(record=report, outcome=ConvertOutcome.RECORDED)


class GiveUpInput(Result):
    file_name: str
    contexts: tuple[ConversionContext, ...] | NotRun
    difftest: Path | BuildFailure
    without_calls: tuple[FunctionName, ...]
    attempts: tuple[Attempt, ...]


class GiveUpOutput(Result):
    outcome: ConvertOutcome


@workflow.step
def give_up(given: GiveUpInput) -> GiveUpOutput:
    """Take the file out of the tree again and say why there is nothing to commit."""
    if isinstance(given.contexts, NotRun):
        return GiveUpOutput(outcome=ConvertOutcome.NOTHING_TO_CONVERT)
    if given.without_calls and not given.attempts:
        return GiveUpOutput(outcome=ConvertOutcome.NO_CAPTURED_CALLS)
    uninstall_file(given.file_name)
    if isinstance(given.difftest, BuildFailure):
        return GiveUpOutput(outcome=ConvertOutcome.DIFFTEST_DID_NOT_BUILD)
    review = given.attempts[-1].review if given.attempts else None
    if review is not None and review.decision is ReviewDecision.REJECT:
        return GiveUpOutput(outcome=ConvertOutcome.REJECTED)
    return GiveUpOutput(outcome=ConvertOutcome.OUT_OF_ATTEMPTS)


# ── helpers ────────────────────────────────────────────────────────────

def _has_calls(snapshots: SnapshotDir, function: FunctionName) -> bool:
    return any(not path.name.endswith(".exit.bin") for path in (snapshots / function).glob("snap_*.bin"))


def _model_attempts(attempts: tuple[Attempt, ...]) -> int:
    return sum(1 for attempt in attempts if not attempt.given)


def _what_went_wrong(attempt: Attempt) -> tuple[str, str]:
    if attempt.review is not None:
        return "The reviewer asked for changes", attempt.review.comments
    if attempt.verification is not None:
        return ("The full verify failed (the quick test had passed)",
                attempt.verification.model_dump_json(indent=1, exclude={"sources"}))
    if attempt.test is not None:
        return "The quick test failed (your functions against the lifted originals)", attempt.test.report
    return "The reply could not be used", "\n".join(attempt.reply.problems) or (attempt.reply.error or "")


def _checks(context: ConversionContext, state: VerifyState) -> FunctionChecks:
    captured = state.captured_difftests if isinstance(state.captured_difftests, tuple) else ()
    oracle = state.oracle if isinstance(state.oracle, tuple) else ()
    return FunctionChecks(
        function=context.function, address=context.address,
        random=next((result for result in state.difftests if result.function == context.function), None),
        captured=next((result for result in captured if result.function == context.function), None),
        oracle=next((result for result in oracle if result.function == context.function), None),
        routes_identical=sum(1 for result in state.locksteps if result.passed), routes_checked=len(state.locksteps))


def _attempts_left(state: ConvertState) -> bool:
    return _model_attempts(state.attempts) < state.rounds * len(state.models)


def _latest(state: ConvertState) -> Attempt:
    return state.attempts[-1]


# ── decisions ──────────────────────────────────────────────────────────

def ready_to_convert(state: ConvertState) -> bool:
    return isinstance(state.contexts, tuple) and isinstance(state.difftest, Path) and not state.without_calls


def cannot_convert(state: ConvertState) -> bool:
    return not ready_to_convert(state)


def quick_test_passed(state: ConvertState) -> bool:
    test = _latest(state).test
    return test is not None and test.passed


def quick_test_failed_with_attempts_left(state: ConvertState) -> bool:
    return not quick_test_passed(state) and _attempts_left(state)


def quick_test_failed_with_no_attempts_left(state: ConvertState) -> bool:
    return not quick_test_passed(state) and not _attempts_left(state)


def verified(state: ConvertState) -> bool:
    verification = _latest(state).verification
    return verification is not None and verification.verdict in ACCEPTED_VERDICTS


def not_verified_with_attempts_left(state: ConvertState) -> bool:
    return not verified(state) and _attempts_left(state)


def not_verified_with_no_attempts_left(state: ConvertState) -> bool:
    return not verified(state) and not _attempts_left(state)


def approved(state: ConvertState) -> bool:
    review = _latest(state).review
    return review is not None and review.decision is ReviewDecision.APPROVE


def changes_requested_with_attempts_left(state: ConvertState) -> bool:
    review = _latest(state).review
    return review is not None and review.decision is ReviewDecision.REQUEST_CHANGES and _attempts_left(state)


def rejected_or_no_attempts_left(state: ConvertState) -> bool:
    return not approved(state) and not changes_requested_with_attempts_left(state)


# ── what the orchestrator reads at review ──────────────────────────────

REVIEW_CHECKLIST = (
    "struct offsets come from enums taken from bn6f include/structs (no `#ifndef` fallbacks)",
    "`ldrsh`/`ldrsb` loads use LDS16/LDS8 (a misaligned ldrsh is a sign-extended byte load)",
    "no `LR = ...` before a GUEST_CALL unless the original returns there (GUEST_CALL keeps LR)",
    "table dispatch enters each handler with the registers the original sets",
    "a second entry reached from a sibling takes the sibling's frame",
    "flags the contract lists as outputs are left as the original leaves them",
    "each narrowed promise (difftest.cfg) holds for every caller, not just one",
    "paths the captured calls never ran are named (a difftest_gen.c mutator can reach them)",
    "typed functions use no registers; every `register form` reason really rules C out",
)


def review_sheet(request: ReviewRequest) -> str:
    """The review request as a short text: history, then the latest file and everything about it."""
    latest = request.attempts[-1]
    lines = [f"# Review: {request.subsystem} -> cport/src/game/{request.file_name}", "", "## Attempts"]
    for attempt in request.attempts:
        test = "-" if attempt.test is None else ("passed" if attempt.test.passed else "failed")
        verdict = attempt.verification.verdict.value if attempt.verification else "-"
        tokens = f"{attempt.tokens.tokens_in}/{attempt.tokens.tokens_out} tokens" if attempt.tokens else ""
        review = f", review: {attempt.review.decision.value}" if attempt.review else ""
        lines.append(f"{attempt.number}. {attempt.model}: quick test {test}, verify {verdict}{review} {tokens}")
    if latest.verification is not None:
        report = latest.verification
        lines += ["", f"## Verify ({latest.verify_run}): {report.verdict.value}",
                  f"never compared: {', '.join(report.never_compared) or 'none'}; "
                  f"routes identical: {len(report.routes_checked) - len(report.failed_routes)}/{len(report.routes_checked)}"]
    if latest.test is not None:
        lines += ["", "## Quick test", *(f"{test.function}: {'ok' if test.passed else 'FAIL'}"
                                         f"{'' if test.compared else ' (NOT COMPARED)'}; captured: "
                                         f"{test.captured.splitlines()[0]}; random: {test.random.splitlines()[0]}"
                                         for test in latest.test.functions)]
        lines += ["", "## Register-world code outside the adapters",
                  *(latest.test.register_use_outside_adapters or ("none",)),
                  "", "## Register form kept (with the model's reason)", *(latest.test.register_form_notes or ("none",))]
    lines += ["", "## Narrowed promises (difftest.cfg)", *(latest.reply.cfg_lines or ("none",)), "", "## Drafted entries"]
    for draft in latest.reply.drafts:
        lines += [f"- {draft.function} -> {draft.proposed_name} ({draft.status.value}): {draft.summary}",
                  f"  interface: {draft.interface}"]
    lines += ["", "## Check while reading", *(f"- {item}" for item in REVIEW_CHECKLIST)]
    lines += ["", "## The file", "```c", latest.reply.file or "", "```",
              "", "Answer with `python -m wf answer RUN REVIEW.json`: "
              '{"decision": "approve" | "request changes" | "reject", "comments": "...", "attribution": "..."}']
    return "\n".join(lines)
