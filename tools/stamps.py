#!/usr/bin/env python3
"""Stamps: what each readable function was made from, and what to do when that changes.

A readable function is a `Next NAME(void)` body in cport/src/game that replaces the lifted one.
Its symbols/db entry carries one `stamp:` (format: symbols/db/README.md); `tools/symdb.py check`
compares every stamp with the tree and names the action for each change (joint plan S1, "The
stamp and its check", docs/reviews/2026-10-10-4-joint-plan.md). Nothing here calls a model or
runs lockstep: it reads files and git.

  tools/symdb.py stamps [--json] [NAME...]             the per-function report
  tools/symdb.py stamp NAME... | --all | --unstamped   (re)write stamps; see stamp_main()

Names in bodies, contracts and callee lists change with every rename, so everything is compared
under each symbol's first name (its root in symbols/db `previous_names`); a rename alone changes
no hash.
"""
from __future__ import annotations

import ast
import dataclasses
import datetime
import hashlib
import json
import os
import pickle
import re
import subprocess
import struct
import sys
import tempfile
from collections.abc import Callable, Iterable, Iterator
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, NamedTuple, NewType

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = "cport/src/game"
INCLUDE = "cport/include"
LIFTED = "cport/src/lifted"
ROM = "bn6f/bn6f.gba"
LIFT_PY = "tools/lifter/lift.py"
LIFTER = "tools/lifter"
POOL = "symbols/pool.tsv"
CACHE = os.path.join(ROOT, "build", "symdb", "cache.pickle")
CACHE_VERSION = 3

Name = NewType("Name", str)          # a symbol name as it is today
Root = NewType("Root", str)          # a symbol's first name (stable across renames)
BlobId = NewType("BlobId", str)      # git's object id of a file's content


# ── where tracked files come from: the working tree, or the index a commit is made from ──

def blob_id(data: bytes) -> BlobId:
    return BlobId(hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest())


def git(*args: str, cwd: str = ROOT, check: bool = True, env: dict[str, str] | None = None) -> str:
    done = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True, check=False, env=env)
    if check and done.returncode != 0:
        raise RuntimeError(f"git {' '.join(args[:2])} failed: {done.stderr.strip()}")
    return done.stdout


TRACKED = ("symbols/db", GAME, INCLUDE, LIFTER, "tools/abi/lint_baseline.json")


class Tree:
    """Tracked files as the check should see them. The working tree by default; with staged=True the
    index being committed (the commit gate), or with commit=REV that commit, so other people's
    unstaged edits never count. Untracked files (lifted output, the ROM) always come from disk."""

    def __init__(self, staged: bool = False, cache: Cache | None = None, commit: str | None = None) -> None:
        self.staged = staged or commit is not None
        self.cache = cache
        self._index: dict[str, BlobId] = {}
        self._data: dict[str, bytes] = {}
        if commit is not None:
            for rec in git("ls-tree", "-r", "-z", commit, "--", *TRACKED).split("\0"):
                if rec:
                    meta, path = rec.split("\t", 1)
                    self._index[path] = BlobId(meta.split()[2])
        elif staged:
            for rec in git("ls-files", "-s", "-z", "--", *TRACKED).split("\0"):
                if rec:
                    meta, path = rec.split("\t", 1)
                    self._index[path] = BlobId(meta.split()[1])

    def listdir(self, directory: str, suffix: str) -> list[str]:
        """Repo-relative paths of the files directly in DIRECTORY ending in SUFFIX."""
        if self.staged:
            return sorted(p for p in self._index if os.path.dirname(p) == directory and p.endswith(suffix))
        full = os.path.join(ROOT, directory)
        if not os.path.isdir(full):
            return []
        return sorted(f"{directory}/{f}" for f in os.listdir(full) if f.endswith(suffix))

    def blob(self, path: str) -> BlobId | None:
        if self.staged and path in self._index:
            return self._index[path]
        if self.cache is not None and not self.staged:
            try:
                st = os.stat(os.path.join(ROOT, path))
            except FileNotFoundError:
                return None
            key = (path, st.st_mtime_ns, st.st_size, st.st_ino)   # unchanged files are not read again
            return self.cache.get("stat", key, lambda: blob_id(self.read(path) or b""))
        data = self.read(path)
        return None if data is None else blob_id(data)

    def exists(self, path: str) -> bool:
        if self.staged and path in self._index:
            return True
        return os.path.exists(os.path.join(ROOT, path))

    def read(self, path: str) -> bytes | None:
        if path in self._data:
            return self._data[path]
        if self.staged and path in self._index:
            self.prefetch([path])
            return self._data[path]
        try:
            with open(os.path.join(ROOT, path), "rb") as fh:
                data = fh.read()
        except FileNotFoundError:
            return None
        self._data[path] = data
        return data

    def prefetch(self, paths: Iterable[str]) -> None:
        """Read many staged files with one `git cat-file --batch`."""
        want = [p for p in paths if p not in self._data and p in self._index]
        if not want:
            return
        done = subprocess.run(["git", "cat-file", "--batch"], cwd=ROOT, check=True, capture_output=True,
                              input="".join(f"{self._index[p]}\n" for p in want).encode())
        out, pos = done.stdout, 0
        for p in want:
            eol = out.index(b"\n", pos)
            size = int(out[pos:eol].split()[2])
            self._data[p] = out[eol + 1: eol + 1 + size]
            pos = eol + 1 + size + 1


# ── a small cache (build/symdb): parsed files by content, lifted headers by file stamp ──

class Cache:
    def __init__(self, path: str = CACHE) -> None:
        self.path = path
        self.data: dict[str, dict[Any, Any]] = {}
        self.used: dict[str, set[Any]] = {}
        self.dirty = False
        try:
            with open(path, "rb") as fh:
                data = pickle.load(fh)
            if data.get("version") == CACHE_VERSION:
                self.data = data["tables"]
        except (OSError, EOFError, pickle.UnpicklingError, AttributeError, KeyError, TypeError, ImportError):
            self.data = {}

    def get(self, table: str, key: Any, make: Callable[[], Any]) -> Any:
        t = self.data.setdefault(table, {})
        self.used.setdefault(table, set()).add(key)
        if key not in t:
            t[key] = make()
            self.dirty = True
        return t[key]

    def save(self) -> None:
        """Keep what this run used (and any table it did not touch), written atomically."""
        if not self.dirty:
            return
        tables = {name: ({k: v for k, v in t.items() if k in self.used[name]} if name in self.used else t)
                  for name, t in self.data.items()}
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        fd, tmp = tempfile.mkstemp(dir=os.path.dirname(self.path), prefix=".cache-")
        with os.fdopen(fd, "wb") as fh:
            pickle.dump({"version": CACHE_VERSION, "tables": tables}, fh, protocol=pickle.HIGHEST_PROTOCOL)
        os.replace(tmp, self.path)


def file_key(path: str) -> tuple[str, int, int] | None:
    try:
        st = os.stat(os.path.join(ROOT, path))
    except FileNotFoundError:
        return None
    return (path, st.st_mtime_ns, st.st_size)


# ── C at file scope: which definitions exist and which names each one uses ──

IDENT = re.compile(r"[A-Za-z_]\w*")
COMMENT_OR_STRING = re.compile(r"/\*.*?\*/|//[^\n]*|\"(?:[^\"\\\n]|\\.)*\"|'(?:[^'\\\n]|\\.)*'", re.S)
TOKEN = re.compile(r"^[ \t]*#(?:[^\n\\]|\\.)*|[{}()\[\];=,]|[A-Za-z_]\w*", re.M | re.S)
NEXT_HEADER = re.compile(r"(?:^|\s)Next\s+(\w+)\s*\(\s*void\s*\)\s*$")
DEFINE = re.compile(r"[ \t]*#\s*define\s+(\w+)")
TYPE_WORDS = {"struct", "union", "enum", "typedef"}
LINKED = {"function", "data"}      # names other files can see (non-static); macros, enumerators, types are per file
NOT_NAMES = {"__attribute__", "sizeof", "static", "const", "volatile", "inline", "extern", "unsigned", "signed"}


class Item(NamedTuple):
    """One file-scope definition: a function, a data object, a macro, one enumerator, a type.
    (Tuples: thousands of them are cached and must unpickle fast.)"""
    kind: str                        # function | data | macro | enumerator | type | proto | pp
    names: tuple[str, ...]
    text: str                        # comments removed, whitespace collapsed
    idents: frozenset[str]
    static: bool = False
    next: bool = False               # a `Next NAME(void)` readable function


def blank(text: str) -> tuple[str, str]:
    """(code with comments and string contents blanked, code with only comments removed), same offsets."""
    def code_of(m: re.Match[str]) -> str:
        s = m.group(0)
        if s[0] in "\"'":
            return s[0] + re.sub(r"[^\n]", " ", s[1:-1]) + s[-1]
        return re.sub(r"[^\n]", " ", s)

    def nocomment_of(m: re.Match[str]) -> str:
        s = m.group(0)
        return s if s[0] in "\"'" else re.sub(r"[^\n]", " ", s)
    return COMMENT_OR_STRING.sub(code_of, text), COMMENT_OR_STRING.sub(nocomment_of, text)


def squash(text: str) -> str:
    return " ".join(text.split())


def parse_c(text: str) -> list[Item]:
    """The file-scope items of a C file or header (a lexer-level split, enough for hashing and
    finding what a function uses; not a C parser)."""
    code, plain = blank(text)
    items: list[Item] = []
    start, depth, pdepth = 0, 0, 0
    toks: list[tuple[str, int]] = []          # this item's tokens at depth 0
    idents: set[str] = set()

    def emit(end: int) -> None:
        nonlocal start, toks, idents
        body = plain[start:end]
        if body.strip():
            items.extend(classify(toks, body, code[start:end], frozenset(idents)))
        start, toks, idents = end, [], set()

    for m in TOKEN.finditer(code):
        tok = m.group(0)
        if tok.lstrip().startswith("#"):
            if depth == 0 and pdepth == 0 and not toks:
                emit(m.start())
                d = DEFINE.match(tok)
                ids = set(IDENT.findall(tok))
                text = squash(plain[m.start():m.end()])
                if d:
                    items.append(Item("macro", (d.group(1),), text, frozenset(ids - {"define", d.group(1)})))
                else:
                    items.append(Item("pp", (), text, frozenset()))
                start = m.end()
            continue
        if tok[0].isalpha() or tok[0] == "_":
            idents.add(tok)
            if depth == 0 and pdepth == 0:
                toks.append((tok, m.start()))
            continue
        if tok in "([":
            if depth == 0 and pdepth == 0:
                toks.append((tok, m.start()))
            pdepth += 1
        elif tok in ")]":
            pdepth = max(0, pdepth - 1)
        elif tok == "{":
            if depth == 0 and pdepth == 0:
                toks.append((tok, m.start()))
            depth += 1
        elif tok == "}":
            depth = max(0, depth - 1)
            if depth == 0 and pdepth == 0 and is_function(toks):
                emit(m.end())
        elif depth == 0 and pdepth == 0:
            toks.append((tok, m.start()))
            if tok == ";":
                emit(m.end())
    emit(len(code))
    return items


def is_function(toks: list[tuple[str, int]]) -> bool:
    """A function definition: NAME ( ... ) { at file scope, with no `=` (an initializer) and not a
    struct/union/enum/typedef definition."""
    words = [t for t, _ in toks]
    if "{" not in words:
        return False
    head = words[: words.index("{")]
    if "(" not in head or "=" in head or head[0] == "typedef":
        return False
    return not (head[0] in TYPE_WORDS and head[-1] != ")")


def classify(toks: list[tuple[str, int]], plain: str, code: str, idents: frozenset[str]) -> list[Item]:
    words = [t for t, _ in toks]
    static = "static" in words
    text = squash(plain)
    if is_function(toks):
        head = words[: words.index("{")]
        name = function_name(head)
        nx = NEXT_HEADER.search(squash(code[: code.index("{")]))
        return [Item("function", (name,) if name else (), text, idents - {name}, static, bool(nx))]
    brace = words.index("{") if "{" in words else -1
    pure_type = words and words[0] in TYPE_WORDS and 0 < brace <= 2   # struct X { ... } [var];
    if words and (words[0] == "typedef" or pure_type):
        if "enum" in words[:2] and brace >= 0:
            return enum_items(code, plain, words, idents)
        names = tuple(w for i, w in enumerate(words) if IDENT.fullmatch(w) and w not in TYPE_WORDS
                      and w not in NOT_NAMES and i + 1 < len(words) and words[i + 1] in ("{", ";", ","))
        return [Item("type", names, text, idents - set(names), static)]
    cut = next((i for i, w in enumerate(words) if w in ("=", "[", ";", ",", "{")), len(words))
    if "(" in words[:cut] and "=" not in words:
        name = function_name(words[:cut])
        return [Item("proto", (name,) if name else (), text, idents, static)]
    names = tuple(w for w in words[:cut] if IDENT.fullmatch(w))[-1:]
    return [Item("data", names, text, idents - set(names), static)]


def function_name(head: list[str]) -> str | None:
    """The identifier before the parameter list (skipping __attribute__((...)))."""
    for i, w in enumerate(head):
        if w == "(" and i > 0 and IDENT.fullmatch(head[i - 1]) and head[i - 1] not in NOT_NAMES:
            return head[i - 1]
    return None


def enum_items(code: str, plain: str, words: list[str], idents: frozenset[str]) -> list[Item]:
    """An enum is one item per enumerator (so adding one enumerator changes no other user), plus the
    type's own names."""
    lo, hi = code.index("{"), code.rindex("}")
    out: list[Item] = []
    index, depth, piece_start = 0, 0, lo + 1
    pieces: list[tuple[int, int]] = []
    for i in range(lo + 1, hi):
        c = code[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "," and depth == 0:
            pieces.append((piece_start, i))
            piece_start = i + 1
    pieces.append((piece_start, hi))
    for a, b in pieces:
        piece = code[a:b]
        m = IDENT.search(piece)
        if not m:
            continue
        text = squash(plain[a:b])
        if "=" not in piece:
            text = f"{text} @{index}"     # an implicit value is its position
        out.append(Item("enumerator", (m.group(0),), text, frozenset(IDENT.findall(piece)) - {m.group(0)}))
        index += 1
    tail = [w for w in words[words.index("{"):] if IDENT.fullmatch(w)]
    tag = [w for w in words[: words.index("{")] if IDENT.fullmatch(w) and w not in TYPE_WORDS]
    if tail or tag:
        out.append(Item("type", tuple(tag + tail), "enum " + " ".join(tag + tail), frozenset()))
    return out


# ── the db's names: every name of a symbol maps to its root, so renames hash alike ──

@dataclass
class Names:
    root: dict[str, Root]                  # any current or previous name -> first name
    current: dict[Root, Name]              # root -> today's name

    @classmethod
    def of(cls, entries: Iterable[dict[str, Any]]) -> Names:
        root: dict[str, Root] = {}
        current: dict[Root, Name] = {}
        for e in entries:
            name = e.get("name")
            if not isinstance(name, str):
                continue
            prev = [p for p in (e.get("previous_names") or []) if isinstance(p, str)]
            r = Root(prev[0] if prev else name)
            for n in (name, *prev):
                root[n] = r
            current[r] = Name(name)
        return cls(root, current)

    def canon(self, ident: str) -> str:
        if ident in self.root:
            return self.root[ident]
        for prefix in ("A_", "L_"):
            if ident.startswith(prefix) and ident[2:] in self.root:
                return prefix + self.root[ident[2:]]
        return ident

    def canon_text(self, text: str) -> str:
        return IDENT.sub(lambda m: self.canon(m.group(0)), text)

    def fingerprint(self) -> str:
        moved = sorted((k, v) for k, v in self.root.items() if k != v)
        return hashlib.sha1(json.dumps(moved).encode()).hexdigest()


# ── the facts a stamp records, as found in the tree now ──

class Contract(NamedTuple):
    """The lifter's register contract (the lifted header): what a function reads and leaves."""
    inputs: tuple[str, ...]
    outputs: tuple[str, ...]
    clobbers: tuple[str, ...]

    def text(self) -> str:
        return f"{' '.join(self.inputs) or '-'} -> {' '.join(self.outputs) or '-'}; clobbers {' '.join(self.clobbers) or '-'}"

    @classmethod
    def parse(cls, text: str) -> Contract | None:
        m = re.fullmatch(r"\s*(.*?)\s*->\s*(.*?)\s*;\s*clobbers\s*(.*?)\s*", text or "")
        if not m:
            return None
        regs = [tuple(x for x in g.split() if x != "-") for g in m.groups()]
        return cls(regs[0], regs[1], regs[2])

    def checked(self) -> set[str]:
        """What the difftest compares (farm.compare_args): every r0-r12 that is not free, and output flags."""
        regs = {f"r{i}" for i in range(13)} - set(self.clobbers)
        return regs | {f for f in ("nz", "c", "v") if f in self.outputs}


class Header(NamedTuple):
    lo: int
    hi: int
    contract: Contract
    asm: str | None                        # "asm00_2.s:613"


HEADER_RE = re.compile(r"^/\* (\w+): ([0-9A-F]{8})-([0-9A-F]{8}) (?:THUMB|ARM)(?:, bn6f ([\w.]+:\d+))? \*/\n"
                       r"/\*   inputs:  ([^\n]*)\n \*   outputs: ([^\n]*)\n \*   clobbers[^:]*: ([^\n]*)\n"
                       r" \*   preserved: [^\n]*? \*/", re.M)
BRANCH_RE = re.compile(r"/\* [0-9A-F]{8}: bl? (\w+)")


def regs(text: str) -> tuple[str, ...]:
    return tuple(x for x in text.split() if x != "-")


def parse_lifted(text: str, with_callees: bool) -> tuple[dict[str, Header], dict[str, tuple[str, ...]]]:
    headers: dict[str, Header] = {}
    targets: dict[str, tuple[str, ...]] = {}
    matches = list(HEADER_RE.finditer(text))
    for i, m in enumerate(matches):
        name = m.group(1)
        headers[name] = Header(int(m.group(2), 16), int(m.group(3), 16),
                               Contract(regs(m.group(5)), regs(m.group(6)), regs(m.group(7))), m.group(4))
        if with_callees:
            end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
            targets[name] = tuple(dict.fromkeys(BRANCH_RE.findall(text, m.end(), end)))
    return headers, targets


@dataclass(frozen=True)
class Facts:
    function: str
    range: str | None
    rom: str | None
    contract: Contract | None
    callees: dict[str, str] = field(default_factory=dict, hash=False, compare=False)   # name -> outputs
    headers: str | None = None
    lifter: str | None = None
    body: str | None = None


@dataclass
class Readable:
    """A readable function found in cport/src/game."""
    name: str
    file: str
    body_items: list[Item]
    header_items: list[Item]


class World:
    """Everything the stamp check reads, loaded once (cached in build/symdb between runs)."""

    def __init__(self, tree: Tree, entries: list[dict[str, Any]], cache: Cache | None = None) -> None:
        self.tree = tree
        self.cache = cache or tree.cache or Cache()
        self._canon: dict[int, tuple[Item, str]] = {}   # the item is kept so its id stays its own
        self._deps: dict[tuple[int, int], list[tuple[Item, bool]]] = {}
        self._empty: dict[str, list[Item]] = {}
        self.names = Names.of(entries)
        self.notes: list[str] = []                 # why some facts could not be found
        self.files = self._game_files()
        self.header_index = self._headers()
        self.readable = self._readable()
        self.lifted, self.lifted_targets = self._lifted()
        self.copies = self._copies()
        self._rom: bytes | None = None
        self.lifter = self._lifter_version()

    # game files and headers

    def _parsed(self, path: str, named_only: bool = False) -> list[Item]:
        """A file's items, cached by content (headers: only the items that define a name)."""
        b = self.tree.blob(path)
        if b is None:
            return []

        def parse() -> list[Item]:
            items = parse_c((self.tree.read(path) or b"").decode("utf-8", "replace"))
            return [it for it in items if it.names and it.kind != "proto"] if named_only else items
        return self.cache.get("h" if named_only else "c", b, parse)

    def _game_files(self) -> dict[str, list[Item]]:
        paths = self.tree.listdir(GAME, ".c") + self.tree.listdir(GAME, ".h")
        self.tree.prefetch(p for p in paths if self.tree.staged)
        return {p: self._parsed(p) for p in paths}

    def _headers(self) -> dict[str, list[Item]]:
        """The headers game code includes (cport/include, transitively), by name defined."""
        index: dict[str, list[Item]] = {}
        seen: set[str] = set()
        todo = ["game.h", "runtime.h"]
        for items in self.files.values():
            todo += [it.text.split('"')[1] for it in items if it.kind == "pp" and '"' in it.text]
        while todo:
            h = todo.pop()
            if h in seen:
                continue
            seen.add(h)
            path = f"{INCLUDE}/{h}"
            b = self.tree.blob(path)
            if b is None:
                continue
            todo += self.cache.get("includes", b, lambda path=path: re.findall(
                r'^\s*#\s*include\s+"([^"]+)"', (self.tree.read(path) or b"").decode("utf-8", "replace"), re.M))
            for it in self._parsed(path, named_only=True):
                if it.kind != "proto":
                    for n in it.names:
                        index.setdefault(n, []).append(it)
        return index

    def _readable(self) -> dict[str, Readable]:
        local: dict[str, dict[str, list[Item]]] = {}
        shared: dict[str, list[Item]] = {}
        for path, items in self.files.items():
            for it in items:
                if it.kind in ("proto", "pp"):
                    continue
                for n in it.names:
                    (local.setdefault(path, {}) if it.static or path.endswith(".h") or it.kind not in LINKED
                     else shared).setdefault(n, []).append(it)
        out: dict[str, Readable] = {}
        for path, items in self.files.items():
            for it in items:
                if it.next and it.names:
                    body, head = self._closure(it, local.setdefault(path, {}), shared)
                    out[it.names[0]] = Readable(it.names[0], path, body, head)
        return out

    def _closure(self, start: Item, local: dict[str, list[Item]], shared: dict[str, list[Item]],
                 memo: dict[tuple[int, int], list[tuple[Item, bool]]] | None = None) -> tuple[list[Item], list[Item]]:
        """The function and every non-readable definition it uses, in game code and in headers. MEMO
        holds what each item names, for one fixed set of files (one per tree state)."""
        memo = self._deps if memo is None else memo
        body: list[Item] = [start]
        head: list[Item] = []
        seen = {id(start)}
        todo = [start]
        while todo:
            item = todo.pop()
            for it, in_game in self._uses(item, local, shared, memo):
                if id(it) in seen:
                    continue
                seen.add(id(it))
                (body if in_game else head).append(it)
                todo.append(it)
        return body, head

    def _uses(self, item: Item, local: dict[str, list[Item]], shared: dict[str, list[Item]],
              memo: dict[tuple[int, int], list[tuple[Item, bool]]]) -> list[tuple[Item, bool]]:
        """The definitions an item names directly (other readable functions excluded: they have stamps
        of their own). Memoized per item and file scope."""
        key = (id(item), id(local))
        if key not in memo:
            out: list[tuple[Item, bool]] = []
            for n in item.idents:
                found = local.get(n) or shared.get(n)
                if found:
                    out += [(it, True) for it in found if not it.next]
                else:
                    out += [(it, False) for it in self.header_index.get(n, ())]
            memo[key] = out
        return memo[key]

    # lifted output, ROM, lifter

    def _lifted(self) -> tuple[dict[str, Header], dict[str, tuple[str, ...]]]:
        headers: dict[str, Header] = {}
        targets: dict[str, tuple[str, ...]] = {}
        full = os.path.join(ROOT, LIFTED)
        files = sorted(f for f in os.listdir(full) if f.endswith(".c")) if os.path.isdir(full) else []
        if not files:
            self.notes.append(f"no lifted output in {LIFTED} (tools/build_lifted.sh): contracts not checked")
        for f in files:
            path = f"{LIFTED}/{f}"
            key = file_key(path)
            callees = f.startswith("difftest_lifted")

            def load(path: str = path, callees: bool = callees) -> tuple[dict[str, Header], dict[str, tuple[str, ...]]]:
                with open(os.path.join(ROOT, path), encoding="utf-8", errors="replace") as fh:
                    return parse_lifted(fh.read(), callees)
            h, t = self.cache.get("lifted", key, load)
            headers.update(h)
            targets.update(t)
        return headers, targets

    def _copies(self) -> list[tuple[int, int, int]]:
        """Where RAM code's bytes are in ROM (vma, lma, size): the ELF's loaded segments (bn6f's
        iwram_text), and the lifter's code copies (code the game itself copies, e.g. SoundMainRAM)."""
        raw = self.tree.read(LIFT_PY) or b""
        out = [(int(v, 16), int(l, 16), int(s, 16)) for v, l, s in
               re.findall(rb'CodeCopy\("\w+",\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+)\)', raw)]
        try:
            with open(os.path.join(ROOT, "bn6f", "bn6f.elf"), "rb") as fh:
                head = fh.read(4096)
            phoff, = struct.unpack_from("<I", head, 0x1C)
            size, count = struct.unpack_from("<HH", head, 0x2A)
            for i in range(count):
                kind, _, vaddr, paddr, filesz = struct.unpack_from("<5I", head, phoff + i * size)
                if kind == 1 and filesz and vaddr != paddr:
                    out.append((vaddr, paddr, filesz))
        except (OSError, struct.error):
            self.notes.append("no bn6f/bn6f.elf: IWRAM code's ROM bytes not found")
        return out

    def rom_bytes(self) -> bytes:
        if self._rom is None:
            try:
                with open(os.path.join(ROOT, ROM), "rb") as fh:
                    self._rom = fh.read()
            except FileNotFoundError:
                self.notes.append(f"no {ROM} (build bn6f): ROM hashes not checked")
                self._rom = b""
        return self._rom

    def _lifter_version(self) -> str:
        """A hash of the lifter's sources (tools/lifter/*.py)."""
        digest = hashlib.sha1()
        for path in self.tree.listdir(LIFTER, ".py"):
            digest.update(path.encode() + b"\0" + (self.tree.read(path) or b""))
        return digest.hexdigest()[:12]

    def rom_key(self, header: Header) -> str | None:
        """farm.input_key: a hash of the function's original bytes (IWRAM copies: their ROM source)."""
        lo, hi = header.lo, header.hi
        for vma, lma, size in self.copies:
            if vma <= lo < hi <= vma + size:
                lo, hi = lma + lo - vma, lma + hi - vma
        rom = self.rom_bytes()
        if 0x08000000 <= lo < hi <= 0x08000000 + len(rom) and hi - lo < 0x10000:
            return "rom:" + hashlib.sha1(rom[lo - 0x08000000: hi - 0x08000000]).hexdigest()[:16]
        return None

    # hashing under roots

    def items_hash(self, items: list[Item]) -> str:
        """Order-free, so moving definitions within or between files changes nothing."""
        texts = sorted(self._canon_of(it) for it in items)
        return hashlib.sha1("\n".join(texts).encode()).hexdigest()[:16]

    def _canon_of(self, it: Item) -> str:
        key = id(it)
        if key not in self._canon:
            self._canon[key] = (it, self.names.canon_text(it.text))
        return self._canon[key][1]

    def facts(self, name: str) -> Facts:
        r = self.readable[name]
        h = self.lifted.get(name)
        callees: dict[str, str] = {}
        if h is not None:
            for t in self.lifted_targets.get(name, ()):
                th = self.lifted.get(t)
                if th is not None and t != name:
                    callees[t] = " ".join(th.contract.outputs) or "-"
        return Facts(function=name,
                     range=f"0x{h.lo:08X}-0x{h.hi:08X}" if h else None,
                     rom=self.rom_key(h) if h else None,
                     contract=h.contract if h else None,
                     callees=callees,
                     headers=self.items_hash(r.header_items)[:12],
                     lifter=self.lifter,
                     body=self.items_hash(r.body_items))


# ── stamps in db entries ──

COPY_NAME = re.compile(r"^(\w+)_iwram_[0-9A-F]{8}$")    # the lifter's name for code the game copies to IWRAM


def home_entry(function: str, names: set[str]) -> str | None:
    """The db entry that holds a readable function's stamp: its own, or for an IWRAM copy
    (SoundMainRAM_iwram_03005700) the entry of the ROM symbol it is copied from."""
    if function in names:
        return function
    m = COPY_NAME.match(function)
    return m.group(1) if m and m.group(1) in names else None


STAMP_KEYS = ("function", "rom", "range", "contract", "callees", "headers", "lifter", "prompt", "model", "run",
              "body", "installed", "edited", "stamped")
UNKNOWN = "unknown"


def stamp_of(facts: Facts, prov: dict[str, Any], entry_name: str) -> dict[str, Any]:
    s: dict[str, Any] = {}
    if facts.function != entry_name:
        s["function"] = facts.function
    s["rom"] = facts.rom or UNKNOWN
    s["range"] = facts.range or UNKNOWN
    s["contract"] = facts.contract.text() if facts.contract else UNKNOWN
    s["callees"] = dict(facts.callees)
    s["headers"] = facts.headers or UNKNOWN
    s["lifter"] = facts.lifter or UNKNOWN
    for k in ("prompt", "model", "run"):
        s[k] = str(prov.get(k) or UNKNOWN)
    s["body"] = facts.body or UNKNOWN
    s["installed"] = str(prov.get("installed") or UNKNOWN)
    if prov.get("edited"):
        s["edited"] = list(prov["edited"])
    s["stamped"] = datetime.date.today().isoformat()
    return s


def stamp_text(stamp: dict[str, Any]) -> str:
    """The `stamp:` block as yaml.safe_dump writes it, so tools that re-dump entries keep it as is."""
    import yaml
    return yaml.safe_dump({"stamp": stamp}, sort_keys=False, allow_unicode=True, width=100)


def replace_stamp(text: str, block: str) -> str:
    """The entry's text with its `stamp:` block replaced (or appended): nothing else changes."""
    lines = text.splitlines(keepends=True)
    out: list[str] = []
    i = 0
    while i < len(lines):
        if re.match(r"stamp:\s*$", lines[i]):
            i += 1
            while i < len(lines) and (lines[i].startswith((" ", "-")) or not lines[i].strip()):
                i += 1
            continue
        out.append(lines[i])
        i += 1
    body = "".join(out)
    if body and not body.endswith("\n"):
        body += "\n"
    return body + block


# ── the check: the fixed action table of the joint plan ──

class Action(Enum):
    MODEL = "back to a model"
    REVERIFY_DEAD = "re-verify, queue the dead writes for removal (R1-A6)"
    REVERIFY = "re-verify"
    RECOMPILE = "recompile and re-verify"
    HAND_EDIT = "re-stamp as a hand edit"
    STAMP = "stamp it"
    INFO = "informational"


ORDER = list(Action)


@dataclass
class Finding:
    function: str
    entry: str | None
    action: Action
    why: str


FLAG_WORDS = {"nz": r"\b(?:SET_FLAGS_\w+|ZERO_FLAG|NEGATIVE_FLAG|cpu\s*\.\s*nz)\b",
              "c": r"\b(?:CARRY_FLAG|cpu\s*\.\s*c)\b", "v": r"\b(?:OVERFLOW_FLAG|cpu\s*\.\s*v)\b"}


def reads_any(items: list[Item], dropped: set[str]) -> bool:
    """Whether readable code mentions a register (or flag) a callee no longer returns: a stand-in for
    the R1-A21 leftover-register lint until that lint can be asked about one caller."""
    text = "\n".join(it.text for it in items)
    for r in dropped:
        if r in FLAG_WORDS:
            if re.search(FLAG_WORDS[r], text):
                return True
        elif re.fullmatch(r"r\d+", r) and re.search(rf"\bR{r[1:]}\b", text):
            return True
        elif r == "lr" and re.search(r"\bLR\b", text):
            return True
    return False


def compare(world: World, entry: str, stamp: dict[str, Any]) -> list[Finding]:
    name = str(stamp.get("function") or entry)
    if name not in world.readable:
        return [Finding(name, entry, Action.INFO, "stamped, but no longer a readable function in cport/src/game")]
    now = world.facts(name)
    out: list[Finding] = []

    def add(action: Action, why: str) -> None:
        out.append(Finding(name, entry, action, why))

    if now.range is not None and stamp.get("range") not in (None, UNKNOWN) and stamp.get("range") != now.range:
        add(Action.MODEL, f"address range {stamp.get('range')} is now {now.range}")
    elif now.rom is not None and stamp.get("rom") not in (None, UNKNOWN) and stamp.get("rom") != now.rom:
        add(Action.MODEL, f"original bytes changed ({stamp.get('rom')} -> {now.rom})")
    old = Contract.parse(str(stamp.get("contract", "")))
    if old is not None and now.contract is not None and old != now.contract:
        wider = (set(now.contract.outputs) - set(old.outputs)) | (now.contract.checked() - old.checked())
        narrower = (set(old.outputs) - set(now.contract.outputs)) | (old.checked() - now.contract.checked())
        if wider:
            add(Action.MODEL, f"own outputs widened by {' '.join(sorted(wider))}")
        elif narrower:
            add(Action.REVERIFY_DEAD, f"own outputs narrowed by {' '.join(sorted(narrower))}")
        if set(old.inputs) != set(now.contract.inputs):
            add(Action.REVERIFY, f"own inputs changed ({' '.join(old.inputs)} -> {' '.join(now.contract.inputs)})")
    now_callees = {world.names.canon(c): set(o.split()) - {"-"} for c, o in now.callees.items()}
    readable = world.readable[name]
    for callee, outputs in (stamp.get("callees") or {}).items():
        r = world.names.canon(str(callee))
        if r not in now_callees:
            continue
        dropped = (set(str(outputs).split()) - {"-"}) - now_callees[r]
        if dropped:
            who = world.names.current.get(Root(r), callee)
            if reads_any(readable.body_items, dropped):
                add(Action.REVERIFY, f"callee {who} no longer returns {' '.join(sorted(dropped))}, which this code reads")
            else:
                add(Action.INFO, f"callee {who} no longer returns {' '.join(sorted(dropped))} (not read here)")
    if stamp.get("body") != now.body:       # (a new body uses other header definitions: one finding)
        add(Action.HAND_EDIT, "body changed since the stamp, with no new stamp")
    elif stamp.get("headers") not in (None, UNKNOWN) and stamp.get("headers") != now.headers:
        add(Action.RECOMPILE, "the header definitions it uses changed")
    if stamp.get("lifter") not in (None, UNKNOWN) and stamp.get("lifter") != now.lifter:
        add(Action.INFO, "lifter changed since the stamp (golden tests and lockstep cover it)")
    return out


def check_stamps(world: World, entries: list[dict[str, Any]]) -> tuple[list[Finding], dict[str, int]]:
    """Every readable function against its entry's stamp."""
    by_function: dict[str, tuple[str, dict[str, Any]]] = {}
    has_entry: set[str] = set()
    for e in entries:
        st = e.get("stamp")
        if isinstance(e.get("name"), str):
            has_entry.add(e["name"])
        if isinstance(st, dict):
            by_function[str(st.get("function") or e.get("name"))] = (str(e.get("name")), st)
    findings: list[Finding] = []
    counts = {"readable": len(world.readable), "stamped": 0}
    for name in sorted(world.readable):
        if name not in by_function:
            home = home_entry(name, has_entry)
            where = f"{home}.yaml" if home else "a symbols/db entry (none yet)"
            findings.append(Finding(name, home, Action.STAMP, f"no stamp in {where}: tools/symdb.py stamp {name}"))
            continue
        counts["stamped"] += 1
        entry, st = by_function[name]
        findings.extend(compare(world, entry, st))
    for name, (entry, _) in by_function.items():
        if name not in world.readable:
            findings.append(Finding(name, entry, Action.INFO, "stamped, but no longer a readable function"))
    return findings, counts


def summary(findings: list[Finding], counts: dict[str, int], notes: list[str], limit: int = 5) -> list[str]:
    acting = {f.function for f in findings if f.action is not Action.INFO}
    lines = [f"stamps: {counts['readable']} readable functions, {counts['stamped']} stamped, "
             f"{len(acting)} need action"]
    for action in ORDER:
        group = [f for f in findings if f.action is action]
        if not group:
            continue
        names = sorted({f.function for f in group})
        lines.append(f"  {action.value}: {len(names)} ({', '.join(names[:limit])}{', ...' if len(names) > limit else ''})")
    lines += [f"  note: {n}" for n in notes]
    return lines


# ── where a function came from: the db entry's own record, pool.tsv and git history ──

MODEL_RES = (
    (re.compile(r"Written by (Claude) through the convert workflow \((?:\d+ attempt\(s\), )?run ([\w-]+)"), "claude"),
    (re.compile(r"Written by ([\w./-]+) through the convert workflow \((?:\d+ attempt\(s\), )?run ([\w-]+)"), None),
    (re.compile(r"Written by (.+?) through the farm workflow \(run ([\w-]+)"), None),
    (re.compile(r"Drafted by (.+?) \(tools/convert/farm\.py, (batch \d+)\)"), None),
    (re.compile(r"(?:Readable version )?[Dd]rafted by (.+?) with tools/convert/farm\.py"), None),
)
HAND_RE = re.compile(r"Hand-written|hand-written|by hand", re.I)


@dataclass
class Commit:
    sha: str
    subject: str
    trailer_model: str | None       # Co-Authored-By: Claude ...
    session: str | None             # Claude-Session: .../session_ID


def pool_models() -> dict[str, str]:
    out: dict[str, str] = {}
    try:
        with open(os.path.join(ROOT, POOL)) as fh:
            rows = [line.rstrip("\n").split("\t") for line in fh if not line.startswith("#")]
    except FileNotFoundError:
        return out
    if not rows:
        return out
    head = rows[0]
    for row in rows[1:]:
        r = dict(zip(head, row))
        if r.get("state") == "installed" and r.get("models", "-") != "-":
            out[r["current_name"]] = r["models"]
    return out


def normalise_model(text: str, pool: str | None) -> str:
    """A provider/model id where one can be told: as written, or the one of pool.tsv's ids (the farm's
    exact ids, every model that ever tried the function) that the entry's wording names."""
    t = text.strip()
    if "/" in t:
        return t.replace("z.ai/", "zai/")
    low = t.lower().replace(" ", "-")
    for provider in ("hyper", "zai", "z.ai", "opencode", "minimax"):
        if low.startswith(provider + "-"):
            return f"{provider.replace('z.ai', 'zai')}/{low[len(provider) + 1:]}"
    ids = (pool or "").split(",")
    same = [i for i in ids if i.split("/")[-1].lower() == low]
    if len(same) == 1:
        return same[0]
    return f"{low} (provider not recorded)"


def provenance_from_entry(e: dict[str, Any], pool: dict[str, str]) -> dict[str, str]:
    """Model and run as the entry's readable-c evidence (or changes) says; empty when it does not."""
    texts = [str(ev.get("detail", "")) for ev in (e.get("evidence") or []) if ev.get("method") == "readable-c"]
    texts += [str(ch.get("tool", "")) for ch in (e.get("changes") or [])]
    name = str(e.get("name"))
    for text in texts:
        for rx, fixed in MODEL_RES:
            m = rx.search(text)
            if m:
                model = fixed or normalise_model(m.group(1), pool.get(name))
                run = m.group(2) if m.lastindex and m.lastindex >= 2 else UNKNOWN
                return {"model": model, "run": run.replace("batch ", "farm.py batch ") if run.startswith("batch") else run}
    for text in texts:
        if HAND_RE.search(text):
            return {"model": "hand", "run": UNKNOWN}
    if name in pool:
        return {"model": pool[name], "run": UNKNOWN}
    return {}


def history(world: World, names: set[str]) -> dict[str, tuple[str, list[str]]]:
    """For each function: the commit that first installed it and the later commits that changed its
    body (renames aside), from the history of cport/src/game. A body that differs from HEAD in the
    tree has the pseudo-commit "uncommitted"."""
    log = git("log", "--reverse", "--format=%x00%H", "--name-only", "HEAD", "--", GAME)
    commits: list[tuple[str, list[str]]] = []
    for chunk in log.split("\0")[1:]:
        lines = chunk.strip().splitlines()
        commits.append((lines[0], [p for p in lines[1:] if p.startswith(GAME + "/") and p.endswith((".c", ".h"))]))
    files: dict[str, list[Item]] = {}
    want = {world.names.canon(n): n for n in names}
    result: dict[str, tuple[str, list[str]]] = {}
    last: dict[str, str] = {}
    blobs: dict[tuple[str, str], bytes] = {}

    def read_at(commit: str, paths: list[str]) -> None:
        spec = "".join(f"{commit}:{p}\n" for p in paths)
        done = subprocess.run(["git", "cat-file", "--batch"], cwd=ROOT, input=spec.encode(), capture_output=True, check=True)
        out, pos = done.stdout, 0
        for p in paths:
            eol = out.index(b"\n", pos)
            head = out[pos:eol].split()
            if len(head) < 3 or head[1] != b"blob":
                blobs[(commit, p)] = b""
                pos = eol + 1
                continue
            size = int(head[2])
            blobs[(commit, p)] = out[eol + 1: eol + 1 + size]
            pos = eol + 1 + size + 1

    def hashes(state: dict[str, list[Item]]) -> dict[str, str]:
        local: dict[str, dict[str, list[Item]]] = {}
        shared: dict[str, list[Item]] = {}
        for path, items in state.items():
            for it in items:
                if it.kind in ("proto", "pp"):
                    continue
                for n in it.names:
                    (local.setdefault(path, {}) if it.static or path.endswith(".h") or it.kind not in LINKED
                     else shared).setdefault(n, []).append(it)
        out: dict[str, str] = {}
        memo: dict[tuple[int, int], list[tuple[Item, bool]]] = {}     # this state's own (ids are reused later)
        for path, items in state.items():
            for it in items:
                if it.next and it.names:
                    r = world.names.canon(it.names[0])
                    if r in want:
                        body, _ = world._closure(it, local.setdefault(path, {}), shared, memo)
                        out[r] = world.items_hash(body)
        return out

    def step(sha: str, state: dict[str, list[Item]]) -> None:
        for r, h in hashes(state).items():
            if r not in result:
                result[r] = (sha, [])
            elif last.get(r) != h:
                result[r][1].append(sha)
            last[r] = h

    for sha, changed in commits:
        read_at(sha, changed)
        for p in changed:
            data = blobs.pop((sha, p))
            if data:
                files[p] = world.cache.get("c", blob_id(data), lambda d=data: parse_c(d.decode("utf-8", "replace")))
            else:
                files.pop(p, None)
        step(sha[:7], files)
    step("uncommitted", world.files)
    return {want[r]: v for r, v in result.items()}


def commit_info(shas: Iterable[str]) -> dict[str, Commit]:
    out: dict[str, Commit] = {}
    for sha in set(shas) - {"uncommitted"}:
        text = git("log", "-1", "--format=%s%x00%B", sha)
        subject, body = text.split("\0", 1)
        m = re.search(r"^Co-Authored-By: (Claude [^<]+?)\s*<", body, re.M)
        s = re.search(r"^Claude-Session: \S*?(session_\w+)", body, re.M)
        out[sha] = Commit(sha, subject, m.group(1) if m else None, s.group(1) if s else None)
    return out


def provenance(world: World, entries: dict[str, dict[str, Any]], names: Iterable[str],
               keep: dict[str, dict[str, Any]], explicit: dict[str, str]) -> dict[str, dict[str, Any]]:
    """Who made each function's current body. In order: what the caller says (--model/--run/--prompt);
    the existing stamp when the body is unchanged; otherwise the entry's record, pool.tsv and git
    history; "unknown" where none of them can tell."""
    names = list(names)
    pool = pool_models()
    hist = history(world, set(names))
    info = commit_info(sha for first, later in hist.values() for sha in [first, *later])
    out: dict[str, dict[str, Any]] = {}
    for n in names:
        first, later = hist.get(n, (UNKNOWN, []))
        installed = later[-1] if later else first
        old = keep.get(n)
        if explicit:
            p: dict[str, Any] = {"prompt": explicit.get("prompt", UNKNOWN), "model": explicit.get("model", UNKNOWN),
                                 "run": explicit.get("run", UNKNOWN), "installed": explicit.get("installed", installed)}
        elif old and old.get("body") == world.facts(n).body:
            p = {k: old.get(k) for k in ("prompt", "model", "run", "installed", "edited")}
            if p.get("installed") in ("uncommitted", None, UNKNOWN):
                p["installed"] = installed
        elif old:
            p = {"prompt": "none", "model": hand_model(info.get(installed)), "run": hand_run(info.get(installed)),
                 "installed": installed, "edited": [*(old.get("edited") or []), installed]}
        else:
            src = provenance_from_entry(entries.get(home_entry(n, set(entries)) or n, {}), pool)
            model, run = src.get("model", UNKNOWN), src.get("run", UNKNOWN)
            if model == "hand":
                model, run = hand_model(info.get(first)), hand_run(info.get(first))
            elif model == "claude":       # Claude as the convert workflow's model: the commit names which
                c = info.get(first)
                model = c.trailer_model if c and c.trailer_model else "claude"
            p = {"prompt": "legacy", "model": model, "run": run, "installed": installed}
            if later:
                p["edited"] = later
        out[n] = p
    return out


def hand_model(c: Commit | None) -> str:
    """Hand-written code: by Claude in a session when the commit says so."""
    return f"hand ({c.trailer_model})" if c and c.trailer_model else "hand"


def hand_run(c: Commit | None) -> str:
    """Hand-written code's run: the Claude Code session the commit names."""
    return f"claude {c.session}" if c and c.session else UNKNOWN


# ── stamp: write stamps into entries ──

def write_stamps(world: World, entries_by_file: dict[str, dict[str, Any]], names: list[str],
                 explicit: dict[str, str], dry_run: bool = False) -> list[str]:
    """(Re)stamp NAMES; returns the entry files written. Entries are re-read just before writing and only
    their stamp block changes, so concurrent edits to the rest of an entry are kept."""
    by_name = {str(e.get("name")): (f, e) for f, e in entries_by_file.items()}
    homes: dict[str, str] = {}
    for f, e in entries_by_file.items():
        st = e.get("stamp")
        if isinstance(st, dict) and st.get("function"):
            homes[str(st["function"])] = f
    keep = {n: (by_name[n][1].get("stamp") if n in by_name else None) or {} for n in names}
    keep = {n: st for n, st in keep.items() if not st.get("function") or st.get("function") == n}
    for n, f in homes.items():
        if n in names:
            keep[n] = entries_by_file[f].get("stamp") or {}
    prov = provenance(world, {n: e for n, (_, e) in by_name.items()}, names, keep, explicit)
    written: list[str] = []
    for n in names:
        home = home_entry(n, set(by_name))
        f = homes.get(n) or (by_name[home][0] if home else None)
        if f is None:
            print(f"{n}: no symbols/db entry to hold its stamp; write the entry first", file=sys.stderr)
            continue
        entry_name = str(entries_by_file[f].get("name"))
        block = stamp_text(stamp_of(world.facts(n), prov[n], entry_name))
        path = os.path.join(ROOT, f)
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
        new = replace_stamp(text, block)
        if new != text and not dry_run:
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(new)
            written.append(f)
    return written


def facts_json(f: Facts) -> dict[str, Any]:
    d = dataclasses.asdict(f)
    d["contract"] = f.contract.text() if f.contract else None
    return d


if __name__ == "__main__":
    sys.exit("use tools/symdb.py stamps / stamp")
