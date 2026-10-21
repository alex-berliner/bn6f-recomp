"""The runtime-facts layer: a record of what the ORIGINAL game actually does at runtime, for
every lifted function, built once from the recordings and with no model judgment anywhere in it.

Three things go into one function's record:

  - **coverage** (from `tools/traces/coverage`): for every route's `.prof`, whether the
    function's entry address was executed, how many cycles the profile attributes to its
    entry instruction and to its whole body, and -- from the route's `.edges`, which hold
    real execution counts at the game's indirect call sites -- the caller addresses and names
    and the calls per 1000 frames of the route. The route lengths come from `wf/routes.py`
    read at run time (`beastsp` keeps growing), never from a number written here.
  - **runtime callers** (the same `.edges`): who called it, from where, how often.
  - **facts** (from the captured calls, when there are any): K of the entry snapshots under
    `build-snapshots/NAME/` replayed in mGBA through `tools/runtime_evidence.py`'s
    `run_original` + `summarise`, with the arguments it was given and the r0 it returned.

Everything is keyed by the function's entry address (`funcindex.lifted_units()` is
address-keyed through `Unit.lo`; names change as bn6f is renamed, so a name is a field here,
never the key). Output, all of it regenerable and never committed:

    build/facts/by_address/0x08XXXXXX.json    one per function
    build/facts/coverage.tsv                  one row per function
    build/facts/progress.txt                 what the run did, as it did it

The workflow lives in `tools/wf/wf/graphs/facts.py`; this file holds its parts. Nothing here
touches bn6f, symbols/db or git, and no model is called: the output is what the recordings
show, for later steps (a claim checker, an mGBA intervention runner) to check names against.
"""
from __future__ import annotations

import contextlib
import hashlib
import os
import re
import shutil
import struct
import subprocess
import tempfile
import threading
import time
from array import array
from bisect import bisect_right
from collections.abc import Generator, Sequence
from concurrent.futures import ThreadPoolExecutor
from functools import lru_cache
from pathlib import Path
from typing import NamedTuple

from wf.repo import REPO, SNAPSHOTS, TRACES   # puts tools/ on sys.path, for funcindex below
from wf.types import FunctionName, GuestAddress, Result, RouteName, ToolMessage
from wf.tools import runtime_evidence as rt

import funcindex  # noqa: E402  # tools/funcindex.py: the lifted units, the functions recorded here
from funcindex import Unit   # noqa: E402

PACKAGE_TREE = Path(__file__).resolve().parents[4]          # the tree this wf package lives in
FACTS = Path(os.environ.get("WF_FACTS_DIR") or PACKAGE_TREE / "build" / "facts")
BY_ADDRESS = "by_address"

DEFAULT_STATES = 8               # K entry states replayed per function (--states)
DEFAULT_JOBS = 6                 # functions replayed at once (--jobs)
DEFAULT_LIMIT = 0                # every function (--limit N takes the first N by address)
CALL_TIMEOUT = 120               # one function's mGBA pass, seconds
MAX_STEPS = 5_000_000            # instructions per replayed call before mgba_run gives up
ROM = REPO / "bn6f/bn6f.gba"     # the ROM mGBA runs (its sha1 is part of the cache key)

# ── the record one function gets ────────────────────────────────────────

class Caller(Result):
    """One call site the recordings show reaching this function: where the call instruction
    is, the function that holds it, and how many times that site called here on that route.
    (The `.edges` files hold the game's indirect and jump call sites only, so a direct
    `bl` caller is not in here -- a fact about indirect calls, not about every call.)"""
    site: str                       # "0x03005C68": the call instruction
    caller: str                     # "0x0800ABCD": the calling function's entry, "" if no unit holds it
    name: FunctionName              # the calling function's name, "(no unit)" when unknown
    count: int


class RouteCoverage(Result):
    """What one recorded route shows about one function. `.prof` holds CYCLES per executed
    PC halfword (not executions), so `entry_cycles`/`function_cycles` are cycles; the call
    counts come from `.edges`, which counts executions at the recorded indirect sites."""
    route: RouteName
    executed: bool = False          # the function's entry address appears in ROUTE.prof
    body_executed: bool = False     # some address of the function's range does
    entry_cycles: int = 0           # cycles the profile attributes to the entry instruction
    function_cycles: int = 0        # cycles it attributes to the whole [entry, end) range
    body_instructions: int = 0      # distinct addresses of the function the profile lists
    frames: int | None = None       # the route's length in game frames
    frames_from: ToolMessage = ToolMessage("")   # "wf/routes.py", "prof header" or "unknown"
    calls: int | None = None        # calls through the recorded indirect sites (ROUTE.edges)
    calls_per_1000_frames: float | None = None
    callers: tuple[Caller, ...] = ()


class RegisterStats(Result):
    """The distinct values one register held across the replayed calls: how many, the range,
    and up to eight of them (a function called with thousands of distinct pointers keeps the
    count and the range, not every value)."""
    reg: str
    samples: int = 0                # how many probes carried a value
    distinct: int = 0
    lowest: int | None = None
    highest: int | None = None
    values: tuple[int, ...] = ()    # up to eight, ascending


class Inputs(Result):
    """What a record was made from, as the cache reads it: a function whose inputs are
    unchanged is not rebuilt. `coverage` is a fingerprint over the `.prof`/`.edges` mtimes
    alone, so a route that grew re-reads the coverage without replaying anything."""
    mgba_run: ToolMessage           # "<sha1> <mtime> <size>"
    rom: ToolMessage                # "<sha1> <mtime>"
    snapshots: ToolMessage          # "<newest mtime> <entry snapshots>"
    states: int = DEFAULT_STATES
    coverage: ToolMessage = ToolMessage("")


class Target(Result):
    """One function of the run: its entry address (the key every output is filed under) and
    the name it has today (a name changes as bn6f is renamed; an address does not)."""
    address: GuestAddress
    name: FunctionName


class FunctionFacts(Result):
    """One function's record. `facts` is None when the function has no captured calls (nothing
    to replay) or when its replay produced none; `facts_error` then says which."""
    address: GuestAddress
    address_hex: str                # "0x0803E18C": the file it lives in under by_address/
    name: FunctionName
    end: GuestAddress               # the unit's [entry, end) range in the lifted headers
    asm: str | None = None          # where bn6f defines it: "asm00_2.s:613"
    reached: bool = False           # any route executed it
    captured: bool = False          # build-snapshots/NAME/ holds entry snapshots of real calls
    captured_states: int = 0        # how many entry snapshots the capture holds
    states: int = DEFAULT_STATES    # how many were replayed
    probes: int = 0
    returned: int = 0
    routes: tuple[RouteCoverage, ...] = ()
    calls: int = 0                  # calls the recorded indirect sites made, over every route
    calls_per_1000_frames: float = 0.0   # the same, over every route's frames
    cycles: int = 0                 # cycles the profiles attribute to it, over every route
    routes_reached: int = 0
    arguments: tuple[RegisterStats, ...] = ()    # r0..r3 (and sp) at the replayed entries
    returns: tuple[RegisterStats, ...] = ()       # r0 at the replayed exits
    facts: rt.RuntimeFacts | None = None
    facts_error: str | None = None
    inputs: Inputs | None = None
    built: ToolMessage = ToolMessage("")          # when this record was written (UTC, ISO)


class FactsReport(Result):
    """What a run produced over every function: how many the routes reached, how many were
    captured, how many carry runtime facts, and why the rest do not."""
    functions: int
    reached: int
    captured: int
    with_facts: int
    facts_errors: int
    error_reasons: tuple[ToolMessage, ...]   # the five most common reasons, with counts
    routes: tuple[RouteName, ...] = ()
    seconds: float = 0.0
    rows: int = 0                   # rows in coverage.tsv
    examples: tuple[ToolMessage, ...] = ()
    notes: tuple[ToolMessage, ...] = ()


# ── the function set: tools/funcindex.py's lifted units, address-keyed ─

def lifted_dir() -> str:
    """Where the lifter's output is read from. The lifted headers are generated and never
    committed, so a worktree of the wf code has none: the main checkout's copy is used
    (REPO, which wf.repo already puts on the path for its other tools)."""
    for tree in (PACKAGE_TREE, REPO):
        candidate = tree / "cport" / "src" / "lifted"
        if candidate.is_dir():
            return str(candidate)
    return funcindex.LIFTED_DIR


@lru_cache(maxsize=1)
def units() -> dict[str, Unit]:
    """Every lifted function, NAME -> Unit. Cached: the run asks for it once and the helpers
    below that used to re-parse the lifted headers per call (see `affordable_summarise`)."""
    return funcindex.lifted_units(lifted_dir())


@lru_cache(maxsize=1)
def ranges() -> dict[str, tuple[int, int]]:
    """NAME -> (start, end) for every lifted unit (the address -> function lookup the
    `.edges` caller sites are named with)."""
    return funcindex.unit_ranges(lifted_dir())


def targets_by_address() -> dict[GuestAddress, Target]:
    """Every lifted function keyed by its entry address, the key every output is filed
    under (the name changes when bn6f is renamed; the address does not)."""
    return {GuestAddress(unit.lo): Target(address=GuestAddress(unit.lo), name=FunctionName(name))
            for name, unit in units().items()}


def address_of(name: str) -> GuestAddress | None:
    """The entry address of the lifted unit called NAME, or None."""
    unit = units().get(name)
    return GuestAddress(unit.lo) if unit is not None else None


def unit_at(address: int) -> str:
    """The name of the lifted function whose range holds ADDRESS, or "(no unit)"."""
    return _unit_finder()(address) or "(no unit)"


@lru_cache(maxsize=1)
def _unit_finder():  # type: ignore[no-untyped-def]
    """funcindex's address -> name lookup over the lifted ranges (built once)."""
    return funcindex.unit_finder(ranges())


# ── route lengths: tools/wf/wf/routes.py, read at run time ─────────────

def route_lengths() -> dict[str, int]:
    """Every recorded route's length in game frames, from wf/routes.py (`ALL_ROUTES` and
    `LONG_ROUTES`) -- read here, never written down here, because `beastsp` grows as the
    vidsync recording is imported."""
    from wf.routes import ALL_ROUTES, LONG_ROUTES      # noqa: PLC0415  (read at run time)
    return {str(route.name): int(route.length) for route in (*ALL_ROUTES, *LONG_ROUTES)}


PROF_SPAN = re.compile(r"game\s+\d+\.\.(\d+)")


# ── coverage: the .prof profiles and the .edges call counts ─────────────

class RouteIndex(NamedTuple):
    """What one recorded route's recordings say, in one pass over its files."""
    name: RouteName
    frames: int | None
    frames_from: ToolMessage
    entry_cycles: dict[int, int]                      # even entry address -> cycles
    body_cycles: dict[int, int]                       # unit start -> cycles over its range
    body_instructions: dict[int, int]                 # unit start -> distinct addresses listed
    callers: dict[int, tuple[Caller, ...]]            # even target address -> its callers


class CoverageIndex(NamedTuple):
    """Every recorded route's recordings, folded into per-function lookups."""
    routes: tuple[RouteIndex, ...]

    def for_function(self, address: GuestAddress, end: GuestAddress) -> tuple[RouteCoverage, ...]:
        """This function's row on every recorded route (a route that never executed it is
        still reported: "did not happen" is a fact)."""
        entry = int(address) & ~1
        out: list[RouteCoverage] = []
        for route in self.routes:
            calls = route.callers.get(entry)
            total_calls = sum(caller.count for caller in calls) if calls else None
            per_1000 = (total_calls * 1000.0 / route.frames) if (total_calls and route.frames) else None
            out.append(RouteCoverage(
                route=route.name,
                executed=bool(route.entry_cycles.get(entry)),
                body_executed=bool(route.body_cycles.get(entry)),
                entry_cycles=route.entry_cycles.get(entry, 0),
                function_cycles=route.body_cycles.get(entry, 0),
                body_instructions=route.body_instructions.get(entry, 0),
                frames=route.frames,
                frames_from=route.frames_from,
                calls=total_calls,
                calls_per_1000_frames=round(per_1000, 4) if per_1000 is not None else None,
                callers=calls or ()))
        return tuple(out)


def coverage_files(traces: Path | None = None) -> tuple[tuple[str, Path | None, Path | None], ...]:
    """Every recorded route that has recordings under TRACES, as (name, .prof, .edges)."""
    folder = (TRACES if traces is None else traces) / "coverage"
    names = {path.stem for path in folder.glob("*.prof")} | {path.stem for path in folder.glob("*.edges")}
    return tuple((name,
                  (folder / f"{name}.prof") if (folder / f"{name}.prof").exists() else None,
                  (folder / f"{name}.edges") if (folder / f"{name}.edges").exists() else None)
                 for name in sorted(names))


def build_coverage_index(traces: Path | None = None, lengths: dict[str, int] | None = None
                         ) -> CoverageIndex:
    """One pass over every `.prof` and `.edges`, folded per route.

    A `.prof` line is `ADDRESS CYCLES` (hex address, cycles the run spent with that PC --
    a cycle profile, so it says how long the function ran, not how often it was called);
    its header names the game-frame span it covers. A `.edges` line is
    `SITE TARGET LR COUNT`, hex addresses and a real execution count, at the indirect call
    and jump sites of the ABI export; its `TARGET` carries the Thumb bit, the site's caller
    is the lifted unit holding it. Every executed PC is attributed to the unit whose range
    holds it (a jump into the middle of a function still counts the function as executed).
    """
    lengths = route_lengths() if lengths is None else lengths
    traces = TRACES if traces is None else traces
    starts: list[int] = []
    ends: list[int] = []
    for lo, hi in ranges().values():
        starts.append(lo)
        ends.append(hi)
    order = sorted(range(len(starts)), key=lambda index: starts[index])
    sorted_starts = [starts[index] for index in order]
    sorted_ends = [ends[index] for index in order]
    out: list[RouteIndex] = []
    for name, prof, edges in coverage_files(traces):
        frames, frames_from = None, ToolMessage("unknown")
        entry_cycles: dict[int, int] = {}
        body_cycles: dict[int, int] = {}
        body_instructions: dict[int, int] = {}
        if prof is not None:
            frames, frames_from = _prof_span(prof, name, lengths)
            entry_cycles, body_cycles, body_instructions = _read_prof(prof, sorted_starts, sorted_ends)
        out.append(RouteIndex(name=RouteName(name), frames=frames, frames_from=frames_from,
                              entry_cycles=entry_cycles, body_cycles=body_cycles,
                              body_instructions=body_instructions,
                              callers=_read_edges(edges) if edges is not None else {}))
    return CoverageIndex(routes=tuple(out))


def _prof_span(prof: Path, name: str, lengths: dict[str, int]) -> tuple[int | None, ToolMessage]:
    """A route's length in game frames: wf/routes.py first (it is the route's definition),
    else the span the profile's own header says it covered (the union sweeps such as
    `chips.prof` name no frames at all)."""
    if name in lengths:
        return int(lengths[name]), ToolMessage("wf/routes.py")
    with prof.open(errors="replace") as fh:
        first = fh.readline()
    found = PROF_SPAN.search(first)
    if found:
        return int(found.group(1)), ToolMessage("prof header")
    return None, ToolMessage("unknown")


def _read_prof(prof: Path, starts: Sequence[int], ends: Sequence[int]
               ) -> tuple[dict[int, int], dict[int, int], dict[int, int]]:
    """A `.prof`'s cycles, attributed to lifted units: cycles at a unit's entry address, the
    cycles over every unit's range, and how many distinct addresses of it the profile lists."""
    entry_cycles: dict[int, int] = {}
    body_cycles: dict[int, int] = {}
    body_instructions: dict[int, int] = {}
    with prof.open(errors="replace") as fh:
        for line in fh:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.split()
            if len(parts) < 2:
                continue
            try:
                address = int(parts[0], 16) & ~1
                cycles = int(parts[1])
            except ValueError:
                continue
            index = bisect_right(starts, address) - 1
            if index < 0 or address >= ends[index]:
                continue                      # BIOS, or a range the lifter does not cover
            unit = starts[index]
            body_cycles[unit] = body_cycles.get(unit, 0) + cycles
            body_instructions[unit] = body_instructions.get(unit, 0) + 1
            if address == unit:
                entry_cycles[unit] = entry_cycles.get(unit, 0) + cycles
    return entry_cycles, body_cycles, body_instructions


def _read_edges(edges: Path) -> dict[int, tuple[Caller, ...]]:
    """A `.edges` file as callee entry address -> the call sites that reached it, biggest
    first. `TARGET` is the called address with the Thumb bit set; the site's caller is the
    lifted unit holding the call instruction."""
    counts: dict[int, dict[int, int]] = {}
    with edges.open(errors="replace") as fh:
        for line in fh:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.split()
            if len(parts) < 4:
                continue
            try:
                site = int(parts[0], 16) & ~1
                target = int(parts[1], 16) & ~1
                count = int(parts[3])
            except ValueError:
                continue
            counts.setdefault(target, {})
            seen = counts[target]
            seen[site] = seen.get(site, 0) + count
    out: dict[int, tuple[Caller, ...]] = {}
    finder = _unit_finder()
    for target, sites in counts.items():
        callers: list[Caller] = []
        for site, count in sites.items():
            name = finder(site)
            entry = ranges()[name][0] if name is not None else None
            callers.append(Caller(site=f"0x{site:08X}",
                                  caller=f"0x{entry:08X}" if entry is not None else "",
                                  name=FunctionName(name or "(no unit)"), count=count))
        out[target] = tuple(sorted(callers, key=lambda row: (-row.count, row.site)))
    return out


# ── captured calls: the entry states and what the original did from them ─

def entry_snapshots(name: FunctionName, snapshots: Path | None = None) -> tuple[Path, ...]:
    """The function's captured entry states (build-snapshots/NAME/snap_*.bin), newest capture
    first. The `.exit.bin` files the capture left beside them are the game's own exits, not
    entry states, and are not entry states for a replay."""
    folder = (SNAPSHOTS if snapshots is None else snapshots) / str(name)
    if not folder.is_dir():
        return ()
    return tuple(sorted((path for path in folder.glob("*.bin")
                         if not path.name.endswith(".exit.bin")),
                        key=lambda path: path.stat().st_mtime, reverse=True))


_TEMPLATE_LOCK = threading.Lock()
_template_cache: bytes | None = None


def io_template() -> bytes:
    """What mGBA's I/O registers read back after --call-snaps loads a snapshot
    (runtime_evidence.py's `_io_template`, cached per process: it is a fixed answer for a
    given mgba_run and ROM, and running it once per function would cost the whole run an
    mGBA boot per function)."""
    global _template_cache
    with _TEMPLATE_LOCK:
        if _template_cache is None:
            _template_cache = rt._io_template()   # noqa: SLF001  (that module's own helper)
        return _template_cache


def replay(name: FunctionName, states: int, work_dir: Path, snapshots: Path | None = None,
           timeout: int = CALL_TIMEOUT) -> tuple[rt.Probe, ...]:
    """Run the function in the ORIGINAL game from K of its captured entry states and report
    what each call did (did it return, which registers changed, which memory words).

    runtime_evidence.py's `run_original` does this for its judge; this is the same replay
    with the budget this layer runs at: K states (--states), a wall clock of TIMEOUT seconds
    for the whole pass, and MAX_STEPS instructions per call instead of the 36,000 that fit
    the judge run. The entry snapshots are written into WORK_DIR (the caller removes it:
    each state is SNAP_MEMORY_BYTES of guest memory, and 8,000 functions' worth would fill
    the disk)."""
    bases = [rt.parse_snap(path) for path in entry_snapshots(name, snapshots)]
    if not bases:
        return ()
    entries = work_dir / "entries"
    entries.mkdir(parents=True, exist_ok=True)
    template = io_template()
    chosen = tuple(bases[index % len(bases)] for index in range(states))
    for index, snap in enumerate(chosen):
        rewritten = rt._rewrite_entry_io(snap, template)      # noqa: SLF001  (that helper)
        (entries / f"snap_{index:03d}.bin").write_bytes(
            struct.pack("<19I", *rewritten.header) + rewritten.memory)
    try:
        subprocess.run([str(rt.find_mgba_run()), str(rt._game_rom()), "--quiet",   # noqa: SLF001
                        "--frames", "1", "--bios", str(rt._game_bios()),            # noqa: SLF001
                        "--call-snaps", str(entries), str(MAX_STEPS)],
                       capture_output=True, check=False, timeout=timeout)
    except (OSError, subprocess.SubprocessError):
        raise RuntimeError(f"mgba_run did not run ({entries})") from None
    probes: list[rt.Probe] = []
    for index in range(states):
        entry = rt.parse_snap(entries / f"snap_{index:03d}.bin")
        exit_path = entries / f"snap_{index:03d}.exit.bin"
        if not exit_path.exists():
            probes.append(rt.Probe(returned=False, problem=ToolMessage(
                f"state {index} did not return within {MAX_STEPS} instructions")))
            continue
        probes.append(rt._probe_from_exit(entry, rt.parse_snap(exit_path)))   # noqa: SLF001
    return tuple(probes)


ARGUMENT_REGISTERS = ("r0", "r1", "r2", "r3", "sp")


def register_stats(register: str, values: Sequence[int]) -> RegisterStats:
    """One register's values across the probes: how many probes carried one, how many were
    distinct, the range, and up to eight of them."""
    distinct = sorted(set(values))
    return RegisterStats(reg=register, samples=len(values), distinct=len(distinct),
                         lowest=distinct[0] if distinct else None,
                         highest=distinct[-1] if distinct else None,
                         values=tuple(distinct[:8]))


def argument_stats(probes: Sequence[rt.Probe]) -> tuple[tuple[RegisterStats, ...], tuple[RegisterStats, ...]]:
    """What the replayed calls were given (r0..r3 and sp at the entry, the ARM calling
    convention's arguments) and what they returned (r0 at the exit). Only the probes that
    returned carry an exit, so the returns are counted over those."""
    returned = [probe for probe in probes if probe.returned and probe.entry is not None]
    arguments = tuple(register_stats(register, [_register_value(probe, register, out=False)
                                                for probe in returned])
                      for register in ARGUMENT_REGISTERS)
    returns = (register_stats("r0", [_register_value(probe, "r0", out=True)
                                     for probe in returned if probe.exit is not None]),)
    return arguments, returns


def _register_value(probe: rt.Probe, register: str, out: bool) -> int:
    """A probe's value in REGISTER: at the entry snapshot, or (OUT) at the exit snapshot."""
    snap = probe.exit if out else probe.entry
    return rt._register(snap.header, register) if snap is not None else 0    # noqa: SLF001


# ── RAM symbols: what `region_of` answers, in one lookup ───────────────

RAM_SECTIONS: tuple[tuple[int, int, str], ...] = (
    (0x02000000, 0x40000, "EWRAM"), (0x03000000, 0x8000, "IWRAM"),
    (0x05000000, 0x400, "PALETTE"), (0x06000000, 0x18000, "VRAM"),
    (0x07000000, 0x400, "OAM"), (0x0E000000, 0x8000, "SRAM"),
)


@lru_cache(maxsize=1)
def region_table() -> tuple[tuple[tuple[int, int, str], "array[int]", tuple[str, ...]], ...]:
    """One answer per RAM address for `runtime_evidence.region_of`: the bn6f RAM symbol that
    contains it, or the bare region name when none does.

    That function walks its 22,000-symbol list for every address, and `summarise` asks it
    once per changed word per probe (a map loader changes 26,000 words a call). Filling the
    same answers into one array per RAM region -- back to front, so the symbol `region_of`
    would have returned first still wins an overlap -- answers each call with one index."""
    syms = rt._ram_symbols()          # noqa: SLF001  (the shared table; built once per process)
    names = tuple(name for _start, _end, name in syms)
    out: list[tuple[tuple[int, int, str], array[int], tuple[str, ...]]] = []
    for base, size, plain in RAM_SECTIONS:
        table = array("i", bytes(4 * size))          # zeros: no symbol covers this address
        for index in range(len(syms) - 1, -1, -1):
            start, end, _name = syms[index]
            for address in range(max(start, base), min(end, base + size)):
                table[address - base] = index + 1
        out.append(((base, size, plain), table, names))
    return tuple(out)


def region_of(address: int) -> str:
    """`runtime_evidence.region_of` from the table: the bn6f RAM symbol that contains
    ADDRESS, the bare region name when none does, "RAM" outside the game's RAM."""
    for (base, size, plain), table, names in region_table():
        if base <= address < base + size:
            index = table[address - base]
            return names[index - 1] if index else plain
    return "RAM"


@contextlib.contextmanager
def affordable_summarise() -> Generator[None]:
    """`runtime_evidence.summarise`, made affordable at this layer's scale (thousands of
    functions in one run) and unchanged in what it reports.

    Three of its helpers do not survive being called per function:
      * `funcindex.lifted_units()` re-parses every lifted header (0.35 s) on every call;
      * `region_of()` walks 22,000 RAM symbols per address, once per changed word per probe;
        `region_table` answers the same names in one array lookup;
      * `_route_coverage()` re-reads a whole `.prof` per function; this layer builds richer
        coverage (cycles, callers, calls per 1000 frames) in one pass, so it stands down and
        `RuntimeFacts.route_coverage` is filled in from that index afterwards.
    """
    original_units, original_region = funcindex.lifted_units, rt.region_of
    original_coverage = rt._route_coverage                        # noqa: SLF001  (stood down below)
    funcindex.lifted_units = units                     # the cached copy (no directory argument)
    rt.region_of = region_of                           # noqa: SLF001  (the O(1) table)
    rt._route_coverage = lambda name: ()               # noqa: SLF001
    try:
        yield
    finally:
        funcindex.lifted_units = original_units
        rt.region_of = original_region                 # noqa: SLF001
        rt._route_coverage = original_coverage         # noqa: SLF001


def summarise(name: FunctionName, address: GuestAddress, probes: Sequence[rt.Probe],
              coverage: Sequence[RouteCoverage]) -> rt.RuntimeFacts:
    """The RuntimeFacts block for one function: registers in and out, the RAM it writes
    grouped by the bn6f symbol that holds them, the I/O registers it writes -- plus this
    layer's route coverage in the field runtime_evidence fills from the `.prof` files."""
    with affordable_summarise():
        facts = rt.summarise(name, address, probes)
    lines = tuple(ToolMessage(f"{route.route}: {route.calls or 0} calls / {route.frames or 0} frames"
                              f" ({(route.calls_per_1000_frames or 0):.1f}/1k)"
                              f", {route.function_cycles} cycles")
                  for route in coverage if route.executed or route.body_executed or route.calls)
    return facts.model_copy(update={"route_coverage": lines})


# ── the cache: what a record was made from ──────────────────────────────

def _sha1(path: Path) -> str:
    digest = hashlib.sha1()                        # noqa: S324  (a cache key, not a security hash)
    with path.open("rb") as fh:
        for block in iter(lambda: fh.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def _stamp(path: Path) -> ToolMessage:
    """`<sha1> <mtime> <size>` for a file (the mgba_run binary and the ROM, whose bytes and
    mtimes decide whether a replay would give the same answer)."""
    try:
        stat = path.stat()
    except OSError:
        return ToolMessage("missing")
    return ToolMessage(f"{_sha1(path)} {int(stat.st_mtime)} {stat.st_size}")


def coverage_fingerprint(traces: Path | None = None) -> ToolMessage:
    """One hash over every `.prof` and `.edges` mtime: the coverage half of the cache key, so
    a route that grew (`beastsp`) re-reads coverage without replaying a single call."""
    parts: list[str] = []
    for name, prof, edges in coverage_files(traces):
        for path in (prof, edges):
            if path is not None:
                parts.append(f"{name}/{path.suffix}:{int(path.stat().st_mtime)}")
    return ToolMessage(hashlib.sha1("\n".join(sorted(parts)).encode()).hexdigest()[:16])   # noqa: S324


def inputs_for(name: FunctionName, states: int, snapshots: Path, traces: Path) -> Inputs:
    """The inputs one function's record is made from, as the cache reads them."""
    snaps = entry_snapshots(name, snapshots)
    newest = max((path.stat().st_mtime for path in snaps), default=0.0)
    return Inputs(mgba_run=_stamp(rt.PROJECT_ROOT_BIN), rom=_stamp(ROM),
                  snapshots=ToolMessage(f"{int(newest)} {len(snaps)}"),
                  states=states, coverage=coverage_fingerprint(traces))


def record_path(facts_dir: Path, address: GuestAddress) -> Path:
    """Where one function's record lives: build/facts/by_address/0x08XXXXXX.json."""
    return facts_dir / BY_ADDRESS / f"0x{int(address):08X}.json"


def read_record(path: Path) -> FunctionFacts | None:
    """A record already on disk, or None when it is missing or unreadable (a half-written
    file is rebuilt rather than trusted)."""
    try:
        return FunctionFacts.model_validate_json(path.read_text())
    except (OSError, ValueError):
        return None


def write_record(path: Path, record: FunctionFacts) -> Path:
    """Write one function's record, atomically (the run writes it from several threads and a
    reader may be looking at it)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + ".partial")
    temp.write_text(record.model_dump_json(indent=2) + "\n")
    temp.replace(path)
    return path


def note_progress(facts_dir: Path, line: str) -> None:
    """One line in build/facts/progress.txt: what the run did, as it did it (a run lasts
    hours; this is what says where it got to)."""
    facts_dir.mkdir(parents=True, exist_ok=True)
    with (facts_dir / "progress.txt").open("a") as fh:
        fh.write(f"{time.strftime('%Y-%m-%d %H:%M:%S')} {line}\n")


# ── step one: what the recordings show, for every function ──────────────

def plan(facts_dir: Path, only: Sequence[str] = (), states: int = DEFAULT_STATES,
         limit: int = DEFAULT_LIMIT, traces: Path | None = None,
         snapshots: Path | None = None) -> tuple[tuple[Target, ...], tuple[Target, ...],
                                                tuple[RouteName, ...], tuple[ToolMessage, ...]]:
    """Build the coverage of every function in the run and write the records that need no
    mGBA (every function whose captured calls have not changed, and every function that has
    no captured calls).

    Returns (targets, to_replay, routes, notes): the functions in the run, the ones whose
    facts still have to be gathered, the routes that were read, and what was decided."""
    traces = TRACES if traces is None else traces
    snapshots = SNAPSHOTS if snapshots is None else snapshots
    wanted = set(only)
    everything = targets_by_address()
    if wanted:
        by_name = {str(target.name): target for target in everything.values()}
        by_hex = {f"0x{int(target.address):08X}": target for target in everything.values()}
        selected = [by_name.get(item) or by_hex.get(item.upper())
                    or by_hex.get("0x" + item.upper().removeprefix("0X")) for item in wanted]
        missing = [item for item, target in zip(wanted, selected) if target is None]
        targets = sorted((target for target in selected if target is not None),
                         key=lambda target: target.address)
        notes = [ToolMessage(f"{item}: not a lifted function (nothing to record)") for item in missing]
    else:
        targets = sorted(everything.values(), key=lambda target: target.address)
        notes = []
    if limit:
        targets = targets[:limit]

    index = build_coverage_index(traces)
    route_names = tuple(route.name for route in index.routes)
    coverage_of = coverage_fingerprint(traces)
    notes.append(ToolMessage(f"read {len(index.routes)} recorded route(s): "
                             f"{', '.join(str(name) for name in route_names)}"))
    to_replay: list[Target] = []
    kept = 0
    replayed = 0
    for target in targets:
        unit = units()[target.name]
        path = record_path(facts_dir, target.address)
        record = read_record(path)
        inputs = inputs_for(target.name, states, snapshots, traces)
        snaps = int(inputs.snapshots.split()[-1])
        coverage = index.for_function(target.address, GuestAddress(unit.hi))
        if record is not None and record.inputs is not None and _same_facts(record, inputs):
            if record.inputs.coverage != coverage_of:
                write_record(path, record.model_copy(update={
                    "routes": coverage, "name": target.name, "inputs": inputs,
                    "reached": _reached(coverage), "calls": _calls(coverage),
                    "calls_per_1000_frames": _calls_per_1000(coverage),
                    "cycles": _cycles(coverage), "routes_reached": _routes_reached(coverage)}))
            kept += 1                 # the captured calls are the same: keep the facts, refresh
            continue                   # the coverage if a recording moved under it
        if not snaps:
            # no captured calls: the record is its coverage, and no mGBA could add to it
            write_record(path, _record(target, unit, coverage, inputs, snaps=0, facts=None,
                                       error=None))
            kept += 1
            continue
        to_replay.append(target)
        replayed += 1
    notes.append(ToolMessage(f"{len(targets)} function(s): {kept} already recorded (or recorded "
                             f"without mGBA), {replayed} to replay"))
    return tuple(targets), tuple(to_replay), route_names, tuple(notes)


def _same_facts(record: FunctionFacts, inputs: Inputs) -> bool:
    """Whether a record's facts still stand: the mGBA binary, the ROM, the function's
    captured entry states and the number of states replayed are all unchanged. (The
    `.prof`/`.edges` mtimes are the other half of the key, `Inputs.coverage`, and only the
    coverage is re-read when those move.)"""
    assert record.inputs is not None
    return (record.inputs.mgba_run == inputs.mgba_run
            and record.inputs.rom == inputs.rom
            and record.inputs.snapshots == inputs.snapshots
            and record.inputs.states == inputs.states)


def _reached(coverage: Sequence[RouteCoverage]) -> bool:
    return any(route.executed or route.body_executed or route.calls for route in coverage)


def _calls(coverage: Sequence[RouteCoverage]) -> int:
    return sum(route.calls or 0 for route in coverage)


def _calls_per_1000(coverage: Sequence[RouteCoverage]) -> float:
    calls = sum(route.calls or 0 for route in coverage)
    frames = sum(route.frames or 0 for route in coverage)
    return round(calls * 1000.0 / frames, 4) if (calls and frames) else 0.0


def _cycles(coverage: Sequence[RouteCoverage]) -> int:
    return sum(route.function_cycles for route in coverage)


def _routes_reached(coverage: Sequence[RouteCoverage]) -> int:
    return sum(1 for route in coverage if route.executed or route.body_executed or route.calls)


def _record(target: Target, unit: Unit, coverage: Sequence[RouteCoverage], inputs: Inputs,
            snaps: int, facts: rt.RuntimeFacts | None, error: str | None, probes: int = 0,
            returned: int = 0, arguments: Sequence[RegisterStats] = (),
            returns: Sequence[RegisterStats] = ()) -> FunctionFacts:
    """One function's record, with the coverage and (when it could be gathered) the facts."""
    return FunctionFacts(address=target.address, address_hex=f"0x{int(target.address):08X}",
                         name=target.name, end=GuestAddress(unit.hi), asm=unit.asm,
                         reached=_reached(coverage), captured=snaps > 0, captured_states=snaps,
                         states=inputs.states, probes=probes, returned=returned,
                         routes=tuple(coverage), calls=_calls(coverage),
                         calls_per_1000_frames=_calls_per_1000(coverage), cycles=_cycles(coverage),
                         routes_reached=_routes_reached(coverage),
                         arguments=tuple(arguments), returns=tuple(returns),
                         facts=facts, facts_error=error, inputs=inputs,
                         built=ToolMessage(time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())))


# ── step two: replay the captured calls ─────────────────────────────────

def gather(facts_dir: Path, to_replay: Sequence[Target], states: int = DEFAULT_STATES,
           jobs: int = DEFAULT_JOBS, traces: Path | None = None,
           snapshots: Path | None = None,
           scratch: Path | None = None) -> tuple[int, int, int, float, tuple[ToolMessage, ...]]:
    """Replay K captured entry states per function, in JOBS threads, and fold what the
    original did into each function's record. A function whose replay crashes or times out
    is recorded with its `facts_error`; it never stops the run.

    Returns (with_facts, errors, skipped, seconds, notes)."""
    if not to_replay:
        return 0, 0, 0, 0.0, ()
    traces = TRACES if traces is None else traces
    snapshots = SNAPSHOTS if snapshots is None else snapshots
    started = time.monotonic()
    index = build_coverage_index(traces)
    rt._ram_symbols()             # noqa: SLF001  (one RAM symbol table for the whole run, not one per thread)
    scratch = scratch or facts_dir / "scratch"
    scratch.mkdir(parents=True, exist_ok=True)
    counts = {"facts": 0, "errors": 0, "skipped": 0}
    lock = threading.Lock()
    notes: list[ToolMessage] = []
    last_note = started

    def one(target: Target) -> None:
        unit = units()[target.name]
        coverage = index.for_function(target.address, GuestAddress(unit.hi))
        inputs = inputs_for(target.name, states, snapshots, traces)
        snaps = len(entry_snapshots(target.name, snapshots))
        probes: tuple[rt.Probe, ...] = ()
        facts: rt.RuntimeFacts | None = None
        error: str | None = None
        returned = 0
        try:
            with tempfile.TemporaryDirectory(dir=scratch, prefix=f"{int(target.address):08X}_") as work:
                probes = replay(target.name, states, Path(work), snapshots=snapshots)
            returned = sum(1 for probe in probes if probe.returned)
            if returned:
                facts = summarise(target.name, target.address, probes, coverage)
            else:
                reasons = [str(probe.problem) for probe in probes if probe.problem]
                error = (reasons[0] if reasons else
                         f"no probe returned of {states} (the original did not come back)")
        except Exception as error_raised:                     # noqa: BLE001  (one function never stops the run)
            error = f"{type(error_raised).__name__}: {error_raised}"[:200]
        arguments, returns = argument_stats(probes)
        record = _record(target, unit, coverage, inputs, snaps=snaps, facts=facts, error=error,
                         probes=len(probes), returned=returned, arguments=arguments, returns=returns)
        write_record(record_path(facts_dir, target.address), record)
        with lock:
            if returned:
                counts["facts"] += 1
            elif error:
                counts["errors"] += 1
            else:
                counts["skipped"] += 1
            if error and len(notes) < 40:
                notes.append(ToolMessage(f"{target.name} (0x{int(target.address):08X}): {error}"))

    note_progress(facts_dir, f"replaying {len(to_replay)} function(s) with {states} state(s) each, "
                             f"{jobs} at a time")
    with ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
        for number, _ in enumerate(pool.map(one, to_replay), start=1):
            now = time.monotonic()
            if number % 25 == 0 or number == len(to_replay):
                note_progress(facts_dir, f"{number}/{len(to_replay)} replayed "
                                         f"({counts['facts']} with facts, {counts['errors']} errors, "
                                         f"{now - started:.0f}s)")
            if now - last_note >= 300:
                last_note = now
                note_progress(facts_dir, f"{number}/{len(to_replay)} replayed after "
                                         f"{now - started:.0f}s")
    shutil.rmtree(scratch, ignore_errors=True)
    seconds = time.monotonic() - started
    note_progress(facts_dir, f"replays finished in {seconds:.0f}s: {counts['facts']} with facts, "
                             f"{counts['errors']} errors, {counts['skipped']} without captures")
    return counts["facts"], counts["errors"], counts["skipped"], seconds, tuple(notes)


# ── step three: the index and the report ───────────────────────────────

def write_index(facts_dir: Path, targets: Sequence[Target]) -> FactsReport:
    """Write build/facts/coverage.tsv (one row per function) and say what the run produced."""
    rows = ["# address\tname\treached\troutes\tcalls/1k\tcaptured\tfacts\terror"]
    reached = captured = with_facts = errors = 0
    reasons: dict[str, int] = {}
    examples: list[ToolMessage] = []
    for target in targets:
        path = record_path(facts_dir, target.address)
        record = read_record(path)
        if record is None:
            rows.append(f"0x{int(target.address):08X}\t{target.name}\t-\t-\t-\t-\t-\t(no record)")
            continue
        reached += 1 if record.reached else 0
        captured += 1 if record.captured else 0
        if record.facts is not None and record.returned:
            with_facts += 1
        if record.facts_error:
            errors += 1
            reason = record.facts_error.split(":")[0][:80]
            reasons[reason] = reasons.get(reason, 0) + 1
        if len(examples) < 3 and record.facts is not None:
            examples.append(ToolMessage(str(path)))
        rows.append("\t".join((
            record.address_hex, record.name, "yes" if record.reached else "no",
            ",".join(str(route.route) for route in record.routes
                     if route.executed or route.body_executed or route.calls) or "-",
            f"{record.calls_per_1000_frames:g}", str(record.captured_states),
            "yes" if (record.facts is not None and record.returned) else "no",
            record.facts_error or "-")))
    facts_dir.mkdir(parents=True, exist_ok=True)
    (facts_dir / "coverage.tsv").write_text("\n".join(rows) + "\n")
    top = tuple(ToolMessage(f"{reason}: {count}") for reason, count in
                sorted(reasons.items(), key=lambda row: (-row[1], row[0]))[:5])
    note_progress(facts_dir, f"coverage.tsv: {len(rows) - 1} function(s), {reached} reached, "
                             f"{with_facts} with runtime facts, {errors} errors")
    return FactsReport(functions=len(targets), reached=reached, captured=captured,
                       with_facts=with_facts, facts_errors=errors, error_reasons=top,
                       rows=len(rows) - 1, examples=tuple(examples))


# ── the three steps, as the graph calls them ───────────────────────────

class Planned(Result):
    """What the run is: the functions in it, the ones whose facts still have to be gathered."""
    targets: tuple[Target, ...]
    replay: tuple[Target, ...]
    routes: tuple[RouteName, ...]
    notes: tuple[ToolMessage, ...]


class Gathered(Result):
    """What the replays produced."""
    facts: int
    errors: int
    skipped: int
    seconds: float
    notes: tuple[ToolMessage, ...]


def step_plan(facts_dir: Path, only: Sequence[str], states: int, limit: int) -> Planned:
    targets, replay, routes, notes = plan(facts_dir, only, states, limit)
    return Planned(targets=targets, replay=replay, routes=routes, notes=notes)


def step_gather(facts_dir: Path, planned: Planned, states: int, jobs: int) -> Gathered:
    facts, errors, skipped, seconds, notes = gather(facts_dir, planned.replay, states, jobs)
    return Gathered(facts=facts, errors=errors, skipped=skipped, seconds=seconds, notes=notes)


def step_write(facts_dir: Path, planned: Planned, gathered: Gathered) -> FactsReport:
    report = write_index(facts_dir, planned.targets)
    return report.model_copy(update={
        "seconds": round(gathered.seconds, 1),
        "routes": planned.routes,
        "notes": planned.notes + gathered.notes})