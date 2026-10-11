"""db entries for converted functions in the form symbols/db/README.md gives: a farm group's entry
converts its own function and lists only the members it calls or that call it (the group is the
run); a subsystem's lists the whole subsystem; sources name the run and the commit."""
from pathlib import Path
from typing import cast

import pytest
import yaml

from wf.repo import REPO
from wf.tools import evidence
from wf.tools.evidence import FunctionChecks, Provenance, write_entries
from wf.tools.subsystem import SymbolDraft
from wf.types import CommitHash, FunctionName, GuestAddress, ModelSpec

FUNCTIONS = {"first": 0x08001000, "second": 0x08002000, "third": 0x08003000}


def entries(monkeypatch: pytest.MonkeyPatch, tmp_path: Path, farm_group: bool,
            placed: dict[FunctionName, Path] | None = None) -> dict[str, dict[str, object]]:
    def no_routes(address: GuestAddress) -> list[str]:
        return []

    def neighbours() -> dict[int, frozenset[int]]:      # first calls second; third: nobody in the group
        return {0x08001000: frozenset({0x08002000, 0x08009000}), 0x08002000: frozenset({0x08001000})}

    monkeypatch.setattr(evidence, "DATABASE", tmp_path)
    monkeypatch.setattr(evidence, "covering_routes", no_routes)
    monkeypatch.setattr(evidence, "call_neighbours", neighbours)
    drafts = tuple(SymbolDraft.model_validate({"function": name, "proposed_name": name, "status": "probable",
                                               "summary": "s", "interface": "i"}) for name in FUNCTIONS)
    checks = tuple(FunctionChecks(function=FunctionName(name), address=GuestAddress(address), random=None,
                                  captured=None, oracle=None, routes_identical=8, routes_checked=8)
                   for name, address in FUNCTIONS.items())
    provenance = Provenance(subsystem="small functions from the model farm, set 40", file=REPO / "cport/src/game/x.c",
                            model=ModelSpec("hyper/glm-5.3-flash"), attempts=1, convert_run="convert-20261010-000000",
                            verify_run="convert-20261010-000000-verify-1", commit=CommitHash("c0ffee0123"),
                            farm_group=farm_group, placed=placed or {})
    paths = write_entries(drafts, checks, provenance)
    return {path.stem: cast(dict[str, object], yaml.safe_load(path.read_text())) for path in paths}


def readable_c(entry: dict[str, object]) -> str:
    items = cast(list[dict[str, str]], entry["evidence"])
    return next(item["reproduce"] for item in items if item["method"] == "readable-c")


def test_a_farm_group_entry_converts_its_own_function_and_relates_by_calls(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    found = entries(monkeypatch, tmp_path, farm_group=True)

    assert readable_c(found["first"]) == \
        "./wf convert --subsystem 'small functions from the model farm, set 40' --file x.c first"
    assert found["first"]["related"] == ["second"] and found["second"]["related"] == ["first"]
    assert "related" not in found["third"]
    assert found["third"]["sources"] == ["tools/wf run convert-20261010-000000", "recomp c0ffee0"]


def test_a_subsystem_entry_names_the_whole_subsystem(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    found = entries(monkeypatch, tmp_path, farm_group=False)

    assert readable_c(found["third"]).endswith("--file x.c first second third")
    assert found["third"]["related"] == ["first", "second"]


def test_a_farm_group_entry_names_the_laid_out_file_its_function_went_to(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    dest = REPO / "cport/src/game/object.c"
    found = entries(monkeypatch, tmp_path, farm_group=True, placed={FunctionName("first"): dest})

    assert found["first"]["readable_c"] == "cport/src/game/object.c"
    assert cast(list[dict[str, object]], found["first"]["evidence"])[0]["refs"] == ["cport/src/game/object.c"]
    assert readable_c(found["first"]).endswith("--file x.c first")          # the command that made it
    assert found["second"]["readable_c"] == "cport/src/game/x.c"            # not placed: the file it was given
