#!/usr/bin/env python3
"""Find readable code that reads a register or flag a callee leaves behind but does not return.

  tools/abi/leftovers.py [PATH...] [--graph FILE]    JSON report on stdout, summary on stderr
  tools/abi/leftovers.py --check                     also exit 1 on a read without a note
  tools/abi/leftovers.py --reads FUNCTION            what FUNCTION reads after each callee (JSON)
  leftovers.reads_after_calls(FUNCTION)              the same from Python (tools/stamps.py)

After `GUEST_CALL(F)` a readable function may read only what F returns. The lifter works
out each function's contract from the original assembly, refined by the call edges the
routes showed (the header of every lifted function, cport/src/lifted/*.c: outputs,
"clobbers (unused by callers)", preserved; tools/lifter/abi_export.py REFINED). A
register F writes but no original caller reads is a leftover: F's value there is an
accident of its code, and a readable or typed F need not reproduce it (the difftest
compares only what F returns). Reading it is the bug class of commit 67673ff
(TestEventFlag's answer is the Z flag; r0 is just the flag's byte).

Each read is one of:
  contract   F never returns it: no original caller reads it after F (lifted header).
  site       F returns it to some callers, but the original of this caller never reads it
             after F (per call site liveness in build-abi/graph.json, lift.py --export-abi).
  passed     not read in C but passed on into the next callee G, which takes it as input,
             where the original of this caller does not do so.
  zflag      the original branches on F's flags right after the call (`bl F; beq x`) and also
             uses r0 later; the C tests r0 and reads no flag. Right only where F's Z always
             means r0 == 0 (not so for TestEventFlag): listed for a look at F, not failing.
A read is followed from the call to its first use along the C text: plain writes in the
same block end it, writes in nested blocks or in a brace-less if do not, a return in the
same block ends the path, and a call inside a loop also checks the loop's start (the next
pass). Approximate, so every hit is checked against the original's assembly.

A site the original really relies on is noted in the function, or in the comment block
before it, as  /* register form: relies on F leaving rN (contract says clobbered) */
(flags: nz, c, v); a leftover that is passed on but provably reaches nothing as
/* leftover unused: F's rN, G's c (why) */. Noted sites are listed but do not fail --check.

The report's `callees` section lists, per callee, the registers readable callers read that
its contract leaves out: what the lifter's contract builder (which reads only the original
assembly) does not see.
"""
from __future__ import annotations

import argparse
import bisect
import functools
import glob
import json
import os
import re
import sys
from dataclasses import asdict, dataclass, field

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lint  # noqa: E402  # pyright: ignore[reportImplicitRelativeImport]  (tools/abi/lint.py, run as a script)
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import gamepaths  # noqa: E402  # tools/gamepaths.py: the game profile (tools/paths.env)

ROOT = lint.ROOT
DEFAULT_PATHS = lint.DEFAULT_PATHS
LIFTED_DIR = os.path.join(ROOT, "cport/src/lifted")
LIFTED_H = lint.LIFTED_H
DEFAULT_GRAPH = gamepaths.ABI_GRAPH

REGS = [f"r{n}" for n in range(13)]
FLAGS = ["nz", "c", "v"]
HEADER_RE = re.compile(
    r"^/\* (\w+): ([0-9A-F]{8})-([0-9A-F]{8}) [^\n]*\*/\n"
    r"/\*   inputs:  ([^\n]*)\n \*   outputs: ([^\n]*)\n"
    r" \*   clobbers \(unused by callers\): ([^\n]*)\n \*   preserved: ([^\n]*?) \*/", re.M)
PROTO_RE = re.compile(r"^Next\s+(\w+)\s*\(\s*void\s*\)\s*;\s*/\*\s*([0-9A-F]{8})", re.M)
NOTE_RE = re.compile(r"relies on (\w+) leaving ((?:(?:r1[0-2]|r\d|nz|c|v)[\s,/]*(?:and\s+)?)+)", re.I)
UNUSED_RE = re.compile(r"leftover unused:\s*((?:\w+'s\s+(?:r1[0-2]|r\d|nz|c|v)\b[\s,;]*(?:and\s+)?)+)", re.I)
TOKEN_RE = re.compile(r"\w+|<<=|>>=|==|!=|<=|>=|&&|\|\||->|\+\+|--|<<|>>|[-+*/%&|^]=|\S")
REG_TOKEN = re.compile(r"^R(1[0-2]|[0-9])$")
INSN_RE = re.compile(r"^\s*/\* [0-9A-F]{8}: (.*?) \*/$", re.M)
FLAG_BRANCH = re.compile(r"^b(eq|ne|cs|cc|hs|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)\b")
BRANCH_FLAGS = {"eq": "nz", "ne": "nz", "mi": "nz", "pl": "nz", "cs": "c", "cc": "c", "hs": "c", "lo": "c",
                "vs": "v", "vc": "v", "hi": "c", "ls": "c", "ge": "v", "lt": "v", "gt": "v", "le": "v"}
FLAG_READS = {"ZERO_FLAG": "nz", "NEGATIVE_FLAG": "nz", "CARRY_FLAG": "c", "OVERFLOW_FLAG": "v",
              "FLAG_Z": "nz", "FLAG_N": "nz", "FLAG_C": "c", "FLAG_V": "v"}
FLAG_SETTERS = {"SET_FLAGS_NZ": ("nz",), "SET_FLAGS_CMP": ("nz", "c", "v"), "SET_FLAGS_ADD": ("nz", "c", "v")}
ENDS_PATH = {"RETURN_TO_CALLER", "RETURN_TO", "return"}
EXIT = "<exit>"                  # Read.into for flags that reach the function's return
COND_KEYWORDS = {"if", "while", "for", "switch"}


def names(text: str) -> set[str]:
    return {t for t in text.split() if t != "-"}


@functools.lru_cache(maxsize=8)
def _read(path: str) -> str:
    return open(path).read()


@dataclass
class Contract:
    """One lifted unit's contract (all its entries share it)."""
    unit: str
    start: int
    end: int
    inputs: set[str]
    outputs: set[str]
    clobbers: set[str]
    preserved: set[str]
    must_write: set[str] | None = None      # from graph.json, when there
    source: tuple[str, int, int] = ("", 0, 0)   # its lifted C: file, start and end offsets

    def after_calls(self, callee: str) -> list[str]:
        """The original's instruction after each `bl CALLEE` in this unit (from the lifted C's
        per-instruction comments)."""
        path, a, b = self.source
        if not path:
            return []
        insns = INSN_RE.findall(_read(path)[a:b])
        return [insns[k + 1] if k + 1 < len(insns) else "" for k, ins in enumerate(insns)
                if re.match(rf"bl\s+{re.escape(callee)}\b", ins)]

    @property
    def leaves(self) -> set[str]:
        """What a call may change: everything it writes except what it restores."""
        return (self.outputs | self.clobbers) - self.preserved


@dataclass
class Contracts:
    units: list[Contract]
    by_entry: dict[str, Contract]           # every entry name -> its unit's contract
    site_live: dict[tuple[int, int], set[str]] = field(default_factory=dict)   # (caller unit, callee unit)
    have_graph: bool = False

    def of(self, name: str) -> Contract | None:
        """The contract of the unit NAME (an entry; for a readable function: what it replaces)."""
        return self.by_entry.get(name)


def load_contracts(lifted_dir: str = LIFTED_DIR, lifted_h: str = LIFTED_H,
                   graph: str | None = DEFAULT_GRAPH) -> Contracts:
    """Contracts from the lifted headers (current names), entry names from lifted.h, and,
    from graph.json when it exists, per call site liveness and must-write sets (by address,
    so renames since the export do not matter)."""
    units: list[Contract] = []
    for path in sorted(glob.glob(os.path.join(lifted_dir, "*.c"))):
        found = list(HEADER_RE.finditer(open(path).read()))
        for k, m in enumerate(found):
            end = found[k + 1].start() if k + 1 < len(found) else -1
            units.append(Contract(m[1], int(m[2], 16), int(m[3], 16), names(m[4]), names(m[5]),
                                  names(m[6]), names(m[7]), source=(path, m.end(), end)))
    units.sort(key=lambda u: u.start)
    starts = [u.start for u in units]

    def unit_at(addr: int) -> Contract | None:
        k = bisect.bisect_right(starts, addr & ~1) - 1
        return units[k] if k >= 0 and units[k].start <= (addr & ~1) < units[k].end else None

    by_entry = {u.unit: u for u in units}
    if os.path.exists(lifted_h):
        for m in PROTO_RE.finditer(open(lifted_h).read()):
            u = unit_at(int(m[2], 16))
            if u is not None:
                by_entry.setdefault(m[1], u)
    c = Contracts(units, by_entry)
    if graph and os.path.exists(graph):
        g = json.load(open(graph))
        addr_of = {name: int(e["addr"], 16) for name, e in g["entries"].items()}
        for name, e in g["entries"].items():
            u = unit_at(addr_of[name])
            if u is not None and u.start == addr_of[name] and "must_write" in e:
                u.must_write = set(e["must_write"])
        for s in g["sites"]:
            if s.get("kind") not in ("call", "tail") or s.get("target") not in addr_of:
                continue
            cu, tu = unit_at(int(s["addr"], 16)), unit_at(addr_of[s["target"]])
            if cu is None or tu is None:
                continue
            c.site_live.setdefault((cu.start, tu.start), set()).update(s.get("live_after", []))
        c.have_graph = True
    return c


# ── the readable side ───────────────────────────────────────────────────

@dataclass
class Block:
    open: int            # token index of {
    close: int           # token index of the matching }
    kind: str            # if | else | while | for | do | switch | plain
    head: int            # token index where its header starts (the keyword), or open


@dataclass
class Scope:
    """A block or brace-less body the scan entered after the call."""
    start: int
    end: int             # its last token (a block's })
    kind: str            # a Block kind, or cond (a brace-less if/else/loop body)
    brace_less: bool
    kills: set[str] = field(default_factory=set)


@dataclass
class Read:
    reg: str
    callee: str
    line: int
    kind: str            # contract | site | passed | zflag (ok: an allowed read, kept for the zflag check)
    into: str | None = None


class Body:
    """A function body as tokens (comments and strings already blanked by lint.strip)."""

    def __init__(self, text: str, first_line: int):
        self.toks: list[str] = []
        self.lines: list[int] = []
        for m in TOKEN_RE.finditer(text):
            self.toks.append(m.group())
            self.lines.append(first_line + text.count("\n", 0, m.start()))
        self.match: dict[int, int] = {}
        stack: list[int] = []
        for i, t in enumerate(self.toks):
            if t in "({[":
                stack.append(i)
            elif t in ")}]" and stack:
                j = stack.pop()
                self.match[i], self.match[j] = j, i
        self.blocks: dict[int, Block] = {}
        for i, t in enumerate(self.toks):
            if t == "{" and i in self.match:
                self.blocks[i] = self._block(i)
        # `else` after a brace-less if body -> where that body starts: a call in the body
        # never runs the else branch (`if (k == 0) GUEST_CALL(f); else GUEST_CALL(g);`)
        self.else_after_body: dict[int, int] = {}
        for i, t in enumerate(self.toks):
            if t == "if" and i + 1 < len(self.toks) and self.toks[i + 1] == "(" and (i + 1) in self.match:
                body = self.match[i + 1] + 1
                if body < len(self.toks) and self.toks[body] != "{":
                    end = self.statement_end(body)
                    if end + 1 < len(self.toks) and self.toks[end + 1] == "else":
                        self.else_after_body[end + 1] = body

    def _block(self, i: int) -> Block:
        prev = self.toks[i - 1] if i else ""
        if prev == ")" and (i - 1) in self.match:
            j = self.match[i - 1]
            kw = self.toks[j - 1] if j else ""
            if kw in COND_KEYWORDS:
                return Block(i, self.match[i], kw, j - 1)
        if prev in ("else", "do"):
            return Block(i, self.match[i], prev, i - 1)
        return Block(i, self.match[i], "plain", i)

    def enclosing(self, i: int) -> list[Block]:
        """Blocks around token i, outermost first."""
        return sorted((b for b in self.blocks.values() if b.open < i < b.close), key=lambda b: b.open)

    def statement_end(self, i: int) -> int:
        """Index of the last token of the statement starting at token i."""
        t, n = self.toks, len(self.toks)
        if i >= n:
            return n - 1
        if t[i] == "{":
            return self.match.get(i, n - 1)
        if t[i] in COND_KEYWORDS and i + 1 < n and t[i + 1] == "(":
            end = self.statement_end(self.match.get(i + 1, n - 2) + 1)
            if t[i] == "if" and end + 1 < n and t[end + 1] == "else":
                end = self.statement_end(end + 2)
            return end
        if t[i] == "do":
            end = self.statement_end(i + 1) + 1                 # `while`
            return self.statement_end(end) if end < n else n - 1
        depth = 0
        while i < n:
            if t[i] in "([{":
                depth += 1
            elif t[i] in ")]}":
                depth -= 1
            elif t[i] == ";" and depth <= 0:
                return i
            i += 1
        return n - 1

    def skip_else(self, i: int) -> int:
        """Past an else-chain starting at token i (the branches a taken if-branch skips)."""
        while i < len(self.toks) and self.toks[i] == "else":
            i += 1
            if self.toks[i] == "if":
                i = self.match[i + 1] + 1
            i = (self.match[i] if self.toks[i] == "{" else self.statement_end(i)) + 1
        return i


class Scanner:
    """Follows the registers and flags one call leaves, through one readable body."""

    def __init__(self, body: Body, contracts: Contracts, caller_unit: Contract | None, deep: bool = False):
        self.b = body
        self.c = contracts
        self.caller_unit = caller_unit
        self.deep = deep              # --deep: also the reads in a `return` expression
        # the flags this function leaves reach a caller that reads them
        self.flags_out = caller_unit is not None and "nz" in caller_unit.outputs

    def calls(self) -> list[tuple[int, int, str]]:
        """(index of GUEST_CALL, index after its `;`, callee) for every direct guest call."""
        t = self.b.toks
        out = []
        for i in range(len(t) - 4):
            if t[i] == "GUEST_CALL" and t[i + 1] == "(" and t[i + 3] == ")":
                end = i + 4 + (t[i + 4] == ";")
                out.append((i, end, t[i + 2]))
        return out

    def classify(self, reg: str, callee: Contract) -> str | None:
        if reg not in callee.outputs:
            return "contract"
        if self.caller_unit is not None and self.c.have_graph:
            live = self.c.site_live.get((self.caller_unit.start, callee.start))
            if live is not None and reg not in live:
                return "site"
        return None

    def follow(self, call_at: int, start: int, callee_name: str, every: bool = False) -> list[Read]:
        """The reads of what this call leaves that break its contract (EVERY: all of them)."""
        callee = self.c.of(callee_name)
        if callee is None:
            return []
        pending = {r for r in callee.leaves if r in REGS or r in FLAGS}
        reads: list[Read] = []
        stack = self.b.enclosing(call_at)
        self._scan(start, len(self.b.toks), pending, stack, callee, reads, call_at)
        # the original branches on the flags right after every call (`bl f; beq x`), the C tests
        # r0 and reads no flag: right only if f's flags always mean "r0 == 0", which is not so for
        # e.g. TestEventFlag (Z = flag clear, r0 = the flag's whole byte)
        plain = [r for r in reads if r.into is None]
        branch = self.branches_after(callee_name)
        if (branch and any(r.reg == "r0" and r.kind == "ok" for r in plain)
                and not any(r.reg in branch for r in plain)):
            first = next(r for r in plain if r.reg == "r0")
            reads.append(Read("r0", callee.unit, first.line, "zflag"))
        return [r for r in reads if (every or r.kind != "ok") and r.into != EXIT]

    def branches_after(self, callee_name: str) -> set[str]:
        """The flags the original of this caller branches on right after calling CALLEE
        (empty unless it does so after every such call)."""
        if self.caller_unit is None:
            return set()
        after = self.caller_unit.after_calls(callee_name)
        conds = [FLAG_BRANCH.match(a) for a in after]
        if not after or not all(conds):
            return set()
        return {BRANCH_FLAGS[m[1]] for m in conds if m}

    def _hit(self, reg: str, i: int, callee: Contract, reads: list[Read], pending: set[str],
             into: str | None = None) -> None:
        pending.discard(reg)
        kind = self.classify(reg, callee)
        if kind is not None and into:
            kind = "passed"
        reads.append(Read(reg, callee.unit, self.b.lines[i], kind or "ok", into))

    def _next_pass(self, loop: Block, pending: set[str], callee: Contract, reads: list[Read],
                   call_at: int) -> None:
        """A call inside a loop: its leftovers reach the loop's condition and the body's
        start on the next pass (a path of its own: its writes do not end the main one)."""
        before = len(reads)
        parents = self.b.enclosing(loop.head)
        mine = set(pending)
        if loop.head < loop.open:                       # while (...) / for (...)
            self._scan(loop.head + 1, loop.open, mine, parents, callee, reads, call_at, back_edge=True)
        self._scan(loop.open + 1, call_at, mine, parents + [loop], callee, reads, call_at, back_edge=True)
        pending.difference_update(r.reg for r in reads[before:])

    def _scan(self, i: int, stop: int, pending: set[str], stack: list[Block], callee: Contract,
              reads: list[Read], call_at: int, back_edge: bool = False) -> None:
        """Follow PENDING (what the call left) from token i. Writes end a register's trail in
        the scope they are made in: for the rest of the path in the call's own block (and
        plain and do blocks), only to the end of the branch in an if, loop or brace-less body."""
        t = self.b.toks
        scopes: list[Scope] = []      # blocks and brace-less bodies opened after the call
        kills: list[str] = []         # plain writes, applied at the end of their statement
        stack = list(stack)
        done_loops: set[int] = set()

        def live(reg: str) -> bool:
            return reg in pending and not any(reg in sc.kills for sc in scopes if sc.start <= i)

        def kill(regs: set[str] | list[str]) -> None:
            active = [sc for sc in scopes if sc.start <= i]
            if active:
                active[-1].kills.update(regs)
            else:
                pending.difference_update(regs)

        def branching() -> bool:
            return any(sc.start <= i and sc.kind not in ("plain", "do") for sc in scopes)

        while i < stop and pending:
            while scopes and scopes[-1].brace_less and i > scopes[-1].end:
                scopes.pop()
            tok = t[i]
            if tok == "{" and i in self.b.blocks:
                blk = self.b.blocks[i]
                scopes.append(Scope(i, blk.close, blk.kind, False))
            elif tok == "}":
                if scopes and not scopes[-1].brace_less and scopes[-1].end == i:
                    sc = scopes.pop()
                    if sc.kind in ("plain", "do"):      # always runs: its writes stand
                        kill(sc.kills)
                    i += 1
                    continue
                if not stack:                           # leaving a block around the call
                    return
                blk = stack.pop()
                if blk.kind in ("while", "for", "do") and blk.open not in done_loops and not back_edge:
                    done_loops.add(blk.open)
                    self._next_pass(blk, pending, callee, reads, call_at)
                if not stack:                           # the function's own body
                    return
                if blk.kind == "if" and i + 1 < len(t) and t[i + 1] == "else":
                    i = self.b.skip_else(i + 1)
                    continue
            elif tok in COND_KEYWORDS and i + 1 < len(t) and t[i + 1] == "(":
                close = self.b.match.get(i + 1, i + 1)
                if close + 1 < len(t) and t[close + 1] != "{" and tok != "switch":
                    scopes.append(Scope(close + 1, self.b.statement_end(close + 1), "cond", True))
            elif tok == "else" and self.b.else_after_body.get(i, i) <= call_at < i:
                i = self.b.skip_else(i)                 # the call was in the if's brace-less body
                continue
            elif tok == "else" and i + 1 < len(t) and t[i + 1] not in ("{", "if"):
                scopes.append(Scope(i + 1, self.b.statement_end(i + 1), "cond", True))
            elif tok == ";":
                if kills:
                    kill(kills)
                kills = []
            elif tok in ENDS_PATH:
                if self.flags_out and live("nz") and not any(r.into == EXIT for r in reads):
                    reads.append(Read("nz", callee.unit, self.b.lines[i], "ok", EXIT))   # reaches its callers
                if not branching():
                    if tok == "return":                 # a tail call reads its callee's inputs
                        if i + 3 < len(t) and t[i + 2] == "(" and t[i + 3] == ")":
                            nxt = self.c.of(t[i + 1])
                            if nxt is not None:
                                for r in sorted(nxt.inputs):
                                    if live(r):
                                        self._hit(r, i, callee, reads, pending, into=t[i + 1])
                        elif self.deep:                 # `return R0 != 0;` reads before the path ends
                            for k in range(i + 1, self.b.statement_end(i)):
                                x = t[k]
                                if REG_TOKEN.match(x) and live(x.lower()):
                                    self._hit(x.lower(), k, callee, reads, pending)
                                elif x in FLAG_READS and live(FLAG_READS[x]):
                                    self._hit(FLAG_READS[x], k, callee, reads, pending)
                                elif x == "cpu" and k + 2 < len(t) and t[k + 1] == "." and t[k + 2] in FLAGS \
                                        and live(t[k + 2]):
                                    self._hit(t[k + 2], k, callee, reads, pending)
                    return
            elif tok == "goto" and not branching():
                return
            elif tok in ("break", "continue") and not branching():
                loops = [k for k, b in enumerate(stack) if b.kind in ("while", "for", "do", "switch")]
                if loops and not scopes:
                    k = loops[-1]
                    blk = stack[k]
                    if tok == "continue" and blk.kind != "switch" and blk.open not in done_loops and not back_edge:
                        done_loops.add(blk.open)
                        self._next_pass(blk, pending, callee, reads, call_at)
                    del stack[k:]
                    i = blk.close + 1
                    if not stack:
                        return
                    continue
            elif tok in ("GUEST_CALL_ADDR", "dispatch", "jump_to"):
                if not branching():
                    return                              # unknown callee: stop following
            elif tok == "GUEST_CALL" and i + 3 < len(t) and t[i + 1] == "(":
                nxt = self.c.of(t[i + 2])
                if nxt is not None:
                    for r in sorted(nxt.inputs):
                        if live(r):
                            self._hit(r, i, callee, reads, pending, into=t[i + 2])
                    if nxt.must_write is not None:
                        kill(nxt.must_write - nxt.preserved)
                i += 4
                continue
            elif REG_TOKEN.match(tok):
                reg = tok.lower()
                if i + 1 < len(t) and t[i + 1] == "=":
                    kills.append(reg)
                elif live(reg):
                    self._hit(reg, i, callee, reads, pending)
            elif tok == "cpu" and i + 2 < len(t) and t[i + 1] == "." and t[i + 2] in FLAGS:
                flag = t[i + 2]
                if i + 3 < len(t) and t[i + 3] == "=":
                    kills.append(flag)
                elif live(flag):
                    self._hit(flag, i, callee, reads, pending)
                i += 3
                continue
            elif tok in FLAG_READS and live(FLAG_READS[tok]):
                self._hit(FLAG_READS[tok], i, callee, reads, pending)
            elif tok in FLAG_SETTERS:
                kills.extend(FLAG_SETTERS[tok])
            i += 1


def notes_of(text: str) -> set[tuple[str, str]]:
    """(callee, register) pairs that a `register form: relies on F leaving rN` or a
    `leftover unused: F's rN` note accepts."""
    out: set[tuple[str, str]] = set()
    for m in NOTE_RE.finditer(text):
        for reg in re.findall(r"r1[0-2]|r\d|nz|c|v", m[2]):
            out.add((m[1], reg))
    for m in UNUSED_RE.finditer(text):
        out.update(re.findall(r"(\w+)'s\s+(r1[0-2]|r\d|nz|c|v)\b", m[1]))
    return out


@dataclass
class Site:
    """One leftover read in the readable code."""
    file: str
    line: int
    function: str
    call_line: int
    callee: str
    callee_unit: str
    reg: str
    kind: str
    into: str | None
    noted: bool


# the `── NAME ──` line tools/layout.py starts each function's part with
PART_RE = re.compile(r"^/\* ── (\w+) ── \*/", re.M)


def part_unit(contracts: Contracts, parts: list[tuple[int, str]], at: int) -> Contract | None:
    """For a helper (no lifted unit of its own): the unit of the entry whose part holds it, whose
    original made the calls the helper now makes (layout keeps a helper in the part that uses it)."""
    name = None
    for start, entry in parts:
        if start <= at:
            name = entry
    return contracts.of(name) if name else None


def scan(paths: list[str], contracts: Contracts, deep: bool = False) -> list[Site]:
    """Every leftover read in the readable code under PATHS. DEEP (--deep): helpers are read with
    the original of the entry whose part holds them (site liveness, the zflag rule), and reads in
    a `return` expression count (`GUEST_CALL(F); return R0 != 0;`, a helper's TestEventFlag bug)."""
    files: list[str] = []
    for p in paths:
        files += sorted(glob.glob(os.path.join(p, "*.c"))) if os.path.isdir(p) else [p]
    sites: list[Site] = []
    for path in files:
        raw = open(path).read()
        if os.path.basename(path) == "abi_thunks.c" or lint.GENERATED_RE.search("\n".join(raw.split("\n")[:3])):
            continue
        _, comments = lint.strip(raw)
        parts = [(m.start(), m.group(1)) for m in PART_RE.finditer(raw)]
        prev_end = 0
        for f in lint.scan_file(os.path.basename(path), raw):
            if f.kind == "file":
                continue
            # the function's comments: inside it, and the comment block right before it
            note_text = "\n".join(c for a, b, c in comments if prev_end <= a and b <= f.body[1] + 1)
            prev_end = f.body[1] + 1
            accepted = notes_of(note_text)
            body = Body(f.text, f.body_line)
            unit = contracts.of(f.name) or (part_unit(contracts, parts, f.body[0]) if deep else None)
            sc = Scanner(body, contracts, unit, deep)
            for call_at, start, callee in sc.calls():
                for r in sc.follow(call_at, start, callee):
                    sites.append(Site(os.path.relpath(path, ROOT), r.line, f.name, body.lines[call_at], callee,
                                      r.callee, r.reg, r.kind, r.into,
                                      (callee, r.reg) in accepted or (r.callee, r.reg) in accepted))
    return sites


def reads_after_calls(function: str, contracts: Contracts | None = None,
                      paths: list[str] = DEFAULT_PATHS) -> dict[str, set[str]]:
    """What one readable function reads (or passes on into its next callee) of what each of its
    callees leaves, by callee name: the registers and flags whose narrowing would reach it.
    Empty when FUNCTION is not in the readable code. For tools/stamps.py's "callee narrowed"
    rule: re-verify when a dropped output is in this set."""
    contracts = contracts or load_contracts()
    files: list[str] = []
    for p in paths:
        files += sorted(glob.glob(os.path.join(p, "*.c"))) if os.path.isdir(p) else [p]
    out: dict[str, set[str]] = {}
    for path in files:
        raw = open(path).read()
        if not re.search(rf"\b{re.escape(function)}\s*\(", raw):
            continue
        for f in lint.scan_file(os.path.basename(path), raw):
            if f.name != function or f.kind == "file":
                continue
            sc = Scanner(Body(f.text, f.body_line), contracts, contracts.of(f.name))
            for call_at, start, callee in sc.calls():
                out.setdefault(callee, set()).update(r.reg for r in sc.follow(call_at, start, callee, every=True)
                                                     if r.kind != "zflag")
            return out
    return out


def callees_summary(sites: list[Site]) -> dict[str, dict[str, list[str]]]:
    """Per callee: what readable callers read beyond its contract (contract) or beyond what
    their own originals read (site, passed), and who. The contract builder sees none of it:
    it reads only the original assembly, so narrowing these callees could break the callers."""
    out: dict[str, dict[str, list[str]]] = {}
    for s in sites:
        if s.kind == "zflag":
            continue
        who = out.setdefault(s.callee_unit, {}).setdefault(f"{s.reg} ({s.kind})", [])
        if s.function not in who:
            who.append(s.function)
    return dict(sorted(out.items()))


def main() -> int:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("paths", nargs="*", default=DEFAULT_PATHS, help=".c files or directories")
    ap.add_argument("--graph", default=DEFAULT_GRAPH, help="lift.py --export-abi output (per site liveness)")
    ap.add_argument("--check", action="store_true", help="exit 1 on a read without a note (zflag: listed only)")
    ap.add_argument("--quiet", action="store_true", help="no summary on stderr")
    ap.add_argument("--deep", action="store_true",
                    help="also helpers (with their part's original) and `return` expressions: catches a helper's "
                         "`return R0 != 0` after TestEventFlag (the mutation survey, docs/reviews/2026-10-11-"
                         "mutation-survey.md); not the gate's default until its open sites are triaged")
    ap.add_argument("--reads", metavar="FUNCTION",
                    help="only list what FUNCTION reads after each of its calls (JSON: callee -> registers)")
    args = ap.parse_args()
    contracts = load_contracts(graph=args.graph)
    if not contracts.units:
        print(f"no lifted function headers in {LIFTED_DIR}: lift first", file=sys.stderr)
        return 2
    if args.reads:
        reads = reads_after_calls(args.reads, contracts, args.paths)
        json.dump({c: sorted(r, key=lambda x: (len(x), x)) for c, r in reads.items()}, sys.stdout, indent=1)
        print()
        return 0
    sites = scan(args.paths, contracts, args.deep)
    open_sites = [s for s in sites if not s.noted]
    failing = [s for s in open_sites if s.kind != "zflag"]
    totals = {k: sum(s.kind == k for s in open_sites) for k in ("contract", "site", "passed", "zflag")}
    report = {"graph": os.path.relpath(args.graph, ROOT) if contracts.have_graph else None,
              "totals": totals, "noted": sum(s.noted for s in sites),
              "sites": [asdict(s) for s in sites], "callees": callees_summary(sites)}
    json.dump(report, sys.stdout, indent=1)
    print()
    if not args.quiet:
        if not contracts.have_graph:
            print(f"leftovers: no {args.graph}: only callee contracts, no per-site check", file=sys.stderr)
        for s in open_sites:
            via = f" (passed into {s.into})" if s.into else ""
            print(f"{s.file}:{s.line} {s.function}: reads {s.reg} after {s.callee} "
                  f"(line {s.call_line}): {s.kind}{via}", file=sys.stderr)
        print(f"leftovers: {len(open_sites)} reads ({totals['contract']} contract, {totals['site']} site, "
              f"{totals['passed']} passed on, {totals['zflag']} r0 for Z), {report['noted']} noted",
              file=sys.stderr)
    return 1 if args.check and failing else 0


if __name__ == "__main__":
    sys.exit(main())
