"""The convert graph with fake capture, verify and git: it stops for Claude's
code, goes back to Claude after a failed verify, and commits once verified."""
from pathlib import Path

import pytest

from wf.burr_typed import Finished, Paused
from wf.graphs import convert
from wf.graphs.capture import CaptureReport, CaptureState
from wf.graphs.convert import CodeRequest, ConvertState, WrittenCode, convert_graph
from wf.graphs.verify import Verdict, VerifyReport, VerifyState
from wf.types import CommitHash, FunctionName, NotRun, SourceVersion, ToolMessage

FUNCTION = FunctionName("someFunction")
ANSWER = WrittenCode(files=(Path("cport/src/game/thing.c"),), battle_code=False, commit_message="Readable thing")


def test_it_stops_for_claude_after_capturing(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, verdicts=[])

    outcome = convert_graph().start(start(tmp_path), run_id="convert-1")

    assert isinstance(outcome, Paused) and outcome.step == "write_readable_c"
    assert isinstance(outcome.request, CodeRequest) and outcome.request.verification is None


def test_a_failed_verify_goes_back_to_claude_with_the_report(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, verdicts=[Verdict.FAILED])
    _ = convert_graph().start(start(tmp_path), run_id="convert-2")

    outcome = convert_graph().resume("convert-2", ANSWER)

    assert isinstance(outcome, Paused) and isinstance(outcome.request, CodeRequest)
    assert outcome.request.verification is not None
    assert outcome.request.verification.verdict is Verdict.FAILED


def test_a_verified_conversion_is_committed(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    commits = fake_tools(monkeypatch, verdicts=[Verdict.FAILED, Verdict.PASSED])
    _ = convert_graph().start(start(tmp_path), run_id="convert-3")
    _ = convert_graph().resume("convert-3", ANSWER)

    outcome = convert_graph().resume("convert-3", ANSWER)

    assert isinstance(outcome, Finished)
    assert outcome.state.commit == CommitHash("c0ffee")
    assert outcome.state.verify_runs == ("convert-3-verify-1", "convert-3-verify-2")
    assert commits == ["Readable thing"]


def start(tmp_path: Path) -> ConvertState:
    return ConvertState(run_dir=tmp_path, subsystem="things", functions=(FUNCTION,))


def fake_tools(monkeypatch: pytest.MonkeyPatch, verdicts: list[Verdict]) -> list[str]:
    commits: list[str] = []
    verdicts_left = iter(verdicts)

    def run_capture(start: CaptureState, run_id: str, tracked: bool = True) -> CaptureReport:
        return CaptureReport(snapshots=start.snapshots, route=NotRun(reason=ToolMessage("fake")), captured=(),
                             never_called=())

    def run_verify(start: VerifyState, run_id: str, tracked: bool = True) -> VerifyReport:
        return VerifyReport(sources=SourceVersion(commit=CommitHash("abc"), has_uncommitted_changes=True),
                            verdict=next(verdicts_left), build_failure=None, failed_functions=(),
                            oracle_differences=(), never_compared=(), routes_checked=(), failed_routes=(),
                            battle_sweep=None)

    def commit_files(files: tuple[Path, ...], message: str) -> CommitHash:
        commits.append(message)
        return CommitHash("c0ffee")

    monkeypatch.setattr(convert, "run_capture", run_capture)
    monkeypatch.setattr(convert, "run_verify", run_verify)
    monkeypatch.setattr(convert, "commit_files", commit_files)
    return commits
