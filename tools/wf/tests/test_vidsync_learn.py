"""vidsync_learn: a fix's writes named in bn6f's terms and added as runtime evidence to their entries."""
from pathlib import Path

import yaml

from wf.tools import vidsync_learn
from wf.tools.vidsync_learn import Fix, Namer, learn
from wf.types import FrameCount, RunName

SYMS = {"eStruct2001c04": 0x02001C04, "oS2001c04_EncounterStepAccumulator": 0x12, "ePrimaryRngSeed": 0x020013F0,
        "eBattleFolder": 0x0203CDB0}
FIELDS = {0x02001C16: ("eStruct2001c04", "oS2001c04_EncounterStepAccumulator")}


def fix(n: int, handler: str, writes: list[tuple[int, int, int]], game: int = 498267) -> Fix:
    return Fix(dir="data/auto/run7y.0", n=n, time="12:18:02", handler=handler, game=FrameCount(game),
               writes=writes, mean=0.9, line=f"12:18:02 fix {n}: write ... at game {game}; going on from there")


def db(tmp_path: Path) -> dict[str, Path]:
    out = {}
    for name in ("oS2001c04_EncounterStepAccumulator", "ePrimaryRngSeed"):
        p = tmp_path / f"{name}.yaml"
        p.write_text(yaml.safe_dump({"name": name, "status": "probable", "evidence": []}, sort_keys=False))
        out[name] = p
    return out


def test_a_field_named_by_syms_py_gets_the_evidence(tmp_path: Path, monkeypatch) -> None:
    monkeypatch.setattr(vidsync_learn, "REPO", tmp_path)
    by_name = db(tmp_path)
    namer = Namer(SYMS, FIELDS, by_name, {})
    learned, seen = learn(RunName("run7y"), [fix(34, "encounter", [(0x02001C16, 0x400, 2), (0x020013F0, 1, 4),
                                                                      (0x0203CDB4, 7, 4)])],
                          namer, by_name, set(), today="2026-10-16")
    assert sorted(learned.entries) == ["ePrimaryRngSeed.yaml", "oS2001c04_EncounterStepAccumulator.yaml"]
    assert learned.unnamed == ("eBattleFolder+0x4",)
    ev = yaml.safe_load(by_name["oS2001c04_EncounterStepAccumulator"].read_text())["evidence"][0]
    assert ev["method"] == "runtime" and "by the encounter handler" in ev["detail"]
    assert "fix 34: " in ev["reproduce"]
    again, _ = learn(RunName("run7y"), [fix(34, "encounter", [(0x02001C16, 0x400, 2)])], namer, by_name, seen)
    assert again.entries == ()                       # the fix was seen
    other, _ = learn(RunName("run7y"), [fix(35, "encounter", [(0x02001C16, 0x500, 2)])], namer, by_name, seen)
    assert other.entries == ()                       # this run and handler have their evidence already


def test_a_field_label_without_an_entry_goes_to_its_struct(tmp_path: Path) -> None:
    by_name = {"ePanelData": tmp_path / "ePanelData.yaml", "eStruct2035280": tmp_path / "eStruct2035280.yaml"}
    namer = Namer({}, {}, by_name, {})
    assert namer.owner("ePanelData4_1_Type") == "ePanelData"
    assert namer.owner("eStruct2035280_UnkFlags_44") == "eStruct2035280"
    assert namer.owner("eT1BattleObject0_HP") is None
    assert namer.owner("word_203CDE8") is None
