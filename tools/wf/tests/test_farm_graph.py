"""The farm graphs with a fake model and a fake acceptance test: the repair
loop, giving up, and a batch skipping finished pairs."""
from pathlib import Path

import pytest

from wf.burr_typed import Finished
from wf.tools.budgets import Budget
from wf.graphs import farm
from wf.graphs.farm import FarmBatchState, FarmOutcome, FarmState, farm_batch_graph, farm_graph
from wf.tools.handover import Tried, tried
from wf.tools.models import (AssemblyText, CandidateC, CandidateTest, Contract, ConversionContext, LiftedC,
                             ModelReply, Prompt, PromptKind, base_model, prompt_of, with_prompt)
from wf.types import BuildFailure, FunctionName, GuestAddress, LogPath, ModelSpec, SnapshotDir, ToolMessage

FUNCTION = FunctionName("leafFunction")
MODEL = ModelSpec("cheap/model")


def test_a_candidate_passing_first_time_is_accepted(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, tmp_path, test_results=[True])

    state = farm_run(tmp_path)

    assert state.outcome is FarmOutcome.ACCEPTED
    assert len(state.attempts) == 1
    assert state.accepted == tmp_path / f"{FUNCTION}.c"


def test_a_failure_goes_back_to_the_model_with_the_report(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    prompts = fake_tools(monkeypatch, tmp_path, test_results=[False, True])

    state = farm_run(tmp_path)

    assert state.outcome is FarmOutcome.ACCEPTED
    assert len(state.attempts) == 2
    assert prompts[1].startswith("repair from your last attempt: Next leafFunction")
    assert "report: memory differs" in prompts[1]


def test_the_typed_prompt_starts_from_another_models_best_attempt(monkeypatch: pytest.MonkeyPatch,
                                                                  tmp_path: Path) -> None:
    earlier = (tried(CandidateC("compiles not"), False, "compile errors:\nline 1: error: x", "another model's (a)"),
               tried(CandidateC("close"), False, RANDOM_DIFFERS, "another model's (b)"))
    prompts = fake_tools(monkeypatch, tmp_path, test_results=[False, True], earlier=earlier)

    state = farm_run(tmp_path)

    assert state.outcome is FarmOutcome.ACCEPTED
    assert prompts[0].startswith("repair from another model's (b): close")      # the best one, not the first
    assert "another model's (a): does not compile (1 errors)" in prompts[0]      # the others, in one line
    assert prompts[1].startswith("repair from another model's (b): close")      # still the best one: ours was worse
    assert "your last attempt: the difftest failed" in prompts[1]


def test_a_repair_starts_from_the_best_attempt_not_the_last(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    reports = [RANDOM_DIFFERS, "compile errors:\nline 3: error: y", "ok"]
    prompts = fake_tools(monkeypatch, tmp_path, test_results=[False, False, True], reports=reports)

    state = farm_run(tmp_path)

    assert state.outcome is FarmOutcome.ACCEPTED
    assert prompts[0] == "first"
    assert prompts[2].startswith("repair from your round 0:")
    assert "your last attempt: does not compile (1 errors)" in prompts[2]


def test_a_typed_candidate_is_kept_finished(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    calls = fake_tools(monkeypatch, tmp_path, test_results=[True])

    _ = farm_run(tmp_path)

    assert calls.kept == ["finished: Next leafFunction(void) { RETURN_TO_CALLER(); }"]
    assert calls.first_failure_only == [False]                                     # every problem reported


def test_out_of_rounds_gives_up(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, tmp_path, test_results=[False, False])

    state = farm_run(tmp_path, rounds=1)

    assert state.outcome is FarmOutcome.OUT_OF_ROUNDS
    assert len(state.attempts) == 2


def test_a_reply_without_code_is_not_asked_again(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    calls = fake_tools(monkeypatch, tmp_path, test_results=[], no_code=True)

    state = farm_run(tmp_path)

    assert state.outcome is FarmOutcome.RUNAWAY
    assert len(state.attempts) == 1
    assert calls.budgets == [12_000]   # each call is asked with its model's reasoning budget


def test_a_provider_error_gives_up(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, tmp_path, test_results=[], provider_fails=True)

    assert farm_run(tmp_path).outcome is FarmOutcome.MODEL_ERROR


def test_no_captured_calls_means_nothing_to_convert(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, tmp_path, test_results=[])

    state = farm_run(tmp_path, with_snapshots=False)

    assert state.outcome is FarmOutcome.NOTHING_TO_CONVERT


def test_a_batch_skips_finished_pairs_and_tallies(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, tmp_path, test_results=[True])
    done = FunctionName("doneBefore")

    def accepted_before(function: FunctionName, model: ModelSpec) -> Path | None:
        return tmp_path / "accepted.c" if function == done else None

    def build_host_difftest(log_dir: Path, build_dir: str = "", tree: Path | None = None) -> Path:
        return tmp_path / "difftest"

    monkeypatch.setattr(farm, "committed_tree", lambda name: tmp_path / name)  # pyright: ignore[reportUnknownLambdaType, reportUnknownArgumentType]
    monkeypatch.setattr(farm, "already_accepted", accepted_before)
    monkeypatch.setattr(farm, "build_host_difftest", build_host_difftest)
    for function in (FUNCTION, done):
        (tmp_path / "snapshots" / function).mkdir(parents=True)

    start = FarmBatchState(run_dir=tmp_path, functions=(FUNCTION, done), models=(MODEL,),
                           snapshots=SnapshotDir(tmp_path / "snapshots"), jobs=2)
    outcome = farm_batch_graph().start(start, run_id="batch")

    assert isinstance(outcome, Finished)
    assert [pair.run_id for pair in outcome.state.pairs] == ["batch-leafFunction-cheap_model", None]
    assert outcome.state.tallies[0].accepted == 2


def test_a_batch_stops_when_difftest_does_not_build(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    def build_host_difftest(log_dir: Path, build_dir: str = "", tree: Path | None = None) -> BuildFailure:
        return BuildFailure(log=LogPath(tmp_path / "build.log"), last_lines=(ToolMessage("error"),))

    monkeypatch.setattr(farm, "committed_tree", lambda name: tmp_path / name)  # pyright: ignore[reportUnknownLambdaType, reportUnknownArgumentType]
    monkeypatch.setattr(farm, "build_host_difftest", build_host_difftest)

    start = FarmBatchState(run_dir=tmp_path, functions=(FUNCTION,), models=(MODEL,),
                           snapshots=SnapshotDir(tmp_path))
    outcome = farm_batch_graph().start(start, run_id="batch-2", tracked=False)

    assert isinstance(outcome, Finished) and outcome.state.pairs == ()


def test_the_old_prompt_is_its_own_pair_without_the_register_check(monkeypatch: pytest.MonkeyPatch,
                                                                     tmp_path: Path) -> None:
    calls = fake_tools(monkeypatch, tmp_path, test_results=[True, True])
    typed_done = FunctionName("typedTriedBefore")
    asked_for: list[ModelSpec] = []

    def accepted_before(function: FunctionName, model: ModelSpec) -> Path | None:
        asked_for.append(model)
        return tmp_path / "accepted.c" if function == typed_done and prompt_of(model) is PromptKind.TYPED else None

    def build_host_difftest(log_dir: Path, build_dir: str = "", tree: Path | None = None) -> Path:
        assert tree == tmp_path / "farm-old"   # not the typed batches' tree, which may be in use
        return tmp_path / "difftest"

    monkeypatch.setattr(farm, "committed_tree", lambda name: tmp_path / name)  # pyright: ignore[reportUnknownLambdaType, reportUnknownArgumentType]
    monkeypatch.setattr(farm, "already_accepted", accepted_before)
    monkeypatch.setattr(farm, "build_host_difftest", build_host_difftest)
    for function in (FUNCTION, typed_done):
        (tmp_path / "snapshots" / function).mkdir(parents=True)

    start = FarmBatchState(run_dir=tmp_path, functions=(FUNCTION, typed_done), models=(MODEL,),
                           snapshots=SnapshotDir(tmp_path / "snapshots"), jobs=1, prompt=PromptKind.OLD)
    outcome = farm_batch_graph().start(start, run_id="batch-old", tracked=False)

    assert isinstance(outcome, Finished)
    assert all(pair.run_id is not None for pair in outcome.state.pairs)   # the typed acceptance does not count
    assert set(asked_for) == {ModelSpec("cheap/model+old")}
    assert calls == ["first old", "first old"]
    assert calls.register_checks == [False, False] and calls.filed == ["cheap/model+old"] * 2
    assert calls.earlier_asked == 0 and not calls.kept[0].startswith("finished")   # no hand-over, kept as written
    assert calls.first_failure_only == [True, True]                                # its test as benchmarked


def test_prompt_tags_round_trip() -> None:
    tagged = with_prompt(MODEL, PromptKind.OLD)
    assert tagged == "cheap/model+old" and prompt_of(tagged) is PromptKind.OLD and base_model(tagged) == MODEL
    assert with_prompt(MODEL, PromptKind.TYPED) == MODEL and prompt_of("hyper_qwen3.8-flash") is PromptKind.TYPED
    assert prompt_of("minimax_MiniMax-M3+old") is PromptKind.OLD


# ── helpers ────────────────────────────────────────────────────────────

RANDOM_DIFFERS = ("captured calls: f  ok  (4 snapshots, 4 runs, 0 failures, 0 out-of-domain)\n"
                  "random inputs: f  FAIL  (300 trials, 7 failures, 0 out-of-domain)")


class Calls(list[Prompt]):
    """The prompts the fake model was given, and what the fake tools were asked."""
    register_checks: list[bool]
    first_failure_only: list[bool]
    filed: list[ModelSpec]
    kept: list[str]
    earlier_asked: int
    budgets: list[int | None]


def farm_run(tmp_path: Path, rounds: int = 3, with_snapshots: bool = True) -> FarmState:
    snapshots = tmp_path / "snapshots"
    if with_snapshots:
        (snapshots / FUNCTION).mkdir(parents=True, exist_ok=True)
    start = FarmState(function=FUNCTION, model=MODEL, snapshots=SnapshotDir(snapshots),
                      difftest=tmp_path / "difftest", rounds=rounds)
    outcome = farm_graph().start(start, run_id="test", tracked=False)
    assert isinstance(outcome, Finished)
    return outcome.state


def fake_tools(monkeypatch: pytest.MonkeyPatch, tmp_path: Path, test_results: list[bool],
               provider_fails: bool = False, no_code: bool = False, earlier: tuple[Tried, ...] = (),
               reports: list[str] | None = None) -> Calls:
    """Fake model and acceptance test (passing or failing as TEST_RESULTS say, with REPORTS or "memory
    differs"), and EARLIER attempts of other models; returns the prompts the model was given."""
    prompts = Calls()
    prompts.register_checks, prompts.filed, prompts.kept, prompts.earlier_asked = [], [], [], 0
    prompts.first_failure_only, prompts.budgets = [], []
    results = iter(test_results)
    texts = iter(reports or [])

    def conversion_context(function: FunctionName) -> ConversionContext:
        return ConversionContext(function=function, address=GuestAddress(0x08001234), assembly=AssemblyText("bx lr"),
                                 lifted=LiftedC("Next f(void) {}"),
                                 contract=Contract(inputs=(), outputs=(), clobbers=(), preserved=()))

    def ask_model(model: ModelSpec, prompt: Prompt, tag: str, wait_out_limits: bool = True,
                  kind: PromptKind = PromptKind.TYPED, budget: int | None = None) -> ModelReply:
        prompts.append(prompt)
        prompts.budgets.append(budget)
        if provider_fails:
            return ModelReply(candidate=None, seconds=1.0, error=ToolMessage("model error: HTTP 500"))
        if no_code:
            return ModelReply(candidate=None, seconds=1.0)
        return ModelReply(candidate=CandidateC("Next leafFunction(void) { RETURN_TO_CALLER(); }"), seconds=1.0)

    def repair_prompt(context: ConversionContext, failure: ToolMessage, candidate: CandidateC | None,
                      prompt: PromptKind = PromptKind.TYPED, whose: str = "your last attempt",
                      history: str = "") -> Prompt:
        if prompt is not PromptKind.TYPED:
            return Prompt(f"repair: {failure}")
        return Prompt(f"repair from {whose}: {candidate}\nreport: {failure}\nhistory: {history}")

    def test_candidate(context: ConversionContext, candidate: CandidateC, snapshots: SnapshotDir,
                       difftest: Path, check_registers: bool = True, first_failure_only: bool = False) -> CandidateTest:
        prompts.register_checks.append(check_registers)
        prompts.first_failure_only.append(first_failure_only)
        passed = next(results)
        report = next(texts, "ok" if passed else "memory differs")
        return CandidateTest(passed=passed, report=ToolMessage(report))

    def accept(function: FunctionName, model: ModelSpec, candidate: CandidateC) -> Path:
        prompts.filed.append(model)
        prompts.kept.append(candidate)
        return tmp_path / f"{function}.c"

    def earlier_attempts(context: ConversionContext, difftest: Path, leave_out: frozenset[str] = frozenset(),
                         snapshots: Path = tmp_path) -> tuple[Tried, ...]:
        prompts.earlier_asked += 1
        return earlier

    def finished(function: FunctionName, candidate: CandidateC) -> CandidateC:
        return CandidateC(f"finished: {candidate}")

    def nothing_before(function: FunctionName, model: ModelSpec) -> None:
        return None

    def not_out_of_rounds(function: FunctionName, model: ModelSpec, rounds: int) -> bool:
        return False

    def first_prompt(context: ConversionContext, prompt: PromptKind = PromptKind.TYPED) -> Prompt:
        return Prompt("first" if prompt is PromptKind.TYPED else f"first {prompt.value}")

    def log_round(function: FunctionName, model: ModelSpec, round_number: int, reply: ModelReply,
                  test: CandidateTest | None, difftest: Path | None = None,
                  prompt: PromptKind = PromptKind.TYPED) -> None:
        return None

    def reasoning_budget(model: ModelSpec, prompt: PromptKind, function: FunctionName) -> Budget:
        return Budget(tokens=12_000, basis="test")

    fakes: dict[str, object] = {
        "reasoning_budget": reasoning_budget,
        "conversion_context": conversion_context, "ask_model": ask_model, "first_prompt": first_prompt,
        "repair_prompt": repair_prompt, "test_candidate": test_candidate, "accept": accept,
        "already_accepted": nothing_before, "out_of_rounds_before": not_out_of_rounds, "log_round": log_round,
        "earlier_attempts": earlier_attempts, "finished": finished,
    }
    for name, fake in fakes.items():
        monkeypatch.setattr(farm, name, fake)
    return prompts
