"""The bn6f struct field constants: what a write at BASE + OFFSET is, in the words the
disassembly itself uses.

Names and claims talk in struct fields -- `oBattleObject_Timer`, `oAIAttackVars_ChipID`,
`oS200ace0_PlayerX` -- because that is how the assembly reads (`strh r0, [r5,#oBattleObject_Timer]`,
bn6f/asm/asm00_2.s:1518). The runtime facts used to group a function's writes by the absolute
RAM symbol that happened to contain them (`eT3BattleObject1_Timer`), which is the same field
with the object index spelled out, so a claim in the disassembly's own vocabulary never matched
what the facts had written down.

So a write is expressed as BASE_REGISTER + OFFSET (OFFSET below `OFFSET_LIMIT`), the offset is
looked up in bn6f's own `o*` constants, and the candidates are narrowed to the struct families
the base register points into. The constants come from `bn6f/bn6f.elf`: bn6f's struct macros
(`def_struct_offsets battle_object_struct, oBattleObject`, include/macros/struct.inc:70) emit one
absolute symbol per field whose value is the field's offset, so `nm bn6f.elf` hands over the
whole table without reading a line of assembly. The FAMILIES (which `o*` names belong to which
struct, which of them are nested inside another) come from `bn6f/include/structs/*.inc`, where
the struct macros are declared and invoked.

Everything here is a pure lookup: no ROM is run, no assembly is read, and an offset nobody
names simply has no field name (the caller records the offset and an empty name list).
"""
from __future__ import annotations

import re
from collections.abc import Sequence
from functools import lru_cache
from typing import NamedTuple

from wf.repo import REPO
from wf.types import ToolMessage

import symdb  # tools/symdb.py: bn6f.elf's symbols (wf.repo put tools/ on the path)

#: how far from a base register a write still counts as "base + offset": every struct bn6f
#: declares is smaller than this (the biggest, oS20364C0, is 0xC0), so an offset past it is a
#: coincidence, not a field.
OFFSET_LIMIT = 0x400

#: bn6f's include tree, where the struct macros live.
STRUCT_INCLUDE = REPO / "bn6f/include"

#: `def_struct_offsets battle_object_struct, oBattleObject`
_DEF_STRUCT = re.compile(r"def_struct_offsets\s+\w+\s*,\s*(o[A-Z]\w*)")
#: `battle_object_struct oT3BattleObject, offset_struct_entry, ...` (a macro call with an o label)
_STRUCT_CALL = re.compile(r"^\s*\w+_struct\s+(o[A-Z]\w*)\s*,", re.M)
#: `.macro object_sprite_struct, label=oObjectSprite`
_STRUCT_MACRO = re.compile(r"\.macro\s+\w+_struct\s*,?\s*label=(o[A-Z]\w*)")
#: a RAM symbol's object part, `eT3BattleObject1_CurAction` -> ("T3BattleObject1", "CurAction")
_E_SYMBOL = re.compile(r"^e([A-Za-z0-9_]*)$")
#: a `T1`/`T3`/`T4` qualifier bn6f gives the per-table copies of a struct family
_TABLE_QUALIFIER = re.compile(r"^o[TBPC]\d(?=[A-Z])")
#: the generic words a RAM symbol puts in front of its object name (`eStruct200ace0_PlayerX`
#: is the struct the macros call `oS200ace0`)
_GENERIC_WORDS = ("Struct", "Object", "Buffer", "Buf", "Array", "Arr", "Global", "Data")


class Field(NamedTuple):
    """What one offset is, in bn6f's own words: the field names that carry it, the struct family
    they belong to, the families the base pointed into, and how many names share the offset
    (offsets are shared across families -- 0x20 is `oBattleObject_Timer` and a dozen other
    structs' field -- so the base register is what disambiguates)."""
    names: tuple[str, ...]
    family: str
    candidates: tuple[str, ...]
    total: int


class Written(NamedTuple):
    """One write, resolved: the struct offset from the struct's own start, the bn6f field names
    at it, the family, and the families the base pointed into."""
    struct_offset: int
    names: tuple[str, ...]
    family: str
    candidates: tuple[str, ...]
    total: int


@lru_cache(maxsize=1)
def constants() -> dict[int, tuple[str, ...]]:
    """Every `o*` offset constant in bn6f.elf: field offset -> the names that carry it.

    A name is kept when it starts with `o` + an upper-case letter (the field constants
    `def_struct_offsets` emits) and its value is below `OFFSET_LIMIT` (a field, not an address)."""
    syms = symdb.elf_symbols()
    out: dict[int, list[str]] = {}
    for name, value in syms.items():
        if len(name) < 2 or name[0] != "o" or not name[1].isupper() or not 0 <= value < OFFSET_LIMIT:
            continue
        out.setdefault(value, []).append(name)
    return {offset: tuple(sorted(names)) for offset, names in sorted(out.items())}


@lru_cache(maxsize=1)
def families() -> tuple[str, ...]:
    """Every struct family bn6f declares: the `o*` names a struct macro is defined or invoked
    with (include/structs/*.inc, include/rom_structs/*.inc) -- `oBattleObject`,
    `oT3BattleObject`, `oAIData`, `oAIAttackVars`, `oS200ace0`, ..."""
    found: set[str] = set()
    if STRUCT_INCLUDE.is_dir():
        for path in sorted(STRUCT_INCLUDE.rglob("*.inc")):
            text = path.read_text(errors="replace")
            for pattern in (_DEF_STRUCT, _STRUCT_CALL, _STRUCT_MACRO):
                found.update(pattern.findall(text))
    if not found:      # no bn6f checkout here: fall back to the ELF (a name other names start
        for names in constants().values():   # with is a family of its own)
            for name in names:
                head = name.split("_", 1)[0]
                if any(other.startswith(head + "_") for other in names):
                    found.add(head)
    return tuple(sorted(found))


@lru_cache(maxsize=None)
def family_of(name: str) -> str:
    """The struct family an `o*` field name belongs to: the longest declared family it starts
    with (`oT3BattleObject_Timer` -> `oT3BattleObject`, `oAIData_ActorType` -> `oAIData`)."""
    best = ""
    for family in families():
        if (name == family or name.startswith(family + "_")) and len(family) > len(best):
            best = family
    return best


@lru_cache(maxsize=None)
def generic_of(name: str) -> str:
    """The family-agnostic spelling of a per-table field: `oT3BattleObject_Timer` ->
    `oBattleObject_Timer`. The game keeps a copy of a struct per battle-object table and the
    assembly names each copy, but a claim about "the battle object's timer" means all of
    them."""
    return _TABLE_QUALIFIER.sub("o", name)


@lru_cache(maxsize=None)
def size_of(family: str) -> int | None:
    """`oFAMILY_Size`, when bn6f declares one (every struct ends in a `Size` field)."""
    name = f"{family}_Size"
    for offset, names in constants().items():
        if name in names:
            return offset
    return None


@lru_cache(maxsize=None)
def members(family: str) -> tuple[tuple[int, str], ...]:
    """A family's own fields, ascending: (offset, `oFAMILY_Field`). The struct's own size
    field and the family name itself are left out (they are not fields to write)."""
    out = [(offset, name) for offset, names in constants().items() for name in names
           if family_of(name) == family and name != family and name != f"{family}_Size"]
    return tuple(sorted(out))


def split_symbol(symbol: str) -> tuple[str, str]:
    """A bn6f RAM symbol's object and field parts: `eT3BattleObject1_CurAction` ->
    ("T3BattleObject1", "CurAction"), `eAIData0` -> ("AIData0", ""), `end_file_3007B00` ->
    ("", "")."""
    match = _E_SYMBOL.match(symbol)
    if match is None:
        return "", ""
    body = match.group(1)
    object_name, _, field = body.partition("_")
    return object_name, field


@lru_cache(maxsize=None)
def families_for_object(object_name: str) -> tuple[str, ...]:
    """The struct families a RAM symbol's object part points into, best first:
    `T3BattleObject1` -> `oT3BattleObject` (the exact copy), then `oBattleObject` (the family
    every copy is one of). Empty when the name says nothing about a struct (a bare buffer,
    `end_file_3007B00`, an unnamed EWRAM word)."""
    if not object_name:
        return ()
    stems = [object_name]
    trimmed_digits = object_name.rstrip("0123456789")
    if trimmed_digits != object_name:
        stems.append(trimmed_digits)
    out: list[str] = []
    for family in families():
        body = family[1:]
        if body in stems:
            out.append(family)
    for family in families():      # the generic family, and the names that drop a word
        body = family[1:]
        if len(body) < 4 or family in out:
            continue
        if any(stem.endswith(body) or body.endswith(stem) for stem in stems):
            out.append(family)
    for word in _GENERIC_WORDS:    # `eStruct200ace0_*` is the struct the macros call `oS200ace0`
        for stem in stems:
            if stem.startswith(word) and len(stem) > len(word):
                rest = stem[len(word):]
                out += [family for family in families()
                        if family[1:].endswith(rest) and family not in out]
    return tuple(dict.fromkeys(out))


def member_offset(families_: Sequence[str], field: str) -> int | None:
    """Where FIELD sits in the first of FAMILIES_ that declares it (`CurAction` in
    `oT3BattleObject` is 0x9), or None when none of them has that field."""
    if not field:
        return 0
    wanted = field.casefold()
    for family in families_:
        for offset, name in members(family):
            if name.casefold().endswith("_" + wanted):
                return offset
    return None


def field_for(offset: int, families_: Sequence[str] = (), limit: int = 8) -> Field:
    """What OFFSET is, in bn6f's words, narrowed to FAMILIES_ where the base register says
    which struct it points into. Only the best-scoring names are kept (a family match beats
    everything; `total` says how many names the offset carries in total). The family is
    named only when the base actually narrowed it: with no candidates to narrow by, or with
    none of the names in one, the names are offset coincidences and `family` stays ""."""
    names = constants().get(offset, ())
    if not names:
        return Field(names=(), family="", candidates=tuple(families_), total=0)
    scored: list[tuple[int, str]] = []
    for name in names:
        inside = not families_ or family_of(name) in families_
        scored.append((0 if inside else 1, name))
    scored.sort()
    best = scored[0][0]
    kept = tuple(name for score, name in scored if score == best)[:limit]
    narrowed = bool(families_) and best == 0
    return Field(names=kept, family=family_of(kept[0]) if (narrowed and kept) else "",
                 candidates=tuple(families_), total=len(names))


@lru_cache(maxsize=None)
def nested(family: str, offset: int, depth: int = 2) -> tuple[str, int] | None:
    """The family a write at FAMILY + OFFSET really lands in when OFFSET is inside a nested
    struct, and the offset within it: `oAIData + 0xA0 + 2` is `oAIAttackVars + 2`
    (`oAIData_AttackVars` is the nested struct; `oAIAttackVars` names its fields). None when
    the offset is one of the family's own fields, or when no nested family fits."""
    if depth <= 0 or not family:
        return None
    enclosing: tuple[int, str] | None = None
    end = size_of(family)
    for member_offset_, member in members(family):
        if member_offset_ > offset:
            end = member_offset_
            break
        enclosing = (member_offset_, member)
    if enclosing is None or end is None or not enclosing[0] <= offset < end:
        return None
    start, member = enclosing
    suffix = member[len(family) + 1:] if member.startswith(family + "_") else ""
    if not suffix:
        return None
    for other in families():
        if other != family and (other == suffix or other.endswith(suffix)):
            return nested(other, offset - start, depth - 1) or (other, offset - start)
    return None


def resolve(offset: int, symbol: str = "", limit: int = 8) -> Written:
    """What a write at (the RAM symbol `symbol` starts at) + OFFSET is.

    The base symbol names both the struct (`eT3BattleObject1_...` -> the `oT3BattleObject`
    family) and, when it is a field symbol rather than the struct's first one
    (`eT3BattleObject1_CurAction`), where in the struct the base sits -- so the offset is
    resolved against the struct's own start, and a nested struct (`oAIData_AttackVars`) is
    stepped into."""
    object_name, field = split_symbol(symbol)
    candidates = families_for_object(object_name)
    start = member_offset(candidates, field) if field else 0
    if start is None:                      # a field this family does not declare: the base is
        start = 0                           # the struct's own first field after all
    struct_offset = start + offset
    family = candidates[0] if candidates else ""
    inner = nested(family, struct_offset) if family else None
    if inner is not None:
        found = field_for(inner[1], (inner[0],), limit)
        return Written(struct_offset=struct_offset, names=found.names, family=inner[0],
                       candidates=candidates, total=found.total)
    found = field_for(struct_offset, candidates, limit)
    return Written(struct_offset=struct_offset, names=found.names,
                   family=found.family or family, candidates=candidates, total=found.total)


def describe(offset: int, symbol: str = "", limit: int = 8) -> ToolMessage:
    """One line for a log or a report: what bn6f calls the field at SYMBOL + OFFSET."""
    written = resolve(offset, symbol, limit)
    where = f"{symbol} + 0x{offset:X}" if symbol else f"+0x{offset:X}"
    names = ", ".join(written.names) if written.names else "(no bn6f field name at that offset)"
    return ToolMessage(f"{where} = struct +0x{written.struct_offset:X}: {names} "
                       f"[{written.family or '?'}]")