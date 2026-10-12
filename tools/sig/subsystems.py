#!/usr/bin/env python3
"""Group the game's functions into subsystems: the call graph plus bn6f's source files.

    python3 tools/sig/subsystems.py [--graph build-abi/graph.json] [--out tools/sig/subsystems.json]

A node is a lifted unit (graph.json `entries`, by `unit`). Edges are the direct calls and tail
calls of `sites` plus the call edges the routes observed (`edges`: jump tables, pointers).
Grouping, deterministic:
  1. Hubs (called from 12 or more other units) are left out at first: they are shared helpers.
  2. Within one bn6f source file, units are joined along call edges, the closest addresses first,
     while the group stays at most CAP units.
  3. A unit with no edge inside its file joins the group of the unit just before it in the file
     (bn6f keeps related code together), if that group has room.
  4. A hub joins the group that holds most of its callers when that group holds 70% of them;
     otherwise it is a subsystem of its own ("shared").
Per subsystem: its units in address order, which are entry points (called from outside it, a
target of calls the routes saw from outside, or with no known caller) and which are internal,
how many are readable, captured (build-snapshots/NAME) and executed by a route (coverage .prof).
"""
from __future__ import annotations

import argparse
import collections
import glob
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# git-ignored outputs (coverage profiles) live in the main checkout; a worktree may lack them
MAIN = os.path.dirname(os.path.realpath(os.path.join(ROOT, ".git"))) if os.path.isdir(os.path.join(ROOT, ".git")) else \
    os.path.normpath(os.path.join(ROOT, open(os.path.join(ROOT, ".git")).read().split("gitdir:")[1].strip(), "../../.."))
CAP = 60
HUB = 12


def load_units(g: dict) -> tuple[dict, dict]:
    """unit -> {addr, src, entries, readable}; entry name -> unit."""
    units: dict[str, dict] = {}
    of: dict[str, str] = {}
    for name, e in g["entries"].items():
        u = units.setdefault(e["unit"], {"addr": None, "src": None, "entries": [], "readable": 0})
        u["entries"].append(name)
        of[name] = e["unit"]
        u["readable"] += bool(e.get("readable"))
        a = int(e["addr"], 16)
        if name == e["unit"]:
            u["own"] = a
        u["addr"] = u.get("own", min(a, u["addr"] if u["addr"] is not None else a))
        if e.get("src") and (name == e["unit"] or not u["src"]):
            u["src"] = e["src"].split(":")[0]
    return units, of


def call_edges(g: dict, of: dict) -> set[tuple[str, str]]:
    out = set()
    for s in g["sites"]:
        if s["kind"] in ("call", "tail") and s.get("target") in of and s["caller"] in of:
            a, b = of[s["caller"]], of[s["target"]]
            if a != b:
                out.add((a, b))
    for x in g["edges"]:
        if x.get("site_unit") in of and x.get("target") in of:
            a, b = of[x["site_unit"]], of[x["target"]]
            if a != b:
                out.add((a, b))
    return out


def coverage() -> set[int]:
    seen = set()
    for p in glob.glob(os.path.join(MAIN, "tools/traces/coverage/*.prof")):
        for line in open(p):
            m = re.match(r"([0-9a-f]{8}) ", line)
            if m:
                seen.add(int(m.group(1), 16))
    return seen


def group(units: dict, edges: set) -> dict[str, int]:
    callers = collections.defaultdict(set)
    for a, b in edges:
        callers[b].add(a)
    hubs = {u for u in units if len(callers[u]) >= HUB}
    parent = {u: u for u in units}
    size = {u: 1 for u in units}

    def find(x: str) -> str:
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a: str, b: str) -> bool:
        ra, rb = find(a), find(b)
        if ra == rb:
            return True
        if size[ra] + size[rb] > CAP:
            return False
        if size[ra] < size[rb]:
            ra, rb = rb, ra
        parent[rb] = ra
        size[ra] += size[rb]
        return True

    inner = sorted(((abs(units[a]["addr"] - units[b]["addr"]), a, b) for a, b in edges
                    if a not in hubs and b not in hubs and units[a]["src"] == units[b]["src"]))
    linked = set()
    for _d, a, b in inner:
        union(a, b)
        linked.update((a, b))
    by_file = collections.defaultdict(list)
    for u, v in units.items():
        by_file[v["src"]].append(u)
    for f, us in by_file.items():
        us.sort(key=lambda u: units[u]["addr"])
        for prev, u in zip(us, us[1:]):
            if u not in linked and u not in hubs and prev not in hubs:
                union(prev, u)
    for h in sorted(hubs, key=lambda u: units[u]["addr"]):
        votes = collections.Counter(find(c) for c in callers[h] if c not in hubs)
        if votes:
            top, n = votes.most_common(1)[0]
            if n >= 0.7 * sum(votes.values()) and units[next(iter(c for c in callers[h] if find(c) == top))]["src"] == units[h]["src"]:
                union(top, h)
    return {u: find(u) for u in units}


def name_of(members: list[str], src: str | None, addr: int) -> str:
    words = collections.Counter()
    for m in members:
        w = re.match(r"([A-Za-z]+?)(?:_|[A-Z0-9])", m)
        if w and not re.fullmatch(r"sub|loc|nullsub|dead|unk|tfunc|thunk", w.group(1), re.I):
            words[w.group(1)] += 1
    stem = (src or "nosrc").removesuffix(".s")
    top = words.most_common(1)
    return f"{stem}/{top[0][0] if top else 'misc'}_{addr:07X}"


def main() -> None:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("--graph", default=os.path.join(ROOT, "build-abi/graph.json"))
    ap.add_argument("--out", default=os.path.join(ROOT, "tools/sig/subsystems.json"))
    args = ap.parse_args()
    g = json.load(open(args.graph))
    units, of = load_units(g)
    edges = call_edges(g, of)
    root = group(units, edges)
    snaps = set(os.listdir(os.path.join(ROOT, "build-snapshots"))) if os.path.isdir(os.path.join(ROOT, "build-snapshots")) else set()
    cov = coverage()
    callers = collections.defaultdict(set)
    for a, b in edges:
        callers[b].add(a)
    members = collections.defaultdict(list)
    for u, r in root.items():
        members[r].append(u)
    subs = []
    for r, us in members.items():
        us.sort(key=lambda u: units[u]["addr"])
        inside = set(us)
        rows = []
        for u in us:
            e = g["entries"].get(u, {})
            outside = sorted(callers[u] - inside)
            entry = bool(outside) or not callers[u] or bool(e.get("indirect_target"))
            rows.append({"unit": u, "addr": f"{units[u]['addr']:#010x}", "entry": entry,
                         "callers_outside": len(outside), "readable": units[u]["readable"] > 0,
                         "captured": any(n in snaps for n in units[u]["entries"]),
                         "covered": units[u]["addr"] in cov or (units[u]["addr"] | 1) in cov})
        src = units[us[0]]["src"]
        subs.append({"name": name_of(us, src, units[us[0]]["addr"]), "src": src, "size": len(us),
                     "entries": sum(r["entry"] for r in rows), "readable": sum(r["readable"] for r in rows),
                     "captured": sum(r["captured"] for r in rows), "covered": sum(r["covered"] for r in rows),
                     "units": rows})
    subs.sort(key=lambda s: int(s["units"][0]["addr"], 16))
    seen = collections.Counter()
    for s in subs:
        seen[s["name"]] += 1
        if seen[s["name"]] > 1:
            s["name"] += f".{seen[s['name']]}"
    json.dump({"format": "tools/sig/subsystems.py v1", "cap": CAP, "hub": HUB, "subsystems": subs},
              open(args.out, "w"), indent=0)
    sizes = collections.Counter(min(s["size"] // 10 * 10, 60) for s in subs)
    print(f"{len(subs)} subsystems over {len(units)} units; sizes (bucket of 10, 60 = 60+): {sorted(sizes.items())}")


if __name__ == "__main__":
    main()
