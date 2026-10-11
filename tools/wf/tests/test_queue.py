"""The shared work queue: routing by size, a job keeping its prompt version, which job a provider's lane
takes, what a farm run does to a job, and the queue graph with fake farm runs."""
from pathlib import Path

import pytest

from wf.burr_typed import Finished
from wf.graphs import queue
from wf.graphs.farm import FarmOutcome, FarmState
from wf.graphs.queue import QueueState, queue_graph
from wf.tools import work_queue
from wf.tools.budgets import Band, PromptVersion
from wf.tools.models import PromptKind, prompt_version
from wf.tools.work_queue import (ROUTES, Job, JobState, Lane, Leftover, Route, after_run, pick, plan, read_ledger,
                                 tokens_spent)
from wf.types import BuildFailure, FunctionName, LogPath, ModelSpec, SnapshotDir, ToolMessage

M3 = ModelSpec("minimax/MiniMax-M3")
GLM = ModelSpec("hyper/glm-5.3-flash")
QWEN = ModelSpec("hyper/qwen3.8-flash")


def leftover(name: str, lines: int, attempts: int = 0) -> Leftover:
    return Leftover(function=FunctionName(name), lines=lines, attempts=attempts)


def job(name: str, models: tuple[ModelSpec, ...], tried: tuple[ModelSpec, ...] = ()) -> Job:
    return Job(function=FunctionName(name), prompt=PromptKind.OLD, version=PromptVersion("old"), lines=15,
               models=models, tried=tried)


def test_jobs_are_routed_by_size_to_one_prompt() -> None:
    order, changed = plan({}, [leftover("small", 15), leftover("tiny", 8), leftover("large", 45)])

    assert [j.function for j in order] == ["small", "tiny", "large"] and len(changed) == 3
    by_name = {j.function: j for j in order}
    assert by_name[FunctionName("small")].prompt is PromptKind.OLD and by_name[FunctionName("small")].models[0] == M3
    assert by_name[FunctionName("tiny")].prompt is PromptKind.TYPED
    assert by_name[FunctionName("large")].version == prompt_version(PromptKind.TYPED)
    assert ROUTES[Band.MEDIUM].prompt is PromptKind.OLD and ROUTES[Band.LARGE].prompt is PromptKind.TYPED


def test_an_open_job_keeps_its_prompt_and_a_retired_version_starts_a_new_job() -> None:
    kept = job("small", (M3, GLM), tried=(M3,))
    stale = Job(function=FunctionName("tiny"), prompt=PromptKind.TYPED, version=PromptVersion("typed-0"), lines=8,
                models=(GLM,))
    ended = job("done", (M3,), tried=(M3,)).model_copy(update={"state": JobState.EVERY_MODEL_TRIED})

    order, changed = plan({kept.function: kept, stale.function: stale, ended.function: ended},
                          [leftover("small", 15), leftover("tiny", 8), leftover("done", 15)])

    assert [j.function for j in order] == ["small", "tiny"]
    assert order[0] == kept   # same job, same prompt, its tries remembered
    assert order[1].version == prompt_version(PromptKind.TYPED) and order[1].tried == ()
    assert [(j.function, j.state) for j in changed] == [("tiny", JobState.RETIRED), ("tiny", JobState.OPEN)]


def test_a_lane_takes_its_own_jobs_first_then_others_rather_than_idle() -> None:
    m3_first = job("a", (M3, GLM))
    glm_first = job("b", (GLM, M3))
    order = [m3_first, glm_first]
    up = {"minimax", "hyper"}

    assert pick(order, "hyper", set(), set(), up) == (glm_first, GLM)
    assert pick(order, "hyper", {FunctionName("b")}, set(), up) == (m3_first, GLM)   # rather than idle
    assert pick(order, "minimax", set(), {FunctionName("a")}, up) == (glm_first, M3)  # a: another farm's
    assert pick(order, "zai", set(), set(), up) is None


def test_a_provider_that_is_down_is_skipped_in_the_order() -> None:
    order = [job("a", (M3, GLM))]

    assert pick(order, "hyper", set(), set(), {"hyper"}) == (order[0], GLM)   # M3 down: glm is next


def test_what_a_farm_run_does_to_its_job() -> None:
    start = job("a", (M3, GLM))

    assert after_run(start, M3, Path("a.c"), False, 5000).state is JobState.ACCEPTED
    failed = after_run(start, M3, None, False, 5000)
    assert failed.state is JobState.OPEN and failed.tried == (M3,) and failed.spent == 5000
    assert after_run(failed, GLM, None, False, 5000).state is JobState.EVERY_MODEL_TRIED
    assert after_run(start, M3, None, False, 70_000, cap=60_000).state is JobState.OVER_CAP
    broken = after_run(start, M3, None, True, 0)
    assert broken.state is JobState.OPEN and broken.tried == () and broken.errors == 1
    for _ in range(5):   # Hyper's hourly limit, again and again: never counts against the job
        start = after_run(start, M3, None, True, 0, outage=True)
    assert start.tried == () and start.errors == 0
    for _ in range(3):   # a model error of another kind, three times: the model is passed over
        broken = after_run(broken, M3, None, True, 0) if broken.errors < 3 else broken
    assert broken.tried == (M3,)


def test_tokens_spent_counts_the_function_and_models_calls(tmp_path: Path) -> None:
    log = tmp_path / "usage.jsonl"
    _ = log.write_text('{"model": "hyper/glm-5.3-flash", "tag": "old round 0", "in": 1, "out": 1}\n')
    offset = log.stat().st_size
    with open(log, "a") as out:
        _ = out.write('{"model": "hyper/glm-5.3-flash", "tag": "f round 0", "function": "f", "in": 100, "out": 50}\n'
                      '{"model": "hyper/glm-5.3-flash", "tag": "f round 1", "in": 10, "out": 5, "stopped": "x"}\n'
                      '{"model": "minimax/MiniMax-M3", "tag": "f round 0", "in": 7, "out": 7}\n'
                      '{"model": "hyper/glm-5.3-flash", "tag": "g round 0", "in": 9, "out": 9}\n')

    assert tokens_spent(log, offset, FunctionName("f"), ModelSpec("hyper/glm-5.3-flash+old")) == 165


# ── the graph ──────────────────────────────────────────────────────────

class FakeFarm:
    """farm_graph() for the queue: each run passes when its model is in PASSES."""

    def __init__(self, passes: set[ModelSpec]) -> None:
        self.passes = passes
        self.runs: list[tuple[FunctionName, ModelSpec, PromptKind, float]] = []

    def __call__(self) -> "FakeFarm":
        return self

    def start(self, start: FarmState, run_id: str) -> Finished[FarmState]:
        self.runs.append((start.function, start.model, start.prompt, start.source_wait))
        passed = start.model in self.passes
        return Finished(run_id=run_id, state=start.model_copy(update={
            "outcome": FarmOutcome.ACCEPTED if passed else FarmOutcome.OUT_OF_ROUNDS,
            "accepted": Path(f"{start.function}.c") if passed else None}))


def fake_queue(monkeypatch: pytest.MonkeyPatch, tmp_path: Path, found: list[Leftover], passes: set[ModelSpec],
               routes: dict[Band, Route]) -> FakeFarm:
    farm = FakeFarm(passes)
    monkeypatch.setattr(work_queue, "LEDGER", tmp_path / "queue-jobs.json")
    monkeypatch.setattr(queue, "STOP", tmp_path / "queue-stop")
    monkeypatch.setattr(queue, "IDLE_SECONDS", 0.01)
    monkeypatch.setattr(queue, "leftovers", lambda game, snapshots: found)  # pyright: ignore[reportUnknownLambdaType, reportUnknownArgumentType]
    monkeypatch.setattr(queue, "plan", lambda ledger, found: plan(ledger, found, routes))  # pyright: ignore[reportUnknownLambdaType, reportUnknownArgumentType]
    monkeypatch.setattr(queue, "busy_elsewhere", lambda: {FunctionName("elsewhere")})
    monkeypatch.setattr(queue, "recently_logged", lambda: set[FunctionName]())
    monkeypatch.setattr(queue, "tokens_spent", lambda log, offset, function, model: 1000)  # pyright: ignore[reportUnknownLambdaType, reportUnknownArgumentType]
    monkeypatch.setattr(queue, "committed_tree", lambda name: tmp_path / name)  # pyright: ignore[reportUnknownLambdaType, reportUnknownArgumentType]
    monkeypatch.setattr(queue, "build_host_difftest", lambda log_dir, build_dir, tree: tmp_path / "difftest")  # pyright: ignore[reportUnknownLambdaType, reportUnknownArgumentType]
    monkeypatch.setattr(queue, "farm_graph", farm)
    return farm


def test_the_queue_runs_each_job_down_its_route_and_ends_out_of_work(monkeypatch: pytest.MonkeyPatch,
                                                                     tmp_path: Path) -> None:
    routes = {band: Route(prompt=PromptKind.OLD, models=(M3, GLM)) for band in Band}
    farm = fake_queue(monkeypatch, tmp_path, [leftover("f", 15), leftover("g", 15), leftover("elsewhere", 15)],
                      passes={GLM}, routes=routes)
    start = QueueState(run_dir=tmp_path, snapshots=SnapshotDir(tmp_path),
                       lanes=(Lane(provider="minimax", workers=1), Lane(provider="hyper", workers=2)))

    outcome = queue_graph().start(start, run_id="queue-test")

    assert isinstance(outcome, Finished) and outcome.state.done
    # (which lane gets a job first depends on the threads: hyper may take one before minimax rather than idle)
    assert {function for function, _, _, _ in farm.runs} == {"f", "g"}
    assert all(model in (M3, GLM) for _, model, _, _ in farm.runs) and len(farm.runs) <= 4
    assert all(prompt is PromptKind.OLD and wait == 0 for _, _, prompt, wait in farm.runs)
    assert "elsewhere" not in {function for function, _, _, _ in farm.runs}   # another farm's function
    ledger = read_ledger()
    assert ledger[FunctionName("f")].state is JobState.ACCEPTED and ledger[FunctionName("g")].state is JobState.ACCEPTED
    assert outcome.state.shifts[0].why_it_ended == "no job left that a lane could take"
    assert sum(lane.accepted for lane in outcome.state.shifts[0].lanes) == 2


def test_a_stop_request_ends_the_queue_before_any_job(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    farm = fake_queue(monkeypatch, tmp_path, [leftover("f", 15)], passes=set(), routes=ROUTES)
    (tmp_path / "queue-stop").touch()
    start = QueueState(run_dir=tmp_path, snapshots=SnapshotDir(tmp_path))

    outcome = queue_graph().start(start, run_id="queue-stopped", tracked=False)

    assert isinstance(outcome, Finished) and farm.runs == [] and outcome.state.shifts == ()


def test_the_queue_stops_when_the_difftest_does_not_build(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    farm = fake_queue(monkeypatch, tmp_path, [leftover("f", 15)], passes=set(), routes=ROUTES)
    failure = BuildFailure(log=LogPath(tmp_path / "log"), last_lines=(ToolMessage("error"),))
    monkeypatch.setattr(queue, "build_host_difftest", lambda log_dir, build_dir, tree: failure)  # pyright: ignore[reportUnknownLambdaType, reportUnknownArgumentType]

    outcome = queue_graph().start(QueueState(run_dir=tmp_path, snapshots=SnapshotDir(tmp_path)), run_id="q",
                                  tracked=False)

    assert isinstance(outcome, Finished) and farm.runs == []


def test_a_job_counts_the_models_other_farms_asked_under_its_prompt_version(tmp_path: Path) -> None:
    (tmp_path / "f.minimax_MiniMax-M3+old.jsonl").write_text('{"round": 0, "ok": false, "report": "compile"}\n')
    (tmp_path / "f.hyper_glm-5.3-flash+old.jsonl").write_text('{"round": 0, "report": "model error: HTTP 429"}\n')
    (tmp_path / "f.hyper_qwen3.8-flash.jsonl").write_text('{"round": 0, "prompt_version": "typed-0"}\n')
    logs = tuple(tmp_path.glob("*.jsonl"))
    found = Leftover(function=FunctionName("f"), lines=15, attempts=3, logs=logs)
    routes = {band: Route(prompt=PromptKind.OLD, models=(M3, GLM, QWEN)) for band in Band}

    order, _ = plan({}, [found], routes)

    assert order[0].tried == (M3,)   # glm's only round was an outage; qwen's was another prompt
    later = order[0].model_copy(update={"tried": ()})
    order, changed = plan({later.function: later}, [found], routes)
    assert order[0].tried == (M3,) and changed == [order[0]]


def test_a_second_stop_request_ends_the_run_at_once() -> None:
    queue._stop.clear()  # pyright: ignore[reportPrivateUsage]
    queue.request_stop()
    with pytest.raises(SystemExit):
        queue.request_stop()
    queue._stop.clear()  # pyright: ignore[reportPrivateUsage]
