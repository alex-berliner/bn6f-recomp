"""vidsync-tick: the triage rules on real digests of the hand-run sync's incidents (tests/data/vidsync_digest:
run6l looped at 370434, run6u's kill and die only scraped past at 377151, run7k gave up at 432728, run7y is
searching), and the check-in's routes with fake PCs on a fake clock: routine check-ins only report (and
pathcheck once an hour), a plain crash restarts, a case for Claude stops at `decide` and is not duplicated, and
each Decision does what it says."""
import json
from dataclasses import dataclass, field
from pathlib import Path

import pytest

from wf.burr_typed import Finished, Paused
from wf.graphs import vidsync_tick
from wf.graphs.vidsync_tick import (Action, Decision, TickMemory, VidsyncTickState, load_memory, note_waiting,
                                    run_vidsync_tick, vidsync_tick_graph)
from wf.tools.vidsync import Checkpoint, DriverEnd, DriverStatus, JobResult, LossCase, PcDeploy
from wf.tools.vidsync_brief import HandlerName, LossClass, LossKind
from wf.tools.vidsync_learn import Learned
from wf.tools.vidsync_triage import FixRecord, LossDigest, TriageKind, triage
from wf.types import FrameCount, LogPath, PcHost, RemotePath, RunName, ToolMessage

DATA = Path(__file__).parent / "data" / "vidsync_digest"


def digest_of(name: str, **changes: object) -> LossDigest:
    raw: dict[str, object] = json.loads((DATA / f"{name}.json").read_text())   # pyright: ignore[reportAny]
    return LossDigest.model_validate({**raw, **changes})


def status(run: str, furthest: int, alive: bool = True, ended: DriverEnd | None = None, loss: int | None = None,
           event: str = "") -> DriverStatus:
    return DriverStatus(run=RunName(run), newest_dir=RemotePath(f"data/auto/{run}.0"), start=None, alive=alive,
                        ended=ended, furthest=FrameCount(furthest), fixes=7, last_event=ToolMessage(event),
                        loss=FrameCount(loss) if loss is not None else None, log_tail=())


# ── the rules ──────────────────────────────────────────────────────────

def test_many_fixes_at_one_loss_is_a_loop() -> None:
    result = triage(status("run6l", 370440, loss=370434), digest_of("run6l"))
    assert result.kind is TriageKind.LOOP and result.needs_claude
    assert "370434" in result.why


def test_a_kill_that_scraped_past_is_suspect_before_it_is_a_loop() -> None:
    result = triage(status("run6u", 377523, loss=377521), digest_of("run6u"))
    assert result.kind is TriageKind.SUSPECT_FIX
    assert result.suspect is not None and result.suspect.handler in ("kill", "die")


def test_a_driver_that_gave_up_needs_claude() -> None:
    result = triage(status("run7k", 432728, alive=False, ended=DriverEnd.STUCK), digest_of("run7k"))
    assert result.kind is TriageKind.STUCK and result.needs_claude
    assert any("map object" in line for line in result.evidence)


def test_a_fresh_loss_is_left_to_search() -> None:
    result = triage(status("run7y", 436362, loss=436362), digest_of("run7y", minutes_on_loss=4.0, recent_fixes=[]))
    assert result.kind is TriageKind.SEARCHING and not result.needs_claude


def test_half_an_hour_on_one_loss_is_stalled() -> None:
    result = triage(status("run7y", 436362, loss=436362), digest_of("run7y", minutes_on_loss=31.0, recent_fixes=[]))
    assert result.kind is TriageKind.STALLED and result.needs_claude


def test_a_crash_with_a_traceback_needs_claude_and_one_without_does_not() -> None:
    crashed = status("run6r", 372864, alive=False, ended=DriverEnd.CRASHED)
    assert triage(crashed, digest_of("run7y", traceback="IndexError: list index out of range")).needs_claude
    assert not triage(crashed, digest_of("run7y", traceback=None)).needs_claude


def test_run7ys_relocate_that_scraped_past_is_suspect() -> None:
    # live, 2026-10-16: fix 2 (relocate, mean cost 2.56) at 435999, then the loss at 436362
    result = triage(status("run7y", 436362, loss=436362), digest_of("run7y"))
    assert result.kind is TriageKind.SUSPECT_FIX and result.suspect is not None and result.suspect.handler == "relocate"


def test_a_poor_fix_followed_by_a_good_one_is_not_suspect() -> None:
    poor = FixRecord(n=24, at="11:50:00", handler="encounter", mean_cost=2.024, empty=False,
                     writes="write 0x2001c16=0x400/2 at game 482567; going on from there")
    good = FixRecord(n=26, at="11:51:00", handler="hand", mean_cost=1.152, empty=False,
                     writes="write 0x203cdb0=0x1/4 at game 482700; going on from there")
    result = triage(status("run7y", 484645, loss=484645), digest_of(
        "run7y", loss=484645, minutes_on_loss=4.0, recent_fixes=[poor.model_dump(), good.model_dump()]))
    assert result.kind is TriageKind.SEARCHING


def test_a_good_fix_is_not_suspect() -> None:
    good = FixRecord(n=1, at="00:00:13", handler="kill", mean_cost=0.84, empty=False,
                     writes="write 0x203aaac=0x0000/2 at game 370285; going on from there")
    result = triage(status("run6r", 371212, loss=371212),
                    digest_of("run7y", loss=371212, minutes_on_loss=2.0, recent_fixes=[good.model_dump()]))
    assert result.kind is TriageKind.SEARCHING


# ── the check-in ───────────────────────────────────────────────────────

@dataclass
class FakePcs:
    status: DriverStatus
    digest: LossDigest
    clock: list[float] = field(default_factory=lambda: [1_000_000.0])
    started: list[tuple[str, str]] = field(default_factory=list[tuple[str, str]])
    jobs: list[str] = field(default_factory=list[str])
    deployed: int = 0
    repairs: list[object] = field(default_factory=list[object])
    pathcheck_lines: list[str] = field(default_factory=lambda: ["holds to the end pokes 12"])
    learned: list[str] = field(default_factory=list[str])

    def install(self, monkeypatch: pytest.MonkeyPatch) -> None:
        def newest_checkpoint(run: RunName | RemotePath, most: FrameCount | None = None) -> Checkpoint | None:
            g = 370000 if most is None else min(most, 370000) - 5000
            return Checkpoint(path=RemotePath(f"data/auto/{run}.0/ckpt_{g:07d}"), g=FrameCount(g))

        def run_job(pc: PcHost, name: str, *script: str, timeout_hours: float = 6,
                    poll_seconds: float = 30) -> JobResult:
            self.jobs.append(name)
            lines = self.pathcheck_lines if name.startswith("pathcheck") else ["saved"]
            return JobResult(name=name, exit_code=0, log_tail=tuple(ToolMessage(line) for line in lines))

        def deploy(pcs: tuple[PcHost, ...], log: LogPath) -> tuple[PcDeploy, ...]:
            self.deployed += 1
            return tuple(PcDeploy(pc=pc, ok=True, commit="abc", uncommitted=False, output=ToolMessage(""))
                         for pc in pcs)

        def find_loss(run: RunName, loss: FrameCount | None = None) -> LossCase:
            return LossCase(run=run, loss=FrameCount(loss or 0), video=0, loss_json=RemotePath("x.json"), pokes=0,
                            checkpoint=Checkpoint(path=RemotePath("data/auto/x/ckpt"), g=FrameCount(0)), log=())

        class FakeRepair:
            def start(self_, start: object, run_id: str) -> Paused:   # pyright: ignore[reportSelfClsParameterName]
                self.repairs.append(start)
                return Paused(run_id=run_id, step="ask_model_for_a_handler", request=self.status,
                              answer_type=DriverStatus)

        def driver_status(run: RunName | None = None) -> DriverStatus:
            return self.status

        def loss_digest(run: RunName) -> LossDigest:
            return self.digest

        def resolve_checkpoint(spec: RemotePath) -> Checkpoint:
            return Checkpoint(path=spec, g=FrameCount(360000))

        def run_names() -> tuple[RunName, ...]:
            return (RunName("run7k"), RunName("run7y"))

        def start_driver(run: RunName, start: RemotePath, replay: RemotePath | None = None) -> None:
            self.started.append((run, start))

        def restart_workers() -> int:
            return 16

        def render_loss(case: LossCase, local_dir: Path) -> tuple[Path, ...]:
            return (local_dir / "compare.png",)

        def now() -> float:
            return self.clock[0]

        def repair_graph() -> FakeRepair:
            return FakeRepair()

        def learn_from_run(run: RunName, seen: set[str]) -> tuple[Learned, set[str]]:
            self.learned.append(run)
            return Learned(), seen

        fakes: dict[str, object] = {
            "driver_status": driver_status, "loss_digest": loss_digest, "newest_checkpoint": newest_checkpoint,
            "resolve_checkpoint": resolve_checkpoint, "run_job": run_job, "run_names": run_names,
            "start_driver": start_driver, "deploy": deploy, "restart_workers": restart_workers,
            "find_loss": find_loss, "render_loss": render_loss, "now": now, "vidsync_repair_graph": repair_graph,
            "learn_from_run": learn_from_run,
        }
        for name, fake in fakes.items():
            monkeypatch.setattr(vidsync_tick, name, fake)


def tick(pcs: FakePcs, monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> Finished[VidsyncTickState] | Paused:
    pcs.install(monkeypatch)
    start = VidsyncTickState(run_dir=tmp_path, memory_path=tmp_path / "memory.json")
    run_id = f"vidsync-tick-test-{len(list(tmp_path.glob('waiting*')))}-{pcs.clock[0]:.0f}"
    outcome = run_vidsync_tick(start, run_id)
    if isinstance(outcome, Paused):
        note_waiting(tmp_path, run_id, tmp_path / "memory.json")
    return outcome


def test_a_moving_sync_only_reports_and_pathchecks_once_an_hour(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(status("run7y", 436100), digest_of("run7y", loss=None, minutes_on_loss=None))
    first = tick(pcs, monkeypatch, tmp_path)
    assert isinstance(first, Finished) and first.state.report is not None
    assert "run7y at 436100 (22.9%)" in first.state.report.line and pcs.jobs == ["pathcheck-run7y-370000"]
    second = tick(pcs, monkeypatch, tmp_path)
    assert isinstance(second, Finished) and pcs.jobs == ["pathcheck-run7y-370000"]   # not again within the hour


def test_a_broken_path_restarts_from_a_clean_checkpoint(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(status("run7y", 436100), digest_of("run7y", loss=None, minutes_on_loss=None),
                  pathcheck_lines=["LOST at game 371262", "first loss 371262 pokes 164"])
    outcome = tick(pcs, monkeypatch, tmp_path)
    assert isinstance(outcome, Finished)
    assert pcs.started == [("run7z", "data/auto/run7y.clean_370962")]


def test_a_plain_crash_restarts_twice_at_most(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(status("run7y", 436100, alive=False, ended=DriverEnd.CRASHED), digest_of("run7y", traceback=None))
    for _ in range(3):
        _ = tick(pcs, monkeypatch, tmp_path)
        pcs.status = pcs.status.model_copy(update={"run": RunName("run7z")})
    assert len(pcs.started) == 2


def test_a_stall_stops_at_decide_once(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(status("run7y", 436362, loss=436362), digest_of("run7y", minutes_on_loss=45.0, recent_fixes=[]))
    outcome = tick(pcs, monkeypatch, tmp_path)
    assert isinstance(outcome, Paused) and outcome.step == "decide"
    again = tick(pcs, monkeypatch, tmp_path)
    assert isinstance(again, Finished) and again.state.report is not None
    assert "a decision waits in" in again.state.report.line


def answer(outcome: Paused, decision: Decision, tmp_path: Path) -> Finished[VidsyncTickState]:
    finished = vidsync_tick_graph().resume(outcome.run_id, decision)
    assert isinstance(finished, Finished)
    return finished


def test_wait_snoozes_the_loss(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(status("run7y", 436362, loss=436362), digest_of("run7y", minutes_on_loss=45.0, recent_fixes=[]))
    paused = tick(pcs, monkeypatch, tmp_path)
    assert isinstance(paused, Paused)
    _ = answer(paused, Decision(action=Action.WAIT, snooze_minutes=60, note="the RNG search is near"), tmp_path)
    memory = load_memory(tmp_path / "memory.json")
    assert memory.waiting is None and "436362" in memory.snoozed
    later = tick(pcs, monkeypatch, tmp_path)
    assert isinstance(later, Finished)          # snoozed: not brought up again within the hour


def test_restart_after_a_suspect_fix_goes_from_before_it(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(status("run6u", 377523, loss=377521), digest_of("run6u"))
    paused = tick(pcs, monkeypatch, tmp_path)
    assert isinstance(paused, Paused)
    done = answer(paused, Decision(action=Action.RESTART, deploy=True, note="the kill cut a battle short"), tmp_path)
    assert pcs.deployed == 1 and len(pcs.started) == 1
    _, start = pcs.started[0]
    assert start.endswith("ckpt_0365000")     # the newest checkpoint before the suspect fix's frame
    assert done.state.restarted_as is not None


def test_repair_opens_vidsync_repair_already_classified(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pcs = FakePcs(status("run7k", 432728, alive=False, ended=DriverEnd.STUCK), digest_of("run7k"))
    paused = tick(pcs, monkeypatch, tmp_path)
    assert isinstance(paused, Paused)
    loss_class = LossClass(kind=LossKind.PUZZLE, what_differs="the orb ring closes in the video", symbols=(),
                           routines=(), handler_name=HandlerName("weather_ring"))
    done = answer(paused, Decision(action=Action.REPAIR, loss_class=loss_class), tmp_path)
    assert done.state.repair_run is not None and len(pcs.repairs) == 1


def test_memory_round_trips(tmp_path: Path) -> None:
    path = tmp_path / "memory.json"
    vidsync_tick.save_memory(TickMemory(snoozed={"1": 2.0}), path)
    assert load_memory(path).snoozed == {"1": 2.0}
