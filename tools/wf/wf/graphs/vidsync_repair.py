"""A new repair handler for a loss the vidsync driver could not get past (docs/vidsync/README.md). The loss is
rendered with the probes (the emulator beside the video, and the video after it) on the driver PC; Claude
looks and says what differs and which symbols and routines matter; an outside model writes the handler from a
brief carrying the symbols/db entries and the routines' assembly; Claude reviews it; it is tested from the
loss checkpoint (it must sync 600 frames past the loss on the driver PC's own workers, and the path must then
hold against the video). What was learnt about the game goes into symbols/db and bn6f through `record`; the
handler is committed, deployed, and the driver restarts past the loss."""
import contextvars
from enum import Enum
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.graphs.record import RecordReport, RecordState, RecordVerdict, run_record
from wf.tools.git import commit_files
from wf.tools.locks import MAIN_TREE, held
from wf.tools.sources import TIERS, Tier, first_up
from wf.tools.vidsync import (DRIVER_PC, PCS, HandlerTest, LossCase, PcDeploy, deploy, find_loss, newest_checkpoint,
                              next_run_name, render_loss, restart_after_test, restart_workers, run_names,
                              start_driver, test_handler)
from wf.tools.vidsync_brief import HANDLERS, Brief, HandlerCandidate, LossClass, ask_model_for_handler, write_brief
from wf.types import CommitHash, FrameCount, LogPath, ModelSpec, Rename, Result, RunName, SymbolName

PROJECT = "recomp-vidsync"


def vidsync_repair_graph() -> Graph["VidsyncRepairState"]:
    return workflow.graph(
        first=find_the_loss,
        ends=[write_report],
        transitions=[
            Transition(after=find_the_loss, go_to=render_the_loss, when=loss_found),
            Transition(after=find_the_loss, go_to=write_report, when=no_loss_found),
            Transition(after=render_the_loss, go_to=classify_the_loss),
            Transition(after=classify_the_loss, go_to=brief_the_model, when=handler_wanted),
            Transition(after=classify_the_loss, go_to=write_report, when=no_handler_wanted),
            Transition(after=brief_the_model, go_to=ask_model_for_a_handler),
            Transition(after=ask_model_for_a_handler, go_to=review_the_handler, when=candidate_written),
            Transition(after=ask_model_for_a_handler, go_to=brief_the_model, when=no_candidate_but_attempts_left),
            Transition(after=ask_model_for_a_handler, go_to=write_report, when=no_candidate_and_no_attempts_left),
            Transition(after=review_the_handler, go_to=test_from_the_loss, when=approved),
            Transition(after=review_the_handler, go_to=brief_the_model, when=changes_requested),
            Transition(after=review_the_handler, go_to=write_report, when=rejected_or_out_of_attempts),
            Transition(after=test_from_the_loss, go_to=record_findings, when=test_passed),
            Transition(after=test_from_the_loss, go_to=brief_the_model, when=test_failed_attempts_left),
            Transition(after=test_from_the_loss, go_to=write_report, when=test_failed_no_attempts_left),
            Transition(after=record_findings, go_to=record_symbols, when=findings_to_record),
            Transition(after=record_findings, go_to=commit_the_handler, when=nothing_to_record),
            Transition(after=record_symbols, go_to=commit_the_handler, when=recorded),
            Transition(after=record_symbols, go_to=write_report, when=record_failed),
            Transition(after=commit_the_handler, go_to=resume_the_driver),
            Transition(after=resume_the_driver, go_to=write_report),
        ],
    )


def run_vidsync_repair(start: "VidsyncRepairState", run_id: str, tracked: bool = True) -> Finished["VidsyncRepairState"]:
    outcome = vidsync_repair_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished):
        raise RuntimeError(f"vidsync-repair {run_id} stopped at {outcome.step}")
    return outcome


# ── the run's state ────────────────────────────────────────────────────

class ReviewVerdict(Enum):
    APPROVE = "approve"                   # test it
    REQUEST_CHANGES = "request changes"   # back to a model, with the notes
    REJECT = "reject"                     # stop here


class HandlerReview(Result):
    """Claude's review of a handler: reads it against the brief (does it write only what the loss needs, from
    syms names; does it judge candidates by syncing on; can it loop or take hours)."""
    verdict: ReviewVerdict
    notes: str = ""


class Findings(Result):
    """What the handler's test taught about the game, and the commit messages (in full, attribution lines
    included). Claude writes or updates the symbols/db entries before answering; they go through `record`."""
    symbols: tuple[SymbolName, ...] = ()
    renames: tuple[Rename, ...] = ()
    record_message: str = ""
    handler_message: str


class RepairVerdict(Enum):
    NO_LOSS = "no loss found for the run"
    NO_HANDLER = "Claude chose not to ask for a handler"
    GAVE_UP = "no handler held: out of attempts, or rejected"
    RECORD_FAILED = "the handler held, but recording the findings failed; nothing committed"
    REPAIRED = "the handler held; committed, deployed, and the driver restarted past the loss"


class RepairReport(Result):
    verdict: RepairVerdict
    case: LossCase | None
    handler: HandlerCandidate | None
    test: HandlerTest | None
    recorded: RecordReport | None
    commit: CommitHash | None
    restarted_as: RunName | None


class VidsyncRepairState(BaseModel):
    run_dir: Path
    run: RunName
    loss: FrameCount | None = None                 # None: the run's newest loss
    models: tuple[ModelSpec, ...] = TIERS[Tier.STRONG]
    rounds: int = 2                                # attempts per model before the next
    max_attempts: int = 4                          # handlers written in all
    case: LossCase | None = None
    pictures: tuple[Path, ...] = ()
    loss_class: LossClass | None = None
    brief: Path | None = None
    feedback: tuple[str, ...] = ()                 # review notes and test results for the next attempt
    attempts: int = 0
    handler: HandlerCandidate | None = None
    review: HandlerReview | None = None
    test: HandlerTest | None = None
    findings: Findings | None = None
    recorded: RecordReport | None = None
    commit: CommitHash | None = None
    deployed: tuple[PcDeploy, ...] = ()
    restarted_as: RunName | None = None
    report: RepairReport | None = None


workflow = Workflow[VidsyncRepairState](VidsyncRepairState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class FindInput(Result):
    run: RunName
    loss: FrameCount | None


class FindOutput(Result):
    case: LossCase | None


@workflow.step
def find_the_loss(given: FindInput) -> FindOutput:
    """The run's loss on the driver PC (remote/loss.py): its loss json, a checkpoint before it, the log."""
    case = find_loss(given.run, given.loss)
    return FindOutput(case=case if case is not None and case.checkpoint is not None else None)


class RenderInput(Result):
    case: LossCase
    run_dir: Path


class RenderOutput(Result):
    pictures: tuple[Path, ...]


@workflow.step
def render_the_loss(given: RenderInput) -> RenderOutput:
    """compare.png (the emulator beside the video, the 150 frames before the loss) and video.png (the video's 8
    seconds after it), made on the driver PC with the probes and copied here."""
    return RenderOutput(pictures=render_loss(given.case, given.run_dir))


class ClassifyRequest(Result):
    case: LossCase
    pictures: tuple[Path, ...]


class ClassifyOutput(Result):
    loss_class: LossClass


@workflow.pause_point
def classify_the_loss(given: ClassifyRequest, answer: LossClass) -> ClassifyOutput:
    """Claude looks at the pictures and the log and says what differs, which symbols/db entries and routines
    the model needs, and the handler's name (or that no new handler is wanted)."""
    return ClassifyOutput(loss_class=answer)


class BriefInput(Result):
    case: LossCase
    loss_class: LossClass
    feedback: tuple[str, ...]
    handler: HandlerCandidate | None
    run_dir: Path
    attempts: int


class BriefOutput(Result):
    brief: Path


@workflow.step
def brief_the_model(given: BriefInput) -> BriefOutput:
    """The brief: the loss, what differs, the handler interface, autosync's helpers and the closest built-in
    handlers, syms names, the symbols/db entries, the routines' assembly, and what earlier attempts were told."""
    previous = given.handler.code if given.handler is not None else None
    text = write_brief(given.case, given.loss_class, given.feedback, previous)
    path = given.run_dir / f"brief-{given.attempts + 1}.md"
    _ = path.write_text(text)
    return BriefOutput(brief=path)


class AskInput(Result):
    brief: Path
    loss_class: LossClass
    models: tuple[ModelSpec, ...]
    rounds: int
    attempts: int
    feedback: tuple[str, ...]
    run_dir: Path


class AskOutput(Result):
    handler: HandlerCandidate
    attempts: int
    feedback: tuple[str, ...]


@workflow.step
def ask_model_for_a_handler(given: AskInput) -> AskOutput:
    """An outside model writes the handler (the strong tier in order, `rounds` attempts each; a provider that is
    down is skipped). A reply that cannot be tested (no code, does not compile, no NAME/try_fix) counts as an
    attempt and goes back with what was wrong."""
    first = min(given.attempts // max(given.rounds, 1), len(given.models) - 1)
    brief = Brief(given.brief.read_text())
    tag = f"{current_run_id()}-{given.attempts + 1}"
    name = given.loss_class.handler_name

    def ask(model: ModelSpec) -> HandlerCandidate:
        return contextvars.copy_context().run(ask_model_for_handler, model, brief, name, tag, given.run_dir)

    def failure(candidate: HandlerCandidate) -> str | None:
        return candidate.problems[0] if candidate.code is None and candidate.problems else None

    _, candidate = first_up(given.models[first:], ask, failure)
    feedback = given.feedback + tuple(f"attempt {given.attempts + 1} ({candidate.model}): {problem}"
                                      for problem in candidate.problems)
    return AskOutput(handler=candidate, attempts=given.attempts + 1, feedback=feedback)


class HandlerReviewRequest(Result):
    handler: HandlerCandidate
    brief: Path
    case: LossCase


class ReviewOutput(Result):
    review: HandlerReview
    feedback: tuple[str, ...]


@workflow.pause_point
def review_the_handler(given: HandlerReviewRequest, answer: HandlerReview) -> ReviewOutput:
    """Claude reviews the handler (its code is in handler.code, the brief beside it): approve to test it,
    request changes (the notes go to the next attempt), or reject."""
    notes = (f"review of attempt by {given.handler.model}: {answer.notes}",) if answer.notes else ()
    return ReviewOutput(review=answer, feedback=notes)


class TestInput(Result):
    case: LossCase
    handler: HandlerCandidate
    attempts: int
    feedback: tuple[str, ...]
    run_dir: Path


class TestOutput(Result):
    test: HandlerTest
    feedback: tuple[str, ...]
    restarted_as: RunName | None = None


@workflow.step
def test_from_the_loss(given: TestInput) -> TestOutput:
    """Put the handler in tools/vidsync/handlers, deploy to the driver PC, and sync from the loss checkpoint
    with only that handler to 600 frames past the loss, on that PC's own workers; then pathcheck the path. A
    handler that fails is taken out of the tree again, and the driver the test stopped starts again (the next
    run from its newest checkpoint): a failed test never leaves the sync stopped waiting for a person."""
    assert given.handler.code is not None
    path = HANDLERS / f"{given.handler.name}.py"
    _ = path.write_text(given.handler.code)
    deployed = deploy((DRIVER_PC,), LogPath(given.run_dir / f"deploy-test-{given.attempts}.log"))
    if not all(pc.ok for pc in deployed):
        path.unlink()
        raise RuntimeError(f"deploy to the driver PC failed: {deployed[0].output}")
    test = test_handler(given.case, given.handler.name, given.attempts)
    if not test.passed:
        path.unlink()
        restarted = restart_after_test(given.case, test)
        summary = (f"test of attempt {given.attempts}: reached {test.reached} (needed {test.needed}), fixes "
                   f"{test.fixes}, pathcheck {'holds' if test.pathcheck_holds else 'does not hold'}; log: "
                   + " | ".join(test.log_tail[-8:]))
        return TestOutput(test=test, feedback=given.feedback + (summary,), restarted_as=restarted)
    return TestOutput(test=test, feedback=given.feedback)


class FindingsRequest(Result):
    case: LossCase
    handler: HandlerCandidate
    test: HandlerTest
    loss_class: LossClass


class FindingsOutput(Result):
    findings: Findings


@workflow.pause_point
def record_findings(given: FindingsRequest, answer: Findings) -> FindingsOutput:
    """Claude writes or updates the symbols/db entries for what the handler relies on (evidence: the fix's
    poke line and pathcheck on the test result; PROCESS.md §6), then answers with them and the commit
    messages."""
    return FindingsOutput(findings=answer)


class RecordInput(Result):
    findings: Findings
    run_dir: Path


class RecordOutput(Result):
    recorded: RecordReport


@workflow.step
def record_symbols(given: RecordInput) -> RecordOutput:
    """The findings through `record` (a child run): symbols/db checked, comments and renames into bn6f, commit."""
    run_id = f"{current_run_id()}-record"
    run_dir = given.run_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    start = RecordState(run_dir=run_dir, symbols=given.findings.symbols, renames=given.findings.renames,
                        commit_message=given.findings.record_message)
    return RecordOutput(recorded=run_record(start, run_id))


class CommitInput(Result):
    handler: HandlerCandidate
    findings: Findings


class CommitOutput(Result):
    commit: CommitHash


@workflow.step
def commit_the_handler(given: CommitInput) -> CommitOutput:
    """Commit tools/vidsync/handlers/NAME.py in the main checkout (holding the main tree's lock)."""
    with held(MAIN_TREE):
        return CommitOutput(commit=commit_files((HANDLERS / f"{given.handler.name}.py",), given.findings.handler_message))


class ResumeInput(Result):
    case: LossCase
    test: HandlerTest
    run_dir: Path


class ResumeOutput(Result):
    deployed: tuple[PcDeploy, ...]
    restarted_as: RunName | None


@workflow.step
def resume_the_driver(given: ResumeInput) -> ResumeOutput:
    """Deploy the committed handler to both PCs, restart the worker server, and start the driver as the next
    run from the test's newest checkpoint past the loss (watch it with `wf vidsync-run --watch`)."""
    deployed = deploy(PCS, LogPath(given.run_dir / "deploy.log"))
    checkpoint = newest_checkpoint(given.test.out_dir)
    if not all(pc.ok for pc in deployed) or checkpoint is None:
        return ResumeOutput(deployed=deployed, restarted_as=None)
    _ = restart_workers()
    run = next_run_name(given.case.run, run_names())
    start_driver(run, checkpoint.path)
    return ResumeOutput(deployed=deployed, restarted_as=run)


class ReportInput(Result):
    case: LossCase | None
    loss_class: LossClass | None
    handler: HandlerCandidate | None
    review: HandlerReview | None
    test: HandlerTest | None
    recorded: RecordReport | None
    commit: CommitHash | None
    restarted_as: RunName | None


class ReportOutput(Result):
    report: RepairReport


@workflow.step
def write_report(given: ReportInput) -> ReportOutput:
    """How the repair ended."""
    if given.case is None:
        verdict = RepairVerdict.NO_LOSS
    elif given.loss_class is not None and not given.loss_class.ask_model:
        verdict = RepairVerdict.NO_HANDLER
    elif given.commit is not None:
        verdict = RepairVerdict.REPAIRED
    elif given.recorded is not None and given.recorded.verdict is not RecordVerdict.RECORDED:
        verdict = RepairVerdict.RECORD_FAILED
    else:
        verdict = RepairVerdict.GAVE_UP
    return ReportOutput(report=RepairReport(verdict=verdict, case=given.case, handler=given.handler, test=given.test,
                                            recorded=given.recorded, commit=given.commit,
                                            restarted_as=given.restarted_as))


# ── decisions ──────────────────────────────────────────────────────────

def loss_found(state: VidsyncRepairState) -> bool:
    return state.case is not None


def no_loss_found(state: VidsyncRepairState) -> bool:
    return state.case is None


def handler_wanted(state: VidsyncRepairState) -> bool:
    return state.loss_class is not None and state.loss_class.ask_model


def no_handler_wanted(state: VidsyncRepairState) -> bool:
    return not handler_wanted(state)


def attempts_left(state: VidsyncRepairState) -> bool:
    return state.attempts < state.max_attempts


def candidate_written(state: VidsyncRepairState) -> bool:
    return state.handler is not None and state.handler.code is not None and not state.handler.problems


def no_candidate_but_attempts_left(state: VidsyncRepairState) -> bool:
    return not candidate_written(state) and attempts_left(state)


def no_candidate_and_no_attempts_left(state: VidsyncRepairState) -> bool:
    return not candidate_written(state) and not attempts_left(state)


def approved(state: VidsyncRepairState) -> bool:
    return state.review is not None and state.review.verdict is ReviewVerdict.APPROVE


def changes_requested(state: VidsyncRepairState) -> bool:
    return (state.review is not None and state.review.verdict is ReviewVerdict.REQUEST_CHANGES
            and attempts_left(state))


def rejected_or_out_of_attempts(state: VidsyncRepairState) -> bool:
    return not approved(state) and not changes_requested(state)


def test_passed(state: VidsyncRepairState) -> bool:
    return state.test is not None and state.test.passed


def test_failed_attempts_left(state: VidsyncRepairState) -> bool:
    return not test_passed(state) and attempts_left(state)


def test_failed_no_attempts_left(state: VidsyncRepairState) -> bool:
    return not test_passed(state) and not attempts_left(state)


def findings_to_record(state: VidsyncRepairState) -> bool:
    return state.findings is not None and bool(state.findings.symbols or state.findings.renames)


def nothing_to_record(state: VidsyncRepairState) -> bool:
    return not findings_to_record(state)


def recorded(state: VidsyncRepairState) -> bool:
    return state.recorded is not None and state.recorded.verdict is RecordVerdict.RECORDED


def record_failed(state: VidsyncRepairState) -> bool:
    return not recorded(state)

