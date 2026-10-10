"""Keep the text logs of the runs symbols/db cites (review 2 Area 1.4). Entries name runs as their
evidence ("run convert-20261009-003052", "verify run convert-20261009-003052-verify-2"), and a run
folder in build-wf-runs may be pruned one day, so its readable text is copied first to
build-wf-archive/RUN_ID/, which no cleanup touches (tools/never_delete.txt). Kept: the .md .log
.json .cfg files, the CPU oracle's reports (oracle/*.txt) and lockstep's first differing frame
(first_bad). Left behind: saved machine states, screenshots, and lockstep's frame digests
(orig.txt, test.txt: text, but ten times the size of the rest and only good for a rerun).

  ./wf archive [--dry-run]     # copies what is new or changed; safe to rerun
"""
import os
import re
import shutil
from pathlib import Path

from wf.repo import REPO, RUNS
from wf.tools.symbols import DATABASE
from wf.types import Result

ARCHIVE = REPO / "build-wf-archive"
# a run id: WORKFLOW-DATE-TIME[-N]; a child run's id starts with its parent's (convert-...-verify-2)
RUN_ID = re.compile(r"\b[a-z][a-z-]*-\d{8}-\d{6}(?:-\d+)?\b")
TEXT_SUFFIXES = frozenset({".md", ".log", ".json", ".cfg"})


class ArchiveReport(Result):
    cited: int                       # runs the db cites
    copied_from: tuple[str, ...]     # runs with files copied (or, in a dry run, to copy) now
    files: int
    bytes: int
    nowhere: tuple[str, ...]         # cited, but in neither build-wf-runs nor the archive
    archive_files: int               # the whole archive, after this
    archive_bytes: int


def cited_runs(database: Path) -> frozenset[str]:
    """The run ids symbols/db mentions (a child run counts as its parent: the parent's folder holds it)."""
    return frozenset(match.group(0) for path in database.glob("*.yaml") for match in RUN_ID.finditer(path.read_text()))


def is_text_log(path: Path) -> bool:
    return (path.suffix in TEXT_SUFFIXES or path.name == "first_bad"
            or (path.suffix == ".txt" and path.parent.name == "oracle"))


def archive_cited_runs(dry_run: bool = False) -> ArchiveReport:
    """Copy the text logs of every cited run into the archive (only what is new or changed)."""
    cited = cited_runs(DATABASE)
    copied_from: list[str] = []
    nowhere: list[str] = []
    files = size = 0
    for run in sorted(cited):
        source = RUNS / run
        if not source.is_dir():
            if not (ARCHIVE / run).is_dir():
                nowhere.append(run)
            continue
        copied = 0
        for path in _text_logs(source):
            target = ARCHIVE / run / path.relative_to(source)
            if _same(path, target):
                continue
            copied += 1
            size += path.stat().st_size
            if not dry_run:
                target.parent.mkdir(parents=True, exist_ok=True)
                _ = shutil.copy2(path, target)
        if copied:
            copied_from.append(run)
            files += copied
    archive = list(_text_logs(ARCHIVE)) if ARCHIVE.is_dir() else []
    return ArchiveReport(cited=len(cited), copied_from=tuple(copied_from), files=files, bytes=size,
                         nowhere=tuple(nowhere), archive_files=len(archive),
                         archive_bytes=sum(path.stat().st_size for path in archive))


def _text_logs(top: Path) -> list[Path]:
    """The text logs under a folder, not following links (a run may link to snapshots elsewhere)."""
    found: list[Path] = []
    for directory, _, names in os.walk(top):
        for name in names:
            path = Path(directory) / name
            if is_text_log(path) and path.is_file() and not path.is_symlink():
                found.append(path)
    return sorted(found)


def _same(source: Path, target: Path) -> bool:
    if not target.is_file():
        return False
    have, want = target.stat(), source.stat()
    return have.st_size == want.st_size and have.st_mtime_ns == want.st_mtime_ns
