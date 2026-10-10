#!/usr/bin/env python3
"""How the output-token cap changes a cheap model's first answer.

  tools/wf/.venv/bin/python tools/convert/experiments/max_tokens.py OUT_DIR --functions FILE
      [--models hyper/glm-5.3-flash,...] [--caps 24000,48000,...] [--jobs N] [--no-stream]
  tools/wf/.venv/bin/python tools/convert/experiments/max_tokens.py OUT_DIR --summary
  tools/wf/.venv/bin/python tools/convert/experiments/max_tokens.py OUT_DIR --retest [--jobs N]

Every (model, cap) pair gets the same functions and the farm's first prompt
(tools/convert/farm.py), one round, no repairs. Each answer goes through the
farm's acceptance test (style and register checks, compile, difftest on the
captured calls and random inputs). One row per call in OUT_DIR/results.jsonl
(the code included); token counts in OUT_DIR/usage.jsonl. Calls stream, so
long answers are not cut off by a read timeout. A rerun skips calls already
in results.jsonl. Nothing is written to tools/convert/accepted or log.
--no-stream asks for the whole reply at once, as the farm does (2026-10-10: the
farm's glm-5.3-flash calls ran away on reasoning about half the time, these
streamed ones rarely; this arm tests whether streaming is the difference).
--retest runs the acceptance test again on every saved answer with the current
tree (no model calls) into OUT_DIR/retest.jsonl; --summary then uses those
outcomes (2026-10-10: after game.h gained stdbool.h).
"""
import argparse
import json
import random
import sys
import threading
import time
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools/wf"))
from wf.tools.models import conversion_context, first_prompt, test_candidate  # noqa: E402
from wf.types import FunctionName, SnapshotDir  # noqa: E402
import farm  # noqa: E402  (tools/convert, on the path through wf.tools.models)

SNAPSHOTS = SnapshotDir(str(ROOT / "build-snapshots"))
DIFFTEST = ROOT / "build-wf-worktrees/farm/cport/build-wf-farm-host/difftest"
MODELS = "hyper/glm-5.3-flash,hyper/deepseek-v4.1-flash,hyper/qwen3.8-flash"
CAPS = "24000,48000,72000,96000,120000"


def run(out: Path, functions: list[str], models: list[str], caps: list[int], jobs: int, stream: bool) -> None:
    results, usage = out / "results.jsonl", out / "usage.jsonl"
    done = set()
    if results.exists():
        done = {(r["function"], r["model"], r["cap"]) for r in map(json.loads, results.read_text().splitlines())
                if r.get("stream", True) == stream}
    contexts = {f: conversion_context(FunctionName(f)) for f in functions}
    calls = []
    for f in functions:              # function by function, so a run cut short leaves every pair balanced
        pairs = [(f, m, c) for m in models for c in caps if (f, m, c) not in done]
        random.Random(f).shuffle(pairs)
        calls += pairs
    lock = threading.Lock()

    def one(call: tuple[str, str, int]) -> None:
        function, model, cap = call
        context = contexts[function]
        row = {"function": function, "model": model, "cap": cap, "stream": stream}
        if context is None:
            row["outcome"] = "no context"
        else:
            started = time.monotonic()
            try:
                reply = farm.llm.complete(model, first_prompt(context), system=farm.SYSTEM, max_tokens=cap,
                                          tag=_tag(function, cap, stream), usage_log=str(usage), stream=stream)
                code = farm.extract_code(reply)
                row["seconds"] = round(time.monotonic() - started)
                if code is None:
                    row["outcome"] = "no code"
                else:
                    test = test_candidate(context, code, SNAPSHOTS, DIFFTEST)
                    row["outcome"] = "passed" if test.passed else "failed"
                    row["report"] = test.report[:1500]
                    row["code"] = code
            except Exception as error:   # provider errors: HTTP (402 out of credit, 429), timeouts
                row["seconds"] = round(time.monotonic() - started)
                row["outcome"] = "error"
                row["report"] = str(error)[:500]
        with lock:
            with open(results, "a") as log:
                log.write(json.dumps(row) + "\n")
            print(f"{function} {model} {cap}: {row['outcome']}", flush=True)

    with ThreadPoolExecutor(jobs) as pool:
        list(pool.map(one, calls))


def retest(out: Path, jobs: int) -> None:
    rows = [json.loads(l) for l in (out / "results.jsonl").read_text().splitlines()]
    rows = [r for r in rows if "code" in r]
    contexts = {f: conversion_context(FunctionName(f)) for f in {r["function"] for r in rows}}
    lock = threading.Lock()

    def one(row: dict) -> None:
        test = test_candidate(contexts[row["function"]], row["code"], SNAPSHOTS, DIFFTEST)
        new = {k: row[k] for k in ("function", "model", "cap")} | {"stream": row.get("stream", True),
               "outcome": "passed" if test.passed else "failed", "report": test.report[:1500]}
        with lock:
            with open(out / "retest.jsonl", "a") as log:
                log.write(json.dumps(new) + "\n")

    (out / "retest.jsonl").unlink(missing_ok=True)
    with ThreadPoolExecutor(jobs) as pool:
        list(pool.map(one, rows))


def _key(r: dict) -> tuple:
    return r["function"], r["model"], r["cap"], r.get("stream", True)


def summary(out: Path) -> str:
    rows = [json.loads(l) for l in (out / "results.jsonl").read_text().splitlines()]
    if (out / "retest.jsonl").exists():
        again = {_key(r): r for r in map(json.loads, (out / "retest.jsonl").read_text().splitlines())}
        rows = [r | {"outcome": again[_key(r)]["outcome"], "report": again[_key(r)]["report"]} if _key(r) in again
                else r for r in rows]
    usage = defaultdict(list)
    if (out / "usage.jsonl").exists():
        for u in map(json.loads, (out / "usage.jsonl").read_text().splitlines()):
            usage[(u["model"], u["tag"])].append(u)
    cells = defaultdict(list)
    for r in rows:
        u = usage.get((r["model"], _tag(r["function"], r["cap"], r.get("stream", True))), [{}])[-1]
        r["out"] = u.get("out") or 0
        cells[(r["model"], r["cap"], r.get("stream", True))].append(r)
    lines = ["| model | cap | calls | hit cap | no code | errors | passed | mean out tokens | mean s | passed per M out |",
             "|---|---|---|---|---|---|---|---|---|---|"]
    for (model, cap, stream), rs in sorted(cells.items()):
        n = len(rs)
        hit = sum(r["out"] >= cap * 0.98 for r in rs)
        outs = sum(r["out"] for r in rs)
        passed = sum(r["outcome"] == "passed" for r in rs)
        secs = [r["seconds"] for r in rs if "seconds" in r]
        lines.append(f"| {model.split('/')[1]}{'' if stream else ' (no stream)'} | {cap // 1000}k | {n} | {hit} | "
                     f"{sum(r['outcome'] == 'no code' for r in rs)} | {sum(r['outcome'] == 'error' for r in rs)} | "
                     f"{passed} | {outs // max(n, 1)} | {sum(secs) // max(len(secs), 1)} | "
                     f"{passed / (outs / 1e6) if outs else 0:.1f} |")
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", type=Path)
    ap.add_argument("--functions")
    ap.add_argument("--models", default=MODELS)
    ap.add_argument("--caps", default=CAPS)
    ap.add_argument("--jobs", type=int, default=24)
    ap.add_argument("--summary", action="store_true")
    ap.add_argument("--no-stream", action="store_true")
    ap.add_argument("--retest", action="store_true")
    args = ap.parse_args()
    if args.summary:
        print(summary(args.out))
        return
    if args.retest:
        retest(args.out, args.jobs)
        return
    args.out.mkdir(parents=True, exist_ok=True)
    functions = [l.strip() for l in open(args.functions) if l.strip()]
    (args.out / "functions.txt").write_text("\n".join(functions) + "\n")
    run(args.out, functions, args.models.split(","), [int(c) for c in args.caps.split(",")], args.jobs,
        not args.no_stream)


def _tag(function: str, cap: int, stream: bool) -> str:
    return f"{function} cap={cap}" + ("" if stream else " no-stream")


if __name__ == "__main__":
    main()
