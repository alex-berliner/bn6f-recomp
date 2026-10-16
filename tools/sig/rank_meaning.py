#!/usr/bin/env python3
"""How sure are we what each symbol means? An outside model scores every function and data label 0-5 from
our records (its name, its symbols/db entry, readable C), for this tree and for an older one, and the two are
compared.

  rank_meaning.py --old-rev REV --old-elf ELF [--model M] [--batch N] [--jobs J]

The symbols are the addresses in symbols/imported_symbols.tsv (functions) and imported_data_symbols.tsv (RAM
labels), the same set for both trees. A tree's name for an address is its bn6f.elf's label there (a named
label before an address-only one); its records are its symbols/db (git archive for the old tree). The model
sees only the records, not the code: the score is what the project knows, not what the code does.

Score = the sum of the scores over 5 x the symbols (100 symbols all at 5: 500/500 = 100%). Writes
symbols/meaning_scores.tsv (per address: both trees' names and scores) and prints the comparison. Scores are
cached by the exact text the model saw (build-rank/cache.jsonl), so a rerun and unchanged symbols cost nothing."""
import argparse
import concurrent.futures
import hashlib
import io
import json
import os
import re
import subprocess
import sys
import tarfile
from collections import Counter

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import llm  # noqa: E402

CACHE = os.path.join(ROOT, "build-rank", "cache.jsonl")
OUT = os.path.join(ROOT, "symbols", "meaning_scores.tsv")
PLACEHOLDER = re.compile(r"^(sub|nullsub|loc|dword|word|byte|unk|off|byte|hword|dword|ptr|jt|thumb|arm|"
                         r"[a-z]+data)_[0-9A-Fa-f]+$|^[A-Za-z]+_?[0-9A-Fa-f]{6,8}$")

SYSTEM = """You rate how well a reverse-engineering project knows what each symbol of a GBA game (Mega Man
Battle Network 6) means, from the project's records only. Score 0-5:
0 = an address-only placeholder name (sub_80123AC, dword_2001C88, unk_...) and no record of its meaning
1 = only a weak hint: a vague or generic name, or a record that says little
2 = a plausible descriptive name or a hypothesis-level description, not checked
3 = a clear name and description read from the assembly (status probable), no tests
4 = clear name and description backed by tests or runtime evidence (difftest, captured calls, emulator,
    the sync), or readable C that is verified
5 = confirmed: purpose fully described (what it does, its inputs/outputs or fields), several kinds of evidence
A descriptive name that keeps an address suffix (battle_dispatchAI_80123AC) is fine: judge the name part.
A descriptive name with no record: 1 if generic (Unk_58, StateFlags), 2 if specific (StatusEffectFinal)."""

PROMPT = """Rate each symbol. Answer with one line per symbol, `INDEX: SCORE`, nothing else.

{items}"""


def tsv_addresses(path: str, name_col: int) -> list[tuple[int, str]]:
    out = []
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        f = line.rstrip("\n").split("\t")
        out.append((int(f[0], 16), f[name_col] if len(f) > name_col else ""))
    return out


def elf_names(elf: str) -> dict[int, list[str]]:
    nm = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True, check=True).stdout
    by: dict[int, list[str]] = {}
    for f in (line.split() for line in nm.splitlines()):
        if len(f) == 3:
            by.setdefault(int(f[0], 16) & (~1 if f[1] in "tT" else ~0), []).append(f[2])
    return by


def pick(names: list[str]) -> str:
    good = [n for n in names if not PLACEHOLDER.match(n) and not n.startswith(("o", "_", "."))]
    return (good or [n for n in names if not n.startswith(("_", "."))] or names)[0]


def db_records(rev: str | None) -> dict[str, dict]:
    """symbols/db at REV (None: the working tree): name -> entry."""
    out = {}
    if rev is None:
        files = ((p, open(os.path.join(ROOT, "symbols", "db", p)).read())
                 for p in os.listdir(os.path.join(ROOT, "symbols", "db")) if p.endswith(".yaml"))
    else:
        data = subprocess.run(["git", "-C", ROOT, "archive", rev, "symbols/db"], capture_output=True, check=True).stdout
        tar = tarfile.open(fileobj=io.BytesIO(data))
        files = ((m.name, tar.extractfile(m).read().decode()) for m in tar.getmembers()
                 if m.isfile() and m.name.endswith(".yaml"))
    for _, text in files:
        try:
            e = yaml.safe_load(text)
        except yaml.YAMLError:
            continue
        if isinstance(e, list):
            e = e[0] if e else None
        if isinstance(e, dict) and e.get("name"):
            out[e["name"]] = e
    return out


def describe(kind: str, name: str, e: dict | None) -> str:
    if not e:
        return f"{kind} {name} | no record"
    methods = sorted({str(x.get("method")) for x in e.get("evidence") or [] if isinstance(x, dict)})
    summary = " ".join(str(e.get("summary") or "").split())[:220]
    parts = [f"{kind} {name}", f"status {e.get('status')}", f"summary: {summary}" if summary else "no summary",
             f"evidence: {', '.join(methods) or 'none'}"]
    if e.get("readable_c"):
        parts.append("readable C")
    return " | ".join(parts)


def field_record(name: str, records: dict[str, dict]) -> dict | None:
    """A RAM label for one struct's field (eCollisionData10_StatusEffectFinal): the field's entry
    (oCollisionData_StatusEffectFinal)."""
    m = re.match(r"^[ei]([A-Za-z0-9]+?)\d*_(\w+)$", name)
    if not m:
        return None
    for base in (m.group(1), re.sub(r"\d+$", "", m.group(1))):
        e = records.get(f"o{base}_{m.group(2)}")
        if e:
            return e
    return None


def snapshot(elf: str, rev: str | None, funcs, data) -> list[tuple[int, str, str, str]]:
    """(address, kind, name, text) for every symbol."""
    names, records = elf_names(elf), db_records(rev)
    rows = []
    for kind, addrs in (("function", funcs), ("data", data)):
        for addr, fallback in addrs:
            name = pick(names[addr]) if addr in names else (fallback or f"{addr:#x}")
            rows.append((addr, kind, name, describe(kind, name, records.get(name) or field_record(name, records))))
    return rows


def key(text: str) -> str:
    return hashlib.sha1(text.encode()).hexdigest()[:16]


def load_cache() -> dict[str, int]:
    cache = {}
    if os.path.exists(CACHE):
        for line in open(CACHE):
            r = json.loads(line)
            cache[r["k"]] = r["s"]
    return cache


def rate(batch: list[str], model: str) -> dict[int, int]:
    items = "\n".join(f"{i}. {t}" for i, t in enumerate(batch))
    reply = llm.complete(model, PROMPT.format(items=items), system=SYSTEM, max_tokens=4000, timeout=600,
                         tag="rank_meaning")
    got = {}
    for m in re.finditer(r"^\s*(\d+)\s*[:=.-]\s*([0-5])\b", reply or "", re.M):
        got[int(m.group(1))] = int(m.group(2))
    return got


def score_all(texts: list[str], model: str, size: int, jobs: int) -> dict[str, int]:
    cache = load_cache()
    todo = sorted({t for t in texts if key(t) not in cache})
    print(f"{len(texts)} symbols, {len(set(texts))} distinct texts, {len(todo)} to score", flush=True)
    batches = [todo[i:i + size] for i in range(0, len(todo), size)]
    os.makedirs(os.path.dirname(CACHE), exist_ok=True)
    with open(CACHE, "a") as out, concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        futures = {pool.submit(rate, b, model): b for b in batches}
        for done, f in enumerate(concurrent.futures.as_completed(futures), 1):
            b = futures[f]
            try:
                got = f.result()
            except Exception as e:      # a failed batch is scored on the next run
                print(f"batch failed: {str(e)[:120]}", flush=True)
                continue
            for i, s in got.items():
                if 0 <= i < len(b):
                    cache[key(b[i])] = s
                    out.write(json.dumps({"k": key(b[i]), "s": s}) + "\n")
            out.flush()
            if done % 20 == 0:
                print(f"{done}/{len(batches)} batches", flush=True)
    return cache


def summary(label: str, rows, scores) -> dict:
    got = [(kind, scores.get(key(t))) for _, kind, _, t in rows]
    rated = [s for _, s in got if s is not None]
    dist = Counter(rated)
    out = {"label": label, "symbols": len(rows), "rated": len(rated), "sum": sum(rated),
           "max": 5 * len(rated), "pct": 100 * sum(rated) / max(5 * len(rated), 1),
           "dist": [dist.get(i, 0) for i in range(6)]}
    for kind in ("function", "data"):
        ks = [s for k, s in got if k == kind and s is not None]
        out[kind] = (sum(ks), 5 * len(ks), 100 * sum(ks) / max(5 * len(ks), 1))
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--old-rev", required=True)
    p.add_argument("--old-elf", required=True)
    p.add_argument("--model", default="hyper/glm-5.3-flash")
    p.add_argument("--batch", type=int, default=50)
    p.add_argument("--jobs", type=int, default=8)
    a = p.parse_args()
    funcs = tsv_addresses(os.path.join(ROOT, "symbols", "imported_symbols.tsv"), 2)
    data = tsv_addresses(os.path.join(ROOT, "symbols", "imported_data_symbols.tsv"), 3)
    now = snapshot(os.path.join(ROOT, "bn6f", "bn6f.elf"), None, funcs, data)
    old = snapshot(a.old_elf, a.old_rev, funcs, data)
    scores = score_all([r[3] for r in now] + [r[3] for r in old], a.model, a.batch, a.jobs)
    with open(OUT, "w") as f:
        f.write("# address\tkind\tname\tscore\tname_before\tscore_before   (tools/sig/rank_meaning.py; before = "
                f"{a.old_rev})\n")
        for n, o in zip(now, old):
            sn, so = scores.get(key(n[3])), scores.get(key(o[3]))
            f.write(f"{n[0]:#010x}\t{n[1]}\t{n[2]}\t{'' if sn is None else sn}\t{o[2]}\t{'' if so is None else so}\n")
    for s in (summary(f"before ({a.old_rev})", old, scores), summary("now", now, scores)):
        print(json.dumps(s))


if __name__ == "__main__":
    main()
