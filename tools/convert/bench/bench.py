#!/usr/bin/env python3
"""The standing benchmark of the farm's prompts (review 3 Area 6.3; joint plan S2 "Measurement").

  tools/wf/.venv/bin/python tools/convert/bench/bench.py run RUN [--arms typed,old] [--model M] [--jobs 3]
                                                             [--only F,G] [--difftest PATH]
  tools/wf/.venv/bin/python tools/convert/bench/bench.py summary RUN [RUN...] [--tsv OUT.tsv]
  tools/wf/.venv/bin/python tools/convert/bench/bench.py retest RUN [--jobs 8]

Every function of tools/convert/bench/functions.tsv (not excluded there) goes to every arm with the same
model, output cap (the farm's, wf.tools.models.MAX_TOKENS) and headers: one round, plus one repair when
the candidate fails a check (as the farm does: a reply without code is not asked again, review 3 B3).
Candidates go through the farm's acceptance test (tools/convert/farm.py test(): style and review rules,
the register check when the arm has it, compile, difftest on the captured calls and on random inputs).

Arms (ARMS below; rerun after every prompt, header or model change, and add an arm for a new prompt):
  typed     the farm's live prompt (farm.py SYSTEM, CONVENTIONS, TEMPLATE, REPAIR), register check on
  old       the prompt before 329fbfd, register-form C allowed (prompt_old.py), register check off
  cards     the live prompt plus a card (symbols/db summary and interface) per function it calls, register
            check on (review 3 A28/B22)
  handover  the live prompt, but round 0 is the farm's repair prompt on the best candidate another model
            wrote before (pool.py's candidates and ranking; its register-check and behaviour reports), then
            one repair (review 3 B31); a function with no earlier candidate gets the plain first prompt
  (cards and handover are not in the default --arms)

Where things go:
  - tools/convert/log/bench/RUN/ (ignored): results.jsonl, one row per round with the code, the report and
    the tokens; retest.jsonl; the difftest build log. A rerun of `run` finishes what is unfinished: pairs
    cut short, and rounds a provider error ended.
  - tools/convert/usage.jsonl (llm.py's shared token log, so plan-unit costs see these calls), tagged
    `bench-ARM FUNC rN RUN` (tools/convert/scorecard.py counts them as "other", not as farm rounds).
  - typed, cards and handover rounds are real farm rounds (typed C, register check on): also logged to
    tools/convert/log/FUNC.MODEL.jsonl (kind "bench ARM"), and a passing candidate goes to
    tools/convert/accepted/FUNC.MODEL.c (unless that file exists), so tools/convert/pool.py and the
    installs pick it up.
  - old-arm candidates are register form and must not be installed: they stay in the run folder only.
The difftest is built once per run from the last commit (build-wf-worktrees/bench), as `farm batch` does.
`retest` runs the acceptance test again on every saved candidate with today's tree, no model calls;
`summary` then judges by those outcomes.
"""
import argparse
import json
import random
import subprocess
import sys
import tempfile
import threading
import time
import traceback
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from functools import cache
from pathlib import Path
from types import ModuleType, SimpleNamespace
from typing import NamedTuple

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tools/wf"))
from wf.tools.build import build_host_difftest, committed_tree  # noqa: E402
from wf.tools.models import (ACCEPTED_DIR, LOG_DIR, MAX_TOKENS, USAGE_LOG, ConversionContext,  # noqa: E402
                             conversion_context, environment_hash, first_prompt, repair_prompt)
from wf.types import BuildFailure, FunctionName  # noqa: E402
import farm  # noqa: E402  (tools/convert, on the path through wf.tools.models)
sys.path.insert(0, str(HERE))
import prompt_old  # noqa: E402

FUNCTIONS = HERE / "functions.tsv"
RUNS = LOG_DIR / "bench"
SNAPSHOTS = ROOT / "build-snapshots"
MODEL = "minimax/MiniMax-M3"
BANDS = ((10, "<=10"), (20, "11-20"), (30, "21-30"), (40, "31-40"), (60, "41-60"), (10 ** 9, ">60"))


class Arm(NamedTuple):
    name: str
    texts: ModuleType | SimpleNamespace   # SYSTEM, CONVENTIONS, TEMPLATE, REPAIR
    check_registers: bool
    farm_result: bool          # passes are real farm results (logged and accepted like the farm's)
    hand_over: bool = False    # round 0 repairs the best earlier candidate of another model (review 3 B31)


_ASSEMBLY = "\nOriginal assembly (bn6f disassembly):"
assert _ASSEMBLY in farm.TEMPLATE
# the live prompt plus a card per called function from symbols/db (review 3 A28/B22: "an arm in the first
# benchmark run, kept if not worse")
CARDS = SimpleNamespace(SYSTEM=farm.SYSTEM, CONVENTIONS=farm.CONVENTIONS, REPAIR=farm.REPAIR,
                        TEMPLATE=farm.TEMPLATE.replace(_ASSEMBLY, "\n{cards}" + _ASSEMBLY[1:]))

ARMS = {arm.name: arm for arm in (
    Arm("typed", farm, check_registers=True, farm_result=True),
    Arm("old", prompt_old, check_registers=False, farm_result=False),
    Arm("cards", CARDS, check_registers=True, farm_result=True),
    Arm("handover", farm, check_registers=True, farm_result=True, hand_over=True),
)}


def prompt_hash(arm: Arm) -> str:
    import hashlib
    texts = (arm.texts.SYSTEM, arm.texts.CONVENTIONS, arm.texts.TEMPLATE, arm.texts.REPAIR)
    return hashlib.sha1("\0".join(texts).encode()).hexdigest()[:12]


def first(arm: Arm, context: ConversionContext) -> str:
    """wf.tools.models.first_prompt with the arm's texts (the typed arm's is checked to be the farm's own)."""
    contract = "\n".join(f"  {part}: {' '.join(names) or '-'}" for part, names in context.contract.by_part().items())
    cards = callee_cards(context.function) if "{cards}" in arm.texts.TEMPLATE else ""
    return arm.texts.TEMPLATE.format(conventions=arm.texts.CONVENTIONS, name=context.function, addr=context.address,
                                     contract=contract, asm=context.assembly, lifted=context.lifted, cards=cards)


@cache
def _assembly() -> dict[str, NamedTuple]:
    sys.path.insert(0, str(ROOT / "tools/convert"))
    import scorecard
    return scorecard.assembly()


def callee_cards(function: str) -> str:
    """Each function the assembly calls with `bl` that has a symbols/db entry: its summary and interface."""
    import yaml
    cards = []
    for callee in sorted(getattr(_assembly().get(function), "calls", ())):
        path = ROOT / "symbols/db" / f"{callee}.yaml"
        if path.exists():
            entry = yaml.safe_load(path.read_text()) or {}
            interface = entry.get("interface")
            cards.append(f"- {callee}: {entry.get('summary') or '(no summary)'}"
                         + (f"\n  interface: {interface}" if interface else ""))
    return "What the functions it calls do (symbols/db):\n" + "\n".join(cards) + "\n\n" if cards else ""


def repair(arm: Arm, context: ConversionContext, failure: str, code: str) -> str:
    """wf.tools.models.repair_prompt with the arm's texts."""
    return first(arm, context) + "\n\n" + arm.texts.REPAIR.format(failure=failure[:3000], code=code or "")


@cache
def _pool():
    """tools/convert/pool.py and every candidate the farm left, by today's name, read once."""
    import pool
    return pool, pool.read_candidates(pool.renames(), pool.lifted_names(ROOT / "cport"))


def best_earlier(function: str, context: ConversionContext, model: str, difftest: Path) -> dict | None:
    """The best candidate another model wrote before, ranked as pool.py ranks them (review 3 Area 2 step 4:
    passed > differs only on random inputs > fewer differing captured calls > compile error > style), and
    what today's test says of it: the register check's report and the behaviour report, together."""
    pool, found = _pool()
    farm.DIFFTEST = str(difftest)
    mine = model.replace("/", "_")
    candidates = pool.distinct([c for c in found.candidates.get(FunctionName(function), []) if c.model != mine])
    if not candidates:
        return None
    order = list(pool.Verdict)
    tested = [pool.retest(FunctionName(function), c, context, SNAPSHOTS) for c in candidates]
    best = min(tested, key=lambda t: (order.index(t.verdict), t.failing_share, t.register_violations))
    code = best.candidate.code
    with tempfile.TemporaryDirectory() as workdir:
        problems = farm.register_problems(function, code, workdir)
    passed, report = test(ARMS["old"], context, code, difftest)    # behaviour, without the register check
    failure = "\n".join(part for part in (problems, "" if passed else report) if part)
    return {"code": code, "report": failure or report, "from": f"{best.candidate.model} {best.candidate.source}",
            "verdict": best.verdict.value, "candidates": len(candidates)}


def functions(only: set[str] | None = None) -> list[dict[str, str]]:
    lines = [line for line in FUNCTIONS.read_text().splitlines() if line and not line.startswith("#")]
    header = lines[0].split("\t")
    rows = [dict(zip(header, line.split("\t"))) for line in lines[1:]]
    return [r for r in rows if r["excluded"] == "-" and (not only or r["function"] in only)]


def failure_class(report: str) -> str:
    """The check a failed round stopped at, in review 3 Area 2's classes."""
    if report.startswith("model error"):
        return "model error"
    if "no ```c code block" in report:
        return "no code"
    if report.startswith("guest registers"):
        return "register check"
    if report.startswith("compile errors"):
        return "compile"
    if "machine-translated style" in report:
        return "style"
    if report.startswith("line ") and ("struct offsets" in report or "LDS16" in report):
        return "review rule"
    if "out of domain: nothing was compared" in report:
        return "nothing testable"
    if "return target" in report:
        return "return target"
    if report.startswith("captured calls") and "random inputs" in report:
        return "behaviour, random inputs"
    if report.startswith("captured calls"):
        return "behaviour, captured calls"
    if "must define" in report:
        return "no entry point"
    return "other"


def test(arm: Arm, context: ConversionContext, code: str, difftest: Path) -> tuple[bool, str]:
    """farm.test, as wf.tools.models.test_candidate runs it, with the arm's register check."""
    farm.DIFFTEST = str(difftest)
    contract = {part: list(names) for part, names in context.contract.by_part().items()}
    with tempfile.TemporaryDirectory() as workdir:
        try:
            return farm.test(context.function, code, str(SNAPSHOTS), contract, workdir,
                             check_registers=arm.check_registers)
        except subprocess.TimeoutExpired:
            return False, "difftest timed out (900 s)"


def usage_of(tag: str) -> dict:
    """The usage row llm.complete just wrote for TAG (the end of the shared log)."""
    with open(USAGE_LOG, "rb") as log:
        log.seek(0, 2)
        log.seek(max(0, log.tell() - 1_000_000))
        tail = log.read().decode(errors="replace").splitlines()
    for line in reversed(tail):
        if tag in line:
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if row.get("tag") == tag:
                return row
    return {}


def farm_log(arm: Arm, row: dict, model: str, difftest: Path) -> None:
    """A typed round as a farm round: tools/convert/log/FUNC.MODEL.jsonl (wf.tools.models.log_round's
    fields) and, when it passed, tools/convert/accepted/FUNC.MODEL.c unless one is there."""
    tag = model.replace("/", "_")
    entry = {"round": row["round"], "ok": row["outcome"] == "passed", "seconds": row.get("seconds", 0),
             "report": row["report"], "code": row.get("code"), "time": row["time"], "model": model,
             "kind": f"bench {arm.name}", "bench_run": row["run"], "outcome": row["outcome"],
             "prompt_hash": row["prompt_hash"], "environment_hash": environment_hash(difftest),
             "max_tokens": MAX_TOKENS, "input_key": farm.input_key(row["function"])}
    with open(LOG_DIR / f"{row['function']}.{tag}.jsonl", "a") as log:
        log.write(json.dumps(entry) + "\n")
    accepted = ACCEPTED_DIR / f"{row['function']}.{tag}.c"
    if row["outcome"] == "passed" and not accepted.exists():
        ACCEPTED_DIR.mkdir(parents=True, exist_ok=True)
        accepted.write_text(row["code"].strip() + "\n")


def read_rows(path: Path) -> list[dict]:
    if not path.exists():
        return []
    rows = []
    for line in path.read_text().splitlines():
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return rows


def run(name: str, arms: list[Arm], model: str, jobs: int, only: set[str] | None, difftest: Path | None) -> None:
    out = RUNS / name
    out.mkdir(parents=True, exist_ok=True)
    results = out / "results.jsonl"
    if difftest is None:
        built = build_host_difftest(out, build_dir="build-wf-bench-host", tree=committed_tree("bench"))
        if isinstance(built, BuildFailure):
            sys.exit(f"the difftest did not build: {built.log}\n{built.last_lines}")
        difftest = built
    chosen = functions(only)
    if any(arm.hand_over for arm in arms):
        _pool()                          # read once, before the threads want it
    contexts = {r["function"]: conversion_context(FunctionName(r["function"])) for r in chosen}
    sample = next(c for c in contexts.values() if c is not None)
    assert first(ARMS["typed"], sample) == first_prompt(sample), "the typed arm's prompt is not the farm's"
    assert repair(ARMS["typed"], sample, "x", "y") == repair_prompt(sample, "x", "y")
    done: dict[tuple[str, str], list[dict]] = defaultdict(list)
    for row in read_rows(results):
        done[(row["function"], row["arm"])].append(row)
    pairs = []
    for r in chosen:                     # function by function, so a run cut short leaves both arms alike
        order = list(arms)
        random.Random(r["function"]).shuffle(order)
        pairs += [(r["function"], arm) for arm in order]
    lock = threading.Lock()

    def one(function: str, arm: Arm) -> None:
        context = contexts[function]
        rows = [row for row in done[(function, arm.name)] if row["outcome"] != "model error"]
        if context is None:
            print(f"{function}: no lifted code or assembly", flush=True)
            return
        while True:
            last = rows[-1] if rows else None
            if last and (last["outcome"] in ("passed", "runaway") or last["round"] >= 1):
                return
            number = 0 if last is None else 1
            prompt = first(arm, context) if last is None else repair(arm, context, last["report"], last["code"])
            handed = best_earlier(function, context, model, difftest) if arm.hand_over and last is None else None
            if handed is not None:
                prompt = repair(arm, context, handed["report"], handed["code"])
            tag = f"bench-{arm.name} {function} r{number} {name}"
            row = {"run": name, "function": function, "arm": arm.name, "round": number, "model": model,
                   "prompt_hash": prompt_hash(arm), "max_tokens": MAX_TOKENS,
                   "environment_hash": environment_hash(difftest)}
            if handed is not None:
                row["handed"] = {k: handed[k] for k in ("from", "verdict", "candidates")}
            started = time.monotonic()
            try:
                reply = farm.llm.complete(model, prompt, system=arm.texts.SYSTEM, max_tokens=MAX_TOKENS, tag=tag,
                                          usage_log=str(USAGE_LOG))
            except Exception as error:   # the provider's own errors: HTTP, timeouts
                row |= {"outcome": "model error", "report": f"model error: {error}"[:500], "code": None}
            else:
                code = farm.extract_code(reply)
                if code is None:
                    row |= {"outcome": "runaway", "report": "the reply had no ```c code block", "code": None}
                else:
                    passed, report = test(arm, context, code, difftest)
                    row |= {"outcome": "passed" if passed else "failed", "report": report[:3000], "code": code}
                usage = usage_of(tag)
                row |= {"in": usage.get("in"), "out": usage.get("out"), "cache_read": usage.get("cache_read"),
                        "answered_by": usage.get("answered_by")}
            row |= {"seconds": round(time.monotonic() - started), "time": datetime.now().isoformat(timespec="seconds"),
                    "class": "-" if row["outcome"] == "passed" else failure_class(row["report"])}
            with lock:
                with open(results, "a") as log:
                    log.write(json.dumps(row) + "\n")
                if arm.farm_result and row["outcome"] != "model error":
                    farm_log(arm, row, model, difftest)
                print(f"{function} {arm.name} r{number}: {row['outcome']} {row['class']} "
                      f"({row.get('in')}+{row.get('out')} tokens)", flush=True)
            if row["outcome"] == "model error":
                return                   # a rerun asks again
            rows.append(row)

    with ThreadPoolExecutor(jobs) as pool:
        for future in [pool.submit(one, function, arm) for function, arm in pairs]:
            try:
                future.result()
            except Exception:            # one pair's bug must not stop the others; a rerun retries it
                traceback.print_exc()


def retest(name: str, jobs: int, difftest: Path | None) -> None:
    out = RUNS / name
    rows = [r for r in read_rows(out / "results.jsonl") if r.get("code")]
    if difftest is None:
        built = build_host_difftest(out, build_dir="build-wf-bench-host", tree=committed_tree("bench"))
        if isinstance(built, BuildFailure):
            sys.exit(f"the difftest did not build: {built.log}")
        difftest = built
    contexts = {f: conversion_context(FunctionName(f)) for f in {r["function"] for r in rows}}
    (out / "retest.jsonl").unlink(missing_ok=True)
    lock = threading.Lock()

    def one(row: dict) -> None:
        context = contexts[row["function"]]
        if context is None:
            return
        passed, report = test(ARMS[row["arm"]], context, row["code"], difftest)
        again = {k: row[k] for k in ("function", "arm", "round")} | {
            "outcome": "passed" if passed else "failed", "report": report[:3000],
            "class": "-" if passed else failure_class(report), "environment_hash": environment_hash(difftest)}
        with lock:
            with open(out / "retest.jsonl", "a") as log:
                log.write(json.dumps(again) + "\n")

    with ThreadPoolExecutor(jobs) as pool:
        list(pool.map(one, rows))


def band(lines: int) -> str:
    return next(label for top, label in BANDS if lines <= top)


class Pair(NamedTuple):
    run: str
    function: str
    arm: str
    band: str
    chosen_as: str
    rounds: list[dict]          # in order; model errors left out

    @property
    def won(self) -> int | None:
        return next((r["round"] for r in self.rounds if r["outcome"] == "passed"), None)

    @property
    def tokens(self) -> int:
        """Input (cache reads included: a repeated prompt is read from MiniMax's cache) plus output, over the
        rounds asked (up to the first pass: a retest may pass an earlier round)."""
        last = self.won if self.won is not None else len(self.rounds)
        return sum(_tokens(r) for r in self.rounds if r["round"] <= last)

    @property
    def runaways(self) -> int:
        return sum(r["outcome"] == "runaway" for r in self.rounds)

    @property
    def capped(self) -> int:
        return sum((r.get("out") or 0) >= 0.98 * MAX_TOKENS for r in self.rounds)


def _tokens(row: dict) -> int:
    return (row.get("in") or 0) + (row.get("cache_read") or 0) + (row.get("out") or 0)


def pairs_of(name: str) -> list[Pair]:
    out = RUNS / name
    rows = [r for r in read_rows(out / "results.jsonl") if r["outcome"] != "model error"]
    again = {(r["function"], r["arm"], r["round"]): r for r in read_rows(out / "retest.jsonl")}
    info = {r["function"]: r for r in functions()}
    grouped: dict[tuple[str, str], list[dict]] = defaultdict(list)
    for r in rows:
        key = (r["function"], r["arm"], r["round"])
        grouped[(r["function"], r["arm"])].append(r | ({k: again[key][k] for k in ("outcome", "report", "class")}
                                                       if key in again else {}))
    return [Pair(name, function, arm, band(int(info[function]["asm_lines"])), info[function]["chosen_as"],
                 sorted(rs, key=lambda r: r["round"]))
            for (function, arm), rs in grouped.items() if function in info]


def summary(names: list[str], tsv: Path | None) -> str:
    """Several runs (replicates of the same functions) pool their attempts: one attempt is one function in
    one arm in one run."""
    pairs = [p for name in names for p in pairs_of(name)]
    if not pairs:
        return f"Runs {', '.join(names)}: no results in {RUNS}"
    arms = sorted({p.arm for p in pairs}, key=lambda a: list(ARMS).index(a) if a in ARMS else 99)
    both = {p.function for p in pairs if p.arm == arms[0]}
    for arm in arms[1:]:
        both &= {p.function for p in pairs if p.arm == arm}
    pairs = [p for p in pairs if p.function in both]          # only functions every arm finished
    runs = {arm: len({p.run for p in pairs if p.arm == arm}) for arm in arms}
    lines = [f"Runs {', '.join(names)}: {len(both)} functions finished in every arm "
             f"({', '.join(f'{arm}: {n} run(s)' for arm, n in runs.items())}). An attempt is one "
             f"function in one arm in one run. Tokens: input (cache reads included) plus output, over the rounds asked.",
             "",
             "| arm | band | attempts | first round passed | passed after the repair | tokens | "
             "tokens per accepted function | runaways (rounds) | rounds at the cap |",
             "|---|---|---|---|---|---|---|---|---|"]
    order = [label for _, label in BANDS] + ["all <=60", "all"]
    for arm in arms:
        cells: dict[str, list[Pair]] = defaultdict(list)
        for p in pairs:
            if p.arm != arm:
                continue
            cells[p.band].append(p)
            cells["all"].append(p)
            if p.band != ">60":
                cells["all <=60"].append(p)
        for label in sorted(cells, key=order.index):
            ps = cells[label]
            n, first_won = len(ps), sum(p.won == 0 for p in ps)
            won, tokens = sum(p.won is not None for p in ps), sum(p.tokens for p in ps)
            lines.append(f"| {arm} | {label} | {n} | {first_won} ({first_won / n:.0%}) | {won} ({won / n:.0%}) | "
                         f"{tokens / 1000:,.0f}k | {f'{tokens / won / 1000:,.0f}k' if won else '-'} | "
                         f"{sum(p.runaways for p in ps)} | {sum(p.capped for p in ps)} |")
    lines += ["", "Why the last round of each unaccepted pair failed:", "",
              "| arm | " + " | ".join(classes := sorted({p.rounds[-1]['class'] for p in pairs if p.won is None})) + " |",
              "|---|" + "---|" * len(classes)]
    for arm in arms:
        count = Counter(p.rounds[-1]["class"] for p in pairs if p.arm == arm and p.won is None)
        lines.append(f"| {arm} | " + " | ".join(str(count[c]) for c in classes) + " |")
    lines += ["", "Paired, on the same function in the same run (passed after the repair):", "",
              "| arms | both | only the first | only the second | neither |", "|---|---|---|---|---|"]
    won = {(p.run, p.function, p.arm): p.won is not None for p in pairs}
    for other in arms[1:]:
        keys = [(run, f) for run, f, arm in won if arm == arms[0] and (run, f, other) in won]
        count = Counter((won[(run, f, arms[0])], won[(run, f, other)]) for run, f in keys)
        lines.append(f"| {arms[0]} / {other} | {count[(True, True)]} | {count[(True, False)]} | {count[(False, True)]} "
                     f"| {count[(False, False)]} |")
    lines += ["", "By how the function was chosen (passed after the repair / attempts):", "",
              "| arm | " + " | ".join(kinds := sorted({p.chosen_as for p in pairs})) + " |", "|---|" + "---|" * len(kinds)]
    for arm in arms:
        lines.append(f"| {arm} | " + " | ".join(
            f"{sum(p.won is not None for p in pairs if p.arm == arm and p.chosen_as == k)}/"
            f"{sum(1 for p in pairs if p.arm == arm and p.chosen_as == k)}" for k in kinds) + " |")
    if tsv is not None:
        rows = ["\t".join(("run", "function", "band", "chosen_as", "arm", "round0", "round1", "passed_in_round",
                           "tokens_in", "tokens_out", "runaways", "last_failure"))]
        for p in sorted(pairs, key=lambda p: (p.run, p.function, list(ARMS).index(p.arm) if p.arm in ARMS else 99)):
            outcome = {r["round"]: r["outcome"] for r in p.rounds}
            rows.append("\t".join((p.run, p.function, p.band, p.chosen_as, p.arm, outcome.get(0, "-"), outcome.get(1, "-"),
                                   str(p.won) if p.won is not None else "-",
                                   str(sum((r.get("in") or 0) + (r.get("cache_read") or 0) for r in p.rounds)),
                                   str(sum(r.get("out") or 0 for r in p.rounds)), str(p.runaways),
                                   "-" if p.won is not None else p.rounds[-1]["class"])))
        tsv.write_text(f"# tools/convert/bench/bench.py summary {' '.join(names)} (one row per run, function and arm; "
                       f"tokens over every round asked, input with cache reads)\n" + "\n".join(rows) + "\n")
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=("run", "summary", "retest"))
    ap.add_argument("run_name", metavar="RUN", nargs="+", help="summary: several runs pool their attempts")
    ap.add_argument("--arms", default="typed,old")
    ap.add_argument("--model", default=MODEL)
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--only", help="comma-separated functions (default: every one in functions.tsv)")
    ap.add_argument("--difftest", type=Path, help="a built host difftest (default: build one from the last commit)")
    ap.add_argument("--tsv", type=Path, help="summary: also write one row per function and arm here")
    args = ap.parse_args()
    if args.difftest is not None:
        args.difftest = args.difftest.resolve()   # farm.test runs it from cport/
    if args.mode == "summary":
        print(summary(args.run_name, args.tsv))
        return
    if len(args.run_name) != 1:
        sys.exit("run and retest take one RUN")
    if args.mode == "retest":
        retest(args.run_name[0], args.jobs, args.difftest)
    else:
        run(args.run_name[0], [ARMS[a] for a in args.arms.split(",")], args.model, args.jobs,
            set(args.only.split(",")) if args.only else None, args.difftest)


if __name__ == "__main__":
    main()
