#!/usr/bin/env python3
"""Typed C structs for the game's memory, generated from bn6f/include/structs/*.inc.

    python3 tools/abi/gen_structs.py [--bn6f bn6f] [--out cport/include/gen/structs.h] [--json]

Each `.inc` defines a struct as an assembler macro (`u8 Name`, `ptr Name`,
`union`/`nextu`/`endu`, `struct_org`, `.space`, nested `*_struct` macros) and
instantiates its field offsets with `def_struct_offsets MACRO, oPrefix`. This
script expands those macros the way the assembler does (a location counter per
instantiation), then checks every field offset against the assembler's own
result: the absolute `oPrefix_Field` symbols in bn6f/bn6f.elf. A struct whose
layout disagrees anywhere is left out and reported, never guessed.

The header (doc: docs/C_CALLING_CONVENTION.md §3.5) has:
  - one `typedef struct` per instantiation prefix (`oToolkit` -> `Toolkit`),
    with explicit `pad_XX` gaps, C unions for `union`/`nextu`/`endu`, nested
    struct members (`ObjectSprite SpriteData`) and bn6f's field names;
  - `_Static_assert(offsetof(...))` on every field and on `sizeof` where the
    struct has a `Size` marker, so a wrong layout cannot compile;
  - guest pointers (`ptr`) as `u32`: the host difftest build is 64-bit;
  - fields the assembler placed off their natural alignment get an
    `aligned(1|2)` type (byte access) so the C layout still matches;
  - `G(T, addr)`: a guest address as a `T *` (plain, non-volatile access);
  - named globals for the RAM instances in bn6f/ewram.s and iwram.s
    (`eToolkit`, alias `toolkit`, `eMapTilesState200be70`, ...), each checked
    against the instance's own field labels in the ELF;
  - `struct_const`/`.equiv` constants, `#ifndef`-guarded and spelled as the
    lifter's bn6f_syms.h spells them, so both headers can be included.

The output is generated, not committed. Nothing includes it yet.
`--json` prints what was generated and skipped.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# field macro -> (size, C type); u8_arr is handled on its own
PRIMS: dict[str, tuple[int, str]] = {
    "u0": (0, ""), "u8": (1, "u8"), "u16": (2, "u16"), "u32": (4, "u32"),
    "s8": (1, "s8"), "s16": (2, "s16"), "s32": (4, "s32"), "ptr": (4, "u32"),
    "enum8": (1, "u8"), "enum16": (2, "u16"), "enum32": (4, "u32"),
    "flags8": (1, "u8"), "flags16": (2, "u16"), "flags32": (4, "u32"),
    "bool": (1, "u8"), "bool8": (1, "u8"),
}
STD_PARAMS = 3          # label, struct_entry, set_struct_start_address
INSTANCE_FILES = ("ewram.s", "iwram.s", "iwram_data.s")
ALIASES = {"eToolkit": "toolkit"}      # doc §1: readable code says toolkit.X


@dataclass
class Macro:
    name: str
    params: list[tuple[str, str | None]]
    body: list[str]
    file: str


@dataclass
class Field:
    label: str              # struct label in effect (oPrefix, or oPrefix_SpriteData)
    name: str
    offset: int
    size: int
    ctype: str              # "" for u0 markers
    count: int = 0          # array length (u8_arr), 0 = scalar
    union: int = -1         # union id, -1 = none
    branch: int = 0


@dataclass
class Member:
    """A nested struct macro expanded under its own label (oPrefix_SpriteData)."""
    label: str
    macro: str
    extras: tuple[str, ...]
    offset: int
    reset: bool             # it restarted the location counter (lives elsewhere)


@dataclass
class Union:
    base: int
    size: int = 0
    branches: int = 1


@dataclass
class Expansion:
    prefix: str
    macro: str
    extras: tuple[str, ...]
    file: str
    fields: list[Field] = field(default_factory=list)
    members: list[Member] = field(default_factory=list)
    unions: list[Union] = field(default_factory=list)


# ── reading the .inc files ──────────────────────────────────────────────────

def statements(text: str) -> list[str]:
    """Lines without comments (`/* */` and `//`), blank ones dropped."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group().count("\n"), text, flags=re.S)
    out = []
    for line in text.split("\n"):
        s = line.split("//")[0].strip()
        if s:
            out.append(s)
    return out


def split_args(rest: str) -> list[str]:
    return [a for a in re.split(r"\s*,\s*|\s+", rest.strip()) if a]


def parse_incs(paths: list[str]) -> tuple[dict[str, Macro], list[tuple[str, list[str], str]], list[tuple[str, str, str]]]:
    """Macros, top-level macro invocations (name, args, file) and top-level
    `.equiv`/`.set` constants (name, expression, file)."""
    macros: dict[str, Macro] = {}
    calls: list[tuple[str, list[str], str]] = []
    consts: list[tuple[str, str, str]] = []
    for path in paths:
        fname = os.path.basename(path)
        stack: list[Macro] = []
        for s in statements(open(path, encoding="utf-8", errors="replace").read()):
            word, _, rest = s.partition(" ")
            word = word.rstrip(",")
            if word == ".macro":
                m = re.match(r"\s*(\w+)\s*,?\s*(.*)", rest.replace("\t", " "))
                assert m, s
                params = []
                for p in split_args(m.group(2)):
                    name, _, default = p.partition("=")
                    params.append((name.split(":")[0], default if "=" in p else None))
                stack.append(Macro(m.group(1), params, [], fname))
            elif word == ".endm":
                mac = stack.pop()
                macros[mac.name] = mac
            elif stack:
                stack[-1].body.append(s.replace("\t", " "))
            elif word in (".equiv", ".set"):
                name, _, expr = rest.partition(",")
                consts.append((name.strip(), expr.strip(), fname))
            elif word == "def_struct_offsets":
                mac, prefix = split_args(rest)
                calls.append((mac, [prefix, "offset_struct_entry", "set_offset_struct_start_address"], fname))
            elif not word.startswith("."):
                calls.append((word, split_args(rest), fname))
    return macros, calls, consts


def evaluate(expr: str, known: dict[str, int]) -> int | None:
    """Value of a constant expression of literals, known names and | & + - << >>."""
    expr = re.sub(r"\b[A-Za-z_]\w*\b", lambda m: str(known[m.group()]) if m.group() in known else "?", expr)
    if "?" in expr or not re.fullmatch(r"[0-9a-fA-FxX\s|&+\-<>()~*]+", expr):
        return None
    try:
        return int(eval(expr, {"__builtins__": {}}))  # noqa: S307 - digits and operators only
    except (SyntaxError, ValueError, TypeError):
        return None


# ── expanding a struct macro like the assembler ─────────────────────────────

class Expander:
    def __init__(self, macros: dict[str, Macro], consts: dict[str, int]):
        self.macros = macros
        self.consts = consts

    def run(self, macro: str, args: list[str], file: str) -> Expansion:
        mac = self.macros[macro]
        bound = self.bind(mac, args)
        extras = tuple(bound[p] for p, _ in mac.params[STD_PARAMS:])
        exp = Expansion(prefix=bound[mac.params[0][0]], macro=macro, extras=extras, file=file)
        self.exp = exp
        self.loc = 0
        self.start = 0
        self.labels: list[str] = []
        self.union: Union | None = None
        self.union_off = 0
        self.resets = 0
        self.expand(mac, bound)
        return exp

    @staticmethod
    def bind(mac: Macro, args: list[str]) -> dict[str, str]:
        bound = {}
        for k, (p, default) in enumerate(mac.params):
            v = args[k] if k < len(args) else default
            if v is None:
                raise ValueError(f"{mac.name}: missing argument {p}")
            bound[p] = v
        return bound

    def expand(self, mac: Macro, bound: dict[str, str]) -> None:
        names = sorted(bound, key=len, reverse=True)
        for line in mac.body:
            for p in names:
                line = line.replace("\\" + p, bound[p])
            line = line.replace("\\()", "")
            self.statement(line.strip())

    def number(self, s: str) -> int:
        v = evaluate(s, self.consts)
        if v is None:
            raise ValueError(f"not a constant: {s}")
        return v

    def add(self, name: str, size: int, ctype: str, count: int = 0) -> None:
        label = self.labels[-1]
        if self.union is not None:
            off = self.union.base + self.union_off
            self.union_off += size
            self.union.size = max(self.union.size, self.union_off)
            u, b = len(self.exp.unions) - 1, self.union.branches - 1
        else:
            off = self.loc
            self.loc += size
            u, b = -1, 0
        self.exp.fields.append(Field(label, name, off, size, ctype, count, u, b))

    def statement(self, s: str) -> None:
        word, _, rest = s.partition(" ")
        word = word.rstrip(",")
        args = split_args(rest)
        if word.endswith(":") or word in (".struct",):
            return                                  # a label, or the offset section itself
        if word in PRIMS:
            size, ctype = PRIMS[word]
            self.add(args[0], size, ctype)
        elif word == "u8_arr":
            n = self.number(args[1])
            self.add(args[0], n, "u8" if n else "", n)
        elif word == "set_struct_label":
            self.labels.append(args[1])
        elif word == "restore_struct_label":
            self.labels.pop()
        elif word == "set_offset_struct_start_address":
            self.loc = self.start = 0
            self.resets += 1
        elif word == "set_struct_start_address":
            self.start = self.loc
        elif word == "struct_org":
            new = self.start + self.number(args[0])
            if new < self.loc:
                raise ValueError(f"struct_org {args[0]} goes back from {self.loc:#x}")
            self.loc = new
        elif word == ".space":
            if self.union is not None:
                raise ValueError(".space inside a union")
            self.loc += self.number(args[0])
        elif word == "union":
            self.union = Union(self.loc)
            self.union_off = 0
            self.exp.unions.append(self.union)
        elif word == "nextu":
            assert self.union is not None
            self.union.branches += 1
            self.union_off = 0
        elif word == "endu":
            assert self.union is not None
            self.loc += self.union.size
            self.union = None
        elif word == "struct_const":
            name, _, expr = rest.partition(",")
            v = evaluate(expr.strip(), self.consts)
            if v is not None:
                self.consts.setdefault(name.strip(), v)
        elif word in self.macros:
            mac = self.macros[word]
            bound = self.bind(mac, args)
            label = bound.get("label", self.labels[-1] if self.labels else "")
            if mac.params and mac.params[0][0] == "label" and label != (self.labels[-1] if self.labels else None):
                resets, at = self.resets, self.loc
                member = Member(label, word, tuple(bound[p] for p, _ in mac.params[STD_PARAMS:]), at, False)
                self.exp.members.append(member)
                self.expand(mac, bound)
                member.reset = self.resets != resets
                member.offset = 0 if member.reset else at
            else:
                self.expand(mac, bound)                 # same label: its fields join this struct
        else:
            raise ValueError(f"unknown statement: {s}")


# ── the ELF: the assembler's own offsets ────────────────────────────────────

def elf_symbols(elf: str) -> tuple[dict[str, set[int]], dict[str, int]]:
    """Absolute symbols (name -> values; object files define them repeatedly)
    and address labels (name -> address)."""
    out = subprocess.run(["readelf", "-sW", elf], capture_output=True, text=True, check=True).stdout
    absolute: dict[str, set[int]] = {}
    labels: dict[str, int] = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) < 8 or not f[0].endswith(":") or not f[0][:-1].isdigit():
            continue
        value, ndx, name = int(f[1], 16), f[6], f[7]
        if ndx == "ABS":
            absolute.setdefault(name, set()).add(value)
        elif ndx != "UND":
            labels.setdefault(name, value)
    return absolute, labels


# ── C layout ────────────────────────────────────────────────────────────────

NATURAL = {"u8": 1, "s8": 1, "u16": 2, "s16": 2, "u32": 4, "s32": 4}


def align_for(ctype: str, *offsets: int) -> int:
    """The largest alignment up to ctype's natural one that divides every offset."""
    a = NATURAL[ctype]
    while any(o % a for o in offsets):
        a //= 2
    return a


@dataclass
class CType:
    name: str
    exp: Expansion
    align: int = 1
    size: int = 0
    lines: list[str] = field(default_factory=list)
    asserts: list[tuple[str, int]] = field(default_factory=list)   # (member designator, offset)


class Layout:
    """Writes one struct from an expansion; types[] holds the structs made so far."""

    def __init__(self, types: dict[tuple[str, tuple[str, ...]], CType], used_aligned: set[str]):
        self.types = types
        self.used_aligned = used_aligned

    def decl(self, f: Field, *offsets: int) -> tuple[str, int]:
        a = align_for(f.ctype, f.offset, *offsets)
        t = f.ctype
        if a < NATURAL[t]:
            t = f"{t}_a{a}"
            self.used_aligned.add(t)
        if f.count:
            return f"{t} {f.name}[{f.count}];", a
        return f"{t} {f.name};", a

    def build(self, name: str, exp: Expansion) -> CType:
        ct = CType(name, exp)
        own = [f for f in exp.fields if f.label == exp.prefix]
        sizef = next((f for f in own if f.name == "Size" and not f.ctype), None)
        items: list[tuple[int, str, object]] = []          # (offset, kind, item)
        for f in own:
            if f.ctype and f.union < 0:
                items.append((f.offset, "field", f))
        for u, un in enumerate(exp.unions):
            if any(f.union == u for f in own if f.ctype):
                items.append((un.base, "union", u))
        for m in exp.members:
            if m.reset:
                ct.lines.append(f"    /* {m.label}: a {m.macro} that restarts at offset 0 (it lives before "
                                f"the struct): not a member */")
                continue
            items.append((m.offset, "member", m))
        items.sort(key=lambda x: (x[0], x[1] != "union"))
        loc = 0
        for off, kind, item in items:
            if off < loc:
                raise ValueError(f"{exp.prefix}: overlapping {kind} at {off:#x} (layout ends at {loc:#x})")
            if off > loc:
                ct.lines.append(f"    u8 pad_{loc:02X}[0x{off - loc:X}];")
            if kind == "field":
                text, a = self.decl(item, sizef.offset if sizef else 0)  # type: ignore[arg-type]
                ct.lines.append(f"    {text:<40} /* 0x{off:02X} */")
                ct.asserts.append((item.name, off))  # type: ignore[union-attr]
                loc = off + item.size  # type: ignore[union-attr]
            elif kind == "union":
                loc = self.union(ct, item, own)  # type: ignore[arg-type]
                a = 1
            else:
                m: Member = item  # type: ignore[assignment]
                sub = self.types.get((m.macro, m.extras))
                if sub is None:
                    raise ValueError(f"{exp.prefix}: member {m.label} has no struct type ({m.macro})")
                mname = m.label[len(exp.prefix) + 1:]
                if off % sub.align:
                    raise ValueError(f"{exp.prefix}: member {mname} at {off:#x} is not {sub.align}-aligned")
                self.check_member(exp, m, sub)
                ct.lines.append(f"    {sub.name + ' ' + mname + ';':<40} /* 0x{off:02X} */")
                ct.asserts.append((mname, off))
                ct.asserts += [(f"{mname}.{d}", off + o) for d, o in sub.asserts]
                loc = off + sub.size
                a = sub.align
            ct.align = max(ct.align, a)
        end = sizef.offset if sizef else loc
        if end < loc:
            raise ValueError(f"{exp.prefix}: fields end at {loc:#x}, after Size {end:#x}")
        if end > loc:
            ct.lines.append(f"    u8 pad_{loc:02X}[0x{end - loc:X}];")
        if end % ct.align:
            raise ValueError(f"{exp.prefix}: size {end:#x} is not a multiple of its alignment {ct.align}")
        ct.size = end
        return ct

    def union(self, ct: CType, u: int, own: list[Field]) -> int:
        un = ct.exp.unions[u]
        ct.lines.append("    union {")
        for b in range(un.branches):
            fs = [f for f in own if f.union == u and f.branch == b and f.ctype]
            if not fs:
                continue
            if len(fs) == 1 and fs[0].offset == un.base:
                text, a = self.decl(fs[0], un.base, un.size)
                ct.lines.append(f"        {text:<36} /* 0x{fs[0].offset:02X} */")
                ct.align = max(ct.align, a)
            else:
                ct.lines.append("        struct {")
                loc = un.base
                for f in fs:
                    if f.offset > loc:
                        ct.lines.append(f"            u8 pad_{loc:02X}[0x{f.offset - loc:X}];")
                    text, a = self.decl(f, un.base, un.size)
                    ct.lines.append(f"            {text:<32} /* 0x{f.offset:02X} */")
                    ct.align = max(ct.align, a)
                    loc = f.offset + f.size
                ct.lines.append("        };")
            ct.asserts += [(f.name, f.offset) for f in fs]
        ct.lines.append("    };")
        return un.base + un.size

    @staticmethod
    def check_member(exp: Expansion, m: Member, sub: CType) -> None:
        """The nested fields sit where the standalone struct has them."""
        mine = {f.name: f.offset - m.offset for f in exp.fields if f.label == m.label}
        theirs = {f.name: f.offset for f in sub.exp.fields if f.label == sub.exp.prefix}
        if mine != theirs:
            raise ValueError(f"{exp.prefix}: member {m.label} differs from {sub.name}")


# ── checks against the ELF ──────────────────────────────────────────────────

def check_offsets(exp: Expansion, absolute: dict[str, set[int]]) -> tuple[int, list[str]]:
    """(fields confirmed by an ELF symbol, mismatches)."""
    ok, bad = 0, []
    for f in exp.fields:
        if f.label != exp.prefix and any(m.label == f.label and m.reset for m in exp.members):
            continue
        vals = absolute.get(f"{f.label}_{f.name}")
        if vals is None:
            continue
        if vals != {f.offset}:
            bad.append(f"{f.label}_{f.name}: generated {f.offset:#x}, ELF {sorted(hex(v) for v in vals)}")
        else:
            ok += 1
    return ok, bad


def instance_type(macro: str, macros: dict[str, Macro]) -> tuple[str, tuple[str, ...]]:
    """The struct key an instance macro makes: the macro itself, or the struct
    macro a one-line wrapper calls (t1_battle_object_struct -> battle_object_struct
    with extra_vars_size 0x2c)."""
    mac = macros[macro]
    if len(mac.body) == 1:
        word, _, rest = mac.body[0].partition(" ")
        if word in macros:
            inner = macros[word]
            bound = Expander.bind(inner, split_args(rest))
            return word, tuple(bound[p] for p, _ in inner.params[STD_PARAMS:])
    return macro, tuple(d or "" for _p, d in mac.params[STD_PARAMS:])


def instance_check(label: str, ct: CType, labels: dict[str, int]) -> tuple[int | None, int, list[str]]:
    """(address, fields confirmed, mismatches): the instance's own field labels
    (eToolkit_JoypadPtr) must sit at address + offset. The address comes from
    them, not from the instance label: a battle object's label is its linked
    list node, 0x10 bytes before the object."""
    syms = [(f"{label}_{d.replace('.', '_')}", off) for d, off in ct.asserts]
    addr = next((labels[sym] - off for sym, off in syms if sym in labels), None)
    if addr is None:
        return None, 0, []
    ok, bad = 0, []
    for sym, off in syms:
        if sym in labels:
            if labels[sym] != addr + off:
                bad.append(f"{sym}: {labels[sym]:#x} != {addr + off:#x}")
            else:
                ok += 1
    return addr, ok, bad


# ── output ──────────────────────────────────────────────────────────────────

HEADER = """\
/* Generated by tools/abi/gen_structs.py from the bn6f/include/structs .inc files: do not edit.
 * Every offset below is asserted against the one the assembler gave the
 * field (the oStruct_Field symbols in bn6f.elf); guest pointers are u32. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "cpu.h"

/* A guest address as a typed pointer. Guest memory sits at its GBA
 * addresses on hardware and in the host test build (TARGET_FLAT), so this
 * is a cast (ordinary, non-volatile access: use LD/ST for I/O registers). */
#ifndef G
#define G(T, addr) ((T *)(uintptr_t)(u32)(addr))
#endif
"""


def render(types: list[CType], aliases: dict[str, tuple[str, str]], consts: dict[str, int],
           instances: list[tuple[str, str, int]], used_aligned: set[str]) -> str:
    out = [HEADER]
    if used_aligned:
        out.append("/* Fields the assembler placed off their natural alignment: byte/halfword access. */")
        for t in sorted(used_aligned):
            base, a = t.split("_a")
            out.append(f"typedef {base} {t} __attribute__((aligned({a})));")
        out.append("")
    if consts:
        out.append("/* struct_const and .equiv constants (spelled as bn6f_syms.h does). */")
        for name, v in sorted(consts.items()):
            out.append(f"#ifndef {name}\n#define {name} 0x{v & 0xFFFFFFFF:X}u\n#endif")
        out.append("")
    for ct in types:
        out.append(f"/* {ct.exp.prefix}: {ct.exp.macro} (bn6f/include/structs/{ct.exp.file}) */")
        out.append(f"typedef struct {ct.name} {{")
        out += ct.lines
        out.append(f"}} {ct.name};")
        for d, off in ct.asserts:
            out.append(f'_Static_assert(offsetof({ct.name}, {d}) == 0x{off:X}, "{ct.name}.{d}");')
        if any(f.name == "Size" and not f.ctype for f in ct.exp.fields if f.label == ct.exp.prefix):
            out.append(f'_Static_assert(sizeof({ct.name}) == 0x{ct.size:X}, "sizeof {ct.name}");')
        out.append("")
    for alias, (orig, why) in sorted(aliases.items()):
        out.append(f"/* {alias}: {why} */")
        out.append(f"typedef {orig} {alias};")
    if aliases:
        out.append("")
    out.append("/* RAM instances (bn6f ewram.s / iwram.s), checked against their field labels. */")
    for label, tname, addr in instances:
        out.append(f"#define {label} (*G({tname}, 0x{addr:08X}u))")
        if label in ALIASES:
            out.append(f"#define {ALIASES[label]} {label}")
    out.append("")
    return "\n".join(out)


def generate(bn6f: str) -> tuple[str, dict]:
    paths = sorted(os.path.join(bn6f, "include", "structs", f)
                   for f in os.listdir(os.path.join(bn6f, "include", "structs")) if f.endswith(".inc"))
    macros, calls, top_consts = parse_incs(paths)
    absolute, labels = elf_symbols(os.path.join(bn6f, "bn6f.elf"))
    consts: dict[str, int] = {}
    for name, expr, _f in top_consts:
        v = evaluate(expr, consts)
        if v is not None:
            consts.setdefault(name, v)
    ex = Expander(macros, consts)
    report: dict = {"structs": [], "skipped": [], "instances": [], "skipped_instances": []}
    types: dict[tuple[str, tuple[str, ...]], CType] = {}
    by_prefix: dict[str, CType] = {}
    ordered: list[CType] = []
    aliases: dict[str, tuple[str, str]] = {}
    used_aligned: set[str] = set()
    layout = Layout(types, used_aligned)
    # nested structs first: a struct is laid out after the members it uses
    pending = [(m, a, f) for m, a, f in calls if m in macros]
    for _round in range(4):
        later = []
        for macro, args, fname in pending:
            try:
                exp = ex.run(macro, args, fname)
            except ValueError as e:
                report["skipped"].append({"struct": args[0] if args else macro, "file": fname, "why": str(e)})
                continue
            name = exp.prefix[1:] if exp.prefix.startswith("o") else exp.prefix
            if any((m.macro, m.extras) not in types for m in exp.members if not m.reset):
                later.append((macro, args, fname))
                continue
            ok, bad = check_offsets(exp, absolute)
            if not ok:
                report["skipped"].append({"struct": name, "file": fname,
                                          "why": "no oStruct_Field symbols in bn6f.elf (not assembled): unverifiable"})
                continue
            if bad:
                report["skipped"].append({"struct": name, "file": fname, "why": "offsets differ from the ELF",
                                          "details": bad[:10]})
                continue
            key = (macro, exp.extras)
            if key in types:
                aliases[name] = (types[key].name, f"same layout as {types[key].name} ({macro})")
                report["structs"].append({"struct": name, "alias_of": types[key].name, "fields_checked": ok})
                continue
            try:
                ct = layout.build(name, exp)
            except ValueError as e:
                report["skipped"].append({"struct": name, "file": fname, "why": str(e)})
                continue
            types[key] = ct
            by_prefix[exp.prefix] = ct
            ordered.append(ct)
            report["structs"].append({"struct": name, "file": fname, "size": ct.size,
                                      "fields": len(ct.asserts), "fields_checked": ok})
        pending = later
        if not pending:
            break
    for macro, args, fname in pending:
        report["skipped"].append({"struct": args[0], "file": fname, "why": "a nested struct it uses was skipped"})
    instances = []
    for fname in INSTANCE_FILES:
        path = os.path.join(bn6f, fname)
        if not os.path.exists(path):
            continue
        for s in statements(open(path, encoding="utf-8", errors="replace").read()):
            m = re.fullmatch(r"(\w+)\s+(\w+)", s)
            if not m or m.group(1) not in macros:
                continue
            macro, label = m.groups()
            ct = types.get(instance_type(macro, macros))
            if ct is None:
                report["skipped_instances"].append({"instance": label, "macro": macro, "why": "no struct type"})
                continue
            addr, ok, bad = instance_check(label, ct, labels)
            if addr is None or bad:
                report["skipped_instances"].append({"instance": label, "macro": macro,
                                                    "why": "field labels differ" if bad else "no field labels",
                                                    "details": bad[:5]})
                continue
            instances.append((label, ct.name, addr))
            entry = {"instance": label, "type": ct.name, "address": f"{addr:#010x}", "fields_checked": ok}
            if labels.get(label, addr) != addr:
                entry["label_address"] = f"{labels[label]:#010x}"
            report["instances"].append(entry)
    keep = {n: v for n, v in consts.items()
            if not re.match(r"o[A-Z]", n) and not re.fullmatch(r"STRUCT_\w+_INC", n)}
    report["constants"] = len(keep)
    return render(ordered, aliases, keep, instances, used_aligned), report


def main() -> None:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("--bn6f", default=os.path.join(ROOT, "bn6f"))
    ap.add_argument("--out", default=os.path.join(ROOT, "cport", "include", "gen", "structs.h"))
    ap.add_argument("--json", action="store_true", help="print the report as JSON")
    args = ap.parse_args()
    text, report = generate(args.bn6f)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w") as fh:
        fh.write(text)
    rel = os.path.relpath(args.out, ROOT)
    report["out"] = args.out if rel.startswith("..") else rel
    if args.json:
        json.dump(report, sys.stdout, indent=1)
        print()
    else:
        print(f"{report['out']}: {len(report['structs'])} structs, {len(report['instances'])} instances, "
              f"{report['constants']} constants; skipped {len(report['skipped'])} structs, "
              f"{len(report['skipped_instances'])} instances")
        for s in report["skipped"]:
            print(f"  skipped {s['struct']} ({s['file']}): {s['why']}")


if __name__ == "__main__":
    main()
