#!/usr/bin/env python3
"""Record the knowledge layer: a `signature:` block in each symbols/db entry, and every function's
signature in symbols/signatures.tsv (most functions have no db entry yet).

    python3 tools/sig/write_db.py [--final build/sig/final.json] [--only NAME...]

The block (symbols/db/README.md, "signature"): the proposed C prototype, each parameter's register,
type and name, the result, the toolkit global, the registers an adapter must still keep for lifted
callers, confidence (confident | unsure | none) and who decided it, and the evidence lines (contract
bounds, liveness, captured calls, bn6f comment, readable C, the model's answer and any disagreement).
Replaced in place on a rerun; nothing else in an entry changes (the text is edited line by line, as
tools/stamps.py does for `stamp:`).
"""
from __future__ import annotations

import argparse
import datetime
import glob
import json
import os
import re

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DB = os.path.join(ROOT, "symbols/db")
TSV = os.path.join(ROOT, "symbols/signatures.tsv")


def regkey(r: str) -> int:
    return int(r[1:]) if r[1:].isdigit() else 99


def chosen(r: dict) -> tuple[list[dict], dict]:
    if r.get("chosen"):
        return r["chosen"]["params"], r["chosen"]["ret"]
    if r.get("model"):          # unsure: the model's proposal, for the record
        return ([p for p in r["model"].get("params", []) if isinstance(p, dict) and p.get("reg") != "r10"],
                r["model"].get("ret") or {})
    return ([{"reg": p["reg"], "type": p["type"] or "u32", "name": p["name"] or p["reg"]} for p in r["params"]
             if p["reg"] != "r10"], {"reg": r["ret"]["from"], "type": r["ret"]["type"] or "u32"})


def proto(name: str, r: dict) -> str:
    ps, ret = chosen(r)
    args = ", ".join(f"{p.get('type') or 'u32'}{'' if str(p.get('type', '')).endswith('*') else ' '}{p.get('name') or p.get('reg')}"
                     for p in ps) or "void"
    rt = ret.get("type") or "void"
    if ret.get("reg") == "r0:r1":
        rt = "U32Pair"
    return f"{rt} {name}({args})"


def evidence(r: dict) -> list[str]:
    ev = [f"contract (lifter, refined): may read {' '.join(r['upper_args']) or 'nothing'}; result {r['ret']['from']}"]
    if r.get("narrow_args") is not None:
        ev.append(f"liveness over the assembly and callees (tools/sig/derive.py): reads {' '.join(r['narrow_args']) or 'nothing'}")
    elif r.get("narrow_partial") is not None:
        ev.append(f"liveness: {' '.join(r['narrow_partial']) or 'nothing'}, but through a callee it could not narrow")
    else:
        ev.append("liveness: not run (a computed jump or a call through an unseen pointer)")
    for p in r["params"]:
        ev += [f"{p['reg']}: {x}" for x in p["evidence"]]
    ev += [f"result: {x}" for x in r["ret"]["evidence"]]
    if r.get("bn6f_comment"):
        ev.append(f"bn6f comment: {r['bn6f_comment']}")
    if r.get("model"):
        m = r["model"]
        ev.append(f"model {m['by']}: {m.get('why', '')}".strip())
    for d in r.get("disagreements") or []:
        ev.append(f"disagreement: {d}")
    if r.get("chosen", {}).get("dropped_by_liveness"):
        ev.append(f"the model kept {' '.join(r['chosen']['dropped_by_liveness'])}; liveness shows nothing reads them")
    if r.get("callee_may_unwind"):
        ev.append("a callee may return elsewhere (lifter): calls to it go through the guest")
    if r["unwinds"]:
        ev.append("returns somewhere other than its caller (lifter): no C signature")
    return ev


def block(name: str, r: dict) -> dict:
    ps, ret = chosen(r)
    sig = {
        "c": proto(name, r) if r["confidence"] != "none" else "none (returns elsewhere)",
        "params": [{"reg": p.get("reg"), "type": p.get("type") or "u32", "name": p.get("name") or p.get("reg")} for p in ps],
        "ret": {"reg": ret.get("reg", "none"), "type": ret.get("type") or "void"},
    }
    if r.get("globals"):
        sig["globals"] = r["globals"]
    if r.get("keep"):
        sig["keep"] = r["keep"]
    sig["confidence"] = r["confidence"]
    sig["decided_by"] = r.get("decided_by", "-") + (f" ({r['model']['by']})" if r.get("decided_by") == "model+mechanical" else "")
    sig["evidence"] = evidence(r)
    sig["reproduce"] = ("python3 tools/sig/bounds.py && python3 tools/sig/derive.py && "
                        "python3 tools/sig/ask.py run && python3 tools/sig/ask.py merge")
    sig["derived"] = datetime.date.today().isoformat()
    return sig


def replace_block(text: str, key: str, new: str) -> str:
    lines = text.splitlines(keepends=True)
    out, i = [], 0
    while i < len(lines):
        if re.match(rf"{key}:\s*$", lines[i]):
            i += 1
            while i < len(lines) and (lines[i].startswith((" ", "-")) or not lines[i].strip()):
                i += 1
            continue
        out.append(lines[i])
        i += 1
    body = "".join(out)
    if body and not body.endswith("\n"):
        body += "\n"
    m = re.search(r"(?m)^stamp:\s*$", body)
    return body[:m.start()] + new + body[m.start():] if m else body + new


def main() -> None:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("--final", default=os.path.join(ROOT, "build/sig/final.json"))
    ap.add_argument("--only", nargs="*")
    a = ap.parse_args()
    final = json.load(open(a.final))
    by_addr = {int(r["addr"], 16): n for n, r in final.items()}
    written = 0
    for path in sorted(glob.glob(os.path.join(DB, "*.yaml"))):
        text = open(path).read()
        head = yaml.safe_load(text)
        if not isinstance(head, dict) or head.get("kind") != "function":
            continue
        name = head["name"] if head["name"] in final else by_addr.get(int(str(head.get("address", "0")), 16))
        if not name or (a.only and name not in a.only):
            continue
        new = yaml.safe_dump({"signature": block(name, final[name])}, sort_keys=False, allow_unicode=True, width=110)
        out = replace_block(text, "signature", new)
        if out != text:
            open(path, "w").write(out)
            written += 1
    rows = ["name\taddress\tconfidence\tdecided_by\tc\tregisters\tevidence"]
    for n, r in sorted(final.items(), key=lambda kv: int(kv[1]["addr"], 16)):
        ps, ret = chosen(r)
        regs = " ".join(p.get("reg") or "?" for p in ps) + f" -> {ret.get('reg', 'none')}"
        ev = "; ".join(evidence(r))[:300].replace("\t", " ").replace("\n", " ")
        c = proto(n, r) if r["confidence"] != "none" else "none"
        rows.append(f"{n}\t{r['addr']}\t{r['confidence']}\t{r.get('decided_by', '-')}\t{c}\t{regs}\t{ev}")
    open(TSV, "w").write("\n".join(rows) + "\n")
    print(f"signature: blocks written in {written} db entries; {len(final)} rows in {os.path.relpath(TSV, ROOT)}")


if __name__ == "__main__":
    main()
