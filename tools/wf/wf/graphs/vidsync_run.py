"""Keep the vidsync sync going on the compute PCs (docs/vidsync/README.md): deploy this checkout's vidsync to
both PCs, restart the worker server and the driver from a checkpoint, then watch the driver's progress, the
furthest game frame its logs reached. Every hour the path so far is replayed against the video (pathcheck);
when it does not hold, the driver restarts from a clean checkpoint before its first loss. When the furthest
frame has not moved for 30 minutes (stuck: not log silence, the driver logs while it searches), or the driver
gave up at a loss, a vidsync-repair run is opened for that loss. Without --start the running driver is only
watched; --watch-hours ends the watch after that long whatever happens."""
from enum import Enum
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Paused, Transition, Workflow, current_run_id
from wf.graphs.vidsync_repair import VidsyncRepairState, vidsync_repair_graph
from wf.tools.vidsync import (DRIVER_PC, PCS, Checkpoint, DriverEnd, DriverStatus, PcDeploy, RemoteError, deploy,
                              driver_status, newest_checkpoint, next_run_name, now, pause, resolve_checkpoint,
                              restart_workers, run_job, run_names, start_driver)
from wf.types import FrameCount, LogPath, RemotePath, Result, RunName, ToolMessage

PROJECT = "recomp-vidsync"


def vidsync_run_graph() -> Graph["VidsyncRunState"]:
    return workflow.graph(
        first=check_driver,
        ends=[write_report],
        transitions=[
            Transition(after=check_driver, go_to=deploy_to_pcs, when=deploy_requested),
            Transition(after=check_driver, go_to=start_driver_from_checkpoint, when=restart_without_deploy),
            Transition(after=check_driver, go_to=watch_progress, when=only_watch),
            Transition(after=deploy_to_pcs, go_to=restart_worker_server, when=deployed_everywhere),
            Transition(after=deploy_to_pcs, go_to=write_report, when=deploy_failed),
            Transition(after=restart_worker_server, go_to=start_driver_from_checkpoint, when=restart_requested),
            Transition(after=restart_worker_server, go_to=watch_progress, when=no_restart_requested),
            Transition(after=start_driver_from_checkpoint, go_to=watch_progress),
            Transition(after=watch_progress, go_to=pathcheck_path, when=pathcheck_due),
            Transition(after=watch_progress, go_to=open_repair, when=stuck_and_repair_wanted),
            Transition(after=watch_progress, go_to=write_report, when=watch_over),
            Transition(after=pathcheck_path, go_to=watch_progress, when=path_holds),
            Transition(after=pathcheck_path, go_to=start_driver_from_checkpoint, when=path_broke),
            Transition(after=open_repair, go_to=write_report),
        ],
    )


def run_vidsync(start: "VidsyncRunState", run_id: str, tracked: bool = True) -> "VidsyncRunReport":
    outcome = vidsync_run_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"vidsync-run {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class WatchOutcome(Enum):
    STALLED = "the furthest frame has not moved for the stall time"
    DRIVER_ENDED = "the driver ended"
    PATHCHECK_DUE = "time to check the path against the video"
    WATCH_ENDED = "the watch time is over"


class DriverStart(Result):
    run: RunName
    start: Checkpoint
    at: float            # epoch seconds


class Pathcheck(Result):
    """pathcheck.py on a checkpoint: does the path hold against the video, replayed with plain steps."""
    checkpoint: Checkpoint
    holds: bool
    first_loss: FrameCount | None
    clean_start: Checkpoint | None    # when it does not hold: a checkpoint before the first loss to restart from
    log_tail: tuple[ToolMessage, ...]


class VidsyncRunReport(Result):
    outcome: WatchOutcome | None
    status: DriverStatus | None
    deployed: tuple[PcDeploy, ...]
    started: tuple[DriverStart, ...]
    pathchecks: tuple[Pathcheck, ...]
    repair_run: str | None
    repair_waiting_at: str | None     # the repair's pause point, when it stopped at one


class VidsyncRunState(BaseModel):
    run_dir: Path
    deploy: bool = True
    start_from: RemotePath | None = None    # restart the driver from this checkpoint, or a run's newest (a run
                                            # name); None: watch the one running
    run: RunName | None = None              # with start_from: the new run's name (None: the next after the last
                                            # run's); without: the run to watch (None: the one started last)
    replay: RemotePath | None = None        # VIDSYNC_REPLAY for the driver (a path stepped as it is)
    poll_minutes: float = 5
    stall_minutes: float = 30
    pathcheck_minutes: float = 60
    watch_hours: float | None = None        # None: until the driver is stuck or ends
    repair: bool = True                     # open a vidsync-repair run when it is stuck
    watch_until: float | None = None
    status: DriverStatus | None = None
    deployed: tuple[PcDeploy, ...] = ()
    workers: int = 0
    started: tuple[DriverStart, ...] = ()
    furthest: FrameCount | None = None
    furthest_since: float | None = None     # when the furthest frame last moved (epoch seconds)
    last_pathcheck_at: float | None = None
    pathchecks: tuple[Pathcheck, ...] = ()
    outcome: WatchOutcome | None = None
    repair_run: str | None = None
    repair_waiting_at: str | None = None
    report: VidsyncRunReport | None = None


workflow = Workflow[VidsyncRunState](VidsyncRunState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class CheckInput(Result):
    run: RunName | None
    start_from: RemotePath | None
    watch_hours: float | None


class CheckOutput(Result):
    status: DriverStatus
    run: RunName | None
    watch_until: float | None


@workflow.step
def check_driver(given: CheckInput) -> CheckOutput:
    """Read the driver's state on the driver PC (and so that it answers): the run to watch (named, or the one
    started last), or the last one when a new run starts; and set when the watch ends."""
    watching = given.start_from is None
    status = driver_status(given.run if watching else None)
    return CheckOutput(status=status, run=(given.run or status.run) if watching else given.run,
                       watch_until=now() + given.watch_hours * 3600 if given.watch_hours is not None else None)


class DeployInput(Result):
    run_dir: Path


class DeployOutput(Result):
    deployed: tuple[PcDeploy, ...]


@workflow.step
def deploy_to_pcs(given: DeployInput) -> DeployOutput:
    """tools/vidsync/deploy.py: this checkout's tools/vidsync, mgba_run's source and bn6f's symbols to both PCs,
    built there (~/vidsync/recomp; their data stays in ~/vidsync/data)."""
    return DeployOutput(deployed=deploy(PCS, LogPath(given.run_dir / "deploy.log")))


class WorkersInput(Result):
    pass


class WorkersOutput(Result):
    workers: int


@workflow.step
def restart_worker_server(given: WorkersInput) -> WorkersOutput:
    """Restart the worker server on 192.168.1.167 from the deployed tree (through WMI); the driver reconnects."""
    return WorkersOutput(workers=restart_workers())


class StartInput(Result):
    start_from: RemotePath | None
    run: RunName | None
    status: DriverStatus | None
    replay: RemotePath | None
    started: tuple[DriverStart, ...]


class StartOutput(Result):
    started: tuple[DriverStart, ...]
    run: RunName
    status: DriverStatus
    furthest: FrameCount | None
    furthest_since: float | None
    start_from: RemotePath | None
    last_pathcheck_at: float | None


@workflow.step
def start_driver_from_checkpoint(given: StartInput) -> StartOutput:
    """Stop the driver and start a new run from the checkpoint (job_driver.sh through WMI), named after the
    last run (run7k -> run7l) unless named; the progress clock starts again."""
    assert given.start_from is not None
    checkpoint = resolve_checkpoint(given.start_from)
    taken = run_names()
    run = given.run if given.run and given.run not in taken else next_run_name(
        given.run or (given.status.run if given.status else RunName("run1")), taken)
    start_driver(run, checkpoint.path, given.replay)
    pause(60)
    status = driver_status(run)
    return StartOutput(started=(*given.started, DriverStart(run=run, start=checkpoint, at=now())), run=run,
                       status=status, furthest=None, furthest_since=now(), start_from=None, last_pathcheck_at=now())


class WatchInput(Result):
    run: RunName | None
    furthest: FrameCount | None
    furthest_since: float | None
    last_pathcheck_at: float | None
    watch_until: float | None
    poll_minutes: float
    stall_minutes: float
    pathcheck_minutes: float


class WatchOutput(Result):
    status: DriverStatus
    furthest: FrameCount | None
    furthest_since: float
    outcome: WatchOutcome


@workflow.step
def watch_progress(given: WatchInput) -> WatchOutput:
    """Read the driver's state every few minutes until it is stuck (the furthest game frame has not moved for
    the stall time), it ended, a pathcheck is due, or the watch is over."""
    furthest, since = given.furthest, given.furthest_since or now()
    last_pathcheck = given.last_pathcheck_at or now()
    unanswered = 0
    while True:
        try:
            status = driver_status(given.run)
        except RemoteError:      # the WireGuard link drops at times: give it the stall time to come back
            unanswered += 1
            if unanswered * given.poll_minutes >= given.stall_minutes:
                raise
            pause(given.poll_minutes * 60)
            continue
        unanswered = 0
        if status.furthest is not None and (furthest is None or status.furthest > furthest):
            furthest, since = status.furthest, now()
        outcome = watch_verdict(status, since, last_pathcheck, given.watch_until, given.stall_minutes,
                                given.pathcheck_minutes, now())
        if outcome is not None:
            return WatchOutput(status=status, furthest=furthest, furthest_since=since, outcome=outcome)
        pause(given.poll_minutes * 60)


def watch_verdict(status: DriverStatus, furthest_since: float, last_pathcheck: float, watch_until: float | None,
                  stall_minutes: float, pathcheck_minutes: float, at: float) -> WatchOutcome | None:
    """What the watch makes of the driver's state at time `at` (None: keep watching)."""
    if not status.alive:
        return WatchOutcome.DRIVER_ENDED
    if at - furthest_since >= stall_minutes * 60:
        return WatchOutcome.STALLED
    if watch_until is not None and at >= watch_until:
        return WatchOutcome.WATCH_ENDED
    if at - last_pathcheck >= pathcheck_minutes * 60:
        return WatchOutcome.PATHCHECK_DUE
    return None


class PathcheckInput(Result):
    run: RunName | None
    pathchecks: tuple[Pathcheck, ...]


class PathcheckOutput(Result):
    pathchecks: tuple[Pathcheck, ...]
    last_pathcheck_at: float
    start_from: RemotePath | None


@workflow.step
def pathcheck_path(given: PathcheckInput) -> PathcheckOutput:
    """Replay the run's newest checkpoint with plain steps against the video (pathcheck.py, a job on the driver
    PC). If it does not hold to its end, save a clean checkpoint 300 frames before the first loss
    (pathcheck.py --save) for the driver to restart from. A checkpoint checked already is not checked again."""
    assert given.run is not None
    checkpoint = newest_checkpoint(given.run)
    if checkpoint is None or any(check.checkpoint == checkpoint for check in given.pathchecks):
        return PathcheckOutput(pathchecks=given.pathchecks, last_pathcheck_at=now(), start_from=None)
    job = run_job(DRIVER_PC, f"pathcheck-{given.run}-{checkpoint.g}", "py.sh", "pathcheck.py", f"{checkpoint.path}.json")
    first_loss = pathcheck_first_loss(job.log_tail)
    clean = None
    if first_loss is not None:
        frame = FrameCount(max(first_loss - 300, 0))
        out = RemotePath(f"data/auto/{given.run}.clean_{frame}")
        saved = run_job(DRIVER_PC, f"clean-{given.run}-{frame}", "py.sh", "pathcheck.py", f"{checkpoint.path}.json",
                        "--save", str(frame), out)
        clean = Checkpoint(path=out, g=frame) if saved.ok else None
    check = Pathcheck(checkpoint=checkpoint, holds=job.ok and first_loss is None, first_loss=first_loss,
                      clean_start=clean, log_tail=job.log_tail[-5:])
    return PathcheckOutput(pathchecks=(*given.pathchecks, check), last_pathcheck_at=now(),
                           start_from=clean.path if clean else None)


def pathcheck_first_loss(lines: tuple[ToolMessage, ...]) -> FrameCount | None:
    """pathcheck.py's last line: "holds to the end pokes N" or "first loss G pokes N"."""
    for line in reversed(lines):
        if line.startswith("holds to the end"):
            return None
        if line.startswith("first loss "):
            return FrameCount(int(line.split()[2]))
    raise RuntimeError("pathcheck.py printed no verdict: " + " | ".join(lines[-5:]))


class RepairInput(Result):
    run_dir: Path
    status: DriverStatus | None


class RepairOutput(Result):
    repair_run: str
    repair_waiting_at: str | None


@workflow.step
def open_repair(given: RepairInput) -> RepairOutput:
    """Open a vidsync-repair run for the loss the driver is stuck on (a child run: it renders and classifies
    the loss, then waits for Claude)."""
    assert given.status is not None
    run_id = f"{current_run_id()}-repair"
    run_dir = given.run_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    outcome = vidsync_repair_graph().start(VidsyncRepairState(run_dir=run_dir, run=given.status.run), run_id)
    return RepairOutput(repair_run=run_id, repair_waiting_at=outcome.step if isinstance(outcome, Paused) else None)


class ReportInput(Result):
    outcome: WatchOutcome | None
    status: DriverStatus | None
    deployed: tuple[PcDeploy, ...]
    started: tuple[DriverStart, ...]
    pathchecks: tuple[Pathcheck, ...]
    repair_run: str | None
    repair_waiting_at: str | None


class ReportOutput(Result):
    report: VidsyncRunReport


@workflow.step
def write_report(given: ReportInput) -> ReportOutput:
    """Where the driver is: its run, the furthest frame, how the watch ended, and the repair opened."""
    return ReportOutput(report=VidsyncRunReport(
        outcome=given.outcome, status=given.status, deployed=given.deployed, started=given.started,
        pathchecks=given.pathchecks, repair_run=given.repair_run, repair_waiting_at=given.repair_waiting_at))


# ── decisions ──────────────────────────────────────────────────────────

def deploy_requested(state: VidsyncRunState) -> bool:
    return state.deploy


def restart_without_deploy(state: VidsyncRunState) -> bool:
    return not state.deploy and state.start_from is not None


def only_watch(state: VidsyncRunState) -> bool:
    return not state.deploy and state.start_from is None


def deployed_everywhere(state: VidsyncRunState) -> bool:
    return bool(state.deployed) and all(pc.ok for pc in state.deployed)


def deploy_failed(state: VidsyncRunState) -> bool:
    return not deployed_everywhere(state)


def restart_requested(state: VidsyncRunState) -> bool:
    return state.start_from is not None


def no_restart_requested(state: VidsyncRunState) -> bool:
    return state.start_from is None


def pathcheck_due(state: VidsyncRunState) -> bool:
    return state.outcome is WatchOutcome.PATHCHECK_DUE


def stuck_and_repair_wanted(state: VidsyncRunState) -> bool:
    return state.repair and is_stuck(state)


def watch_over(state: VidsyncRunState) -> bool:
    return not pathcheck_due(state) and not stuck_and_repair_wanted(state)


def is_stuck(state: VidsyncRunState) -> bool:
    """No progress for the stall time, or the driver gave up at a loss (not a crash or the video's end)."""
    return state.outcome is WatchOutcome.STALLED or (
        state.outcome is WatchOutcome.DRIVER_ENDED and state.status is not None
        and state.status.ended is DriverEnd.STUCK)


def path_holds(state: VidsyncRunState) -> bool:
    return state.start_from is None


def path_broke(state: VidsyncRunState) -> bool:
    return state.start_from is not None

