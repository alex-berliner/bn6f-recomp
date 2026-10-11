"""The names bn6f's assembly gives the constants lifted code uses (review R1-A22).

The lifter used to name a constant through a table from each address to the first label the ELF
has there, and to name an immediate only when the source line said `#NAME` with a value above 9.
Most RAM addresses carry several labels (a struct and its first field, the end of one object and
the start of the next), so hundreds of literal loads came out under another label than the one
the source loads (`.word eOWPlayerObjectEnd` as A_eOWPlayerObject_SpriteData_Size, `.word eS200BC50`
as A_eS200BC50_Index00), and struct offsets 0-9 and field references the disassembler's comment
names (`strb r0, [r1,#0x3] // (byte_200A00B - 0x200a008)`) came out as bare numbers.

Now the lifter writes what the source wrote, wherever that can be found and checked:
- a literal-pool load: the pool word's expression (`off_800B858: .word eOWPlayerObjectEnd`,
  found by the pool label's address), or the operand of `ldr rN, =EXPR`;
- an immediate operand or offset: the operand as written (`#oObjectSprite_Unk_06`,
  `#(OBJECT_FLAG_ACTIVE | OBJECT_FLAG_STOP_SPRITE_UPDATE)`); where it is a bare number and the
  comment gives the field (`// (byte_200A00B - 0x200a008)`), that expression;
- an `adr`: its operand.
An expression is used only when it evaluates, with the values bn6f's ELF gives its symbols, to the
value the instruction has; otherwise the lifter falls back to what it did before (the number, or
an address's label). A number the source writes stays a number. Labels become `A_label` and
assembler constants keep their names (the lifter defines both in bn6f_syms.h with the ELF's
values), so the compiled code is the same: only the spelling changes.
"""
from __future__ import annotations

import os
import re
import subprocess
from dataclasses import dataclass, field

from .elfinfo import SYM_RE, ElfInfo
from .emit import cname, h

# Names an assembler constant can't have in C: keywords, and names C headers define already.
C_KEYWORDS = {
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern",
    "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while",
    "_Bool", "_Static_assert", "bool", "true", "false", "NULL",
}

TOKEN = re.compile(r"\s*(?:(0[xX][0-9a-fA-F]+|0[bB][01]+|\d+)|([A-Za-z_.$][\w.$]*)|(<<|>>|[-+*/%|&^~()]))")
# GAS's binary operator precedence (as(1) "Infix Operators"), highest first; left to right within one
GAS_LEVELS = [("*", "/", "%", "<<", ">>"), ("|", "&", "^"), ("+", "-")]
# C's, for writing the parentheses back
C_PREC = {"*": 13, "/": 13, "%": 13, "+": 12, "-": 12, "<<": 11, ">>": 11, "&": 8, "^": 7, "|": 6}
UNARY_PREC = 14
ATOM_PREC = 16


@dataclass(frozen=True)
class Num:
    value: int
    text: str           # in C: decimal as written, hex (and binary) as upper-case hex


@dataclass(frozen=True)
class Sym:
    name: str


@dataclass(frozen=True)
class Unary:
    op: str
    x: Expr


@dataclass(frozen=True)
class Binary:
    op: str
    left: Expr
    right: Expr


Expr = Num | Sym | Unary | Binary


class ParseError(Exception):
    pass


def parse(text: str) -> Expr:
    """An assembler expression (numbers, symbols, + - * / % << >> | & ^ ~, parentheses)."""
    toks: list[tuple[str, str]] = []
    pos = 0
    text = text.strip()
    while pos < len(text):
        m = TOKEN.match(text, pos)
        if m is None or m.end() == pos:
            raise ParseError(text)
        num, ident, op = m.groups()
        toks.append(("num", num) if num else ("sym", ident) if ident else ("op", op))
        pos = m.end()
    at = 0

    def peek() -> tuple[str, str] | None:
        return toks[at] if at < len(toks) else None

    def take() -> tuple[str, str]:
        nonlocal at
        if at >= len(toks):
            raise ParseError(text)
        at += 1
        return toks[at - 1]

    def primary() -> Expr:
        kind, val = take()
        if kind == "num":
            v = int(val, 0)
            return Num(v, str(v) if val.isdigit() else f"0x{v:X}")
        if kind == "sym":
            return Sym(val)
        if val == "(":
            e = level(0)
            if take() != ("op", ")"):
                raise ParseError(text)
            return e
        if val in ("-", "~", "+"):
            x = primary()
            return x if val == "+" else Unary(val, x)
        raise ParseError(text)

    def level(k: int) -> Expr:
        if k == len(GAS_LEVELS):
            return primary()
        e = level(k + 1)
        while (t := peek()) is not None and t[0] == "op" and t[1] in GAS_LEVELS[len(GAS_LEVELS) - 1 - k]:
            _ = take()
            e = Binary(t[1], e, level(k + 1))
        return e

    e = level(0)
    if at != len(toks):
        raise ParseError(text)
    return e


@dataclass
class Rendered:
    c: str                                                  # the C expression
    labels: dict[str, int] = field(default_factory=dict)    # A_ name -> value (bn6f_syms.h)
    equates: dict[str, int] = field(default_factory=dict)   # constant name -> value (bn6f_syms.h)


class SourceNames:
    """What bn6f's source calls the constants of its instructions (module docstring)."""

    def __init__(self, info: ElfInfo, copies, function_names: set[str]):
        self.info = info
        self.copies = copies                      # code the game copies to RAM: lifted at its RAM address
        self.function_names = function_names      # lifted entry points: a constant can't share a name
        self.labels, self.equates, ambiguous = self._symbols()
        # one C name per address name; names whose C names collide are left alone
        by_c: dict[str, set[int]] = {}
        for nm, v in self.address_names():
            by_c.setdefault(self.label_c(nm), set()).add(v)
        self.bad = ambiguous | {nm for nm, v in self.address_names() if len(by_c[self.label_c(nm)]) > 1}
        self.pool = self._pools()
        self.used: dict[str, set[int]] = {}       # what was named where: kind -> instruction addresses

    # ── symbols as the assembler sees them ─────────────────────────────

    def _symbols(self) -> tuple[dict[str, int], dict[str, int], set[str]]:
        """Labels (value as `.word NAME` assembles it: Thumb functions without their mode bit),
        assembler constants, and the names that have several values (local labels, set symbols)."""
        labels: dict[str, set[int]] = {}
        equates: dict[str, set[int]] = {}
        out = subprocess.run(["readelf", "-sW", os.path.join(self.info.src_root, "bn6f.elf")],
                             check=True, capture_output=True, text=True).stdout
        for line in out.splitlines():
            m = SYM_RE.match(line)
            if not m:
                continue
            value, _size, typ, _bind, ndx, name = m.groups()
            if ndx == "UND" or not name or name.startswith("$") or name.endswith(".o"):
                continue
            v = int(value, 16)
            if ndx == "ABS":
                if typ == "NOTYPE":
                    equates.setdefault(name, set()).add(v)
                continue
            labels.setdefault(name, set()).add(v & ~1 if typ == "FUNC" else v)
        ambiguous = {n for n, vs in (*labels.items(), *equates.items()) if len(vs) > 1} | (labels.keys() & equates.keys())
        return ({n: min(vs) for n, vs in labels.items()}, {n: min(vs) for n, vs in equates.items()}, ambiguous)

    def address_names(self):
        """The names written A_NAME: labels, and assembler constants whose value is a GBA address
        (I/O registers, fixed RAM spots), as the lifter always wrote both."""
        yield from self.labels.items()
        yield from ((nm, v) for nm, v in self.equates.items() if is_address(v))

    @staticmethod
    def label_c(name: str) -> str:
        """A label's C name (as the lifter always spelled them)."""
        return "A_" + cname(name).removeprefix("bn_")

    # ── literal pools ──────────────────────────────────────────────────

    def _pools(self) -> dict[int, list[str]]:
        """Word address -> its `.word` expression(s), from the .s sources: a label gives an address,
        each `.word` item after it the next four bytes; anything else ends the run. (A file the build
        does not assemble may reuse a label: the value check in literal() picks the right one.)"""
        label = re.compile(r"^\s*([A-Za-z_.$][\w.$]*)::?(.*)$")
        word = re.compile(r"^\s*\.(?:word|4byte|long)\s+(.*)$")
        pool: dict[int, list[str]] = {}
        for path in sorted(set(self.info._src_paths.values())):
            with open(path, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
            addr: int | None = None
            for raw in text.split("\n"):
                s = strip_comment(raw)
                if not s:
                    continue
                while (m := label.match(s)) is not None:
                    nm = m.group(1)
                    addr = self.labels.get(nm) if nm not in self.bad else None
                    s = m.group(2).strip()
                if not s:
                    continue
                w = word.match(s)
                if w is None or addr is None:
                    addr = None
                    continue
                for item in split_items(w.group(1)):
                    pool.setdefault(addr, []).append(item)
                    addr += 4
        return pool

    def rom_address(self, a: int) -> int:
        """Where code the game copies to RAM (lifted at its RAM address) is in bn6f's source."""
        for c in self.copies:
            if c.vma <= a < c.vma + c.size:
                return a - c.vma + c.src
        return a

    # ── the three places ───────────────────────────────────────────────

    def literal(self, ins, literal_addr: int | None, value: int) -> Rendered | None:
        """A PC-relative load of VALUE from LITERAL_ADDR: the pool word's expression, or `ldr rN, =EXPR`."""
        texts = []
        if literal_addr is not None:
            texts += self.pool.get(self.rom_address(literal_addr), [])
        code = self._code(ins.addr)
        m = re.match(r"ldr\w*\s+\w+\s*,\s*=\s*(.+)$", code)
        if m:
            texts.append(m.group(1))
        for t in texts:
            r = self.render(t, value, plain=True)
            if r is not None:
                self._count("literal" if r.labels or r.equates else "literal number", ins.addr)
                return r
        return None

    def immediate(self, ins, value: int) -> Rendered | None:
        """An immediate operand or offset: the operand as written; a bare number by the comment's field."""
        src = self.info.source_line(ins.addr) or ""
        code, _, comment = src.partition("//")
        ops = re.findall(r"#\s*([^,\]#]+)", strip_comment(code))
        r, kind = None, ""
        if len(ops) == 1:
            written = ops[0].strip()
            if not re.fullmatch(r"[-+]?(0[xX][0-9a-fA-F]+|\d+)", written):
                r, kind = self.render(written, value), "operand"
            elif (m := re.fullmatch(r"\s*(\(.*\))\s*", comment)) is not None and _int(written) == value:
                r, kind = self.render(m.group(1), value), "comment"
        if r is not None:
            self._count(kind, ins.addr)
        return r

    def address(self, ins, value: int) -> Rendered | None:
        """`adr rN, EXPR`."""
        m = re.match(r"adr\w*\s+\w+\s*,\s*(.+)$", self._code(ins.addr))
        r = self.render(m.group(1), value) if m else None
        if r is not None:
            self._count("adr", ins.addr)
        return r

    # ── expressions ────────────────────────────────────────────────────

    def render(self, text: str, value: int, plain: bool = False) -> Rendered | None:
        """TEXT in C, if it names something and evaluates to VALUE (as C will compute it). PLAIN: a
        number (or an expression of numbers) is VALUE written as the lifter writes numbers."""
        try:
            e = parse(text)
        except (ParseError, ValueError):
            return None
        out = Rendered("")
        try:
            v = self._eval(e, out)
        except _Reject:
            return None
        if v is None or v & 0xFFFFFFFF != value & 0xFFFFFFFF:
            return None
        if not (out.labels or out.equates):
            if not plain:
                return None
            out.c = h(value)
            return out
        c, prec = self._c(e)
        out.c = c if prec == ATOM_PREC else f"({c})"
        return out

    def _eval(self, e: Expr, out: Rendered) -> int:
        if isinstance(e, Num):
            return e.value
        if isinstance(e, Sym):
            nm = e.name
            if nm in self.bad:
                raise _Reject
            if nm in self.labels:
                out.labels[self.label_c(nm)] = self.labels[nm]
                return self.labels[nm]
            if nm in self.equates and is_address(self.equates[nm]):
                out.labels[self.label_c(nm)] = self.equates[nm]
                return self.equates[nm]
            if nm in self.equates and self.equate_ok(nm):
                out.equates[nm] = self.equates[nm]
                return self.equates[nm]
            raise _Reject
        if isinstance(e, Unary):
            x = self._eval(e.x, out)
            return -x if e.op == "-" else ~x
        a, b = self._eval(e.left, out), self._eval(e.right, out)
        op = e.op
        if op in ("/", "%", ">>"):
            # C computes these unsigned (bn6f_syms.h's values are `u`): agree only on non-negative values
            if not (0 <= a < 1 << 32 and 0 <= b < 1 << 32) or (op != ">>" and b == 0) or (op == ">>" and b >= 32):
                raise _Reject
            return a // b if op == "/" else a % b if op == "%" else a >> b
        if op == "<<":
            if not 0 <= b < 32:
                raise _Reject
            return a << b
        return {"+": a + b, "-": a - b, "*": a * b, "|": a | b, "&": a & b, "^": a ^ b}[op]

    def equate_ok(self, nm: str) -> bool:
        return (re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", nm) is not None and nm not in C_KEYWORDS
                and not nm.startswith("__") and nm not in self.function_names)

    def _c(self, e: Expr) -> tuple[str, int]:
        if isinstance(e, Num):
            return e.text, ATOM_PREC
        if isinstance(e, Sym):
            address = e.name in self.labels or is_address(self.equates[e.name])
            return (self.label_c(e.name) if address else e.name), ATOM_PREC
        if isinstance(e, Unary):
            x, p = self._c(e.x)
            return f"{e.op}{x if p >= UNARY_PREC else f'({x})'}", UNARY_PREC
        p = C_PREC[e.op]
        a, pa = self._c(e.left)
        b, pb = self._c(e.right)
        return f"{a if pa >= p else f'({a})'} {e.op} {b if pb > p else f'({b})'}", p

    # ── helpers ────────────────────────────────────────────────────────

    def _code(self, addr: int) -> str:
        return strip_comment(self.info.source_line(addr) or "")

    def _count(self, what: str, addr: int) -> None:
        self.used.setdefault(what, set()).add(addr)


class _Reject(Exception):
    pass


def is_address(v: int) -> bool:
    return 0x02000000 <= v < 0x10000000


def _int(text: str) -> int | None:
    try:
        return int(text, 0)
    except ValueError:
        return None


def strip_comment(s: str) -> str:
    s = re.sub(r"/\*.*?\*/", "", s)
    return s.split("//")[0].split("@")[0].strip()


def split_items(text: str) -> list[str]:
    """`.word a, b+1, (c|d)`: the items, split at top-level commas."""
    items, depth, cur = [], 0, ""
    for ch in text:
        if ch == "," and depth == 0:
            items.append(cur.strip())
            cur = ""
            continue
        depth += (ch == "(") - (ch == ")")
        cur += ch
    if cur.strip():
        items.append(cur.strip())
    return items
