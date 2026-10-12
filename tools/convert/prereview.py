#!/usr/bin/env python3
"""An outside model's first read of farm candidates against their assembly, so Claude reviews only
what it flags (plus a sample).

  prereview.py bench [--model M]... [--per-class N] [--jobs J]   score against Claude's past verdicts
  prereview.py run FILE... [--model M]... [--jobs J]             pre-review candidate files

Each candidate gets one call per model: the function's bn6f assembly, its own and its callees' contracts
(the lifted headers), the review checklist (wf convert's REVIEW_CHECKLIST) and the C. The model answers
JSON: {"verdict": "pass" | "flag", "problems": [{"where": ..., "why": ...}]}. A candidate is flagged when
any model flags it (or its answer can't be read).

bench reads Claude's install-round ledgers (build-wf-runs/*/review.tsv: ok / reject) and reports, per
model and for all models together, how many of Claude's rejects were flagged and how many of its oks.
Results: build-prereview/*.jsonl (resumable)."""
import argparse
import concurrent.futures
import csv
import glob
import json
import os
import random
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "tools", "convert"))
import ast  # noqa: E402
import farm  # noqa: E402  find_asm, find_lifted, contract_of, callees
import llm  # noqa: E402


def _checklist():
    """wf convert's REVIEW_CHECKLIST, read from its source (wf needs its own venv)."""
    tree = ast.parse(open(os.path.join(ROOT, "tools", "wf", "wf", "graphs", "convert.py")).read())
    for node in tree.body:
        if isinstance(node, ast.Assign) and getattr(node.targets[0], "id", "") == "REVIEW_CHECKLIST":
            return ast.literal_eval(node.value)
    raise SystemExit("REVIEW_CHECKLIST not found in tools/wf/wf/graphs/convert.py")


REVIEW_CHECKLIST = _checklist()

OUT = os.path.join(ROOT, "build-prereview")
CANDIDATES = os.path.join(ROOT, "tools", "convert")
MODELS = ("hyper/glm-5.3-flash", "minimax/MiniMax-M3")

SYSTEM = """You review C translations of THUMB functions from the GBA game Mega Man Battle Network 6.
The C must do exactly what the assembly does on EVERY path, including paths that tests may never run.
You are the first reader: flag a candidate when you see a real difference in behaviour; do not flag style.

The C runs on an emulated CPU: R0-R12, SP, LR are the guest registers; GUEST_CALL(f) calls a game
function with the registers as they are (so every register the callee reads must be set first);
ZERO_FLAG()/CARRY_FLAG() read flags; LD8/LD16/LD32/LDS8/LDS16 and ST8/ST16/ST32 access guest memory.
A function's contract (from the original's callers) lists its inputs, outputs (registers callers read
after it), clobbers (callers never read them) and preserved registers. Typed helpers return values;
`Next NAME(void)` adapters move registers in and out."""

PROMPT = """## Function {name}

### Assembly (bn6f, the original)
```
{asm}
```

### Contracts
{contracts}

### Check while reading
{checklist}

### The C candidate
```c
{code}
```

Read the C against the assembly path by path. Answer with JSON only:
{{"verdict": "pass" | "flag", "problems": [{{"where": "C line or construct", "why": "what differs from the assembly, and on which path"}}]}}
Use "flag" only for differences in behaviour that a caller or the game could see (registers or flags in
the outputs or preserved, memory writes, calls and their inputs, return values, loops, conditions).
"""


def contracts(name):
    lines = []
    lifted, _ = farm.find_lifted(name)
    if lifted:
        c = farm.contract_of(lifted)
        lines.append(f"- {name} (this function): inputs {' '.join(c['inputs']) or '-'}; outputs "
                     f"{' '.join(c['outputs']) or '-'}; clobbers {' '.join(c['clobbers']) or '-'}; "
                     f"preserved {' '.join(c['preserved']) or '-'}")
    for callee in farm.callees(name, lifted)[:20]:
        body, _ = farm.find_lifted(callee)
        if body:
            c = farm.contract_of(body)
            lines.append(f"- {callee} (callee): inputs {' '.join(c['inputs']) or '-'}; outputs "
                         f"{' '.join(c['outputs']) or '-'}")
    return "\n".join(lines) or "(none found)"


def prompt_for(name, code):
    asm = farm.find_asm(name)
    if asm is None:
        return None
    return PROMPT.format(name=name, asm=asm, contracts=contracts(name),
                         checklist="\n".join(f"- {item}" for item in REVIEW_CHECKLIST), code=code)


def parse(reply):
    m = re.search(r"\{.*\}", reply or "", re.S)
    if not m:
        return None
    text = m.group(0)
    for candidate in (text, re.sub(r",\s*([}\]])", r"\1", text)):
        try:
            got = json.loads(candidate)
            if isinstance(got, dict) and got.get("verdict") in ("pass", "flag"):
                return got
        except json.JSONDecodeError:
            pass
    v = re.search(r'"verdict"\s*:\s*"(pass|flag)"', text)
    return {"verdict": v.group(1), "problems": [], "lenient": True} if v else None


def review(name, tag, path, model):
    code = open(path).read()
    prompt = prompt_for(name, code)
    if prompt is None:
        return {"name": name, "tag": tag, "model": model, "verdict": "flag", "error": "no assembly found"}
    try:
        reply = llm.complete(model, prompt, system=SYSTEM, max_tokens=16000, timeout=900,
                             tag=f"{name} prereview", stream=True)
    except Exception as e:  # a provider failure: flagged, so Claude reads it
        return {"name": name, "tag": tag, "model": model, "verdict": "flag", "error": str(e)[:300]}
    got = parse(reply)
    if got is None:
        return {"name": name, "tag": tag, "model": model, "verdict": "flag", "error": "unreadable answer",
                "reply": (reply or "")[-500:]}
    return {"name": name, "tag": tag, "model": model, "verdict": got["verdict"],
            "problems": got.get("problems", [])[:10], "lenient": got.get("lenient", False)}


def run_all(items, models, out_path, jobs):
    """items: (name, tag, path, label). Resumable: skips (name, tag, model) already in out_path."""
    done = set()
    if os.path.exists(out_path):
        for line in open(out_path):
            r = json.loads(line)
            done.add((r["name"], r["tag"], r["model"]))
    work = [(n, t, p, lab, m) for n, t, p, lab in items for m in models if (n, t, m) not in done]
    os.makedirs(OUT, exist_ok=True)
    with open(out_path, "a") as out, concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        futures = {pool.submit(review, n, t, p, m): lab for n, t, p, lab, m in work}
        for i, f in enumerate(concurrent.futures.as_completed(futures), 1):
            r = f.result()
            r["label"] = futures[f]
            out.write(json.dumps(r) + "\n")
            out.flush()
            if i % 20 == 0:
                print(f"{i}/{len(work)}", flush=True)


def ledger():
    """(function, tag) -> Claude's verdict (ok / reject) and the candidate file it was about."""
    rows = {}
    for path in sorted(glob.glob(os.path.join(ROOT, "build-wf-runs", "*", "review.tsv"))):
        for r in csv.DictReader(open(path), delimiter="\t"):
            if r.get("verdict") not in ("ok", "reject"):
                continue
            folder = "accepted" if r["verdict"] == "ok" else "rejected"
            f = os.path.join(CANDIDATES, folder, f"{r['function']}.{r['tag']}.c")
            if os.path.exists(f):
                rows[(r["function"], r["tag"])] = (r["verdict"], f, r.get("why", ""))
    return rows


def bench(args):
    rows = ledger()
    rng = random.Random(1)
    rejects = sorted(k for k, v in rows.items() if v[0] == "reject")
    oks = sorted(k for k, v in rows.items() if v[0] == "ok")
    pick = rng.sample(rejects, min(args.per_class, len(rejects))) + rng.sample(oks, min(args.per_class, len(oks)))
    items = [(n, t, rows[(n, t)][1], rows[(n, t)][0]) for n, t in pick]
    out = os.path.join(OUT, "bench.jsonl")
    run_all(items, args.model, out, args.jobs)
    report(out, args.model)


def report(out, models):
    results = [json.loads(line) for line in open(out)]
    by = {}
    for r in results:
        by.setdefault((r["name"], r["tag"]), {})[r["model"]] = r
    def line(label, flagged):
        rej = [k for k, v in by.items() if next(iter(v.values()))["label"] == "reject" and flagged(v) is not None]
        ok = [k for k, v in by.items() if next(iter(v.values()))["label"] == "ok" and flagged(v) is not None]
        fr = sum(flagged(by[k]) for k in rej)
        fo = sum(flagged(by[k]) for k in ok)
        print(f"{label:28} Claude's rejects flagged {fr}/{len(rej)}   Claude's oks flagged {fo}/{len(ok)}")
    for m in models:
        line(m, lambda v, m=m: (v[m]["verdict"] == "flag") if m in v else None)
    line("any model flags", lambda v: any(r["verdict"] == "flag" for r in v.values()) if len(v) == len(models) else None)
    errors = sum(1 for r in results if r.get("error"))
    print(f"answers that were errors or unreadable (counted as flags): {errors} of {len(results)}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("bench")
    b.add_argument("--model", action="append")
    b.add_argument("--per-class", type=int, default=60)
    b.add_argument("--jobs", type=int, default=8)
    r = sub.add_parser("run")
    r.add_argument("files", nargs="+")
    r.add_argument("--model", action="append")
    r.add_argument("--jobs", type=int, default=8)
    r.add_argument("--out", default=os.path.join(OUT, "run.jsonl"))
    s = sub.add_parser("report")
    s.add_argument("--model", action="append")
    s.add_argument("--out", default=os.path.join(OUT, "bench.jsonl"))
    args = p.parse_args()
    args.model = tuple(args.model or MODELS)
    if args.cmd == "bench":
        bench(args)
    elif args.cmd == "report":
        report(args.out, args.model)
    else:
        items = []
        for f in args.files:
            base = os.path.basename(f)[:-2]
            name, tag = base.split(".", 1)
            items.append((name, tag, f, "new"))
        run_all(items, args.model, args.out, args.jobs)


if __name__ == "__main__":
    main()
