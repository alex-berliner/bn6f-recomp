"""The drafts graph with a fake model: every candidate without a draft is
drafted, failures are reported, and nothing is asked when all have one."""
from collections.abc import Callable
from pathlib import Path
from typing import cast

import pytest

from wf.graphs import drafts
from wf.tools import drafts as drafts_tools
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


def test_a_word_in_a_comment_is_no_definition_in_a_farm_group() -> None:
    # sub_8003BF4: "enum" in a comment, then a body that sets R0: R0 was renamed apart (R0_sub_8003BF4)
    first = ("/* OBJECT_FLAG_VISIBLE in the disassembly's enum */\n"
             "static void helper(u32 position) {\n    R0 = position;\n    GUEST_CALL(f);\n}\n")
    second = "static void other(u32 slot) {\n    R0 = slot;\n    GUEST_CALL(g);\n}\n"
    kept_apart = cast(Callable[..., list[tuple[str, str]]], getattr(drafts_tools, "_names_kept_apart"))  # pyright: ignore[reportExplicitAny]
    kept = kept_apart([(FunctionName("sub_8003BF4"), first), (FunctionName("sub_8003C10"), second)], {})
    assert "R0_" not in "".join(body for _, body in kept)
