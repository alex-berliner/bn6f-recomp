"""The record graph with fake database tools: problems stop it, renames
re-verify the port, a broken port is not committed."""
from dataclasses import dataclass, field
from pathlib import Path

import pytest

from wf.graphs import record
from wf.graphs.record import RecordReport, RecordState, RecordVerdict, record_graph
from wf.graphs.verify import Verdict, VerifyReport, VerifyState
from wf.burr_typed import Finished
from wf.types import (CommitHash, LogPath, Rename, SourceVersion, SymbolEntry, SymbolName, SymbolStatus,
                      ToolMessage)

KNOWN = SymbolName("knownSymbol")
NEW_NAME = SymbolName("knownSymbolRenamed")


@dataclass
class FakeDatabase:
    problems: tuple[ToolMessage, ...] = ()
    status: SymbolStatus = SymbolStatus.PROBABLE
    port_still_verifies: bool = True
    called: list[str] = field(default_factory=list[str])


def test_database_problems_stop_before_anything_changes(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    database = FakeDatabase(problems=(ToolMessage("symbols/db/x.yaml: missing summary"),))
    report = record_with(database, monkeypatch, tmp_path, renames=())

    assert report.verdict is RecordVerdict.DATABASE_PROBLEMS
    assert database.called == ["check"]


def test_a_hypothesis_is_not_renamed(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    database = FakeDatabase(status=SymbolStatus.HYPOTHESIS)
    report = record_with(database, monkeypatch, tmp_path, renames=(Rename(old=KNOWN, new=NEW_NAME),))

    assert report.verdict is RecordVerdict.DATABASE_PROBLEMS
    assert "hypotheses get comments only" in report.problems[0]


def test_without_renames_it_comments_renders_and_commits(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    database = FakeDatabase()
    report = record_with(database, monkeypatch, tmp_path, renames=())

    assert report.verdict is RecordVerdict.RECORDED
    assert database.called == ["check", "comments", "render", "commit"]


def test_renames_re_verify_the_port_before_committing(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    database = FakeDatabase()
    report = record_with(database, monkeypatch, tmp_path, renames=(Rename(old=KNOWN, new=NEW_NAME),))

    assert report.verdict is RecordVerdict.RECORDED
    assert database.called == ["check", "rename", "pc port symbols", "comments", "render", "verify", "commit"]


def test_a_port_broken_by_renames_is_not_committed(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    database = FakeDatabase(port_still_verifies=False)
    report = record_with(database, monkeypatch, tmp_path, renames=(Rename(old=KNOWN, new=NEW_NAME),))

    assert report.verdict is RecordVerdict.PORT_BROKE
    assert "commit" not in database.called


# ── helpers ────────────────────────────────────────────────────────────

def record_with(database: FakeDatabase, monkeypatch: pytest.MonkeyPatch, run_dir: Path,
                renames: tuple[Rename, ...]) -> RecordReport:
    install(database, monkeypatch, run_dir)
    start = RecordState(run_dir=run_dir, symbols=(KNOWN,), renames=renames,
                        commit_message="Document knownSymbol")
    outcome = record_graph().start(start, run_id="record", tracked=False)
    assert isinstance(outcome, Finished) and outcome.state.report is not None
    return outcome.state.report


def install(database: FakeDatabase, monkeypatch: pytest.MonkeyPatch, run_dir: Path) -> None:
    def check_database(log: LogPath) -> tuple[ToolMessage, ...]:
        database.called.append("check")
        return database.problems

    def entry(name: SymbolName) -> SymbolEntry | None:
        return SymbolEntry(name=name, status=database.status, file=run_dir / f"{name}.yaml")

    def rename_symbols(renames: tuple[Rename, ...], log: LogPath) -> CommitHash:
        database.called.append("rename")
        return CommitHash("b1")

    def refresh_pc_port_symbols(log: LogPath) -> None:
        database.called.append("pc port symbols")

    def write_bn6f_comments(log: LogPath) -> CommitHash | None:
        database.called.append("comments")
        return CommitHash("b2")

    def render_database(log: LogPath) -> None:
        database.called.append("render")

    def run_verify(start: VerifyState, run_id: str, tracked: bool = True) -> VerifyReport:
        database.called.append("verify")
        verdict = Verdict.PASSED if database.port_still_verifies else Verdict.FAILED
        return VerifyReport(sources=SourceVersion(commit=CommitHash("abc"), has_uncommitted_changes=True),
                            verdict=verdict, build_failure=None, failed_functions=(), oracle_differences=(),
                            never_compared=(), routes_checked=(), failed_routes=(), battle_sweep=None)

    def commit_files(files: tuple[Path, ...], message: str) -> CommitHash:
        database.called.append("commit")
        return CommitHash("c0ffee")

    def changed_files(under: tuple[Path, ...]) -> tuple[Path, ...]:
        return (run_dir / "symbols.yaml",)

    fakes: dict[str, object] = {
        "check_database": check_database, "entry": entry, "rename_symbols": rename_symbols,
        "refresh_pc_port_symbols": refresh_pc_port_symbols, "write_bn6f_comments": write_bn6f_comments,
        "render_database": render_database, "run_verify": run_verify, "commit_files": commit_files,
        "changed_files": changed_files,
    }
    for name, fake in fakes.items():
        monkeypatch.setattr(record, name, fake)
