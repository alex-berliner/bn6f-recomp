"""The convert graph with a fake model, quick test, verify, git and record:
failures and review comments go back to the model, models escalate, the
orchestrator only answers the review, give-ups take the file out again."""
from dataclasses import dataclass, field
from pathlib import Path

import pytest

from wf.burr_typed import Finished, Paused
from wf.graphs import convert
from wf.graphs.capture import CaptureReport, CaptureState
from wf.graphs.convert import (GIVEN_CANDIDATE, ConvertOutcome, ConvertState, Review, ReviewDecision, ReviewRequest,
                               convert_graph, review_sheet)
from wf.graphs.record import RecordReport, RecordState, RecordVerdict
from wf.graphs.verify import Verdict, VerifyReport, VerifyState
from wf.tools.evidence import FunctionChecks, Provenance
from wf.tools.models import AssemblyText, Contract, ConversionContext, LiftedC, Prompt
from wf.tools.subsystem import CandidateFile, FileReply, FileTest, SymbolDraft, TokenUse
from wf.types import (BuildFailure, CommitHash, FunctionName, GuestAddress, ModelSpec, SnapshotDir,
                      SourceVersion, SymbolName, SymbolStatus, ToolMessage)

FUNCTION = FunctionName("sub_8030808")
STRONG, STRONGER = ModelSpec("opencode/strong"), ModelSpec("opencode/stronger")
APPROVE = Review(decision=ReviewDecision.APPROVE, attribution="Reviewed-by: test")


@dataclass
class World:
    """The fakes' script: what each call returns, and what happened."""
    quick_tests: list[bool] = field(default_factory=list[bool])
    verdicts: list[Verdict] = field(default_factory=list[Verdict])
    asked: list[ModelSpec] = field(default_factory=list[ModelSpec])
    prompts: list[str] = field(default_factory=list[str])
    installed: list[bool] = field(default_factory=list[bool])
    commits: list[str] = field(default_factory=list[str])
    recorded: list[RecordState] = field(default_factory=list[RecordState])
    file_exists: bool = False
    entries: dict[FunctionName, tuple[FunctionName, ...]] = field(default_factory=dict[FunctionName, tuple[FunctionName, ...]])
    prompted: list[tuple[FunctionName, ...]] = field(default_factory=list[tuple[FunctionName, ...]])
    verified: list[tuple[FunctionName, ...]] = field(default_factory=list[tuple[FunctionName, ...]])


def test_a_good_first_attempt_stops_for_review_then_commits_and_records(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True], verdicts=[Verdict.PASSED])
    paused = start(world, monkeypatch, tmp_path, run_id="convert-1")

    assert isinstance(paused, Paused) and isinstance(paused.request, ReviewRequest)
    assert "## The file" in review_sheet(paused.request)
    finished = convert_graph().resume("convert-1", APPROVE)

    assert isinstance(finished, Finished) and finished.state.outcome is ConvertOutcome.RECORDED
    assert world.asked == [STRONG]
    assert world.commits and "Reviewed-by: test" in world.commits[0]
    assert world.recorded[0].renames[0].new == SymbolName("redrawBGTileMap_8030808")


def test_labels_entered_mid_function_are_written_but_only_the_functions_verified(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    label = FunctionName("loc_803083E")
    world = World(quick_tests=[True], verdicts=[Verdict.PASSED], entries={FUNCTION: (label,)})
    paused = start(world, monkeypatch, tmp_path, run_id="convert-entries")

    assert isinstance(paused, Paused)
    assert world.prompted == [(FUNCTION, label)]
    assert world.verified == [(FUNCTION,)]


def test_a_failed_quick_test_goes_back_to_the_same_model(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[False, True], verdicts=[Verdict.PASSED])
    paused = start(world, monkeypatch, tmp_path, run_id="convert-2")

    assert isinstance(paused, Paused)
    assert world.asked == [STRONG, STRONG]
    assert "The quick test failed" in world.prompts[1]


def test_a_model_out_of_rounds_hands_over_to_the_next(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[False, True], verdicts=[Verdict.PASSED])
    _ = start(world, monkeypatch, tmp_path, run_id="convert-3", rounds=1)

    assert world.asked == [STRONG, STRONGER]


def test_a_failed_verify_takes_the_file_out_and_goes_back_to_the_model(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True, True], verdicts=[Verdict.FAILED, Verdict.PASSED])
    paused = start(world, monkeypatch, tmp_path, run_id="convert-4")

    assert isinstance(paused, Paused)
    assert world.installed == [True, False, True]
    assert "The full verify failed" in world.prompts[1]


def test_review_comments_go_to_the_model_not_the_orchestrator(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True, True], verdicts=[Verdict.PASSED, Verdict.PASSED])
    _ = start(world, monkeypatch, tmp_path, run_id="convert-5")

    again = convert_graph().resume("convert-5", Review(decision=ReviewDecision.REQUEST_CHANGES,
                                                       comments="make draw_edge take a struct"))

    assert isinstance(again, Paused)
    assert "make draw_edge take a struct" in world.prompts[1]


def test_a_rejection_takes_the_file_out(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True], verdicts=[Verdict.PASSED])
    _ = start(world, monkeypatch, tmp_path, run_id="convert-6")

    finished = convert_graph().resume("convert-6", Review(decision=ReviewDecision.REJECT))

    assert isinstance(finished, Finished) and finished.state.outcome is ConvertOutcome.REJECTED
    assert world.installed[-1] is False and not world.commits


def test_every_model_out_of_attempts_gives_up(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[False, False])
    finished = start(world, monkeypatch, tmp_path, run_id="convert-7", rounds=1)

    assert isinstance(finished, Finished) and finished.state.outcome is ConvertOutcome.OUT_OF_ATTEMPTS


def test_a_given_candidate_is_tried_before_any_model(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True], verdicts=[Verdict.PASSED])
    candidate = tmp_path / "reviewed.c"
    _ = candidate.write_text("Next sub_8030808(void) { RETURN_TO_CALLER(); }\n")
    paused = start(world, monkeypatch, tmp_path, run_id="convert-8", candidate=candidate)

    assert isinstance(paused, Paused) and isinstance(paused.request, ReviewRequest)
    assert paused.request.attempts[0].model == GIVEN_CANDIDATE and world.asked == []


def test_an_existing_file_is_never_overwritten(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(file_exists=True)
    finished = start(world, monkeypatch, tmp_path, run_id="convert-9")

    assert isinstance(finished, Finished) and finished.state.outcome is ConvertOutcome.NOTHING_TO_CONVERT
    assert world.asked == []


def test_no_model_is_asked_when_a_function_has_no_captured_calls(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World()
    finished = start(world, monkeypatch, tmp_path, run_id="convert-10", with_calls=False)

    assert isinstance(finished, Finished) and finished.state.outcome is ConvertOutcome.NO_CAPTURED_CALLS
    assert finished.state.without_calls == (FUNCTION,) and world.asked == []


# ── the fakes ──────────────────────────────────────────────────────────

def start(world: World, monkeypatch: pytest.MonkeyPatch, tmp_path: Path, run_id: str, rounds: int = 3,
          candidate: Path | None = None, with_calls: bool = True) -> Finished[ConvertState] | Paused:
    install(world, monkeypatch, tmp_path)
    if with_calls:
        (tmp_path / "snaps" / FUNCTION).mkdir(parents=True, exist_ok=True)
        _ = (tmp_path / "snaps" / FUNCTION / "snap_wander_0.bin").write_bytes(b"")
    state = ConvertState(run_dir=tmp_path, subsystem="BG map redraw", file_name="bg_redraw.c", functions=(FUNCTION,),
                         models=(STRONG, STRONGER), rounds=rounds, snapshots=SnapshotDir(tmp_path / "snaps"),
                         candidate=candidate)
    return convert_graph().start(state, run_id=run_id)


DRAFT = SymbolDraft(function=FUNCTION, proposed_name=SymbolName("redrawBGTileMap_8030808"),
                    status=SymbolStatus.PROBABLE, summary="Redraw the whole BG window.",
                    interface="void redraw(MapTiles *tiles)", details="")


def install(world: World, monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    def run_capture(start: CaptureState, run_id: str, tracked: bool = True) -> CaptureReport:
        return CaptureReport(snapshots=start.snapshots, routes=(), captured=(),
                             never_called=())

    def build_host_difftest(log_dir: Path, build_dir: str = "") -> Path | BuildFailure:
        return tmp_path / "difftest"

    def game_file(file_name: str) -> Path:
        path = tmp_path / "game" / file_name
        if world.file_exists:
            path.parent.mkdir(exist_ok=True)
            _ = path.write_text("existing")
        return path

    def conversion_context(function: FunctionName) -> ConversionContext:
        return ConversionContext(function=function, address=GuestAddress(0x08030808), assembly=AssemblyText("bx lr"),
                                 lifted=LiftedC("..."), contract=Contract(inputs=(), outputs=(), clobbers=(),
                                                                          preserved=()))

    def other_entries(context: ConversionContext) -> tuple[ConversionContext, ...]:
        return tuple(context.model_copy(update={"function": label, "entry_of": context.function})
                     for label in world.entries.get(context.function, ()))

    def subsystem_prompt(subsystem: str, file_name: str, contexts: tuple[ConversionContext, ...]) -> Prompt:
        world.prompted.append(tuple(context.function for context in contexts))
        return Prompt("convert these")

    def ask_model_for_file(model: ModelSpec, prompt: Prompt, tag: str, record_dir: Path) -> FileReply:
        world.asked.append(model)
        world.prompts.append(prompt)
        return FileReply(file=CandidateFile("Next sub_8030808(void) { RETURN_TO_CALLER(); }"), cfg_lines=(),
                         drafts=(DRAFT,), problems=(), seconds=1.0)

    def candidate_from_file(path: Path) -> FileReply:
        return FileReply(file=CandidateFile(path.read_text()), cfg_lines=(), drafts=(DRAFT,), problems=(), seconds=0.0)

    def test_candidate_file(contexts: tuple[ConversionContext, ...], reply: FileReply, snapshots: SnapshotDir,
                            difftest: Path) -> FileTest:
        passed = world.quick_tests.pop(0)
        return FileTest(passed=passed, report=ToolMessage("ok" if passed else "sub_8030808: FAIL"), functions=(),
                        register_use_outside_adapters=(), register_form_notes=())

    def install_file(file_name: str, code: CandidateFile, cfg_lines: tuple[str, ...]) -> Path:
        world.installed.append(True)
        return tmp_path / file_name

    def uninstall_file(file_name: str) -> None:
        world.installed.append(False)

    def run_verify_in_full(start: VerifyState, run_id: str, tracked: bool = True) -> VerifyState:
        world.verified.append(start.functions)
        report = VerifyReport(sources=SourceVersion(commit=CommitHash("abc"), has_uncommitted_changes=True),
                              verdict=world.verdicts.pop(0), build_failure=None, failed_functions=(),
                              oracle_differences=(), never_compared=(), routes_checked=(), failed_routes=(),
                              battle_sweep=None)
        return start.model_copy(update={"report": report})

    def commit_files(files: tuple[Path, ...], message: str) -> CommitHash:
        world.commits.append(message)
        return CommitHash("c0ffee0")

    def write_entries(drafts: tuple[SymbolDraft, ...], checks: tuple[FunctionChecks, ...],
                      provenance: Provenance) -> tuple[Path, ...]:
        return tuple(tmp_path / f"{draft.function}.yaml" for draft in drafts)

    def run_record(start: RecordState, run_id: str, tracked: bool = True) -> RecordReport:
        world.recorded.append(start)
        return RecordReport(verdict=RecordVerdict.RECORDED, problems=(), renamed_in_bn6f=None, comments_in_bn6f=None,
                            verification=None, commit=CommitHash("d0c0"))

    def tokens_for(tag: str) -> TokenUse:
        return TokenUse(tokens_in=100, tokens_out=50)

    fakes: dict[str, object] = {
        "run_capture": run_capture, "build_host_difftest": build_host_difftest, "game_file": game_file,
        "conversion_context": conversion_context, "other_entries": other_entries, "subsystem_prompt": subsystem_prompt,
        "ask_model_for_file": ask_model_for_file, "candidate_from_file": candidate_from_file,
        "test_candidate_file": test_candidate_file, "install_file": install_file, "uninstall_file": uninstall_file,
        "run_verify_in_full": run_verify_in_full, "commit_files": commit_files, "write_entries": write_entries,
        "run_record": run_record, "tokens_for": tokens_for,
    }
    for name, fake in fakes.items():
        monkeypatch.setattr(convert, name, fake)
