"""The convert graph with a fake model, quick test, verify, git and record:
failures and review comments go back to the model, models escalate, the
orchestrator only answers the review, give-ups take the file out again."""
from contextlib import nullcontext
from dataclasses import dataclass, field
from pathlib import Path

import pytest

from wf.burr_typed import Finished, Paused
from wf.graphs import convert
from wf.graphs.capture import CaptureReport, CaptureState
from wf.graphs.convert import (GIVEN_CANDIDATE, Attempt, ConvertOutcome, ConvertState, Review, ReviewDecision,
                               ReviewRequest, convert_graph, failing_functions, review_sheet)
from wf.graphs.record import RecordReport, RecordState, RecordVerdict
from wf.graphs.verify import Verdict, VerifyReport, VerifyState
from wf.repo import REPO
from wf.tools import sources
from wf.tools.evidence import FunctionChecks, Provenance
from wf.tools.models import AssemblyText, Contract, ConversionContext, LiftedC, Prompt
from wf.tools.registers import LintedFunction, RegisterCheck
from wf.tools.retype import BASELINE, QUEUE, QueueRow
from wf.tools.subsystem import CandidateFile, FileReply, FileTest, FunctionTest, SymbolDraft, TokenUse
from wf.types import (BattleSweepResult, BuildFailure, CommitHash, FunctionName, GuestAddress, LogPath, ModelSpec,
                      SnapshotDir,
                      SourceVersion, SymbolName, SymbolStatus, ToolMessage)

FUNCTION = FunctionName("sub_8030808")
OTHER = FunctionName("sub_8030892")
FARM_MODEL = ModelSpec("hyper/cheap")
STRONG, STRONGER = ModelSpec("opencode/strong"), ModelSpec("zai/stronger")
QUOTA_SPENT = ToolMessage("model error: opencode/strong: HTTP 429 (retry after 200000s): weekly quota used")
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
    blocked: set[ModelSpec] = field(default_factory=set[ModelSpec])
    out_of_quota: set[ModelSpec] = field(default_factory=set[ModelSpec])   # these answer with a 429
    reported: list[ModelSpec] = field(default_factory=list[ModelSpec])     # sources recorded as down
    assembled: list[tuple[FunctionName, ...]] = field(default_factory=list[tuple[FunctionName, ...]])
    name_failures: bool = True  # a failed quick test names its last function
    removed_trees: list[Path] = field(default_factory=list[Path])
    misfits: dict[FunctionName, str] = field(default_factory=dict[FunctionName, str])   # layout conflicts
    restored: list[Path] = field(default_factory=list[Path])        # worktrees reset to their commit
    laid_out: list[Path] = field(default_factory=list[Path])        # trees a farm group was laid out in
    committed_files: list[tuple[Path, ...]] = field(default_factory=list[tuple[Path, ...]])


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


def test_a_farm_group_drops_failing_functions_and_keeps_the_rest(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[False, True], verdicts=[Verdict.PASSED])
    paused = start(world, monkeypatch, tmp_path, run_id="convert-farm", farm_model=FARM_MODEL,
                   functions=(FUNCTION, OTHER))

    assert isinstance(paused, Paused) and isinstance(paused.request, ReviewRequest)
    assert [attempt.model for attempt in paused.request.attempts] == [FARM_MODEL, FARM_MODEL]
    assert world.assembled == [(FUNCTION, OTHER), (FUNCTION,)]
    assert [each.function for each in paused.request.dropped] == [OTHER]
    assert world.asked == []  # a farm group never goes to the strong models


def test_a_farm_group_is_given_up_when_no_function_is_named(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[False], verdicts=[], name_failures=False)
    finished = start(world, monkeypatch, tmp_path, run_id="convert-farm-2", farm_model=FARM_MODEL,
                     functions=(FUNCTION, OTHER))

    assert isinstance(finished, Finished) and finished.state.outcome is ConvertOutcome.OUT_OF_ATTEMPTS
    assert world.asked == []


def test_review_can_take_functions_out_of_a_farm_group(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True, True], verdicts=[Verdict.PASSED, Verdict.PASSED])
    paused = start(world, monkeypatch, tmp_path, run_id="convert-farm-3", farm_model=FARM_MODEL,
                   functions=(FUNCTION, OTHER))
    assert isinstance(paused, Paused)

    again = convert_graph().resume("convert-farm-3", Review(decision=ReviewDecision.REQUEST_CHANGES,
                                                            comments="unclear", drop=(OTHER,)))

    assert isinstance(again, Paused) and isinstance(again.request, ReviewRequest)
    assert world.assembled[-1] == (FUNCTION,) and world.verified[-1] == (FUNCTION,)


def test_a_rate_limited_provider_is_passed_over(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True], verdicts=[Verdict.PASSED], blocked={STRONG})
    paused = start(world, monkeypatch, tmp_path, run_id="convert-blocked")

    assert isinstance(paused, Paused)
    assert world.asked == [STRONGER]


def test_a_provider_out_of_quota_hands_the_same_round_to_the_next(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[False, True], verdicts=[Verdict.PASSED], out_of_quota={STRONG})
    paused = start(world, monkeypatch, tmp_path, run_id="convert-quota")

    assert isinstance(paused, Paused)
    assert world.asked == [STRONG, STRONGER, STRONGER] and world.reported == [STRONG]


def test_every_provider_out_of_quota_gives_up_saying_so(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[False], out_of_quota={STRONG, STRONGER})
    finished = start(world, monkeypatch, tmp_path, run_id="convert-all-out")

    assert isinstance(finished, Finished) and finished.state.outcome is ConvertOutcome.SOURCES_OUT
    assert world.asked == [STRONG, STRONGER]


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


def test_a_rejection_takes_the_run_tree_away(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True], verdicts=[Verdict.PASSED])
    _ = start(world, monkeypatch, tmp_path, run_id="convert-6")

    finished = convert_graph().resume("convert-6", Review(decision=ReviewDecision.REJECT))

    assert isinstance(finished, Finished) and finished.state.outcome is ConvertOutcome.REJECTED
    assert world.removed_trees == [tmp_path / "tree"] and not world.commits  # the file was only in the run's tree


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


def test_a_farm_group_goes_to_the_laid_out_files_for_its_verify_and_its_commit(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True], verdicts=[Verdict.PASSED])
    paused = start(world, monkeypatch, tmp_path, run_id="convert-farm-4", farm_model=FARM_MODEL,
                   functions=(FUNCTION, OTHER))
    assert isinstance(paused, Paused) and isinstance(paused.request, ReviewRequest)
    assert world.laid_out == [tmp_path / "tree"] and paused.request.attempts[-1].laid_out

    finished = convert_graph().resume("convert-farm-4", APPROVE)

    assert isinstance(finished, Finished)
    assert world.laid_out[-1] == REPO
    assert world.committed_files[0][:1] == (REPO / "cport/src/game/asm03_0.c",)
    assert "laid out into asm03_0.c (tools/layout.py)" in world.commits[0]


def test_a_farm_function_that_does_not_fit_the_laid_out_files_is_dropped(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    why = "cport/src/game/asm03_0.c: sub_8030892 uses SPEED, which the part of x defines before it"
    world = World(quick_tests=[True, True], verdicts=[Verdict.PASSED], misfits={OTHER: why})
    paused = start(world, monkeypatch, tmp_path, run_id="convert-farm-5", farm_model=FARM_MODEL,
                   functions=(FUNCTION, OTHER))

    assert isinstance(paused, Paused) and isinstance(paused.request, ReviewRequest)
    assert [(each.function, each.reason) for each in paused.request.dropped] == \
        [(OTHER, f"does not fit the laid-out files: {why}")]
    assert world.verified == [(FUNCTION,)]


def test_a_failed_farm_verify_resets_the_worktree(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True], verdicts=[Verdict.FAILED])
    _ = start(world, monkeypatch, tmp_path, run_id="convert-farm-6", farm_model=FARM_MODEL, functions=(FUNCTION,))

    assert world.laid_out == [tmp_path / "tree"]
    assert world.restored.count(tmp_path / "tree") >= 3   # quick test, install, and after the failed verify


def test_an_old_prompt_farm_group_keeps_register_form_and_queues_it_for_retype(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True], verdicts=[Verdict.PASSED])
    queued: list[tuple[FunctionName, ...]] = []
    filled: list[tuple[tuple[FunctionName, ...], CommitHash]] = []

    def queue_install(functions: tuple[FunctionName, ...], assembled: str, laid_out: tuple[Path, ...],
                      model: ModelSpec, prompt_hash: str, run: str) -> tuple[QueueRow, ...]:
        queued.append(functions)
        assert model == ModelSpec("hyper/cheap+old") and run == "convert-farm-old"
        return tuple(QueueRow(function=function, lint_keys=(function,), prompt_hash=prompt_hash, model=model,
                              install_run=run, install_commit="-", why="register form, old prompt; queued for retype")
                     for function in functions)

    def fill_install_commit(functions: tuple[FunctionName, ...], commit: CommitHash) -> bool:
        filled.append((functions, commit))
        return True

    def register_form(code: str, file_name: str) -> RegisterCheck:
        bad = LintedFunction(name=FUNCTION, kind="next", line=1, counts={"regs": 3}, lines={"regs": [1]},
                             exception=None, violation=True)
        return RegisterCheck(violations=(bad,), exceptions=(), report="registers")

    def leftover_reads(code: str, file_name: str) -> dict[FunctionName, str]:   # OTHER's, until it is dropped
        return {OTHER: "reads r0 after TestEventFlag, which does not return it (site)"} if not world.verified \
            and len(world.assembled) == 1 else {}

    monkeypatch.setattr(convert, "queue_install", queue_install)
    monkeypatch.setattr(convert, "fill_install_commit", fill_install_commit)
    monkeypatch.setattr(convert, "check_register_use", register_form)
    monkeypatch.setattr(convert, "leftover_reads", leftover_reads)
    world.quick_tests = [True, True]
    paused = start(world, monkeypatch, tmp_path, run_id="convert-farm-old", farm_model=ModelSpec("hyper/cheap+old"),
                   functions=(FUNCTION, OTHER), register_check_faked=True)
    assert isinstance(paused, Paused) and isinstance(paused.request, ReviewRequest)
    assert [(each.function, each.reason) for each in paused.request.dropped] == \
        [(OTHER, "leftover read: reads r0 after TestEventFlag, which does not return it (site)")]

    finished = convert_graph().resume("convert-farm-old", APPROVE)

    assert isinstance(finished, Finished) and finished.state.retype_queued == (FUNCTION,)
    assert queued == [(FUNCTION,)] and filled == [((FUNCTION,), CommitHash("c0ffee0"))]
    assert world.committed_files[0][-3:] == (BASELINE, QUEUE, REPO / "cport/host/difftest.cfg")
    assert "written by hyper/cheap+old" in world.commits[0] and "queued for retype" in world.commits[0]
    assert "laid out into asm03_0.c (tools/layout.py)" in world.commits[0]
    assert world.committed_files[1] == (QUEUE,)   # the install commit, filled in after the record


def test_a_typed_farm_group_in_register_form_is_still_refused(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    world = World(quick_tests=[True], verdicts=[Verdict.PASSED])

    def register_form(code: str, file_name: str) -> RegisterCheck:
        bad = LintedFunction(name=FUNCTION, kind="next", line=1, counts={"regs": 3}, lines={"regs": [1]},
                             exception=None, violation=True)
        return RegisterCheck(violations=(bad,), exceptions=(), report="registers")

    monkeypatch.setattr(convert, "check_register_use", register_form)
    finished = start(world, monkeypatch, tmp_path, run_id="convert-farm-typed", farm_model=FARM_MODEL,
                     functions=(FUNCTION,), register_check_faked=True)

    assert isinstance(finished, Finished) and finished.state.outcome is ConvertOutcome.REGISTER_FORM


def test_a_battle_sweep_difference_no_check_names_sets_half_the_group_aside() -> None:
    code = "\n".join(f"/* ── {name} ── */\nNext {name}(void) {{ RETURN_TO_CALLER(); }}"
                     for name in ("sub_8000100", "sub_8000200", "sub_8000300", "sub_8000400"))
    sweep = BattleSweepResult(battles=116, identical=105, failures=(ToolMessage("0x80af270"),) * 11,
                              log=LogPath(Path("sweep.log")), seconds=1.0)
    report = VerifyReport(sources=SourceVersion(commit=CommitHash("abc"), has_uncommitted_changes=True),
                          verdict=Verdict.FAILED, build_failure=None, failed_functions=(), oracle_differences=(),
                          never_compared=(), routes_checked=(), failed_routes=(), battle_sweep=sweep)
    attempt = Attempt(number=1, model=FARM_MODEL, given=True, verification=report,
                      reply=FileReply(file=CandidateFile(code), cfg_lines=(), drafts=(), problems=(), seconds=0.0))

    failing = failing_functions(attempt)

    assert sorted(failing) == ["sub_8000300", "sub_8000400"]
    assert "the battle sweep differed in 11 battles" in failing[FunctionName("sub_8000300")]


# ── the fakes ──────────────────────────────────────────────────────────

def start(world: World, monkeypatch: pytest.MonkeyPatch, tmp_path: Path, run_id: str, rounds: int = 3,
          candidate: Path | None = None, with_calls: bool = True,
          farm_model: ModelSpec | None = None,
          functions: tuple[FunctionName, ...] = (FUNCTION,),
          register_check_faked: bool = False) -> Finished[ConvertState] | Paused:
    install(world, monkeypatch, tmp_path, register_check_faked)
    if with_calls:
        for function in functions:
            (tmp_path / "snaps" / function).mkdir(parents=True, exist_ok=True)
            _ = (tmp_path / "snaps" / function / "snap_wander_0.bin").write_bytes(b"")
    state = ConvertState(run_dir=tmp_path, subsystem="BG map redraw", file_name="bg_redraw.c", functions=functions,
                         models=(STRONG, STRONGER), rounds=rounds, snapshots=SnapshotDir(tmp_path / "snaps"),
                         candidate=candidate, farm_model=farm_model)
    return convert_graph().start(state, run_id=run_id)


DRAFT = SymbolDraft(function=FUNCTION, proposed_name=SymbolName("redrawBGTileMap_8030808"),
                    status=SymbolStatus.PROBABLE, summary="Redraw the whole BG window.",
                    interface="void redraw(MapTiles *tiles)", details="")


def install(world: World, monkeypatch: pytest.MonkeyPatch, tmp_path: Path, register_check_faked: bool = False) -> None:
    def run_capture(start: CaptureState, run_id: str, tracked: bool = True) -> CaptureReport:
        return CaptureReport(snapshots=start.snapshots, routes=(), captured=(),
                             never_called=())

    def build_host_difftest(log_dir: Path, build_dir: str = "", tree: Path | None = None) -> Path | BuildFailure:
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

    def ask_model_for_file(model: ModelSpec, prompt: Prompt, tag: str, record_dir: Path,
                           wait_out_limits: bool = True) -> FileReply:
        world.asked.append(model)
        world.prompts.append(prompt)
        if model in world.out_of_quota:
            return FileReply(file=None, cfg_lines=(), drafts=(), problems=(), seconds=1.0, error=QUOTA_SPENT)
        return FileReply(file=CandidateFile("Next sub_8030808(void) { RETURN_TO_CALLER(); }"), cfg_lines=(),
                         drafts=(DRAFT,), problems=(), seconds=1.0)

    def candidate_from_farm(functions: tuple[FunctionName, ...], model: ModelSpec, subsystem: str,
                            file_name: str, taken: dict[FunctionName, frozenset[str]] | None = None) -> FileReply:
        world.assembled.append(functions)
        return FileReply(file=CandidateFile("Next sub_8030808(void) { RETURN_TO_CALLER(); }"), cfg_lines=(),
                         drafts=(DRAFT,), problems=(), seconds=0.0)

    def candidate_from_file(path: Path) -> FileReply:
        return FileReply(file=CandidateFile(path.read_text()), cfg_lines=(), drafts=(DRAFT,), problems=(), seconds=0.0)

    def test_candidate_file(contexts: tuple[ConversionContext, ...], reply: FileReply, snapshots: SnapshotDir,
                            difftest: Path) -> FileTest:
        passed = world.quick_tests.pop(0)
        named = not passed and world.name_failures
        tests = tuple(FunctionTest(function=context.function, captured=ToolMessage("ok"), random=ToolMessage("ok"),
                                   passed=not (named and context is contexts[-1])) for context in contexts)
        return FileTest(passed=passed, report=ToolMessage("ok" if passed else "sub_8030808: FAIL"), functions=tests,
                        register_use_outside_adapters=(), register_form_notes=())

    def install_file(file_name: str, code: CandidateFile, cfg_lines: tuple[str, ...], tree: Path = tmp_path) -> Path:
        world.installed.append(True)
        return tmp_path / file_name

    def uninstall_file(file_name: str, tree: Path = tmp_path) -> None:
        world.installed.append(False)

    def committed_tree(name: str) -> Path:
        return tmp_path / "tree"

    def remove_tree(tree: Path) -> None:
        world.removed_trees.append(tree)

    def run_verify_in_full(start: VerifyState, run_id: str, tracked: bool = True) -> VerifyState:
        world.verified.append(start.functions)
        report = VerifyReport(sources=SourceVersion(commit=CommitHash("abc"), has_uncommitted_changes=True),
                              verdict=world.verdicts.pop(0), build_failure=None, failed_functions=(),
                              oracle_differences=(), never_compared=(), routes_checked=(), failed_routes=(),
                              battle_sweep=None)
        return start.model_copy(update={"report": report})

    def commit_files(files: tuple[Path, ...], message: str) -> CommitHash:
        world.commits.append(message)
        world.committed_files.append(files)
        return CommitHash("c0ffee0")

    def restore_game_files(tree: Path) -> None:
        world.restored.append(tree)

    def lay_out_group(file_name: str, tree: Path = REPO) -> tuple[Path, ...]:
        world.laid_out.append(tree)
        return (tree / "cport/src/game/asm03_0.c",)

    def layout_conflicts(file_name: str, code: CandidateFile, functions: tuple[FunctionName, ...],
                         tree: Path) -> dict[FunctionName, str]:
        return {function: why for function, why in world.misfits.items() if function in functions}

    def names_taken(functions: tuple[FunctionName, ...], tree: Path = REPO) -> dict[FunctionName, frozenset[str]]:
        return {}

    def placed(functions: tuple[FunctionName, ...], tree: Path = REPO) -> dict[FunctionName, Path]:
        return {function: REPO / "cport/src/game/asm03_0.c" for function in functions}

    def write_entries(drafts: tuple[SymbolDraft, ...], checks: tuple[FunctionChecks, ...],
                      provenance: Provenance) -> tuple[Path, ...]:
        return tuple(tmp_path / f"{draft.function}.yaml" for draft in drafts)

    def run_record(start: RecordState, run_id: str, tracked: bool = True) -> RecordReport:
        world.recorded.append(start)
        return RecordReport(verdict=RecordVerdict.RECORDED, problems=(), renamed_in_bn6f=None, comments_in_bn6f=None,
                            verification=None, commit=CommitHash("d0c0"))

    def down_until(model: ModelSpec) -> float | None:
        return 1e12 if model in world.blocked or model in world.reported else None

    def report_failure(model: ModelSpec, error: str) -> None:
        world.reported.append(model)

    def tokens_for(tag: str) -> TokenUse:
        return TokenUse(tokens_in=100, tokens_out=50)

    fakes: dict[str, object] = {
        "run_capture": run_capture, "build_host_difftest": build_host_difftest, "game_file": game_file,
        "conversion_context": conversion_context, "other_entries": other_entries, "subsystem_prompt": subsystem_prompt,
        "ask_model_for_file": ask_model_for_file, "candidate_from_file": candidate_from_file,
        "candidate_from_farm": candidate_from_farm, "committed_tree": committed_tree, "remove_tree": remove_tree,
        "held": lambda name, exclusive=True: nullcontext(),  # pyright: ignore[reportUnknownLambdaType]
        "test_candidate_file": test_candidate_file, "install_file": install_file, "uninstall_file": uninstall_file,
        "run_verify_in_full": run_verify_in_full, "commit_files": commit_files, "write_entries": write_entries,
        "run_record": run_record, "tokens_for": tokens_for, "restore_game_files": restore_game_files,
        "lay_out_group": lay_out_group, "layout_conflicts": layout_conflicts, "placed": placed,
        "names_taken": names_taken,
    }
    for name, fake in fakes.items():
        monkeypatch.setattr(convert, name, fake)
    monkeypatch.setattr(sources, "down_until", down_until)
    monkeypatch.setattr(sources, "report_failure", report_failure)
    def registers_clean(code: str, file_name: str) -> RegisterCheck:   # the fixtures are in register form
        return RegisterCheck(violations=(), exceptions=(), report="")

    if not register_check_faked:   # tests of the register check itself set their own
        monkeypatch.setattr(convert, "check_register_use", registers_clean)
