"""A strong outside model converts a subsystem (callers and callees together)
to readable C with C call and return semantics (docs/PROCESS.md §4): capture
the original's calls, ask the model for the whole file, check it keeps guest
registers to thin adapters (tools/abi/lint.py), quick-test it per function
against the lifted originals, install it and run the full verify,
then stop for the orchestrator's review. Failures and review comments go back
to the model (3 attempts per model, then the next model). A group of farm
candidates goes the same way, except that failing functions are taken out of
it (they stay in the farm's pool) instead of going to a model, and its file is an
inbox: each function goes to the file of its bn6f source (tools/layout.py), where
the verify builds and the commit puts it. A group of the farm's old-prompt candidates
(`--farm-model MODEL+old`) may keep register-form code: the register check is shown at
review but does not stop it, and its commit queues each function with register-level
code for retype (tools/abi/retype_queue.tsv) and raises the lint baseline by exactly
their hits (tools/wf/wf/tools/retype.py). Approved work is
committed, its symbols/db entries are written from the model's drafts plus the
run's own evidence, and a child record run renames and documents it.
The orchestrator never writes the code: it chooses the work and reviews."""
import re
import subprocess
from collections import Counter
from enum import Enum
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Graph, Transition, Workflow, current_run_id
from wf.graphs.capture import CaptureReport, CaptureState, run_capture
from wf.graphs.record import RecordReport, RecordState, run_record
from wf.graphs.verify import Verdict, VerifyReport, VerifyState, run_verify_in_full
from wf.routes import ALL_ROUTES
from wf.repo import REPO, SNAPSHOTS
from wf.tools.build import build_host_difftest, committed_tree, remove_tree
from wf.tools.drafts import candidate_from_farm, function_at_line
from wf.tools.evidence import FunctionChecks, Provenance, renames, write_entries
from wf.tools.git import commit_files
from wf.tools.locks import MAIN_TREE, held
from wf.tools.install import (DIFFTEST_CFG, game_file, install_file, lay_out_group, layout_conflicts, names_taken,
                              placed, restore_game_files, uninstall_file)
from wf.tools.models import ConversionContext, PromptKind, conversion_context, other_entries, prompt_hash, prompt_of
from wf.tools.retype import BASELINE, QUEUE, fill_install_commit, leftover_reads, queue_install
from wf.tools.registers import RegisterCheck, check_register_use
from wf.tools.sources import TIERS, Tier, first_up, is_source_failure
from wf.tools.subsystem import (FileReply, FileTest, TokenUse, ask_model_for_file, candidate_from_file, repair_prompt,
                                subsystem_prompt, test_candidate_file, tokens_for)
from wf.types import (BuildFailure, CommitHash, FunctionName, ModelSpec, NotRun, Rename, Result, Route, SnapshotDir,
                      SymbolName, ToolMessage)

PROJECT = "recomp-convert"
DEFAULT_MODELS = TIERS[Tier.STRONG]
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
            Transition(after=ask_model_for_file_step, go_to=check_registers),
            Transition(after=check_registers, go_to=quick_test, when=registers_clean),
            Transition(after=check_registers, go_to=ask_model_for_file_step, when=registers_used_with_attempts_left),
            Transition(after=check_registers, go_to=give_up, when=registers_used_with_no_attempts_left),
            Transition(after=quick_test, go_to=install_and_verify, when=quick_test_passed),
            Transition(after=quick_test, go_to=drop_failing_functions, when=farm_functions_to_drop),
            Transition(after=quick_test, go_to=ask_model_for_file_step, when=quick_test_failed_with_attempts_left),
            Transition(after=quick_test, go_to=give_up, when=quick_test_failed_with_no_attempts_left),
            Transition(after=install_and_verify, go_to=review_conversion, when=verified),
            Transition(after=install_and_verify, go_to=drop_failing_functions, when=farm_functions_to_drop),
            Transition(after=install_and_verify, go_to=ask_model_for_file_step, when=not_verified_with_attempts_left),
            Transition(after=install_and_verify, go_to=give_up, when=not_verified_with_no_attempts_left),
            Transition(after=review_conversion, go_to=commit_conversion, when=approved),
            Transition(after=review_conversion, go_to=drop_failing_functions, when=farm_functions_to_drop),
            Transition(after=review_conversion, go_to=ask_model_for_file_step, when=changes_requested_with_attempts_left),
            Transition(after=review_conversion, go_to=give_up, when=rejected_or_no_attempts_left),
            Transition(after=drop_failing_functions, go_to=quick_test),
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
    drop: tuple[FunctionName, ...] = ()   # a farm group: take these out, test and verify the rest again


class Attempt(Result):
    number: int
    model: ModelSpec          # who wrote the file
    given: bool = False       # a ready file (--candidate, --farm-model): not one of the models' attempts
    reply: FileReply
    tokens: TokenUse | None = None
    registers: RegisterCheck | None = None
    test: FileTest | None = None
    verification: VerifyReport | None = None
    verify_run: str | None = None
    review: Review | None = None
    laid_out: bool = False                    # a farm group's functions went to the laid-out files for the verify
    blamed: dict[FunctionName, str] = {}      # functions the install named: build errors in the laid-out files


class ConvertOutcome(Enum):
    RECORDED = "committed and recorded"
    REJECTED = "rejected at review"
    OUT_OF_ATTEMPTS = "every model ran out of attempts"
    NOTHING_TO_CONVERT = "nothing to convert (no lifted code, or the file exists)"
    NO_CAPTURED_CALLS = "some functions have no captured calls, so they could not be checked"
    COMMITTED_RECORD_PENDING = "committed with its entries; renames and bn6f comments wait for a batch record"
    DIFFTEST_DID_NOT_BUILD = "the host difftest did not build"
    SOURCES_OUT = "every model's provider was out of quota for longer than the run waits"
    REGISTER_FORM = "the given file keeps guest registers outside thin adapters (tools/abi/lint.py)"


class DroppedFunction(Result):
    function: FunctionName
    reason: str


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
    defer_record: bool = False                       # leave renames and bn6f comments to one batch record run
    tree: Path | None = None                         # the run's own worktree (installs, builds and checks happen there)
    capture: CaptureReport | NotRun | None = None
    contexts: tuple[ConversionContext, ...] | NotRun | None = None
    difftest: Path | BuildFailure | None = None
    without_calls: tuple[FunctionName, ...] = ()
    dropped: tuple[DroppedFunction, ...] = ()        # a farm group's functions taken out (they stay in the farm pool)
    attempts: tuple[Attempt, ...] = ()
    checks: tuple[FunctionChecks, ...] = ()
    commit: CommitHash | None = None
    retype_queued: tuple[FunctionName, ...] = ()     # old-prompt functions the commit queued for retype
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
                         snapshots=SnapshotDir(SNAPSHOTS), candidate_routes=given.routes)
    report = run_capture(start, f"{current_run_id()}-capture")
    return CaptureOutput(capture=report, snapshots=report.snapshots)


class GatherInput(Result):
    run_dir: Path
    file_name: str
    functions: tuple[FunctionName, ...]
    snapshots: SnapshotDir
    farm_model: ModelSpec | None


class GatherOutput(Result):
    contexts: tuple[ConversionContext, ...] | NotRun
    difftest: Path | BuildFailure
    without_calls: tuple[FunctionName, ...]
    functions: tuple[FunctionName, ...]
    tree: Path
    dropped: tuple[DroppedFunction, ...] = ()


@workflow.step
def gather_context(given: GatherInput) -> GatherOutput:
    """Each function's assembly, lifted C and register contract, and that it has captured calls (no model is
    asked to write what nothing could check); labels callers enter in the middle of a function join the
    subsystem (a replacement must define them); build the host difftest for quick tests."""
    tree = committed_tree(current_run_id())   # the run's own worktree: parallel runs never meet in one tree
    difftest = build_host_difftest(given.run_dir, tree=tree)
    without_calls = tuple(function for function in given.functions if not _has_calls(given.snapshots, function))
    if game_file(given.file_name).exists():
        return GatherOutput(contexts=NotRun(reason=ToolMessage(f"cport/src/game/{given.file_name} already exists")),
                            difftest=difftest, without_calls=without_calls, functions=given.functions,
                            tree=tree)
    found = {function: conversion_context(function) for function in given.functions}
    missing = [function for function, context in found.items() if context is None]
    if missing and given.farm_model is None:
        return GatherOutput(contexts=NotRun(reason=ToolMessage(f"no lifted code or assembly: {', '.join(missing)}")),
                            difftest=difftest, without_calls=without_calls, functions=given.functions,
                            tree=tree)
    contexts = tuple(context for context in found.values() if context is not None)
    entries = {context.function: other_entries(context) for context in contexts}
    if given.farm_model is None:
        with_entries = tuple(each for context in contexts for each in (context, *entries[context.function]))
        return GatherOutput(contexts=with_entries, difftest=difftest, without_calls=without_calls,
                            functions=given.functions, tree=tree)
    # a farm group leaves out what cannot go in rather than refusing the whole group
    dropped = ([DroppedFunction(function=function, reason="no lifted code or assembly") for function in missing]
               + [DroppedFunction(function=function, reason="no captured calls") for function in without_calls]
               + [DroppedFunction(function=context.function, reason="its code has other entries the farm did not write")
                  for context in contexts if entries[context.function] and context.function not in without_calls])
    out = {each.function for each in dropped}
    kept = tuple(context for context in contexts if context.function not in out)
    return GatherOutput(contexts=kept, difftest=difftest, without_calls=(), dropped=tuple(dropped),
                        functions=tuple(context.function for context in kept), tree=tree)


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
    """Ask the first model with rounds left whose provider is up (sources.first_up) for the whole file;
    after a failure or review, show it what went wrong."""
    number = len(given.attempts) + 1
    if not given.attempts and given.candidate is not None:
        attempt = Attempt(number=number, model=GIVEN_CANDIDATE, given=True, reply=candidate_from_file(given.candidate))
        return AskOutput(attempts=(attempt,))
    if not given.attempts and given.farm_model is not None:
        functions = tuple(context.function for context in given.contexts)
        reply = candidate_from_farm(functions, given.farm_model, given.subsystem, given.file_name,
                                    names_taken(functions))
        return AskOutput(attempts=(Attempt(number=number, model=given.farm_model, given=True, reply=reply),))
    models = _with_rounds_left(given.models, given.attempts, given.rounds)
    prompt = subsystem_prompt(given.subsystem, given.file_name, given.contexts)
    if given.attempts:
        what_failed, report = _what_went_wrong(given.attempts[-1])
        prompt = repair_prompt(prompt, what_failed, report, given.attempts[-1].reply)
    tag = f"{current_run_id()} round {number}"

    def ask(model: ModelSpec) -> FileReply:
        return ask_model_for_file(model, prompt, tag, given.run_dir / "prompts", wait_out_limits=len(models) == 1)

    def failure(reply: FileReply) -> str | None:
        return reply.error

    model, reply = first_up(models, ask, failure)
    return AskOutput(attempts=(*given.attempts, Attempt(number=number, model=model, reply=reply, tokens=tokens_for(tag))))


class RegistersInput(Result):
    file_name: str
    attempts: tuple[Attempt, ...]


class RegistersOutput(Result):
    attempts: tuple[Attempt, ...]


@workflow.step
def check_registers(given: RegistersInput) -> RegistersOutput:
    """Guest registers only in thin `Next` adapters, or where a `register form` note says why
    (tools/abi/lint.py on the candidate file, before any test)."""
    latest = given.attempts[-1]
    if latest.reply.file is None:
        return RegistersOutput(attempts=given.attempts)   # nothing to lint: the quick test reports the reply
    check = check_register_use(latest.reply.file, given.file_name)
    return RegistersOutput(attempts=(*given.attempts[:-1], latest.model_copy(update={"registers": check})))


class QuickTestInput(Result):
    contexts: tuple[ConversionContext, ...]
    snapshots: SnapshotDir
    difftest: Path
    attempts: tuple[Attempt, ...]
    file_name: str
    farm_model: ModelSpec | None
    tree: Path | None


class QuickTestOutput(Result):
    attempts: tuple[Attempt, ...]


@workflow.step
def quick_test(given: QuickTestInput) -> QuickTestOutput:
    """Each function of the file against its lifted original on the captured calls and random inputs; a
    farm group's functions must also fit the laid-out files they go to (tools/layout.py), and an old-prompt
    group's must not read registers a callee leaves behind (tools/abi/leftovers.py, which the gate runs)."""
    latest = given.attempts[-1]
    test = test_candidate_file(given.contexts, latest.reply, given.snapshots, given.difftest)
    if given.farm_model is not None and given.tree is not None and latest.reply.file is not None:
        restore_game_files(given.tree)    # an earlier attempt's install, laid out there
        functions = tuple(context.function for context in given.contexts)
        conflicts = layout_conflicts(given.file_name, latest.reply.file, functions, given.tree)
        if conflicts:
            lines = "\n".join(f"layout: {function}: {why}" for function, why in conflicts.items())
            test = test.model_copy(update={"passed": False, "report": ToolMessage(f"{test.report}\n{lines}")})
        if prompt_of(given.farm_model) is PromptKind.OLD:   # register form reads leftovers the gate refuses
            reads = leftover_reads(latest.reply.file, given.file_name)
            if reads:
                lines = "\n".join(f"leftovers: {function}: {why}" for function, why in reads.items())
                test = test.model_copy(update={"passed": False, "report": ToolMessage(f"{test.report}\n{lines}")})
    return QuickTestOutput(attempts=(*given.attempts[:-1], latest.model_copy(update={"test": test})))


class VerifyInput(Result):
    run_dir: Path
    file_name: str
    contexts: tuple[ConversionContext, ...]
    snapshots: SnapshotDir
    routes: tuple[Route, ...]
    battle_code: bool
    attempts: tuple[Attempt, ...]
    tree: Path | None
    farm_model: ModelSpec | None = None


class VerifyOutput(Result):
    attempts: tuple[Attempt, ...]
    checks: tuple[FunctionChecks, ...]


@workflow.step
def install_and_verify(given: VerifyInput) -> VerifyOutput:
    """Put the file in the tree (a farm group's functions into the laid-out files) and run every check
    (a child verify run); take it out again if it fails."""
    latest = given.attempts[-1]
    assert latest.reply.file is not None
    tree = given.tree or REPO
    group = given.farm_model is not None and tree != REPO
    if group:
        restore_game_files(tree)
    install_file(given.file_name, latest.reply.file, latest.reply.cfg_lines, tree)
    laid_out = lay_out_group(given.file_name, tree) if group else ()
    run_id = f"{current_run_id()}-verify-{latest.number}"
    run_dir = given.run_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    functions = tuple(context.function for context in given.contexts if context.entry_of is None)
    state = run_verify_in_full(VerifyState(run_dir=run_dir, functions=functions, snapshots=given.snapshots,
                                           routes=given.routes, battle_code=given.battle_code, tree=tree), run_id)
    assert state.report is not None
    blamed = _blamed_by_build(state.report, laid_out, functions) if group else {}
    if state.report.verdict not in ACCEPTED_VERDICTS:
        uninstall_file(given.file_name, tree)
        if group:
            restore_game_files(tree)
    checks = tuple(_checks(context, state) for context in given.contexts)
    updated = latest.model_copy(update={"verification": state.report, "verify_run": run_id, "laid_out": group,
                                        "blamed": blamed})
    return VerifyOutput(attempts=(*given.attempts[:-1], updated), checks=checks)


def _blamed_by_build(report: VerifyReport, laid_out: tuple[Path, ...],
                     functions: tuple[FunctionName, ...]) -> dict[FunctionName, str]:
    """The group's functions whose parts of the laid-out files a build error points into."""
    blamed: dict[FunctionName, str] = {}
    texts = {path.name: path.read_text() for path in laid_out if path.exists()}
    for line in report.build_failure.last_lines if report.build_failure is not None else ():
        where = re.search(r"src/game/([\w.]+):(\d+):", line)
        owner = function_at_line(texts[where[1]], int(where[2])) if where and where[1] in texts else None
        if owner is not None and owner in functions:
            _ = blamed.setdefault(owner, f"the full build failed in its code: {line.strip()}"[:300])
    return blamed


class ReviewRequest(Result):
    """What the orchestrator is shown: the file, its promises and drafts, the evidence, the attempts."""
    subsystem: str
    file_name: str
    attempts: tuple[Attempt, ...]
    dropped: tuple[DroppedFunction, ...] = ()


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
    farm_model: ModelSpec | None = None


class CommitOutput(Result):
    commit: CommitHash
    retype_queued: tuple[FunctionName, ...] = ()


@workflow.step
def commit_conversion(given: CommitInput) -> CommitOutput:
    """Put the file and its difftest.cfg lines into the main tree and commit them there (locally, never
    pushed), holding the main tree's lock: parallel runs share difftest.cfg. A farm group's functions go
    to the laid-out files, as in its verify; an old-prompt group's register-level code is queued for retype
    and added to the lint baseline in the same commit (the gate allows that growth and no other)."""
    latest = given.attempts[-1]
    assert latest.review is not None and latest.verification is not None
    assert latest.reply.file is not None
    queued: tuple[FunctionName, ...] = ()
    with held(MAIN_TREE):
        install_file(given.file_name, latest.reply.file, latest.reply.cfg_lines)
        if given.farm_model is None:
            files: tuple[Path, ...] = (game_file(given.file_name),)
        else:
            try:
                files = lay_out_group(given.file_name)
            except Exception:
                uninstall_file(given.file_name)
                raise
            if prompt_of(given.farm_model) is PromptKind.OLD:
                try:
                    rows = queue_install(given.functions, latest.reply.file, files, given.farm_model,
                                         prompt_hash(PromptKind.OLD), current_run_id())
                except Exception:
                    _restore_main_tree((*files, BASELINE, QUEUE))
                    uninstall_file(given.file_name)
                    raise
                queued = tuple(row.function for row in rows)
                files = (*files, BASELINE, QUEUE) if rows else files
        where = (f"; laid out into {', '.join(path.name for path in files)} (tools/layout.py)"
                 if given.farm_model is not None else "")
        message = (f"Readable {given.subsystem} ({len(given.functions)} functions, written by {latest.model})\n\n"
                   f"{', '.join(given.functions)}.\nWritten by {latest.model} through the convert workflow "
                   f"(run {current_run_id()}, attempt {latest.number}){where}; quick test and full verify passed "
                   f"(verify run {latest.verify_run}: {latest.verification.verdict.value}); reviewed and approved"
                   + (f": {latest.review.comments}" if latest.review.comments else "") + ".\n\n"
                   + (f"Register form from the farm's old prompt ({prompt_hash(PromptKind.OLD)}): {len(queued)} of them "
                      f"queued for retype in tools/abi/retype_queue.tsv ({', '.join(queued)}); the lint baseline grows "
                      "by their hits only.\n\n" if queued else "")
                   + latest.review.attribution)
        return CommitOutput(commit=commit_files((*files, DIFFTEST_CFG), message.strip() + "\n"), retype_queued=queued)


def _restore_main_tree(paths: tuple[Path, ...]) -> None:
    """The main checkout's files back to the last commit (a failed old-prompt queueing left them half-written)."""
    _ = subprocess.run(["git", "-C", str(REPO), "checkout", "HEAD", "--", *map(str, paths)], check=False,
                       capture_output=True)


class EntriesInput(Result):
    subsystem: str
    file_name: str
    attempts: tuple[Attempt, ...]
    checks: tuple[FunctionChecks, ...]
    commit: CommitHash
    farm_model: ModelSpec | None = None      # a farm group: its entries cite the run, not the group
    retype_queued: tuple[FunctionName, ...] = ()


class EntriesOutput(Result):
    entries: tuple[Path, ...]
    renames: tuple[Rename, ...]


@workflow.step
def write_symbol_entries(given: EntriesInput) -> EntriesOutput:
    """symbols/db entries from the model's drafts plus this run's evidence; renames for confirmed/probable meanings."""
    latest = given.attempts[-1]
    assert latest.verify_run is not None
    functions = tuple(check.function for check in given.checks)
    provenance = Provenance(subsystem=given.subsystem, file=game_file(given.file_name), model=latest.model,
                            attempts=latest.number, convert_run=current_run_id(), verify_run=latest.verify_run,
                            commit=given.commit, farm_group=given.farm_model is not None,
                            placed=placed(functions) if given.farm_model is not None else {},
                            retype_queued=given.retype_queued, prompt_hash=prompt_hash(PromptKind.OLD))
    return EntriesOutput(entries=write_entries(latest.reply.drafts, given.checks, provenance),
                         renames=renames(latest.reply.drafts, given.checks))


class RecordInput(Result):
    run_dir: Path
    subsystem: str
    functions: tuple[FunctionName, ...]
    renames: tuple[Rename, ...]
    entries: tuple[Path, ...]
    attempts: tuple[Attempt, ...]
    defer_record: bool
    tree: Path | None
    commit: CommitHash | None = None
    retype_queued: tuple[FunctionName, ...] = ()


class RecordOutput(Result):
    record: RecordReport | None
    outcome: ConvertOutcome


@workflow.step
def record_symbols(given: RecordInput) -> RecordOutput:
    """Check the entries, rename in bn6f, write the comments, re-verify, commit (a child record run); or, when
    runs go in parallel, commit the entries and leave the renames to one batch record (PENDING_RENAMES). The
    retype queue's rows of this install get its commit (a commit cannot name itself)."""
    if given.tree is not None:
        remove_tree(given.tree)
    review = given.attempts[-1].review
    if given.defer_record:
        message = (f"Document {given.subsystem}: symbols/db entries (renames wait for a batch record)\n\n"
                   f"Entries drafted by {given.attempts[-1].model}, evidence from convert run {current_run_id()}.\n\n"
                   + (review.attribution if review else ""))
        with held(MAIN_TREE):
            with open(PENDING_RENAMES, "a") as pending:
                _ = pending.write("".join(f"{rename.old} {rename.new}\n" for rename in given.renames))
            filled = given.commit is not None and fill_install_commit(given.retype_queued, given.commit)
            _ = commit_files((*given.entries, PENDING_RENAMES, *((QUEUE,) if filled else ())), message.strip() + "\n")
        return RecordOutput(record=None, outcome=ConvertOutcome.COMMITTED_RECORD_PENDING)
    if given.commit is not None and given.retype_queued:
        with held(MAIN_TREE):
            if fill_install_commit(given.retype_queued, given.commit):
                _ = commit_files((QUEUE,), f"Retype queue: install commit {given.commit[:10]} of "
                                           f"{len(given.retype_queued)} functions\n\n"
                                           + (review.attribution if review else ""))
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


class DropInput(Result):
    subsystem: str
    file_name: str
    contexts: tuple[ConversionContext, ...]
    farm_model: ModelSpec | None
    attempts: tuple[Attempt, ...]
    dropped: tuple[DroppedFunction, ...]


class DropOutput(Result):
    contexts: tuple[ConversionContext, ...]
    functions: tuple[FunctionName, ...]
    attempts: tuple[Attempt, ...]
    dropped: tuple[DroppedFunction, ...]


@workflow.step
def drop_failing_functions(given: DropInput) -> DropOutput:
    """A farm group keeps what passes: the functions the quick test, the verify or the review named
    are taken out (they stay in the farm's pool), and the rest is assembled again and re-tested."""
    assert given.farm_model is not None
    failing = failing_functions(given.attempts[-1])
    kept = tuple(context for context in given.contexts if context.function not in failing)
    functions = tuple(context.function for context in kept)
    reply = candidate_from_farm(functions, given.farm_model, given.subsystem, given.file_name, names_taken(functions))
    attempt = Attempt(number=len(given.attempts) + 1, model=given.farm_model, given=True, reply=reply)
    return DropOutput(contexts=kept, functions=functions, attempts=(*given.attempts, attempt),
                      dropped=(*given.dropped, *(DroppedFunction(function=function, reason=reason)
                                                 for function, reason in failing.items())))


PENDING_RENAMES = REPO / "symbols/pending_renames.txt"   # OLD NEW per line, for the next batch record


class GiveUpInput(Result):
    file_name: str
    contexts: tuple[ConversionContext, ...] | NotRun
    difftest: Path | BuildFailure
    without_calls: tuple[FunctionName, ...]
    attempts: tuple[Attempt, ...]
    tree: Path | None


class GiveUpOutput(Result):
    outcome: ConvertOutcome


@workflow.step
def give_up(given: GiveUpInput) -> GiveUpOutput:
    """Take the file out of the tree again and say why there is nothing to commit."""
    if isinstance(given.contexts, NotRun):
        return GiveUpOutput(outcome=ConvertOutcome.NOTHING_TO_CONVERT)
    if given.without_calls and not given.attempts:
        return GiveUpOutput(outcome=ConvertOutcome.NO_CAPTURED_CALLS)
    if given.tree is not None:
        remove_tree(given.tree)   # the file was only ever installed there
    else:
        uninstall_file(given.file_name)
    if isinstance(given.difftest, BuildFailure):
        return GiveUpOutput(outcome=ConvertOutcome.DIFFTEST_DID_NOT_BUILD)
    review = given.attempts[-1].review if given.attempts else None
    if review is not None and review.decision is ReviewDecision.REJECT:
        return GiveUpOutput(outcome=ConvertOutcome.REJECTED)
    if given.attempts and _sources_out(given.attempts[-1]):
        return GiveUpOutput(outcome=ConvertOutcome.SOURCES_OUT)
    registers = given.attempts[-1].registers if given.attempts else None
    if given.attempts and given.attempts[-1].given and registers is not None and not registers.clean:
        return GiveUpOutput(outcome=ConvertOutcome.REGISTER_FORM)
    return GiveUpOutput(outcome=ConvertOutcome.OUT_OF_ATTEMPTS)


# ── helpers ────────────────────────────────────────────────────────────

def _has_calls(snapshots: SnapshotDir, function: FunctionName) -> bool:
    return any(not path.name.endswith(".exit.bin") for path in (snapshots / function).glob("snap_*.bin"))


def _with_rounds_left(models: tuple[ModelSpec, ...], attempts: tuple[Attempt, ...],
                      rounds: int) -> tuple[ModelSpec, ...]:
    """The models (in order) that have had fewer than `rounds` attempts: each model gets its rounds,
    whichever order the providers' quotas let them come in."""
    used = Counter(attempt.model for attempt in attempts if not attempt.given)
    return tuple(model for model in models if used[model] < rounds)


def _sources_out(attempt: Attempt) -> bool:
    """The attempt failed because every provider was out (first_up waited as long as it may)."""
    return attempt.reply.error is not None and is_source_failure(attempt.reply.error)


def _what_went_wrong(attempt: Attempt) -> tuple[str, str]:
    if attempt.review is not None:
        return "The reviewer asked for changes", attempt.review.comments
    if attempt.verification is not None:
        return ("The full verify failed (the quick test had passed)",
                attempt.verification.model_dump_json(indent=1, exclude={"sources"}))
    if attempt.test is not None:
        return "The quick test failed (your functions against the lifted originals)", attempt.test.report
    if attempt.registers is not None and not attempt.registers.clean:
        return "The register check failed (before any test)", attempt.registers.report
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
    if state.farm_model is not None:
        return False   # a farm group drops failing functions; its file is too big for a model to repair
    if state.attempts and _sources_out(state.attempts[-1]):
        return False
    return bool(_with_rounds_left(state.models, state.attempts, state.rounds))


_SECTION_START = re.compile(r"^/\* ── \w+ ── \*/$")


def failing_functions(attempt: Attempt) -> dict[FunctionName, str]:
    """The functions of a farm group the latest test, verify or review named, with why."""
    failing: dict[FunctionName, str] = {}
    code = attempt.reply.file or ""
    if attempt.review is not None:
        for function in attempt.review.drop:
            failing[function] = f"taken out at review: {attempt.review.comments}"[:300]
    if attempt.verification is not None:
        for function in attempt.verification.failed_functions:
            failing[function] = "difftest found a difference in the full verify"
        for function in attempt.verification.oracle_differences:
            failing[function] = "the CPU oracle found a difference in the full verify"
        failing.update(attempt.blamed)
        build = attempt.verification.build_failure
        for line in build.last_lines if build is not None and not attempt.laid_out else ():
            where = re.search(r"src/game/[\w.]+:(\d+):", line)
            owner = function_at_line(code, int(where[1])) if where else None
            if owner is not None:
                failing[owner] = f"the full build failed in its code: {line.strip()}"[:300]
        routes = attempt.verification.failed_routes
        if routes and not failing:
            # the game differs on a route and no check names a function: set the later half aside (it goes
            # back to the pool and halves again in a later group) and verify the rest
            parts = [owner for owner in (function_at_line(code, number)
                                         for number, line in enumerate(code.splitlines(), start=1)
                                         if _SECTION_START.match(line)) if owner is not None]
            for function in parts[len(parts) // 2:]:
                failing[function] = (f"set aside: lockstep differed on {', '.join(routes)} and no check named a "
                                     "function; bisecting")
    if attempt.test is not None and not attempt.test.passed:
        for test in attempt.test.functions:
            if not test.passed:
                failing[test.function] = f"quick test: {test.captured.splitlines()[0]}; {test.random.splitlines()[0]}"[:300]
        for line in attempt.test.report.splitlines():
            where = re.match(r"line (\d+)", line)
            owner = function_at_line(code, int(where[1])) if where else None
            if owner is not None:
                failing[owner] = f"does not compile with the others: {line.strip()}"[:300]
            # a definition twice, or of a function outside the group: the candidate whose part holds the
            # stray definition goes (not the function it defines, whose own part may be fine)
            named = re.match(r"`Next (\w+)\(void\)` must be defined exactly once", line)
            outside = re.match(r"defines functions outside the subsystem: (.*)", line)
            for defined in ([named[1]] if named else []) + (outside[1].split(", ") if outside else []):
                for owner in _parts_defining(code, defined):
                    if owner != defined:
                        failing[owner] = f"its part also defines `Next {defined}(void)`"
            laid = re.match(r"layout: (\w+): (.*)", line)
            if laid:
                failing[FunctionName(laid[1])] = f"does not fit the laid-out files: {laid[2]}"[:300]
            leftover = re.match(r"leftovers: (\w+): (.*)", line)
            if leftover:
                failing[FunctionName(leftover[1])] = f"leftover read: {leftover[2]}"[:300]
            undrafted = re.match(r"no ```yaml entry for: (.*)", line)
            for function in undrafted[1].split(", ") if undrafted else ():
                failing[FunctionName(function)] = "no drafted symbols/db entry yet"
    return failing


def _parts_defining(code: str, name: str) -> list[FunctionName]:
    """Whose parts of an assembled farm file define `Next NAME(void)`."""
    owners: list[FunctionName] = []
    for match in re.finditer(rf"^[ \t]*(Next)\s+{re.escape(name)}\s*\(\s*void\s*\)\s*\{{", code, re.M):
        owner = function_at_line(code, code.count("\n", 0, match.start(1)) + 1)   # the brace may be a line down
        if owner is not None:
            owners.append(owner)
    return owners


def _contexts(state: ConvertState) -> tuple[ConversionContext, ...]:
    return state.contexts if isinstance(state.contexts, tuple) else ()


def _latest(state: ConvertState) -> Attempt:
    return state.attempts[-1]


# ── decisions ──────────────────────────────────────────────────────────

def ready_to_convert(state: ConvertState) -> bool:
    return isinstance(state.contexts, tuple) and isinstance(state.difftest, Path) and not state.without_calls


def cannot_convert(state: ConvertState) -> bool:
    return not ready_to_convert(state)


def registers_clean(state: ConvertState) -> bool:
    """No register-level code outside adapters, or a group of the farm's old-prompt candidates (register form
    allowed: its install queues them for retype)."""
    check = _latest(state).registers
    return check is None or check.clean or _register_form_allowed(state)


def _register_form_allowed(state: ConvertState) -> bool:
    return (state.farm_model is not None and prompt_of(state.farm_model) is PromptKind.OLD
            and _latest(state).given)


def registers_used_with_attempts_left(state: ConvertState) -> bool:
    return not registers_clean(state) and _attempts_left(state)


def registers_used_with_no_attempts_left(state: ConvertState) -> bool:
    return not registers_clean(state) and not _attempts_left(state)


def quick_test_passed(state: ConvertState) -> bool:
    test = _latest(state).test
    return test is not None and test.passed


def farm_functions_to_drop(state: ConvertState) -> bool:
    """A farm group whose latest stage failed (or was sent back at review) and names functions to take
    out, with some left to keep."""
    latest = _latest(state)
    if state.farm_model is None or not latest.given:
        return False
    stage_failed = (not quick_test_passed(state) if latest.verification is None and latest.review is None
                    else not verified(state) if latest.review is None
                    else latest.review.decision is ReviewDecision.REQUEST_CHANGES)
    failing = failing_functions(latest)
    return stage_failed and bool(failing) and any(context.function not in failing for context in _contexts(state))


def quick_test_failed_with_attempts_left(state: ConvertState) -> bool:
    return not quick_test_passed(state) and not farm_functions_to_drop(state) and _attempts_left(state)


def quick_test_failed_with_no_attempts_left(state: ConvertState) -> bool:
    return not quick_test_passed(state) and not farm_functions_to_drop(state) and not _attempts_left(state)


def verified(state: ConvertState) -> bool:
    verification = _latest(state).verification
    return verification is not None and verification.verdict in ACCEPTED_VERDICTS


def not_verified_with_attempts_left(state: ConvertState) -> bool:
    return not verified(state) and not farm_functions_to_drop(state) and _attempts_left(state)


def not_verified_with_no_attempts_left(state: ConvertState) -> bool:
    return not verified(state) and not farm_functions_to_drop(state) and not _attempts_left(state)


def approved(state: ConvertState) -> bool:
    review = _latest(state).review
    return review is not None and review.decision is ReviewDecision.APPROVE


def changes_requested_with_attempts_left(state: ConvertState) -> bool:
    review = _latest(state).review
    return (review is not None and review.decision is ReviewDecision.REQUEST_CHANGES
            and not farm_functions_to_drop(state) and _attempts_left(state))


def rejected_or_no_attempts_left(state: ConvertState) -> bool:
    return not approved(state) and not farm_functions_to_drop(state) and not changes_requested_with_attempts_left(state)


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
    if request.dropped:
        lines += ["", f"## Taken out of this farm group ({len(request.dropped)}; they stay in the farm pool)",
                  *(f"- {each.function}: {each.reason}" for each in request.dropped)]
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
              '{"decision": "approve" | "request changes" | "reject", "comments": "...", "attribution": "...", '
              '"drop": [farm groups: functions to take out]}']
    return "\n".join(lines)
