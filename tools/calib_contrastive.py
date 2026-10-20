#!/usr/bin/env python3
"""Calibrate the contrastive runtime-evidence judge (tools/wf/wf/tools/runtime_evidence.py:
CONTRASTIVE_SYSTEM, CONTRASTIVE_TEMPLATE, build_contrastive_candidates, contrastive_judge_prompt,
parse_contrastive, ask_contrastive_judge).

The previous version of `ask_judge` was a rubber stamp: over 24 negative-control pairs
(the runtime facts of function A presented with the (name, summary) of function B), it said
`consistent` 14 times. The verdict carried no information. The fix was to flip the question
from "is this name consistent with the facts?" to "pick the candidate that fits these facts"
-- a multiple-choice question with 5 options: 1 truth + 4 decoys.

This script measures the contrastive judge on two bars (per the brief):

  (a) accuracy at picking the TRUE option among 5 (chance = 20%);
  (b) with the true option REMOVED (4 decoys only, the right answer is 0/none),
      how often it still picks one at confidence >= 0.7 (false-positive rate; should be low).

Data: 300 functions in build-wf-runs/runtime-evidence-20261019-180951/ (every one has a matching
symbols/db/NAME.yaml; 60 of them are sampled with a fixed seed). The runtime-evidence yaml files
are not valid YAML (the previous batch's hand-rolled f-string format broke on colon-space in the
judge's reason text -- line 47 of e.g. ai_attackFrontPanel.yaml has `reason:` followed by a
paragraph that contains `... matches: r5 is ...`); we recover the `detail:` text with a regex
the way neg_control.py does. The true name+summary come from symbols/db/NAME.yaml which is
valid.

The script is resumable: results accumulate in CALIB_RESULTS (default
/tmp/claude-1000/-home-alex-Code-recomp/9585631a-4f29-4891-aaaa-3eb8ba1d5242/scratchpad/calib_results.json)
so a re-run skips completed functions. Each function costs 2 model calls (the 5-option question
and the 4-decoy question); we parallelise across --jobs workers.

  tools/calib_contrastive.py [--count N] [--seed S] [--jobs J] [--model M]

Honest reporting rule: per the brief, if (a) is not clearly above chance (20%) and (b) is not
clearly low, we say the runtime facts are not discriminating enough and we DO NOT tune on the
evaluation set. The CI is the Wilson 95% interval.
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import json
import os
import random
import re
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import NamedTuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent / "wf"))

import yaml

import llm  # tools/llm.py
from wf.tools import runtime_evidence as RE
from wf.types import (FunctionName, GuestAddress, ModelSpec, ToolMessage, SymbolName)


# The script lives in the worktree but reads runtime evidence from the main tree's
# build-wf-runs/ (the dir is gitignored, so it doesn't propagate to worktrees). The
# env var overrides the default -- handy for tests.
REPO = Path(os.environ.get("RECOMP_REPO", str(Path(__file__).resolve().parent.parent)))
RUN_DIR = (Path(REPO) / "build-wf-runs/runtime-evidence-20261019-180951").resolve()
DB_DIR = Path(REPO) / "symbols/db"
RESULTS_PATH = Path("/tmp/claude-1000/-home-alex-Code-recomp/9585631a-4f29-4891-aaaa-3eb8ba1d5242/scratchpad/calib_results.json")
DEFAULT_MODEL = "minimax/MiniMax-M3"
DEFAULT_COUNT = 60
DEFAULT_SEED = 7                # matches neg_control.py's seed
DEFAULT_JOBS = 4
CONFIDENCE_THRESHOLD = 0.7       # the brief's "confident contradicting pick" threshold
DETAIL_RE = re.compile(r"detail: \|\n(.*?)\n    (?:refs|reproduce|result)", re.S)


# ── detail-text parser ─────────────────────────────────────────────────────


def _strip_indent(text: str) -> str:
    """The detail text in the runtime-evidence yaml is inside a 6-space block scalar; strip
    that prefix per line so the parser sees the same indentation as `_runtime_facts_text`."""
    return "\n".join(l[6:] if len(l) >= 6 else l for l in text.splitlines())


def extract_detail(raw_text: str) -> str:
    """Pull the `detail:` body out of one runtime-evidence yaml. The files are not valid yaml
    (the judge's reason text earlier in the same file contains `colon-space` which the scanner
    treats as a mapping key) -- a regex is the only way."""
    m = DETAIL_RE.search(raw_text)
    return _strip_indent(m.group(1)) if m else ""


def parse_detail_to_facts(detail: str, name: str, address: int) -> RE.RuntimeFacts:
    """Build a `RuntimeFacts` from the parsed `detail` text. We do not need exact equivalence
    with a fresh probe -- the contrastive judge sees only the rendered text and uses the same
    fields. What we DO need: probes/returned counts (so the prompt can say "K of K"), register
    writes/inputs/outputs (the model reads these), and memory writes (build_contrastive_candidates
    uses these to pick hard decoys -- the RAM-symbol-overlap heuristic)."""
    lines = detail.splitlines()
    probes = 1
    returned = 0
    register_inputs: list[ToolMessage] = []
    register_outputs: list[ToolMessage] = []
    registers_written: list[ToolMessage] = []
    written_patterns: list[ToolMessage] = []
    memory_writes: list[RE.MemoryRegionWrite] = []
    io_writes: list[RE.IOWrite] = []
    route_coverage: list[ToolMessage] = []
    section: str | None = None
    for line in lines:
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("Confirmed (or contradicted)"):
            continue
        if stripped.startswith("Judge's verdict:"):
            break                                    # the rest is the verdict text; not facts
        if stripped.startswith("Registers the function changed"):
            m = re.search(r"of (\d+) probe\(s\), (\d+) returned", stripped)
            if m:
                probes, returned = int(m.group(1)), int(m.group(2))
            section = "registers"
            continue
        if stripped.startswith("Distinct register inputs/outputs"):
            section = "io_regs"
            continue
        if stripped.startswith("RAM writes, grouped") or stripped == "RAM writes:":
            section = "ram"
            continue
        if stripped.startswith("I/O writes:"):
            section = "io"
            continue
        if stripped.startswith("Recorded-route coverage:"):
            section = "routes"
            continue
        if stripped in ("(none)", "(none observed)"):
            continue
        if section == "registers" and stripped.startswith("writes "):
            registers_written.append(ToolMessage(stripped))
        elif section == "io_regs":
            if stripped.startswith("inputs "):
                register_inputs.append(ToolMessage(stripped))
            elif stripped.startswith("outputs "):
                register_outputs.append(ToolMessage(stripped))
        elif section == "ram":
            m = re.match(r"(\S+):\s*(\d+) bytes,\s*(\d+) distinct value\(s\)", stripped)
            if m:
                memory_writes.append(RE.MemoryRegionWrite(symbol=SymbolName(m.group(1)),
                                                            address=0,
                                                            bytes=int(m.group(2)),
                                                            distinct_values=int(m.group(3))))
        elif section == "io":
            m = re.match(r"(\S+)\s+\(0x([0-9A-Fa-f]+)\)\s*=\s*0x([0-9A-Fa-f]+)\s+\((\d+) bytes, in (\d+) probe\(s\)\)",
                         stripped)
            if m:
                io_writes.append(RE.IOWrite(name=SymbolName(m.group(1)),
                                              address=GuestAddress(int(m.group(2), 16)),
                                              size=int(m.group(4)),
                                              value=int(m.group(3), 16),
                                              matched=int(m.group(5))))
        elif section == "routes":
            route_coverage.append(ToolMessage(stripped))
    return RE.RuntimeFacts(name=FunctionName(name), address=GuestAddress(address),
                             probes=probes, returned=returned,
                             register_inputs=tuple(register_inputs),
                             register_outputs=tuple(register_outputs),
                             registers_written=tuple(registers_written),
                             written_patterns=tuple(written_patterns),
                             io_writes=tuple(io_writes), memory_writes=tuple(memory_writes),
                             route_coverage=tuple(route_coverage))


def _read_symbols_db_summary(name: str) -> str:
    """The function's true summary (the entry in symbols/db/{name}.yaml). An empty string if the
    entry is malformed; the contrastive judge still runs (the candidate just has no summary)."""
    p = DB_DIR / f"{name}.yaml"
    if not p.is_file():
        return ""
    try:
        d = yaml.safe_load(p.read_text())
    except yaml.YAMLError:
        return ""
    if not isinstance(d, dict):
        return ""
    s = d.get("summary")
    return str(s).strip() if isinstance(s, str) else ""


def _read_symbols_db_address(name: str) -> int:
    p = DB_DIR / f"{name}.yaml"
    if not p.is_file():
        return 0
    try:
        d = yaml.safe_load(p.read_text())
    except yaml.YAMLError:
        return 0
    if not isinstance(d, dict):
        return 0
    a = d.get("address")
    if isinstance(a, str):
        return int(a, 16) if a.lower().startswith("0x") else int(a)
    return int(a) if isinstance(a, int) else 0


# ── 5-option / 4-decoy question builders ────────────────────────────────


def _summaries_for(pool: frozenset[str]) -> dict[FunctionName, str]:
    """The summary for every decoy we'll consider (loading symbols/db lazily on demand)."""
    return {FunctionName(n): _read_symbols_db_summary(n) for n in pool}


def _five_option_question(name: str, summary: str, facts: RE.RuntimeFacts,
                            pool: frozenset[str],
                            summaries: dict[FunctionName, str],
                            rng: random.Random) -> list[RE.ContrastiveCandidate]:
    """Reuses `build_contrastive_candidates` so the candidate selection logic is identical to
    what a real `wf runtime-evidence --contrastive` run uses."""
    return RE.build_contrastive_candidates(FunctionName(name), summary, pool, facts,
                                            lambda n: summaries.get(FunctionName(n), ""), rng)


def _four_decoy_question(candidates: list[RE.ContrastiveCandidate],
                          rng: random.Random) -> list[RE.ContrastiveCandidate]:
    """The same 4 decoys the 5-option question had, but with the truth removed and the labels
    re-numbered 1..4 (and re-shuffled). The right answer is choice 0 (none fit) -- if the
    model still picks one at confidence >= 0.7, that's a false positive.
    The brief calls this "with the true option REMOVED"; re-using the same decoys (rather than
    a fresh random set) is what makes the comparison fair."""
    decoys = [c for c in candidates if not c.is_truth]
    labelled = [RE.ContrastiveCandidate(label=idx, name=c.name, summary=c.summary,
                                            is_truth=False) for idx, c in enumerate(decoys, start=1)]
    rng.shuffle(labelled)
    return labelled


# ── model call with the brief's 529 back-off ────────────────────────────


def _call_judge(model: str, prompt: str, system: str, max_tokens: int, tag: str) -> str:
    """Wrap llm.complete with the brief's 60s / 5-retry back-off for `529 overloaded` (MiniMax
    has been returning 529s when overloaded; the previous agent died on one)."""
    for attempt in range(5):
        try:
            return llm.complete(model, prompt, system=system, max_tokens=max_tokens,
                                 timeout=180, retries=0, tag=tag,
                                 usage_log=str(REPO / "tools/convert/usage.jsonl"))
        except Exception as error:                                  # noqa: BLE001
            msg = str(error)
            if "529" in msg or "overloaded" in msg.lower() or "HTTP 429" in msg:
                wait = 60 + 30 * attempt
                print(f"  {tag}: {msg[:120]}; sleeping {wait}s (attempt {attempt + 1}/5)",
                      file=sys.stderr)
                time.sleep(wait)
                continue
            raise
    raise RuntimeError(f"model kept returning 529/429 over 5 attempts: {tag}")


# ── one function's two questions ────────────────────────────────────────


@dataclass
class FunctionResult:
    """One function's calibration outcome: the 5-option question's choice+confidence, and the
    4-decoy question's choice+confidence. The `truth_picked` flag drives accuracy (a); the
    `false_positive` flag drives the false-positive rate (b)."""
    name: str
    truth_label: int
    five_choice: int
    five_confidence: float
    five_reason: str
    five_problem: str | None
    four_choice: int
    four_confidence: float
    four_reason: str
    four_problem: str | None

    @property
    def truth_picked(self) -> bool:
        return self.five_problem is None and self.five_choice == self.truth_label

    @property
    def false_positive(self) -> bool:
        return (self.four_problem is None
                and 1 <= self.four_choice <= 4
                and self.four_confidence >= CONFIDENCE_THRESHOLD)


def _evaluate_one(name: str, summary: str, facts: RE.RuntimeFacts,
                    pool: frozenset[str], summaries: dict[FunctionName, str],
                    seed: int) -> FunctionResult:
    """Two questions for one function:
      1) 5-option (truth + 4 decoys); record whether the model picked the truth.
      2) 4-decoy (no truth); record whether the model picked any at conf >= 0.7.
    The same seed-based RNG drives both so a re-run is reproducible."""
    rng = random.Random(seed)
    five = _five_option_question(name, summary, facts, pool, summaries, rng)
    truth_label = next((c.label for c in five if c.is_truth), 0)
    prompt5 = RE.contrastive_judge_prompt(facts, five)
    reply5 = _call_judge(DEFAULT_MODEL, prompt5, RE.CONTRASTIVE_SYSTEM, 600,
                          tag=f"calib5 {name}")
    choice5, conf5, reason5, problem5 = RE.parse_contrastive(reply5)

    four = _four_decoy_question(five, rng)
    prompt4 = RE.contrastive_judge_prompt(facts, four)
    reply4 = _call_judge(DEFAULT_MODEL, prompt4, RE.CONTRASTIVE_SYSTEM, 600,
                          tag=f"calib4 {name}")
    choice4, conf4, reason4, problem4 = RE.parse_contrastive(reply4)
    return FunctionResult(name=name, truth_label=truth_label,
                            five_choice=choice5, five_confidence=conf5,
                            five_reason=reason5[:300],
                            five_problem=str(problem5) if problem5 is not None else None,
                            four_choice=choice4, four_confidence=conf4,
                            four_reason=reason4[:300],
                            four_problem=str(problem4) if problem4 is not None else None)


# ── sample + dispatch ───────────────────────────────────────────────────


def _gather_functions() -> list[tuple[str, str, int, RE.RuntimeFacts]]:
    """The 300 functions with runtime-evidence + symbols/db entries, with their true name+summary
    and a parsed RuntimeFacts. Sorted so the seed picks the same 60 every time."""
    out: list[tuple[str, str, int, RE.RuntimeFacts]] = []
    for path in sorted(RUN_DIR.glob("*.yaml")):
        name = path.stem
        summary = _read_symbols_db_summary(name)
        if not summary:
            continue                                  # we need a summary for the contrastive judge
        address = _read_symbols_db_address(name)
        raw = path.read_text(errors="replace")
        detail = extract_detail(raw)
        if not detail:
            continue
        facts = parse_detail_to_facts(detail, name, address)
        out.append((name, summary, address, facts))
    return out


def _load_results() -> dict[str, dict]:
    """The resumable state. Each function's two questions' outcomes, keyed by name. Loaded from
    disk at start; saved after each function completes."""
    if RESULTS_PATH.is_file():
        try:
            return json.loads(RESULTS_PATH.read_text())
        except json.JSONDecodeError:
            return {}
    return {}


def _save_results(results: dict[str, dict]) -> None:
    RESULTS_PATH.parent.mkdir(parents=True, exist_ok=True)
    RESULTS_PATH.write_text(json.dumps(results, indent=2, sort_keys=True))


def _wilson_ci(successes: int, total: int) -> tuple[float, float]:
    """Wilson 95% CI for a binomial proportion (the brief's "CI [lo, hi]" notation)."""
    if total == 0:
        return (0.0, 0.0)
    from math import sqrt
    p = successes / total
    z = 1.96
    n = total
    denom = 1 + z * z / n
    centre = (p + z * z / (2 * n)) / denom
    half = (z * sqrt(p * (1 - p) / n + z * z / (4 * n * n))) / denom
    return (max(0.0, centre - half), min(1.0, centre + half))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--count", type=int, default=DEFAULT_COUNT,
                        help=f"how many functions to evaluate (default {DEFAULT_COUNT})")
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED,
                        help=f"sample seed (default {DEFAULT_SEED})")
    parser.add_argument("--jobs", type=int, default=DEFAULT_JOBS,
                        help=f"parallel model calls (default {DEFAULT_JOBS})")
    parser.add_argument("--model", type=str, default=DEFAULT_MODEL,
                        help=f"judge model (default {DEFAULT_MODEL})")
    args = parser.parse_args()

    funcs = _gather_functions()
    if not funcs:
        print(f"no functions found under {RUN_DIR}", file=sys.stderr)
        sys.exit(2)
    rng = random.Random(args.seed)
    rng.shuffle(funcs)
    sample = funcs[: args.count]
    print(f"sampled {len(sample)} functions from {len(funcs)} (seed={args.seed})")

    pool = RE.investigate_sources_names()
    if len(pool) < 5:
        print(f"the decoy pool is only {len(pool)} names; need at least 5", file=sys.stderr)
        sys.exit(2)
    summaries = _summaries_for(pool)

    results = _load_results()
    todo: list[tuple[str, str, int, RE.RuntimeFacts]] = []
    for name, summary, address, facts in sample:
        if name in results:
            continue
        todo.append((name, summary, address, facts))
    print(f"{len(todo)} to do, {len(results)} already in cache")

    def _run(args_tuple: tuple[str, str, int, RE.RuntimeFacts, int]) -> FunctionResult:
        name, summary, _addr, facts, seed = args_tuple
        # Re-pick the seed deterministically per function so re-runs hit the same shuffle; the
        # call to _five_option_question() will reuse the RNG via the `seed` argument below.
        # We pass the seed directly into _evaluate_one.
        return _evaluate_one(name, summary, facts, pool, summaries, seed)

    # Per-function seed: hash(name) so the truth position is reproducible.
    def _seed_for(name: str) -> int:
        return (hash(name) & 0xFFFFFFFF) ^ args.seed

    pending = [(name, summary, address, facts, _seed_for(name))
                for name, summary, address, facts in todo]
    started = time.monotonic()
    if pending:
        with cf.ThreadPoolExecutor(max_workers=args.jobs) as ex:
            futures = {ex.submit(_run, p): p[0] for p in pending}
            for fut in cf.as_completed(futures):
                name = futures[fut]
                try:
                    r = fut.result()
                except Exception as error:                              # noqa: BLE001
                    print(f"{name}: FAILED {error}", file=sys.stderr)
                    continue
                results[r.name] = {"truth_label": r.truth_label,
                                    "five_choice": r.five_choice,
                                    "five_confidence": r.five_confidence,
                                    "five_reason": r.five_reason,
                                    "five_problem": r.five_problem,
                                    "four_choice": r.four_choice,
                                    "four_confidence": r.four_confidence,
                                    "four_reason": r.four_reason,
                                    "four_problem": r.four_problem,
                                    "truth_picked": r.truth_picked,
                                    "false_positive": r.false_positive}
                _save_results(results)
                done = len(results)
                total = len(sample)
                print(f"[{done}/{total}] {name}: 5opt pick={r.five_choice} "
                       f"conf={r.five_confidence:.2f} {'OK' if r.truth_picked else 'MISS'} ; "
                       f"4dec pick={r.four_choice} conf={r.four_confidence:.2f} "
                       f"{'FP' if r.false_positive else 'no'}")

    elapsed = time.monotonic() - started
    print()
    print(f"=== calibration ({len(results)}/{len(sample)}) "
           f"in {elapsed:.0f}s on {DEFAULT_MODEL} ===")
    a_total = 0
    a_hits = 0
    b_total = 0
    b_fp = 0
    for r in results.values():
        a_total += 1
        if r["truth_picked"]:
            a_hits += 1
        b_total += 1
        if r["false_positive"]:
            b_fp += 1
    if a_total == 0:
        print("no completed runs")
        return
    a = a_hits / a_total
    b = b_fp / b_total
    a_lo, a_hi = _wilson_ci(a_hits, a_total)
    b_lo, b_hi = _wilson_ci(b_fp, b_total)
    print(f"(a) accuracy picking truth among 5: {a_hits}/{a_total} = {a:.1%}; "
           f"95% CI [{a_lo:.1%}, {a_hi:.1%}]; chance = 20%")
    print(f"(b) false-positive rate (truth removed, picked at conf >= {CONFIDENCE_THRESHOLD}): "
           f"{b_fp}/{b_total} = {b:.1%}; 95% CI [{b_lo:.1%}, {b_hi:.1%}]")
    print()
    # The brief's calibration rule: BOTH (a) must be clearly above chance and (b) clearly low.
    # "Clearly above chance" = the lower CI bound is at least, say, 30% (the chance is 20%;
    # 30% would mean the *entire CI* is in the band that excludes chance). "Clearly low" =
    # the upper CI bound is well below 0.20. If either is not clearly so, the runtime facts
    # are not discriminating enough; do NOT tune on it.
    a_clr = a_lo >= 0.30
    b_clr = b_hi <= 0.20
    if a_clr and b_clr:
        print("VERDICT: (a) clearly above chance AND (b) clearly low.")
    elif not a_clr and not b_clr:
        print("VERDICT: (a) is not clearly above chance and (b) is not clearly low.")
        print("Per the brief, the runtime facts are not discriminating enough to deploy the")
        print("contrastive judge; do NOT tune on this evaluation set.")
    elif not a_clr:
        print("VERDICT: (a) is not clearly above chance; (b) is clearly low.")
        print("Per the brief, this is not enough to deploy the contrastive judge.")
    else:
        print("VERDICT: (a) is clearly above chance; (b) is not clearly low.")
        print("Per the brief, the false-positive rate is too high to deploy the contrastive")
        print("judge; do NOT tune on this evaluation set.")
    print()
    print(f"per-function results saved to {RESULTS_PATH}")


if __name__ == "__main__":
    main()