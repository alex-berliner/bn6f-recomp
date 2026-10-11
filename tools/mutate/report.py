"""Tables from a mutation survey (tools/mutate/mutate.py report): what each check kills.

Verdict of one check on one mutant:
  killed     it reported a difference the unmutated code does not have (the baseline)
  survived   it compared something and saw nothing new
  nothing    it ran but compared nothing (every input out of domain)
  not run    it does not apply (no captured calls; no route runs the code; not in the lockstep subset)
Kill rates are given over every tested mutant ("of all") and over those the check compared
("when it compared").
"""
from __future__ import annotations

import collections
import json
import os

ENTRY_CHECKS = ("random", "captured", "oracle")
CHECKS = ("random", "captured", "oracle", "leftovers", "warnings", "lockstep")
CHEAP = ("random", "captured", "oracle", "leftovers")       # the cheap checks verify runs (warnings: extra)
CLASS_ORDER = ("flag_leftover", "flags", "registers", "return_target", "sign_width", "offset", "store",
               "boundary", "classic")


def load(out: str):
    def jl(name):
        path = os.path.join(out, name)
        return [json.loads(line) for line in open(path)] if os.path.exists(path) else []
    plan = json.load(open(os.path.join(out, "plan.json")))
    baseline = json.load(open(os.path.join(out, "baseline.json")))
    results = jl("results.jsonl")
    lockstep = {r["id"]: r for r in jl("lockstep.jsonl")}
    return plan, baseline, results, lockstep


def entry_verdict(check: str, r: dict, baseline: dict) -> str:
    entries = r["checks"].get(check, {}).get("entries", {})
    if not entries:
        return "not run"
    seen = set()
    for e, m in entries.items():
        b = baseline.get(e, {}).get(check, {})
        if m.get("outcome") == "failed" and m.get("failures", 0) > b.get("failures", 0):
            return "killed"
        seen.add(m.get("outcome"))
    if "passed" in seen or "failed" in seen:
        return "survived"
    if "nothing" in seen:
        return "nothing"
    return "not run"


def verdicts(r: dict, baseline: dict, lockstep: dict) -> dict[str, str]:
    v = {c: entry_verdict(c, r, baseline) for c in ENTRY_CHECKS}
    left = r["checks"].get("leftovers", {})
    base_left = baseline.get("_files", {}).get(r["file"], {}).get("leftovers", {}).get("count", 0)
    v["leftovers"] = "killed" if left.get("count", 0) > base_left else "survived" if left else "not run"
    warn = r["checks"].get("warnings")
    v["warnings"] = "not run" if warn is None else "killed" if warn["outcome"] == "failed" else "survived"
    ls = lockstep.get(r["id"])
    if ls is None:
        v["lockstep"] = "not run"
    elif ls["status"] == "failed":
        v["lockstep"] = "killed"
    elif ls["status"] == "passed":
        v["lockstep"] = "survived"
    elif ls["status"] == "no route":
        v["lockstep"] = "no route"
    else:
        v["lockstep"] = "error"
    return v


def pct(a: int, b: int) -> str:
    return f"{100 * a / b:.0f}%" if b else "-"


def table(header: list[str], rows: list[list]) -> str:
    out = ["| " + " | ".join(header) + " |", "|" + "|".join("---" for _ in header) + "|"]
    out += ["| " + " | ".join(str(x) for x in row) + " |" for row in rows]
    return "\n".join(out)


def main(out: str) -> None:
    plan, baseline, results, lockstep = load(out)
    tested = [r for r in results if r["status"] == "tested"]
    for r in tested:
        r["v"] = verdicts(r, baseline, lockstep)
        r["any_cheap"] = any(r["v"][c] == "killed" for c in CHEAP)
    functions = {f["name"]: f for f in plan["functions"]}
    print(f"# Mutation survey ({len(plan['functions'])} functions, plan seed {plan['seed']}, tree {plan['tree']})\n")

    # ── mutants
    status = collections.Counter((r["cls"], r["status"]) for r in results)
    rows = []
    for cls in CLASS_ORDER:
        rows.append([cls, status[(cls, "tested")], status[(cls, "equivalent")], status[(cls, "invalid")]])
    rows.append(["all", *(sum(status[(c, s)] for c in CLASS_ORDER) for s in ("tested", "equivalent", "invalid"))])
    print("## Mutants\n")
    print(table(["class", "tested", "same object code (dropped)", "do not compile (dropped)"], rows), "\n")

    # ── per check
    print("## Kill rate per check\n")
    rows = []
    for c in CHECKS:
        vs = collections.Counter(r["v"][c] for r in tested)
        ran = vs["killed"] + vs["survived"]
        population = len(tested) if c != "lockstep" else sum(1 for r in tested if r["v"][c] not in ("not run",))
        rows.append([c, vs["killed"], f"{pct(vs['killed'], population)} of {population}", pct(vs["killed"], ran),
                     vs["nothing"], vs["not run"] + vs["no route"]])
    print(table(["check", "killed", "of all", "when it compared", "compared nothing", "not run"], rows), "\n")
    cheap_kill = sum(r["any_cheap"] for r in tested)
    print(f"Any cheap check (random, captured, oracle, leftovers): {cheap_kill} of {len(tested)} ({pct(cheap_kill, len(tested))}).\n")

    # ── class x check
    print("## Kill rate per class and check (of all tested mutants of the class)\n")
    rows = []
    for cls in CLASS_ORDER:
        rs = [r for r in tested if r["cls"] == cls]
        if not rs:
            continue
        row = [cls, len(rs)]
        for c in CHECKS[:-1]:
            row.append(pct(sum(r["v"][c] == "killed" for r in rs), len(rs)))
        row.append(pct(sum(r["any_cheap"] for r in rs), len(rs)))
        ls = [r for r in rs if r["v"]["lockstep"] in ("killed", "survived")]
        row.append(f"{pct(sum(r['v']['lockstep'] == 'killed' for r in ls), len(ls))} of {len(ls)}")
        rows.append(row)
    print(table(["class", "n", *CHECKS[:-1], "any cheap", "lockstep"], rows), "\n")

    # ── per op
    print("## Per operator\n")
    rows = []
    ops = collections.defaultdict(list)
    for r in tested:
        ops[(r["cls"], r["op"])].append(r)
    for (cls, op), rs in sorted(ops.items(), key=lambda kv: (CLASS_ORDER.index(kv[0][0]), kv[0][1])):
        rows.append([cls, op, len(rs), *(pct(sum(r["v"][c] == "killed" for r in rs), len(rs)) for c in ("random", "captured", "oracle")),
                     pct(sum(r["any_cheap"] for r in rs), len(rs))])
    print(table(["class", "operator", "n", "random", "captured", "oracle", "any cheap"], rows), "\n")

    # ── strata
    print("## Per stratum (function size in halfwords: S <= 20, M <= 60, L <= 200, XL above)\n")
    rows = []
    strata = collections.defaultdict(list)
    for r in tested:
        strata[functions[r["entry"]]["stratum"]].append(r)
    for s, rs in sorted(strata.items()):
        rows.append([s, len(rs), *(pct(sum(r["v"][c] == "killed" for r in rs), len(rs)) for c in ("random", "captured", "oracle")),
                     pct(sum(r["any_cheap"] for r in rs), len(rs)),
                     pct(sum(r["v"]["random"] == "nothing" for r in rs), len(rs))])
    print(table(["stratum", "n", "random", "captured", "oracle", "any cheap", "random compared nothing"], rows), "\n")

    # ── combinations
    print("## Which checks kill (cheap checks)\n")
    combos = collections.Counter(
        "+".join(c for c in CHEAP if r["v"][c] == "killed") or "none" for r in tested)
    rows = [[k, n, pct(n, len(tested))] for k, n in combos.most_common()]
    print(table(["killed by", "mutants", "share"], rows), "\n")
    only = {c: sum(1 for r in tested if r["v"][c] == "killed" and not any(r["v"][d] == "killed" for d in CHEAP if d != c))
            for c in CHEAP}
    print("Killed by one cheap check only: " + ", ".join(f"{c} {n}" for c, n in only.items()) + "\n")

    ls = [r for r in tested if r["v"]["lockstep"] in ("killed", "survived")]
    if ls:
        print(f"## Lockstep subset ({len(ls)} mutants replayed)\n")
        rows = []
        for cheap in (True, False):
            sub = [r for r in ls if r["any_cheap"] == cheap]
            k = sum(r["v"]["lockstep"] == "killed" for r in sub)
            rows.append(["killed by a cheap check" if cheap else "survived the cheap checks", len(sub), k, pct(k, len(sub))])
        print(table(["", "replayed", "lockstep killed", "rate"], rows), "\n")
        only_ls = [r for r in ls if r["v"]["lockstep"] == "killed" and not r["any_cheap"]]
        print(f"Only lockstep: {len(only_ls)}: " + ", ".join(f"{r['id']} ({r['cls']}/{r['op']} {r['entry']})" for r in only_ls[:40]) + "\n")
        none = [r for r in ls if r["v"]["lockstep"] == "survived" and not r["any_cheap"]]
        print(f"Nothing (lockstep replayed): {len(none)}\n")
    survivors = [r for r in tested if not r["any_cheap"] and r["v"]["lockstep"] != "killed"]
    print(f"Survivors of every check run: {len(survivors)} of {len(tested)}\n")
    extras(out, tested, baseline)


def extras(out: str, tested: list[dict], baseline: dict) -> None:
    """The optional measurements: an earlier run of the same mutants (a check changed since), the
    leftover lint with --deep, reach probes, rechecks."""
    def jl(name):
        path = os.path.join(out, name)
        return [json.loads(line) for line in open(path)] if os.path.exists(path) else []
    by_id = {r["id"]: r for r in tested}
    earlier = {r["id"]: r for r in jl("run1-results.jsonl") if r["status"] == "tested"}
    if earlier and os.path.exists(os.path.join(out, "run1-baseline.json")):
        b1 = json.load(open(os.path.join(out, "run1-baseline.json")))
        print("## Earlier run of the same mutants (run1: before a check changed)\n")
        rows = []
        for cls in CLASS_ORDER + ("all",):
            rs = [r for r in tested if r["id"] in earlier and (cls == "all" or r["cls"] == cls)]
            if not rs:
                continue
            k1 = sum(entry_verdict("oracle", earlier[r["id"]], b1) == "killed" for r in rs)
            k2 = sum(r["v"]["oracle"] == "killed" for r in rs)
            a1 = sum(any(entry_verdict(c, earlier[r["id"]], b1) == "killed" for c in ENTRY_CHECKS) or
                     r["v"]["leftovers"] == "killed" for r in rs)
            a2 = sum(r["any_cheap"] for r in rs)
            rows.append([cls, len(rs), pct(k1, len(rs)), pct(k2, len(rs)), pct(a1, len(rs)), pct(a2, len(rs))])
        print(table(["class", "n", "oracle run1", "oracle now", "any cheap run1", "any cheap now"], rows), "\n")
    deep = {r["id"]: r for r in jl("leftovers-deep.jsonl")}
    if deep:
        print("## Leftover lint: default vs --deep\n")
        rows = []
        for cls in CLASS_ORDER:
            rs = [r for r in tested if r["cls"] == cls and r["id"] in deep]
            if not rs:
                continue
            d0 = sum(deep[r["id"]]["default"] == "fail" for r in rs)
            d1 = sum(deep[r["id"]]["deep"] == "fail" for r in rs)
            l1 = sum(deep[r["id"]]["deep"] == "listed" for r in rs)
            rows.append([cls, len(rs), pct(d0, len(rs)), pct(d1, len(rs)), l1])
        print(table(["class", "n", "default fails", "--deep fails", "--deep lists (zflag)"], rows), "\n")
    probes = jl("probes.jsonl")
    if probes:
        reached = {}
        for p in probes:
            if p.get("status") == "tested":
                for i in p["ids"]:
                    reached[i] = p["reached"]
        surv = [r for r in tested if not r["any_cheap"] and r["id"] in reached]
        print(f"## Reach probes ({len(surv)} cheap-check survivors probed)\n")
        rows = []
        for cls in CLASS_ORDER + ("all",):
            rs = [r for r in surv if cls == "all" or r["cls"] == cls]
            if not rs:
                continue
            row = [cls, len(rs)]
            for c in ENTRY_CHECKS:
                row.append(pct(sum(reached[r["id"]][c] for r in rs), len(rs)))
            row.append(pct(sum(not any(reached[r["id"]].values()) for r in rs), len(rs)))
            rows.append(row)
        print(table(["class", "survivors", "random reached", "captured reached", "oracle reached", "no check reached"], rows), "\n")
    for path in sorted(os.listdir(out)):
        if path.startswith("recheck-") and path.endswith(".jsonl"):
            name = path[len("recheck-"):-len(".jsonl")]
            rb = os.path.join(out, f"recheck-{name}.baseline.json")
            b = json.load(open(rb)) if os.path.exists(rb) else {}
            rs = [r for r in jl(path) if r["status"] == "tested"]
            print(f"## Recheck {name} ({len(rs)} mutants)\n")
            rows = []
            for c in ENTRY_CHECKS:
                ran = [r for r in rs if c in r["checks"]]
                if not ran:
                    continue
                now_k = sum(entry_verdict(c, r, b) == "killed" for r in ran)
                before = sum(by_id[r["id"]]["v"][c] == "killed" for r in ran if r["id"] in by_id)
                rows.append([c, len(ran), before, now_k])
            print(table(["check", "mutants", "killed in the survey", "killed in the recheck"], rows), "\n")


def export(out: str, path: str, review: str | None = None) -> None:
    """One row per tested mutant: what it changed and what every check said (tools/mutate/survey-*.tsv)."""
    plan, baseline, results, lockstep = load(out)

    def jl(name):
        p = os.path.join(out, name)
        return [json.loads(line) for line in open(p)] if os.path.exists(p) else []
    cands = {c["id"]: c for c in plan["candidates"]}
    functions = {f["name"]: f for f in plan["functions"]}
    earlier = {r["id"]: r for r in jl("run1-results.jsonl") if r["status"] == "tested"}
    b1 = json.load(open(os.path.join(out, "run1-baseline.json"))) if earlier else {}
    deep = {r["id"]: r for r in jl("leftovers-deep.jsonl")}
    reached = {i: p["reached"] for p in jl("probes.jsonl") if p.get("status") == "tested" for i in p["ids"]}
    ls_probe = {r["id"]: r for r in jl("lockstep-probe.jsonl")}
    rechecks = {}
    for name in sorted(os.listdir(out)):
        if name.startswith("recheck-") and name.endswith(".jsonl"):
            tag = name[len("recheck-"):-len(".jsonl")]
            rb = os.path.join(out, f"recheck-{tag}.baseline.json")
            b = json.load(open(rb)) if os.path.exists(rb) else {}
            rechecks[tag] = {r["id"]: {c: entry_verdict(c, r, b) for c in r.get("checks", {}) if c in ENTRY_CHECKS}
                             for r in jl(name) if r["status"] == "tested"}
    verdict_of = {}
    if review and os.path.exists(review):
        for line in open(review):
            f = line.rstrip("\n").split("\t")
            if len(f) >= 2 and f[0].startswith("m"):
                verdict_of[f[0]] = f[1]
    cols = ["id", "entry", "stratum", "file", "line", "class", "operator", "change", "entries", *CHECKS,
            "oracle_run1", "leftovers_deep", "reached_random", "reached_captured", "reached_oracle",
            "lockstep_probe", *(f"recheck_{t}" for t in rechecks), "review"]
    with open(path, "w") as fh:
        fh.write("\t".join(cols) + "\n")
        for r in results:
            if r["status"] != "tested":
                continue
            c = cands[r["id"]]
            v = verdicts(r, baseline, lockstep)
            rp = reached.get(r["id"], {})
            lp = ls_probe.get(r["id"])
            row = [r["id"], r["entry"], functions[r["entry"]]["stratum"], c["file"], c["line"], c["cls"], c["op"],
                   c["note"].replace("\t", " ").replace("\n", " "), len(c["touched"]), *(v[k] for k in CHECKS),
                   entry_verdict("oracle", earlier[r["id"]], b1) if r["id"] in earlier else "",
                   deep.get(r["id"], {}).get("deep", ""),
                   *(("yes" if rp[k] else "no") if k in rp else "" for k in ENTRY_CHECKS),
                   ("ran" if lp["status"] == "failed" else "not run" if lp["status"] == "passed" else lp["status"]) if lp else "",
                   *(",".join(f"{k}:{x}" for k, x in rechecks[t].get(r["id"], {}).items()) for t in rechecks),
                   verdict_of.get(r["id"], "")]
            fh.write("\t".join(str(x) for x in row) + "\n")
