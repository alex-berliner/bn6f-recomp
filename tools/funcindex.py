"""The function index: the game's functions as the tools see them, read from the files that define
them, with one parser for each instead of one per tool (review 1 B15: "one shared library for the
duplicated parsers", first the parts typify needs).

  lifted_units(DIR)        NAME -> Unit: every lifted function, from the header comment the lifter
                           writes above it in cport/src/lifted (range, mode, bn6f source line, contract)
  readable_functions(DIR)  NAME -> file: every readable override, `Next NAME(void) {` in cport/src/game
                           (the pattern tools/lifter/lift.py uses to leave the lifted body out)
  unit_finder(UNITS)       address -> the unit whose range holds it (profiles, coverage)
  graph_contracts(PATH)    NAME -> GraphContract: the ABI export's register contract (build-abi/graph.json)
  call_neighbours(PATH)    address -> the addresses of its direct callers and callees (the export's
                           call sites; indirect calls are not direct)

Users: tools/stamps.py (contracts and callees for the stamp check), tools/coverage.py and
tools/bisect_readable.py (unit ranges, readable names), tools/symdb_rewrite.py and the convert
workflow's db entries (tools/wf/wf/tools/evidence.py: `related:`). Standard library only, and typed
strictly: tools/wf imports it.
"""
from __future__ import annotations

import bisect
import json
import os
import re
from collections.abc import Callable, Iterable
from typing import NamedTuple, cast

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIFTED_DIR = os.path.join(ROOT, "cport", "src", "lifted")
GAME_DIR = os.path.join(ROOT, "cport", "src", "game")

# `Next NAME(void) {`, maybe with IWRAM_ / ARM_CODE attributes first: a readable body that replaces
# the lifted one (the lifter's own OVERRIDE_RE).
READABLE = re.compile(r"^(?:IWRAM_\w+\s+|ARM_CODE\s+)*Next\s+(\w+)\s*\(\s*void\s*\)\s*\{", re.M)

# The lifter's header above each lifted function:
#   /* NAME: 0800A000-0800A01C THUMB, bn6f asm00_0.s:123 */
#   /*   inputs:  r0 r1 lr
#    *   outputs: r0
#    *   clobbers (free after return): r1 r2 r3 r12 nz c v
#    *   preserved: r4 r5 r6 r7 */
UNIT_START = re.compile(r"^/\* (\w+): ([0-9A-F]{8})-([0-9A-F]{8}) (THUMB|ARM)", re.M)
UNIT_HEADER = re.compile(r"^/\* (\w+): ([0-9A-F]{8})-([0-9A-F]{8}) (?:THUMB|ARM)(?:, bn6f ([\w.]+:\d+))? \*/\n"
                         r"/\*   inputs:  ([^\n]*)\n \*   outputs: ([^\n]*)\n \*   clobbers[^:]*: ([^\n]*)\n"
                         r" \*   preserved: [^\n]*? \*/", re.M)
BRANCH = re.compile(r"/\* [0-9A-F]{8}: bl? (\w+)")     # a call or branch, in a lifted body's comments
BOGUS_SPAN = 0x10000         # a few headers end far away (data the disassembly put inside the function)


def regs(text: str) -> tuple[str, ...]:
    return tuple(x for x in text.split() if x != "-")


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
        return cls(regs(m.group(1)), regs(m.group(2)), regs(m.group(3)))

    def checked(self) -> set[str]:
        """What the difftest compares (farm.compare_args): every r0-r12 that is not free, and output flags."""
        kept = {f"r{i}" for i in range(13)} - set(self.clobbers)
        return kept | {f for f in ("nz", "c", "v") if f in self.outputs}


class Unit(NamedTuple):
    """One lifted function, as its header describes it. (A tuple: tools cache thousands of them.)"""
    lo: int
    hi: int
    contract: Contract
    asm: str | None              # where bn6f defines it: "asm00_2.s:613"


def parse_units(text: str, with_callees: bool = False) -> tuple[dict[str, Unit], dict[str, tuple[str, ...]]]:
    """The units of one lifted file, and (WITH_CALLEES) each one's direct callees in order: the targets
    of its `bl` and branches to other units. Callees are complete only in the difftest_lifted files,
    where every unit's body is written out."""
    units: dict[str, Unit] = {}
    callees: dict[str, tuple[str, ...]] = {}
    matches = list(UNIT_HEADER.finditer(text))
    for i, m in enumerate(matches):
        name = m.group(1)
        units[name] = Unit(int(m.group(2), 16), int(m.group(3), 16),
                           Contract(regs(m.group(5)), regs(m.group(6)), regs(m.group(7))), m.group(4))
        if with_callees:
            end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
            callees[name] = tuple(dict.fromkeys(BRANCH.findall(text, m.end(), end)))
    return units, callees


def lifted_files(directory: str = LIFTED_DIR) -> list[str]:
    return sorted(os.path.join(directory, f) for f in os.listdir(directory) if f.endswith(".c")) \
        if os.path.isdir(directory) else []


def lifted_units(directory: str = LIFTED_DIR) -> dict[str, Unit]:
    """Every lifted function of the lifted output in DIRECTORY (tools/build_lifted.sh writes it)."""
    out: dict[str, Unit] = {}
    for path in lifted_files(directory):
        with open(path, encoding="utf-8", errors="replace") as fh:
            out.update(parse_units(fh.read())[0])
    return out


def unit_ranges(directory: str = LIFTED_DIR) -> dict[str, tuple[int, int]]:
    """NAME -> (start, end) from the header's first line only (cheaper than lifted_units; an end that is
    far away, a bogus header, is taken as the start)."""
    out: dict[str, tuple[int, int]] = {}
    for path in lifted_files(directory):
        with open(path, encoding="utf-8", errors="replace") as fh:
            for m in UNIT_START.finditer(fh.read()):
                lo, hi = int(m.group(2), 16), int(m.group(3), 16)
                out[m.group(1)] = (lo, hi if hi - lo < BOGUS_SPAN else lo)
    return out


def unit_finder(ranges: dict[str, tuple[int, int]]) -> Callable[[int], str | None]:
    """A function from an address to the name of the unit whose [start, end) holds it."""
    by_start = sorted((lo, hi, name) for name, (lo, hi) in ranges.items())
    starts = [lo for lo, _, _ in by_start]

    def find(address: int) -> str | None:
        i = bisect.bisect_right(starts, address) - 1
        if i >= 0 and address < by_start[i][1]:
            return by_start[i][2]
        return None
    return find


def readable_in(text: str) -> list[str]:
    """The readable functions a C file defines."""
    return READABLE.findall(text)


def readable_functions(directory: str = GAME_DIR) -> dict[str, str]:
    """NAME -> path of the cport/src/game file holding its readable body."""
    out: dict[str, str] = {}
    if not os.path.isdir(directory):
        return out
    for f in sorted(os.listdir(directory)):
        if f.endswith(".c"):
            path = os.path.join(directory, f)
            with open(path, encoding="utf-8", errors="replace") as fh:
                for name in readable_in(fh.read()):
                    out[name] = path
    return out


# ── the ABI export (tools/lifter/abi_export.py, `lift.py --export-abi`; format: docs/C_CALLING_CONVENTION.md 3.1)

class GraphContract(NamedTuple):
    address: int
    unit: str
    inputs: tuple[str, ...]
    outputs: tuple[str, ...]
    preserved: tuple[str, ...]
    readable: bool


def _graph(path: str) -> dict[str, object]:
    with open(path) as fh:
        return cast(dict[str, object], json.load(fh))


def _strings(value: object) -> tuple[str, ...]:
    return tuple(str(x) for x in cast(list[object], value)) if isinstance(value, list) else ()


def graph_contracts(path: str) -> dict[str, GraphContract]:
    """Every entry point of the export, with its register contract (the refined one the export computes)."""
    entries = cast(dict[str, dict[str, object]], _graph(path).get("entries", {}))
    return {name: GraphContract(int(str(e["addr"]), 16) & ~1, str(e["unit"]), _strings(e.get("inputs")),
                                _strings(e.get("outputs")), _strings(e.get("preserved")), bool(e.get("readable")))
            for name, e in entries.items()}


def call_neighbours(path: str) -> dict[int, frozenset[int]]:
    """Address -> addresses of the functions it calls or branches to, and of those that call it: the
    export's call sites with a known target (`bl`, tail calls, jumps), keyed by address so the names
    the export was made with may be out of date."""
    graph = _graph(path)
    entries = cast(dict[str, dict[str, object]], graph.get("entries", {}))
    address = {name: int(str(e["addr"]), 16) & ~1 for name, e in entries.items()}
    in_unit: dict[str, set[int]] = {}
    for name, e in entries.items():
        in_unit.setdefault(str(e["unit"]), set()).add(address[name])
    out: dict[int, set[int]] = {}
    for site in cast(list[dict[str, object]], graph.get("sites", [])):
        target = site.get("target")
        if not isinstance(target, str) or target not in address:
            continue
        callee = address[target]
        for caller in in_unit.get(str(site.get("caller")), ()):
            if caller != callee:
                out.setdefault(caller, set()).add(callee)
                out.setdefault(callee, set()).add(caller)
    return {a: frozenset(n) for a, n in out.items()}


def related_by_calls(me: int, others: Iterable[tuple[str, int]], neighbours: dict[int, frozenset[int]]) -> list[str]:
    """The names among OTHERS (name, address) that call ME or that ME calls, in their order."""
    near = neighbours.get(me & ~1, frozenset())
    return [name for name, a in others if a & ~1 in near]
