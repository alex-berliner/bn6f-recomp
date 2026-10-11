"""The shared work queue (review 3 Area 5.3-5): one long run that keeps every provider with quota busy on
the functions left to farm (captured calls, no passing candidate), each routed by its size to one prompt
and an order of models (wf/tools/work_queue.py ROUTES: the old prompt at 11-30 lines, MiniMax-M3 first; the
typed prompt elsewhere). Each provider has a lane of workers. A worker takes the next job whose next model is
its provider's (or, rather than idle, one where its model comes later), runs one farm run on it (one round
plus one repair, review 3 Area 2; runaways are stopped at their reasoning budget) and records the result in
the jobs' ledger, build-wf-state/queue-jobs.json, which every queue run shares. A job keeps its prompt
version, and ends when a candidate passes, every model of its route has tried, or it has spent its token
cap. A provider that is down (rate limit, spent credit, wf/tools/sources.py) takes no jobs until it is
back; Hyper's lane starts again at its daily reset. Functions other farms are on (their `wf farm` arguments)
are left alone. A shift ends every few hours: the difftest is rebuilt from the last commit and the work
planned again. `wf queue --stop` (or SIGTERM) lets the farm runs under way finish and ends the run."""
import contextvars
import threading
import time
import traceback
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.graphs.farm import FarmOutcome, FarmState, farm_graph
from wf.tools.build import build_host_difftest, committed_tree
from wf.tools.models import USAGE_LOG
from wf.tools.sources import down_until, is_source_failure
from wf.tools.work_queue import (CAP, GAME, LANES, STOP, Job, JobState, Lane, Ledger, after_run, busy_elsewhere,
                                 leftovers, pick, plan, read_ledger, recently_logged, tokens_spent, update_ledger,
                                 usage_offset)
from wf.types import BuildFailure, FunctionName, ModelSpec, Result, SnapshotDir

PROJECT = "recomp-queue"
REPLAN_SECONDS = 600     # the work is planned again this often within a shift (new captures, other farms)
IDLE_SECONDS = 60        # a worker with nothing to take looks again after this
DRAIN_SECONDS = 300      # at a shift's end, how long to wait for the farm runs under way before the next shift
                         # begins (slower ones finish in the background; their functions stay taken)


def queue_graph() -> Graph["QueueState"]:
    return queue.graph(
        first=build_difftest,
        ends=[summarise],
        transitions=[
            Transition(after=build_difftest, go_to=plan_work, when=difftest_built),
            Transition(after=build_difftest, go_to=summarise, when=difftest_did_not_build),
            Transition(after=plan_work, go_to=keep_every_provider_busy, when=there_is_work),
            Transition(after=plan_work, go_to=summarise, when=there_is_no_work),
            Transition(after=keep_every_provider_busy, go_to=build_difftest, when=the_shift_ended),
            Transition(after=keep_every_provider_busy, go_to=summarise, when=the_queue_is_done),
        ],
    )


# ── state ──────────────────────────────────────────────────────────────

class WorkPlan(Result):
    """What plan_work found."""
    open_jobs: int
    new_jobs: int
    retired_jobs: int
    busy_elsewhere: int                     # leftover functions other farms are on now
    by_route: tuple[str, ...]               # "11-20 old: 90 open", per size band


class LaneTally(Result):
    provider: str
    farm_runs: int
    accepted: int
    runaways: int           # runs that ended on a reply without code (stopped at the budget or not)
    provider_failures: int
    tokens: int


class ShiftReport(Result):
    began: str
    ended: str
    lanes: tuple[LaneTally, ...]
    accepted: tuple[FunctionName, ...]
    why_it_ended: str


class QueueState(BaseModel):
    run_dir: Path
    snapshots: SnapshotDir
    lanes: tuple[Lane, ...] = LANES
    rounds: int = 1                  # repairs per model (review 3 Area 2: one, then a fresh model)
    cap: int = CAP                   # tokens per job over all its models
    shift_hours: float = 3.0
    until: float | None = None       # epoch seconds: take no new jobs after this (`wf queue --hours`)
    leave_alone: tuple[Path, ...] = ()   # files listing functions another driver will farm (`--leave-alone`)
    difftest: Path | BuildFailure | None = None
    work: WorkPlan | None = None
    shifts: tuple[ShiftReport, ...] = ()
    done: bool = False               # told to stop, past `until`, or out of work
    totals: tuple[LaneTally, ...] = ()   # each provider's, over every shift


queue = Workflow[QueueState](QueueState, project=PROJECT)
_stop = threading.Event()
# the functions this process's farm runs are on, over every shift (a run left to finish after its shift ended
# keeps its function from the next shift)
_in_flight: set[FunctionName] = set()


def request_stop() -> None:
    """Take no new jobs; the run ends once the farm runs under way have finished (SIGTERM, `wf queue --stop`).
    Asked a second time, the run ends at once: the jobs under way stay open in the ledger and are taken again
    by the next queue."""
    if _stop.is_set():
        raise SystemExit("stopped at once: the jobs under way stay open")
    _stop.set()


def stop_requested() -> bool:
    return _stop.is_set() or STOP.exists()


# ── steps ──────────────────────────────────────────────────────────────

class BuildDifftestInput(Result):
    run_dir: Path
    shifts: tuple[ShiftReport, ...]


class BuildDifftestOutput(Result):
    difftest: Path | BuildFailure


@queue.step
def build_difftest(given: BuildDifftestInput) -> BuildDifftestOutput:
    """Build the host difftest from the last commit in a worktree of the queue's own (no farm run is under
    way: a shift starts with none)."""
    log_dir = given.run_dir / f"shift-{len(given.shifts)}"
    log_dir.mkdir(parents=True, exist_ok=True)
    # two build folders, shift by shift: a farm run still finishing from the last shift keeps its difftest
    return BuildDifftestOutput(difftest=build_host_difftest(log_dir, build_dir=f"build-wf-queue-host-{len(given.shifts) % 2}",
                                                            tree=committed_tree("queue")))


class PlanInput(Result):
    snapshots: SnapshotDir
    leave_alone: tuple[Path, ...]


class PlanOutput(Result):
    work: WorkPlan


@queue.step
def plan_work(given: PlanInput) -> PlanOutput:
    """Find the functions left to farm and give each a job (or keep its open one), with its route."""
    order, changed, busy = _plan(given.snapshots, given.leave_alone)
    by_band: dict[str, int] = {}
    for job in order:
        label = f"{job.band.value} {job.version}"
        by_band[label] = by_band.get(label, 0) + 1
    return PlanOutput(work=WorkPlan(
        open_jobs=len(order), new_jobs=sum(1 for job in changed if job.state is JobState.OPEN),
        retired_jobs=sum(1 for job in changed if job.state is JobState.RETIRED),
        busy_elsewhere=sum(1 for job in order if job.function in busy),
        by_route=tuple(f"{label}: {count} open" for label, count in sorted(by_band.items()))))


def _plan(snapshots: SnapshotDir, leave_alone: tuple[Path, ...]) -> tuple[list[Job], list[Job], set[FunctionName]]:
    """(open jobs in order, jobs that changed, functions to leave alone now: other farms' and the listed)."""
    order, changed = plan(read_ledger(), leftovers(GAME, snapshots))
    if changed:
        _ = update_ledger(changed)
    listed = {FunctionName(line.strip()) for path in leave_alone if path.exists()
              for line in path.read_text().splitlines() if line.strip()}
    return order, changed, busy_elsewhere() | listed


class WorkInput(Result):
    run_dir: Path
    snapshots: SnapshotDir
    lanes: tuple[Lane, ...]
    rounds: int
    cap: int
    shift_hours: float
    until: float | None
    leave_alone: tuple[Path, ...]
    difftest: Path
    shifts: tuple[ShiftReport, ...]


class WorkOutput(Result):
    shifts: tuple[ShiftReport, ...]
    done: bool


@queue.step
def keep_every_provider_busy(given: WorkInput) -> WorkOutput:
    """Run every lane's workers until the shift ends, the queue is told to stop or runs out of work; each
    farm run is a child run of this step."""
    board = _Board(given)
    began = time.strftime("%Y-%m-%d %H:%M:%S")
    workers = [threading.Thread(target=contextvars.copy_context().run, args=(board.work, lane),
                                name=f"{lane.provider}-{number}", daemon=True)
               for lane in given.lanes for number in range(lane.workers)]
    for worker in workers:
        worker.start()
    while True:   # every worker has finished; or the shift is over and the slow ones finish in the background
        alive = [worker for worker in workers if worker.is_alive()]
        if not alive or (board.shift_over() and board.drained_long_enough()):
            break
        alive[0].join(timeout=2)
    report = ShiftReport(began=began, ended=time.strftime("%Y-%m-%d %H:%M:%S"),
                         lanes=tuple(board.tallies[lane.provider].made() for lane in given.lanes),
                         accepted=tuple(board.accepted), why_it_ended=board.why_it_ended())
    return WorkOutput(shifts=(*given.shifts, report), done=board.done())


class SummariseInput(Result):
    shifts: tuple[ShiftReport, ...]


class SummariseOutput(Result):
    totals: tuple[LaneTally, ...]


@queue.step
def summarise(given: SummariseInput) -> SummariseOutput:
    """Each provider's farm runs, passes, runaways, failures and tokens over every shift
    (tools/convert/scorecard.py has the cost in plan units)."""
    totals: dict[str, _Tally] = {}
    for shift in given.shifts:
        for lane in shift.lanes:
            totals.setdefault(lane.provider, _Tally(lane.provider)).add(lane)
    return SummariseOutput(totals=tuple(tally.made() for tally in totals.values()))


# ── decisions ──────────────────────────────────────────────────────────

def difftest_built(state: QueueState) -> bool:
    return isinstance(state.difftest, Path)


def difftest_did_not_build(state: QueueState) -> bool:
    return isinstance(state.difftest, BuildFailure)


def there_is_work(state: QueueState) -> bool:
    return state.work is not None and state.work.open_jobs > 0 and not stop_requested()


def there_is_no_work(state: QueueState) -> bool:
    return not there_is_work(state)


def the_shift_ended(state: QueueState) -> bool:
    return not state.done


def the_queue_is_done(state: QueueState) -> bool:
    return state.done


# ── the workers ────────────────────────────────────────────────────────

class _Tally:
    def __init__(self, name: str) -> None:
        self.name = name
        self.runs = self.accepted = self.runaways = self.failures = self.tokens = 0

    def add(self, lane: LaneTally) -> None:
        self.runs += lane.farm_runs
        self.accepted += lane.accepted
        self.runaways += lane.runaways
        self.failures += lane.provider_failures
        self.tokens += lane.tokens

    def made(self) -> LaneTally:
        return LaneTally(provider=self.name, farm_runs=self.runs, accepted=self.accepted, runaways=self.runaways,
                         provider_failures=self.failures, tokens=self.tokens)


class _Board:
    """What a shift's workers share: the jobs in order, which are under way, and the tallies."""

    def __init__(self, given: WorkInput) -> None:
        self.given = given
        self.lock = threading.Lock()
        self.ends = time.time() + given.shift_hours * 3600
        self.order: list[Job] = []
        self.busy: set[FunctionName] = set()
        self.planned = 0.0
        self.in_flight = _in_flight
        self.over_since: float | None = None   # when the shift's end was first seen
        self.ours: dict[FunctionName, float] = {}   # our functions and when their last run ended
        self.tallies = {lane.provider: _Tally(lane.provider) for lane in given.lanes}
        self.accepted: list[FunctionName] = []
        self.out_of_work = False
        self.run_id = current_run_id()
        self.runs = 0

    # when the shift is over for a worker
    def past_until(self) -> bool:
        return self.given.until is not None and time.time() > self.given.until

    def finishing(self) -> bool:
        return stop_requested() or time.time() > self.ends or self.past_until() or self.out_of_work

    def shift_over(self) -> bool:
        """The shift's hours are up (not a stop: a stop waits for every farm run under way)."""
        over = time.time() > self.ends and not self.done()
        if over and self.over_since is None:
            self.over_since = time.time()
        return over

    def drained_long_enough(self) -> bool:
        return self.over_since is not None and time.time() - self.over_since > DRAIN_SECONDS

    def done(self) -> bool:
        return stop_requested() or self.past_until() or self.out_of_work

    def why_it_ended(self) -> str:
        if stop_requested():
            return "told to stop"
        if self.past_until():
            return "its hours were up"
        if self.out_of_work:
            return "no job left that a lane could take"
        return f"the shift's {self.given.shift_hours:g} hours were up"

    def take(self, lane: Lane) -> tuple[Job, ModelSpec] | None:
        with self.lock:
            if time.time() - self.planned > REPLAN_SECONDS:
                order, _, busy = _plan(self.given.snapshots, self.given.leave_alone)
                recent = recently_logged() - {name for name, ended in self.ours.items() if time.time() - ended < 900}
                self.order, self.busy, self.planned = order, busy | recent, time.time()
            up = {other.provider for other in self.given.lanes if provider_up(other.provider)}
            found = pick(self.order, lane.provider, self.in_flight, self.busy, up)
            if found is not None:
                self.in_flight.add(found[0].function)
                self.runs += 1
            elif not self.in_flight and all(job.function in self.busy for job in self.order):
                self.out_of_work = True   # every job of the last plan has ended, or is another farm's
            return found

    def work(self, lane: Lane) -> None:
        """One worker: take a job for the lane's provider, run it, record it; wait when there is none."""
        while not self.finishing():
            found = self.take(lane)
            if found is None:
                _ = _stop.wait(IDLE_SECONDS)
                continue
            job, model = found
            try:
                self.record(lane, job, model, *self.run(job, model))
            except Exception:   # a tool's bug in one farm run: kept in the run's log, counted against the job
                with open(self.given.run_dir / "errors.log", "a") as log:
                    _ = log.write(f"{time.strftime('%H:%M:%S')} {job.function} {model}\n{traceback.format_exc()}\n")
                self.record(lane, job, model, FarmOutcome.MODEL_ERROR, None, 0, 0)
            finally:
                with self.lock:
                    self.in_flight.discard(job.function)
                    self.ours[job.function] = time.time()

    def run(self, job: Job, model: ModelSpec) -> tuple[FarmOutcome, Path | None, int, int, bool]:
        """One farm run: (outcome, the accepted file, runaway rounds, tokens spent, whether it ended on a
        provider outage)."""
        offset = usage_offset(USAGE_LOG)
        run_id = f"{self.run_id}-{self.runs}-{job.function}-{model.replace('/', '_')}"
        start = FarmState(function=job.function, model=model, snapshots=self.given.snapshots,
                          difftest=self.given.difftest, rounds=self.given.rounds, prompt=job.prompt, source_wait=0)
        outcome = farm_graph().start(start, run_id)
        if not isinstance(outcome, Finished) or outcome.state.outcome is None:
            raise RuntimeError(f"farm run {run_id} did not finish")
        attempts = outcome.state.attempts
        runaways = sum(1 for attempt in attempts if attempt.reply.candidate is None and attempt.reply.error is None)
        last_error = attempts[-1].reply.error if attempts else None
        return (outcome.state.outcome, outcome.state.accepted, runaways,
                tokens_spent(USAGE_LOG, offset, job.function, model),
                last_error is not None and is_source_failure(last_error))

    def record(self, lane: Lane, job: Job, model: ModelSpec, outcome: FarmOutcome, accepted: Path | None,
               runaways: int, tokens: int, outage: bool = False) -> None:
        failed = outcome in (FarmOutcome.MODEL_ERROR, FarmOutcome.SOURCES_OUT)
        updated = after_run(job, model, accepted, provider_failed=failed, spent=tokens,
                            nothing_to_convert=outcome is FarmOutcome.NOTHING_TO_CONVERT, cap=self.given.cap,
                            outage=outage or outcome is FarmOutcome.SOURCES_OUT)
        _ = update_ledger([updated])
        with self.lock:
            self.order = [updated if other.function == job.function else other for other in self.order
                          if other.function != job.function or updated.state is JobState.OPEN]
            tally = self.tallies[lane.provider]
            tally.runs += 1
            tally.accepted += outcome is FarmOutcome.ACCEPTED
            tally.runaways += runaways
            tally.failures += failed
            tally.tokens += tokens
            if accepted is not None:
                self.accepted.append(job.function)


def provider_up(name: str) -> bool:
    """Whether the provider takes requests now (its recorded outage or its pacer's rate limit has lifted)."""
    return down_until(ModelSpec(f"{name}/any")) is None


def ledger_summary(ledger: Ledger) -> list[str]:
    """`wf queue --status`: the jobs by state and size band, the open ones by the model they go to next, and
    the tokens spent."""
    by_state: dict[str, int] = {}
    by_next: dict[str, int] = {}
    spent = 0
    for job in ledger.values():
        key = f"{job.state.value} ({job.band.value} {job.version})"
        by_state[key] = by_state.get(key, 0) + 1
        spent += job.spent
        if job.state is JobState.OPEN and job.remaining():
            by_next[job.remaining()[0]] = by_next.get(job.remaining()[0], 0) + 1
    lines = [f"{len(ledger)} jobs, {spent / 1e6:.1f}M tokens spent"]
    lines += [f"  {key}: {count}" for key, count in sorted(by_state.items())]
    lines += ["open jobs by their next model:"] + [f"  {model}: {count}" for model, count in sorted(by_next.items())]
    return lines

