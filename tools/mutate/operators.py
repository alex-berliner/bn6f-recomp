"""Mutation operators: small semantic changes of the kinds of bugs this project has had.

Each operator looks at one function body (tokens of the blanked text, tools/mutate/csrc.py) and
proposes edits (offset range, replacement). The classes follow the bug history (git log, the farm's
failure logs, symbols/pool_drops.tsv, docs/PROCESS.md section 7):

  flag_leftover  a callee's flag answer read from its leftover r0     (67673ff, e988740, 569a765)
  flags          flag outputs dropped, swapped or of the wrong kind    (ec5c0ac; 318 farm rounds)
  registers      adapter/mirroring: wrong or missing register         (b86ffb0; 976 farm rounds)
  return_target  returns somewhere other than the caller              (b86ffb0, 80d5a92)
  sign_width     sign extension or truncation lost                    (4d2bee5; oracle drops)
  offset         wrong struct field offset or index stride            (sub_813DA94, lockstep)
  store          dropped store, wrong store or load width             (529 farm memory diffs)
  boundary       off-by-one constant or comparison, negated test      (f554532, 2a173b0)
  classic        &&/||, +/-, a dropped call (textbook operators, for comparison)
"""
from __future__ import annotations

import re
from collections.abc import Iterator
from dataclasses import dataclass

from csrc import CFile, Function, Token, tokens

REG = re.compile(r"^R(\d|1[0-2])$")
OFFSET_MACRO = re.compile(r"^o[A-Z]\w*_\w+$")
MEM = {"LD8": 1, "LD16": 2, "LD32": 4, "LDS8": 1, "LDS16": 2, "ST8": 1, "ST16": 2, "ST32": 4}
FLAG_SETTERS = ("SET_FLAGS_NZ", "SET_FLAGS_CMP", "SET_FLAGS_ADD")
NOT_CALLS = {"if", "while", "for", "switch", "return", "sizeof", "RETURN_TO_CALLER", "RETURN_TO", *MEM,
             *FLAG_SETTERS, "ZERO_FLAG", "CARRY_FLAG", "NEGATIVE_FLAG", "OVERFLOW_FLAG"}
STATEMENT_START_AFTER = {";", "{", "}", ":", "else"}

CLASSES = ("flag_leftover", "flags", "registers", "return_target", "sign_width", "offset", "store",
           "boundary", "classic")


@dataclass(frozen=True)
class Edit:
    cls: str
    op: str
    function: str          # the C function the edit is in
    start: int
    end: int
    replacement: str
    note: str              # "LDS16 -> LD16"

    def key(self) -> str:
        return f"{self.function}:{self.start}:{self.op}"


class Body:
    """One function body's tokens with the context the operators need."""

    def __init__(self, cfile: CFile, fn: Function):
        self.cfile = cfile
        self.fn = fn
        self.toks: list[Token] = tokens(cfile.code, fn.body_open + 1, fn.body_close)
        self.close: dict[int, int] = {}       # token index of an opening bracket -> its closing one
        self.open: dict[int, int] = {}        # and back
        stack: list[int] = []
        for i, t in enumerate(self.toks):
            if t.text in "([{" and t.kind == "op":
                stack.append(i)
            elif t.text in ")]}" and t.kind == "op" and stack:
                self.close[stack[-1]] = i
                self.open[i] = stack.pop()
        # tokens inside the address argument of LD*/ST*: index -> access width
        self.address_width: dict[int, int] = {}
        for i, t in enumerate(self.toks):
            if t.text in MEM and i + 1 < len(self.toks) and self.toks[i + 1].text == "(":
                lo = i + 2
                hi = self.close.get(i + 1, lo)
                if t.text.startswith("ST"):
                    hi = self.top_level_comma(lo, hi) or hi
                for k in range(lo, hi):
                    self.address_width.setdefault(k, MEM[t.text])

    def top_level_comma(self, lo: int, hi: int) -> int | None:
        depth = 0
        for k in range(lo, hi):
            x = self.toks[k].text
            if x in "([{":
                depth += 1
            elif x in ")]}":
                depth -= 1
            elif x == "," and depth == 0:
                return k
        return None

    def text(self, lo: int, hi: int) -> str:
        """Source text of tokens lo..hi-1 (the original text, comments inside kept)."""
        return self.cfile.text[self.toks[lo].start:self.toks[hi - 1].end]

    def statement_start(self, i: int) -> bool:
        if i == 0:
            return True
        prev = self.toks[i - 1].text
        if prev in STATEMENT_START_AFTER:
            return True
        if prev == ")":
            # the end of an if/while/for condition
            open_i = self.open.get(i - 1)
            return open_i is not None and open_i > 0 and self.toks[open_i - 1].text in ("if", "while", "for")
        return False

    def statement_end(self, i: int) -> int | None:
        """Token index of the `;` ending the statement that starts at I (not crossing a block)."""
        depth = 0
        for k in range(i, len(self.toks)):
            x = self.toks[k].text
            if x in "([":
                depth += 1
            elif x in ")]":
                depth -= 1
            elif x in "{}":
                return None
            elif x == ";" and depth == 0:
                return k
        return None

    def edit(self, cls: str, op: str, lo: int, hi: int, replacement: str, note: str) -> Edit:
        """Replace tokens lo..hi-1."""
        return Edit(cls, op, self.fn.name, self.toks[lo].start, self.toks[hi - 1].end, replacement, note)

    def drop_statement(self, cls: str, op: str, i: int) -> Edit | None:
        if not self.statement_start(i):
            return None
        end = self.statement_end(i)
        if end is None:
            return None
        snippet = " ".join(self.text(i, end + 1).split())
        return self.edit(cls, op, i, end + 1, ";", f"drop `{snippet[:70]}`")


def is_register_code(fn: Function) -> bool:
    return fn.is_next or "register_form" in fn.name or fn.name.startswith("call_")


def proposals(cfile: CFile, fn: Function) -> Iterator[Edit]:
    b = Body(cfile, fn)
    toks = b.toks
    n = len(toks)
    for i, t in enumerate(toks):
        x = t.text
        nxt = toks[i + 1].text if i + 1 < n else ""
        prev = toks[i - 1].text if i else ""
        # ── flag_leftover: the flag answer taken from r0 (what a caller of TestEventFlag got wrong)
        if x == "ZERO_FLAG" and nxt == "(" and i + 2 < n and toks[i + 2].text == ")":
            yield b.edit("flag_leftover", "zero_flag_from_r0", i, i + 3, "(R0 == 0)", "ZERO_FLAG() -> (R0 == 0)")
        # ── flags
        if x in FLAG_SETTERS and nxt == "(":
            drop = b.drop_statement("flags", "flags_drop", i)
            if drop:
                yield drop
            close = b.close.get(i + 1)
            comma = b.top_level_comma(i + 2, close) if close else None
            if close and comma and x in ("SET_FLAGS_CMP", "SET_FLAGS_ADD"):
                a, c = b.text(i + 2, comma), b.text(comma + 1, close)
                if x == "SET_FLAGS_CMP":
                    yield b.edit("flags", "flags_cmp_swap", i, close + 1, f"SET_FLAGS_CMP({c}, {a})",
                                 "SET_FLAGS_CMP(a, b) -> (b, a)")
                sign = "-" if x == "SET_FLAGS_CMP" else "+"
                yield b.edit("flags", "flags_kind", i, close + 1, f"SET_FLAGS_NZ(({a}) {sign} ({c}))",
                             f"{x} -> SET_FLAGS_NZ (C and V lost)")
        if x == "cpu" and nxt == "." and i + 3 < n and toks[i + 2].text in ("c", "v", "nz") and toks[i + 3].text == "=":
            drop = b.drop_statement("flags", "flags_drop", i)
            if drop:
                yield drop
        # ── registers (adapters, register-form helpers, register bodies)
        m = REG.match(x)
        if m and t.kind == "id" and is_register_code(fn):
            r = int(m.group(1))
            other = r + 1 if r < 12 else r - 1
            if nxt == "=" and b.statement_start(i):
                if fn.is_next:
                    yield b.edit("registers", "reg_out_swap", i, i + 1, f"R{other}", f"R{r} = ... -> R{other} = ...")
                drop = b.drop_statement("registers", "reg_set_drop", i)
                if drop:
                    yield drop
            elif nxt not in ("=", "++", "--", "+=", "-=", "|=", "&=", "^=", "<<=", ">>=") and prev not in ("++", "--", "&"):
                yield b.edit("registers", "reg_in_swap", i, i + 1, f"R{other}", f"reads R{r} -> R{other}")
        # ── return_target
        if x == "RETURN_TO_CALLER" and nxt == "(" and i + 2 < n and toks[i + 2].text == ")":
            yield b.edit("return_target", "ret_target", i, i + 3, "RETURN_TO(LR + 4)",
                         "RETURN_TO_CALLER() -> RETURN_TO(LR + 4)")
        # ── sign_width
        if x in ("LDS16", "LDS8") and nxt == "(":
            yield b.edit("sign_width", "lds_unsigned", i, i + 1, x.replace("LDS", "LD"), f"{x} -> {x.replace('LDS', 'LD')}")
        if x == "(" and i + 2 < n and toks[i + 2].text == ")" and toks[i + 1].text in ("s8", "s16", "u8", "u16", "s32"):
            ty = toks[i + 1].text
            after = toks[i + 3] if i + 3 < n else None
            if after is not None and (after.kind in ("id", "num") or after.text == "("):   # a cast
                if ty in ("s8", "s16"):
                    yield b.edit("sign_width", "cast_unsigned", i + 1, i + 2, "u" + ty[1:], f"({ty}) -> (u{ty[1:]})")
                elif ty == "s32":
                    yield b.edit("sign_width", "drop_s32", i, i + 3, "", "(s32) cast dropped")
                else:
                    yield b.edit("sign_width", "drop_trunc", i, i + 3, "", f"({ty}) cast dropped")
        if x in ("u8", "u16", "s8", "s16") and b.statement_start(i) and i + 2 < n and toks[i + 1].kind == "id" \
                and toks[i + 2].text in ("=", ";"):
            wide = "s32" if x.startswith("s") else "u32"
            yield b.edit("sign_width", "widen_local", i, i + 1, wide, f"local {x} {toks[i + 1].text} -> {wide}")
        # ── offset (inside an LD*/ST* address)
        width = b.address_width.get(i)
        if width and t.kind == "num" and prev in ("+", "-") and not nxt in ("*",):
            value = int(re.sub(r"[uUlL]+$", "", x), 0)
            yield b.edit("offset", "offset_shift", i, i + 1, hex(value + width), f"address offset {x} -> {hex(value + width)}")
        if width and t.kind == "id" and OFFSET_MACRO.match(x):
            yield b.edit("offset", "offset_shift", i, i + 1, f"({x} + {width})", f"{x} -> {x} + {width}")
        if width and x in ("<<", "*") and nxt and toks[i + 1].kind == "num":
            k = int(re.sub(r"[uUlL]+$", "", nxt), 0)
            if x == "<<" and 1 <= k <= 4:
                yield b.edit("offset", "index_scale", i + 1, i + 2, str(k - 1 if k > 1 else 2), f"index << {k} -> << {k - 1 if k > 1 else 2}")
            elif x == "*" and k in (2, 4, 8, 16, 32):
                yield b.edit("offset", "index_scale", i + 1, i + 2, str(k // 2), f"index * {k} -> * {k // 2}")
        # ── store
        if x in ("ST8", "ST16", "ST32") and nxt == "(":
            drop = b.drop_statement("store", "store_drop", i)
            if drop:
                yield drop
            other = {"ST8": "ST16", "ST16": "ST8", "ST32": "ST16"}[x]
            yield b.edit("store", "store_width", i, i + 1, other, f"{x} -> {other}")
        if x in ("LD8", "LD16", "LD32") and nxt == "(":
            other = {"LD8": "LD16", "LD16": "LD8", "LD32": "LD16"}[x]
            yield b.edit("store", "load_width", i, i + 1, other, f"{x} -> {other}")
        # ── boundary
        if t.kind == "op" and x in ("<", "<=", ">", ">="):
            other = {"<": "<=", "<=": "<", ">": ">=", ">=": ">"}[x]
            yield b.edit("boundary", "rel_boundary", i, i + 1, other, f"{x} -> {other}")
        if t.kind == "op" and x in ("==", "!="):
            other = "!=" if x == "==" else "=="
            yield b.edit("boundary", "eq_negate", i, i + 1, other, f"{x} -> {other}")
        if t.kind == "num" and not width and prev not in ("<<", ">>", "[") and not (prev == "(" and i >= 2 and toks[i - 2].text in MEM):
            value = int(re.sub(r"[uUlL]+$", "", x), 0)
            suffix = re.search(r"[uUlL]*$", x).group(0)
            new = value + 1 if value == 0 or (t.start % 2 == 0) else value - 1   # deterministic per site
            yield b.edit("boundary", "const_adjust", i, i + 1, (hex(new) if x.lower().startswith("0x") else str(new)) + suffix,
                         f"constant {x} -> {new}")
        # ── classic
        if t.kind == "op" and x in ("&&", "||"):
            yield b.edit("classic", "logic_swap", i, i + 1, "||" if x == "&&" else "&&", f"{x} -> {'||' if x == '&&' else '&&'}")
        if t.kind == "op" and x in ("+", "-") and i and (toks[i - 1].kind in ("id", "num") or prev in (")", "]")):
            yield b.edit("classic", "arith_swap", i, i + 1, "-" if x == "+" else "+", f"{x} -> {'-' if x == '+' else '+'}")
        if t.kind == "id" and nxt == "(" and x not in NOT_CALLS and (x in cfile.functions or x in ("GUEST_CALL", "GUEST_CALL_ADDR")):
            drop = b.drop_statement("classic", "call_drop", i)
            if drop:
                yield drop


def apply(text: str, e: Edit) -> str:
    return text[:e.start] + e.replacement + text[e.end:]


# ── reach probes ───────────────────────────────────────────────────────

# A probe changes one spare byte of guest memory (nothing in bn6f lives above 0x0203F800), which
# every host check compares: a check that reports a difference for a probe ran the probed
# statement on an input it compared. On the GBA (lockstep) a probe hangs the game instead.
PROBE_HOST = "(*(volatile unsigned char *)(unsigned long)0x0203FFF0u += 1)"
PROBE_GBA = "({ for (;;) ; 0; })"


def probe_edit(cfile: CFile, fn: Function, start: int, probe: str = PROBE_HOST) -> Edit | None:
    """An edit that runs PROBE whenever the statement holding offset START runs (for a mutation in a
    `case` label: when that case is taken; in an if/while/switch condition: when it is evaluated)."""
    b = Body(cfile, fn)
    toks = b.toks
    i = next((k for k, t in enumerate(toks) if t.start == start), None)
    if i is None:
        return None
    depth = [0] * len(toks)
    d = 0
    for k, t in enumerate(toks):
        if t.text in ")]":
            d -= 1
        depth[k] = d
        if t.text in "([":
            d += 1
    # a case label: probe the statements it leads to
    k = i
    while k > 0 and toks[k - 1].text not in (";", "{", "}", ":"):
        k -= 1
    if toks[k].text == "case" and k <= i:
        colon = next((m for m in range(i, len(toks)) if toks[m].text == ":" and depth[m] == depth[k]), None)
        if colon is None:
            return None
        at = toks[colon].end
        return Edit("probe", "probe", fn.name, at, at, f" {probe};", "probe after the case label")
    j = i
    while j > 0 and not (depth[j] == 0 and b.statement_start(j)):
        j -= 1
    head = toks[j].text
    prev = toks[j - 1].text if j else "{"
    if head in ("if", "while", "switch") and j + 1 < len(toks) and toks[j + 1].text == "(":
        at = toks[j + 1].end
        return Edit("probe", "probe", fn.name, at, at, f"{probe}, ", f"probe in the {head} condition")
    if head in ("else", "do", "case", "default"):
        return None
    if prev in (";", "{", "}", ":") or j == 0:
        at = toks[j].start
        return Edit("probe", "probe", fn.name, at, at, f"{probe}; ", "probe before the statement")
    if head == "for":
        return None
    end = b.statement_end(j)
    if end is None:
        return None
    text = cfile.text[toks[j].start:toks[end].end]
    return Edit("probe", "probe", fn.name, toks[j].start, toks[end].end, f"{{ {probe}; {text} }}",
                "probe in braces around the statement")
