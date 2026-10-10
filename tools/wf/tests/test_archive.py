"""The archive of cited runs: the text logs of every run symbols/db names are copied (a child
run's through its parent), binary files and lockstep's digests are not, a rerun copies nothing
new, and a cited run that is nowhere is reported."""
from pathlib import Path

import pytest

from wf.tools import archive
from wf.tools.archive import archive_cited_runs

KEPT = ("build.log", "convert-20261009-003052-verify-2/oracle.log", "convert-20261009-003052-verify-2/oracle/report.txt",
        "convert-20261009-003052-verify-2/lockstep-wander/first_bad", "review.json", "difftest.cfg", "summary.md")
LEFT = ("convert-20261009-003052-verify-2/lockstep-wander/orig.txt", "snapshots/f/snap_0.bin", "shot.ppm")


@pytest.fixture
def runs(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> Path:
    database, runs = tmp_path / "db", tmp_path / "runs"
    database.mkdir()
    _ = (database / "f.yaml").write_text("detail: run convert-20261009-003052\nresult: ok (verify run "
                                         "convert-20261009-003052-verify-2)\nalso: run convert-20261001-000000\n")
    for name in (*KEPT, *LEFT):
        path = runs / "convert-20261009-003052" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        _ = path.write_text(name)
    (runs / "farm-batch-20261010-151951").mkdir()   # not cited
    _ = (runs / "farm-batch-20261010-151951" / "build.log").write_text("x")
    monkeypatch.setattr(archive, "DATABASE", database)
    monkeypatch.setattr(archive, "RUNS", runs)
    monkeypatch.setattr(archive, "ARCHIVE", tmp_path / "archive")
    return runs


def test_the_text_logs_of_cited_runs_are_kept(runs: Path) -> None:
    report = archive_cited_runs()
    kept = archive.ARCHIVE / "convert-20261009-003052"
    assert sorted(str(path.relative_to(kept)) for path in kept.rglob("*") if path.is_file()) == sorted(KEPT)
    assert not (archive.ARCHIVE / "farm-batch-20261010-151951").exists()
    assert report.copied_from == ("convert-20261009-003052",) and report.files == len(KEPT)
    assert report.nowhere == ("convert-20261001-000000",)
    assert report.archive_files == len(KEPT)


def test_a_rerun_copies_only_what_changed(runs: Path) -> None:
    _ = archive_cited_runs()
    assert archive_cited_runs().files == 0
    _ = (runs / "convert-20261009-003052" / "build.log").write_text("a longer build log")
    assert archive_cited_runs().files == 1


def test_a_dry_run_copies_nothing(runs: Path) -> None:
    assert archive_cited_runs(dry_run=True).files == len(KEPT)
    assert not archive.ARCHIVE.exists()
