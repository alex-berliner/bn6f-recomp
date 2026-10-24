#!/usr/bin/env python3
"""The three-arm A/B table (without facts / facts v1 / facts v2) and the name-quality check.

    python pilot/name_quality_ab.py [--facts-dir DIR] [--pairs N]

* reads the six runs' report.tsv (build/investigate/RUN/report.tsv) and prints, per arm and
  over both arms: functions, names proposed, probable+agree, tokens;
* prints, for every function any arm named, the names side by side with the v2 facts hint
  (per-call scenes, attributed struct-field writes, runtime callers);
* marks the names that CONTRADICT the facts -- a scene prefix naming a scene the captured
  calls were never in, or a struct prefix naming a family none of the attributed field
  writes belong to -- and counts the contradictions per arm.

The facts come from the v2 facts directory (`--facts-dir`, default: this worktree's
build/facts-v2), which is where the per-call scenes, the struct-field writes and the CPU
entry counts live.
"""
from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from pathlib import Path

REPO = Path("/home/alex/Code/recomp")
INVESTIGATE = REPO / "build/investigate"

ARMS: dict[str, tuple[str, str]] = {
    "without": ("investigate-20261021-101004", "investigate-20261021-101527"),
    "v1": ("investigate-20261021-100724", "investigate-20261021-101229"),
    "v2": ("investigate-20261024-064512", "investigate-20261024-064746"),
}
ORDER = ("without", "v1", "v2")

# ── what a name's leading word claims ──────────────────────────────────────────────────
#
# A scene claim: the name says the function belongs to one scene of the vocabulary the
# per-call scenes use (scenes.py). `battleObject_`/`battleState_` are NOT scene claims -- they
# name a struct family -- so they are handled below.
SCENE_CLAIMS: dict[str, str] = {
    "battle": "battle",
    "overworld": "overworld",
    "ow": "overworld",
    "menu": "menu",
    "submenu": "menu",
    "cutscene": "cutscene",
    "title": "title",
}

#: `battle` + one of these is a struct family, not a scene (`battleObject_`, `battleState_`).
BATTLE_STRUCT_SECOND = ("object", "state", "data", "ui")

# A struct claim: the name says the function works on one struct family. The value is the
# families the claim covers (the per-table copies of the battle object count as one family).
STRUCT_CLAIMS: dict[str, tuple[str, ...]] = {
    "battleobject": ("oBattleObject", "oT1BattleObject", "oT3BattleObject", "oT4BattleObject"),
    "owplayer": ("oOWPlayerObject",),
    "chatbox": ("oChatbox",),
    "renderinfo": ("oRenderInfo",),
    "joypad": ("oJoypad",),
    "overworldmapobject": ("oOverworldMapObject",),
    "aiattack": ("oAIAttackVars",),
    "ai": ("oAIAttackVars", "oAIData", "oAIState"),
    "gamestate": ("oGameState",),
    "battlestate": ("oBattleState",),
    "collisiondata": ("oCollisionData",),
    "camera": ("oCamera",),
    "paneldata": ("oPanelData",),
    "mapscript": ("oMapScriptState",),
}


def head_words(name: str) -> list[str]:
    """A name split into words: `battleObjectHandleChargeAttack` -> battle, Object, Handle,
    Charge, Attack; `battle_drawBcdNumberGrid` -> battle, draw, Bcd, ..."""
    return re.findall(r"[A-Z]+(?![a-z])|[A-Z][a-z0-9]*|[a-z0-9]+", name)


def lead(name: str) -> str:
    """The leading word of a name, lowercased."""
    words = head_words(name)
    return words[0].lower() if words else ""


def scene_claim(name: str) -> str | None:
    """The scene the name claims, or None when the name claims none."""
    words = head_words(name)
    first = words[0].lower() if words else ""
    if first == "battle" and len(words) > 1 and words[1].lower() in BATTLE_STRUCT_SECOND:
        return None                      # a struct family, not a scene
    return SCENE_CLAIMS.get(first)


def struct_claim(name: str) -> tuple[str, ...] | None:
    """The struct families the name claims, or None when it claims none."""
    words = head_words(name)
    if len(words) < 2:
        return None
    compound = (words[0] + words[1]).lower()
    if compound in STRUCT_CLAIMS:
        return STRUCT_CLAIMS[compound]
    if words[0].lower() in STRUCT_CLAIMS:
        return STRUCT_CLAIMS[words[0].lower()]
    return None


def family_of(field_name: str) -> str:
    """The struct family a `o*` field name belongs to: `oT3BattleObject_Timer` -> oT3BattleObject."""
    return field_name.split("_", 1)[0]


class Row:
    """One function's row of a run's report.tsv."""

    def __init__(self, fields: list[str]) -> None:
        self.address, self.name, self.proposed, self.status = fields[:4]
        self.score, self.check = fields[4], fields[5]
        self.tokens = int(fields[6])

    @property
    def named(self) -> bool:
        return self.proposed != "-"


def read_run(run_id: str) -> dict[str, Row]:
    rows: dict[str, Row] = {}
    for line in (INVESTIGATE / run_id / "report.tsv").read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        row = Row(line.split("\t"))
        rows[row.name] = row
    return rows


def totals(rows: dict[str, Row]) -> tuple[int, int, int, int]:
    """(functions, named, probable+agree, tokens)."""
    return (len(rows),
            sum(1 for row in rows.values() if row.named),
            sum(1 for row in rows.values() if row.status == "probable" and row.check == "agree"),
            sum(row.tokens for row in rows.values()))


class Facts:
    """The v2 facts of one function, as the name-quality check needs them."""

    def __init__(self, record: dict) -> None:
        self.captured = bool(record.get("scenes_at_call"))
        scenes = Counter(call["scene"] for call in record.get("scenes_at_call") or [])
        self.scenes = scenes
        self.fields: list[str] = []
        for write in record.get("field_writes") or []:
            if not write.get("attributed"):
                continue
            for name in write.get("field_names") or []:
                if name not in self.fields:
                    self.fields.append(name)
        self.callers: list[str] = []
        for route in record.get("routes") or []:
            for caller in route.get("callers") or []:
                label = caller.get("name") or caller.get("caller") or "?"
                entry = f"{label} ({caller.get('count', 0)})"
                if entry not in self.callers:
                    self.callers.append(entry)

    def hint(self) -> str:
        if not self.captured:
            return "no captured calls (uncaptured)"
        scenes = ", ".join(f"{scene} x{n}" for scene, n in self.scenes.most_common())
        fields = ", ".join(self.fields[:6]) or "no attributed field"
        callers = ", ".join(self.callers[:3]) or "no runtime caller"
        return f"scenes[{scenes}] fields[{fields}] callers[{callers}]"

    def scene_contradiction(self, name: str) -> str | None:
        """The scene the name claims, when the captured calls were never in it -- otherwise None.

        A `cutscene` name against an `overworld` observation is reported apart (`cutscene`):
        the cutscene commands are dispatched from the map handler, so the mode bytes say
        `overworld` while the code is plainly cutscene code. That is a limit of the scene
        vocabulary, not a wrong name."""
        claim = scene_claim(name)
        if claim is None or not self.captured:
            return None
        if claim in self.scenes:
            return None
        if claim == "cutscene" and "overworld" in self.scenes:
            return "cutscene"
        return claim

    def struct_contradiction(self, name: str) -> bool:
        claim = struct_claim(name)
        if claim is None or not self.fields:
            return False
        written = {family_of(field) for field in self.fields}
        return not (written & set(claim))


def contradiction_marks(facts: Facts | None, name: str) -> str:
    """The contradiction marks for one name: `SCENE` (the scene it claims was never seen),
    `CUTSCENE` (a cutscene name whose calls the mode bytes called overworld) and `STRUCT`
    (it claims a struct family it was never seen writing)."""
    if facts is None:
        return ""
    marks = ""
    scene = facts.scene_contradiction(name)
    if scene == "cutscene":
        marks += "CUTSCENE "
    elif scene:
        marks += "SCENE "
    if facts.struct_contradiction(name):
        marks += "STRUCT "
    return marks.strip()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--facts-dir", type=Path, default=None,
                        help="the v2 facts directory (default: build/facts-v2 in this worktree)")
    parser.add_argument("--pairs", type=int, default=0,
                        help="how many side-by-side rows to print (0: all)")
    args = parser.parse_args()

    facts_dir = args.facts_dir or Path(__file__).resolve().parents[3] / "build/facts-v2"

    runs = {arm: (read_run(a), read_run(b)) for arm, (a, b) in ARMS.items()}

    print("Three arms, 120 functions each (proposer and checker minimax/MiniMax-M3):\n")
    header = f"  {'arm':10s} {'run A':32s} {'run B':32s} {'proposed':>9s} {'prob+agree':>10s} {'tokens':>10s}"
    print(header)
    for arm in ORDER:
        a, b = runs[arm]
        ta, tb = totals(a), totals(b)
        merged = a | b
        tm = totals(merged)
        print(f"  {arm:10s} {ARMS[arm][0]:32s} {ARMS[arm][1]:32s}")
        print(f"  {'':10s} A: {ta[1]:2d} named, {ta[2]:2d} probable+agree, {ta[3]:7,d} tokens"
              f"   |   B: {tb[1]:2d} named, {tb[2]:2d} probable+agree, {tb[3]:7,d} tokens"
              f"   |   both: {tm[1]:2d} named, {tm[2]:2d} probable+agree, {tm[3]:7,d} tokens "
              f"({tm[3] // max(tm[0], 1):,}/fn)")
    print()

    # the facts of every function
    facts: dict[str, Facts] = {}
    for path in sorted((facts_dir / "by_address").glob("*.json")):
        record = json.loads(path.read_text())
        facts[record["name"]] = Facts(record)

    union = sorted({name for arm in ORDER for run in runs[arm]
                    for name, row in run.items() if row.named})

    print(f"Names side by side ({len(union)} functions named in at least one arm); "
          f"marks: SCENE = the name claims a scene no captured call was in, "
          f"STRUCT = it claims a struct family no attributed field write belongs to\n")
    print(f"  {'function':14s} {'without':44s} {'v1':44s} {'v2':44s}")
    print(f"  {'':14s} {'marks':44s} {'marks':44s} {'marks':44s}")
    counts = {arm: Counter() for arm in ORDER}
    for name in union:
        cells = []
        for arm in ORDER:
            merged = runs[arm][0] | runs[arm][1]
            row = merged.get(name)
            if row is None or not row.named:
                cells.append(("-" * 44, ""))
                continue
            marks = contradiction_marks(facts.get(name), row.proposed)
            if marks:
                scene = facts[name].scene_contradiction(row.proposed)
                if scene == "cutscene":
                    counts[arm]["cutscene"] += 1
                elif scene:
                    counts[arm]["scene"] += 1
                if facts[name].struct_contradiction(row.proposed):
                    counts[arm]["struct"] += 1
                counts[arm]["any"] += 1
            cells.append((f"{row.proposed} {marks}".ljust(44), marks))
        print(f"  {name:14s} " + " ".join(cell[0] for cell in cells))
        fact = facts.get(name)
        if fact is not None:
            print(f"  {'':14s} facts: {fact.hint()}")
    print()

    print("Contradictions per arm (over the names that arm proposed):\n")
    print(f"  {'arm':10s} {'named':>6s} {'checkable':>9s} {'scene':>6s} {'cutscene':>9s} {'struct':>7s} {'any':>5s}")
    for arm in ORDER:
        merged = runs[arm][0] | runs[arm][1]
        named = sum(1 for row in merged.values() if row.named)
        checkable = sum(1 for name, row in merged.items() if row.named
                        and facts.get(name) is not None and facts[name].captured)
        print(f"  {arm:10s} {named:6d} {checkable:9d} {counts[arm]['scene']:6d} "
              f"{counts[arm]['cutscene']:9d} {counts[arm]['struct']:7d} {counts[arm]['any']:5d}")

    print("\n  the contradicted names, per arm:")
    for arm in ORDER:
        merged = runs[arm][0] | runs[arm][1]
        print(f"    {arm}:")
        for name in sorted(merged):
            row = merged[name]
            if not row.named:
                continue
            marks = contradiction_marks(facts.get(name), row.proposed)
            if marks:
                fact = facts[name]
                print(f"      {name:14s} {row.proposed:44s} {marks:22s} {fact.hint()}")

    print("\nSplit on whether the function's calls were captured (so the block could say more"
          " than route coverage):\n")
    print(f"  {'arm':10s} {'group':10s} {'n':>4s} {'named':>6s} {'prob+agree':>11s}")
    for arm in ORDER:
        merged = runs[arm][0] | runs[arm][1]
        for label, want in (("captured", True), ("uncaptured", False)):
            group = [row for name, row in merged.items()
                     if (facts.get(name) is not None and facts[name].captured) == want]
            named = sum(1 for row in group if row.named)
            probable = sum(1 for row in group if row.status == "probable" and row.check == "agree")
            print(f"  {arm:10s} {label:10s} {len(group):4d} {named:6d} {probable:11d}")

    print("\nThe same function, named in both arms:\n")
    print(f"  {'pair':22s} {'both named':>11s} {'same name':>10s} {'different':>9s}")
    for left, right in (("without", "v1"), ("without", "v2"), ("v1", "v2")):
        lrows = runs[left][0] | runs[left][1]
        rrows = runs[right][0] | runs[right][1]
        both = [name for name in lrows if lrows[name].named and rrows.get(name) and rrows[name].named]
        same = sum(1 for name in both if lrows[name].proposed == rrows[name].proposed)
        print(f"  {left + ' vs ' + right:22s} {len(both):11d} {same:10d} {len(both) - same:9d}")


if __name__ == "__main__":
    main()
