#!/usr/bin/env python3
"""The A/B pilot's table: what `wf investigate` proposed with and without the runtime-facts block,
per arm and over both, and the functions both arms named (differently) side by side.

    python pilot/summarise_ab.py --run-with-facts RUN --run-without RUN ...   # arm A
    python pilot/summarise_ab.py --run-with-facts RUN --run-without RUN ... --pairs 15

Reads each run's build/investigate/RUN/report.tsv (the address, the old name, the proposed name,
the candidate's status, the self score, the cross-check's verdict and the tokens of both calls).
"""
import argparse
from pathlib import Path

REPO = Path("/home/alex/Code/recomp")
INVESTIGATE = REPO / "build/investigate"


class Row:
    """One function's row of a run's report.tsv."""

    def __init__(self, fields: list[str]) -> None:
        self.address, self.name, self.proposed, self.status, self.score, self.check = fields[:6]
        self.tokens = int(fields[6])

    @property
    def named(self) -> bool:
        return self.proposed != "-"


def read_run(run_id: str) -> dict[str, Row]:
    """One run's rows, by old name."""
    rows: dict[str, Row] = {}
    for line in (INVESTIGATE / run_id / "report.tsv").read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        row = Row(line.split("\t"))
        rows[row.name] = row
    return rows


def totals(rows: dict[str, Row]) -> tuple[int, int, int, int]:
    """(functions, proposed a name, probable with the checker's agreement, tokens)."""
    proposed = sum(1 for row in rows.values() if row.named)
    probable = sum(1 for row in rows.values() if row.status == "probable")
    return len(rows), proposed, probable, sum(row.tokens for row in rows.values())


def arm(with_facts: str, without: str) -> tuple[str, dict[str, Row], dict[str, Row], list[str]]:
    """One arm's two runs: what each produced and how the functions both named line up."""
    facts, plain = read_run(with_facts), read_run(without)
    shown = []
    for label, rows in (("with facts", facts), ("without facts", plain)):
        count, proposed, probable, tokens = totals(rows)
        shown.append(f"  {label:14s} {count:3d} functions, {proposed:3d} proposed, {probable:3d} probable+agree, "
                     f"{tokens:7,d} tokens")
    both = sorted(name for name in facts
                  if facts[name].named and plain.get(name) and plain[name].named)
    only_facts = sorted(name for name in facts if facts[name].named
                        and not (plain.get(name) and plain[name].named))
    only_plain = sorted(name for name in plain if plain[name].named
                        and not (facts.get(name) and facts[name].named))
    print(f"arm {with_facts} (facts) / {without} (no facts):")
    print("\n".join(shown))
    print(f"  {len(both)} functions named in both runs ({len(only_facts)} only with facts, "
          f"{len(only_plain)} only without)\n")
    return "".join(shown), facts, plain, only_facts + only_plain


def facts_hint(row: Row) -> str:
    """The one line of a function's runtime facts that judges a name: the scenes it runs in and the
    RAM it writes (empty when `wf facts` holds no record for it)."""
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from wf.tools import investigate                                    # noqa: PLC0415  (the worktree's)
    record = investigate.facts_record(int(row.address, 16))
    if record is None or not record.reached:
        return "(no runtime facts)"
    block = investigate.runtime_facts_text(record, row.name)         # type: ignore[arg-type]
    keep = [line.strip() for line in block.splitlines()
            if line.strip().startswith(("scenes it runs in", "RAM symbols it wrote", "I/O it wrote",
                                        "runtime callers", "arguments:"))]
    return "; ".join(part.split(": ", 1)[1] for part in keep[:3])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-with-facts", action="append", required=True,
                        help="a run with --facts (once per arm)")
    parser.add_argument("--run-without", action="append", required=True,
                        help="the same arm without --facts")
    parser.add_argument("--pairs", type=int, default=15, help="how many side-by-side names to show")
    parser.add_argument("--facts-hints", action="store_true",
                        help="with each pair, the runtime facts it was named from")
    given = parser.parse_args()

    print("A/B: `wf investigate` with and without the runtime-facts block "
          "(proposer and checker both minimax/MiniMax-M3)\n")
    arms = [arm(a, b) for a, b in zip(given.run_with_facts, given.run_without, strict=True)]

    print("both arms together:")
    for label in ("with facts", "without facts"):
        rows: dict[str, Row] = {}
        for _, facts, plain, _ in arms:
            rows |= facts if label == "with facts" else plain
        count, proposed, probable, tokens = totals(rows)
        print(f"  {label:14s} {count:3d} functions, {proposed:3d} proposed, {probable:3d} probable+agree, "
              f"{tokens:7,d} tokens")

    pairs: list[tuple[str, Row, Row]] = []
    for _, facts, plain, _ in arms:
        for name in sorted(set(facts) & set(plain)):
            if facts[name].named and plain[name].named and facts[name].proposed != plain[name].proposed:
                pairs.append((name, facts[name], plain[name]))
    print(f"\nnamed differently in both arms ({len(pairs)}; the first {given.pairs}):")
    print(f"  {'function':18s} {'with facts':38s} without facts")
    for name, with_row, without_row in pairs[:given.pairs]:
        print(f"  {name:18s} {with_row.proposed:38s} {without_row.proposed}")
        if given.facts_hints:
            print(f"    {with_row.address} facts: {facts_hint(with_row)}")


if __name__ == "__main__":
    main()