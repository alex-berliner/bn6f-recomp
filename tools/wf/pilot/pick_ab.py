#!/usr/bin/env python3
"""The A/B pilot's two halves: 120 reached sub_* functions no earlier investigate run named, split
at random. Reads build/facts/by_address (reached) and build/investigate/*/report.tsv (what the
earlier runs proposed), writes the two --only-file lists and prints what went into them.

    python pilot/pick_ab.py [--seed 20261021] [--count 120] [--out DIR]
"""
import argparse
import json
import random
from pathlib import Path

REPO = Path("/home/alex/Code/recomp")
FACTS = REPO / "build/facts/by_address"
INVESTIGATE = REPO / "build/investigate"
SCORES = REPO / "symbols/meaning_scores.tsv"


def proposed_names() -> set[str]:
    """Every name an earlier investigate run proposed (report.tsv's `proposed` column, not `-`)."""
    out: set[str] = set()
    for report in INVESTIGATE.glob("*/report.tsv"):
        for line in report.read_text().splitlines():
            if line.startswith("#") or not line.strip():
                continue
            fields = line.split("\t")
            if len(fields) > 2 and fields[2] not in ("-", ""):
                out.add(fields[1])
    return out


def reached_sub() -> list[str]:
    """The placeholder functions meaning_scores.tsv lists whose `wf facts` record says a recorded
    route reached them."""
    out: list[str] = []
    for line in SCORES.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        fields = line.split("\t")
        if len(fields) < 4 or fields[1] != "function" or not fields[2].startswith("sub_"):
            continue
        if int(fields[3]) > 1:
            continue
        record = FACTS / f"0x{int(fields[0], 16) & ~1:08X}.json"
        if not record.exists():
            continue
        try:
            if json.loads(record.read_text()).get("reached") is True:
                out.append(fields[2])
        except json.JSONDecodeError:
            continue
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=20261021)
    parser.add_argument("--count", type=int, default=120)
    parser.add_argument("--out", type=Path, default=Path(__file__).resolve().parent)
    given = parser.parse_args()

    named = proposed_names()
    eligible = [name for name in reached_sub() if name not in named]
    picked = sorted(random.Random(given.seed).sample(eligible, given.count))
    half = len(picked) // 2
    groups = {"armA": picked[:half], "armB": picked[half:]}
    for arm, names in groups.items():
        path = given.out / f"ab-{arm}.txt"
        path.write_text("\n".join(names) + "\n")
        print(f"{path}: {len(names)} functions")
    print(f"reached sub_* functions no earlier run named: {len(eligible)}; picked {len(picked)} of them")


if __name__ == "__main__":
    main()
