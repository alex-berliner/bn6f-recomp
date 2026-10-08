#!/usr/bin/env python3
"""Compare models on readable conversion: tools/convert/scorecard.py [FUNC...]

From the farm's logs (tools/convert/log/FUNC.MODEL.jsonl: one line per
round) and llm.py's usage log (tools/convert/usage.jsonl: tokens per call),
per model over the given functions (default: every function it attempted):
  converted  functions with an accepted version / functions it finished
  requests   model calls made (first draft + repairs)
  median s   median seconds per call
  tokens     input + output tokens per converted function (output includes
             reasoning where the provider reports it); only calls logged
             with usage count, so older runs show "-"
The cost per converted function follows from the provider's price or quota:
compare tools/convert/quota.py snapshots taken before and after a run.
"""
import collections, glob, json, os, statistics, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
want = set(sys.argv[1:])

rounds = collections.defaultdict(list)          # (model, func) -> [round records]
for p in glob.glob(os.path.join(ROOT, "tools/convert/log/*.jsonl")):
    func, tag = os.path.basename(p)[:-6].split(".", 1)
    if want and func not in want:
        continue
    model = tag.replace("_", "/", 1)
    rounds[(model, func)] = [json.loads(l) for l in open(p) if l.strip()]

usage = collections.defaultdict(lambda: [0, 0, 0])   # (model, func) -> [calls, in, out]
up = os.path.join(ROOT, "tools/convert/usage.jsonl")
if os.path.exists(up):
    for l in open(up):
        r = json.loads(l)
        func = r.get("tag", "").split(" round ")[0]
        if not func or (want and func not in want):
            continue
        u = usage[(r["model"], func)]
        u[0] += 1
        u[1] += r.get("in") or 0
        u[2] += r.get("out") or 0

rows = []
for model in sorted({m for m, _ in rounds}):
    funcs = [f for m, f in rounds if m == model]
    passed = [f for f in funcs if any(r.get("ok") for r in rounds[(model, f)])]
    finished = [f for f in funcs if f in passed or len(rounds[(model, f)]) >= 4]
    secs = [r.get("seconds", 0) for f in funcs for r in rounds[(model, f)]]
    calls = sum(len(rounds[(model, f)]) for f in funcs)
    tok_in = sum(usage[(model, f)][1] for f in funcs)
    tok_out = sum(usage[(model, f)][2] for f in funcs)
    logged = sum(usage[(model, f)][0] for f in funcs)
    per = f"{(tok_in + tok_out) / len(passed) / 1000:.0f}k" if passed and logged else "-"
    rows.append((model, len(passed), len(finished), calls, statistics.median(secs) if secs else 0, per,
                 f"{tok_in / 1000:.0f}k/{tok_out / 1000:.0f}k" if logged else "-"))
print(f"{'model':36s} {'converted':>10s} {'requests':>9s} {'median s':>9s} {'tokens/conv':>12s} {'in/out tokens':>14s}")
for model, ok, fin, calls, med, per, io in sorted(rows, key=lambda r: (-r[1], r[3])):
    print(f"{model:36s} {f'{ok}/{fin}':>10s} {calls:9d} {med:9.0f} {per:>12s} {io:>14s}")
