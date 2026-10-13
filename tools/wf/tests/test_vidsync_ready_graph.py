"""The vidsync-ready graph with a fake driver PC: export, cut, check, copy; segments that leave the run stop it
before anything is copied; the import (which replays the route here) runs only when asked for."""
import json
from dataclasses import dataclass, field
from pathlib import Path

import pytest

from wf.graphs import vidsync_ready
from wf.graphs.vidsync_ready import ReadyReport, Segment, VidsyncReadyState, run_vidsync_ready, segment_check
from wf.tools.vidsync import Checkpoint, JobResult
from wf.types import FrameCount, PcHost, RemotePath, RunName, ToolMessage


def segment(n: int, start: int, end: int, differs: int | None = None, chain: bool = True) -> dict[str, object]:
    return {"segment": n, "run_from": start, "checkpoint_to": end, "run_to": end + 1000, "differs_at": differs,
            "teardown_save_is_next_start": chain}


@dataclass
class FakeDriverPc:
    segments: list[dict[str, object]]
    export_ok: bool = True
    jobs: list[str] = field(default_factory=list[str])
    commands: list[str] = field(default_factory=list[str])

    def install(self, monkeypatch: pytest.MonkeyPatch) -> None:
        def run_job(pc: PcHost, name: str, *script: str, timeout_hours: float = 6,
                    poll_seconds: float = 30) -> JobResult:
            self.jobs.append(" ".join(script))
            ok = self.export_ok or not name.startswith("export")
            return JobResult(name=name, exit_code=0 if ok else 1, log_tail=(ToolMessage("verify: agree"),))

        def fetch(pc: PcHost, path: RemotePath, local: Path) -> Path:
            local.parent.mkdir(parents=True, exist_ok=True)
            if path.endswith("segments.json"):
                _ = local.write_text(json.dumps(self.segments))
            else:   # the tarball of the recording
                import tarfile
                source = local.parent / "made"
                (source / "segments").mkdir(parents=True, exist_ok=True)
                _ = (source / "t.trace").write_text("# gbarecomp-keyinput-v1\n")
                with tarfile.open(local, "w:gz") as tar:
                    tar.add(source / "t.trace", arcname="t.trace")
            return local

        def on_pc(pc: PcHost, script: str, timeout: float = 600) -> str:
            self.commands.append(script)
            return ""

        def resolve_checkpoint(spec: RemotePath) -> Checkpoint:
            return Checkpoint(path=spec, g=FrameCount(12766))

        def newest_checkpoint(run: RunName | RemotePath, most: FrameCount | None = None) -> Checkpoint:
            return Checkpoint(path=RemotePath(f"data/auto/{run}.0/fix_0002"), g=FrameCount(most or 432800))

        for name, fake in (("run_job", run_job), ("fetch", fetch), ("on_pc", on_pc),
                           ("resolve_checkpoint", resolve_checkpoint), ("newest_checkpoint", newest_checkpoint)):
            monkeypatch.setattr(vidsync_ready, name, fake)


def ready_with(pc: FakeDriverPc, monkeypatch: pytest.MonkeyPatch, tmp_path: Path, **options: object) -> ReadyReport:
    pc.install(monkeypatch)
    start = VidsyncReadyState.model_validate({"run_dir": tmp_path, "local_ready": tmp_path / "ready", **options})
    return run_vidsync_ready(start, run_id="vidsync-ready-test", tracked=False)


def test_it_exports_cuts_checks_and_copies(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pc = FakeDriverPc(segments=[segment(0, 0, 6000), segment(1, 6000, 12766)])
    report = ready_with(pc, monkeypatch, tmp_path, checkpoint_spec="data/auto/run1/fix_0003", out_dir="data/ready-test",
                        name="test", every=6000)

    assert pc.jobs == ["export.sh data/auto/run1/fix_0003 data/ready-test test",
                       "segments.sh data/ready-test/test.result.json data/ready-test/segments 6000"]
    assert report.segments is not None and report.segments.segments == 2 and not report.segments.differing
    assert report.copied_to == tmp_path / "ready" and (tmp_path / "ready" / "t.trace").exists()
    assert report.imported is None and report.lockstep is None


def test_segments_that_leave_the_run_stop_it(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pc = FakeDriverPc(segments=[segment(0, 0, 20000), segment(1, 20000, 40000, differs=31234)])
    report = ready_with(pc, monkeypatch, tmp_path, checkpoint_spec="run7l", most=40000)

    assert report.checkpoint is not None and report.checkpoint.g == 40000
    assert report.segments is not None and report.segments.differing == (1,) and report.copied_to is None


def test_a_failed_export_cuts_nothing(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pc = FakeDriverPc(segments=[], export_ok=False)
    report = ready_with(pc, monkeypatch, tmp_path, checkpoint_spec="data/auto/run1/fix_0003")

    assert len(pc.jobs) == 1 and report.export is not None and not report.export.ok and report.segments is None


def test_long_segments_and_chain_breaks_are_reported() -> None:
    check = segment_check([Segment.model_validate(s) for s in (segment(0, 0, 20679), segment(1, 60207, 100104, chain=False),
                                                                 segment(2, 100104, 120000))], 30000)
    assert check.longer_than_spacing == (1,) and check.save_chain_breaks == (1,) and check.differing == ()
