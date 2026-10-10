#!/usr/bin/env python3
"""The symbol database's one-script rewrite (review 2 Area 6: A9, A19, C5; joint plan S1 "db and layout").

  python3 tools/symdb_rewrite.py [--dry-run] [-v]   rewrite symbols/db in place; a second run changes nothing
  python3 tools/symdb_rewrite.py --show NAME...     what it would change in these entries (nothing written)
  python3 tools/symdb_rewrite.py --verify [REV]     check the entries against REV's (default HEAD): only
                                                     the changes below, and every other byte of evidence kept

It changes `reproduce:` lines and `related:` lists, nothing else; every other line of an entry stays
byte for byte (the file is edited where those two keys are, not re-dumped).

1. Through the launcher. `cd tools/wf && .venv/bin/python -m wf CMD` and the `.venv/bin/python -m wf`
   lines after it become `./wf CMD`, run from the repo root (paths re-based). The placeholder
   SNAPSHOT_DIR and the old link build-wf-runs/snapshots-latest become build-snapshots, the snapshot
   store; a literal `--function NAME` gets the entry's name.
2. A farm group's convert command (A9). `./wf convert --subsystem S --file leaves_farm_N.c F1 ... Fn`
   repeated the whole group in each of its n entries; it becomes the command for this function alone,
   under its current name. Nothing is lost: the group is exactly the entries whose readable-c
   evidence cites the same run in its detail (checked here before anything is written).
3. A farm group's `related:` (A19) listed the rest of the group. It keeps only the members that call
   this function or that it calls (direct call sites in build-abi/graph.json, through
   tools/funcindex.py), under their current names; the key goes when none is left.
4. /tmp (C5). The scratch folders the old commands used (/tmp/snaps, /tmp/snaps_npc, /tmp/oracle,
   /tmp/before) are gone; the same commands now write to build/repro/ (ignored build output). The
   captured calls are made again by the very capture command beside them (a replay of a recorded
   route is deterministic), the oracle report and the before-files by theirs. An oracle line with no
   capture beside it takes its base states from build-snapshots, the store that survives.
   tools/convert/farm.py has had no command line since b56d9d4: its lines become `./wf farm`.
5. Today's names. A symbol's earlier name in a command (not in a comment) becomes its current one
   (from the entries' previous_names), so the command runs against today's bn6f.
"""
from __future__ import annotations

import os
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass, field
from typing import Any

import yaml

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funcindex  # noqa: E402  # tools/funcindex.py: direct callers and callees from the ABI export
import gamepaths  # noqa: E402  # tools/gamepaths.py: where the ABI export is
from stamps import Names  # noqa: E402  # every name of a symbol -> its first name, and today's name

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DB = "symbols/db"
WIDTH = 110                       # what the convert workflow's entries are dumped with (wf/tools/evidence.py)
RUN_ID = re.compile(r"\b[a-z][a-z-]*-\d{8}-\d{6}(?:-\d+)?\b")
CONVERT_RUN = re.compile(r"\brun (convert-\d{8}-\d{6})\b")
FARM_FILE = re.compile(r"leaves_farm_\d+\.c")
WF_CD = "cd tools/wf && .venv/bin/python -m wf "
WF_VENV = ".venv/bin/python -m wf "
SCRATCH = ("snaps_npc", "snaps", "oracle", "before")   # the /tmp folders the old commands used
FARM_PY = re.compile(r"^python3 tools/convert/farm\.py --snaps (\S+) --models ([^\s,]+) (.*)$")
CONVERT_OPTIONS = {"--subsystem", "--file", "--model", "--rounds", "--snapshots", "--farm-model", "--candidate"}


class Unhandled(Exception):
    """A reproduce line this script does not know how to rewrite (fix it by hand, then rerun)."""


# ── the db as a whole: names, addresses, farm groups, call neighbours ──

@dataclass
class Group:
    run: str
    file: str
    members: list[str]            # as the command listed them (names of the day)


@dataclass
class World:
    names: Names
    address: dict[str, int]                       # root -> address
    neighbours: dict[int, frozenset[int]]
    groups: dict[str, Group] = field(default_factory=dict)          # entry name -> its farm group

    def root(self, name: str) -> str:
        return self.names.root.get(name, name)

    def current(self, name: str) -> str:
        return self.names.current.get(self.names.root.get(name, name), name)

    def current_words(self, code: str) -> str:
        """CODE with every earlier name of a db symbol replaced by today's (commands must run today)."""
        return re.sub(r"[A-Za-z_]\w*", lambda m: self.current(m.group(0)), code)


def load(paths: list[str]) -> dict[str, tuple[str, dict[str, Any]]]:
    out = {}
    for p in paths:
        with open(os.path.join(ROOT, p), encoding="utf-8") as fh:
            text = fh.read()
        out[p] = (text, yaml.load(text, Loader=yaml.CSafeLoader))
    return out


def convert_words(line: str) -> tuple[list[str], list[str]] | None:
    """(options with their values, function names) of a one-line `./wf convert` command, else None."""
    if not line.startswith(("./wf convert ", WF_CD + "convert ")) or "#" in line:
        return None
    words = shlex.split(line.split(" -m wf ", 1)[-1] if line.startswith("cd ") else line[len("./wf "):])
    options: list[str] = []
    functions: list[str] = []
    it = iter(words[1:])
    for w in it:
        if w in CONVERT_OPTIONS:
            options += [w, next(it)]
        elif w.startswith("--"):
            options.append(w)
        else:
            functions.append(w)
    return options, functions


def farm_group(e: dict[str, Any]) -> Group | None:
    """The farm group an entry's readable-c evidence names (the whole group's convert command)."""
    for ev in e.get("evidence") or []:
        text = ev.get("reproduce") if isinstance(ev, dict) else None
        if ev.get("method") != "readable-c" or not isinstance(text, str):
            continue
        for line in text.split("\n"):
            parsed = convert_words(line)
            if parsed is None:
                continue
            options, functions = parsed
            file = options[options.index("--file") + 1] if "--file" in options else ""
            if FARM_FILE.fullmatch(file) and len(functions) > 1:
                run = CONVERT_RUN.search(str(ev.get("detail", "")))
                if run is None:
                    raise Unhandled(f"{e['name']}: a farm group's evidence without its run id")
                return Group(run.group(1), file, functions)
    return None


def world(entries: dict[str, tuple[str, dict[str, Any]]], need_graph: bool = True) -> World:
    es = [e for _, e in entries.values()]
    names = Names.of(es)
    address = {}
    for e in es:
        a = e.get("address")
        address[names.root.get(e["name"], e["name"])] = (int(a, 16) if isinstance(a, str) else int(a)) & ~1
    neighbours = funcindex.call_neighbours(gamepaths.ABI_GRAPH) if need_graph else {}
    w = World(names, address, neighbours)
    by_run: dict[str, set[str]] = {}
    for e in es:
        g = farm_group(e)
        if g is not None:
            w.groups[e["name"]] = g
            by_run.setdefault(g.run, set()).add(w.root(e["name"]))
    for name, g in w.groups.items():       # the group is exactly the entries citing its run
        listed = {w.root(m) for m in g.members}
        if listed != by_run[g.run]:
            raise Unhandled(f"{name}: run {g.run}'s group is not the entries citing it "
                            f"({sorted(listed ^ by_run[g.run])[:5]})")
        if w.root(name) not in listed:
            raise Unhandled(f"{name}: not in its own group's command")
    return w


# ── one reproduce text ──

def rewrite_reproduce(text: str, me: str, group: Group | None, method: str | None, w: World) -> str:
    lines = text.split("\n")
    captures_here = any(re.search(r"capture\.sh /tmp/snaps\b", line) for line in lines)
    out: list[str] = []
    in_wf = False                               # a `cd tools/wf` earlier in the text is in force
    for line in lines:
        wf_line = False
        if line.startswith(WF_CD):
            line, in_wf, wf_line = "./wf " + line[len(WF_CD):], True, True
        elif in_wf and line.startswith(WF_VENV):
            line, wf_line = "./wf " + line[len(WF_VENV):], True
        elif in_wf and line.strip() and not line.lstrip().startswith("#"):
            raise Unhandled(f"{me}: a command in tools/wf that is not wf: {line!r}")
        if wf_line:
            line = line.replace("../../build-wf-runs/snapshots-latest", "build-snapshots")
            line = re.sub(r"(?<=\s)SNAPSHOT_DIR\b", "build-snapshots", line)
            line = re.sub(r"(?<=--function )NAME\b", me, line)
            line = line.replace(" ../../", " ")
            if "../" in line.split("#")[0]:
                raise Unhandled(f"{me}: a path relative to tools/wf left: {line!r}")
        line = line.replace("build-wf-runs/snapshots-latest", "build-snapshots")
        if line.lstrip().startswith("#") or "  #" in line:
            code, sep, comment = line.partition("#") if line.lstrip().startswith("#") else line.partition("  #")
            line = code + sep + comment.replace("python -m wf ", "./wf ")
        moved = rewrite_tmp(line, me, captures_here)
        m = FARM_PY.match(moved)
        if m:
            snaps, model, rest = m.groups()
            moved = f"./wf farm --model {model} --snapshots {snaps} {rest}"
        if not moved.lstrip().startswith("#"):  # a command names symbols by today's names, so it runs
            code, sep, comment = moved.partition("  #")
            moved = w.current_words(code) + sep + comment
        line = moved
        if group is not None and method == "readable-c":
            parsed = convert_words(line)
            if parsed is not None and FARM_FILE.fullmatch(parsed[0][parsed[0].index("--file") + 1]) \
                    and len(parsed[1]) > 1:
                line = f"./wf convert {' '.join(shlex.quote(w) for w in parsed[0])} {me}"
        out.append(line)
    new = "\n".join(out)
    if "/tmp" in new:
        raise Unhandled(f"{me}: /tmp left in {new!r}")
    return new


def rewrite_tmp(line: str, me: str, captures_here: bool) -> str:
    if "/tmp" not in line:
        return line
    line = line.replace("ORACLE_TMP=/tmp ", "")
    if "--base /tmp/snaps" in line and not captures_here:
        line = line.replace("--base /tmp/snaps", "--base build-snapshots")
    inside_cport = line.startswith("(cd cport && ")
    for d in SCRATCH:
        line = re.sub(rf"/tmp/{d}\b", ("../build/repro/" if inside_cport else "build/repro/") + d, line)
    return line


# ── one entry: new values, then the text edited where they are ──

@dataclass
class Change:
    reproduce: dict[int, str] = field(default_factory=dict)    # evidence index -> new text
    related: list[str] | None = None                           # new list ([] = drop the key)

    def __bool__(self) -> bool:
        return bool(self.reproduce) or self.related is not None


def changes(e: dict[str, Any], w: World) -> Change:
    me = e["name"]
    group = w.groups.get(me)
    ch = Change()
    for i, ev in enumerate(e.get("evidence") or []):
        text = ev.get("reproduce") if isinstance(ev, dict) else None
        if isinstance(text, str):
            new = rewrite_reproduce(text, me, group, ev.get("method"), w)
            if new != text:
                ch.reproduce[i] = new
    related = e.get("related")
    if group is not None and isinstance(related, list):
        mine = w.address[w.root(me)]
        near = w.neighbours.get(mine, frozenset())
        keep = [w.current(n) for n in related if w.address.get(w.root(n)) in near]
        if keep != related:
            ch.related = keep
    return ch


def _offsets(text: str) -> list[int]:
    starts = [0]
    for line in text.split("\n")[:-1]:
        starts.append(starts[-1] + len(line) + 1)
    return starts


def _pos(starts: list[int], mark: Any) -> int:
    return starts[mark.line] + mark.column


def _dump_item_key(key: str, value: Any) -> str:
    """KEY: VALUE as it stands inside a top-level list's item (indent 2, continuation lines indent 4)."""
    text = yaml.dump([{key: value}], Dumper=yaml.CSafeDumper, sort_keys=False, width=WIDTH, allow_unicode=True)
    assert text.startswith("- ")
    return text[2:]


def apply(text: str, e: dict[str, Any], ch: Change) -> str:
    node = yaml.compose(text, Loader=yaml.CSafeLoader)
    starts = _offsets(text)
    edits: list[tuple[int, int, str]] = []
    for k, v in node.value:
        if k.value == "evidence":
            for i, item in enumerate(v.value):
                if i not in ch.reproduce:
                    continue
                for kk, vv in item.value:
                    if kk.value == "reproduce":
                        if kk.start_mark.column != 2:
                            raise Unhandled(f"{e['name']}: evidence keys not at column 2")
                        a, b = _pos(starts, kk.start_mark), _pos(starts, vv.end_mark)
                        new = _dump_item_key("reproduce", ch.reproduce[i])
                        edits.append((a, b, new if text[b - 1] == "\n" else new.rstrip("\n")))
        elif k.value == "related" and ch.related is not None:
            a, b = _pos(starts, k.start_mark), _pos(starts, v.end_mark)
            if k.start_mark.column != 0:
                raise Unhandled(f"{e['name']}: related not at column 0")
            if text[b - 1] != "\n":                 # a flow list ends on its own line: take the newline too
                b = text.index("\n", b) + 1
            edits.append((a, b, yaml.dump({"related": ch.related}, Dumper=yaml.CSafeDumper, sort_keys=False,
                                          width=WIDTH, allow_unicode=True) if ch.related else ""))
    for a, b, new in sorted(edits, reverse=True):
        text = text[:a] + new + text[b:]
    expected = dict(e)
    if ch.reproduce:
        expected["evidence"] = [dict(ev, reproduce=ch.reproduce[i]) if i in ch.reproduce else ev
                                for i, ev in enumerate(e["evidence"])]
    if ch.related is not None:
        if ch.related:
            expected["related"] = ch.related
        else:
            del expected["related"]
    if yaml.load(text, Loader=yaml.CSafeLoader) != expected:
        raise Unhandled(f"{e['name']}: the edited text does not read back as intended")
    return text


# ── verify: what changed between REV and the working tree is only the above ──

STANDALONE_HEX = re.compile(r"(?<![\w./-])[0-9a-f]{7,40}(?![\w/])")
STANDALONE_INT = re.compile(r"(?<![\w./-])\d+(?![\w./])")


def verify(rev: str) -> bool:
    paths = sorted(p for p in subprocess.run(["git", "ls-tree", "--name-only", rev, f"{DB}/"], cwd=ROOT,
                                             capture_output=True, text=True, check=True).stdout.split()
                   if p.endswith(".yaml"))
    now = load(paths)
    old = {p: subprocess.run(["git", "show", f"{rev}:{p}"], cwd=ROOT, capture_output=True, text=True,
                             check=True).stdout for p in paths}
    w = world({p: (t, yaml.load(t, Loader=yaml.CSafeLoader)) for p, t in old.items()}, need_graph=False)
    bad: list[str] = []
    counts = {"entries": len(paths), "changed": 0, "reproduce": 0, "related": 0}
    for p in paths:
        text, new = now[p]
        o = yaml.load(old[p], Loader=yaml.CSafeLoader)
        if text != old[p]:
            counts["changed"] += 1

        def fail(msg: str, p: str = p) -> None:
            bad.append(f"{p}: {msg}")
        if set(new) not in (set(o), set(o) - {"related"}):
            fail(f"keys {sorted(set(o) ^ set(new))} changed")
        for k in o:
            if k not in ("evidence", "related") and o[k] != new.get(k):
                fail(f"{k} changed")
        if "stamp" in o and old[p][old[p].index("\nstamp:"):] != text[text.index("\nstamp:"):]:
            fail("the stamp block's text changed")
        oe, ne = o.get("evidence") or [], new.get("evidence") or []
        if len(oe) != len(ne):
            fail("evidence items added or removed")
        for a, b in zip(oe, ne):
            if {k: v for k, v in a.items() if k != "reproduce"} != {k: v for k, v in b.items() if k != "reproduce"}:
                fail(f"evidence {a.get('method')}: more than its reproduce line changed")
            ra, rb = str(a.get("reproduce") or ""), str(b.get("reproduce") or "")
            if ra != rb:
                counts["reproduce"] += 1
                for pattern, what in ((STANDALONE_HEX, "commit or address"), (STANDALONE_INT, "number"),
                                      (RUN_ID, "run id")):
                    lost = set(pattern.findall(ra)) - set(pattern.findall(rb))
                    if lost:
                        fail(f"evidence {a.get('method')}: reproduce lost {what}s {sorted(lost)[:5]}")
                if "/tmp" in rb:
                    fail("a /tmp path is left")
        if o.get("related") != new.get("related"):
            counts["related"] += 1
            if new["name"] not in w.groups:
                fail("related changed, but it is not a farm group's list")
            kept = {w.root(n) for n in new.get("related") or []}
            if not kept <= {w.root(n) for n in o.get("related") or []}:
                fail("related gained names")
        for found in RUN_ID.findall(old[p]):
            if found not in text:
                fail(f"run id {found} lost")
    for line in bad[:40]:
        print(line)
    print(f"{counts['entries']} entries, {counts['changed']} changed ({counts['reproduce']} reproduce lines, "
          f"{counts['related']} related lists); {len(bad)} problems")
    return not bad


# ── main ──

def main(argv: list[str]) -> int:
    if "--verify" in argv:
        rest = [a for a in argv if not a.startswith("-")]
        return 0 if verify(rest[0] if rest else "HEAD") else 1
    show = "--show" in argv
    names = [a for a in argv if not a.startswith("-")]
    paths = sorted(f"{DB}/{f}" for f in os.listdir(os.path.join(ROOT, DB)) if f.endswith(".yaml"))
    entries = load(paths)
    w = world(entries)
    before = after = 0
    changed: list[str] = []
    for p, (text, e) in entries.items():
        if names and e["name"] not in names:
            continue
        ch = changes(e, w)
        new = apply(text, e, ch) if ch else text
        before += len(text.encode())
        after += len(new.encode())
        if new == text:
            continue
        changed.append(p)
        if show or "-v" in argv:
            print(f"── {p}")
        if show:
            for line in subprocess.run(["diff", "-u", "--label", "before", "--label", "after",
                                        os.path.join(ROOT, p), "-"], input=new, capture_output=True,
                                       text=True).stdout.splitlines()[2:]:
                print(line)
        elif "--dry-run" not in argv:
            with open(os.path.join(ROOT, p), "w", encoding="utf-8") as fh:
                fh.write(new)
    verb = "would change" if show or "--dry-run" in argv else "changed"
    print(f"{verb} {len(changed)} of {len(entries)} entries; {before:,} -> {after:,} bytes "
          f"({len(w.groups)} farm-group entries)")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except Unhandled as err:
        sys.exit(f"symdb_rewrite: {err}")
