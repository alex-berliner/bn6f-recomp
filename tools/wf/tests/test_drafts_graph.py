"""The drafts graph with a fake model: every candidate without a draft is
drafted, failures are reported, and nothing is asked when all have one."""
from pathlib import Path

import pytest

from wf.graphs import drafts
from wf.graphs.drafts import DraftsState, run_drafts
from wf.tools.drafts import Draft
from wf.types import FunctionName, ModelSpec, ToolMessage

MODEL = ModelSpec("cheap/model")


def test_each_undrafted_candidate_is_drafted(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    asked = fake_tools(monkeypatch, tmp_path, ("good", "bad"))

    report = run_drafts(DraftsState(model=MODEL), run_id="drafts-1", tracked=False)

    assert sorted(asked) == ["bad", "good"]
    assert report.written == 1 and [draft.function for draft in report.failed] == ["bad"]


def test_nothing_is_asked_when_every_candidate_has_a_draft(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    asked = fake_tools(monkeypatch, tmp_path, ())

    report = run_drafts(DraftsState(model=MODEL), run_id="drafts-2", tracked=False)

    assert asked == [] and report.written == 0


def fake_tools(monkeypatch: pytest.MonkeyPatch, tmp_path: Path, undrafted: tuple[str, ...]) -> list[str]:
    asked: list[str] = []

    def find(model: ModelSpec, functions: tuple[FunctionName, ...] | None) -> tuple[Path, ...]:
        return tuple(tmp_path / f"{name}.c" for name in undrafted)

    def draft_entry(model: ModelSpec, candidate: Path, tag: str) -> Draft:
        asked.append(candidate.stem)
        good = candidate.stem == "good"
        return Draft(function=FunctionName(candidate.stem), path=candidate.with_suffix(".yaml") if good else None,
                     problem=None if good else ToolMessage("no ```yaml block"), seconds=1.0)

    monkeypatch.setattr(drafts, "undrafted", find)
    monkeypatch.setattr(drafts, "draft_entry", draft_entry)
    return asked
