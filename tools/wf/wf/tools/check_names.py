"""`wf check-names`: check a function's name against what the game did at runtime, and against
what happens when the function is taken away -- with a calibration gate built in.

A name is a claim about what a function does. `wf investigate` asks a model to read that claim
off the assembly; `wf facts` records what the ORIGINAL game did at runtime for every lifted
function, with no model in the loop; `wf intervene` breaks a function on purpose and watches the
machine. This module is where the three meet -- and no model ever sees the result:

  1. **Claims, written BLIND.** A model is shown a name and its symbols/db description, and
     nothing else (neither the runtime facts nor the assembly), and writes one small JSON object:
     checkable `claims` (`reached_in_scene(battle)`, `writes_ram(oBattleState)`,
     `reads_or_writes_io(palette)`, `returns_value(pointer)`, `called_every_frame(no)`,
     `has_argument(0)`) plus one `prediction`: an intervention (`skip` / `return:K`) and 1-3
     predicates in `wf intervene`'s predicate language over a window in frames. A name whose
     prediction holds is a name whose causal claim survived being taken away.
  2. **Passive check (script).** Each claim against the facts json: `pass` / `fail` / `unknown`
     (the facts cannot show it: write-only sound and DMA registers, one replayed call). Name
     level: `contradicted` if any claim fails, `consistent` if every checkable one passes,
     `unchecked` otherwise.
  3. **Active check (script).** The prediction runs through `wf intervene` on the route the
     facts pick, against a `null` control on the same function, route and window. The
     predicates are evaluated against the anchor diffs: `held` / `failed` / `unreached` /
     `hung` / `nondeterministic` (plus `indeterminate`, `none`, `error` for what the harness,
     not the model, has to say).
  4. **Calibration gate, built in.** Every name in the batch is also checked against ANOTHER
     randomly chosen name in the batch: that partner's claims are evaluated against this
     function's facts, and that partner's prediction is run against THIS function's
     intervention (swapped controls). A level is granted only where the batch discriminates --
     the swapped claim pass rate must sit at least `SWAPPED_MIN_CLAIM_MARGIN` below the true
     one, and the swapped prediction held rate below `SWAPPED_MAX_PREDICTION_RATE`. Otherwise
     the report says the checker is not discriminating and grants nothing.

Nothing here writes under bn6f/ or symbols/db; a run's output is its own directory under
build-wf-runs/. No model ever judges the outcome: the model writes the claims blind, and this
file does the arithmetic.
"""
from __future__ import annotations

import json
import random
import re
import shutil
import threading
import time
from collections.abc import Sequence
from enum import Enum
from pathlib import Path
from typing import Annotated, Literal, NamedTuple, cast

from pydantic import Field, ValidationError, model_validator

from wf.repo import REPO, RUNS
from wf.tools import facts as ft
from wf.tools import intervene as it
from wf.tools import structfields as sf
from wf.tools.facts import FunctionFacts, RouteCoverage
from wf.types import (FrameCount, FunctionName, GuestAddress, ModelSpec, Result, RouteName,
                      ToolMessage)

import llm  # noqa: E402  # tools/llm.py: the model providers (wf.repo put tools/ on the path)

PROJECT = "recomp-check-names"
SYMBOLS_DB = REPO / "symbols/db"                 # the names and their descriptions (read only)
FACTS = ft.FACTS                                 # build/facts: by_address/0xADDR.json, coverage.tsv
AUDIT_TSV = REPO / "docs/reviews/2026-10-21-minimax-name-audit.tsv"

DEFAULT_COUNT = 24                               # names taken when none are named (--count)
DEFAULT_JOBS = 4                                 # model calls in flight (--jobs)
DEFAULT_MODEL = ModelSpec("hyper/minimax-m3")     # MiniMax is out of quota; the same model
                                                     # on Hyper is the brief's substitute
DEFAULT_FRAMES = 120                             # a prediction's window, in game frames
DEFAULT_FRAMES_WINDOW = 1200                     # how far into a route to hunt for the first call
MAX_ROUTE_TRIES = 3                              # candidate routes to try for one function
MAX_PREDICATES = 3
MAX_TOKENS = 2000
MODEL_TIMEOUT = 180                              # one model call, seconds (wf's convention)
MODEL_RETRIES = 3
USAGE_LOG = REPO / "tools/convert/usage.jsonl"    # tools/llm.py logs every call here (shared)

# ── the calibration gate ────────────────────────────────────────────────
SWAPPED_MAX_PREDICTION_RATE = 0.15   # a swapped prediction must hold less often than this
SWAPPED_MIN_CLAIM_MARGIN = 0.30       # the true claim pass rate minus the swapped one
EVERY_FRAME_PER_1000 = 500.0          # "called every frame" = this many calls / 1000 frames

# ── the claim vocabulary ────────────────────────────────────────────────

Scene = Literal["battle", "overworld", "menu", "title", "cutscene"]
SCENES: tuple[Scene, ...] = ("battle", "overworld", "menu", "title", "cutscene")

IOGroup = Literal["oam", "palette", "vram", "display", "sound", "dma", "input"]
IO_GROUPS: tuple[IOGroup, ...] = ("oam", "palette", "vram", "display", "sound", "dma", "input")

ReturnKind = Literal["bool", "small_int", "pointer", "none"]
RETURN_KINDS: tuple[ReturnKind, ...] = ("bool", "small_int", "pointer", "none")


class ReachedInScene(Result):
    """`reached_in_scene(battle)`: a recorded route in that scene executed this function."""
    kind: Literal["reached_in_scene"] = "reached_in_scene"
    scene: Scene


class WritesRam(Result):
    """`writes_ram(oBattleState)`: the replayed calls wrote bytes inside a RAM symbol whose bn6f
    name starts with PREFIX (case-insensitive; `_` separates the fields)."""
    kind: Literal["writes_ram"] = "writes_ram"
    symbol_prefix: str = Field(min_length=2, max_length=60)


class ReadsOrWritesIo(Result):
    """`reads_or_writes_io(palette)`: the function touched that part of the machine. Only
    `oam`/`palette`/`vram` (through the bn6f buffers behind them) and `display` (the display
    registers that read back) can be decided; the rest are always `unknown`."""
    kind: Literal["reads_or_writes_io"] = "reads_or_writes_io"
    group: IOGroup


class ReturnsValue(Result):
    """`returns_value(pointer)`: what r0 held when the replayed calls returned."""
    kind: Literal["returns_value"] = "returns_value"
    value: ReturnKind


class CalledEveryFrame(Result):
    """`called_every_frame(no)`: the instrumented call sites call it more, or less, often than
    once every two frames."""
    kind: Literal["called_every_frame"] = "called_every_frame"
    yes: bool


class HasArgument(Result):
    """`has_argument(n)`: N is 0..3 for r0..r3 and 4 for the stack pointer; the caller passes
    more than one value in that register across the replayed calls."""
    kind: Literal["has_argument"] = "has_argument"
    n: int = Field(ge=0, le=4)


Claim = Annotated[ReachedInScene | WritesRam | ReadsOrWritesIo | ReturnsValue
                   | CalledEveryFrame | HasArgument, Field(discriminator="kind")]

CLAIM_KINDS: tuple[str, ...] = ("reached_in_scene", "writes_ram", "reads_or_writes_io",
                                "returns_value", "called_every_frame", "has_argument")


class DoIt(Result):
    """The prediction's intervention -- make the function do nothing (`skip`), or return a
    constant (`return:K`, 0..255) -- and what the machine must show if the name is right."""
    intervention: Literal["skip", "return:K"]
    k: int | None = None
    predicates: tuple[str, ...] = ()
    window: int = Field(default=DEFAULT_FRAMES, ge=10, le=2000)

    @model_validator(mode="after")
    def _consistent(self) -> "DoIt":
        if self.intervention == "skip" and self.k is not None:
            raise ValueError("skip carries no K")
        if self.intervention == "return:K" and (self.k is None or not 0 <= self.k <= 255):
            raise ValueError("return:K needs a K in 0..255")
        if not 1 <= len(self.predicates) <= MAX_PREDICATES:
            raise ValueError(f"a prediction needs 1..{MAX_PREDICATES} predicates")
        return self

    def label(self) -> str:
        """`return:1 / differs(anchor=oam, within=60) x window=120`, the way a report row says
        what was run."""
        what = self.intervention if self.intervention == "skip" else f"return:{self.k}"
        return f"{what} / {', '.join(self.predicates)} x window={self.window}"

    @property
    def kind(self) -> str:
        """The `wf intervene` intervention kind (`skip` / `return`)."""
        return "skip" if self.intervention == "skip" else "return"


class NoEffect(Result):
    """The model says the name implies no visible effect at all, and why."""
    intervention: Literal["none"] = "none"
    reason: str = Field(min_length=1)


Action = Annotated[DoIt | NoEffect, Field(discriminator="intervention")]


class Reply(Result):
    """What one blind call gave back: the claims, the prediction, and a line of notes."""
    claims: tuple[Claim, ...] = ()
    prediction: Action
    notes: ToolMessage = ToolMessage("")

    @model_validator(mode="after")
    def _something_to_check(self) -> "Reply":
        if not self.claims and isinstance(self.prediction, NoEffect):
            raise ValueError("no claims and no prediction: nothing to check")
        return self


class Answer(Result):
    """One function's blind answer as it is recorded: the reply, or what kept us from one."""
    name: FunctionName
    address: GuestAddress
    reply: Reply | None = None
    tokens: int = 0
    seconds: float = 0.0
    retries: int = 0
    problem: ToolMessage | None = None


# ── what the facts say, and what a claim against them comes to ──────────

class ClaimCheck(Result):
    """One claim against one facts record: `pass`, `fail` (the facts contradict it) or
    `unknown` (the facts cannot show it -- the field that would decide it is missing)."""
    claim: Claim
    status: Literal["pass", "fail", "unknown"]
    observed: ToolMessage


Passive = Literal["consistent", "contradicted", "unchecked"]

NO_FACTS = "no runtime facts (no captured call replayed into a return)"


def read_record(address_hex: str, facts_dir: Path = FACTS) -> FunctionFacts | None:
    """The `wf facts` record filed under 0xADDR, or None when it is missing or unreadable.

    `address_hex` is whatever the record lives under; the file name is case-sensitive, so
    look it up as written (`0x08001234.json`, never `0X08001234.JSON`)."""
    return ft.read_record(facts_dir / ft.BY_ADDRESS / f"{address_hex}.json")


# routes -> scene, in one small table (the brief's). A route that is not in it is `None`: the
# checker says nothing about a scene it cannot name, and the run's notes ask which one it is.
ROUTE_SCENES: tuple[tuple[str, Scene], ...] = (
    ("battle", "battle"),          # the prefix covers battles1, battles2, battlesweep
    ("chips", "battle"),
    ("story", "overworld"),        # the prefix covers story1..story9
    ("maps", "overworld"),
    ("wander", "overworld"),
    ("menus", "menu"),             # the prefix covers menus2, menus3
    ("title_mash_a", "title"),
)


def scene_of(route: str) -> Scene | None:
    """The scene a recorded route shows, or None when the route is not in the table (a
    speedrun that spans every scene, a fuzzing sweep, a route added after this was written)."""
    for prefix, scene in ROUTE_SCENES:
        if route == prefix or route.startswith(prefix) or route.startswith(prefix + "_"):
            return scene
    return None


def unclear_routes(routes: Sequence[str]) -> tuple[str, ...]:
    """The routes in the batch whose scene the table cannot name -- asked about in the notes,
    because a claim about them cannot be checked."""
    return tuple(sorted({route for route in routes if scene_of(route) is None}))


def reached_routes(record: FunctionFacts) -> tuple[RouteCoverage, ...]:
    """The recordings that show this function being executed."""
    return tuple(route for route in record.routes
                 if route.executed or route.body_executed or route.calls)


def check_claim(claim: Claim, record: FunctionFacts | None) -> ClaimCheck:
    """One claim against one facts record. Pure: it reads the record and answers, and never asks
    anything of the emulator, the model or the disk."""
    if record is None:
        return ClaimCheck(claim=claim, status="unknown",
                          observed=ToolMessage("no facts record for this function"))
    if isinstance(claim, ReachedInScene):
        return _reached_in_scene(claim, record)
    if isinstance(claim, WritesRam):
        return _writes_ram(claim, record)
    if isinstance(claim, ReadsOrWritesIo):
        return _reads_or_writes_io(claim, record)
    if isinstance(claim, ReturnsValue):
        return _returns_value(claim, record)
    if isinstance(claim, CalledEveryFrame):
        return _called_every_frame(claim, record)
    return _has_argument(claim, record)


def _reached_in_scene(claim: ReachedInScene, record: FunctionFacts) -> ClaimCheck:
    """Was this function reached in `claim.scene`? Prefers `record.scenes_at_call` -- the per-call
    scene each captured call's own entry snapshot says the game was in -- because a route is an
    input tape, not a scene (`story2` runs a battle, opens a menu and returns to the map).
    Falls back to the route->scene table when no per-call scenes were recorded (the captures
    were not replayed, or the replay produced no scenes)."""
    captured_scenes = seen_scenes_at_call(record)
    if captured_scenes is not None:
        observed_scenes = captured_scenes
        where = ", ".join(f"{s.state}={s.scene}" for s in record.scenes_at_call[:4])
        if not record.scenes_at_call:
            return ClaimCheck(claim=claim, status="unknown", observed=ToolMessage(
                f"{len(record.scenes_at_call)} scene(s) captured; none could be named "
                "(entry snapshot's mode bytes were unreadable or outside the jump tables)"))
        if claim.scene in observed_scenes:
            return ClaimCheck(claim=claim, status="pass", observed=ToolMessage(
                f"over {len(record.scenes_at_call)} captured call(s) it ran in {claim.scene} "
                f"({where})"))
        return ClaimCheck(claim=claim, status="fail", observed=ToolMessage(
            f"over {len(record.scenes_at_call)} captured call(s) it ran in "
            f"{sorted(observed_scenes)} ({where}); never {claim.scene}"))
    reached = reached_routes(record)
    if not reached:
        return ClaimCheck(claim=claim, status="fail", observed=ToolMessage(
            f"no recorded route executed it (0 of {len(record.routes)} routes)"))
    scenes = {scene_of(str(route.route)) for route in reached}
    where = ", ".join(str(route.route) for route in reached) or "(none)"
    if claim.scene in scenes:
        return ClaimCheck(claim=claim, status="pass", observed=ToolMessage(
            f"executed on {where} ({claim.scene})"))
    if scenes == {None}:
        return ClaimCheck(claim=claim, status="unknown", observed=ToolMessage(
            f"executed on {where}, whose scene the route table cannot name"))
    return ClaimCheck(claim=claim, status="fail", observed=ToolMessage(
        f"executed on {where}: scenes {sorted(s for s in scenes if s)}, never {claim.scene}"))


def seen_scenes_at_call(record: FunctionFacts) -> set[str] | None:
    """The distinct scenes the record's per-call scenes observed, or None when no per-call
    scenes were recorded (the captures weren't replayed). `other` is removed -- a function only
    ever seen in `other` (an unnamed mode) is not contradicted by a scene claim about it."""
    if not record.scenes_at_call:
        return None
    return {call.scene for call in record.scenes_at_call if call.scene != "other"}


def _writes_ram(claim: WritesRam, record: FunctionFacts) -> ClaimCheck:
    """Did the function write into RAM that bn6f names with `claim.symbol_prefix`?

    Claims are written in the disassembly's own vocabulary (`oBattleObject_Timer`,
    `oAIData`), which names a struct FIELD or FAMILY, while a RAM symbol names a struct
    INSTANCE with the object index spelled out (`eT3BattleObject1_Timer` is the same field).
    So the two are matched through bn6f's struct tables, never as text, in three steps of
    decreasing strength:

    * `field_writes` whose `field_names` the base register points into (`attributed`), or whose
      `struct_guess` is a family the base register sits in (`in_family`). Offsets are shared
      across families (0x8 is `oBattleObject_CurAction` and eighty-five other structs' first
      field), so a name at an offset the base does not point into is not evidence about it.
    * `field_writes`' base region and the `memory_writes` regions the calls changed, read back
      through the family table: `eT3BattleObject1_Timer` answers to `oBattleObject_Timer` and
      to the family `oBattleObject`, and `eBattleState0_HP` to `oBattleState_HP`.
    * nothing at all -- `unknown`, because no capture was replayed or no base pointed at RAM."""
    prefix = claim.symbol_prefix.casefold()
    hits: list[str] = []
    for write in record.field_writes:
        if write.attributed:
            hits += [name for name in write.field_names
                     if name.casefold().startswith(prefix) and name not in hits]
        if write.in_family and write.struct_guess.casefold().startswith(prefix):
            if write.struct_guess not in hits:
                hits.append(write.struct_guess)
    if record.facts is not None and record.returned:
        for region in record.facts.memory_writes:
            symbol = str(region.symbol)
            if symbol.casefold().startswith(prefix) and symbol not in hits:
                hits.append(symbol)
            # a region symbol that IS a field (`eT3BattleObject1_Timer`) resolves to the bn6f
            # field names at its own offset, which is what the claim is spelled in
            written = sf.resolve(0, symbol)
            for name in written.names:
                if name.casefold().startswith(prefix) and name not in hits:
                    hits.append(name)
            for family in written.candidates:
                if family.casefold().startswith(prefix) and family not in hits:
                    hits.append(family)
    if hits:
        return ClaimCheck(claim=claim, status="pass", observed=ToolMessage(
            f"over {record.returned} replayed call(s) it wrote inside {', '.join(hits[:4])}"))
    if record.facts is None or not record.returned:
        return ClaimCheck(claim=claim, status="unknown", observed=_no_facts(record))
    biggest = (", ".join(f"{write.symbol}({write.bytes}B)"
                         for write in record.facts.memory_writes[:4]) or "nothing")
    return ClaimCheck(claim=claim, status="fail", observed=ToolMessage(
        f"over {record.returned} replayed call(s) it wrote {biggest}; none of it is "
        f"{claim.symbol_prefix}"))


# the bn6f buffers behind the hardware regions: a write the game makes "to OAM" lands in a
# shadow buffer in IWRAM/EWRAM first, so the bare region names would miss nearly every drawing
# function (the facts attribute each changed word to the bn6f symbol that contains it).
RAM_GROUP_PATTERNS: dict[str, tuple[re.Pattern[str], ...]] = {
    "oam": (re.compile(r"oam", re.I),),
    "palette": (re.compile(r"pal", re.I),),
    "vram": (re.compile(r"vram", re.I),),
}

DISPLAY_REGISTERS: re.Pattern[str] = re.compile(
    r"^(DISPCNT|DISPSTAT|VCOUNT|BG[0-3](CNT|HOFS|VOFS|X|Y|PA|PB|PC|PD)|WIN|WININ|WINOUT|MOSAIC|BLD)",
    re.I)

# what the facts' I/O diff throws away by design (runtime_evidence.py's _IO_VOLATILE): the
# write-only sound and DMA registers and the key input. A claim about them is never decidable.
NEVER_CHECKABLE_IO: tuple[IOGroup, ...] = ("sound", "dma", "input")


def _reads_or_writes_io(claim: ReadsOrWritesIo, record: FunctionFacts) -> ClaimCheck:
    if claim.group in NEVER_CHECKABLE_IO:
        return ClaimCheck(claim=claim, status="unknown", observed=ToolMessage(
            f"the facts' I/O diff drops every {claim.group} register (write-only, or open bus "
            f"on read): nothing here can decide it"))
    if record.facts is None or not record.returned:
        return ClaimCheck(claim=claim, status="unknown", observed=_no_facts(record))
    if claim.group in RAM_GROUP_PATTERNS:
        pattern = RAM_GROUP_PATTERNS[claim.group][0]
        symbols = [str(write.symbol) for write in record.facts.memory_writes]
        hits = [symbol for symbol in symbols if pattern.search(symbol)]
        biggest = (", ".join(f"{write.symbol}({write.bytes}B)"
                             for write in record.facts.memory_writes[:4]) or "no RAM at all")
        if hits:
            return ClaimCheck(claim=claim, status="pass", observed=ToolMessage(
                f"over {record.returned} replayed call(s) it wrote {', '.join(hits[:4])}"))
        return ClaimCheck(claim=claim, status="fail", observed=ToolMessage(
            f"over {record.returned} replayed call(s) it wrote {biggest}: nothing that lands "
            f"in {claim.group}"))
    names = [str(write.name) for write in record.facts.io_writes]
    hits = [name for name in names if DISPLAY_REGISTERS.match(name)]
    if hits:
        return ClaimCheck(claim=claim, status="pass", observed=ToolMessage(
            f"over {record.returned} replayed call(s) it wrote the display register(s) "
            f"{', '.join(sorted(set(hits))[:4])}"))
    return ClaimCheck(claim=claim, status="fail", observed=ToolMessage(
        f"over {record.returned} replayed call(s) the I/O registers it wrote were "
        f"{', '.join(sorted(set(names))[:4]) or 'none'}: no display register"))


def _returns_value(claim: ReturnsValue, record: FunctionFacts) -> ClaimCheck:
    if not record.returned or not record.returns:
        return ClaimCheck(claim=claim, status="unknown", observed=_no_facts(record))
    out = record.returns[0]
    if not out.samples or out.lowest is None or out.highest is None:
        return ClaimCheck(claim=claim, status="unknown", observed=_no_facts(record))
    entry = next((stat for stat in record.arguments if stat.reg == "r0"), None)
    where = (f"r0 came back {out.samples} time(s), {out.distinct} distinct value(s), "
             f"0x{out.lowest:08X}..0x{out.highest:08X}")
    if claim.value == "none":
        unchanged = (entry is not None and entry.distinct == out.distinct
                     and entry.lowest == out.lowest and entry.highest == out.highest
                     and entry.values == out.values)
        if unchanged:
            return ClaimCheck(claim=claim, status="pass", observed=ToolMessage(
                f"r0 is always the value the caller passed in ({where})"))
        return ClaimCheck(claim=claim, status="fail", observed=ToolMessage(
            f"r0 is rewritten: {where}, the caller's own value differs"))
    low, high = out.lowest, out.highest
    if claim.value == "bool":
        held = 0 <= low and high <= 1
    elif claim.value == "small_int":
        held = 0 <= low and high <= 0xFF
    else:                                                # pointer
        held = _inside_ram_or_rom(low, high)
    if held:
        return ClaimCheck(claim=claim, status="pass", observed=ToolMessage(where))
    return ClaimCheck(claim=claim, status="fail",
                      observed=ToolMessage(f"{where}, which is not a {claim.value}"))


def _inside_ram_or_rom(low: int, high: int) -> bool:
    """Whether every address between LOW and HIGH is inside one of the GBA's RAM or ROM blocks
    (EWRAM/IWRAM/palette/VRAM/OAM/SRAM, or ROM). One value outside them fails a pointer claim."""
    for base, size in ((0x02000000, 0x60000), (0x05000000, 0x20400), (0x08000000, 0x02000000),
                       (0x0E000000, 0x8000)):
        if base <= low and high < base + size:
            return True
    return False


def _called_every_frame(claim: CalledEveryFrame, record: FunctionFacts) -> ClaimCheck:
    """Is the function called once a frame (or more) on the best recorded route? Prefers real
    entries (`entries_per_1000_frames` from the per-route entry counts) -- the CPU entering the
    function's entry address on the route -- over `calls_per_1000_frames`, which is only the
    recorded indirect call sites (`bl` and direct-call entries are invisible to it, and an
    instrumented site can be wrong about which function it lands on)."""
    reached = reached_routes(record)
    if not reached:
        return ClaimCheck(claim=claim, status="fail", observed=ToolMessage(
            "no recorded route executed it, so it is not called every frame"))
    entry_rates = [(str(route.route), float(route.entries_per_1000_frames)) for route in reached
                   if route.entries_per_1000_frames is not None]
    if entry_rates:
        best_route, best = max(entry_rates, key=lambda row: row[1])
        every = best >= EVERY_FRAME_PER_1000
        shown = (f"{best:.1f} entries / 1000 frames on {best_route} "
                 f"({len(entry_rates)} counted route(s))")
        if every == claim.yes:
            return ClaimCheck(claim=claim, status="pass", observed=ToolMessage(shown))
        return ClaimCheck(claim=claim, status="fail", observed=ToolMessage(shown))
    rates = [(str(route.route), float(route.calls_per_1000_frames)) for route in reached
             if route.calls_per_1000_frames is not None]
    if not rates:
        return ClaimCheck(claim=claim, status="unknown", observed=ToolMessage(
            "no recorded route carries call counts for it (the indirect call sites are not "
            "instrumented and the entry counts were not collected for any route)"))
    best_route, best = max(rates, key=lambda row: row[1])
    every = best >= EVERY_FRAME_PER_1000
    shown = (f"most on {best_route}: {best:.1f} calls / 1000 frames over {len(rates)} route(s)"
             " (from .edges, not entries)")
    if every == claim.yes:
        return ClaimCheck(claim=claim, status="pass", observed=ToolMessage(shown))
    return ClaimCheck(claim=claim, status="fail", observed=ToolMessage(shown))


def _has_argument(claim: HasArgument, record: FunctionFacts) -> ClaimCheck:
    if not record.returned or not record.arguments:
        return ClaimCheck(claim=claim, status="unknown", observed=_no_facts(record))
    register = ft.ARGUMENT_REGISTERS[claim.n]
    stat = next((row for row in record.arguments if row.reg == register), None)
    if stat is None or stat.samples < 2 or stat.lowest is None:
        samples = stat.samples if stat is not None else 0
        return ClaimCheck(claim=claim, status="unknown", observed=ToolMessage(
            f"{register} was carried by {samples} replayed call(s); one call cannot show an "
            f"argument"))
    if stat.distinct > 1:
        return ClaimCheck(claim=claim, status="pass", observed=ToolMessage(
            f"{register} held {stat.distinct} distinct values over {stat.samples} call(s)"))
    return ClaimCheck(claim=claim, status="fail", observed=ToolMessage(
        f"{register} held one value (0x{stat.lowest:08X}) over {stat.samples} call(s)"))


def _no_facts(record: FunctionFacts) -> ToolMessage:
    why = record.facts_error or ("no probe returned" if record.probes else "no captured calls")
    return ToolMessage(f"{NO_FACTS}: {why}")


def check_claims(claims: Sequence[Claim], record: FunctionFacts | None) -> tuple[ClaimCheck, ...]:
    return tuple(check_claim(claim, record) for claim in claims)


def passive_verdict(checks: Sequence[ClaimCheck]) -> Passive:
    """`contradicted` when any claim failed, `consistent` when every checkable one passed,
    `unchecked` when nothing was settled."""
    if any(check.status == "fail" for check in checks):
        return "contradicted"
    if not checks or all(check.status == "unknown" for check in checks):
        return "unchecked"
    return "consistent"


def claim_text(claim: Claim) -> str:
    """The claim as the one `kind(value)` string the prompt spells it with."""
    if isinstance(claim, ReachedInScene):
        return claim.scene
    if isinstance(claim, WritesRam):
        return claim.symbol_prefix
    if isinstance(claim, ReadsOrWritesIo):
        return claim.group
    if isinstance(claim, ReturnsValue):
        return claim.value
    if isinstance(claim, CalledEveryFrame):
        return "yes" if claim.yes else "no"
    return f"r{claim.n}" if claim.n < 4 else "sp"


# ── routes: which recording to intervene on ────────────────────────────

def candidate_routes(record: FunctionFacts | None, limit: int = MAX_ROUTE_TRIES
                     ) -> tuple[tuple[RouteName, FrameCount], ...]:
    """The routes the facts pick, shortest first: the recorded routes that executed this
    function and that `wf intervene` can replay (a route with a trace and a save). The lengths
    come from wf/routes.py read at run time, so a route that grew is not stale."""
    from wf.routes import ALL_ROUTES, LONG_ROUTES       # noqa: PLC0415
    if record is None:
        return ()
    replayable = {str(route.name): route for route in (*ALL_ROUTES, *LONG_ROUTES)
                  if route.trace.exists()}
    reached = [str(route.route) for route in reached_routes(record)]
    ordered = sorted(reached, key=lambda item: (int(replayable[item].length)
                                                if item in replayable else 1 << 30, item))
    return tuple((RouteName(name), FrameCount(int(replayable[name].length)))
                 for name in ordered if name in replayable)[:limit]


# ── the blind model call ───────────────────────────────────────────────

CLAIMS_SYSTEM = ("You write testable claims about what a function in a disassembled GBA game "
                 "does at runtime, before anything about it has been observed. Reply with "
                 "exactly one JSON object and nothing else.")

CLAIMS_TEMPLATE = """A function of "Mega Man Battle Network 6" (GBA) is named `{name}`.

Its symbols/db entry, written by an earlier assembly-reading step (it may be wrong):
{description}

You have NOT seen this function run. Do not guess numbers. Write what the name claims, in a form
a recording of the running game could settle, plus one causal prediction: what the machine must
show if the name is right and the function is taken away.

`claims`: up to five objects, each with a `kind`:
  {{"kind": "reached_in_scene", "scene": "battle"}}            scene: battle | overworld | menu | title | cutscene
  {{"kind": "writes_ram", "symbol_prefix": "oBattleState"}}    a bn6f RAM symbol it writes, up to `_`
  {{"kind": "reads_or_writes_io", "group": "palette"}}          group: oam | palette | vram | display | sound | dma | input
  {{"kind": "returns_value", "value": "pointer"}}               value: bool | small_int | pointer | none
  {{"kind": "called_every_frame", "yes": false}}
  {{"kind": "has_argument", "n": 0}}                           n: 0..3 = r0..r3, 4 = the stack pointer

What the observations can and cannot settle (write claims you can live with):
  * only memory and registers whose value CHANGED are recorded: a write of the value already
    there, and every read, are invisible.
  * I/O registers that read back as junk (sound, DMA, key input, BG scroll, windows, blend) are
    never recorded, so sound/dma/input claims are never falsifiable: use them only when nothing
    else in the name predicts an effect.
  * OAM, palette and VRAM writes are attributed to the bn6f buffer the game keeps them in (a
    symbol whose name holds `Oam`, `Pal` or `Vram`), not to the hardware address.
  * `has_argument(n)` means the caller passes more than one value in that register.
  * `called_every_frame` counts the call sites the recordings instrumented (the indirect ones).

`prediction`: exactly one object.
  {{"intervention": "skip", "predicates": ["differs(anchor=oam, within=60)"], "window": 120}}
    "skip" makes the function return immediately; "return:K" makes it return the constant K
    (0..255). 1 to 3 predicates, each one of
      differs(anchor=oam, within=60)             that anchor differs within 60 game frames
      unchanged(anchor=palette, for=120)         it does not differ over the first 120 frames
      oam_entries_missing(min=4, first_after=10) at least 4 OAM entries differ, first at/after frame 10
      screen_differs(region=0,0,240,160)         the screen differs
      sound_write_missing(reg=SOUNDCNT_X)        that sound register is not written
    anchors: oam, palette, vram_bg, vram_obj, display_io, dma_control, sound_io, screen
    window: how many game frames to watch (60..240).
  or {{"intervention": "none", "reason": "..."}} when the name implies no visible effect at all.

Reply with exactly one JSON object and nothing else:
{{"claims": [ ... ], "prediction": {{...}}, "notes": "one line, optional: anything unclear (e.g. which recorded route a scene claim is about)"}}"""

_usage_lock = threading.Lock()
_consumed: dict[Path, int] = {}
_spent: dict[tuple[Path, str], int] = {}


def logged_tokens(tag: str) -> int:
    """Input+output tokens tools/llm.py logged for the call TAG (the log is read forward, so a
    run of many calls does not re-read the whole file)."""
    with _usage_lock:
        try:
            size = USAGE_LOG.stat().st_size
        except FileNotFoundError:
            return 0
        start = _consumed.get(USAGE_LOG, 0)
        if size > start:
            with USAGE_LOG.open("rb") as log:
                log.seek(start)
                text = log.read().decode(errors="replace")
            if not text.endswith("\n"):
                text = f"{text.rpartition(chr(10))[0]}\n" if chr(10) in text else ""
            for line in text.splitlines():
                try:
                    loaded = cast(object, json.loads(line))
                except json.JSONDecodeError:
                    continue
                if not isinstance(loaded, dict):
                    continue
                row = cast(dict[str, object], loaded)
                if row.get("tag") == tag:
                    into, out = row.get("in"), row.get("out")
                    if isinstance(into, int) and isinstance(out, int):
                        _spent[(USAGE_LOG, tag)] = into + out
            _consumed[USAGE_LOG] = start + len(text.encode())
        return _spent.get((USAGE_LOG, tag), 0)


def complete(spec: ModelSpec, prompt: str, system: str, tag: str) -> tuple[str, ToolMessage | None]:
    """tools/llm.py complete() -- the same call `wf investigate` makes, with this workflow's own
    timeout and retry count. Hyper is not used here."""
    try:
        reply = llm.complete(spec, prompt, system=system, max_tokens=MAX_TOKENS,   # pyright: ignore[reportUnknownMemberType, reportAny]
                             timeout=MODEL_TIMEOUT, retries=MODEL_RETRIES, tag=tag,
                             usage_log=str(USAGE_LOG))
    except Exception as error:                                     # noqa: BLE001
        return "", ToolMessage(str(error)[:300])
    return str(reply), None


def parse_reply(reply: str) -> tuple[Reply | None, ToolMessage | None]:
    """The model's JSON object as a `Reply`, or what kept us from reading one (one retry on
    that, then the function is recorded with the problem)."""
    match = re.search(r"```(?:json)?\s*(\{.*?\})\s*```", reply, re.S)
    candidates = [match[1]] if match else []
    first, last = reply.find("{"), reply.rfind("}")
    if 0 <= first < last:
        candidates.append(reply[first:last + 1])
    for candidate in candidates:
        try:
            loaded = json.loads(candidate)
        except json.JSONDecodeError:
            continue
        if not isinstance(loaded, dict):
            continue
        try:
            return Reply.model_validate(loaded), None
        except ValidationError as error:
            problems = "; ".join(f"{'.'.join(str(part) for part in item['loc'])}: {item['msg']}"
                                for item in error.errors()[:4])
            return None, ToolMessage(problems[:300] or "the object did not match the schema")
    if reply:
        # the response did not contain a JSON object: at least record a slice of it so the
        # run's notes say what the model went for instead.
        tail = reply[-300:].replace("\n", " ") if len(reply) > 300 else reply.replace("\n", " ")
        return None, ToolMessage(f"the reply held no JSON object (last 300 chars: ...{tail})")
    return None, ToolMessage("the reply held no JSON object")


def ask_claims(model: ModelSpec, name: FunctionName, address: GuestAddress, description: str,
               run_dir: Path, run_id: str) -> Answer:
    """The blind answer for one function: the claims and the prediction, written before anything
    about the function was observed. An unusable reply is asked once more; if that fails too the
    answer is recorded with its problem and the name gets no level from a model."""
    cached = cached_answer(run_dir, name)
    if cached is not None:
        return cached
    started = time.monotonic()
    prompt = CLAIMS_TEMPLATE.format(name=name, description=description)
    tokens = retries = 0
    reply: Reply | None = None
    problem: ToolMessage | None = None
    for attempt in range(2):
        tag = f"{run_id} check-names {name} {attempt}"
        text, failure = complete(model, prompt, CLAIMS_SYSTEM, tag)
        tokens += logged_tokens(tag)
        problem = failure
        if problem is None:
            reply, problem = parse_reply(text)
        if problem is None and reply is not None:
            break
        retries = attempt + 1
        prompt = (f"{prompt}\n\nYour previous reply could not be used: {problem}\n"
                  f"Reply again with only the JSON object, nothing else.")
    answer = Answer(name=name, address=address, reply=reply, tokens=tokens,
                    seconds=round(time.monotonic() - started, 1), retries=retries, problem=problem)
    keep_answer(run_dir, answer)
    return answer


def cached_answer(run_dir: Path, name: FunctionName) -> Answer | None:
    """What an earlier step of this run already got for NAME (a run is resumable)."""
    path = run_dir / "claims" / f"{name}.json"
    if not path.exists():
        return None
    try:
        return Answer.model_validate_json(path.read_text())
    except ValueError:
        return None


def keep_answer(run_dir: Path, answer: Answer) -> None:
    folder = run_dir / "claims"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / f"{answer.name}.json").write_text(answer.model_dump_json(indent=2) + "\n")


# ── the active check ───────────────────────────────────────────────────
#
# Everything this workflow asks of `wf intervene` goes through ONE function,
# `intervene_once` below: resolve the function, find the frame it first runs on, patch a copy
# of the ROM, replay baseline and patched from the same cached state, and hand back the anchor
# diffs. `kind="probe"` only finds the first frame (no ROM is written). A fix landing in
# `wf intervene` drops in behind this function alone.

Active = Literal["held", "failed", "unreached", "hung", "nondeterministic", "indeterminate",
                 "none", "error"]
#: the outcomes that are an answer about the PREDICTION: anything else is the harness's, and a
#: prediction is only counted as held or failed when the control was sound.
TESTED: tuple[Active, ...] = ("held", "failed", "hung", "indeterminate")


class Observation(Result):
    """What one `wf intervene` call gave back: its RunRecord (whatever its type), the ROM copies
    it wrote (regenerable: this run deletes them again), and what went wrong, if anything."""
    record: object | None = None
    roms: tuple[Path, ...] = ()
    first_frame: int | None = None
    route: RouteName | None = None
    problem: ToolMessage | None = None
    seconds: float = 0.0

    @property
    def ran(self) -> bool:
        # `probe` only sets `first_frame` (it patches nothing, so there is no RunRecord),
        # the patched runs set both. Either way, the function ran on this route inside the
        # hunt window and the rest of the pipeline has something to judge.
        return self.first_frame is not None


def intervene_once(*, name: FunctionName, address_hex: str, route: RouteName, kind: str,
                   k: int | None = None, window: int = DEFAULT_FRAMES, out_dir: Path,
                   frames_window: int = DEFAULT_FRAMES_WINDOW) -> Observation:
    """THE call into `wf intervene`. `kind` is `skip` / `return` / `null` / `probe` (probe only
    finds the first game frame the route runs the function, patching nothing)."""
    from wf.routes import route_named                                   # noqa: PLC0415
    started = time.monotonic()
    import gamepaths                                                    # noqa: PLC0415
    mgba_run = Path(gamepaths.MGBA_RUN)
    bios = Path(gamepaths.GBA_BIOS)
    game_route = route_named(route)
    try:
        info = it.find_function(address_hex, on=game_route)
    except ValueError:
        try:
            info = it.find_function(str(name), on=game_route)
        except ValueError as error:
            return Observation(route=route, problem=ToolMessage(str(error)[:300]))
    if kind == "probe":
        try:
            first = it.first_game_frame(info, game_route, mgba_run=mgba_run, bios=bios,
                                        frames_window=frames_window)
        except Exception as error:                                     # noqa: BLE001
            return Observation(route=route, first_frame=None,
                               problem=ToolMessage(f"the frame search failed: {error}"[:300]))
        return Observation(first_frame=first, route=route,
                           seconds=round(time.monotonic() - started, 1))
    before = _patched_rom_paths(info, kind, k)
    try:
        record = it.run_intervene_one(function=info, intervention=kind, k=k, route=game_route,
                                      frames=window, mgba_run=mgba_run, bios=bios,
                                      out_dir=out_dir, anchors=it.load_anchors(),
                                      frames_window=frames_window)
    except Exception as error:                                         # noqa: BLE001
        return Observation(roms=_roms_made(before), route=route,
                           problem=ToolMessage(f"{type(error).__name__}: {error}"[:300]),
                           seconds=round(time.monotonic() - started, 1))
    made = _roms_made(before)
    first_frame = cast("int | None", getattr(record, "first_frame_executed", None))
    return Observation(record=record, roms=made, first_frame=first_frame, route=route,
                       problem=None if first_frame is not None else ToolMessage(
                           f"{name} does not run on {route} inside the first {frames_window} "
                           f"game frames"),
                       seconds=round(time.monotonic() - started, 1))


def _patched_rom_paths(info: object, kind: str, k: int | None) -> tuple[Path, ...]:
    """Where `wf intervene` writes the patched copies for this function and intervention, so a
    run can delete them again (they are regenerable and 90 functions x 3 is gigabytes)."""
    address = cast("int", getattr(info, "address", 0))
    name = cast("str", getattr(info, "name", ""))
    tag = kind if k is None else f"return-{k}"
    return (it.PATCHED_DIR / f"bn6f_{name}_{address:08x}_{tag}.gba",)


def _roms_made(candidates: Sequence[Path]) -> tuple[Path, ...]:
    return tuple(path for path in candidates if path.exists())


def drop_roms(roms: Sequence[Path]) -> None:
    """Take the patched copies out again once their run is judged."""
    for rom in roms:
        rom.unlink(missing_ok=True)


class PredicateCall(Result):
    """One predicate of the prediction, and what it said about the observed diff."""
    spec: str
    verdict: bool | None            # None: indeterminate, or a predicate this module cannot parse
    problem: ToolMessage | None = None


class ActiveResult(Result):
    """What running one prediction came to.

    * `held` -- every predicate held on the observed diff.
    * `failed` -- one did not (the control was sound, so this says something about the name).
    * `unreached` -- the function never ran on the route inside the hunt window.
    * `hung` -- the patched run wedged the game, and the control was sound.
    * `nondeterministic` -- the `null` control differed or the baseline hung: this start point
      proves nothing.
    * `indeterminate` -- no predicate could be decided from the diffs.
    * `none` -- the model wrote no prediction (the name implies no visible effect).
    * `error` -- the run itself failed.
    """
    outcome: Active
    intervention: ToolMessage = ToolMessage("")
    predicates: tuple[PredicateCall, ...] = ()
    route: RouteName | None = None
    window: int = DEFAULT_FRAMES
    first_frame: int | None = None
    differing_anchors: int = 0
    first_diff: ToolMessage | None = None
    sound: bool | None = None        # was the control (null) sound?
    seconds: float = 0.0
    problem: ToolMessage | None = None

    @property
    def held(self) -> bool:
        return self.outcome == "held"

    @property
    def tested(self) -> bool:
        return self.outcome in TESTED


class Control(Result):
    """The `null` control for one (function, route, window): the function's own bytes written
    back, so the replay must show nothing at all. Not sound -> no verdict from this start point."""
    route: RouteName
    window: int
    deterministic: bool | None
    hung: bool
    seconds: float = 0.0
    problem: ToolMessage | None = None

    @property
    def sound(self) -> bool:
        return bool(self.deterministic) and not self.hung


def judge_prediction(action: Action, record: object | None, control: Control | None,
                     *, route: RouteName | None = None, seconds: float = 0.0) -> ActiveResult:
    """What the observed diffs say about the prediction. Pure arithmetic over the record:
    the type of `record` is `wf intervene`'s, and every branch is read off it."""
    if isinstance(action, NoEffect):
        return ActiveResult(outcome="none", intervention=ToolMessage(f"none: {action.reason}"),
                            window=DEFAULT_FRAMES, seconds=seconds)
    if record is None:
        return ActiveResult(outcome="error", intervention=ToolMessage(action.label()),
                            window=action.window, route=route, seconds=seconds,
                            problem=ToolMessage("no intervention run"))
    first_frame = cast("int | None", getattr(record, "first_frame_executed", None))
    hung = bool(getattr(record, "hung", False))
    where = route or RouteName(str(getattr(record, "route", "")))
    diffs = tuple(getattr(record, "anchor_diffs", ()))
    differing = sum(int(getattr(diff, "differing_frames", 0)) for diff in diffs)
    first = next((f"{getattr(diff, 'name', '?')}@{getattr(diff, 'first_frame', '?')}"
                  for diff in diffs if int(getattr(diff, "differing_frames", 0))), None)
    common = {"intervention": ToolMessage(action.label()), "window": action.window,
              "route": where, "first_frame": first_frame, "differing_anchors": differing,
              "first_diff": first, "seconds": seconds}
    if first_frame is None:
        return ActiveResult(outcome="unreached", problem=ToolMessage(
            "the function does not run on this route inside the hunt window, so the "
            "prediction was never tested"), **common)
    if control is not None and not control.sound:
        return ActiveResult(outcome="nondeterministic", sound=False, problem=ToolMessage(
            f"the null control on {control.route} was not sound (deterministic="
            f"{control.deterministic}, hung={control.hung})"), **common)
    calls = tuple(_one_predicate(spec, record) for spec in action.predicates)
    verds = tuple(call.verdict for call in calls)
    if hung:
        outcome, problem = "hung", ToolMessage("the patched run wedged the game "
                                                "(no further game frame)")
    elif any(verdict is False for verdict in verds):
        outcome, problem = "failed", ToolMessage(
            f"{sum(1 for verdict in verds if verdict is False)} of {len(calls)} predicate(s) "
            "did not hold")
    elif verds and all(verdict is True for verdict in verds):
        outcome, problem = "held", None
    else:
        outcome, problem = "indeterminate", ToolMessage(
            "no predicate could be decided from the observed diffs")
    return ActiveResult(outcome=outcome, intervention=ToolMessage(action.label()),
                        window=action.window, route=where, first_frame=first_frame,
                        first_diff=first, seconds=seconds, problem=problem,
                        predicates=calls,
                        differing_anchors=differing,
                        sound=control.sound if control else None)


def _one_predicate(spec: str, record: object) -> PredicateCall:
    """One predicate of the prediction, parsed in `wf intervene`'s predicate language and
    evaluated against its anchor diffs. A predicate this module cannot parse is an `error` on
    that predicate alone, and never counted as held."""
    try:
        predicate = it.make_predicate(spec)
    except ValueError as error:
        return PredicateCall(spec=spec, verdict=None, problem=ToolMessage(str(error)[:200]))
    verdict = it.evaluate_predicate(predicate, cast("it.RunRecord", record))
    return PredicateCall(spec=spec, verdict=None if verdict is None else bool(verdict))


def prediction_from(reply: Reply | None) -> Action | None:
    """The prediction a blind answer carries, or None when it carries none (or none at all)."""
    return reply.prediction if reply is not None else None


# ── the batch: which functions, and who is paired with whom ────────────

class Pairing(NamedTuple):
    """Who is checked against whose facts: name -> the other name in the batch it is paired
    with (its swapped control)."""
    swaps: dict[str, str]


def pair_up(names: Sequence[str], seed: int) -> Pairing:
    """Each name paired with another name in the batch, drawn at random: that partner's claims
    and prediction are the control, evaluated against THIS function's facts and intervention.
    A name is never paired with itself; with two names the pairing is mutual."""
    ordered = list(names)
    if len(ordered) < 2:
        return Pairing(swaps={})
    rng = random.Random(seed)
    pool = list(ordered)
    rng.shuffle(pool)
    swaps: dict[str, str] = {}
    for index, name in enumerate(ordered):
        choices = [other for other in pool if other != name]
        swaps[name] = choices[index % len(choices)]
    return Pairing(swaps=swaps)


# ── calibration ────────────────────────────────────────────────────────

class Calibration(Result):
    """What this batch's swapped controls say about the checker itself. A level is granted only
    where the batch discriminates: see `claims_ok` / `predictions_ok`."""
    names: int
    true_claims_checkable: int
    true_claims_passed: int
    swapped_claims_checkable: int
    swapped_claims_passed: int
    true_predictions: int               # written and actually tested
    true_held: int
    swapped_predictions: int
    swapped_held: int
    true_claim_rate: float | None
    swapped_claim_rate: float | None
    claim_margin: float | None
    swapped_prediction_rate: float | None
    claims_ok: bool
    predictions_ok: bool
    by_claim_kind: tuple[ToolMessage, ...] = ()
    outcomes: tuple[ToolMessage, ...] = ()
    reasons: tuple[ToolMessage, ...] = ()

    @property
    def discriminating(self) -> bool:
        return self.claims_ok and self.predictions_ok


def _rate(passed: int, checkable: int) -> float | None:
    return round(passed / checkable, 4) if checkable else None


def _tally(checks: Sequence[ClaimCheck]) -> tuple[int, int]:
    """(checkable, passed) over one group of claim checks -- `unknown` is neither."""
    checkable = [check for check in checks if check.status in ("pass", "fail")]
    return len(checkable), sum(1 for check in checkable if check.status == "pass")


def calibration(true_checks: Sequence[tuple[ClaimCheck, ...]],
                swapped_checks: Sequence[tuple[ClaimCheck, ...]],
                true_runs: Sequence[ActiveResult],
                swapped_runs: Sequence[ActiveResult]) -> Calibration:
    """The gate's arithmetic: what the true claims and predictions did, what the swapped ones
    did against the same facts and the same interventions, and whether the gap is big enough to
    grant anything."""
    flat_true = [check for group in true_checks for check in group]
    flat_swapped = [check for group in swapped_checks for check in group]
    true_checkable, true_passed = _tally(flat_true)
    swapped_checkable, swapped_passed = _tally(flat_swapped)
    tested_true = [run for run in true_runs if run.tested]
    tested_swapped = [run for run in swapped_runs if run.tested]
    true_held = sum(1 for run in tested_true if run.held)
    swapped_held = sum(1 for run in tested_swapped if run.held)
    true_rate = _rate(true_passed, true_checkable)
    swapped_rate = _rate(swapped_passed, swapped_checkable)
    margin = (round(true_rate - swapped_rate, 4)
              if true_rate is not None and swapped_rate is not None else None)
    swapped_prediction_rate = _rate(swapped_held, len(tested_swapped))
    claims_ok = (margin is not None and true_checkable > 0 and swapped_checkable > 0
                 and margin >= SWAPPED_MIN_CLAIM_MARGIN)
    predictions_ok = (swapped_prediction_rate is not None
                      and swapped_prediction_rate < SWAPPED_MAX_PREDICTION_RATE)
    reasons: list[ToolMessage] = []
    if not claims_ok:
        reasons.append(ToolMessage(
            f"claim gate: true {true_rate} vs swapped {swapped_rate} (margin {margin}; the bar "
            f"is true - swapped >= {SWAPPED_MIN_CLAIM_MARGIN}) -- the claims do not separate "
            f"this function's facts from another function's"))
    if not predictions_ok:
        reasons.append(ToolMessage(
            f"prediction gate: a swapped prediction held {swapped_prediction_rate} of the "
            f"time (the bar is < {SWAPPED_MAX_PREDICTION_RATE}), so a held one carries nothing"))
    return Calibration(
        names=len(true_checks),
        true_claims_checkable=true_checkable, true_claims_passed=true_passed,
        swapped_claims_checkable=swapped_checkable, swapped_claims_passed=swapped_passed,
        true_predictions=len(tested_true), true_held=true_held,
        swapped_predictions=len(tested_swapped), swapped_held=swapped_held,
        true_claim_rate=true_rate, swapped_claim_rate=swapped_rate, claim_margin=margin,
        swapped_prediction_rate=swapped_prediction_rate, claims_ok=claims_ok,
        predictions_ok=predictions_ok,
        by_claim_kind=_by_claim_kind(flat_true, flat_swapped),
        outcomes=_outcomes(tested_true, tested_swapped), reasons=tuple(reasons))


def _by_claim_kind(true_checks: Sequence[ClaimCheck], swapped_checks: Sequence[ClaimCheck]
                   ) -> tuple[ToolMessage, ...]:
    """One line per claim kind: how often the true claims of that kind passed, and the swapped
    ones. A kind the models write often and that passes either way is a kind that carries
    nothing, and this is where that shows."""
    tally: dict[str, dict[str, tuple[int, int]]] = {}
    for checks, key in ((true_checks, "true"), (swapped_checks, "swapped")):
        for check in checks:
            if check.status == "unknown":
                continue
            row = tally.setdefault(check.claim.kind, {"true": (0, 0), "swapped": (0, 0)})
            checkable, passed = row[key]                  # (total, passed)
            row[key] = (checkable + 1, passed + (1 if check.status == "pass" else 0))
    lines = []
    for kind in sorted(tally):
        true_row = tally[kind]["true"]
        swapped_row = tally[kind]["swapped"]
        lines.append(ToolMessage(
            f"{kind}: true {true_row[1]}/{true_row[0]} passed, swapped {swapped_row[1]}"
            f"/{swapped_row[0]}"))
    return tuple(lines)


def _outcomes(true_runs: Sequence[ActiveResult], swapped_runs: Sequence[ActiveResult]
              ) -> tuple[ToolMessage, ...]:
    kinds = sorted({run.outcome for run in (*true_runs, *swapped_runs)})
    return tuple(ToolMessage(
        f"{outcome}: true {sum(1 for run in true_runs if run.outcome == outcome)}, swapped "
        f"{sum(1 for run in swapped_runs if run.outcome == outcome)}") for outcome in kinds)


# ── levels ─────────────────────────────────────────────────────────────

class Level(str, Enum):
    """What a name earned in this batch. Reported, never written into symbols/db.

    * `L1` -- no runtime support at all (nothing checkable, no prediction).
    * `L2 consistent` -- every checkable claim passed.
    * `L3 confirmed` -- the prediction held and its swapped control did not.
    * `contradicted` -- a claim failed, or the prediction failed with a sound control.
    * `unreached` -- no recorded route ever executed it.
    * `ungated` -- the batch does not discriminate, so nothing is granted.
    """
    L1 = "L1"
    L2 = "L2 consistent"
    L3 = "L3 confirmed"
    CONTRADICTED = "contradicted"
    UNREACHED = "unreached"
    UNGATED = "ungated"


def level_for(*, reached: bool, passive: Passive, active: ActiveResult, control_held: bool,
              claims_ok: bool, predictions_ok: bool) -> tuple[Level, ToolMessage]:
    """The one place a level is decided. `claims_ok` / `predictions_ok` are the batch's gate: a
    class of level is only granted when its own gate passed, and `ungated` says which."""
    if not reached:
        return Level.UNREACHED, ToolMessage("")
    if active.outcome == "failed":
        if predictions_ok:
            return Level.CONTRADICTED, ToolMessage("")
        return Level.UNGATED, ToolMessage("the prediction gate did not pass")
    if passive == "contradicted":
        if claims_ok:
            return Level.CONTRADICTED, ToolMessage("")
        return Level.UNGATED, ToolMessage("the claim gate did not pass")
    if active.held and not control_held and predictions_ok:
        return Level.L3, ToolMessage("")
    if passive == "consistent":
        if claims_ok:
            return Level.L2, ToolMessage("")
        return Level.UNGATED, ToolMessage("the claim gate did not pass")
    return Level.L1, ToolMessage("")


# ── the whole batch's result ───────────────────────────────────────────

class NameResult(Result):
    """One name's outcome in this batch: what was claimed, what the facts said, what the
    intervention did, what its swapped control did, and the level."""
    name: FunctionName
    address: GuestAddress
    address_hex: str
    level: Level
    gated_by: ToolMessage = ToolMessage("")
    passive: Passive = "unchecked"
    answer: Answer | None = None
    checks: tuple[ClaimCheck, ...] = ()
    swapped_from: FunctionName | None = None
    swapped_checks: tuple[ClaimCheck, ...] = ()
    active: ActiveResult | None = None
    swapped_active: ActiveResult | None = None
    control: Control | None = None
    routes: tuple[tuple[RouteName, FrameCount], ...] = ()
    notes: tuple[ToolMessage, ...] = ()

    @property
    def failing(self) -> tuple[ClaimCheck, ...]:
        return tuple(check for check in self.checks if check.status == "fail")

    @property
    def swapped_failing(self) -> tuple[ClaimCheck, ...]:
        return tuple(check for check in self.swapped_checks if check.status == "fail")

    def summary(self) -> ToolMessage:
        """One line for report.tsv: the level, the first failing claim and what was observed."""
        bits = [self.level.value]
        if self.gated_by:
            bits.append(str(self.gated_by))
        if self.failing:
            first = self.failing[0]
            bits.append(f"{first.claim.kind}({claim_text(first.claim)}) failed: {first.observed}")
        elif self.passive == "consistent":
            bits.append(f"{sum(1 for c in self.checks if c.status == 'pass')} claim(s) passed")
        if self.active is not None:
            bits.append(f"prediction {self.active.outcome}"
                        + (f" [{self.active.intervention}]" if self.active.intervention else ""))
        if self.notes:
            bits.append("; ".join(str(note) for note in self.notes))
        return ToolMessage("; ".join(bits))


def describe_check(check: ClaimCheck) -> str:
    """One claim check as a line: `writes_ram(oX)=pass: over 8 replayed call(s) it wrote ...`."""
    return f"{check.claim.kind}({claim_text(check.claim)})={check.status}: {check.observed}"


class BatchReport(Result):
    """What the run produced: the levels, the calibration, and what it could not decide."""
    run_id: str
    selected: int
    answered: int
    levels: dict[str, int]
    calibration: Calibration
    seconds: float = 0.0
    tokens: int = 0
    notes: tuple[ToolMessage, ...] = ()
    problems: tuple[ToolMessage, ...] = ()


# ── writing the run's output ───────────────────────────────────────────

REPORT_COLUMNS = ("name", "address", "level", "gated_by", "passive", "claims_pass", "claims_fail",
                  "claims_unknown", "prediction", "active", "route", "swapped_from",
                  "swapped_active", "control", "seconds", "summary")


def write_report(results: Sequence[NameResult], cal: Calibration, run_dir: Path, run_id: str,
                 seconds: float, tokens: int,
                 notes: Sequence[ToolMessage] = ()) -> BatchReport:
    """The run's output: report.tsv (one row per name), NAME.json per name, calibration.md."""
    run_dir.mkdir(parents=True, exist_ok=True)
    rows = ["\t".join(REPORT_COLUMNS)]
    levels: dict[str, int] = {}
    for result in results:
        levels[result.level.value] = levels.get(result.level.value, 0) + 1
        active, swapped = result.active, result.swapped_active
        rows.append("\t".join((
            result.name, result.address_hex, result.level.value, result.gated_by,
            result.passive,
            str(sum(1 for check in result.checks if check.status == "pass")),
            str(len(result.failing)), str(sum(1 for check in result.checks
                                             if check.status == "unknown")),
            str(active.intervention) if active else "-",
            active.outcome if active else "-",
            str(active.route) if active and active.route else "-",
            str(result.swapped_from) if result.swapped_from else "-",
            swapped.outcome if swapped else "-",
            (("sound" if result.control.sound else "unsound") if result.control else "-"),
            f"{result.answer.seconds:.1f}" if result.answer else "-",
            str(result.summary()).replace("\t", " "))))
        (run_dir / f"{result.name}.json").write_text(result.model_dump_json(indent=2) + "\n")
    (run_dir / "report.tsv").write_text("\n".join(rows) + "\n")
    (run_dir / "calibration.md").write_text(calibration_md(cal, run_id, notes))
    problems = tuple(note for result in results for note in _problems(result))
    return BatchReport(run_id=run_id, selected=len(results),
                       answered=sum(1 for result in results
                                    if result.answer is not None and result.answer.reply),
                       levels=levels, calibration=cal, seconds=round(seconds, 1), tokens=tokens,
                       notes=tuple(notes), problems=problems)


def _problems(result: NameResult) -> tuple[ToolMessage, ...]:
    out: list[ToolMessage] = []
    if result.answer is None or result.answer.reply is None:
        out.append(ToolMessage(f"{result.name}: no usable claims "
                               f"({result.answer.problem if result.answer else 'no model call'})"))
    for run in (result.active, result.swapped_active):
        if run is not None and run.problem:
            out.append(ToolMessage(f"{result.name}: {run.problem}"))
    out += [ToolMessage(f"{result.name}: {note}") for note in result.notes]
    return tuple(out)


def calibration_md(cal: Calibration, run_id: str, notes: Sequence[ToolMessage] = ()) -> str:
    """calibration.md: the batch's own numbers, in the order a reader wants them."""
    gate = ("The batch discriminates, so levels are granted."
            if cal.discriminating else
            "**The checker is not discriminating on this batch, so nothing is granted.**")
    lines = [f"# check-names calibration -- {run_id}", "", gate, "",
             "A claim or a prediction is worth something only if the same check, run against"
             " ANOTHER",
             "function's claims or prediction and this function's facts and intervention, would"
             " fail.",
             "Every name in the batch is paired with a random other name in the batch; the"
             " swapped",
             "column is that partner's answer checked here.", "", "## The gate", "",
             "| check | true | swapped | bar | verdict |", "|---|---|---|---|---|",
             f"| claims | {cal.true_claims_passed}/{cal.true_claims_checkable} passed "
             f"({_pct(cal.true_claim_rate)}) | {cal.swapped_claims_passed}/"
             f"{cal.swapped_claims_checkable} passed ({_pct(cal.swapped_claim_rate)}) | "
             f"true - swapped >= {SWAPPED_MIN_CLAIM_MARGIN} | "
             f"{'PASS' if cal.claims_ok else 'FAIL'} (margin {_pct(cal.claim_margin)}) |",
             f"| predictions | {cal.true_held}/{cal.true_predictions} held | "
             f"{cal.swapped_held}/{cal.swapped_predictions} held | "
             f"swapped < {SWAPPED_MAX_PREDICTION_RATE} | "
             f"{'PASS' if cal.predictions_ok else 'FAIL'} "
             f"(swapped {_pct(cal.swapped_prediction_rate)}) |", ""]
    if cal.reasons:
        lines += ["### why the gate says what it says", ""]
        lines += [f"* {reason}" for reason in cal.reasons]
        lines.append("")
    lines += ["## Claims by kind", "", "| kind | true passed | swapped passed |", "|---|---|---|"]
    for line in cal.by_claim_kind:
        kind, _, rest = str(line).partition(": true ")
        true_passed, _, rest = rest.partition(" passed, swapped ")
        swapped_passed = rest.partition("/")[0]
        lines.append(f"| `{kind}` | {true_passed} | {swapped_passed} |")
    lines += ["", "## Prediction outcomes", ""]
    lines += [f"* {line}" for line in cal.outcomes] or ["* (no prediction was tested)"]
    lines += ["", "## Notes", ""]
    lines += [f"* {note}" for note in notes] or ["* (none)"]
    return "\n".join(lines) + "\n"


def _gate_word(cal: Calibration) -> str:
    """The gate's own verdict, in one line for the printed summary."""
    return ("discriminating, levels granted" if cal.discriminating
            else "NOT discriminating, nothing granted")


def _pct(value: float | None) -> str:
    return f"{value * 100:.1f}%" if value is not None else "n/a"


def summarise(report: BatchReport) -> str:
    """The run's numbers as one block of text: what `wf check-names` prints when it finishes."""
    cal = report.calibration
    lines = [f"run: {report.run_id}",
             f"functions checked: {report.selected} "
             f"({report.answered} with usable blind claims)"]
    for level in (Level.CONTRADICTED, Level.L3, Level.L2, Level.UNREACHED, Level.UNGATED,
                  Level.L1):
        if level.value in report.levels:
            lines.append(f"  {level.value}: {report.levels[level.value]}")
    lines += [f"calibration -- claims: true {cal.true_claims_passed}/{cal.true_claims_checkable} "
              f"({_pct(cal.true_claim_rate)}) vs swapped {cal.swapped_claims_passed}/"
              f"{cal.swapped_claims_checkable} ({_pct(cal.swapped_claim_rate)}), margin "
              f"{_pct(cal.claim_margin)} ({'ok' if cal.claims_ok else 'FAILS the gate'})",
              f"calibration -- predictions: true {cal.true_held}/{cal.true_predictions} held vs "
              f"swapped {cal.swapped_held}/{cal.swapped_predictions} held "
              f"({_pct(cal.swapped_prediction_rate)}) "
              f"({'ok' if cal.predictions_ok else 'FAILS the gate'})",
              f"gate: {_gate_word(cal)}",
              f"wall time: {report.seconds:.0f}s, {report.tokens} tokens"]
    if report.problems:
        lines.append(f"problems ({len(report.problems)}):")
        lines += [f"  {problem}" for problem in report.problems[:10]]
    return "\n".join(lines)


# ── choosing the batch ─────────────────────────────────────────────────

def coverage_index(facts_dir: Path = FACTS) -> dict[str, tuple[str, bool]]:
    """NAME -> (0xADDRESS, reached) from build/facts/coverage.tsv, with a fallback: when
    coverage.tsv is empty (the index is mid-rebuild -- the facts layer starts a run by
    truncating it before the rows go in) `by_address/0xADDR.json` holds every record and
    is read here so a check-names run started against a regenerating facts layer still
    finds the functions the audit names."""
    path = facts_dir / "coverage.tsv"
    out: dict[str, tuple[str, bool]] = {}
    if path.exists():
        for line in path.read_text().splitlines()[1:]:
            parts = line.split("\t")
            if len(parts) < 3 or not parts[0].startswith("0x"):
                continue
            out[parts[1]] = (parts[0], parts[2] == "yes")
    if out:
        return out
    by_address = facts_dir / "by_address"
    if by_address.is_dir():
        for record_path in by_address.glob("0x*.json"):
            try:
                loaded = json.loads(record_path.read_text())
            except (OSError, ValueError):
                continue
            name = str(loaded.get("name") or "")
            if not name:
                continue
            address = loaded.get("address_hex") or record_path.stem
            out[name] = (str(address), bool(loaded.get("reached")))
    return out


def names_from_tsv(path: Path | str, where: Sequence[str] = ()) -> tuple[str, ...]:
    """The names in a TSV's `name` column, in file order, keeping the rows that match every
    `key=value` of WHERE (e.g. `kind=function` and `verdict=wrong` on the name audit)."""
    path = Path(path)
    if not path.exists():
        return ()
        return ()
    lines = path.read_text().splitlines()
    if not lines:
        return ()
    header = lines[0].split("\t")
    wanted = dict(item.split("=", 1) for item in where if "=" in item)
    names: list[str] = []
    for line in lines[1:]:
        row = dict(zip(header, line.split("\t"), strict=False))
        name = row.get("name", "")
        if name and all(row.get(key, "") == value for key, value in wanted.items()):
            names.append(name)
    return tuple(dict.fromkeys(names))


def random_reached(count: int, seed: int, pool: Sequence[str] | None = None,
                   exclude: Sequence[str] = (), facts_dir: Path = FACTS) -> tuple[str, ...]:
    """COUNT function names from symbols/db whose `wf facts` record says a recorded route
    reached them, drawn at random from SEED (so the same run id takes the same names)."""
    index = coverage_index(facts_dir)
    out_of_the_way = set(exclude)
    reached = [name for name in (pool if pool is not None else sorted(index))
               if name not in out_of_the_way and index.get(name, ("", False))[1]]
    reached = [name for name in reached if (SYMBOLS_DB / f"{name}.yaml").exists()]
    rng = random.Random(seed)
    rng.shuffle(reached)
    return tuple(sorted(reached[:max(0, count)]))


def description_of(name: str) -> str:
    """The symbols/db entry's own words about NAME: the summary, the interface, the details and
    the open questions. Never the facts and never the assembly: the claims have to be written
    before either is looked at."""
    import yaml                                                       # noqa: PLC0415
    path = SYMBOLS_DB / f"{name}.yaml"
    if not path.exists():
        return f"(there is no symbols/db entry for {name})"
    try:
        loaded = yaml.safe_load(path.read_text())
    except yaml.YAMLError:
        return f"(the symbols/db entry for {name} does not parse)"
    if not isinstance(loaded, dict):
        return f"(the symbols/db entry for {name} is not a mapping)"
    parts = [" ".join(str(loaded[key]).split()) for key in ("summary", "interface")
             if isinstance(loaded.get(key), str) and str(loaded[key]).strip()]
    details = loaded.get("details")
    if isinstance(details, str) and details.strip():
        parts.append(" ".join(details.split()))
    questions = loaded.get("open_questions")
    if isinstance(questions, list) and questions:
        parts.append("open questions: "
                     + "; ".join(" ".join(str(item).split()) for item in questions[:3]))
    return " / ".join(parts) or "(the entry has no summary, details or interface)"


def drop_run_artifacts(run_dir: Path) -> None:
    """The states and shots a run left under its own directory are regenerable."""
    shutil.rmtree(run_dir / "states", ignore_errors=True)