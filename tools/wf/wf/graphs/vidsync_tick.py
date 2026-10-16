"""One check-in on the vidsync sync, for a recurring prompt (every 15 minutes): the way the hand-run sync was
kept going (10-10..10-13), as a workflow. It reads the driver's state and the log around the loss it is on
(remote/digest.py), and sorts it (wf/tools/vidsync_triage.py):

- moving, or on a loss for less than 30 minutes: nothing to do; every hour the path is checked against the
  video (pathcheck), and a broken path restarts from a clean checkpoint before its first loss;
- the driver died without a traceback: restart from its newest checkpoint (at most twice in a row);
- a loop, a suspect fix, a crash with a traceback, an internal error, 30 minutes on one loss, or the driver gave
  up: the loss is rendered and the run stops at `decide`, for Claude to look and choose one Decision:
  wait (snooze this loss), restart from a checkpoint (after a code change: deploy first; a suspect fix:
  the checkpoint before it), open a vidsync-repair run with the loss already classified (a new handler), or
  escalate to the user (time box spent, or the fix needs a decision only the user can make).

A check-in never starts a second decision while one waits: it reports the waiting run instead. What carries
from one check-in to the next (the last pathcheck, snoozed losses, crash restarts, the waiting run) is kept in
build-wf-state/vidsync-tick.json. Every check-in ends with one plain line: game frame, % of the video, fixes,
what it is doing."""
import json
from enum import Enum
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Paused, Transition, Workflow, current_run_id
from wf.graphs.vidsync_repair import VidsyncRepairState, vidsync_repair_graph
from wf.repo import STATE_DIR
from wf.tools.vidsync import (DRIVER_PC, PCS, Checkpoint, DriverStatus, LossCase, PcDeploy, deploy,
                              driver_status, find_loss, newest_checkpoint, next_run_name, now, render_loss,
                              resolve_checkpoint, restart_workers, run_job, run_names, start_driver)
from wf.tools.vidsync_brief import LossClass
from wf.tools.vidsync_learn import learn_from_run
from wf.tools.git import commit_files
from wf.repo import REPO
from wf.tools.vidsync_triage import LossDigest, Triage, TriageKind, fix_frame, loss_digest, triage
from wf.types import FrameCount, LogPath, RemotePath, Result, RunName, ToolMessage

PROJECT = "recomp-vidsync"
VIDEO_FRAMES = 1903801
MEMORY = STATE_DIR / "vidsync-tick.json"
PATHCHECK_MINUTES = 60
LEARN_MINUTES = 60
CRASH_RESTARTS = 2


def vidsync_tick_graph() -> Graph["VidsyncTickState"]:
    return workflow.graph(
        first=check_in,
        ends=[write_report],
        transitions=[
            Transition(after=check_in, go_to=write_report, when=decision_waiting),
            Transition(after=check_in, go_to=restart_after_crash, when=crashed_plainly),
            Transition(after=check_in, go_to=render_for_decision, when=needs_decision),
            Transition(after=check_in, go_to=pathcheck_if_due, when=routine),
            Transition(after=restart_after_crash, go_to=write_report),
            Transition(after=pathcheck_if_due, go_to=learn_if_due),
            Transition(after=learn_if_due, go_to=write_report),
            Transition(after=render_for_decision, go_to=decide),
            Transition(after=decide, go_to=act_on_decision),
            Transition(after=act_on_decision, go_to=write_report),
        ],
    )


def run_vidsync_tick(start: "VidsyncTickState", run_id: str, tracked: bool = True) -> Finished["VidsyncTickState"] | Paused:
    return vidsync_tick_graph().start(start, run_id, tracked=tracked)


# ── what carries between check-ins ─────────────────────────────────────

class TickMemory(BaseModel):
    last_pathcheck_at: float = 0
    last_learn_at: float = 0
    learned_fixes: list[str] = []          # fixes whose writes are in symbols/db already (run:dir:n)
    pathchecked: list[str] = []            # checkpoints already replayed against the video
    snoozed: dict[str, float] = {}         # loss (game frame) -> until when it is not brought up again
    crash_restarts: int = 0                # restarts in a row after a crash with no traceback (0 once alive)
    waiting: str | None = None             # a check-in run waiting at `decide`
    history: list[str] = []                # the last check-ins' lines


def load_memory(path: Path = MEMORY) -> TickMemory:
    return TickMemory.model_validate_json(path.read_text()) if path.exists() else TickMemory()


def save_memory(memory: TickMemory, path: Path = MEMORY) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    _ = path.write_text(memory.model_dump_json(indent=1))


# ── Claude's decision ──────────────────────────────────────────────────

class Action(Enum):
    WAIT = "wait"            # leave the driver searching; this loss is not brought up again for snooze_minutes
    RESTART = "restart"      # restart the driver from `checkpoint` (deploy first when code changed)
    REPAIR = "repair"        # a new handler: a vidsync-repair run, the loss classified already (loss_class)
    ESCALATE = "escalate"    # stop bringing it up; the report asks the user (note says what to decide)


class Decision(Result):
    """What Claude makes of a loss after looking at the pictures and the evidence. A change to tools/vidsync
    (a handler's threshold, a driver bug) is committed first, then RESTART with deploy=True."""
    action: Action
    note: str = ""                              # what differs and why this action (goes into the report)
    checkpoint: RemotePath | None = None        # RESTART: a checkpoint path, or a run's name (its newest);
                                                # None with a suspect fix: the newest before that fix
    deploy: bool = False                        # RESTART: deploy this checkout to both PCs first
    loss_class: LossClass | None = None         # REPAIR
    snooze_minutes: float = 60                  # WAIT (and ESCALATE: how long not to bring it up)


# ── the run's state ────────────────────────────────────────────────────

class TickReport(Result):
    line: str                                   # the check-in's one line
    triage: Triage | None
    decision: Decision | None
    restarted_as: RunName | None
    repair_run: str | None
    pathcheck: str | None


class VidsyncTickState(BaseModel):
    run_dir: Path
    run: RunName | None = None                  # the run to look at (None: the one started last)
    memory_path: Path = MEMORY
    memory: TickMemory | None = None
    status: DriverStatus | None = None
    digest: LossDigest | None = None
    triage: Triage | None = None
    case: LossCase | None = None
    pictures: tuple[Path, ...] = ()
    decision: Decision | None = None
    restarted_as: RunName | None = None
    deployed: tuple[PcDeploy, ...] = ()
    repair_run: str | None = None
    pathcheck: str | None = None
    learned: str | None = None
    report: TickReport | None = None


workflow = Workflow[VidsyncTickState](VidsyncTickState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class CheckInInput(Result):
    run: RunName | None
    memory_path: Path


class CheckInOutput(Result):
    memory: TickMemory
    status: DriverStatus
    digest: LossDigest | None
    triage: Triage | None


@workflow.step
def check_in(given: CheckInInput) -> CheckInOutput:
    """The driver's state, the log digest of the loss it is on, and what they mean (vidsync_triage.triage).
    A loss snoozed by an earlier decision is not brought up again until its time is up; a decision still
    waiting is not duplicated."""
    memory = load_memory(given.memory_path)
    if memory.waiting is not None and vidsync_tick_graph().waiting(memory.waiting) is None:
        memory.waiting = None            # answered since
    status = driver_status(given.run)
    if status.alive:
        memory.crash_restarts = 0
    digest = loss_digest(status.run)
    sorted_ = triage(status, digest)
    if sorted_.needs_claude and sorted_.loss is not None and memory.snoozed.get(str(sorted_.loss), 0) > now():
        sorted_ = sorted_.model_copy(update={"needs_claude": False, "suggestion": "snoozed: " + sorted_.suggestion})
    return CheckInOutput(memory=memory, status=status, digest=digest, triage=sorted_)


class CrashInput(Result):
    memory: TickMemory
    status: DriverStatus


class CrashOutput(Result):
    memory: TickMemory
    restarted_as: RunName | None


@workflow.step
def restart_after_crash(given: CrashInput) -> CrashOutput:
    """The driver died with no traceback (supervise.sh gave up, the PC rebooted): start the next run from the
    newest checkpoint, at most twice in a row (a third crash before it is seen running is left for Claude)."""
    memory = given.memory.model_copy(deep=True)
    if memory.crash_restarts >= CRASH_RESTARTS:
        return CrashOutput(memory=memory, restarted_as=None)
    checkpoint = newest_checkpoint(given.status.run)
    if checkpoint is None:
        return CrashOutput(memory=memory, restarted_as=None)
    run = next_run_name(given.status.run, run_names())
    start_driver(run, checkpoint.path)
    memory.crash_restarts += 1
    return CrashOutput(memory=memory, restarted_as=run)


class PathcheckInput(Result):
    memory: TickMemory
    status: DriverStatus


class PathcheckOutput(Result):
    memory: TickMemory
    pathcheck: str | None
    restarted_as: RunName | None


@workflow.step
def pathcheck_if_due(given: PathcheckInput) -> PathcheckOutput:
    """Once an hour, the run's newest checkpoint replayed with plain steps against the video (pathcheck.py on
    the driver PC). When it does not hold, a clean checkpoint 300 frames before the first loss is saved
    (pathcheck.py --save) and the driver restarts from it."""
    memory = given.memory.model_copy(deep=True)
    if not given.status.alive or now() - memory.last_pathcheck_at < PATHCHECK_MINUTES * 60:
        return PathcheckOutput(memory=memory, pathcheck=None, restarted_as=None)
    memory.last_pathcheck_at = now()
    checkpoint = newest_checkpoint(given.status.run)
    if checkpoint is None or checkpoint.path in memory.pathchecked:
        return PathcheckOutput(memory=memory, pathcheck=None, restarted_as=None)
    memory.pathchecked = (memory.pathchecked + [checkpoint.path])[-50:]
    job = run_job(DRIVER_PC, f"pathcheck-{given.status.run}-{checkpoint.g}", "py.sh", "pathcheck.py",
                  f"{checkpoint.path}.json")
    first_loss = first_loss_of(job.log_tail)
    if first_loss is None:
        return PathcheckOutput(memory=memory, pathcheck=f"holds to {checkpoint.g}", restarted_as=None)
    frame = FrameCount(max(first_loss - 300, 0))
    out = RemotePath(f"data/auto/{given.status.run}.clean_{frame}")
    saved = run_job(DRIVER_PC, f"clean-{given.status.run}-{frame}", "py.sh", "pathcheck.py", f"{checkpoint.path}.json",
                    "--save", str(frame), out)
    if not saved.ok:
        return PathcheckOutput(memory=memory, pathcheck=f"lost at {first_loss}; no clean checkpoint", restarted_as=None)
    run = next_run_name(given.status.run, run_names())
    start_driver(run, out)
    return PathcheckOutput(memory=memory, pathcheck=f"lost at {first_loss}: restarted from {out}", restarted_as=run)


class LearnInput(Result):
    memory: TickMemory
    status: DriverStatus


class LearnOutput(Result):
    memory: TickMemory
    learned: str | None


@workflow.step
def learn_if_due(given: LearnInput) -> LearnOutput:
    """Once an hour, what the run's new fixes wrote goes into symbols/db as runtime evidence on the entries of
    the fields they wrote (wf/tools/vidsync_learn.py), committed; written addresses with no entry are listed
    in build-vidsync/learned.md for `record`. Nothing is renamed here."""
    memory = given.memory.model_copy(deep=True)
    if now() - memory.last_learn_at < LEARN_MINUTES * 60:
        return LearnOutput(memory=memory, learned=None)
    memory.last_learn_at = now()
    learned, seen = learn_from_run(given.status.run, set(memory.learned_fixes))
    memory.learned_fixes = sorted(seen)[-5000:]
    note = f"learned: {len(learned.entries)} entries got evidence, {len(learned.unnamed)} addresses unnamed"
    if learned.entries:
        commit_files(tuple(REPO / e for e in learned.entries),
                     f"symbols/db: runtime evidence from the vidsync sync's fixes (run {given.status.run}: "
                     f"{len(learned.entries)} entries)\n\nWritten by wf vidsync-tick's learn step "
                     "(wf/tools/vidsync_learn.py).")
    return LearnOutput(memory=memory, learned=note)


def first_loss_of(lines: tuple[ToolMessage, ...]) -> FrameCount | None:
    """pathcheck.py's verdict: "holds to the end pokes N" or "first loss G pokes N"."""
    for line in reversed(lines):
        if line.startswith("holds to the end"):
            return None
        if line.startswith("first loss "):
            return FrameCount(int(line.split()[2]))
    raise RuntimeError("pathcheck.py printed no verdict: " + " | ".join(lines[-5:]))


class RenderInput(Result):
    status: DriverStatus
    triage: Triage
    run_dir: Path


class RenderOutput(Result):
    case: LossCase | None
    pictures: tuple[Path, ...]


@workflow.step
def render_for_decision(given: RenderInput) -> RenderOutput:
    """The loss's pictures (the emulator beside the video before the loss, the video after it), made on the
    driver PC with the probes (remote/render.py) and copied here; none when there is no loss (a crash)."""
    if given.triage.loss is None:
        return RenderOutput(case=None, pictures=())
    case = find_loss(given.status.run, given.triage.loss)
    if case is None or case.checkpoint is None:
        return RenderOutput(case=case, pictures=())
    return RenderOutput(case=case, pictures=render_loss(case, given.run_dir))


class DecideRequest(Result):
    triage: Triage
    status: DriverStatus
    digest: LossDigest | None
    pictures: tuple[Path, ...]


class DecideOutput(Result):
    decision: Decision


@workflow.pause_point
def decide(given: DecideRequest, answer: Decision) -> DecideOutput:
    """Claude looks at the pictures, the triage and the digest (each handler's last line at the loss, the fixes
    there, the RNG search) and answers one Decision. Time box: a loss that has taken two hours of looking is
    escalated, not looked at again."""
    return DecideOutput(decision=answer)


class ActInput(Result):
    decision: Decision
    triage: Triage
    status: DriverStatus
    case: LossCase | None
    memory: TickMemory
    run_dir: Path


class ActOutput(Result):
    memory: TickMemory
    restarted_as: RunName | None
    deployed: tuple[PcDeploy, ...]
    repair_run: str | None


@workflow.step
def act_on_decision(given: ActInput) -> ActOutput:
    """Do what Claude decided: snooze the loss, restart (deploying first if asked; a suspect fix's restart goes
    from the newest checkpoint before it), open a vidsync-repair run past its classify step, or escalate."""
    memory = given.memory.model_copy(deep=True)
    memory.waiting = None
    decision, loss = given.decision, given.triage.loss
    if decision.action in (Action.WAIT, Action.ESCALATE) and loss is not None:
        memory.snoozed[str(loss)] = now() + decision.snooze_minutes * 60
        return ActOutput(memory=memory, restarted_as=None, deployed=(), repair_run=None)
    if decision.action is Action.RESTART:
        deployed = deploy(PCS, LogPath(given.run_dir / "deploy.log")) if decision.deploy else ()
        if deployed and not all(pc.ok for pc in deployed):
            raise RuntimeError(f"deploy failed: {[pc.output for pc in deployed if not pc.ok]}")
        if deployed:
            _ = restart_workers()
        checkpoint = restart_point(decision, given.triage, given.status.run)
        run = next_run_name(given.status.run, run_names())
        start_driver(run, checkpoint.path)
        return ActOutput(memory=memory, restarted_as=run, deployed=deployed, repair_run=None)
    assert decision.action is Action.REPAIR and decision.loss_class is not None
    run_id = f"{current_run_id()}-repair"
    run_dir = given.run_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    _ = vidsync_repair_graph().start(VidsyncRepairState(run_dir=run_dir, run=given.status.run, loss=loss,
                                                       loss_class=decision.loss_class), run_id)
    if loss is not None:
        memory.snoozed[str(loss)] = now() + 6 * 3600     # the repair run has it now
    return ActOutput(memory=memory, restarted_as=None, deployed=(), repair_run=run_id)


def restart_point(decision: Decision, sorted_: Triage, run: RunName) -> Checkpoint:
    """The checkpoint a RESTART goes from: the one named, or before a suspect fix the newest checkpoint
    before its first write, or the run's newest."""
    if decision.checkpoint is not None:
        return resolve_checkpoint(decision.checkpoint)
    if sorted_.kind is TriageKind.SUSPECT_FIX and sorted_.suspect is not None:
        frame = fix_frame(sorted_.suspect)
        if frame is not None:
            before = newest_checkpoint(run, FrameCount(frame - 1))
            if before is not None:
                return before
    newest = newest_checkpoint(run)
    if newest is None:
        raise RuntimeError(f"{run} has no checkpoint to restart from")
    return newest


class ReportInput(Result):
    status: DriverStatus | None
    triage: Triage | None
    decision: Decision | None
    restarted_as: RunName | None
    repair_run: str | None
    pathcheck: str | None
    learned: str | None
    memory: TickMemory | None
    memory_path: Path


class ReportOutput(Result):
    report: TickReport
    memory: TickMemory | None


@workflow.step
def write_report(given: ReportInput) -> ReportOutput:
    """The check-in's one line (game frame, % of the video, fixes, what it is doing, what this check-in did),
    and what carries to the next check-in saved."""
    memory = (given.memory or load_memory(given.memory_path)).model_copy(deep=True)
    line = report_line(given.status, given.triage, given.decision, given.restarted_as, given.repair_run,
                       given.pathcheck, memory.waiting, given.learned)
    memory.history = (memory.history + [line])[-40:]
    save_memory(memory, given.memory_path)
    return ReportOutput(report=TickReport(line=line, triage=given.triage, decision=given.decision,
                                          restarted_as=given.restarted_as, repair_run=given.repair_run,
                                          pathcheck=given.pathcheck), memory=memory)


def report_line(status: DriverStatus | None, sorted_: Triage | None, decision: Decision | None,
                restarted: RunName | None, repair: str | None, pathcheck: str | None, waiting: str | None,
                learned: str | None = None) -> str:
    if status is None:
        return "no driver status"
    where = (f"{status.run} at {status.furthest} ({100 * (status.furthest or 0) / VIDEO_FRAMES:.1f}%), "
             f"{status.fixes} fixes")
    doing = sorted_.kind.value if sorted_ else "?"
    if sorted_ and sorted_.loss is not None and sorted_.kind is not TriageKind.PROGRESSING:
        doing += f" ({sorted_.why})"
    did: list[str] = []
    if waiting:
        did.append(f"a decision waits in {waiting}")
    if decision:
        did.append(f"decided {decision.action.value}" + (f": {decision.note}" if decision.note else ""))
    if restarted:
        did.append(f"restarted as {restarted}")
    if repair:
        did.append(f"repair opened: {repair}")
    if pathcheck:
        did.append(f"pathcheck {pathcheck}")
    if learned:
        did.append(learned)
    return where + "; " + doing + ("; " + "; ".join(did) if did else "")


# ── decisions ──────────────────────────────────────────────────────────

def decision_waiting(state: VidsyncTickState) -> bool:
    return state.memory is not None and state.memory.waiting is not None


def crashed_plainly(state: VidsyncTickState) -> bool:
    return (not decision_waiting(state) and state.triage is not None and state.triage.kind is TriageKind.CRASHED
            and not state.triage.needs_claude)


def needs_decision(state: VidsyncTickState) -> bool:
    return not decision_waiting(state) and state.triage is not None and state.triage.needs_claude


def routine(state: VidsyncTickState) -> bool:
    return not decision_waiting(state) and not crashed_plainly(state) and not needs_decision(state)


def note_waiting(run_dir: Path, run_id: str, memory_path: Path = MEMORY) -> None:
    """After a check-in stopped at `decide`: remember it, so the next check-in reports it instead of opening
    another (the CLI calls this when the run comes back Paused)."""
    memory = load_memory(memory_path)
    memory.waiting = run_id
    save_memory(memory, memory_path)
    _ = (run_dir / "waiting").write_text(json.dumps({"run": run_id}))
