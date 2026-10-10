"""The record graph with fake database tools: problems stop it, bn6f is built
once, renames re-verify the port only when its ROM changed, a broken port is
not committed."""
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
    port_roms: list[str] = field(default_factory=lambda: ["rom-a", "rom-a"])   # before and after renames
    called: list[str] = field(default_factory=list[str])
    comments_built_bn6f: list[bool] = field(default_factory=list[bool])


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
    assert database.comments_built_bn6f == [True]


def test_renames_with_an_unchanged_port_rom_skip_the_re_verify(monkeypatch: pytest.MonkeyPatch,
                                                                 tmp_path: Path) -> None:
    database = FakeDatabase()
    report = record_with(database, monkeypatch, tmp_path, renames=(Rename(old=KNOWN, new=NEW_NAME),))

    assert report.verdict is RecordVerdict.RECORDED and report.port_rom_unchanged
    assert database.called == ["check", "port", "comments", "rename", "pc port symbols", "render", "port", "commit"]
    assert database.comments_built_bn6f == [False]     # the rename's build is the run's only one


def test_renames_that_change_the_port_rom_re_verify_it(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    database = FakeDatabase(port_roms=["rom-a", "rom-b"])
    report = record_with(database, monkeypatch, tmp_path, renames=(Rename(old=KNOWN, new=NEW_NAME),))

    assert report.verdict is RecordVerdict.RECORDED and not report.port_rom_unchanged
    assert database.called[-3:] == ["port", "verify", "commit"]


def test_a_port_that_did_not_build_before_renames_is_re_verified(monkeypatch: pytest.MonkeyPatch,
                                                                    tmp_path: Path) -> None:
    database = FakeDatabase(port_roms=["", ""])      # no ROM either time: nothing to compare
    report = record_with(database, monkeypatch, tmp_path, renames=(Rename(old=KNOWN, new=NEW_NAME),))

    assert "verify" in database.called and not report.port_rom_unchanged


def test_a_port_broken_by_renames_is_not_committed(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    database = FakeDatabase(port_still_verifies=False, port_roms=["rom-a", "rom-b"])
    report = record_with(database, monkeypatch, tmp_path, renames=(Rename(old=KNOWN, new=NEW_NAME),))

    assert report.verdict is RecordVerdict.PORT_BROKE
    assert "commit" not in database.called


def test_deferred_renames_are_emptied_in_the_commit(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    database = FakeDatabase()
    pending = tmp_path / "pending_renames.txt"
    _ = pending.write_text(f"{KNOWN} {NEW_NAME}\n")
    report = record_with(database, monkeypatch, tmp_path, renames=(Rename(old=KNOWN, new=NEW_NAME),), pending=pending)

    assert report.verdict is RecordVerdict.RECORDED and pending.read_text() == ""


# ── helpers ────────────────────────────────────────────────────────────

def record_with(database: FakeDatabase, monkeypatch: pytest.MonkeyPatch, run_dir: Path,
                renames: tuple[Rename, ...], pending: Path | None = None) -> RecordReport:
    install(database, monkeypatch, run_dir)
    start = RecordState(run_dir=run_dir, symbols=(KNOWN,), renames=renames,
                        commit_message="Document knownSymbol", pending_renames=pending)
    outcome = record_graph().start(start, run_id="record", tracked=False)
    assert isinstance(outcome, Finished) and outcome.state.report is not None
    return outcome.state.report


def install(database: FakeDatabase, monkeypatch: pytest.MonkeyPatch, run_dir: Path) -> None:
    def check_database(log: LogPath) -> tuple[ToolMessage, ...]:
        database.called.append("check")
        return database.problems

    def entry(name: SymbolName) -> SymbolEntry | None:
        return SymbolEntry(name=name, status=database.status, file=run_dir / f"{name}.yaml")

    def rename_symbols(renames: tuple[Rename, ...], log: LogPath, run: str | None = None) -> CommitHash:
        database.called.append("rename")
        return CommitHash("b1")

    def refresh_pc_port_symbols(log: LogPath, build: bool = True) -> None:
        database.called.append("pc port symbols")

    def write_bn6f_comments(log: LogPath, build: bool = True) -> CommitHash | None:
        database.called.append("comments")
        database.comments_built_bn6f.append(build)
        return CommitHash("b2")

    def port_rom_hash(log_dir: Path) -> str | None:
        database.called.append("port")
        return database.port_roms.pop(0) or None

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
        "changed_files": changed_files, "port_rom_hash": port_rom_hash,
    }
    for name, fake in fakes.items():
        monkeypatch.setattr(record, name, fake)
