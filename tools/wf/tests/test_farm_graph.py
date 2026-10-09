"""The farm graphs with a fake model and a fake acceptance test: the repair
loop, giving up, and a batch skipping finished pairs."""
from pathlib import Path

import pytest

from wf.burr_typed import Finished
from wf.graphs import farm
from wf.graphs.farm import FarmBatchState, FarmOutcome, FarmState, farm_batch_graph, farm_graph
from wf.tools.drafts import Draft
from wf.tools.models import (AssemblyText, CandidateC, CandidateTest, Contract, ConversionContext, LiftedC,
                             ModelReply, Prompt)
from wf.types import BuildFailure, FunctionName, GuestAddress, LogPath, ModelSpec, SnapshotDir, ToolMessage

FUNCTION = FunctionName("leafFunction")
MODEL = ModelSpec("cheap/model")


def test_a_candidate_passing_first_time_is_accepted(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, tmp_path, test_results=[True])

    state = farm_run(tmp_path)

    assert state.outcome is FarmOutcome.ACCEPTED
    assert len(state.attempts) == 1
    assert state.draft is not None and state.draft.path == tmp_path / f"{FUNCTION}.yaml"


def test_a_failure_goes_back_to_the_model_with_the_report(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    prompts = fake_tools(monkeypatch, tmp_path, test_results=[False, True])

    state = farm_run(tmp_path)

    assert state.outcome is FarmOutcome.ACCEPTED
    assert len(state.attempts) == 2
    assert "repair: memory differs" in prompts[1]


def test_out_of_rounds_gives_up(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_tools(monkeypatch, tmp_path, test_results=[False, False])

    state = farm_run(tmp_path, rounds=1)

    assert state.outcome is FarmOutcome.OUT_OF_ROUNDS
    assert len(state.attempts) == 2


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


# ── helpers ────────────────────────────────────────────────────────────

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
               provider_fails: bool = False) -> list[Prompt]:
    """Fake model and acceptance test; returns the prompts the model was given."""
    prompts: list[Prompt] = []
    results = iter(test_results)

    def conversion_context(function: FunctionName) -> ConversionContext:
        return ConversionContext(function=function, address=GuestAddress(0x08001234), assembly=AssemblyText("bx lr"),
                                 lifted=LiftedC("Next f(void) {}"),
                                 contract=Contract(inputs=(), outputs=(), clobbers=(), preserved=()))

    def ask_model(model: ModelSpec, prompt: Prompt, tag: str, wait_out_limits: bool = True) -> ModelReply:
        prompts.append(prompt)
        if provider_fails:
            return ModelReply(candidate=None, seconds=1.0, error=ToolMessage("model error: HTTP 500"))
        return ModelReply(candidate=CandidateC("Next leafFunction(void) { RETURN_TO_CALLER(); }"), seconds=1.0)

    def repair_prompt(context: ConversionContext, failure: ToolMessage, candidate: CandidateC | None) -> Prompt:
        return Prompt(f"repair: {failure}")

    def test_candidate(context: ConversionContext, candidate: CandidateC, snapshots: SnapshotDir,
                       difftest: Path) -> CandidateTest:
        passed = next(results)
        return CandidateTest(passed=passed, report=ToolMessage("ok" if passed else "memory differs"))

    def accept(function: FunctionName, model: ModelSpec, candidate: CandidateC) -> Path:
        return tmp_path / f"{function}.c"

    def nothing_before(function: FunctionName, model: ModelSpec) -> None:
        return None

    def not_out_of_rounds(function: FunctionName, model: ModelSpec, rounds: int) -> bool:
        return False

    def first_prompt(context: ConversionContext) -> Prompt:
        return Prompt("first")

    def log_round(function: FunctionName, model: ModelSpec, round_number: int, reply: ModelReply,
                  test: CandidateTest | None) -> None:
        return None

    def draft_entry(model: ModelSpec, candidate: Path, tag: str) -> Draft:
        return Draft(function=FUNCTION, path=candidate.with_suffix(".yaml"), problem=None, seconds=1.0)

    fakes: dict[str, object] = {
        "conversion_context": conversion_context, "ask_model": ask_model, "first_prompt": first_prompt,
        "repair_prompt": repair_prompt, "test_candidate": test_candidate, "accept": accept,
        "already_accepted": nothing_before, "out_of_rounds_before": not_out_of_rounds, "log_round": log_round,
        "draft_entry": draft_entry,
    }
    for name, fake in fakes.items():
        monkeypatch.setattr(farm, name, fake)
    return prompts
