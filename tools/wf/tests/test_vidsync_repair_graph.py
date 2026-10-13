"""The vidsync-repair graph with fake PCs and a fake model: it stops for Claude to classify the loss and to
review the handler; a handler that fails its test goes back to a model with the test's result; one that holds
is recorded, committed and the driver restarted past the loss. The brief carries the symbols/db entries and
the routines' assembly; a reply is checked before review."""
from dataclasses import dataclass, field
from pathlib import Path

import pytest

from wf.burr_typed import Finished, Paused
from wf.graphs import vidsync_repair
from wf.graphs.record import RecordReport, RecordState, RecordVerdict
from wf.graphs.vidsync_repair import (Findings, HandlerReview, RepairVerdict, ReviewVerdict, VidsyncRepairState,
                                      vidsync_repair_graph)
from wf.tools import vidsync_brief
from wf.tools.vidsync import Checkpoint, HandlerTest, LossCase, PcDeploy
from wf.tools.vidsync_brief import Brief, HandlerCandidate, HandlerCode, HandlerName, LossClass, LossKind, parse_handler
from wf.types import (CommitHash, FrameCount, LogPath, ModelSpec, PcHost, RemotePath, RunName, SymbolName,
                      ToolMessage)

GOOD = HandlerCode('NAME = "weather_orbs"\n\n\ndef try_fix(D, sync):\n    return None\n')
CASE = LossCase(run=RunName("run7l"), loss=FrameCount(432728), video=440775,
                loss_json=RemotePath("data/auto/run7l.0/loss_432728.json"), pokes=206,
                checkpoint=Checkpoint(path=RemotePath("data/auto/run6y.0/fix_0026"), g=FrameCount(432312)),
                log=(ToolMessage("lost the video near game 432728"),))
CLASS = LossClass(kind=LossKind.PUZZLE, what_differs="the rainbow-orb trail empties in the video, not here",
                  symbols=(SymbolName("eWeatherCompState"),), routines=(), handler_name=HandlerName("weather_orbs"))
FINDINGS = Findings(symbols=(SymbolName("eWeatherCompState"),), record_message="record", handler_message="handler")


@dataclass
class Fakes:
    tests_pass: list[bool] = field(default_factory=lambda: [True])
    called: list[str] = field(default_factory=list[str])
    briefs: list[str] = field(default_factory=list[str])

    def install(self, monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
        results = iter(self.tests_pass)
        monkeypatch.setattr(vidsync_repair, "HANDLERS", tmp_path / "handlers")
        (tmp_path / "handlers").mkdir()

        def find_loss(run: RunName, loss: FrameCount | None = None) -> LossCase:
            return CASE

        def render_loss(case: LossCase, local_dir: Path) -> tuple[Path, ...]:
            return (local_dir / "compare.png", local_dir / "video.png")

        def ask(model: ModelSpec, brief: Brief, name: HandlerName, tag: str, record_dir: Path) -> HandlerCandidate:
            self.called.append("model")
            self.briefs.append(brief)
            return HandlerCandidate(name=name, code=GOOD, model=model, problems=(), notes=(), brief_file=record_dir / "b",
                                    reply_file=None, seconds=1.0)

        def deploy(pcs: tuple[PcHost, ...], log: LogPath) -> tuple[PcDeploy, ...]:
            self.called.append(f"deploy {len(pcs)}")
            return tuple(PcDeploy(pc=pc, ok=True, commit="c", uncommitted=True, output=ToolMessage("")) for pc in pcs)

        def test_handler(case: LossCase, handler: str, attempt: int, hold: int = 600) -> HandlerTest:
            self.called.append("test")
            passed = next(results)
            return HandlerTest(out_dir=RemotePath(f"data/auto/run7l-test-{handler}-{attempt}"),
                               reached=FrameCount(433328 if passed else 432740), needed=FrameCount(433328),
                               fixes=1 if passed else 0, pathcheck_holds=passed,
                               log_tail=(ToolMessage("done: stuck near game 432740"),))

        def run_record(start: RecordState, run_id: str, tracked: bool = True) -> RecordReport:
            self.called.append("record")
            return RecordReport(verdict=RecordVerdict.RECORDED, problems=(), renamed_in_bn6f=None,
                                comments_in_bn6f=None, verification=None, commit=CommitHash("r"))

        def commit_files(files: tuple[Path, ...], message: str) -> CommitHash:
            self.called.append("commit")
            return CommitHash("h")

        def newest_checkpoint(run: RunName | RemotePath, most: FrameCount | None = None) -> Checkpoint:
            return Checkpoint(path=RemotePath(f"{run}/fix_0001"), g=FrameCount(432800))

        def start_driver(run: RunName, start: RemotePath, replay: RemotePath | None = None) -> None:
            self.called.append(f"driver {run}")

        for name, fake in (("find_loss", find_loss), ("render_loss", render_loss), ("ask_model_for_handler", ask),
                           ("deploy", deploy), ("test_handler", test_handler), ("run_record", run_record),
                           ("commit_files", commit_files), ("newest_checkpoint", newest_checkpoint),
                           ("start_driver", start_driver), ("restart_workers", lambda: 16),
                           ("run_names", lambda: (RunName("run7k"), RunName("run7l")))):
            monkeypatch.setattr(vidsync_repair, name, fake)


def start(fakes: Fakes, monkeypatch: pytest.MonkeyPatch, tmp_path: Path, run_id: str) -> Paused:
    fakes.install(monkeypatch, tmp_path)
    state = VidsyncRepairState(run_dir=tmp_path, run=RunName("run7l"), models=(ModelSpec("opencode/kimi-k3"),))
    paused = vidsync_repair_graph().start(state, run_id)
    assert isinstance(paused, Paused) and paused.step == "classify_the_loss"
    return paused


def test_a_handler_that_holds_is_recorded_committed_and_the_driver_restarted(monkeypatch: pytest.MonkeyPatch,
                                                                             tmp_path: Path) -> None:
    fakes = Fakes()
    _ = start(fakes, monkeypatch, tmp_path, "repair-1")
    review = vidsync_repair_graph().resume("repair-1", CLASS)
    assert isinstance(review, Paused) and review.step == "review_the_handler"
    findings = vidsync_repair_graph().resume("repair-1", HandlerReview(verdict=ReviewVerdict.APPROVE))
    assert isinstance(findings, Paused) and findings.step == "record_findings"
    done = vidsync_repair_graph().resume("repair-1", FINDINGS)

    assert isinstance(done, Finished) and done.state.report is not None
    assert done.state.report.verdict is RepairVerdict.REPAIRED and done.state.report.restarted_as == "run7m"
    assert fakes.called == ["model", "deploy 1", "test", "record", "commit", "deploy 2", "driver run7m"]
    assert (tmp_path / "handlers" / "weather_orbs.py").read_text() == GOOD


def test_a_failed_test_goes_back_to_the_model_with_its_result(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fakes = Fakes(tests_pass=[False, True])
    _ = start(fakes, monkeypatch, tmp_path, "repair-2")
    _ = vidsync_repair_graph().resume("repair-2", CLASS)
    again = vidsync_repair_graph().resume("repair-2", HandlerReview(verdict=ReviewVerdict.APPROVE))

    assert isinstance(again, Paused) and again.step == "review_the_handler"
    assert "test of attempt 1: reached 432740 (needed 433328)" in fakes.briefs[1]
    assert not (tmp_path / "handlers" / "weather_orbs.py").exists()     # a failed handler leaves the tree


def test_requested_changes_reach_the_next_brief_and_a_rejection_ends_it(monkeypatch: pytest.MonkeyPatch,
                                                                        tmp_path: Path) -> None:
    fakes = Fakes()
    _ = start(fakes, monkeypatch, tmp_path, "repair-3")
    _ = vidsync_repair_graph().resume("repair-3", CLASS)
    again = vidsync_repair_graph().resume("repair-3", HandlerReview(verdict=ReviewVerdict.REQUEST_CHANGES,
                                                                    notes="judge by path_reach, not one frame"))
    assert isinstance(again, Paused) and "judge by path_reach, not one frame" in fakes.briefs[1]
    done = vidsync_repair_graph().resume("repair-3", HandlerReview(verdict=ReviewVerdict.REJECT))

    assert isinstance(done, Finished) and done.state.report is not None
    assert done.state.report.verdict is RepairVerdict.GAVE_UP and "test" not in fakes.called


def test_claude_can_decline_a_new_handler(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fakes = Fakes()
    _ = start(fakes, monkeypatch, tmp_path, "repair-4")
    done = vidsync_repair_graph().resume("repair-4", CLASS.model_copy(update={"ask_model": False}))

    assert isinstance(done, Finished) and done.state.report is not None
    assert done.state.report.verdict is RepairVerdict.NO_HANDLER and fakes.called == []


def test_the_brief_carries_the_entries_and_the_assembly(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    database = tmp_path / "db"
    database.mkdir()
    _ = (database / "eWeatherCompState.yaml").write_text("name: eWeatherCompState\nstatus: probable\n")
    monkeypatch.setattr(vidsync_brief, "DATABASE", database)
    def find_asm(name: str) -> str:
        return f"{name}:\n\tpush {{r4, lr}}"

    monkeypatch.setattr(vidsync_brief.FARM, "find_asm", find_asm)
    brief = vidsync_brief.write_brief(CASE, CLASS.model_copy(update={"routines": ("weatherComp_init_808FDC0",)}),
                                      ("review: no bare addresses",), GOOD)

    assert "status: probable" in brief and "weatherComp_init_808FDC0:\n\tpush {r4, lr}" in brief
    assert "def path_reach(" in brief and "def try_tornado(" in brief      # helpers and the closest handlers
    assert "PRIMARY_RNG_SEED" in brief and "review: no bare addresses" in brief


def test_replies_are_checked_before_review() -> None:
    code, problems, notes = parse_handler(f"Here:\n```python\n{GOOD}```\n", HandlerName("weather_orbs"))
    assert code == GOOD and problems == () and notes == ()

    _, problems, _ = parse_handler("```python\ndef try_fix(D):\n    pass\n```", HandlerName("weather_orbs"))
    assert len(problems) == 2

    _, _, notes = parse_handler(f"```python\n{GOOD}x = 0x02000348\n```", HandlerName("weather_orbs"))
    assert notes == ("hard-coded address 0x02000348: use a syms name",)

    code, problems, _ = parse_handler("no code", HandlerName("weather_orbs"))
    assert code is None and problems
