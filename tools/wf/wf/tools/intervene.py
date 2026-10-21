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

  - select_intervene_targets(...)  from .prof counts on tools/traces/coverage
  - find_function(name_or_addr)    bn6f.elf via tools/lifter/elfinfo.py
  - patch_rom(...)                 reads orig ROM, returns a temp patched copy
  - first_game_frame(...)          bisect mgba_run to find the first frame the
                                   function executes on a route
  - observe(...)                   run baseline + patched from the same state
                                   with the same trace, observe anchors
  - anchors_cfg(...)               read symbols/anchors.yaml
  - make_predicate(...)            a small typed language over the JSON output
  - intervene_result(...)          fold the observations into the result JSON
  - run_intervene_one(...)         the end-to-end: target, frame, observe, JSON

Pilot: `wf intervene-pilot` (graphs/intervene.py) picks 12 named, frequent
functions and 12 random ones, runs null + skip on each, prints a table.

Patched ROMs live under build/intervene/, never bn6f/. Output JSON per
(function, intervention, route) under build/intervene/RUN/.
"""
from __future__ import annotations

import bisect
import io
import os
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
STATES_DIR = DIR / "states"                              # cached emulator states
PATCHED_DIR = DIR / "patched-roms"                       # patched copies of bn6f.gba (never the original)
DEFAULT_FRAMES = 120                                     # the brief's default window length
PATCH_HEAD_MIN = 4                                       # refuse functions shorter than this in bytes
DEFAULT_FRAME_TARGETS = (8, 16, 32)                      # candidate frame counts to observe at
EVERY_ANCHORS_DEFAULT = 1                                # frame stride for anchors (1 = every frame)
PREDICATE_TIMEOUT = 30                                   # mgba_run per observation, seconds
PREDICATE_STATE_TIMEOUT = 30                             # --save-state per observation, seconds

# Bytes
SKIP_BYTES = b"\x70\x47"                                # THUMB `bx lr` (0x4770, little-endian)
# `movs r0, #K ; bx lr` — THUMB `movs r0, #(K<<0)` is 0x2000|K (LSB set = register, 8-bit imm).
# `bx lr` is 0x4770; together they form a 4-byte thumbnail patch (encoded `0x00 0x20 0x70 0x47`).
def return_k_bytes(k: int) -> bytes:
    if not 0 <= k <= 255:
        raise ValueError(f"return k must be 0..255 (got {k})")
    return bytes([0x00 | k, 0x20, 0x70, 0x47])           # little-endian: low byte, second byte, ...


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
    start_frame: int                       # game frame the run started from (cached state was at start_frame - lead_in)
    lead_in_frames: int                    # how many game frames before the first frame the function executed
    first_frame_executed: int | None       # first game frame the function executed in baseline
    frames_observed: int
    deterministic: bool | None = None      # null run produced no diff; or no null was run (None = not tested)
    hung: bool                             # no new game frame in 300 hardware frames
    notes: tuple[ToolMessage, ...]
    anchor_diffs: tuple[AnchorDiff, ...]   # one per anchor (sorted by name)
    shot_baseline: str | None
    shot_patched: str | None
    shot_diff: str | None
    artifacts_dir: str                     # where the .png + .ppm + .json live


# ── targets ───────────────────────────────────────────────────────────

# 12 named, frequently executed thumb functions whose visible effect is obvious from their
# name: drawing, palette fades, OAM, sound, sprite. The pilot pulls 12 anchors from here.
# These are REAL bn6f.elf symbols (current `nm`), not model-proposed placeholders.
PILOT_NAMED: tuple[str, ...] = (
    "LoadGFXAnims",
    "call_m4aSoundMain",
    "PlaySoundEffect",
    "PlayMusic",
    "LoadBGAnimData",
    "LoadBGAnimForMapGroup",
    "LoadGFXAnim",
    "screenFade_80062C8",
    "SetScreenFade",
    "IsScreenFadeActive",
    "palette_copyTableRow_80AA104",
    "owPlayer_setPalette_809e2a0",
)


def select_intervene_targets(count: int = 12, *,
                              named: Sequence[str] = PILOT_NAMED,
                              only: Sequence[str] = (),
                              rng_seed: int = 0) -> tuple[FunctionName, ...]:
    """The functions an intervene pilot watches. `named` are the obvious-effect
    ones (drawn from the names `wf investigate` proposed for drawing / palette / OAM
    / sound / sprite routes). `only`, if set, narrows to those names regardless."""
    rng = random.Random(rng_seed)
    pool: list[str] = list(named)
    if only:
        wanted = [name for name in only if name in pool]
        return tuple(wanted)
    chosen = list(named[: min(len(named), count)])
    if len(chosen) < count:
        # Add some lifted units from the executed set with a name that hints at
        # drawing/OAM/palette/fade/sound/sprite (heuristic for the pilot).
        extras = _lucky_extras(pool, count - len(chosen), rng)
        chosen.extend(extras)
    return tuple(chosen)


def _lucky_extras(pool: list[str], want: int, rng: random.Random) -> list[str]:
    """A handful of names from the lifted set that match the obvious-effect keywords,
    filtered down to functions `wf intervene` can actually patch.

    The lifted set (`funcindex.unit_ranges()`) is keyed by *unit* names, which are a
    superset of the bn6f ELF's symbols — a unit may cover an inlined or unnamed region.
    `find_function` refuses RAM-resident, ARM-mode or too-short targets, so filter here
    rather than let the pilot burn a slot on a row that can only ever say `unsupported`."""
    hints = ("Draw", "OAM", "Pal", "Fade", "Sound", "Sprite", "Anim", "Battle", "Chip")
    try:
        ranges = funcindex.unit_ranges()
    except FileNotFoundError:
        return []
    candidates = [name for name in sorted(ranges) if any(h in name for h in hints)]
    rng.shuffle(candidates)
    chosen: list[str] = []
    for pick in candidates:
        if len(chosen) >= want:
            break
        if pick in pool:
            continue
        try:
            find_function(pick)
        except ValueError:
            continue
        chosen.append(pick)
        pool.append(pick)
    return chosen


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


def pick_route(function: FunctionInfo, *, max_window: int = 400,
                routes: Sequence[Route] | None = None) -> Route | None:
    """The shortest recorded route on which `function` is called within `max_window`
    game frames. Tries each route in length order, runs `first_game_frame`, returning
    the first hit. A None means: no route in the default set executed it within
    `max_window`; the caller can re-pick with a larger window. (This intentionally
    re-uses `first_game_frame` rather than a separate lookup; the cheapest per-test
    cost is one bisect, and keeping the route-hunt logic with the bisect keeps them
    in sync.)"""
    from wf.routes import ALL_ROUTES                       # noqa: PLC0415
    candidates = sorted(routes or ALL_ROUTES, key=lambda r: int(r.length))
    mgba_run = _resolve_mgba_run()
    bios = _resolve_game_bios()
    for route in candidates:
        try:
            ff = first_game_frame(function, route, lead_in_frames=4,
                                  mgba_run=mgba_run, bios=bios,
                                  frames_window=max_window)
        except Exception:
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
    function.address. The returned slice is the same length."""
    name = intervention
    if name == "skip":
        payload = SKIP_BYTES
    elif name == "return":
        if k is None:
            raise ValueError("return:K needs K")
        payload = return_k_bytes(k)
    elif name == "null":
        payload = baseline_bytes[: len(SKIP_BYTES)]      # determinism control: re-write its own bytes
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
    head = data[offset: offset + max(PATCH_HEAD_MIN, 4)]
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


# ── game-frame search ─────────────────────────────────────────────────

def first_game_frame(function: FunctionInfo, route: Route, *,
                     lead_in_frames: int = 4,
                     mgba_run: Path,
                     bios: Path,
                     frames_window: int = 1200) -> int | None:
    """The first game frame the route executes `function`. Bisects over frames
    with mgba_run --count-entry (heuristic; falls back to --until-game-frame
    + --trace-pc when --count-entry is missing). Returns None if no execution
    in the window. The hit frame is what `wf intervene` runs the patched
    ROM forward from (with `lead_in_frames` saved before it).

    `frames_window` bounds the search. 1200 is the default because most of a
    route's interesting behaviour is inside the first ~20 seconds of play, and
    the bisect costs ~log2(window) mgba_run invocations (13 s at 1200 on a
    2000-frame route). Pass a smaller window to answer "does this run early?"
    cheaply; a None here means "not within the window", never "never"."""
    # Fast path: --count-entry. Loads the trace, runs up to `until`, prints the top hits
    # (counted from the start). We bisect until the count is > 0 (the function executed
    # within the window).
    lo, hi = 1, max(2, min(int(route.length), frames_window))
    while lo < hi:
        mid = (lo + hi) // 2
        try:
            n = _count_entry(function.address, route, mid, mgba_run=mgba_run, bios=bios)
        except subprocess.TimeoutExpired:
            hi = mid
            continue
        if n > 0:
            hi = mid
        else:
            lo = mid + 1
    n_final = _count_entry(function.address, route, lo, mgba_run=mgba_run, bios=bios)
    return lo if n_final > 0 else None


def _count_entry(address: GuestAddress, route: Route, until: int, *,
                 mgba_run: Path, bios: Path) -> int:
    """Run mgba_run --count-entry up to game frame `until`; return the hit count.
    `--profile-game 0 until` makes mgba_run step inside the window (otherwise the
    count histogram is never dumped). The trace replay follows the same input."""
    import gamepaths  # noqa: PLC0415
    sav = Path(gamepaths.TRACES) / f"{route.name}.sav"
    save_arg = ["--save", str(sav)] if sav.exists() else []
    cmd = [str(mgba_run), str(_origin_rom()),
           "--quiet", "--frames", "200000",
           "--bios", str(bios),
           "--input", str(route.trace),
           "--game-frame-addr", _game_frame_addr(),
           "--profile-game", "0", str(until),
           "--count-entry", str(int(address))]
    if sav.exists():
        cmd[3:3] = ["--save", str(sav)]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=PREDICATE_TIMEOUT).stdout
    except subprocess.TimeoutExpired:
        return 0
    total = 0
    for line in out.splitlines():
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


# ── state save / load ─────────────────────────────────────────────────

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


def observe(*, function: FunctionInfo, intervention: str, k: int | None,
            route: Route, state_path: Path,
            frames: int, baseline_rom: Path, patched_rom: Path,
            anchors: tuple[AnchorSpec, ...],
            mgba_run: Path, bios: Path,
            out_dir: Path) -> tuple[tuple[AnchorDiff, ...], bool, bool]:
    """Replay `frames` game frames from `state_path` on baseline_rom AND patched_rom,
    capturing per-frame anchor data, and return (per-anchor diffs, base_hung, pat_hung).
    `base_hung`/`pat_hung` are True if the run timed out or its game frame counter
    stopped advancing (mgba_run's 'stalled:' stderr line)."""
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
    base_hung = _observe_one(rom=baseline_rom, route=route, state_path=state_path,
                              start_frame=start_frame,
                              frames=frames, anchors=anchors,
                              log=base_log, shots_dir=base_shots,
                              mgba_run=mgba_run, bios=bios)
    pat_hung = _observe_one(rom=patched_rom, route=route, state_path=state_path,
                              start_frame=start_frame,
                              frames=frames, anchors=anchors,
                              log=pat_log, shots_dir=pat_shots,
                              mgba_run=mgba_run, bios=bios)
    diffs = _diff_log_lines(base_log, pat_log, base_shots, pat_shots, anchors, frames)
    return diffs, base_hung, pat_hung


def _observe_one(*, rom: Path, route: Route, state_path: Path, start_frame: int,
                 frames: int, anchors: tuple[AnchorSpec, ...],
                 log: Path, shots_dir: Path,
                 mgba_run: Path, bios: Path) -> bool:
    """Run mgba_run with state + trace + per-frame observation flags. Writes stdout
    to `log`. Saves shots to `shots_dir`. Returns True if the run was hung (timed out
    or mgba_run wrote a 'stalled:' line on stderr)."""
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
        log.with_suffix(".stderr").write_text(error.stderr or "")
        return True                                # hung
    log.write_text(proc.stdout)
    stderr_text = proc.stderr
    log.with_suffix(".stderr").write_text(stderr_text)
    if "stalled:" in stderr_text:
        return True
    return _stalled_before(proc.stdout, want_game_frame)


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
                      lead_in_frames: int = 4,
                      frames_window: int = 1200) -> RunRecord:
    """End-to-end intervention for one (function, intervention, route): patch the ROM,
    find the first frame the function executes on the route, save an emulator state at
    (frame - lead_in), replay both ROMs, observe anchors, summarise. Writes a JSON file
    to `out_dir/<name>_<addr>_<kind>[_k]_<route>.json` and side-files for shots."""
    out_dir.mkdir(parents=True, exist_ok=True)
    if intervention not in {"skip", "return", "null"}:
        raise ValueError(f"unsupported intervention {intervention}")
    patched = patch_rom(_origin_rom(), function, intervention, k, PATCHED_DIR)
    f0 = first_game_frame(function, route, lead_in_frames=lead_in_frames,
                          mgba_run=mgba_run, bios=bios, frames_window=frames_window)
    if f0 is None:
        return RunRecord(function=function.name, address=function.address,
                         intervention=intervention + (f":{k}" if k is not None else ""),
                         intervention_k=k, route=route.name,
                         rom_hashes=("", ""),
                         start_frame=0, lead_in_frames=lead_in_frames,
                         first_frame_executed=None, frames_observed=frames,
                         deterministic=None, hung=False,
                         notes=(ToolMessage(f"{function.name} does not execute on {route.name} "
                                            f"within the first {frames_window} game frames"),),
                         anchor_diffs=(), shot_baseline=None, shot_patched=None,
                         shot_diff=None, artifacts_dir=str(out_dir))
    start = max(1, f0 - lead_in_frames)
    state_path = STATES_DIR / f"{route.name}_{start:06d}.state"
    state_path.parent.mkdir(parents=True, exist_ok=True)
    if not state_path.exists() or state_path.stat().st_size == 0:
        save_state_at(route, at_frame=start, mgba_run=mgba_run, bios=bios, out=state_path)
    diffs, base_hung, pat_hung = observe(function=function, intervention=intervention, k=k,
                                          route=route, state_path=state_path, frames=frames,
                                          baseline_rom=_origin_rom(), patched_rom=patched,
                                          anchors=anchors, mgba_run=mgba_run, bios=bios,
                                          out_dir=out_dir)
    # Determinism is a property of the *route/window*, so only a hung baseline
    # invalidates it. A hung patched run is the intervention's own doing — a real
    # causal result (the patch wedged the game), not a measurement failure.
    deterministic = (not _any_diffs(diffs)) and not base_hung
    hung = base_hung or pat_hung
    artefacts = out_dir / f"{function.name}_{function.address:08x}_{intervention}{f'_{k}' if k is not None else ''}_{route.name}"
    first_diff = next((d for d in diffs if d.differing_frames and d.first_frame is not None), None)
    sb, sp, sd = _best_shot_pair(artefacts, first_diff)
    # Notes live on the record so report.tsv rows never inherit another run's verdict.
    notes: list[ToolMessage] = [
        ToolMessage(f"{function.name} first executes at game frame {f0}; replayed from "
                    f"the cached state at frame {start}"),
    ]
    if base_hung:
        notes.append(ToolMessage("baseline hung: the route/window is not sound, "
                                 "this run proves nothing"))
    if pat_hung:
        notes.append(ToolMessage("patched run hung (no further game frame): the "
                                 "intervention wedged the game"))
    if first_diff is not None:
        notes.append(ToolMessage(f"first anchor difference: {first_diff.name} at "
                                 f"game frame {first_diff.first_frame}"))
    elif not hung:
        notes.append(ToolMessage("no anchor difference over the window"))
    rec = RunRecord(function=function.name, address=function.address,
                    intervention=intervention + (f":{k}" if k is not None else ""),
                    intervention_k=k, route=route.name,
                    rom_hashes=(_short_hash(_origin_rom()), _short_hash(patched)),
                    start_frame=start, lead_in_frames=lead_in_frames,
                    first_frame_executed=f0, frames_observed=frames,
                    deterministic=deterministic, hung=hung, notes=tuple(notes),
                    anchor_diffs=diffs,
                    shot_baseline=sb, shot_patched=sp, shot_diff=sd,
                    artifacts_dir=str(artefacts))
    json_path = artefacts / "result.json"
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(rec.model_dump_json(indent=2))
    return rec


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
