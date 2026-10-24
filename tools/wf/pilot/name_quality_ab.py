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
import os
from collections import Counter
from pathlib import Path

from wf.tools import facts as facts_layer
from wf.tools import facts_check

REPO = Path("/home/alex/Code/recomp")
MAIN_INVESTIGATE = REPO / "build/investigate"
WORKTREE_INVESTIGATE = REPO / "build-wf-worktrees/inv-facts3/build/investigate"
INVESTIGATE = Path(os.environ.get("WF_INVESTIGATE_DIR", WORKTREE_INVESTIGATE))

#: arm name -> (run-id a, run-id b, base directory for both). The first three arms live in the
#: main checkout's build/investigate/; the v3 arms live in this worktree's build/investigate/.
ARMS: dict[str, tuple[str, str, Path]] = {
    "without": ("investigate-20261021-101004", "investigate-20261021-101527", MAIN_INVESTIGATE),
    "v1":     ("investigate-20261021-100724", "investigate-20261021-101229", MAIN_INVESTIGATE),
    "v2":     ("investigate-20261024-064512", "investigate-20261024-064746", MAIN_INVESTIGATE),
    "v3":     ("investigate-v3-armA", "investigate-v3-armB", WORKTREE_INVESTIGATE),
}
ORDER = ("without", "v1", "v2", "v3")

class Row:
    """One function's row of a run's report.tsv."""

    def __init__(self, fields: list[str]) -> None:
        self.address, self.name, self.proposed, self.status = fields[:4]
        self.score, self.check = fields[4], fields[5]
        self.tokens = int(fields[6])

    @property
    def named(self) -> bool:
        return self.proposed != "-"


def read_run(run_id: str, base: Path) -> dict[str, Row]:
    rows: dict[str, Row] = {}
    path = base / run_id / "report.tsv"
    if not path.exists():
        raise FileNotFoundError(f"report.tsv missing for {run_id} under {base}")
    for line in path.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        # v3 added a `rejection` column on top of v2's nine.  Read by position, so older reports
        # without `retries` would break here; the table handles it through the Row required fields.
        fields = line.split("\t")
        # the rejection column is the last one.  rows from runs before v2 may have fewer
        # columns; pad with `-` so legacy rows still parse.
        while len(fields) < 9:
            fields.append("-")
        row = Row(fields)
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

    def __init__(self, record: dict[str, object]) -> None:
        self.record = facts_layer.FunctionFacts.model_validate(record)
        self.captured = bool(self.record.scenes_at_call)
        self.scenes = Counter(call.scene for call in self.record.scenes_at_call)
        self.fields: list[str] = []
        for write in self.record.field_writes:
            if not write.attributed:
                continue
            for name in write.field_names:
                if name not in self.fields:
                    self.fields.append(name)
        self.callers: list[str] = []
        for route in self.record.routes:
            for caller in route.callers:
                entry = f"{caller.name} ({caller.count})"
                if entry not in self.callers:
                    self.callers.append(entry)

    def hint(self) -> str:
        if not self.captured:
            return "no captured calls (uncaptured)"
        scenes = ", ".join(f"{scene} x{n}" for scene, n in self.scenes.most_common())
        fields = ", ".join(self.fields[:6]) or "no attributed field"
        callers = ", ".join(self.callers[:3]) or "no runtime caller"
        return f"scenes[{scenes}] fields[{fields}] callers[{callers}]"

    def contradictions(self, name: str) -> tuple[facts_check.Contradiction, ...]:
        """The package's typed, mechanical check for this name."""
        return facts_check.contradictions(name, self.record)


def contradiction_marks(facts: Facts | None, name: str) -> str:
    """The contradiction marks for one name: `SCENE` (the per-call majority is another
    scene) and `STRUCT` (the attributed writes belong to another family)."""
    if facts is None:
        return ""
    return " ".join(issue.kind.upper() for issue in facts.contradictions(name))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--facts-dir", type=Path, default=None,
                        help="the v2 facts directory (default: build/facts-v2 in this worktree)")
    parser.add_argument("--pairs", type=int, default=0,
                        help="how many side-by-side rows to print (0: all)")
    args = parser.parse_args()

    facts_dir = args.facts_dir or Path(__file__).resolve().parents[3] / "build/facts-v2"

    runs = {arm: (read_run(arm_a, base), read_run(arm_b, base))
            for arm, (arm_a, arm_b, base) in ARMS.items()}

    print("Four arms, 120 functions each (proposer and checker minimax/MiniMax-M3 for "
          "without/v1/v2; hyper/minimax-m3 for v3):\n")
    header = f"  {'arm':10s} {'run A':32s} {'run B':32s} {'proposed':>9s} {'prob+agree':>10s} {'tokens':>10s}"
    print(header)
    model_note = {"without": "minimax/MiniMax-M3", "v1": "minimax/MiniMax-M3",
                  "v2": "minimax/MiniMax-M3", "v3": "hyper/minimax-m3"}
    for arm in ORDER:
        a, b = runs[arm]
        ta, tb = totals(a), totals(b)
        merged = a | b
        tm = totals(merged)
        print(f"  {arm:10s} {ARMS[arm][0]:32s} {ARMS[arm][1]:32s} ({model_note[arm]})")
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
    print(f"  {'function':14s} {'without':44s} {'v1':44s} {'v2':44s} {'v3':44s}")
    print(f"  {'':14s} {'marks':44s} {'marks':44s} {'marks':44s} {'marks':44s}")
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
                for issue in facts[name].contradictions(row.proposed):
                    counts[arm][issue.kind] += 1
                counts[arm]["any"] += 1
            cells.append((f"{row.proposed} {marks}".ljust(44), marks))
        print(f"  {name:14s} " + " ".join(f"{cell[0]:44s}" for cell in cells))
        fact = facts.get(name)
        if fact is not None:
            print(f"  {'':14s} facts: {fact.hint()}")
    print()

    print("Contradictions per arm (over the names that arm proposed):\n")
    print(f"  {'arm':10s} {'named':>6s} {'checkable':>9s} {'scene':>6s} {'struct':>7s} {'any':>5s}")
    for arm in ORDER:
        merged = runs[arm][0] | runs[arm][1]
        named = sum(1 for row in merged.values() if row.named)
        checkable = sum(1 for name, row in merged.items() if row.named
                        and facts.get(name) is not None and facts[name].captured)
        print(f"  {arm:10s} {named:6d} {checkable:9d} {counts[arm]['scene']:6d} "
              f"{counts[arm]['struct']:7d} {counts[arm]['any']:5d}")

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
    for left, right in (("without", "v1"), ("without", "v2"), ("without", "v3"),
                        ("v1", "v2"), ("v1", "v3"), ("v2", "v3")):
        lrows = runs[left][0] | runs[left][1]
        rrows = runs[right][0] | runs[right][1]
        both = [name for name in lrows if lrows[name].named and rrows.get(name) and rrows[name].named]
        same = sum(1 for name in both if lrows[name].proposed == rrows[name].proposed)
        print(f"  {left + ' vs ' + right:22s} {len(both):11d} {same:10d} {len(both) - same:9d}")


if __name__ == "__main__":
    main()
