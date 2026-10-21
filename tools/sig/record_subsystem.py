#!/usr/bin/env python3
"""Record a subsystem converted as a whole in symbols/db: each member's entry gets the new file in
`readable_c:`, evidence items (readable-c, difftest, cpu-oracle, lockstep), a `changes:` item and the
members it calls or is called by in `related:`; a member that is no longer a `Next` body (internal: a
static typed C function its subsystem calls directly) loses its now stale `stamp:`.

    python3 tools/sig/record_subsystem.py SPEC.json

SPEC: {"subsystem", "file", "members": [...], "entries": [...], "names": {member: c_name},
       "readable": "...", "difftest": "...", "oracle": "...", "lockstep": "...", "reproduce": {...},
       "commit": "...", "date": "YYYY-MM-DD"}
Entries are edited as text, key by key (the order of symbols/db/README.md), so nothing else changes.
"""
from __future__ import annotations

import json
import os
import re
import sys

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import names as symnames  # noqa: E402

DB = os.path.join(ROOT, "symbols/db")
ORDER = ["name", "address", "kind", "previous_names", "status", "summary", "interface", "details", "evidence",
         "changes", "readable_c", "coverage", "related", "open_questions", "sources", "abi", "signature", "stamp"]


def dump(key: str, value) -> str:
    return yaml.safe_dump({key: value}, sort_keys=False, allow_unicode=True, width=110)


def blocks(text: str) -> list[tuple[str, str]]:
    out: list[tuple[str, str]] = []
    for line in text.splitlines(keepends=True):
        m = re.match(r"([a-z_]+):", line)
        if m and not line.startswith((" ", "-")):
            out.append((m.group(1), line))
        elif out:
            out[-1] = (out[-1][0], out[-1][1] + line)
    return out


def rebuild(parts: list[tuple[str, str]]) -> str:
    """The entry's keys in their own order; a new key goes before `signature:`/`stamp:` (README order)."""
    return "".join(v if v.endswith("\n") else v + "\n" for _k, v in parts)


def insert(parts: list[tuple[str, str]], key: str, text: str) -> list[tuple[str, str]]:
    at = next((i for i, (k, _v) in enumerate(parts) if k in ("signature", "stamp")), len(parts))
    return parts[:at] + [(key, text)] + parts[at:]


def main() -> None:
    spec = json.load(open(sys.argv[1]))
    members, entries = spec["members"], set(spec["entries"])
    graph = symnames.resolved_graph(json.load(open(os.path.join(ROOT, "build-abi/graph.json"))))
    calls = {}
    for s in graph["sites"]:
        if s["kind"] in ("call", "tail") and s.get("target") in members and s["caller"] in members:
            calls.setdefault(s["caller"], set()).add(s["target"])
            calls.setdefault(s["target"], set()).add(s["caller"])
    for e in graph["edges"]:
        if e.get("site_unit") in members and e.get("target") in members:
            calls.setdefault(e["site_unit"], set()).add(e["target"])
            calls.setdefault(e["target"], set()).add(e["site_unit"])
    done = 0
    for name in members:
        path = os.path.join(DB, name + ".yaml")
        if not os.path.exists(path):
            print(f"no entry: {name}")
            continue
        text = open(path).read()
        data = yaml.safe_load(text)
        parts = [(k, v) for k, v in blocks(text)]
        keys = [k for k, _v in parts]
        role = ("an entry point: its `Next` adapter reads the registers and calls the typed function "
                f"`{spec['names'].get(name, '?')}`" if name in entries else
                f"internal: the static typed function `{spec['names'].get(name, '?')}`, which the subsystem "
                "calls directly; no register adapter (the lifted original stays as L_ code for the difftest)")
        ev = list(data.get("evidence") or [])
        ev.append({"method": "readable-c", "date": spec["date"],
                   "detail": f"{spec['readable']} This function is {role}.",
                   "refs": [spec["file"]], "reproduce": spec["reproduce"]["readable"]})
        ev.append({"method": "difftest", "date": spec["date"], "detail": spec["difftest_detail"],
                   "reproduce": spec["reproduce"]["difftest"], "result": spec["difftest"]})
        ev.append({"method": "cpu-oracle", "date": spec["date"], "detail": spec["oracle_detail"],
                   "reproduce": spec["reproduce"]["oracle"], "result": spec["oracle"]})
        ev.append({"method": "lockstep", "date": spec["date"],
                   "detail": "Palette, VRAM and OAM identical to the original at every checkpoint.",
                   "reproduce": spec["reproduce"]["lockstep"], "result": spec["lockstep"]})
        ch = list(data.get("changes") or [])
        ch.append({"date": spec["date"], "what": f"readable C rewritten with its whole subsystem ({spec['subsystem']}) "
                   f"in {spec['file']}", "where": f"recomp {spec['commit']}", "tool": "tools/sig (subsystem pilot)"})
        rel = sorted(set(data.get("related") or []) | calls.get(name, set()))
        new = {"evidence": dump("evidence", ev), "changes": dump("changes", ch),
               "readable_c": dump("readable_c", spec["file"])}
        if rel:
            new["related"] = dump("related", rel)
        parts = [(k, new.pop(k, v)) for k, v in parts]
        for k, v in new.items():
            parts = insert(parts, k, v)
        if name not in entries:
            parts = [(k, v) for k, v in parts if k != "stamp"]
        out = rebuild(parts)
        if out != text:
            open(path, "w").write(out)
            done += 1
        _ = keys
    print(f"{done} entries recorded")


if __name__ == "__main__":
    main()
