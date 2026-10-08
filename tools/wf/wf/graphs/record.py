"""Put what was learnt about symbols into the symbol database and the
disassembly (docs/PROCESS.md §6, and the user's rule: every discovery goes
into bn6f and symbols/db with evidence and how to reproduce it).

Claude writes or updates the symbols/db entries first; this workflow checks
the database, renames symbols in bn6f (only confirmed or probable meanings),
writes the `// symdb:` comments into bn6f, renders SYMBOLS.md, re-verifies
the port when anything was renamed, and commits."""
from enum import Enum
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.graphs.verify import Verdict, VerifyReport, VerifyState, run_verify
from wf.repo import REPO
from wf.routes import ALL_ROUTES
from wf.tools.git import changed_files, commit_files
from wf.tools.symbols import (check_database, entry, refresh_pc_port_symbols, rename_symbols, render_database,
                              write_bn6f_comments)
from wf.types import CommitHash, LogPath, Rename, Result, SymbolName, SymbolStatus, ToolMessage

PROJECT = "recomp-record"
RENAMEABLE = (SymbolStatus.CONFIRMED, SymbolStatus.PROBABLE)
ACCEPTED_VERDICTS = (Verdict.PASSED, Verdict.PASSED_WITH_UNCHECKED_FUNCTIONS)
RECORDED_PATHS = (REPO / "symbols", REPO / "bn6f", REPO / "cport", REPO / "tools", REPO / "docs")


def record_graph() -> Graph["RecordState"]:
    return workflow.graph(
        first=check_entries,
        ends=[write_report],
        transitions=[
            Transition(after=check_entries, go_to=rename_in_bn6f, when=clean_with_renames),
            Transition(after=check_entries, go_to=write_comments_into_bn6f, when=clean_without_renames),
            Transition(after=check_entries, go_to=write_report, when=problems_found),
            Transition(after=rename_in_bn6f, go_to=refresh_pc_port_symbols_after_renames),
            Transition(after=refresh_pc_port_symbols_after_renames, go_to=write_comments_into_bn6f),
            Transition(after=write_comments_into_bn6f, go_to=render_symbols),
            Transition(after=render_symbols, go_to=reverify_port, when=symbols_were_renamed),
            Transition(after=render_symbols, go_to=commit_records, when=nothing_was_renamed),
            Transition(after=reverify_port, go_to=commit_records, when=port_still_verified),
            Transition(after=reverify_port, go_to=write_report, when=port_no_longer_verified),
            Transition(after=commit_records, go_to=write_report),
        ],
    )


def run_record(start: "RecordState", run_id: str, tracked: bool = True) -> "RecordReport":
    outcome = record_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"record run {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class RecordVerdict(Enum):
    RECORDED = "recorded and committed"
    DATABASE_PROBLEMS = "the database has problems; nothing was changed"
    PORT_BROKE = "renamed, but the port no longer verifies; nothing was committed"


class RecordReport(Result):
    verdict: RecordVerdict
    problems: tuple[ToolMessage, ...]
    renamed_in_bn6f: CommitHash | None
    comments_in_bn6f: CommitHash | None
    verification: VerifyReport | None
    commit: CommitHash | None


class RecordState(BaseModel):
    run_dir: Path
    symbols: tuple[SymbolName, ...]          # entries Claude wrote or updated
    renames: tuple[Rename, ...] = ()
    commit_message: str                      # in full, attribution lines included
    problems: tuple[ToolMessage, ...] = ()
    renamed_in_bn6f: CommitHash | None = None
    comments_in_bn6f: CommitHash | None = None
    verification: VerifyReport | None = None
    commit: CommitHash | None = None
    report: RecordReport | None = None


workflow = Workflow[RecordState](RecordState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class CheckInput(Result):
    symbols: tuple[SymbolName, ...]
    renames: tuple[Rename, ...]
    run_dir: Path


class CheckOutput(Result):
    problems: tuple[ToolMessage, ...]


@workflow.step
def check_entries(given: CheckInput) -> CheckOutput:
    """Check the database, that each symbol has an entry, and that renames have a confirmed or probable meaning."""
    problems = list(check_database(LogPath(given.run_dir / "symdb.log")))
    for symbol in given.symbols:
        if entry(symbol) is None:
            problems.append(ToolMessage(f"{symbol}: no symbols/db entry"))
    for rename in given.renames:
        found = entry(rename.old)
        if found is None:
            problems.append(ToolMessage(f"{rename.old}: no symbols/db entry to rename"))
        elif found.status not in RENAMEABLE:
            problems.append(ToolMessage(f"{rename.old}: status {found.status.value}; "
                                        "hypotheses get comments only, not renames"))
    return CheckOutput(problems=tuple(problems))


class RenameInput(Result):
    renames: tuple[Rename, ...]
    run_dir: Path


class RenameOutput(Result):
    renamed_in_bn6f: CommitHash


@workflow.step
def rename_in_bn6f(given: RenameInput) -> RenameOutput:
    """Rename in bn6f (ROM stays identical, bn6f commit), in the port's code, and regenerate the lifted C."""
    return RenameOutput(renamed_in_bn6f=rename_symbols(given.renames, LogPath(given.run_dir / "rename.log")))


class RefreshInput(Result):
    run_dir: Path


class RefreshOutput(Result):
    pass


@workflow.step
def refresh_pc_port_symbols_after_renames(given: RefreshInput) -> RefreshOutput:
    """Refresh the PC port's symbol overlay from the renamed bn6f (tools/import_symbols.sh)."""
    refresh_pc_port_symbols(LogPath(given.run_dir / "import_symbols.log"))
    return RefreshOutput()


class CommentsInput(Result):
    run_dir: Path


class CommentsOutput(Result):
    comments_in_bn6f: CommitHash | None


@workflow.step
def write_comments_into_bn6f(given: CommentsInput) -> CommentsOutput:
    """Write each entry's `// symdb:` block at its definition in bn6f (ROM stays identical, bn6f commit)."""
    return CommentsOutput(comments_in_bn6f=write_bn6f_comments(LogPath(given.run_dir / "symdoc.log")))


class RenderInput(Result):
    run_dir: Path


class RenderOutput(Result):
    pass


@workflow.step
def render_symbols(given: RenderInput) -> RenderOutput:
    """Regenerate symbols/SYMBOLS.md and symbols/symdb.sqlite."""
    render_database(LogPath(given.run_dir / "render.log"))
    return RenderOutput()


class ReverifyInput(Result):
    run_dir: Path


class ReverifyOutput(Result):
    verification: VerifyReport


@workflow.step
def reverify_port(given: ReverifyInput) -> ReverifyOutput:
    """Renames change the port's layout, not its behaviour: replay every route to make sure (a child verify run)."""
    run_id = f"{current_run_id()}-verify"
    run_dir = given.run_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    start = VerifyState(run_dir=run_dir, functions=(), snapshots=None, routes=ALL_ROUTES, battle_code=False)
    return ReverifyOutput(verification=run_verify(start, run_id))


class CommitInput(Result):
    commit_message: str


class CommitOutput(Result):
    commit: CommitHash


@workflow.step
def commit_records(given: CommitInput) -> CommitOutput:
    """Commit the database, the bn6f submodule pointer and the renamed port code locally."""
    return CommitOutput(commit=commit_files(changed_files(RECORDED_PATHS), given.commit_message))


class ReportInput(Result):
    problems: tuple[ToolMessage, ...]
    renamed_in_bn6f: CommitHash | None
    comments_in_bn6f: CommitHash | None
    verification: VerifyReport | None
    commit: CommitHash | None


class ReportOutput(Result):
    report: RecordReport


@workflow.step
def write_report(given: ReportInput) -> ReportOutput:
    """Say what was recorded, or why nothing was."""
    if given.problems:
        verdict = RecordVerdict.DATABASE_PROBLEMS
    elif given.commit is None:
        verdict = RecordVerdict.PORT_BROKE
    else:
        verdict = RecordVerdict.RECORDED
    return ReportOutput(report=RecordReport(verdict=verdict, problems=given.problems,
                                            renamed_in_bn6f=given.renamed_in_bn6f,
                                            comments_in_bn6f=given.comments_in_bn6f,
                                            verification=given.verification, commit=given.commit))


# ── decisions ──────────────────────────────────────────────────────────

def problems_found(state: RecordState) -> bool:
    return bool(state.problems)


def clean_with_renames(state: RecordState) -> bool:
    return not state.problems and bool(state.renames)


def clean_without_renames(state: RecordState) -> bool:
    return not state.problems and not state.renames


def symbols_were_renamed(state: RecordState) -> bool:
    return state.renamed_in_bn6f is not None


def nothing_was_renamed(state: RecordState) -> bool:
    return state.renamed_in_bn6f is None


def port_still_verified(state: RecordState) -> bool:
    return state.verification is not None and state.verification.verdict in ACCEPTED_VERDICTS


def port_no_longer_verified(state: RecordState) -> bool:
    return not port_still_verified(state)
