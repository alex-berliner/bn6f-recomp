"""The vidsync-run graph with fake PCs on a fake clock: deploy, restart, watch; stuck means the furthest game
frame stopped moving (not that the log went quiet); a driver that gave up opens a repair; the hourly pathcheck
goes back to watching when the path holds and restarts from a clean checkpoint when it does not."""
from dataclasses import dataclass, field
from pathlib import Path

import pytest

from wf.burr_typed import Paused
from wf.graphs import vidsync_run
from wf.graphs.vidsync_run import (VidsyncRunReport, VidsyncRunState, WatchOutcome, pathcheck_first_loss,
                                   run_vidsync, watch_verdict)
from wf.tools.vidsync import Checkpoint, DriverEnd, DriverStatus, JobResult, PcDeploy, next_run_name
from wf.types import FrameCount, LogPath, PcHost, RemotePath, RunName, ToolMessage

START = RemotePath("data/auto/run6y.0/fix_0026")


def status(furthest: int, alive: bool = True, ended: DriverEnd | None = None, event: str = "", run: str = "run7l") -> DriverStatus:
    return DriverStatus(run=RunName(run), newest_dir=RemotePath(f"data/auto/{run}.0"), start=START, alive=alive,
                        ended=ended, furthest=FrameCount(furthest), fixes=0, last_event=ToolMessage(event), loss=None,
                        log_tail=())


@dataclass
class FakePcs:
    """What the PCs answer: the driver's status at each poll, in turn (the last one repeats)."""
    statuses: list[DriverStatus]
    deploy_ok: bool = True
    pathcheck_lines: list[str] = field(default_factory=lambda: ["holds to the end pokes 12"])
    checkpoints: list[Checkpoint] = field(default_factory=lambda: [Checkpoint(path=RemotePath("data/auto/run7l.0/ckpt_0435000"),
                                                                              g=FrameCount(435000))])
    clock: list[float] = field(default_factory=lambda: [0.0])
    called: list[str] = field(default_factory=list[str])
    started: list[tuple[str, str]] = field(default_factory=list[tuple[str, str]])
    repairs: list[str] = field(default_factory=list[str])
    jobs: list[str] = field(default_factory=list[str])

    def install(self, monkeypatch: pytest.MonkeyPatch) -> None:
        polls = iter(self.statuses)
        last: list[DriverStatus] = []

        def driver_status(run: RunName | None = None) -> DriverStatus:
            last[:] = [next(polls, last[0] if last else self.statuses[-1])]
            return last[0]

        def deploy(pcs: tuple[PcHost, ...], log: LogPath) -> tuple[PcDeploy, ...]:
            self.called.append("deploy")
            return tuple(PcDeploy(pc=pc, ok=self.deploy_ok, commit="abc", uncommitted=False, output=ToolMessage(""))
                         for pc in pcs)

        def restart_workers() -> int:
            self.called.append("workers")
            return 16

        def start_driver(run: RunName, start: RemotePath, replay: RemotePath | None = None) -> None:
            self.called.append("driver")
            self.started.append((run, start))

        def resolve_checkpoint(spec: RemotePath) -> Checkpoint:
            return Checkpoint(path=spec, g=FrameCount(432312))

        def newest_checkpoint(run: RunName | RemotePath, most: FrameCount | None = None) -> Checkpoint | None:
            return self.checkpoints[0]

        def run_job(pc: PcHost, name: str, *script: str, timeout_hours: float = 6,
                    poll_seconds: float = 30) -> JobResult:
            self.called.append(name.split("-")[0])
            self.jobs.append(name)
            lines = self.pathcheck_lines if name.startswith("pathcheck") else ["saved"]
            return JobResult(name=name, exit_code=0, log_tail=tuple(ToolMessage(line) for line in lines))

        def run_names() -> tuple[RunName, ...]:
            return (RunName("run7j"), RunName("run7k"), *(RunName(run) for run, _ in self.started))

        def now() -> float:
            return self.clock[0]

        def pause(seconds: float) -> None:
            self.clock[0] += seconds

        class FakeRepair:
            def start(self_, start: object, run_id: str) -> Paused:   # pyright: ignore[reportSelfClsParameterName]
                self.repairs.append(run_id)
                return Paused(run_id=run_id, step="classify_the_loss", request=status(0), answer_type=DriverStatus)

        for name, fake in (("driver_status", driver_status), ("deploy", deploy), ("restart_workers", restart_workers),
                           ("start_driver", start_driver), ("resolve_checkpoint", resolve_checkpoint),
                           ("newest_checkpoint", newest_checkpoint), ("run_job", run_job), ("run_names", run_names),
                           ("now", now), ("pause", pause), ("vidsync_repair_graph", lambda: FakeRepair())):
            monkeypatch.setattr(vidsync_run, name, fake)


def run_with(pcs: FakePcs, monkeypatch: pytest.MonkeyPatch, tmp_path: Path, **options: object) -> VidsyncRunReport:
    pcs.install(monkeypatch)
    start = VidsyncRunState.model_validate({"run_dir": tmp_path, **options})
    return run_vidsync(start, run_id="vidsync-run-test", tracked=False)


def test_it_deploys_restarts_and_names_the_next_run(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(statuses=[status(432312, run="run7k"), *(status(432312 + 100 * k) for k in range(20))])
    report = run_with(pcs, monkeypatch, tmp_path, start_from=START, watch_hours=0.5, poll_minutes=5)

    assert pcs.called[:3] == ["deploy", "workers", "driver"]
    assert pcs.started == [("run7l", START)]
    assert report.outcome is WatchOutcome.WATCH_ENDED


def test_stuck_means_the_furthest_frame_stopped_moving(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    # the driver keeps logging (a new last event each poll) but gets no further than 432728
    busy = [status(432728, event=f"W=432420: |j| <= {300 * k}, furthest 432728") for k in range(30)]
    pcs = FakePcs(statuses=[status(432312, run="run7k"), status(432500), *busy])
    report = run_with(pcs, monkeypatch, tmp_path, start_from=START, deploy=False, poll_minutes=5, repair=False,
                      pathcheck_minutes=600)

    assert report.outcome is WatchOutcome.STALLED
    assert pcs.clock[0] >= 30 * 60 and report.repair_run is None


def test_a_driver_that_gave_up_opens_a_repair(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(statuses=[status(432728, run="run7k", alive=False, ended=DriverEnd.STUCK)])
    report = run_with(pcs, monkeypatch, tmp_path, deploy=False)

    assert report.outcome is WatchOutcome.DRIVER_ENDED
    assert report.repair_run == "vidsync-run-test-repair" and report.repair_waiting_at == "classify_the_loss"
    assert "driver" not in pcs.called          # only watched: nothing restarted


def test_a_crashed_driver_is_reported_not_repaired(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(statuses=[status(432728, run="run7k", alive=False, ended=DriverEnd.CRASHED)])
    report = run_with(pcs, monkeypatch, tmp_path, deploy=False)

    assert report.outcome is WatchOutcome.DRIVER_ENDED and pcs.repairs == []


def test_a_path_that_holds_goes_back_to_watching(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(statuses=[status(432312, run="run7k"), *(status(432312 + 50 * k) for k in range(100))])
    report = run_with(pcs, monkeypatch, tmp_path, start_from=START, deploy=False, watch_hours=2.5, poll_minutes=10)

    assert len(report.pathchecks) == 1 and report.pathchecks[0].holds   # the same checkpoint is not checked twice
    assert report.outcome is WatchOutcome.WATCH_ENDED and len(report.started) == 1


def test_a_path_that_broke_restarts_from_a_clean_checkpoint(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(statuses=[status(432312, run="run7k"), *(status(432312 + 50 * k) for k in range(100))],
                  pathcheck_lines=["LOST at game 433100 (video 441000)", "first loss 433100 pokes 12"])
    report = run_with(pcs, monkeypatch, tmp_path, start_from=START, deploy=False, watch_hours=1.5, poll_minutes=10)

    assert report.pathchecks[0].first_loss == 433100
    assert pcs.started[1] == ("run7m", "data/auto/run7l.clean_432800")


def test_watching_alone_checks_the_running_runs_path(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(statuses=[status(432312 + 50 * k, run="run7k") for k in range(100)])
    report = run_with(pcs, monkeypatch, tmp_path, deploy=False, watch_hours=1.5, poll_minutes=10)

    assert pcs.jobs == ["pathcheck-run7k-435000"] and report.started == ()


def test_a_failed_deploy_starts_nothing(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(statuses=[status(432312, run="run7k")], deploy_ok=False)
    report = run_with(pcs, monkeypatch, tmp_path, start_from=START)

    assert pcs.called == ["deploy"] and report.outcome is None and not report.deployed[0].ok


def test_watch_verdicts() -> None:
    running = status(1000)
    assert watch_verdict(running, 0, 0, None, 30, 60, 29 * 60) is None
    assert watch_verdict(running, 0, 0, None, 30, 60, 30 * 60) is WatchOutcome.STALLED
    assert watch_verdict(running, 3000, 0, None, 30, 60, 61 * 60) is WatchOutcome.PATHCHECK_DUE
    assert watch_verdict(running, 1000, 0, 1100, 30, 60, 1200) is WatchOutcome.WATCH_ENDED
    assert watch_verdict(status(1000, alive=False), 1000, 1000, None, 30, 60, 1000) is WatchOutcome.DRIVER_ENDED


def test_run_names_follow_the_runs_own_sequence() -> None:
    taken = (RunName("run7l"),)
    assert next_run_name(RunName("run7k"), taken) == "run7m"
    assert next_run_name(RunName("run7z"), ()) == "run8a"
    assert next_run_name(RunName("run9"), ()) == "run9a"


def test_pathcheck_verdicts() -> None:
    assert pathcheck_first_loss((ToolMessage("game 500 mean 0.3"), ToolMessage("holds to the end pokes 3"))) is None
    assert pathcheck_first_loss((ToolMessage("first loss 1234 pokes 3"),)) == 1234
    with pytest.raises(RuntimeError):
        _ = pathcheck_first_loss((ToolMessage("Traceback"),))
