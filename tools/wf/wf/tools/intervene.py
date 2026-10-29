"""`wf intervene`: mGBA intervention runner for causal checks of function names.

A function's name makes a prediction about what it does ("drawHpBar -> updates
the HP digits' OAM entries each frame"). The runtime-evidence workflow sees
what a function *did*, but didn't vary it; this one varies it and watches.
Three interventions:

  - `skip`:    entry -> 0x4770 (`bx lr`); the function has no effect.
  - `return K`: entry -> `movs r0, #K ; bx lr` (0<=K<=255). A constant return
               refutes "the function computes something different from K" if
               the inputs differ from K look the same; useful for return-type
               booleans ("did the player die?").
  - `null`:    entry rewritten with its own bytes (control: must produce no
               difference, proving the replay is deterministic; if a `null`
               ever differs, that route/window is non-deterministic and is
               invalidated as a start point).

The pure parts here:

  - select_intervene_targets(...)  from the facts layer (build/facts/by_address)
  - find_function(name_or_addr)    bn6f.elf via tools/lifter/elfinfo.py
  - function_facts(...)            which routes execute a function (wf facts)
  - ensure_route_states(...)       cache mgba_run states along a route
  - patch_rom(...)                 reads orig ROM, returns a patched copy
  - first_game_frame(...)          scan + bisect with --count-entry over the WHOLE
                                   route, from the cached states
  - observe(...)                   run baseline + patched from the same state
                                   with the same trace, observe anchors
  - anchors_cfg(...)               read symbols/anchors.yaml
  - make_predicate(...)            a small typed language over the JSON output
  - intervene_result(...)          fold the observations into the result JSON
  - run_intervene_one(...)         the end-to-end: target, frame, observe, JSON

Pilot: `wf intervene-pilot` (graphs/intervene.py) picks 24 named, frequently
executed functions and 24 random reached ones, runs null + skip + return:0 +
return:1 on each, and prints a table.

Cached emulator states live under build/intervene/states/ROUTE/ (shared by every
function on that route); the patched ROM of a run is written to a temp dir and
deleted with it. Output JSON per (function, intervention, route) under
build/intervene/RUN/.
"""
from __future__ import annotations

import bisect
import io
import json
import os
from functools import lru_cache
import random
import shlex
import sys
import re
import shutil
import struct
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET
from collections.abc import Iterator, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Literal, cast

import yaml

from wf.repo import REPO, RUNS, TRACES
from wf.types import (FrameCount, FunctionName, GuestAddress, Result, Route, RouteName, ToolMessage,
                      SymbolName)

# wf/repo.py puts the main checkout's tools/ on sys.path for the small helpers (gamepaths,
# llm, funcindex, symdb). The lifter is a package under tools/, so the same path lookup lets
# `import lifter.elfinfo` find it; we add tools/ defensively in case REPO is a worktree root.
_TOOLS = str(REPO / "tools")
if _TOOLS not in sys.path:
    sys.path.insert(0, _TOOLS)

import lifter.elfinfo as elfinfo  # tools/lifter/elfinfo.py: bn6f ELF / ROM bytes / function ranges
import funcindex  # tools/funcindex.py: lifted units (with raw bn6f-source addresses)

PROJECT = "recomp-intervene"
DIR = REPO / "build/intervene"                            # all intervene outputs
STATES_DIR = DIR / "states"                              # cached emulator states, per route
PATCHED_DIR = DIR / "patched-roms"                       # patched copies of bn6f.gba (never the original)
# The facts layer (`wf facts`, tools/wf/wf/tools/facts.py): one JSON per function
# address, saying which recorded routes execute it. It lives in the main checkout
# and is never written here; an absolute path, overridable for a worktree.
FACTS_DIR = Path(os.environ.get("WF_FACTS_DIR") or REPO / "build" / "facts" / "by_address")
DEFAULT_FRAMES = 120                                     # the brief's default window length
PATCH_HEAD_MIN = 4                                       # refuse functions shorter than this in bytes (max patch = movs r0,#K; mov pc,lr)
DEFAULT_FRAME_TARGETS = (8, 16, 32)                      # candidate frame counts to observe at
EVERY_ANCHORS_DEFAULT = 1                                # frame stride for anchors (1 = every frame)
PREDICATE_TIMEOUT = 30                                   # mgba_run per observation, seconds
PREDICATE_STATE_TIMEOUT = 30                             # --save-state per observation, seconds
COUNT_TIMEOUT = 900                                      # one --count-entry probe, seconds
STATE_RUN_TIMEOUT = 1800                                 # one whole-route --save-state-every run
LEAD_IN_FRAMES = 8                                       # how many frames before f0 the state is taken at
STATE_STRIDE_MIN = 32                                    # finest cached-state spacing, game frames
STATE_STRIDE_TARGET = 400                                # a route keeps at most ~this many states
SHOT_KEEP_PAIRS = 1                                      # screenshot pairs kept per run (inputs, not results)

# Bytes
# # Thumb patch encodings
#
# The patched ROM's bytes at `function.address`. The brief selects the return
# encoding from the function's own epilogue (`return_kind_for` decides which):
#
# | function returns with     | skip bytes    | reasoning                                      |
# |---------------------------|---------------|-------------------------------------------------|
# | `pop {…pc}`               | `f7 46`       | `mov pc, lr` does NOT take the IS on ARMv4T     |
# | `mov pc, X`               | `f7 46`       | so it matters when LR has bit 0 clear (`mov lr,|
# | `bx <other reg>`          | `f7 46`       | pc` left it clear): a `bx lr` would land in ARM.|
# | unknown                   | `f7 46`       | the brief's safe default.                       |
# | `bx lr` (and only that)   | `70 47`       | caller set LR with bit 0 set (Thumb call); bx  |
# |                           |               | stays Thumb. No `pop`/`mov pc` in this fn; safe.|
#
# The previous version always used `push {lr}; pop {pc}` (works everywhere); the
# brief asks for an encoding that mirrors the function's own epilogue, so we
# narrow it where we can detect the function's choice. `mov pc, lr`/`bx lr` is
# 2 bytes; the old `push {lr}; pop {pc}` was 4. PATCH_HEAD_MIN shifts from 6 to
# 4 (the longest patch is now `movs r0,#K; <return>`, 4 bytes).
#
# See docs/workflows/intervene-notes.md for the call_m4aSoundMain traces and
# the per-trampoline `mov lr, pc; bx r0; pop {r0}; bx r0` shape that made
# patched-bx-lr crash with 2.4M illegal opcodes.
MOV_PC_LR_BYTES = b"\xf7\x46"                            # THUMB `mov pc, lr` (0x46F7)
BX_LR_BYTES = b"\x70\x47"                                # THUMB `bx lr` (0x4770)
SKIP_BYTES = MOV_PC_LR_BYTES                             # the safe default; skip_bytes_for patches it
# `movs r0, #(K<<0)` is 0x2000|K (LSB set = the 8-bit immediate form), 2 bytes.
# The trailing 2-byte return is the same one `skip` uses for this function. Total: 4 bytes.


# ── return-kind detection ─────────────────────────────────────
#
# The patch's return encoding has to be decided from the function's *instructions*,
# not from a byte pattern. A byte-pattern scan of the function's last bytes reads
# the literal pool as code: `cgbSoundInit_814EB84` ends with the data word
# 0x4770df2a, whose low half is exactly `bx lr` (0x4770) -- a pattern scan calls the
# function `bx lr` and the patch would then be the encoding that crashes when the
# caller's LR has bit 0 clear.
#
# So the function's code is decoded with the repo's own ARMv4T decoder
# (tools/lifter/isa.py, the one the lifter uses) over the ranges the ELF's $t/$d
# mapping symbols mark as code, and the returns are read off the decoded
# instructions. The mapping symbols are what separates `cgbSoundInit`'s literal
# pool from its code; the decoder is what keeps a `pop {…,pc}` inside a function
# from being read as a `bx lr` somewhere else.

def _code_ranges(info: elfinfo.ElfInfo, addr: int, size: int) -> tuple[tuple[int, int], ...]:
    """The THUMB code ranges inside [addr, addr+size), from the ELF's mapping symbols.

    `$t` marks THUMB code, `$a` ARM code, `$d` data. Only `$t` ranges are decoded
    here: an ARM range inside a THUMB function (`umul3232H32` has one) has ARM
    return semantics, and the caller of `return_kind_for` gets "unknown" for those.
    """
    mapping = info.mapping
    if not mapping:
        return ((addr, addr + size),)
    addrs = [a for a, _kind in mapping]
    end = addr + max(0, size)
    out: list[tuple[int, int]] = []
    index = bisect.bisect_right(addrs, addr) - 1
    if index < 0:
        index = 0
    kind = mapping[index][1]
    pos = addr
    while pos < end and index + 1 < len(mapping):
        nxt = min(mapping[index + 1][0], end)
        if kind == "t":
            out.append((pos, nxt))
        pos = nxt
        index += 1
        kind = mapping[index][1]
    if pos < end and kind == "t":
        out.append((pos, end))
    return tuple(out)


def _has_arm_code(info: elfinfo.ElfInfo, addr: int, size: int) -> bool:
    """True if the function's range holds ARM code (an `$a` mapping symbol)."""
    end = addr + max(0, size)
    return any(a >= addr and a < end and kind == "a" for a, kind in info.mapping)


def _function_returns(info: elfinfo.ElfInfo, addr: int, size: int) -> tuple[str, ...]:
    """Every return-ish instruction of the function, in program order.

    "pop_pc"     `pop {…, pc}`     (0xBDxx) -- the SDK's epilogue
    "mov_pc_lr"  `mov pc, lr`      (0x46F7)
    "mov_pc"     `mov pc, <reg>`   (0x46xx)
    "bx_lr"      `bx lr`           (0x4770)
    "bx_other"   `bx <other reg>`  (0x47xx) -- returns to a computed address
    """
    from lifter import isa  # noqa: PLC0415  (tools/lifter/isa.py: the ARMv4T decoder)
    out: list[str] = []
    for start, stop in _code_ranges(info, addr, size):
        at = start
        while at + 1 < stop:
            hw = info.u16(at)
            if hw is None:
                break
            nxt = info.u16(at + 2)
            # A BL is two halfwords; the decoder needs the second one to know it is a call.
            if (hw >> 11) == 0b11110 and nxt is not None and (nxt >> 11) == 0b11111:
                insn = isa.decode_thumb(at, hw, nxt)
            else:
                insn = isa.decode_thumb(at, hw, None)
            if insn.kind == "block" and insn.op == "ldm" and 15 in insn.regs:
                out.append("pop_pc")
            elif insn.kind == "dp" and insn.op == "mov" and insn.rd == 15:
                source = insn.op2.rm if insn.op2 is not None else -1
                out.append("mov_pc_lr" if source == 14 else "mov_pc")
            elif insn.kind == "bx":
                out.append("bx_lr" if insn.rm == 14 else "bx_other")
            at += insn.size
    return tuple(out)


@lru_cache(maxsize=4096)
def _return_kind_cached(addr: int, size: int) -> str:
    info = _load_b6f_info()
    if info.rom_offset(addr) is None:
        return "unknown"
    if _has_arm_code(info, addr, size):
        # Mixed THUMB/ARM function (the THUMB entry jumps into ARM code that ends in
        # ARM `bx lr`, which DOES take the instruction set from LR's bit 0). The
        # safe default is the mode-preserving `mov pc, lr`.
        return "unknown"
    kinds = _function_returns(info, addr, size)
    if not kinds:
        return "unknown"
    # A function with several exits: the mode-preserving ones win. `mov pc, lr` is
    # correct whether the caller left LR's bit 0 set or clear, so it is safe for
    # every exit; `bx lr` is only used when EVERY exit is `bx lr`.
    for kind in reversed(kinds):
        if kind in ("pop_pc", "mov_pc_lr", "mov_pc"):
            return kind
    if all(kind == "bx_lr" for kind in kinds):
        return "bx_lr"
    return "bx_other"


def return_kind_for(function: FunctionInfo) -> str:
    """What return encoding `function` uses, read off its decoded instructions.

    Returns one of "pop_pc", "mov_pc_lr", "mov_pc", "bx_lr", "bx_other", "unknown".
    """
    return _return_kind_cached(int(function.address), int(function.size))


def skip_bytes(kind: str = "mov_pc_lr") -> bytes:
    """The bytes the skip patch writes for a return `kind`. Pure: takes the kind as
    an argument so unit tests can pin down each branch without a real ROM.

    'pop_pc' / 'mov_pc_lr' / 'mov_pc' / 'bx_other' / 'unknown' → `mov pc, lr` (0x46F7).
    'bx_lr' → `bx lr` (0x4770).
    """
    if kind == "bx_lr":
        return BX_LR_BYTES
    return MOV_PC_LR_BYTES


def skip_bytes_for(function: FunctionInfo) -> tuple[bytes, str]:
    """The bytes the skip patch writes at function.address, plus the chosen kind.

    Reads the function's own epilogue (`return_kind_for`) and picks the corresponding
    return. Per the brief: 'pop_pc' / 'mov_pc' / unknown → `mov pc, lr` (mode-
    preserving, safe when LR's bit 0 is clear); 'bx_lr' → `bx lr`. Cross-checked
    by the call_m4aSoundMain regression on battles1."""
    kind = return_kind_for(function)
    return skip_bytes(kind), kind


def return_k_bytes(k: int) -> bytes:
    if not 0 <= k <= 255:
        raise ValueError(f"return k must be 0..255 (got {k})")
    return bytes([0x00 | k, 0x20]) + SKIP_BYTES


def return_k_bytes_for(k: int, function: FunctionInfo | None = None, *,
                        kind: str | None = None) -> tuple[bytes, str]:
    """`movs r0, #K; <chosen return>`. The return matches the function's own
    epilogue: `mov pc, lr` by default, `bx lr` when the function returns with
    `bx lr`.

    `kind` short-circuits the ROM lookup -- unit tests pin the dispatcher this
    way; production callers pass a FunctionInfo and read the kind from it.
    """
    if not 0 <= k <= 255:
        raise ValueError(f"return k must be 0..255 (got {k})")
    if kind is None:
        kind = return_kind_for(function) if function is not None else "unknown"
    return bytes([0x00 | k, 0x20]) + skip_bytes(kind), kind


# ── results ───────────────────────────────────────────────────────────

class InterventionKind(str):
    """skip / return:K / null; thin string-tagged union around the patchers' inputs."""
    skip = "skip"
    return_k = "return:K"
    null = "null"

    @classmethod
    def parse(cls, text: str, k: int | None = None) -> tuple[str, int | None]:
        """'skip' | 'return' (K=0) | 'return:K' | 'return' with an explicit k | 'null'."""
        if text == "skip":
            return cls.skip, None
        if text == "null":
            return cls.null, None
        if text in ("return", cls.return_k):
            value = 0 if k is None else k
        elif text.startswith("return:"):
            value = int(text.split(":", 1)[1])
        else:
            raise ValueError(f"unknown intervention {text!r} (use 'skip', 'return:K', 'null')")
        if not 0 <= value <= 255:
            raise ValueError(f"return:K needs K in 0..255 (got {value})")
        return cls.return_k, value

    @staticmethod
    def canonical(kind: str) -> str:
        """The three intervention identities, without the K: 'return:K' -> 'return'.

        The graph and `run_intervene_one` both switch on the *kind* and carry K in a
        separate field, so a 'return:K' string that reaches them as the kind matches
        neither branch. Normalise at the boundary rather than widening every check."""
        return "return" if kind.startswith("return") else kind


class AnchorDiff(Result):
    """Per-anchor diff between baseline (orig ROM) and patched ROM replayed from the same
    state with the same trace. `first_frame` is the first game frame the anchor differs
    (None: always identical over the window). `differing_frames` is how many. `size`
    describes the difference (bytes / OAM entries / pixels), framed in `unit`."""
    name: str
    kind: str                              # what the anchor was: 'memory' | 'register' | 'screen' | 'fingerprint'
    first_frame: int | None
    differing_frames: int
    size: int
    unit: str                              # "bytes" | "entries" | "pixels" | "words" | "hashes"


class RunRecord(Result):
    """One (function, intervention, route) run: per-anchor diffs, the start/end frames,
    the first frame the function was exercised at in baseline, determinism status, and
    a screenshot pair + diff image at the first differing frame (if any)."""
    function: FunctionName
    address: GuestAddress
    intervention: str                      # 'skip' | 'return:K' | 'null'
    intervention_k: int | None = None
    route: RouteName
    rom_hashes: tuple[str, str]            # SHA-1 (or unique-tag) of base and patched ROM
    patch_bytes: str = ""                  # hex of what was written at the entry ("" for null)
    start_frame: int                       # game frame the run started from (cached state was at start_frame - lead_in)
    lead_in_frames: int                    # how many game frames before the first frame the function executed
    first_frame_executed: int | None       # first game frame the function executed in baseline
    frames_observed: int
    deterministic: bool | None = None      # null run produced no diff; or no null was run (None = not tested)
    hung: bool                             # no new game frame in 300 hardware frames
    hung_side: str = ""                    # 'baseline' | 'patched' | 'both' | '' (who stopped making frames)
    illegal_opcodes: tuple[int, int] = (0, 0)   # (baseline, patched) illegal opcodes mgba_run counted
    notes: tuple[ToolMessage, ...]
    anchor_diffs: tuple[AnchorDiff, ...]   # one per anchor (sorted by name)
    shot_baseline: str | None
    shot_patched: str | None
    shot_diff: str | None
    artifacts_dir: str                     # where the .png + .ppm + .json live


# ── targets ───────────────────────────────────────────────────────

# 24 named, frequently executed functions whose visible effect is obvious from their
# name: drawing/OAM/palette/fade/sound/text/menu plus battle logic. The pilot pulls 24
# from here and 24 more at random from every function the facts layer says some route
# executes. These are REAL bn6f.elf symbols (current `nm`), not model-proposed
# placeholders, and each is reached on at least one recorded route. Six of them are the
# functions an earlier pilot reported `hung` on: their re-run is the regression test.
PILOT_NAMED: tuple[str, ...] = (
    # sound
    "call_m4aSoundMain",
    "SoundMain",
    "sound_dispatchTask_8000630",
    # screen fade
    "SetScreenFade",
    "IsScreenFadeActive",
    "screenFade_InitFadeBlock_800634C",
    # graphics / tile animation
    "LoadGFXAnims",
    "LoadGFXAnim",
    "startScreen_AnimatePressStart_803E938",
    "sprite_gfxAnimState_GotoNext_8002310",
    # drawing: background tile maps, lines, text tile rows
    "drawBGTileMapLeftColumns_803066E",
    "owObject_lineDraw_8030CAC",
    "drawChipNameTileRow",
    # OAM
    "battle_drawChipCountsOAM_801C082",
    "sprite_submitOamSet_802F994",
    "copyOamEntry_80AA004",
    "chatbox_emitTextGridOAM_804021C",
    # palette
    "sprite_getPalette",
    "copyPalletesToIWRAM_8002650",
    # text
    "chatbox_stepTextPrinter_801CE92",
    # menu
    "pet_menuUpdateAndHandleExit_8005462",
    "submenuDrawHpGauges",
    # battle logic
    "RunBattleObjectLogic",
    "checkThenStartBattle_8005A8C",
)


def select_intervene_targets(count: int = 24, *,
                              named: Sequence[str] = PILOT_NAMED,
                              only: Sequence[str] = (),
                              rng_seed: int = 0,
                              reached: Sequence[str] | None = None) -> tuple[FunctionName, ...]:
    """The functions an intervene pilot watches. `named` are the obvious-effect ones
    (the names `wf investigate` proposed for drawing / palette / OAM / fade / sound
    routes). `only`, if set, narrows to those names regardless. When `named` is short
    of `count` the tail is drawn from `reached` -- the functions the facts layer says
    some route executes -- so the pilot also covers code nobody picked by hand."""
    if only:
        return tuple(name for name in only if name in named)
    chosen = list(named[: min(len(named), count)])
    if len(chosen) < count and reached:
        rng = random.Random(rng_seed)
        pool = sorted(set(reached) - set(chosen))
        rng.shuffle(pool)
        for pick in pool:
            if len(chosen) >= count:
                break
            try:
                find_function(pick)             # refuse RAM / ARM / too-short targets here
            except ValueError:
                continue
            chosen.append(pick)
    return tuple(chosen)


def reached_function_names() -> tuple[FunctionName, ...]:
    """Every function the facts layer marks as reached on some route, as `NAME` (the
    bn6f.elf spelling `find_function` accepts). This is the pool the pilot's random
    half is drawn from: a function the recordings never execute can only ever produce
    a `not reached` row, which is why the earlier pilot threw away 17 of 24 rows."""
    names: list[str] = []
    if not FACTS_DIR.is_dir():
        return ()
    for path in sorted(FACTS_DIR.glob("0x*.json")):
        try:
            data = json.loads(path.read_text())
        except (OSError, ValueError):
            continue
        if not data.get("reached"):
            continue
        name = str(data.get("name") or "")
        if name:
            names.append(name)
    return tuple(names)


# ── function lookup ───────────────────────────────────────────────────

# elf_symbols returns name -> address. Mode is from `info.funcs[addr & ~1]`.
# Address may be in RAM (lives at 0x02... or 0x03...) or ROM (0x08... or 0x09...).
# We refuse anything outside ROM, outside THUMB, or smaller than PATCH_HEAD_MIN.

@dataclass(frozen=True)
class FunctionInfo:
    """A function the intervene workflow can patch: ROM-resident, THUMB, long enough."""
    name: FunctionName
    address: GuestAddress
    size: int
    mode: Literal["t", "a"]


# ── the facts layer: which routes execute a function ────────────────

@dataclass(frozen=True)
class RouteFacts:
    """What `wf facts` (build/facts/by_address/0xADDR.json) records for one function on
    one recorded route: `executed` is the function's entry address appearing in the
    route's coverage profile, `frames` the route's length in game frames."""
    route: str
    executed: bool
    frames: int | None


def function_facts(address: GuestAddress) -> tuple[RouteFacts, ...]:
    """Every route the facts layer holds for `address`, or () when the record is
    missing (facts not built for this function; the caller then falls back to
    searching). Reads a file and nothing else -- no emulator, no writes."""
    path = FACTS_DIR / f"0x{int(address):08X}.json"
    if not path.exists():
        return ()
    try:
        data = json.loads(path.read_text())
    except (OSError, ValueError):
        return ()
    out: list[RouteFacts] = []
    for entry in data.get("routes") or ():
        name = str(entry.get("route") or "")
        if not name:
            continue
        frames = entry.get("frames")
        out.append(RouteFacts(route=name, executed=bool(entry.get("executed")),
                              frames=int(frames) if isinstance(frames, int) else None))
    return tuple(out)


def routes_executing(function: FunctionInfo) -> tuple[Route, ...]:
    """The recorded routes that execute `function`, shortest first. This is what
    replaces "try every route's bisect window": the facts already know, for every
    function, which recordings reach it, so the 17/24 `not reached` rows an earlier
    pilot produced came from searching 1200 frames of the wrong route, not from the
    functions being dead. A route the facts list but `wf/routes.py` does not know
    (battlesweep, chips, fuzz, maps: coverage with no recording) is dropped -- there
    is no trace to replay."""
    from wf.routes import ALL_ROUTES, LONG_ROUTES                     # noqa: PLC0415
    by_name = {str(r.name): r for r in ALL_ROUTES + LONG_ROUTES}
    facts = function_facts(function.address)
    chosen: list[tuple[int, str, Route]] = []
    for entry in facts:
        if not entry.executed or entry.route not in by_name:
            continue
        route = by_name[entry.route]
        length = entry.frames if entry.frames is not None else int(route.length)
        chosen.append((length, entry.route, route))
    chosen.sort(key=lambda t: (t[0], t[1]))
    return tuple(route for _, _, route in chosen)


def confirm_route(function: FunctionInfo, candidates: Sequence[Route], *,
                  mgba_run: Path, bios: Path) -> tuple[Route | None, list[str]]:
    """The first of `candidates` (shortest first) that actually enters `function`,
    plus the ones it rejected.

    The facts layer decides the *order* -- it knows which recordings reach a function
    without running anything -- but it is loose about it: its `executed` comes from a
    coverage profile, and a profile credits a function's whole range, so a symbol whose
    tail is reached by a jump into the middle reads as executed although nothing ever
    branches to its entry (`sound_dispatchTask_8000630` on `wander`: 0 entries at
    0x08000630 in a whole-route `--count-entry`). The execution map is the exact check,
    so the route a run actually uses is one the map confirms."""
    rejected: list[str] = []
    for route in candidates:
        grid = ensure_route_states(route, mgba_run=mgba_run, bios=bios)
        known, segment = first_execution_segment(function.address, route, grid=grid,
                                                 last=int(route.length),
                                                 mgba_run=mgba_run, bios=bios)
        if not known or segment is not None:
            return route, rejected
        rejected.append(str(route.name))
    return None, rejected


def pick_route(function: FunctionInfo, *, routes: Sequence[Route] | None = None) -> Route | None:
    """The route to run `function`'s interventions on: the shortest recorded route the
    facts layer says executes it, confirmed by the route's execution map (see
    `confirm_route`). Falls back to a bisect search over the recorded routes when there
    is no facts record for this function (facts not built yet)."""
    executing = routes_executing(function)
    if routes is not None:
        by_name = {str(r.name): r for r in routes}
        executing = tuple(by_name[str(r.name)] for r in executing if str(r.name) in by_name)
    if executing:
        mgba_run = _resolve_mgba_run()
        bios = _resolve_game_bios()
        picked, _rejected = confirm_route(function, executing, mgba_run=mgba_run, bios=bios)
        return picked or executing[0]
    return _pick_route_by_search(function)


def _pick_route_by_search(function: FunctionInfo, *, max_window: int = 400,
                          routes: Sequence[Route] | None = None) -> Route | None:
    """No facts record for this function: bisect each recorded route's window in
    length order and take the first hit, exactly as the first version did."""
    from wf.routes import ALL_ROUTES                       # noqa: PLC0415
    candidates = sorted(routes or ALL_ROUTES, key=lambda r: int(r.length))
    mgba_run = _resolve_mgba_run()
    bios = _resolve_game_bios()
    for route in candidates:
        try:
            ff = first_game_frame(function, route, lead_in_frames=4,
                                  mgba_run=mgba_run, bios=bios,
                                  frames_window=max_window)
        except Exception:                                   # noqa: BLE001
            continue
        if ff is not None:
            return route
    return None


def _resolve_mgba_run() -> Path:
    import gamepaths  # noqa: PLC0415
    return Path(gamepaths.MGBA_RUN)


def _resolve_game_bios() -> Path:
    import gamepaths  # noqa: PLC0415
    return Path(gamepaths.GBA_BIOS)


def find_function(name_or_addr: str, *, on: Route | None = None) -> FunctionInfo:
    """Resolve a `wf intervene --function SPEC`. SPEC is a bn6f symbol name or
    a hex address (the THUMB bit is allowed). Raises ValueError on not-found /
    RAM / ARM / too-short functions (the brief asks us to mark such
    interventions `unsupported` in v1). If multiple functions share NAME (the
    bn6f disassembly has many region-specific LoadGFXAnims wrappers, for
    example), the `on` route's edges file picks the one most-called in that
    route; default: the first by address."""
    info = _load_b6f_info()
    spec = name_or_addr.strip()
    addr: int | None = None
    name: str | None = None
    if spec.startswith(("0x", "0X")) or re.fullmatch(r"[0-9A-Fa-f]{6,8}", spec):
        candidate = int(spec, 16) & ~1
        if candidate in info.funcs:
            addr = candidate
        elif (candidate | 1) in info.funcs:
            addr = candidate | 1
        else:
            raise ValueError(f"{spec}: no function at that address in bn6f.elf")
    else:
        candidates = [a for a, (m, _s, n) in info.funcs.items() if n == spec]
        if not candidates:
            raise ValueError(f"{spec}: not in bn6f.elf symbols (or no readable name)")
        if len(candidates) > 1 and on is not None:
            addr = _most_called(candidates, on) or candidates[0]
        else:
            addr = min(candidates)            # deterministic: smallest address
        name = spec
    mode, size, nm_name = info.funcs[addr]
    if mode != "t":
        raise ValueError(f"{nm_name} at {addr:#010x} is ARM (not THUMB); v1 refuses ARM interventions")
    if not (0x08000000 <= addr < 0x0A000000):
        raise ValueError(f"{nm_name} at {addr:#010x} is not in ROM; v1 refuses RAM interventions")
    if size < PATCH_HEAD_MIN:
        raise ValueError(f"{nm_name} at {addr:#010x} is {size} bytes; need at least {PATCH_HEAD_MIN}")
    return FunctionInfo(name=FunctionName(name or nm_name), address=GuestAddress(addr), size=size,
                        mode=cast(Literal["t", "a"], mode))


def _most_called(addresses: Sequence[int], route: Route) -> int | None:
    """The address most often used as a call *target* in `route`'s edges file
    (NAME.edges). Ties break to the smallest address."""
    import gamepaths  # noqa: PLC0415
    path = Path(gamepaths.TRACES) / "coverage" / f"{route.name}.edges"
    if not path.exists():
        return None
    counts: dict[int, int] = {addr: 0 for addr in addresses}
    for line in path.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        parts = line.split()
        if len(parts) < 4:
            continue
        try:
            target = int(parts[1], 16) & ~1
        except ValueError:
            continue
        if target in counts:
            counts[target] += int(parts[3])
    best_addr = max(sorted(counts), key=lambda a: (counts[a], -a))
    return best_addr if counts[best_addr] > 0 else None


_BN6F_INFO: elfinfo.ElfInfo | None = None


def _load_b6f_info() -> elfinfo.ElfInfo:
    global _BN6F_INFO
    if _BN6F_INFO is None:
        _BN6F_INFO = elfinfo.load(str(REPO / "bn6f"))
    return _BN6F_INFO


# ── patching ──────────────────────────────────────────────────────────

def patch_bytes(orig: bytes, offset: int, new: bytes) -> bytes:
    """Replace ORIG[offset:offset+len(new)] with NEW (must be <= original slice length).
    Returns a fresh bytes of the same length. Raises if the patch would straddle
    a different section / odd boundary."""
    if offset < 0 or offset + len(new) > len(orig):
        raise ValueError(f"patch @{offset:#x} of {len(new)} bytes is outside ROM ({len(orig)} bytes)")
    if offset & 1:
        raise ValueError(f"patch @{offset:#x} starts at an odd byte address (THUMB-only)")
    return orig[:offset] + new + orig[offset + len(new):]


def patch_for(intervention: str, k: int | None, function: FunctionInfo,
              baseline_bytes: bytes) -> bytes:
    """The bytes that `intervention` writes at function.address. baseline_bytes
    is a small slice (>= the patch length) of the original ROM starting at
    function.address. The returned slice is the same length.

    `skip` and `return:K` are kind-aware: the return encoding matches the
    function's own epilogue (the per-function `pop {pc}`/ `mov pc, X` / `bx lr`
    detection). `null` re-writes the function's own bytes (whichever prefix the
    patch for this function consumes) so the ROM comes out byte-identical and
    any diff is the window's nondeterminism, not the patch."""
    name = intervention
    if name == "skip":
        payload, _kind = skip_bytes_for(function)
    elif name == "return":
        if k is None:
            raise ValueError("return:K needs K")
        payload, _kind = return_k_bytes_for(k, function)
    elif name == "null":
        # Whichever prefix the skip patch consumes on THIS function (2 bytes for
        # most, 4 bytes if return_kind ends up as `unknown` -- practically always
        # 2).  Using the same length as skip makes `patch_for('null')` line up
        # byte-for-byte with the original.
        skip_pay, _ = skip_bytes_for(function)
        payload = baseline_bytes[: len(skip_pay)]
    else:
        raise ValueError(f"unknown intervention {intervention!r}")
    if len(payload) > len(baseline_bytes):
        raise ValueError(f"function {function.name} at {function.address:#010x} has {len(baseline_bytes)} "
                         f"bytes of head; cannot fit {len(payload)}-byte patch")
    return payload


def patch_rom(orig_rom: Path, function: FunctionInfo, intervention: str, k: int | None,
              out_dir: Path) -> Path:
    """Produce a patched copy of orig_rom with a single function rewritten.
    Returns the path of the new ROM. Original is never touched."""
    out_dir.mkdir(parents=True, exist_ok=True)
    data = orig_rom.read_bytes()
    info = _load_b6f_info()
    offset = info.rom_offset(function.address)
    if offset is None:
        raise ValueError(f"function {function.name} at {function.address:#010x} has no ROM offset")
    head = data[offset: offset + PATCH_HEAD_MIN]
    patch = patch_for(intervention, k, function, head)
    patched = bytearray(data)
    patched[offset: offset + len(patch)] = patch
    dest = out_dir / _patched_name(function, intervention, k)
    dest.write_bytes(bytes(patched))
    return dest


def _patched_name(function: FunctionInfo, intervention: str, k: int | None) -> str:
    tag = intervention
    if k is not None:
        tag = f"return-{k}"
    return f"bn6f_{function.name}_{function.address:08x}_{tag}.gba"


# ── game-frame search ─────────────────────────────────────────────

# The search is a scan of the WHOLE route, not a 1200-frame window: an earlier pilot
# reported 17 of 24 functions as "not reached" because it only ever looked 20 seconds
# into one route. Two things make the whole route affordable:
#
#   - `ensure_route_states` caches an mGBA state every STATE_STRIDE frames along the
#     route in ONE whole-frame pass (no per-instruction stepping), and every function
#     on that route reuses them;
#   - each probe is `mgba_run --count-entry ADDR --profile-game 0 END` resumed from the
#     cached state at the probe's start, so a probe only ever steps the frames between
#     its start and END, never the route from the beginning.
#
# The coarse scan reads the route's execution map (one profiled pass per route, shared
# by every function on it) to learn WHICH segment of the route grid runs the function,
# then bisects the cumulative entry count inside that one segment. Without a usable map
# it falls back to galloping over the segments -- ranges 1, 2, 4, ... chunks wide, until
# one has a hit -- and binary-searching inside the hit range: log2(chunks) probes. The
# gallop is what `hit(chunk)`'s non-monotonicity (a function can run in one chunk and
# never again) forces; a plain binary search over the chunks assumes the opposite.
#
# The grid has to line up exactly with what `mgba_run --save-state-every N` writes --
# `PREFIX_%07u.state` at every multiple of N -- or every probe falls back to running the
# route from the beginning (an earlier version of this file asked for frames 1, 33, 65,
# ... against a grid mgba_run wrote at 32, 64, 96, ...; nothing lined up, `first_game_frame`
# degenerated into 13 full-route runs per function and a one-function pilot took minutes).

SEGMENT_FLUSH = 16                                         # segments between map writes

def state_stride(route: Route) -> int:
    """How many game frames between two cached states of `route`: the smallest power of
    two (at least STATE_STRIDE_MIN) that keeps a route down to ~STATE_STRIDE_TARGET
    states, so a short route is scanned finely and `beastsp` does not fill the disk."""
    length = max(1, int(route.length))
    stride = STATE_STRIDE_MIN
    while length // stride > STATE_STRIDE_TARGET:
        stride *= 2
    return stride


FINGERPRINT_LEN = 12                                          # sha1-prefix length used as the cache-key suffix


def route_fingerprint(route: Route) -> str:
    """A short, stable fingerprint of the route's recording files (`build/traces/ROUTE.trace`,
    `ROUTE.poke`, `ROUTE.sav`). The cache directory below uses it so a freshly re-imported
    route (beastsp was rebuilt from video on 2026-10-24) cannot reuse the previous batch's
    cached emulator states -- those states were taken against the OLD route's bytes, and
    replaying them against the NEW route can land the function at the wrong frame.

    Files are hashed in a fixed order with a sentinel between them so the same route cannot
    end up with the same fingerprint as a different one. Missing files contribute nothing
    (so a re-import that lacks the optional `.sav` still hashes the recording). The hash
    covers file CONTENTS, not absolute paths: the same recording under a worktree's path
    or under the main checkout's path must hash to the same fingerprint, so a re-import
    of the same bytes does not invalidate cached states. The route NAME is mixed in as a
    domain separator so two routes can never share a fingerprint even on identical bytes."""
    import gamepaths                                                     # noqa: PLC0415
    import hashlib                                                       # noqa: PLC0415
    h = hashlib.sha1()
    h.update(route.name.encode("utf-8"))
    h.update(b"\0")
    files = (route.trace,
             Path(gamepaths.TRACES) / f"{route.name}.poke",
             Path(gamepaths.TRACES) / f"{route.name}.sav")
    for path in files:
        h.update(path.name.encode("utf-8"))
        h.update(b"\0")
        if path.exists():
            with path.open("rb") as fh:
                while True:
                    chunk = fh.read(1 << 20)
                    if not chunk:
                        break
                    h.update(chunk)
            h.update(b"\xff")
    return h.hexdigest()[:FINGERPRINT_LEN]


def route_states_dir(route: Route) -> Path:
    """`build/intervene/states/ROUTE/FINGERPRINT/` -- one directory per recorded route *version*,
    shared by every function, every intervention and every run on that version. The version
    is `route_fingerprint(route)`: re-importing a route under the same name produces a
    different fingerprint, so a different set of cached states is used (the old directory
    is left alone, accumulating as a stale version that nothing reads)."""
    return STATES_DIR / str(route.name) / route_fingerprint(route)


def state_path_at(route: Route, frame: int) -> Path:
    """`.../states/ROUTE/FINGERPRINT/ROUTE_%07d.state`. Seven digits, because that is what
    `mgba_run --save-state-every` writes (`"%s_%07u.state"`, tools/mgba_run.c) -- the
    name has to agree with the bulk pass's output or nothing is reused."""
    return route_states_dir(route) / f"{route.name}_{int(frame):07d}.state"


def _state_is_usable(path: Path) -> bool:
    return path.exists() and path.stat().st_size > 0


def cached_frames(route: Route) -> tuple[int, ...]:
    """The game frames `ROUTE` already has a usable cached state for, ascending.
    Reads the directory (no emulator); what is missing is made by `ensure_route_states`."""
    out: list[int] = []
    prefix = f"{route.name}_"
    for path in route_states_dir(route).glob(f"{prefix}*.state"):
        stem = path.stem
        try:
            frame = int(stem[len(prefix):])
        except ValueError:
            continue
        if _state_is_usable(path):
            out.append(frame)
    return tuple(sorted(out))


def ensure_route_states(route: Route, *, mgba_run: Path, bios: Path,
                        until: int | None = None) -> tuple[int, ...]:
    """Cache an mGBA state every `state_stride(route)` game frames of `route`, plus one
    at frame 1 (the first state a run can start from: mgba_run exits immediately at
    game frame 0). One whole-frame pass over the route does all of it; a route whose
    states are already there costs nothing. Returns the cached frames, ascending.

    The grid is frame 1 plus every multiple of the stride -- exactly where
    `mgba_run --save-state-every STRIDE PREFIX` writes. Nothing is saved one frame at a
    time afterwards: each of those would be a run from the start of the route, so a
    half-built grid is left half-built (the search then covers the prefix that is
    there) rather than costing hundreds of full-route passes."""
    stride = state_stride(route)
    last = int(until if until is not None else route.length)
    out_dir = route_states_dir(route)
    out_dir.mkdir(parents=True, exist_ok=True)
    grid = [f for f in range(stride, last + 1, stride)]
    missing = [f for f in grid if not _state_is_usable(state_path_at(route, f))]
    head = not _state_is_usable(state_path_at(route, 1))
    if missing or head:
        prefix = str(out_dir / str(route.name))
        import gamepaths  # noqa: PLC0415
        sav = Path(gamepaths.TRACES) / f"{route.name}.sav"
        cmd = [str(mgba_run), str(_origin_rom()), "--quiet",
               "--frames", str(last * 3 + 1000), "--bios", str(bios),
               "--input", str(route.trace),
               "--game-frame-addr", _game_frame_addr(),
               "--until-game-frame", str(last),
               "--save-state-every", str(stride), prefix]
        if sav.exists():
            cmd.append("--save")
            cmd.append(str(sav))
        subprocess.run(cmd, capture_output=True, text=True, check=False,
                       timeout=STATE_RUN_TIMEOUT)
        if head or not _state_is_usable(state_path_at(route, 1)):
            save_state_at(route, at_frame=1, mgba_run=mgba_run, bios=bios,
                          out=state_path_at(route, 1))
    have = [f for f in grid if _state_is_usable(state_path_at(route, f))]
    if _state_is_usable(state_path_at(route, 1)):
        have.insert(0, 1)
    return tuple(have)


def nearest_cached_state(route: Route, frame: int) -> int | None:
    """The greatest cached game frame at or before `frame`, or None if the route has no
    state there yet. A run wants to start `lead_in_frames` before the function first
    executes; snapping that down to the cached grid is what keeps it from having to
    replay the route from the beginning just to reach the start frame."""
    at_or_before = [f for f in cached_frames(route) if f <= int(frame)]
    return at_or_before[-1] if at_or_before else None


# ── the per-route execution map ──────────────────────────────────
#
# One profiled pass over a route answers "which addresses execute, and in which
# stride-sized segment" for EVERY function on that route at once: `mgba_run
# --profile-game` writes one line per PC that got cycles. That pass costs one whole
# route in emulated frames -- 34 s for wander, 85 s for battles1, ~30 min for beastsp --
# and it is shared by every function on the route, where a per-function gallop costs
# twice the function's own first frame (`--count-entry` counts one address at a time).
#
# So the map is built once, cached on disk next to the states, and a function's first
# frame then costs ~log2(stride) cheap probes inside its own segment.

EXEC_MAP_DIR = DIR / "exec-maps"                            # EXECMAP_ROUTE.json per route
EXEC_MAP_THREADS = 4                                        # segments are independent runs


def exec_map_path(route: Route) -> Path:
    return EXEC_MAP_DIR / f"{route.name}.json"


def _load_exec_map(route: Route) -> dict[int, int] | None:
    """The cached {pc: first segment index} map, or None when there is none or it was
    built from a different ROM / route length / grid."""
    path = exec_map_path(route)
    if not path.exists():
        return None
    try:
        data = json.loads(path.read_text())
    except (OSError, ValueError):
        return None
    if (data.get("rom") != _short_hash(_origin_rom())
            or int(data.get("stride") or 0) != state_stride(route)
            or int(data.get("length") or 0) != int(route.length)):
        return None
    return {int(pc): int(idx) for pc, idx in (data.get("first") or {}).items()}


def _write_exec_map(route: Route, first: dict[int, int], done: int, grid: tuple[int, ...]) -> None:
    EXEC_MAP_DIR.mkdir(parents=True, exist_ok=True)
    tmp = exec_map_path(route).with_suffix(".json.tmp")
    tmp.write_text(json.dumps({"rom": _short_hash(_origin_rom()),
                               "route": str(route.name), "stride": state_stride(route),
                               "length": int(route.length), "segments": list(grid),
                               "covered": done, "first": {str(pc): idx for pc, idx in first.items()}}))
    tmp.replace(exec_map_path(route))


def ensure_route_exec_map(route: Route, *, grid: Sequence[int], last: int,
                          mgba_run: Path, bios: Path) -> dict[int, int] | None:
    """The route's {pc: first segment index} map, building (or topping up) it if needed.
    Segments are independent `mgba_run --profile-game` runs resumed from the cached
    state at the segment's first frame, so they fan out over EXEC_MAP_THREADS; the map
    is written to disk every SEGMENT_FLUSH segments so an interrupted pass resumes."""
    cached = _load_exec_map(route)
    first: dict[int, int] = dict(cached) if cached is not None else {}
    edges = list(grid) + [last + 1]
    done = len(cached) if cached is not None else 0        # segments already in `first`
    if cached is None or done >= len(edges) - 1:
        if cached is not None and done >= len(edges) - 1:
            return cached
        first, done = {}, 0
    todo = range(done, len(edges) - 1)
    if todo:
        from concurrent.futures import ThreadPoolExecutor                  # noqa: PLC0415
        workdir = EXEC_MAP_DIR / str(route.name)
        workdir.mkdir(parents=True, exist_ok=True)
        try:
            with ThreadPoolExecutor(max_workers=EXEC_MAP_THREADS) as pool:
                for index, pcs in enumerate(pool.map(
                        lambda i: _profile_pcs(route, edges[i], edges[i + 1], mgba_run=mgba_run,
                                               bios=bios, workdir=workdir), todo),
                        start=done):
                    for pc in pcs:
                        first.setdefault(pc, index)
                    if (index + 1) % SEGMENT_FLUSH == 0:
                        _write_exec_map(route, first, index + 1, tuple(edges))
        finally:
            shutil.rmtree(workdir, ignore_errors=True)
        if first:                                   # an all-failed pass learns nothing;
            _write_exec_map(route, first, len(edges) - 1, tuple(edges))  # do not cache it
    return first


SEGMENT_FLUSH = 16                                         # segments between map writes


def _profile_pcs(route: Route, start: int, until: int, *, mgba_run: Path, bios: Path,
                 workdir: Path) -> list[int]:
    """Every PC that got cycles in the game frames [start, until) -- the set of
    addresses the route actually executes there. `mgba_run` writes profile.txt into the
    working directory, so each worker gets its own."""
    import tempfile                                                     # noqa: PLC0415
    import gamepaths                                                    # noqa: PLC0415
    sav = Path(gamepaths.TRACES) / f"{route.name}.sav"
    with tempfile.TemporaryDirectory(dir=str(workdir)) as here:
        cmd = [str(mgba_run), str(_origin_rom()),
               "--quiet", "--frames", str((until - start) * 4 + 64),
               "--bios", str(bios), "--input", str(route.trace),
               "--game-frame-addr", _game_frame_addr(),
               "--load-state", str(state_path_at(route, start)),
               "--resume-game-frame", str(start),
               "--until-game-frame", str(until + 8),
               "--profile-game", "0", str(until)]
        if sav.exists():
            cmd.extend(["--save", str(sav)])
        try:
            subprocess.run(cmd, capture_output=True, text=True, check=False,
                           timeout=STATE_RUN_TIMEOUT, cwd=here)
        except subprocess.TimeoutExpired:
            return []
        profile = Path(here) / "profile.txt"
        if not profile.exists():
            return []
        out: list[int] = []
        for line in profile.read_text().splitlines():
            if line.startswith("#"):
                continue
            head = line.split(" ", 1)[0]
            try:
                out.append(int(head, 16))
            except ValueError:
                continue
        return out


def first_execution_segment(address: GuestAddress, route: Route, *,
                            grid: Sequence[int], last: int, mgba_run: Path,
                            bios: Path) -> tuple[bool, int | None]:
    """Which segment of `route` first executes `address`, from the route's execution map.

    `(True, index)`  the map says segment `index` is the first one that runs it;
    `(True, None)`   the map is complete and the route never runs it;
    `(False, None)`  no usable map (the pass failed) -- the caller has to gallop.

    This is the answer to "does any recording reach this function", and it is the same
    answer for every function on the route, which is why the map is worth building."""
    mapping = ensure_route_exec_map(route, grid=grid, last=last, mgba_run=mgba_run, bios=bios)
    if not mapping:
        return False, None
    index = mapping.get(int(address))
    return True, index


def first_game_frame(function: FunctionInfo, route: Route, *,
                     lead_in_frames: int = LEAD_IN_FRAMES,
                     mgba_run: Path,
                     bios: Path,
                     frames_window: int | None = None) -> int | None:
    """The first game frame of `route` on which it executes `function`, or None if it
    never does within `frames_window` game frames (None: None means the whole route).

    The hit frame is what `wf intervene` starts from, with `lead_in_frames` game frames
    of headroom (so the state is never taken inside the function). See the comment
    above for how the scan is made affordable."""
    last = int(frames_window if frames_window is not None else route.length)
    frames = [f for f in ensure_route_states(route, mgba_run=mgba_run, bios=bios,
                                              until=last) if f <= last]
    if not frames:
        return None
    chunks = len(frames)

    def _count_chunks(first: int, stop: int) -> int:
        """Entries over the chunks [first, stop): frames [frames[first], frames[stop]),
        with the tail chunk running to the end of the search."""
        lo = frames[first]
        hi = frames[stop] if stop < chunks else last + 1
        return _count_entry(function.address, route, lo, hi,
                            mgba_run=mgba_run, bios=bios)

    # 1. the execution map, when it is there: it names the first segment outright.
    authoritative, segment = first_execution_segment(
        function.address, route, grid=frames, last=last, mgba_run=mgba_run, bios=bios)
    if authoritative and segment is not None:
        lo, hi = segment, segment + 1
    elif authoritative:
        return None                              # the route never executes it
    else:
        # 2. no map: gallop over chunk ranges 1, 2, 4, ... chunks wide.
        start, stop = 0, 1
        while True:
            stop = min(start + 1 if start == 0 else start * 2 + 1, chunks)
            if _count_chunks(start, stop) > 0:
                break
            if stop >= chunks:
                return None
            start = stop
        lo, hi = start, stop                      # lower bound inside the hit range
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if _count_chunks(lo, mid) > 0:
                hi = mid
            else:
                lo = mid
    anchor = frames[lo]                            # the hit is in [anchor, top)
    top = frames[hi] if hi < chunks else last + 1
    # The exact frame, by bisecting the CUMULATIVE count from that chunk's own cached
    # state. The count's start frame has to stay pinned at `anchor`: `_count_entry`
    # resumes from the state at its first frame and answers 0 when there is none, and
    # only grid frames have a state -- letting the start wander off the grid (which a
    # plain [lo, mid) bisect does the moment the left half is empty) reports every
    # probe as "no hit" and walks the answer to the end of the chunk.
    base = anchor
    while top - base > 1:
        mid = (base + top) // 2
        if _count_entry(function.address, route, anchor, mid,
                        mgba_run=mgba_run, bios=bios) > 0:
            top = mid
        else:
            base = mid
    return base


def _count_entry(address: GuestAddress, route: Route, start: int, until: int, *,
                 mgba_run: Path, bios: Path) -> int:
    """How many times the CPU entered `address` in the game frames [start, until).
    mgba_run only counts while it is stepping, so the probe is
    `--profile-game 0 until --count-entry ADDR` resumed from the cached state at
    `start` (which also keeps the cost to the frames in the window). `--until-game-frame`
    has to sit *past* the window: mgba_run checks it before the profile dump, and a
    value equal to the window's end would break out without printing the counts."""
    if until <= start:
        return 0
    state = state_path_at(route, start)
    if not _state_is_usable(state):
        # No cached state at `start`: the caller has to anchor its windows on a grid
        # frame (see first_game_frame). Answering 0 rather than running the route from
        # the beginning keeps a mis-anchored probe from costing a whole route pass.
        return 0
    import gamepaths  # noqa: PLC0415
    sav = Path(gamepaths.TRACES) / f"{route.name}.sav"
    cmd = [str(mgba_run), str(_origin_rom()),
           "--quiet", "--frames", str((until - start) * 4 + 64),
           "--bios", str(bios),
           "--input", str(route.trace),
           "--game-frame-addr", _game_frame_addr(),
           "--load-state", str(state),
           "--resume-game-frame", str(start),
           "--until-game-frame", str(until + 8),
           "--profile-game", "0", str(until),
           "--count-entry", str(int(address))]
    if sav.exists():
        cmd.extend(["--save", str(sav)])
    try:
        # cwd: --profile-game writes its profile.txt into the working directory, and the
        # count histogram is printed with it. Keep that out of the checkout.
        done = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=COUNT_TIMEOUT, cwd=str(route_states_dir(route)))
    except subprocess.TimeoutExpired:
        return 0
    total = 0
    for line in done.stdout.splitlines():
        m = re.match(r"^count\s+(\d+)\s+", line)
        if m:
            total += int(m.group(1))
    return total


def _origin_rom() -> Path:
    import gamepaths  # noqa: PLC0415
    return Path(gamepaths.ORIG_ROM)


def _game_bios() -> Path:
    import gamepaths  # noqa: PLC0415
    return Path(gamepaths.GBA_BIOS)


def _game_frame_addr() -> str:
    """The game's frame counter address as mgba_run expects a hex string (with
    the `0x` prefix). mgba_run's strtoul treats an 0x-prefixed value as hex,
    so a decimal-ish decimal string `33592276` would be parsed as a hex one.
    Pass the original '0x020093D4' through."""
    import gamepaths  # noqa: PLC0415
    value = gamepaths.GAME_FRAME_ADDR
    if value.startswith(("0x", "0X")):
        return value
    return f"0x{value}"


def _poke_arg(route: Route) -> str:
    """The mgba_run --poke-at lines a route needs (its NAME.poke, copied from tools/traces).
    mgba_run wants the file in the loop that runs --frames (the trace loop reads inputs
    on each game frame; the pokes are replayed by --poke-at at fixed points). The wf approach
    here is to leverage --poke once at boot via --save (NAME.sav), so the per-frame input
    file is the .trace alone. See tools/lockstep.sh for the same trick."""
    return ""                                              # NAME.sav replays via --save below


# ── state save / load ──────────────────────────────────────────

def save_state_at(route: Route, *, at_frame: int, mgba_run: Path, bios: Path,
                  out: Path) -> Path:
    """Save an mGBA state at the start of hardware frame whose game-frame is at_frame,
    replayed with route's .trace / .sav / .poke. Returns the .state path."""
    out.parent.mkdir(parents=True, exist_ok=True)
    if out.exists() and out.stat().st_size > 0:
        return out
    if at_frame < 1:
        raise ValueError(f"at_frame must be >= 1 (mgba_run --until-game-frame 0 exits immediately); "
                          f"got {at_frame}")
    import gamepaths  # noqa: PLC0415
    sav = Path(gamepaths.TRACES) / f"{route.name}.sav"
    save_arg = ["--save", str(sav)] if sav.exists() else []
    cmd = [str(mgba_run), str(_origin_rom()), "--quiet",
           "--frames", "200000", "--bios", str(bios),
           "--input", str(route.trace),
           "--game-frame-addr", _game_frame_addr(),
           "--until-game-frame", str(at_frame),
           "--save-state", str(out)] + save_arg
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False,
                          timeout=PREDICATE_STATE_TIMEOUT)
    (out.with_suffix(out.suffix + ".stderr")).write_text(proc.stderr)
    if not out.exists():
        # mgba_run sometimes writes the state file with an empty body when something went
        # wrong; the next call will retry. The pilot itself surfaces the error.
        out.write_bytes(b"")
    return out


# ── observation: run baseline + patched from same state, watch anchors ─

# Anchors summary
@dataclass(frozen=True)
class AnchorSpec:
    name: str
    base: int
    size: int
    block_size: int = 0           # 0 = full anchor; >0 = hash per block_size
    every: int = 1                # frames between samples
    registers: tuple[tuple[str, int], ...] = ()
    is_screen: bool = False
    is_register_set: bool = False
    unit: str = "bytes"


def load_anchors(path: Path | None = None) -> tuple[AnchorSpec, ...]:
    """Read symbols/anchors.yaml and produce the list of AnchorSpec. Two sections:
    `hardware:` (OAM, palette, VRAM, I/O register sets, screen) and `ram:` (game
    structs, whose names are only trustworthy once a `null` run has confirmed the
    route/window is deterministic). Looks in the worktree first, then the main tree
    (a worktree's symbols/ may not mirror the main checkout)."""
    if path is None:
        here = Path(__file__).resolve()
        for ancestor in here.parents:
            candidate = ancestor / "symbols/anchors.yaml"
            if candidate.exists():
                path = candidate
                break
        if path is None:
            path = REPO / "symbols/anchors.yaml"
    if not path.exists():
        raise FileNotFoundError(f"no anchors.yaml at {path}")
    raw = yaml.safe_load(path.read_text()) or {}
    out: list[AnchorSpec] = []
    for section in ("hardware", "ram"):
        for entry in raw.get(section) or []:
            if not isinstance(entry, dict):
                continue
            name = str(entry.get("name") or "")
            if not name:
                continue
            if "registers" in entry:
                regs = tuple((str(r["name"]), int(str(r["address"]), 0)) for r in entry["registers"])
                out.append(AnchorSpec(name=name, base=0, size=0, registers=regs, every=1,
                                      is_register_set=True, unit="registers"))
            elif name == "screen":
                out.append(AnchorSpec(name=name, base=0, size=0, every=int(entry.get("every", 1)),
                                      is_screen=True, unit="pixels"))
            else:
                base = int(str(entry["base"]), 0)
                size = int(str(entry["size"]), 0)
                block = int(str(entry.get("block_size", 0))) if "block_size" in entry else 0
                every = int(str(entry.get("every", 1)))
                out.append(AnchorSpec(name=name, base=base, size=size, block_size=block, every=every,
                                      unit="bytes" if not block else "hashes"))
    return tuple(out)


def _hash_block(data: bytes) -> int:
    """FNV-1a 64 (matches mgba_run's digest output: 1469598103934665603 mod 2^64)."""
    h = 1469598103934665603
    for x in range(0, len(data), 4):
        chunk = data[x:x + 4]
        value = int.from_bytes(chunk.ljust(4, b"\x00"), "little")
        h ^= value
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


@dataclass(frozen=True)
class SideRun:
    """What one side (baseline or patched) of an observation actually did. A hang is
    only trustworthy when it comes with this evidence: mgba_run's game-frame counter
    stopped advancing inside the window AND the CPU executed illegal opcodes (or the
    process was killed by the timeout). An earlier pilot reported `hung` from a
    `stalled:` line alone, which mgba_run also prints for an ordinary speed threshold,
    and called a function hung when the *baseline* -- the unpatched game -- was the
    side that stopped."""
    reached_frame: int | None            # last game frame the counter got to (None: never printed)
    want_frame: int
    timed_out: bool                     # the process was still running at the timeout
    illegal_opcodes: int                # mGBA's own count of illegal instructions executed
    stalled: bool                       # mgba_run wrote its own `stalled:` line

    @property
    def hung(self) -> bool:
        """The run stopped making game frames inside the window."""
        if self.timed_out:
            return True
        return self.reached_frame is None or self.reached_frame < self.want_frame


def observe(*, function: FunctionInfo, intervention: str, k: int | None,
            route: Route, state_path: Path,
            frames: int, baseline_rom: Path, patched_rom: Path,
            anchors: tuple[AnchorSpec, ...],
            mgba_run: Path, bios: Path,
            out_dir: Path) -> "Observation":
    """Replay `frames` game frames from `state_path` on baseline_rom AND patched_rom,
    capturing per-frame anchor data. Both sides run from the same cached state with the
    same trace and the same budget, so every difference is the intervention's doing."""
    label = f"{function.name}_{function.address:08x}_{intervention}{f'_{k}' if k is not None else ''}_{route.name}"
    base_dir = out_dir / label / "baseline"
    pat_dir = out_dir / label / "patched"
    base_dir.mkdir(parents=True, exist_ok=True)
    pat_dir.mkdir(parents=True, exist_ok=True)
    base_log = base_dir / "log.txt"
    pat_log = pat_dir / "log.txt"
    base_shots = base_dir / "shots"
    pat_shots = pat_dir / "shots"
    base_shots.mkdir(exist_ok=True)
    pat_shots.mkdir(exist_ok=True)
    start_frame = int(state_path.stem.split("_")[-1])
    base_run = _observe_one(rom=baseline_rom, route=route, state_path=state_path,
                            start_frame=start_frame, frames=frames, anchors=anchors,
                            log=base_log, shots_dir=base_shots,
                            mgba_run=mgba_run, bios=bios)
    pat_run = _observe_one(rom=patched_rom, route=route, state_path=state_path,
                           start_frame=start_frame, frames=frames, anchors=anchors,
                           log=pat_log, shots_dir=pat_shots,
                           mgba_run=mgba_run, bios=bios)
    diffs = _diff_log_lines(base_log, pat_log, base_shots, pat_shots, anchors, frames)
    _prune_shots(base_shots, pat_shots, diffs)          # keep the first differing pair only
    hung_sides = tuple(name for name, side in (("baseline", base_run), ("patched", pat_run))
                       if side.hung)
    return Observation(anchor_diffs=diffs, baseline=base_run, patched=pat_run,
                       shots=(base_shots, pat_shots), logs=(base_log, pat_log),
                       start_frame=start_frame, hung_side="|".join(hung_sides))


@dataclass(frozen=True)
class Observation:
    anchor_diffs: tuple[AnchorDiff, ...]
    baseline: SideRun
    patched: SideRun
    shots: tuple[Path, Path]
    logs: tuple[Path, Path]
    start_frame: int
    hung_side: str

    @property
    def hung(self) -> bool:
        return bool(self.hung_side)

    def notes(self) -> tuple[ToolMessage, ...]:
        """Evidence lines for a hang, so a `hung` row can be checked rather than taken
        on trust: which side stopped, how far its frame counter got, and how many
        illegal instructions mGBA counted before it did."""
        out: list[ToolMessage] = []
        for name, side in (("baseline", self.baseline), ("patched", self.patched)):
            if not side.hung:
                continue
            if side.illegal_opcodes:
                out.append(ToolMessage(
                    f"{name} run hung: stopped at game frame {side.reached_frame} of "
                    f"{side.want_frame} and mGBA counted {side.illegal_opcodes} illegal opcodes "
                    f"(execution left the function)"))
            elif side.timed_out:
                out.append(ToolMessage(
                    f"{name} run hung: still running when the timeout expired "
                    f"(stopped at game frame {side.reached_frame} of {side.want_frame}, "
                    f"no illegal opcodes -- an intervention-induced wait loop)"))
            else:
                out.append(ToolMessage(
                    f"{name} run hung: stopped at game frame {side.reached_frame} of "
                    f"{side.want_frame} with no illegal opcodes (an intervention-induced wait loop)"))
        if self.baseline.hung and not self.patched.hung:
            out.append(ToolMessage(
                "the unpatched baseline hung too: this window is not usable for "
                "comparing the intervention"))
        return tuple(out)


def _observe_one(*, rom: Path, route: Route, state_path: Path, start_frame: int,
                 frames: int, anchors: tuple[AnchorSpec, ...],
                 log: Path, shots_dir: Path,
                 mgba_run: Path, bios: Path) -> SideRun:
    """Run mgba_run with state + trace + per-frame observation flags. Writes stdout
    to `log`, stderr next to it, saves shots to `shots_dir`. Returns the evidence for
    whether the run kept making game frames for the whole window."""
    anchors = anchors or ()
    has_digest = any(not a.is_screen and not a.is_register_set for a in anchors)
    has_watch = any(a.is_register_set for a in anchors)
    has_screen = any(a.is_screen for a in anchors)
    hw_budget = max(frames * 4, 500)
    import gamepaths  # noqa: PLC0415
    sav = Path(gamepaths.TRACES) / f"{route.name}.sav"
    save_arg = ["--save", str(sav)] if sav.exists() else []
    cmd = [str(mgba_run), str(rom), "--quiet",
           "--frames", str(hw_budget),
           "--bios", str(bios),
           "--input", str(route.trace),
           "--game-frame-addr", _game_frame_addr(),
           "--load-state", str(state_path),
           "--until-game-frame", str(start_frame + frames),
           "--resume-game-frame", str(start_frame),
           ]
    if has_digest:
        cmd.extend(["--digest-every", "1"])
    if has_watch:
        # Single contiguous range: 0x04000000 .. 0x040000E0 covers DISPCNT..DMA control
        # (skipping over the few unmapped halfwords; mgba_run reads what's there).
        cmd.extend(["--watch", "0x04000000", "0xe0"])
    if has_screen:
        screen_every = max(a.every for a in anchors if a.is_screen)
        cmd.extend(["--shots", str(shots_dir), "--every", str(max(1, screen_every))])
    cmd.extend(save_arg)
    want_game_frame = start_frame + frames
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=PREDICATE_TIMEOUT * 30,
                              check=False)
    except subprocess.TimeoutExpired as error:
        log.write_text(error.stdout or "")
        stderr_text = error.stderr or ""
        log.with_suffix(".stderr").write_text(stderr_text)
        return SideRun(reached_frame=_last_reached(error.stdout or ""), want_frame=want_game_frame,
                       timed_out=True, illegal_opcodes=_illegal_opcodes(stderr_text),
                       stalled="stalled:" in stderr_text)
    log.write_text(proc.stdout)
    stderr_text = proc.stderr
    log.with_suffix(".stderr").write_text(stderr_text)
    return SideRun(reached_frame=_last_reached(proc.stdout), want_frame=want_game_frame,
                   timed_out=False, illegal_opcodes=_illegal_opcodes(stderr_text),
                   stalled="stalled:" in stderr_text)


_ILLEGAL_RE = re.compile(r"^(\d+) illegal opcodes executed in all$", re.MULTILINE)


def _illegal_opcodes(stderr_text: str) -> int:
    """mGBA's own count of illegal instructions (mgba_run prints it on stderr when it
    is nonzero). Zero when the CPU never left valid code -- the difference between
    "wedged in a wait loop" and "ran off into EWRAM"."""
    m = _ILLEGAL_RE.search(stderr_text or "")
    return int(m.group(1)) if m else 0


def _last_reached(log_text: str) -> int | None:
    m = _REACHED_RE.search(log_text or "")
    return int(m.group(1)) if m else None


def _prune_shots(base_shots: Path, pat_shots: Path,
                 diffs: tuple[AnchorDiff, ...]) -> None:
    """Keep only the screenshot pair at the first differing frame.

    The per-frame PPMs are inputs to the diff, not results: 60 frames x 240x160x3 is
    7 MB a side, so a pilot of 48 functions x 4 interventions would leave 2.6 GB of
    identical images behind. The pair the JSON points at is copied out first."""
    screen = next((d for d in diffs if d.kind == "screen" and d.first_frame is not None), None)
    if screen is None:
        for shots in (base_shots, pat_shots):
            for ppm in shots.glob("frame_*.ppm"):
                ppm.unlink()
        return
    keep = f"frame_{int(screen.first_frame):06d}.ppm"
    for shots in (base_shots, pat_shots):
        for ppm in sorted(shots.glob("frame_*.ppm")):
            if ppm.name != keep:
                ppm.unlink()


_DIGEST_RE = re.compile(r"^digest game=(\d+) hw=\d+ (?:ewram=\S+\s+)?pal=(\S+) vram=(\S+) oam=(\S+)")
_WATCH_RE = re.compile(r"^watch game=(\d+) hw=\d+ (.+)$")
_REACHED_RE = re.compile(r"^reached game frame (\d+) at hardware frame (\d+)", re.MULTILINE)


def _stalled_before(log_text: str, want_game_frame: int) -> bool:
    """True if the run never reached `want_game_frame`.

    mgba_run prints `reached game frame N at hardware frame H` only once the
    game-frame counter actually gets there; when an intervention wedges the CPU
    (an infinite loop, or execution that has fallen out of THUMB into ARM) the
    hardware frame budget runs out instead and the line is absent. That absence
    *is* the hang signal — mgba_run's own `stalled:` warning needs a longer
    budget than the observation window, so it never fires here."""
    m = _REACHED_RE.search(log_text)
    if m is None:
        return True
    return int(m.group(1)) < want_game_frame


def _diff_log_lines(base_log: Path, pat_log: Path, base_shots: Path, pat_shots: Path,
                    anchors: tuple[AnchorSpec, ...], frames: int) -> tuple[AnchorDiff, ...]:
    """Read both mgba_run logs; produce one AnchorDiff per anchor."""
    base = _parse_log(base_log.read_text() if base_log.exists() else "")
    pat = _parse_log(pat_log.read_text() if pat_log.exists() else "")
    out: list[AnchorDiff] = []
    for anchor in anchors:
        if anchor.is_screen:
            out.append(_diff_screen(anchor, base_shots, pat_shots))
        elif anchor.is_register_set:
            out.append(_diff_io_set(anchor, base.watch, pat.watch))
        else:
            out.append(_diff_memory_or_fingerprint(anchor, base.digest, pat.digest))
    return tuple(out)


@dataclass(frozen=True)
class _LogKind:
    digest: dict[int, dict[str, str]]          # game frame -> {pal, vram, oam, ewram}
    watch: dict[int, bytes]                    # game frame -> raw bytes from --watch


def _parse_log(text: str) -> _LogKind:
    digest: dict[int, dict[str, str]] = {}
    watch: dict[int, bytes] = {}
    for line in text.splitlines():
        m = _DIGEST_RE.match(line)
        if m:
            f = int(m.group(1))
            digest[f] = {"pal": m.group(2), "vram": m.group(3), "oam": m.group(4)}
            continue
        m = _WATCH_RE.match(line)
        if m:
            f = int(m.group(1))
            try:
                watch[f] = bytes.fromhex(m.group(2).replace(" ", ""))
            except ValueError:
                continue
    return _LogKind(digest=digest, watch=watch)


def _diff_memory_or_fingerprint(anchor: AnchorSpec, base: dict[int, dict[str, str]],
                                pat: dict[int, dict[str, str]]) -> AnchorDiff:
    if anchor.name == "oam":
        return _diff_hash(anchor, "oam", base, pat)
    if anchor.name == "palette":
        return _diff_hash(anchor, "pal", base, pat)
    if anchor.name == "vram_bg" or anchor.name == "vram_obj":
        return _diff_hash(anchor, "vram", base, pat)
    # generic memory: digest doesn't help; only the bool digest says differ or not.
    return _diff_hash(anchor, anchor.name, base, pat)


def _diff_hash(anchor: AnchorSpec, key: str, base: dict[int, dict[str, str]],
                pat: dict[int, dict[str, str]]) -> AnchorDiff:
    """Compare two streams of per-frame digests at the named sub-anchor. The
    observation is per-frame, so the first_frame field is game-frame based."""
    frames = sorted(set(base) | set(pat))
    first = None
    diffs = 0
    for f in frames:
        b = base.get(f)
        p = pat.get(f)
        if b is None or p is None:
            continue
        if b.get(key) != p.get(key):
            diffs += 1
            if first is None:
                first = f
    return AnchorDiff(name=anchor.name, kind=_kind(anchor), first_frame=first,
                       differing_frames=diffs, size=diffs, unit=anchor.unit)


def _diff_io_set(anchor: AnchorSpec, base: dict[int, bytes],
                 pat: dict[int, bytes]) -> AnchorDiff:
    """Compare two streams of --watch raw bytes per game frame. `size` is the
    number of bytes that differed across all frames; the first_frame field is
    the first game frame the bytes differed at."""
    frames = sorted(set(base) | set(pat))
    first = None
    differing = 0
    bytes_diff = 0
    for f in frames:
        b = base.get(f)
        p = pat.get(f)
        if b is None or p is None:
            continue
        # Only the addresses in the anchor's register list (the rest is the watch range).
        for name, addr in anchor.registers:
            off = addr - 0x04000000
            if not 0 <= off < min(len(b), len(p)):
                continue
            block = max(1, 2 if addr % 2 == 0 and addr + 1 < len(b) else 1)
            for k in range(block):
                if b[off + k] != p[off + k]:
                    differing += 1
                    bytes_diff += 1
                    if first is None:
                        first = f
                    break
    return AnchorDiff(name=anchor.name, kind=_kind(anchor), first_frame=first,
                       differing_frames=differing, size=bytes_diff, unit=anchor.unit)


def _diff_screen(anchor: AnchorSpec, base_shots: Path, pat_shots: Path) -> AnchorDiff:
    """Compare two screens-per-frame directories frame_N.ppm. `first_frame` is
    the first HARDWARE frame that differs (mgba_run --shots uses hardware frames
    in filenames; the observe code doesn't decode the game's frame counter into
    filenames in v1)."""
    base_ppms = sorted(base_shots.glob("frame_*.ppm"))
    pat_ppms = sorted(pat_shots.glob("frame_*.ppm"))
    first = None
    diff_count = 0
    pixel_diffs = 0
    for b_path, p_path in zip(base_ppms, pat_ppms):
        try:
            b = _read_ppm_pixels(b_path)
            p = _read_ppm_pixels(p_path)
        except Exception:
            continue
        try:
            f = int(b_path.stem.split("_")[-1])
        except ValueError:
            continue
        if b != p:
            diff_count += 1
            pixel_diffs += sum(1 for x, y in zip(b, p) if x != y)
            if first is None:
                first = f
    return AnchorDiff(name=anchor.name, kind=_kind(anchor), first_frame=first,
                       differing_frames=diff_count, size=pixel_diffs, unit=anchor.unit)


def _read_ppm_pixels(path: Path) -> bytes:
    """Read a P6 PPM (from mgba_run --shots) and return its pixel bytes."""
    data = path.read_bytes()
    # P3 / P6 header: P6\nWxH\n255\n then RGB triples.
    end_header = 0
    nl = 0
    for i, byte in enumerate(data):
        if byte == 0x0A:                                  # \n
            nl += 1
            if nl == 3:
                end_header = i + 1
                break
    return data[end_header:]


def _kind(anchor: AnchorSpec) -> str:
    if anchor.is_screen:
        return "screen"
    if anchor.is_register_set:
        return "register"
    if anchor.block_size:
        return "fingerprint"
    return "memory"


def _diff_bytes(anchor: AnchorSpec, base: bytes, pat: bytes) -> AnchorDiff:
    """Simple byte-wise diff with first-frame and differing count for the fragment size of
    one anchor sample."""
    n = min(len(base), len(pat))
    first = None
    diffs = 0
    sample = max(1, len(base) // max(1, len(base) // (anchor.size or len(base))))
    total_bytes = 0
    for off in range(0, n, sample):
        if base[off: off + sample] != pat[off: off + sample]:
            diffs += 1
            total_bytes += sum(1 for a, b in zip(base[off: off + sample],
                                                 pat[off: off + sample]) if a != b)
            if first is None:
                # frame index: one frame per sample (we sample every `every` frames)
                first = (off // sample) * anchor.every
    return AnchorDiff(name=anchor.name, kind=_kind(anchor), first_frame=first,
                       differing_frames=diffs, size=total_bytes, unit=anchor.unit)


# ── predicates ────────────────────────────────────────────────────────

# A small typed language. Each predicate is a dataclass; `evaluate(pred, run)` returns True/False
# (or None for "indeterminate", used by `oam_entries_missing` when there are no OAM diffs).
@dataclass(frozen=True)
class Differs:
    anchor: str
    within: int | None = None                 # `within=N`: differs in some frame <= N

    def evaluate(self, run: RunRecord) -> bool:
        for diff in run.anchor_diffs:
            if diff.name != self.anchor or diff.differing_frames == 0:
                continue
            if self.within is None:
                return True
            if diff.first_frame is not None and diff.first_frame <= self.within:
                return True
        return False


@dataclass(frozen=True)
class Unchanged:
    anchor: str
    for_: int                                 # `for=N`: no diffs in the first N frames

    def evaluate(self, run: RunRecord) -> bool:
        for diff in run.anchor_diffs:
            if diff.name == self.anchor and diff.differing_frames > 0 and \
               (diff.first_frame or 0) < self.for_:
                return False
        return True


@dataclass(frozen=True)
class OamEntriesMissing:
    min_k: int                                 # `oam_entries_missing(min=k)`: at least k OAM entries
                                               #  differ first in frame >= F (k: integer)
    first_after: int                           # `first_after=F`: differences first appear at frame >= F

    def evaluate(self, run: RunRecord) -> bool | None:
        for diff in run.anchor_diffs:
            if diff.name == "oam":
                if diff.size >= self.min_k and (diff.first_frame or 0) >= self.first_after:
                    return True
                return False
        return None


@dataclass(frozen=True)
class ScreenDiffers:
    region: tuple[int, int, int, int]         # (x, y, w, h)

    def evaluate(self, run: RunRecord) -> bool:
        for diff in run.anchor_diffs:
            if diff.name == "screen" and diff.differing_frames > 0:
                return True
        return False


@dataclass(frozen=True)
class SoundWriteMissing:
    reg: str

    def evaluate(self, run: RunRecord) -> bool:
        # sound_io: register writes missing == the function did not write that
        # sound register. A `differs=False` on sound_io with no differing register at the
        # specified name passes.
        for diff in run.anchor_diffs:
            if diff.name == "sound_io" and diff.differing_frames > 0 and diff.size == 0:
                continue
        for diff in run.anchor_diffs:
            if diff.name == self.reg and diff.differing_frames > 0:
                return False
        return True


@dataclass(frozen=True)
class RamAnchor:
    name: str
    threshold: int                             # `ram_anchor(name) baseline_minus_patched >= k`

    def evaluate(self, run: RunRecord) -> bool:
        for diff in run.anchor_diffs:
            if diff.name == self.name and diff.size >= self.threshold:
                return True
        return False


Predicate = Differs | Unchanged | OamEntriesMissing | ScreenDiffers | SoundWriteMissing | RamAnchor


def make_predicate(spec: str) -> Predicate:
    """Parse a small subset of `tools/wf/wf/tools/intervene.py`'s predicate DSL.
    Examples:
        differs(anchor=oam, within=60)
        unchanged(anchor=palette, for=120)
        oam_entries_missing(min=4, first_after=10)
        screen_differs(region=0,0,240,160)
        sound_write_missing(reg=SOUNDCNT_X)
        ram_anchor(name=oBattleState_HP) baseline_minus_patched >= 16
    """
    s = spec.strip()
    # differs(...)
    m = re.fullmatch(r"differs\s*\(\s*anchor\s*=\s*(\w+)(?:\s*,\s*within\s*=\s*(\d+))?\s*\)", s)
    if m:
        return Differs(anchor=m.group(1), within=int(m.group(2)) if m.group(2) else None)
    m = re.fullmatch(r"unchanged\s*\(\s*anchor\s*=\s*(\w+)\s*,\s*for\s*=\s*(\d+)\s*\)", s)
    if m:
        return Unchanged(anchor=m.group(1), for_=int(m.group(2)))
    m = re.fullmatch(r"oam_entries_missing\s*\(\s*min\s*=\s*(\d+)\s*,\s*first_after\s*=\s*(\d+)\s*\)", s)
    if m:
        return OamEntriesMissing(min_k=int(m.group(1)), first_after=int(m.group(2)))
    m = re.fullmatch(r"screen_differs\s*\(\s*region\s*=\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)", s)
    if m:
        return ScreenDiffers(region=(int(m.group(1)), int(m.group(2)),
                                     int(m.group(3)), int(m.group(4))))
    m = re.fullmatch(r"sound_write_missing\s*\(\s*reg\s*=\s*(\w+)\s*\)", s)
    if m:
        return SoundWriteMissing(reg=m.group(1))
    m = re.fullmatch(r"ram_anchor\s*\(\s*name\s*=\s*(\w+)\s*\)\s*baseline_minus_patched\s*>=\s*(\d+)", s)
    if m:
        return RamAnchor(name=m.group(1), threshold=int(m.group(2)))
    raise ValueError(f"unknown predicate: {spec!r}")


def evaluate_predicate(pred: Predicate, run: RunRecord) -> bool | None:
    """True / False / None (indeterminate). Same name semantics as the predicate DSL."""
    return pred.evaluate(run)


# ── per-run summary ──────────────────────────────────────────────────

def summarise_run(record: RunRecord, *, deterministic: bool, hung: bool,
                  first_frame_executed: int | None) -> RunRecord:
    """Stamp determinism / hung / first_executed into a copy of `record` and return it.
    The graph's write step uses this to produce the per-file JSON."""
    return record.model_copy(update={"deterministic": deterministic, "hung": hung,
                                     "first_frame_executed": first_frame_executed})


# ── end-to-end one (function, intervention, route) ───────────────────

def run_intervene_one(*, function: FunctionInfo, intervention: str, k: int | None,
                      route: Route, frames: int, mgba_run: Path, bios: Path,
                      out_dir: Path, anchors: tuple[AnchorSpec, ...],
                      lead_in_frames: int = LEAD_IN_FRAMES,
                      frames_window: int | None = None,
                      first_frame: int | None = None) -> RunRecord:
    """End-to-end intervention for one (function, intervention, route): find the first
    frame the function executes on the route, save an emulator state `lead_in_frames`
    before it, replay baseline and patched from that state, observe anchors, summarise.

    `first_frame` lets a caller (the pilot) search once per function and reuse the
    answer for all four interventions and the route check; `None` searches here.

    The patched ROM goes to a temp directory that is deleted when the run returns,
    patched or not: build/intervene/patched-roms reached 945 MB because every one of
    the pilot's runs kept its own 8 MB copy forever. Writes a JSON file to
    `out_dir/NAME_ADDR_KIND[_k]_ROUTE/result.json` and side-files for shots."""
    out_dir.mkdir(parents=True, exist_ok=True)
    if intervention not in {"skip", "return", "null"}:
        raise ValueError(f"unsupported intervention {intervention}")
    if first_frame is None:
        first_frame = first_game_frame(function, route, lead_in_frames=lead_in_frames,
                                       mgba_run=mgba_run, bios=bios,
                                       frames_window=frames_window)
    label = f"{function.name}_{function.address:08x}_{intervention}{f'_{k}' if k is not None else ''}_{route.name}"
    if first_frame is None:
        return RunRecord(function=function.name, address=function.address,
                         intervention=intervention + (f":{k}" if k is not None else ""),
                         intervention_k=k, route=route.name,
                         rom_hashes=("", ""),
                         start_frame=0, lead_in_frames=lead_in_frames,
                         first_frame_executed=None, frames_observed=frames,
                         deterministic=None, hung=False,
                         notes=(ToolMessage(f"{function.name} never executes on "
                                            f"{route.name} (searched the whole "
                                            f"{int(route.length)}-frame route)"),),
                         anchor_diffs=(), shot_baseline=None, shot_patched=None,
                         shot_diff=None, artifacts_dir=str(out_dir / label))
    start = max(1, first_frame - lead_in_frames)
    # Snap down to the cached grid: the state at `first_frame - lead_in` usually does
    # not exist, and saving it would mean replaying the route from its first frame. The
    # cached state at or before it is at most one stride earlier, which is more
    # head-room, not less.
    snapped = nearest_cached_state(route, start)
    if snapped is not None:
        start = snapped
    state_path = state_path_at(route, start)
    if not state_path.exists() or state_path.stat().st_size == 0:
        save_state_at(route, at_frame=start, mgba_run=mgba_run, bios=bios, out=state_path)
    rom_dir = Path(tempfile.mkdtemp(prefix=f"{label[:60]}-", dir=str(PATCHED_DIR)))
    try:
        patched = patch_rom(_origin_rom(), function, intervention, k, rom_dir)
        patch_hex = patched_bytes_hex(function, intervention, k, patched)
        observation = observe(function=function, intervention=intervention, k=k,
                              route=route, state_path=state_path, frames=frames,
                              baseline_rom=_origin_rom(), patched_rom=patched,
                              anchors=anchors, mgba_run=mgba_run, bios=bios,
                              out_dir=out_dir)
        patched_hash = _short_hash(patched)      # before the temp copy goes away
    finally:
        shutil.rmtree(rom_dir, ignore_errors=True)         # the temporary patched ROM, always
    diffs = observation.anchor_diffs
    base_hung = observation.baseline.hung
    pat_hung = observation.patched.hung
    # Determinism is a property of the *route/window*, so only a hung baseline
    # invalidates it. A hung patched run is the intervention's own doing -- a real
    # causal result (the patch wedged the game), not a measurement failure.
    deterministic = (not _any_diffs(diffs)) and not base_hung
    hung = base_hung or pat_hung
    artefacts = out_dir / label
    first_diff = next((d for d in diffs if d.differing_frames and d.first_frame is not None), None)
    sb, sp, sd = _best_shot_pair(artefacts, first_diff)
    # Notes live on the record so report.tsv rows never inherit another run's verdict.
    notes: list[ToolMessage] = list(observation.notes())
    notes.insert(0, ToolMessage(f"{function.name} first executes at game frame {first_frame}; "
                                f"replayed from the cached state at frame {start}"))
    if first_diff is not None:
        notes.append(ToolMessage(f"first anchor difference: {first_diff.name} at "
                                 f"game frame {first_diff.first_frame}"))
    elif not hung:
        notes.append(ToolMessage("no anchor difference over the window"))
    rec = RunRecord(function=function.name, address=function.address,
                    intervention=intervention + (f":{k}" if k is not None else ""),
                    intervention_k=k, route=route.name,
                    rom_hashes=(_short_hash(_origin_rom()), patched_hash),
                    patch_bytes=patch_hex,
                    start_frame=start, lead_in_frames=lead_in_frames,
                    first_frame_executed=first_frame, frames_observed=frames,
                    deterministic=deterministic, hung=hung,
                    hung_side=observation.hung_side,
                    illegal_opcodes=(observation.baseline.illegal_opcodes,
                                     observation.patched.illegal_opcodes),
                    notes=tuple(notes),
                    anchor_diffs=diffs,
                    shot_baseline=sb, shot_patched=sp, shot_diff=sd,
                    artifacts_dir=str(artefacts))
    json_path = artefacts / "result.json"
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(rec.model_dump_json(indent=2))
    return rec


def patched_bytes_hex(function: FunctionInfo, intervention: str, k: int | None,
                      patched_rom: Path) -> str:
    """The bytes the intervention wrote at the entry point, as hex. The record has to
    carry it: the crash this fixes was a difference in *how* the function returns, and
    "skipped" alone does not say which of two encodings was used."""
    try:
        info = _load_b6f_info()
        offset = info.rom_offset(function.address)
        data = patched_rom.read_bytes()
        if offset is None:
            return ""
        if intervention == "null":
            skip_pay, _kind = skip_bytes_for(function)
            n = len(skip_pay)
        elif intervention == "skip":
            skip_pay, _kind = skip_bytes_for(function)
            n = len(skip_pay)
        elif intervention == "return":
            pay, _kind = return_k_bytes_for(k or 0, function)
            n = len(pay)
        else:
            n = 0
        return data[offset: offset + n].hex()
    except (OSError, ValueError):
        return ""


def _best_shot_pair(artefacts: Path, first_diff: AnchorDiff | None) -> tuple[str | None, str | None, str | None]:
    """Pick a baseline/patched pair (and a diff image) to report. v1 emits the file names
    only; the actuals live next to result.json. We don't generate an explicit visual
    diff in v1 (mgba_run's --shots per-frame is enough; the JSON records which frames)."""
    sb = artefacts / "baseline" / "shots"
    sp = artefacts / "patched" / "shots"
    return (str(sb) if sb.exists() else None,
            str(sp) if sp.exists() else None,
            None)


def _any_diffs(diffs: Sequence[AnchorDiff]) -> bool:
    return any(d.differing_frames > 0 for d in diffs)


def _short_hash(path: Path) -> str:
    if not path.exists():
        return ""
    import hashlib
    h = hashlib.sha1()
    h.update(path.read_bytes())
    return h.hexdigest()[:16]
