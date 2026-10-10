#!/usr/bin/env python3
"""The symbol database (symbols/db/*.yaml, format in symbols/db/README.md).

  tools/symdb.py check [--staged] [-v]   validate every entry against the disassembly; check stamps
  tools/symdb.py render                  write symbols/SYMBOLS.md (index) and symbols/symdb.sqlite
  tools/symdb.py show NAME               print one entry (current or previous name)
  tools/symdb.py stamps [--json] [NAME...]                     what each stamp check found, per function
  tools/symdb.py stamp (NAME... | --all | --unstamped | --changed) [--model M --run R --prompt P]
                                                               (re)write stamps (tools/stamps.py)

check: required fields; the name exists in bn6f/bn6f.elf at `address`; previous names no longer
exist in the disassembly; readable_c files exist; status/kind/method values are known; a stamp
has its fields. Problems go to stdout ("FILE: problem", then "N entries, M problems") and fail
the check. Warnings go to stderr and do not: paths in `reproduce:` lines that do not exist, and
the stamp report (tools/stamps.py: what changed since each readable function was stamped and
what to do about it). --staged checks the index (what a commit would contain) instead of the
working tree.

Parsed entries, the ELF's symbols and the stamp inputs are cached in build/symdb/ by content
(git blob ids) or file stamp, so a check takes about a second.
"""
from __future__ import annotations

import json
import os
import re
import sqlite3
import subprocess
import sys
from collections.abc import Iterable
from dataclasses import dataclass
from typing import Any

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stamps  # noqa: E402
from stamps import ROOT, Cache, Tree  # noqa: E402

DB = os.path.join(ROOT, "symbols", "db")
DB_DIR = "symbols/db"
ELF = os.path.join(ROOT, stamps.ELF)
REQUIRED = ("name", "address", "kind", "status", "summary", "evidence")
KINDS = {"function", "data", "ram", "constant", "struct-field"}
STATUS = {"confirmed", "probable", "hypothesis"}
METHODS = {"assembly-reading", "caller-analysis", "cpu-oracle", "difftest", "lockstep", "coverage", "readable-c",
           "library-match", "data-inspection", "hardware"}
STAMP_REQUIRED = ("rom", "range", "contract", "headers", "lifter", "prompt", "model", "run", "body", "installed")


# ── loading: every entry, parsed once per content ──

SLIM = ("name", "address", "kind", "status", "previous_names", "readable_c", "stamp")


def _parse(texts: list[str]) -> list[Any]:
    import yaml
    return [yaml.load(t, Loader=yaml.CSafeLoader) for t in texts]


def slim(e: Any) -> dict[str, Any]:
    """What check and the stamp check read of an entry (the cache keeps only this)."""
    if not isinstance(e, dict):
        return {"_unparsed": True}
    out = {k: e[k] for k in SLIM if k in e}
    out["_has"] = [k for k in REQUIRED if k in e]
    out["_evidence"] = [(ev.get("method"), bool(ev.get("detail"))) if isinstance(ev, dict) else None
                        for ev in (e.get("evidence") or [])]
    out["_reproduce_paths"] = reproduce_paths(e)
    return out


def load_full(paths: list[str] | None = None) -> list[dict[str, Any]]:
    """Whole entries (render, show, stamp): parsed from the files, not cached."""
    tree = Tree()
    paths = paths if paths is not None else tree.listdir(DB_DIR, ".yaml")
    out = []
    for p, e in zip(paths, _parse([(tree.read(p) or b"").decode("utf-8") for p in paths])):
        e = e if isinstance(e, dict) else {"_unparsed": True}
        e["_file"] = p
        out.append(e)
    return out


def load(tree: Tree | None = None, cache: Cache | None = None) -> list[dict[str, Any]]:
    """Every entry's slim record (with `_file`, the repo-relative path), from the working tree or the
    index, parsed once per content."""
    cache = cache or (tree.cache if tree else None) or Cache()
    tree = tree or Tree(cache=cache)
    paths = tree.listdir(DB_DIR, ".yaml")
    blobs = {p: _blob(tree, cache, p) for p in paths}
    table = cache.data.setdefault("yaml", {})
    missing = [p for p in paths if blobs[p] not in table]
    if missing:
        tree.prefetch(missing)
        texts = [(tree.read(p) or b"").decode("utf-8") for p in missing]
        if len(texts) > 200:
            from concurrent.futures import ProcessPoolExecutor
            chunks = [texts[i::8] for i in range(8)]
            with ProcessPoolExecutor(8) as pool:
                parts = list(pool.map(_parse, chunks))
            parsed = [None] * len(texts)
            for i, part in enumerate(parts):
                parsed[i::8] = part
        else:
            parsed = _parse(texts)
        for p, e in zip(missing, parsed):
            table[blobs[p]] = slim(e)
        cache.dirty = True
    out = []
    for p in paths:
        e = dict(cache.get("yaml", blobs[p], lambda: {"_unparsed": True}))
        e["_file"] = p
        out.append(e)
    return out


def _blob(tree: Tree, cache: Cache, path: str) -> str:
    return tree.blob(path) or ""


def entries():
    """Every entry, whole, as the other tools read them (symdoc.py, pool.py)."""
    return load_full()


def elf_symbols(cache: Cache | None = None, wanted: Iterable[str] | None = None) -> dict[str, int]:
    """bn6f.elf's symbols (name -> address). With a cache: only the WANTED names, remembered per ELF
    build, so nm (half a second) runs only when the ELF or the set of names changes."""
    def read() -> dict[str, int]:
        nm = subprocess.run(["arm-none-eabi-nm", ELF], capture_output=True, text=True, check=True).stdout
        syms: dict[str, int] = {}
        for line in nm.splitlines():
            f = line.split()
            if len(f) == 3:
                syms.setdefault(f[2], int(f[0], 16) & ~1)
        return syms
    if cache is None or wanted is None:
        return read()
    known: dict[str, int | None] = cache.get("nm", stamps.file_key(stamps.ELF), dict)
    wanted = set(wanted)
    if not wanted <= known.keys():
        full = read()
        known.update({n: full.get(n) for n in wanted})
        cache.dirty = True
    return {n: a for n in wanted if (a := known[n]) is not None}


# ── reproduce lines: the paths they name must exist ──

WORD = re.compile(r"\"[^\"]*\"|'[^']*'|&&|\|\||[;|()&\n]|[^\s;&|()]+")
PATHLIKE = re.compile(r"^(?:\.{1,2}/|/)?[\w.+-]+(?:/[\w.+*?\[\]{}-]*)+$")
GENERATED = re.compile(r"(?:^|/)(?:build[\w-]*|\.venv|out|__pycache__)(?:/|$)")


def reproduce_paths(e: dict[str, Any]) -> list[tuple[str, str]]:
    """(directory, path) for each path-like word in the entry's reproduce lines, following `cd`."""
    out: list[tuple[str, str]] = []
    for ev in e.get("evidence") or []:
        text = ev.get("reproduce") if isinstance(ev, dict) else None
        if not isinstance(text, str):
            continue
        cwd, stack, after_cd = "", [], False
        for w in WORD.findall(text):
            if w == "(":
                stack.append(cwd)
            elif w == ")":
                cwd = stack.pop() if stack else ""
            elif w == "cd":
                after_cd = True
            elif w in ("&&", "||", ";", "|", "&", "\n"):
                after_cd = False
            elif w[0] in "\"'" or "$" in w or "<" in w or ">" in w or ":" in w.split("=")[-1][:6]:
                after_cd = False
            else:
                word = w.split("=", 1)[1] if "=" in w and not w.startswith("-") else w
                word = word.split("=", 1)[1] if word.startswith("--") and "=" in word else word
                if after_cd:
                    cwd = os.path.normpath(os.path.join(cwd, word)) if not word.startswith("/") else word
                    after_cd = False
                elif PATHLIKE.match(word) and not word.startswith("//"):
                    out.append((cwd, word))     # model ids (hyper/glm-5.3-flash) are dropped later
    return out


def missing_paths(es: list[dict[str, Any]]) -> dict[str, list[str]]:
    """Per entry file, the reproduce paths that exist nowhere (generated build output is not counted)."""
    import glob as globmod
    missing: dict[tuple[str, str], bool] = {}
    out: dict[str, list[str]] = {}
    for e in es:
        for cwd, p in e.get("_reproduce_paths", []):
            if (cwd, p) not in missing:
                missing[(cwd, p)] = not _exists(cwd, p, globmod.glob)
            if missing[(cwd, p)]:
                out.setdefault(e["_file"], []).append(p)
    return out


def _exists(cwd: str, p: str, glob: Any) -> bool:
    """Whether a reproduce path exists; generated build output and words that are not paths here
    (a model id such as hyper/glm-5.3-flash) count as existing."""
    full = p if p.startswith("/") else os.path.normpath(os.path.join(ROOT, cwd, p))
    if not full.startswith("/tmp/") and GENERATED.search(full[len(ROOT):] if full.startswith(ROOT) else full):
        return True
    first = p.split("/")[0]
    if first not in ("", ".", "..") and not os.path.isdir(os.path.join(ROOT, cwd, first)):
        return True
    return bool(glob(full)) if any(c in full for c in "*?[") else os.path.exists(full)


# ── check ──

@dataclass
class Problem:
    file: str
    message: str
    elf: bool = False         # depends on bn6f.elf (renames in flight make these temporary)


def problems(es: list[dict[str, Any]], syms: dict[str, int], tree: Tree) -> list[Problem]:
    out: list[Problem] = []
    names: dict[str, str] = {}
    for e in es:
        where = e["_file"]

        def err(msg: str, elf: bool = False) -> None:
            out.append(Problem(where, msg, elf))
        if e.get("_unparsed"):
            err("not a YAML mapping")
            continue
        for k in REQUIRED:
            if k not in e["_has"]:
                err(f"missing {k}")
        if os.path.basename(where) != f"{e.get('name')}.yaml":
            err("file name is not NAME.yaml")
        if e.get("name") in names:
            err(f"duplicate of {names[e['name']]}")
        names[e.get("name")] = where
        if e.get("kind") not in KINDS:
            err(f"unknown kind {e.get('kind')}")
        if e.get("status") not in STATUS:
            err(f"unknown status {e.get('status')}")
        addr = e.get("address")
        try:
            addr = int(addr, 16) if isinstance(addr, str) else addr
        except ValueError:
            err(f"address {addr!r} is not a number")
            addr = None
        if e.get("kind") != "struct-field":
            if e.get("name") not in syms:
                err(f"{e.get('name')} is not a symbol of bn6f.elf", elf=True)
            elif addr is not None and syms[e["name"]] != (addr & ~1):
                err(f"address {addr:#x} but bn6f.elf has {syms[e['name']]:#x}", elf=True)
        for old in e.get("previous_names") or []:
            if old in syms:
                err(f"previous name {old} still exists in bn6f.elf", elf=True)
        for ev in e["_evidence"]:
            if ev is None:
                err("evidence item is not a mapping")
                continue
            method, has_detail = ev
            if method not in METHODS:
                err(f"unknown evidence method {method}")
            if not has_detail:
                err("evidence without detail")
        rc = e.get("readable_c")
        if rc and not tree.exists(rc):
            err(f"readable_c {rc} does not exist")
        st = e.get("stamp")
        if st is not None:
            if not isinstance(st, dict):
                err("stamp is not a mapping")
            else:
                missing = [k for k in STAMP_REQUIRED if not st.get(k)]
                if missing:
                    err(f"stamp lacks {', '.join(missing)}")
                if not isinstance(st.get("callees", {}), dict):
                    err("stamp callees is not a mapping")
                if st.get("contract") not in (None, stamps.UNKNOWN) and stamps.Contract.parse(str(st["contract"])) is None:
                    err(f"stamp contract {st['contract']!r} is not 'INPUTS -> OUTPUTS; clobbers REGS'")
    return out


@dataclass
class Report:
    entries: int
    problems: list[Problem]
    missing_paths: dict[str, list[str]]
    findings: list[stamps.Finding]
    counts: dict[str, int]
    notes: list[str]


def run_check(tree: Tree | None = None, with_stamps: bool = True) -> Report:
    cache = (tree.cache if tree else None) or Cache()
    tree = tree or Tree(cache=cache)
    tree.cache = cache
    es = load(tree, cache)
    syms = elf_symbols(cache, {n for e in es for n in [e.get("name"), *(e.get("previous_names") or [])]
                               if isinstance(n, str)})
    found = problems(es, syms, tree)
    missing = missing_paths(es)
    findings: list[stamps.Finding] = []
    counts = {"readable": 0, "stamped": 0}
    notes: list[str] = []
    if with_stamps:
        world = stamps.World(tree, es, cache)
        findings, counts = stamps.check_stamps(world, es)
        notes = world.notes
    cache.save()
    return Report(len(es), found, missing, findings, counts, notes)


def check(argv: list[str] | None = None) -> bool:
    argv = argv or []
    verbose = "-v" in argv or "--verbose" in argv
    rep = run_check(Tree(staged="--staged" in argv, cache=Cache()))
    for p in rep.problems:
        print(f"{p.file}: {p.message}")
    print_warnings(rep, verbose)
    print(f"{rep.entries} entries, {len(rep.problems)} problems")
    return not rep.problems


def print_warnings(rep: Report, verbose: bool) -> None:
    """On stderr: the record workflow reads stdout's "FILE: problem" lines as problems."""
    if rep.missing_paths:
        n = sum(len(v) for v in rep.missing_paths.values())
        tmp = sum(1 for v in rep.missing_paths.values() for p in v if p.startswith("/tmp/"))
        print(f"warning: {len(rep.missing_paths)} entries have reproduce lines naming {n} paths that do not exist "
              f"({tmp} under /tmp); tools/symdb.py check -v lists them", file=sys.stderr)
        if verbose:
            for f, paths in sorted(rep.missing_paths.items()):
                print(f"  {f}: {' '.join(sorted(set(paths)))}", file=sys.stderr)
    for line in stamps.summary(rep.findings, rep.counts, rep.notes):
        print(line, file=sys.stderr)
    if verbose:
        for f in rep.findings:
            print(f"  {f.function}: {f.action.value}: {f.why}", file=sys.stderr)


# ── stamps ──

def stamps_report(argv: list[str]) -> bool:
    names = [a for a in argv if not a.startswith("-")]
    rep = run_check(Tree())
    findings = [f for f in rep.findings if not names or f.function in names or f.entry in names]
    if "--json" in argv:
        json.dump([{"function": f.function, "entry": f.entry, "action": f.action.value, "why": f.why}
                   for f in findings], sys.stdout, indent=1)
        print()
    else:
        for f in sorted(findings, key=lambda f: (stamps.ORDER.index(f.action), f.function)):
            print(f"{f.function}: {f.action.value}: {f.why}")
        for line in stamps.summary(findings, rep.counts, rep.notes):
            print(line)
    return True


def stamp(argv: list[str]) -> bool:
    """Write stamps. NAME...: these functions. --all: every readable function. --unstamped: those
    without a stamp. --changed: those whose stamp check asks for a re-stamp (hand edits).
    Facts come from HEAD: commit the bodies first, then stamp them (and commit the entries).
    --model/--run/--prompt/--installed say who made the bodies (an install); without them the
    existing stamp is kept where the body is unchanged, an edited body is stamped as a hand edit,
    and a first stamp takes model and run from the entry, symbols/pool.tsv and git history
    (prompt "legacy"), "unknown" where none of them tells. Writes only the `stamp:` block."""
    opts: dict[str, str] = {}
    names: list[str] = []
    it = iter(argv)
    for a in it:
        if a in ("--model", "--run", "--prompt", "--installed"):
            opts[a[2:]] = next(it)
        elif not a.startswith("-"):
            names.append(a)
    cache = Cache()
    tree = Tree(cache=cache, commit="HEAD")      # a stamp records committed bodies (and its install commit)
    es = load(tree, cache)
    world = stamps.World(tree, es, cache)
    if "--all" in argv:
        names = sorted(world.readable)
    elif "--unstamped" in argv or "--changed" in argv:
        findings, _ = stamps.check_stamps(world, es)
        acts = {stamps.Action.STAMP} if "--unstamped" in argv else {stamps.Action.HAND_EDIT}
        names = sorted({f.function for f in findings if f.action in acts})
    unknown = [n for n in names if n not in world.readable]
    if unknown:
        sys.exit(f"not readable functions (no `Next NAME(void)` in cport/src/game): {' '.join(unknown)}")
    if not names:
        print("nothing to stamp")
        return True
    if opts and "prompt" not in opts:
        sys.exit("--model/--run need --prompt too (the prompt hash, or 'none' for a hand edit)")
    full = {e["_file"]: e for e in load_full()}
    written = stamps.write_stamps(world, full, names, opts, dry_run="--dry-run" in argv)
    cache.save()
    print(f"stamped {len(names)} functions; {len(written)} entries changed")
    return True


# ── render and show ──

def render():
    es = entries()
    lines = ["# Symbols documented by this project", "",
             "Generated by `tools/symdb.py render` from `symbols/db/*.yaml` (format and status levels:",
             "`symbols/db/README.md`). Each entry records the evidence and how to reproduce it.", "",
             "| Name | Address | Kind | Status | Previously | Summary | Readable C |", "|---|---|---|---|---|---|---|"]
    num = lambda a: int(a, 16) if isinstance(a, str) else (a or 0)
    for e in sorted(es, key=lambda e: num(e.get("address"))):
        prev = ", ".join(e.get("previous_names") or []) or "—"
        rc = f"`{e['readable_c']}`" if e.get("readable_c") else "—"
        lines.append(f"| [`{e['name']}`](db/{e['name']}.yaml) | `{num(e.get('address')):#010x}` | {e['kind']} | "
                     f"{e['status']} | {prev} | {e['summary']} | {rc} |")
    open(os.path.join(ROOT, "symbols", "SYMBOLS.md"), "w").write("\n".join(lines) + "\n")
    dbp = os.path.join(ROOT, "symbols", "symdb.sqlite")
    if os.path.exists(dbp):
        os.remove(dbp)
    con = sqlite3.connect(dbp)
    con.execute("create table symbols (name text primary key, address integer, kind text, status text, "
                "summary text, interface text, details text, readable_c text, previous_names text, "
                "coverage text, related text, open_questions text, file text)")
    con.execute("create table evidence (name text, method text, date text, detail text, refs text, "
                "reproduce text, result text)")
    con.execute("create table changes (name text, date text, what text, where_ text, tool text)")
    for e in es:
        con.execute("insert into symbols values (?,?,?,?,?,?,?,?,?,?,?,?,?)",
                    (e["name"], num(e.get("address")), e["kind"], e["status"], e["summary"], e.get("interface"),
                     e.get("details"), e.get("readable_c"), json.dumps(e.get("previous_names") or []),
                     json.dumps(e.get("coverage") or []), json.dumps(e.get("related") or []),
                     json.dumps(e.get("open_questions") or []), e["_file"]))
        for ev in e.get("evidence") or []:
            con.execute("insert into evidence values (?,?,?,?,?,?,?)",
                        (e["name"], ev.get("method"), str(ev.get("date", "")), ev.get("detail"),
                         json.dumps(ev.get("refs") or []), ev.get("reproduce"), ev.get("result")))
        for ch in e.get("changes") or []:
            con.execute("insert into changes values (?,?,?,?,?)",
                        (e["name"], str(ch.get("date", "")), ch.get("what"), ch.get("where"), ch.get("tool")))
    con.commit()
    print(f"rendered {len(es)} entries: symbols/SYMBOLS.md, symbols/symdb.sqlite")


def show(name):
    for e in entries():
        if e["name"] == name or name in (e.get("previous_names") or []):
            print(open(os.path.join(ROOT, e["_file"])).read())
            return True
    print(f"{name}: not in the database")
    return False


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    rest = sys.argv[2:]
    if cmd == "check":
        ok = check(rest)
    elif cmd == "render":
        ok = render()
    elif cmd == "show" and rest:
        ok = show(rest[0])
    elif cmd == "stamps":
        ok = stamps_report(rest)
    elif cmd == "stamp":
        ok = stamp(rest)
    else:
        sys.exit(__doc__)
    sys.exit(0 if ok in (True, None) else 1)
