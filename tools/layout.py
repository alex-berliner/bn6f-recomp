#!/usr/bin/env python3
"""The layout of cport/src/game: which file each readable function lives in (review 1 Area 8.4 and
A11, joint plan S1 "db and layout": the regroup of the model farm's grab-bag files).

The rule. Subsystems converted as a whole (a convert run's own file: npc_update.c, m4a_mixer.c, ...)
keep their files. Every other readable function, one converted on its own (the model farm), lives in
the file of the bn6f source that defines it: `object.c` holds those of bn6f/asm/object.s. A big
source is cut into parts at fixed addresses (tools/layout.txt): the first part keeps the source's
name, a later one is named after its first address (`asm00_2_8013BDA.c`). In a file the functions
are in address order, each in its own part, which starts with the line `/* ── NAME ── */` and holds
everything the function needs (its comment, local enums and macros, static helpers), as the farm
wrote it; the file's header is generated. Laying out never changes a part's text.

Inbox files: the farm's old grab-bags (`leaves_*.c`) and `inbox_*.c` files (and a farm group the
convert workflow is installing) are emptied into the laid-out files and deleted. A file without part
lines (leaves_misc.c) is cut at its functions: each part runs from the end of the previous function
to the end of its own, and joins the previous one when it uses something that one defines.

A part must be self-contained where it lands (a problem stops the layout):
  - no two parts of a file define the same file-level name, other than a macro (C allows defining a
    macro again; each part's own definition comes before its uses, so it sees its own);
  - no part uses a name another part of its file defines, unless it defines it itself: it would see the
    neighbour's definition (a macro counts from its `#define` on, so a part may come before a neighbour
    that defines a macro it takes from the shared headers). Two parts that break this only by order
    are cut apart with a split address in tools/layout.txt.
These are the identifier-level rules tools/stamps.py's body hash follows, so a layout that passes them
moves no function's stamp.

  tools/layout.py                 lay out the working tree: move parts, delete inboxes, update symbols/db
  tools/layout.py --dry-run       say what would move
  tools/layout.py --check         (the commit gate, with --staged for the index) exit 1 unless laid out
  tools/layout.py --where NAME... the file each function belongs in
  tools/layout.py --sizes         functions and lines per laid-out file (add a split above SPLIT_WARN_LINES)

symbols/db: a moved function's entry gets its new file in `readable_c:` and in evidence `refs:` that
named the old file, and a `changes:` item saying it moved (the text of the entry is edited line by
line, nothing else changes). Standard library only, and typed: tools/wf imports it.
"""
from __future__ import annotations

import argparse
import bisect
import datetime
import fnmatch
import functools
import hashlib
import os
import re
import subprocess
import sys
from collections.abc import Iterable, Mapping
from dataclasses import dataclass, field
from typing import NamedTuple

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gamepaths  # noqa: E402  # tools/gamepaths.py: where bn6f.elf is
import stamps  # noqa: E402  # tools/stamps.py: the C item parser, staged-tree reads, db entry homes

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = "cport/src/game"
DB = "symbols/db"
TABLE = os.path.join(ROOT, "tools", "layout.txt")
INBOX = ("leaves_*.c", "inbox_*.c")
SPLIT_WARN_LINES = 4000           # a laid-out file longer than this wants a split address in tools/layout.txt
PART = re.compile(r"^/\* ── (\w+) ── \*/$", re.M)
GROUP_LINE = re.compile(r"^/\* ── bn6f [\w.]+ ─+ \*/\n", re.M)     # leaves_misc.c's per-source headings
NEXT = re.compile(r"^(?:IWRAM_\w+\s+|ARM_CODE\s+)*Next\s+(\w+)\s*\(\s*void\s*\)\s*\{", re.M)
IWRAM_COPY = re.compile(r"_iwram_([0-9A-F]{8})$")


# ── where bn6f defines each function ──

@dataclass(frozen=True)
class Bn6f:
    """bn6f.elf's symbols (name -> address) and which source each address was assembled from (the
    DWARF line table, as the lifter reads it): runs of (first address, source file)."""
    symbols: Mapping[str, int]
    starts: tuple[int, ...]
    sources: tuple[str, ...]

    def source(self, address: int) -> str | None:
        i = bisect.bisect_right(self.starts, address) - 1
        return self.sources[i] if i >= 0 else None

    def address(self, name: str) -> int | None:
        if name in self.symbols:
            return self.symbols[name]
        m = IWRAM_COPY.search(name)          # the lifter's IWRAM copy of a ROM routine: its run address
        return int(m.group(1), 16) if m else None

    def stems(self) -> set[str]:
        return {os.path.splitext(s)[0] for s in self.sources}


@functools.cache
def bn6f(elf: str = gamepaths.ORIG_ELF) -> Bn6f:
    def run(*args: str) -> str:
        return subprocess.run(["readelf", *args, elf], check=True, capture_output=True, text=True).stdout
    symbols: dict[str, int] = {}
    for line in run("-sW").splitlines():
        p = line.split()
        if len(p) >= 8 and p[0].endswith(":") and p[6] not in ("UND", "ABS") and re.fullmatch(r"[0-9a-f]{8}", p[1]):
            symbols.setdefault(p[7], int(p[1], 16) & ~1)
    at: dict[int, str] = {}
    for line in run("--debug-dump=decodedline").splitlines():
        p = line.split()
        if len(p) >= 3 and p[0].endswith(".s") and p[1].isdigit() and p[2].startswith("0x"):
            at.setdefault(int(p[2], 16), p[0])
    starts: list[int] = []
    sources: list[str] = []
    for address in sorted(at):
        if not sources or sources[-1] != at[address]:
            starts.append(address)
            sources.append(at[address])
    return Bn6f(symbols, tuple(starts), tuple(sources))


# ── the files: which ones the layout owns, and the split table ──

def read_table(path: str = TABLE) -> dict[str, tuple[int, ...]]:
    """tools/layout.txt: a bn6f source's name without `.s`, then the first address of each part after
    the first."""
    out: dict[str, tuple[int, ...]] = {}
    with open(path) as fh:
        for line in fh:
            words = line.split("#", 1)[0].split()
            if words:
                out[words[0]] = tuple(sorted(int(w, 16) for w in words[1:]))
    return out


def part_name(stem: str, start: int | None) -> str:
    return f"{stem}.c" if start is None else f"{stem}_{start:X}.c"


def file_for(address: int, b: Bn6f, table: Mapping[str, tuple[int, ...]]) -> str | None:
    source = b.source(address)
    if source is None:
        return None
    stem = os.path.splitext(source)[0]
    splits = table.get(stem, ())
    i = bisect.bisect_right(splits, address) - 1
    return part_name(stem, splits[i] if i >= 0 else None)


def owned_names(b: Bn6f, table: Mapping[str, tuple[int, ...]]) -> set[str]:
    """Every file name the layout may write."""
    names = {f"{stem}.c" for stem in b.stems()}
    names |= {part_name(stem, a) for stem, splits in table.items() for a in splits}
    return names


def is_inbox(name: str, extra: Iterable[str] = ()) -> bool:
    return name in set(extra) or any(fnmatch.fnmatch(name, pattern) for pattern in INBOX)


def header(name: str, b: Bn6f, table: Mapping[str, tuple[int, ...]]) -> str:
    stem = name[:-2]
    m = re.fullmatch(r"(\w+)_([0-9A-F]+)", stem)
    if m and m.group(1) in table and int(m.group(2), 16) in table[m.group(1)]:
        stem, start = m.group(1), int(m.group(2), 16)
    else:
        start = None
    splits = table.get(stem, ())
    later = [a for a in splits if start is None or a > start]
    if start is None and splits:
        span = f", up to 0x{splits[0]:08X}"
    elif start is not None:
        span = f", 0x{start:08X}-0x{later[0]:08X}" if later else f", from 0x{start:08X}"
    else:
        span = ""
    return ("/*\n"
            f" * {name}: readable functions of bn6f {stem}.s{span}, in address order.\n"
            " *\n"
            " * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in\n"
            " * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).\n"
            " * Each function's part starts at its `── NAME ──` line and holds what it needs.\n"
            " */\n"
            '#include "game.h"\n'
            '#include "runtime.h"\n')


# ── parts ──

LOCAL = ("macro", "enumerator", "type", "data", "function")


@dataclass
class Part:
    """One function's part of a file (or two functions', when one enters the other's frame)."""
    owner: str
    text: str                      # from its `/* ── NAME ── */` line to its end, trailing space removed
    origin: str                    # the file it was read from
    functions: tuple[str, ...]     # the `Next` functions it defines
    defines: frozenset[str]        # file-level names it defines (not its Next functions)
    macros: frozenset[str]
    uses: frozenset[str]           # every identifier in it

    @classmethod
    def of(cls, owner: str, text: str, origin: str) -> Part:
        items = stamps.parse_c(text)
        defined = [it for it in items if it.kind in LOCAL and not it.next]
        return cls(owner, text, origin, tuple(NEXT.findall(text)),
                   frozenset(n for it in defined for n in it.names),
                   frozenset(n for it in defined if it.kind == "macro" for n in it.names),
                   frozenset(i for it in items for i in it.idents))

    def digest(self) -> str:
        """The part's exact text (comments and layout included), for proving a move changed nothing."""
        return hashlib.sha1(self.text.encode()).hexdigest()[:16]


def parts_of(name: str, text: str) -> tuple[str, list[Part]]:
    """(the text before the first part, the parts) of a file; a file without part lines is cut at its
    functions."""
    marks = list(PART.finditer(text))
    if marks:
        parts = [Part.of(m.group(1), text[m.start(): marks[i + 1].start() if i + 1 < len(marks) else len(text)].rstrip(),
                         name) for i, m in enumerate(marks)]
        return text[: marks[0].start()], parts
    return cut_at_functions(name, text)


def cut_at_functions(name: str, text: str) -> tuple[str, list[Part]]:
    """A hand-assembled file (leaves_misc.c): its header (up to the first blank line after the
    includes), then one part per function, from the end of the previous one to its closing brace."""
    nexts = list(NEXT.finditer(text))
    if not nexts:
        return text, []
    includes = list(re.finditer(r'^#include .*\n', text[: nexts[0].start()], re.M))
    start = includes[-1].end() if includes else 0
    head, rest = text[:start], GROUP_LINE.sub("", text[start:])
    parts: list[Part] = []
    pos = 0
    for m in NEXT.finditer(rest):
        if m.start() < pos:
            continue
        end = closing_brace(rest, m.end() - 1)
        chunk = rest[pos:end].strip("\n")
        pos = end
        part = Part.of(m.group(1), f"/* ── {m.group(1)} ── */\n{chunk}", name)
        if parts and part.uses & parts[-1].defines - part.defines:
            parts[-1] = Part.of(parts[-1].owner, f"{parts[-1].text}\n\n{chunk}", name)
        else:
            parts.append(part)
    if rest[pos:].strip():
        parts[-1] = Part.of(parts[-1].owner, f"{parts[-1].text}\n\n{rest[pos:].strip()}", name)
    return head, parts


def closing_brace(text: str, open_at: int) -> int:
    """The offset just past the brace that closes the one at OPEN_AT (comments and strings skipped)."""
    code, _ = stamps.blank(text)
    depth = 0
    for i in range(open_at, len(code)):
        if code[i] == "{":
            depth += 1
        elif code[i] == "}":
            depth -= 1
            if depth == 0:
                return i + 1
    raise ValueError(f"no closing brace for the one at offset {open_at}")


# ── the layout ──

class Move(NamedTuple):
    function: str
    old: str           # file names under cport/src/game
    new: str


class Conflict(NamedTuple):
    """A part that cannot land where the rule puts it: BLAME names the parts involved, the one to take
    out first (the part that would see a neighbour's definition, the later of two definitions); NAME is the
    C name concerned, and DEFINERS the parts that define it (renaming it in one of them settles it)."""
    text: str
    blame: tuple[str, ...]
    name: str = ""
    definers: tuple[str, ...] = ()


@dataclass
class Plan:
    files: dict[str, str | None] = field(default_factory=dict)   # files to write (None: delete)
    moves: list[Move] = field(default_factory=list)
    problems: list[str] = field(default_factory=list)
    conflicts: list[Conflict] = field(default_factory=list)      # the problems that name parts
    parts: dict[str, Part] = field(default_factory=dict)        # owner -> its part, where it lands

    @property
    def laid_out(self) -> bool:
        return not self.files and not self.problems


def plan(files: Mapping[str, str], inbox: Iterable[str] = (), b: Bn6f | None = None,
         table: Mapping[str, tuple[int, ...]] | None = None) -> Plan:
    """What laying out FILES (name -> text, every .c file of cport/src/game) changes."""
    b = b or bn6f()
    table = read_table() if table is None else table
    owned = owned_names(b, table)
    out = Plan()
    placed: dict[str, list[tuple[int, Part]]] = {}
    for name in sorted(files):
        if name not in owned and not is_inbox(name, inbox):
            continue
        head, parts = parts_of(name, files[name])
        if name in owned and parts and head != header(name, b, table) + "\n":
            out.problems.append(f"{GAME}/{name}: text before its first part is not the generated header "
                                "(it would be lost); move it into a part")
        for part in parts:
            address = b.address(part.owner)
            if address is None:
                address = next((a for a in map(b.address, part.functions) if a is not None), None)
            dest = file_for(address, b, table) if address is not None else None
            if dest is None:
                out.conflicts.append(Conflict(f"{GAME}/{name}: {part.owner} is not a bn6f symbol, so it has no place",
                                              (part.owner,)))
                continue
            placed.setdefault(dest, []).append((address, part))
            if dest != name:
                out.moves += [Move(f, name, dest) for f in part.functions or (part.owner,)]
    for dest, parts in sorted(placed.items()):
        parts.sort(key=lambda ap: (ap[0], ap[1].owner))
        out.conflicts += part_problems(dest, [p for _, p in parts])
        for _, part in parts:
            out.parts[part.owner] = part
        text = header(dest, b, table) + "\n" + "\n\n".join(p.text for _, p in parts) + "\n"
        if files.get(dest) != text:
            out.files[dest] = text
    for name in files:
        if name in owned and name not in placed and name in files:
            out.files[name] = None              # every part moved out
        elif is_inbox(name, inbox):
            out.files[name] = None
    out.problems += [c.text for c in out.conflicts]
    return out


def part_problems(dest: str, parts: list[Part]) -> list[Conflict]:
    """The rules of the module docstring, for one file's parts in their order. A macro counts from its
    `#define` on (as for the compiler, and tools/stamps.py's body hash), other names everywhere."""
    out: list[Conflict] = []
    definers: dict[str, list[int]] = {}
    for i, part in enumerate(parts):
        for n in part.defines:
            definers.setdefault(n, []).append(i)
    for n, which in sorted(definers.items()):
        if len(which) > 1 and not all(n in parts[i].macros for i in which):
            out.append(Conflict(f"{GAME}/{dest}: {n} is defined by the parts of {', '.join(parts[i].owner for i in which)} "
                                "(a C name defined twice in one file)", tuple(parts[i].owner for i in reversed(which)),
                                n, tuple(parts[i].owner for i in which)))
    for i, part in enumerate(parts):
        for n in sorted((part.uses - part.defines) & definers.keys()):
            seen = [parts[j].owner for j in definers[n] if j != i and (j < i or n not in parts[j].macros)]
            if seen:
                out.append(Conflict(f"{GAME}/{dest}: {part.owner} uses {n}, which the part of {', '.join(seen)} defines"
                                    f"{' before it' if n in parts[definers[n][0]].macros else ''} (it would see a "
                                    "neighbour's definition)", (part.owner, *seen), n, tuple(seen)))
    return out


# ── the tree ──

def read_game(staged: bool = False, game_dir: str | None = None) -> dict[str, str]:
    """Every .c file of cport/src/game: in the working tree, the index (staged), or another tree."""
    if game_dir is not None:
        return {f: open(os.path.join(game_dir, f), encoding="utf-8").read()
                for f in sorted(os.listdir(game_dir)) if f.endswith(".c")}
    tree = stamps.Tree(staged=staged)
    paths = tree.listdir(GAME, ".c")
    tree.prefetch(paths)
    return {os.path.basename(p): (tree.read(p) or b"").decode("utf-8") for p in paths}


def write_game(p: Plan, game_dir: str) -> None:
    for name, text in sorted(p.files.items()):
        path = os.path.join(game_dir, name)
        if text is None:
            if os.path.exists(path):
                os.unlink(path)
        else:
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(text)


def lay_out(game_dir: str = os.path.join(ROOT, GAME), inbox: Iterable[str] = (),
            db_dir: str | None = os.path.join(ROOT, DB), note: str = "") -> Plan:
    """Lay out the files in GAME_DIR (and update the entries in DB_DIR); nothing is written when there
    are problems."""
    p = plan(read_game(game_dir=game_dir), inbox)
    if p.problems:
        return p
    write_game(p, game_dir)
    if db_dir is not None:
        update_db(p.moves, db_dir, note)
    return p


def where(names: Iterable[str]) -> dict[str, str | None]:
    b, table = bn6f(), read_table()
    return {n: (file_for(a, b, table) if (a := b.address(n)) is not None else None) for n in names}


# ── symbols/db ──

def readable_c_mismatches(files: Mapping[str, str], entries: Iterable[Mapping[str, object]]) -> list[tuple[str, str]]:
    """(entry path, problem) for each entry whose `readable_c:` does not name the file that holds its
    readable function (FILES: every .c file of cport/src/game; ENTRIES as tools/symdb.py loads them)."""
    by_name = {str(e.get("name")): e for e in entries}
    out: list[tuple[str, str]] = []
    for name, text in sorted(files.items()):
        for function in NEXT.findall(text):
            home = stamps.home_entry(function, set(by_name))
            if home is None:
                continue
            e = by_name[home]
            rc = e.get("readable_c")
            if rc is not None and rc != f"{GAME}/{name}":
                out.append((str(e.get("_file") or f"{DB}/{home}.yaml"),
                            f"readable_c is {rc}, but {function} is in {GAME}/{name}"))
    return out


def update_db(moves: Iterable[Move], db_dir: str, note: str = "") -> list[str]:
    """Each moved function's entry: readable_c, refs that named the old file, and a changes item."""
    by_entry: dict[str, list[Move]] = {}
    names = {f[:-5] for f in os.listdir(db_dir) if f.endswith(".yaml")}
    for move in moves:
        home = stamps.home_entry(move.function, names)
        if home is not None:
            by_entry.setdefault(home, []).append(move)
    written: list[str] = []
    today = datetime.date.today().isoformat()
    for entry, ms in sorted(by_entry.items()):
        path = os.path.join(db_dir, entry + ".yaml")
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
        new = text
        for m in ms:
            old, dest = f"{GAME}/{m.old}", f"{GAME}/{m.new}"
            new = re.sub(rf"^readable_c: {re.escape(old)}$", f"readable_c: {dest}", new, flags=re.M)
            new = re.sub(rf"^(\s*- ){re.escape(old)}$", rf"\g<1>{dest}", new, flags=re.M)
            new = add_change(new, {"date": today, "what": f"moved from {old} to {dest} by tools/layout.py "
                                   f"({note or 'the layout by bn6f source'}; the function's text unchanged)",
                                   "tool": "tools/layout.py"})
        if new != text:
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(new)
            written.append(path)
    return written


def add_change(text: str, change: Mapping[str, str]) -> str:
    """Append an item to the entry's `changes:` list (or add the list before `readable_c:`), in the
    form yaml.safe_dump writes, without re-dumping the rest."""
    item = "".join(f"{'- ' if i == 0 else '  '}{k}: {quote(v)}\n" for i, (k, v) in enumerate(change.items()))
    lines = text.splitlines(keepends=True)
    start = next((i for i, ln in enumerate(lines) if ln == "changes:\n"), None)
    if start is None:
        at = next((i for i, ln in enumerate(lines) if ln.startswith(("readable_c:", "coverage:", "related:",
                                                                      "open_questions:", "sources:", "stamp:"))),
                  len(lines))
        return "".join(lines[:at]) + "changes:\n" + item + "".join(lines[at:])
    end = start + 1
    while end < len(lines) and lines[end].startswith(("- ", "  ")):
        end += 1
    return "".join(lines[:end]) + item + "".join(lines[end:])


def quote(value: str) -> str:
    """A scalar as yaml.safe_dump would write it on one line (quoted when plain would not read back)."""
    plain = re.fullmatch(r"[A-Za-z0-9_./(][^:#\n]*[^:\s]|[A-Za-z0-9_]", value) and not re.search(r": | #|^[-?!&*]", value)
    return value if plain and not re.fullmatch(r"\d{4}-\d\d-\d\d|true|false|null|yes|no|on|off|[-+.\d]+", value, re.I) \
        else "'" + value.replace("'", "''") + "'"


# ── command line ──

def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0] if __doc__ else None)
    ap.add_argument("--check", action="store_true", help="exit 1 unless the tree is laid out")
    ap.add_argument("--staged", action="store_true", help="with --check: the index (the commit gate)")
    ap.add_argument("--dry-run", action="store_true", help="say what would move, write nothing")
    ap.add_argument("--where", nargs="+", metavar="NAME", help="the file each function belongs in")
    ap.add_argument("--sizes", action="store_true", help="functions and lines per laid-out file")
    ap.add_argument("--inbox", action="append", default=[], help="another file to empty into the layout")
    ap.add_argument("--note", default="", help="why, for the changes items in symbols/db")
    args = ap.parse_args(argv)
    if args.where:
        for name, dest in where(args.where).items():
            print(f"{name}\t{GAME}/{dest}" if dest else f"{name}\t(not a bn6f symbol)")
        return 0
    if args.sizes:
        b, table = bn6f(), read_table()
        owned = owned_names(b, table)
        for name, text in read_game().items():
            if name in owned:
                lines = text.count("\n")
                warn = f"  (over {SPLIT_WARN_LINES}: add a split to tools/layout.txt)" if lines > SPLIT_WARN_LINES else ""
                print(f"{name:28s} {len(NEXT.findall(text)):4d} functions {lines:6d} lines{warn}")
        return 0
    p = plan(read_game(staged=args.check and args.staged), args.inbox)
    for problem in p.problems:
        print(f"layout: {problem}", file=sys.stderr)
    if args.check:
        if p.files:
            print(f"layout: not laid out: {len(p.moves)} functions would move, "
                  f"{sum(t is not None for t in p.files.values())} files would be written and "
                  f"{sum(t is None for t in p.files.values())} deleted ({', '.join(sorted(p.files)[:6])}"
                  f"{' ...' if len(p.files) > 6 else ''}); run tools/layout.py", file=sys.stderr)
        return 0 if p.laid_out else 1
    if args.dry_run or p.problems:
        for m in p.moves:
            print(f"{m.function}\t{m.old} -> {m.new}")
        print(f"{len(p.moves)} functions would move; {len(p.files)} files would change"
              + (f"; {len(p.problems)} problems, nothing written" if p.problems else ""), file=sys.stderr)
        return 1 if p.problems else 0
    write_game(p, os.path.join(ROOT, GAME))
    written = update_db(p.moves, os.path.join(ROOT, DB), args.note)
    print(f"moved {len(p.moves)} functions; wrote {sum(t is not None for t in p.files.values())} files, deleted "
          f"{sum(t is None for t in p.files.values())}; updated {len(written)} symbols/db entries", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
