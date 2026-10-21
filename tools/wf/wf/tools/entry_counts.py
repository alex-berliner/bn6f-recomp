"""How often each function is entered, per route -- a real entry count, not the profile's cycles.

`.prof` records CYCLES per executed PC, which says how long a function ran and nothing about
how often it was called: a three-cycle helper called 600 times a frame and a 600-cycle one
called three times sum to the same. `.edges` has real counts, but only for the indirect call
sites the ABI export instruments, so a function called with `bl` has no count at all. A claim
like `called_every_frame(yes)` is about exactly this number, so it has to come from somewhere
real.

`tools/mgba_run --count-entries FILE OUT [G0 [G1]]` is the somewhere: it steps the game
instruction by instruction over the game-frame window [G0, G1) and counts every entry into
every address in FILE (no cycle attribution, ~4 ms per frame). The routes are replayed in
windows the way `tools/import_route.py` profiles them -- from power-on for the first window,
from the state saved at the end of the previous window after that, which keeps the whole route
at a cost of (windows x WINDOW / 2) frames of emulation rather than the route's full length.

The output is a directory of `ADDRESS<TAB>COUNT` lines, one file per route:

    WF_COUNTS_DIR=build/counts python -m wf graphs entry-counts --jobs 12

`facts` reads those as a per-function lookup; a route nobody counted has no counts, which is
None rather than 0 (a count nobody measured is not a count of zero).
"""
from __future__ import annotations

import importlib.util
import os
import subprocess
import time
from collections.abc import Callable, Sequence
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from pathlib import Path
from typing import NamedTuple

from wf.repo import TRACES
from wf.types import Result, RouteName, ToolMessage
from wf.tools import runtime_evidence as rt

import funcindex  # noqa: E402  # tools/funcindex.py: the lifted units (addresses are the keys here)

WINDOW = 10_000               # game frames per mGBA pass; 10,000 is ~45 s, well inside TIMEOUT
BUDGET = 3 * WINDOW           # hardware frames one pass may run for (a window starts mid-frame)
TIMEOUT = 1_800               # one pass, seconds
DEFAULT_JOBS = 12             # routes counted at once (--jobs); each holds one mGBA core
#: one pass is given every lifted entry address (13,690 of them, well inside the runner's own
#: 65,536-entry table), so a window is one pass no matter how many functions the run covers
MAX_ADDRESSES = 65_000


def counts_dir() -> Path:
    """Where the per-route entry counts live (`WF_COUNTS_DIR` moves it; build/counts by
    default -- next to the facts it feeds, and never committed)."""
    return Path(os.environ.get("WF_COUNTS_DIR") or Path(__file__).resolve().parents[4]
                / "build" / "counts")


def trace_of(route: str) -> Path | None:
    """The mGBA input trace a route replays from (`tools/traces/ROUTE.trace`), or None when
    the route has no recording."""
    path = TRACES / f"{route}.trace"
    return path if path.exists() else None


def routes_to_count() -> tuple[RouteName, ...]:
    """The recorded routes a count can be made for, longest first: they are counted in that
    order so the run is bounded by the longest route rather than by the sum of all of them."""
    lengths = _lengths()
    routes = [RouteName(path.stem) for path in sorted((TRACES).glob("*.trace"))]
    return tuple(sorted(routes, key=lambda route: -lengths.get(str(route), 0)))


def _lengths() -> dict[str, int]:
    """Every recorded route's length in game frames (`wf/routes.py`, read at run time)."""
    from wf.routes import ALL_ROUTES, LONG_ROUTES     # noqa: PLC0415  (read at run time)
    return {str(route.name): int(route.length) for route in (*ALL_ROUTES, *LONG_ROUTES)}


def entries_of(units: dict[str, funcindex.Unit]) -> tuple[int, ...]:
    """Every lifted unit's entry address: what `--count-entries` is asked about. The Thumb bit
    is dropped (the runner masks it off too), so a unit at an odd `lo` is counted once."""
    return tuple(sorted({unit.lo & ~1 for unit in units.values()}))


def all_entries() -> tuple[int, ...]:
    """Every lifted function's entry address, from `tools/funcindex.py`."""
    return entries_of(funcindex.lifted_units())


def state_path(route: str, frame: int, out_dir: Path) -> Path:
    """The state a window of ROUTE starts from.

    `tools/import_route.py` saves a checkpoint every 10,000 game frames of a long route
    (gamepaths.CHECKPOINTS); where they exist they are used, because the route's committed
    `.edges` were observed through exactly those checkpoints and a window resumed from one is
    the machine the coverage saw. Where they do not, each pass saves its own state into the
    counts directory, and the next window resumes from that."""
    external = _checkpoints(route) / f"{route}_{frame:07d}.state"
    return external if external.exists() else out_dir / route / f"{route}_{frame:07d}.state"


def _checkpoints(route: str) -> Path:
    """Where `tools/import_route.py` put ROUTE's checkpoints (an empty path when it never made
    any, so nothing is looked up)."""
    import gamepaths                     # noqa: PLC0415  (tools/gamepaths.py)
    folder = Path(str(getattr(gamepaths, "CHECKPOINTS", ""))) / route
    return folder if folder.is_dir() else Path("/nonexistent")


def windows_of(length: int) -> tuple[tuple[int, int], ...]:
    """ROUTE's LENGTH game frames as (first, frames) windows of at most WINDOW frames."""
    return tuple((first, min(WINDOW, length - first))
                 for first in range(0, length, WINDOW))


def replay_args(route: str, trace: Path, first: int, counts: Path, out: Path, frames: int,
                out_dir: Path, save_next: int | None) -> list[str]:
    """The `tools/mgba_run` arguments that count entries over the game-frame window
    [FIRST, FIRST + FRAMES) of ROUTE.

    Same shape as `tools/import_route.py`'s `replay_args` + `observe_window`: the trace is the
    `--input`, the game's frame counter is the `--game-frame-addr`, a trace's `.poke` sidecar is
    passed as `--poke-at` (mgba_run skips the writes before `--resume-game-frame`, which are
    already in the state), and a window after the first resumes from the state the previous
    window saved."""
    args = [str(rt.find_mgba_run()), str(rt._game_rom()), "--quiet",        # noqa: SLF001
            "--bios", str(rt._game_bios()),                                  # noqa: SLF001
            "--input", str(trace), "--game-frame-addr", str(_game_frame_addr())]
    args += _poke_args(trace)
    args += ["--frames", str(BUDGET)]
    if first:
        args += ["--load-state", str(state_path(route, first, out_dir)),
                 "--resume-game-frame", str(first)]
    args += ["--count-entries", str(counts), str(out), str(first), str(first + frames)]
    if save_next is not None:
        args += ["--save-state", str(state_path(route, save_next, out_dir))]
    return args


def _game_frame_addr() -> int:
    """The address of the game's frame counter (`tools/gamepaths.py`; the routes' digests and
    checkpoints are all counted against this one)."""
    import gamepaths     # noqa: PLC0415  (tools/gamepaths.py: reads tools/paths.env)
    return int(gamepaths.GAME_FRAME_ADDR, 0)


def _poke_args(trace: Path) -> list[str]:
    """A trace's live-write sidecar as mgba_run arguments.

    `tools/abi/call_edges.py` is the one place that knows the sidecar's format ("GAME_FRAME
    ADDR=VALUE/SIZE,..." plus the address the writes land at, read out of the original ELF's
    `main_awaitFrame`), so it is loaded from its path rather than reimplemented -- a route
    without it plays a different game: `battle`'s single poke at game frame 16100 is what
    starts the battle, and a count taken without it misses the whole battle. A sidecar that
    cannot be read is an error, never a silent empty list."""
    if not trace.with_suffix(".poke").exists():
        return []
    module = _call_edges()
    if module is None:
        raise RuntimeError(f"{trace.with_suffix('.poke')} exists but tools/abi/call_edges.py "
                           f"could not be loaded, so this window would replay a different game")
    return list(module.poke_args(str(trace)))


def _call_edges() -> object | None:
    """`tools/abi/call_edges.py` loaded by path (wf.repo puts `tools/` on the path, not
    `tools/abi/`), or None when it is not there."""
    path = Path(__file__).resolve().parents[4] / "tools" / "abi" / "call_edges.py"
    if not path.exists():
        return None
    spec = importlib.util.spec_from_file_location("wf_call_edges", path)
    if spec is None or spec.loader is None:
        return None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _read_counts(path: Path) -> dict[int, int]:
    """One pass's counts as {address: count}. The runner writes `ADDR COUNT` (space) and this
    module's own merge writes `ADDR\\tCOUNT`; both are read here, and `#` header lines are
    skipped."""
    out: dict[int, int] = {}
    for line in path.read_text().splitlines():
        fields = line.strip().replace("\t", " ").split()
        if len(fields) >= 2 and not fields[0].startswith("#"):
            out[int(fields[0], 16)] = int(fields[1])
    return out


def _write_listing(entries: Sequence[int], path: Path) -> Path:
    """The address list every pass is given (`--count-entries FILE`): one hex address a line,
    as the runner reads it."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(f"{address:08X}\n" for address in entries))
    return path


def _count_window(route: str, trace: Path, first: int, frames: int, listing: Path,
                  out_dir: Path, save_next: int | None) -> dict[int, int]:
    """Count entries over the game-frame window [FIRST, FIRST + FRAMES) of ROUTE, writing the
    pass's own counts under the route's counts directory. The pass steps the game either way,
    so every address is counted in every window: an address that already has a count costs
    nothing extra, and dropping the ones that came back at zero would lose the calls a
    function makes later in the route."""
    out = out_dir / route / f"window_{first:07d}.tsv"
    args = replay_args(route, trace, first, listing, out, frames, out_dir, save_next)
    done = subprocess.run(args, capture_output=True, text=True, check=False, timeout=TIMEOUT)
    if done.returncode != 0 or not out.exists():
        raise RuntimeError(f"mgba_run --count-entries failed on {route} frames "
                           f"{first}..{first + frames}: {done.stderr.strip()[-300:]}")
    counts = _read_counts(out)
    out.unlink(missing_ok=True)
    return counts


def count_route(route: RouteName, entries: Sequence[int], length: int, out_dir: Path,
                progress: Path | None = None) -> Path:
    """Count every entry into ENTRIES over ROUTE's LENGTH game frames, one window at a time,
    each resuming from the state the previous one saved; write `out_dir/ROUTE.tsv`."""
    trace = trace_of(str(route))
    if trace is None:
        raise FileNotFoundError(f"no recording for route {route} ({TRACES}/{route}.trace)")
    if not entries:
        raise ValueError("no entry addresses to count")
    listing = _write_listing(entries, out_dir / str(route) / "entries.txt")
    totals: dict[int, int] = {}
    for first, frames in windows_of(length):
        totals.update(_count_window(str(route), trace, first, frames, listing, out_dir,
                                    None if first + frames >= length else first + frames))
        if progress is not None:
            progress.write_text(f"{route}: game frames {first}..{first + frames}, "
                                f"{len(totals)} of {len(entries)} entries so far\n")
    return _merge(str(route), totals, out_dir)


def count_route_windows(route: RouteName, entries: Sequence[int], length: int, out_dir: Path,
                         jobs: int = DEFAULT_JOBS) -> Path:
    """Count ROUTE with every window at once, which needs a state at the start of each one:
    a long route's windows are independent given `tools/import_route.py`'s checkpoints
    (`beastsp`'s 44 windows take the time of the slowest one instead of 44 times it)."""
    if not _checkpoints_exist(str(route), length, out_dir):
        return count_route(route, entries, length, out_dir)
    trace = trace_of(str(route))
    listing = _write_listing(entries, out_dir / str(route) / "entries.txt")
    with ThreadPoolExecutor(max_workers=min(jobs, len(windows_of(length)))) as pool:
        parts = list(pool.map(
            lambda window: _count_window(str(route), trace, window[0], window[1], listing,
                                         out_dir, None),
            windows_of(length)))
    totals: dict[int, int] = {}
    for part in parts:
        totals.update(part)
    return _merge(str(route), totals, out_dir)


def _checkpoints_exist(route: str, length: int, out_dir: Path) -> bool:
    """Whether every window but the first can start from a state that is already on disk."""
    return all(state_path(route, first, out_dir).exists()
               for first, _frames in windows_of(length)[1:])


def _merge(route: str, totals: dict[int, int], out_dir: Path) -> Path:
    """The route's counts as `out_dir/ROUTE.tsv`, written through a `.partial` so a reader
    never sees a half-written count."""
    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / f"{route}.tsv"
    partial = out.with_suffix(".partial")
    partial.write_text("".join(f"{address:08X}\t{count}\n"
                               for address, count in sorted(totals.items())))
    partial.replace(out)
    return out


def _count_chained(route: str, trace: Path, windows: Sequence[tuple[int, int]], listing: Path,
                   out_dir: Path, length: int) -> dict[int, int]:
    """One route's windows, each resuming from the state the previous one saved."""
    totals: dict[int, int] = {}
    for first, frames in windows:
        totals.update(_count_window(route, trace, first, frames, listing, out_dir,
                                    None if first + frames >= length else first + frames))
    return totals


class CountsRun(Result):
    """What a counting run produced: the routes, the addresses, the seconds, one line per
    route, and the routes (or windows) that could not be counted."""
    routes: int
    entries: int
    seconds: float
    files: tuple[ToolMessage, ...] = ()
    problems: tuple[ToolMessage, ...] = ()


def count_all(routes: Sequence[RouteName], entries: Sequence[int], jobs: int = DEFAULT_JOBS,
              out_dir: Path | None = None, lengths: dict[str, int] | None = None) -> CountsRun:
    """Count ENTRIES over every route in ROUTES and write one `ROUTE.tsv` per route.

    The work is one job per WINDOW where the windows are independent (a route with a state at
    the start of every window -- `tools/import_route.py`'s checkpoints for the long route --
    is counted in the time of its slowest window rather than 44 times it) and one job for the
    whole route where they are not (each window resuming from the state the previous one
    saved). Every job is an emulator on a core of its own, so the pool is flat: a pool per
    route would start an emulator per core per route. Longest route first, and a route that
    fails (a missing recording, a timeout) is a line in `problems`, not a failed run."""
    out_dir = counts_dir() if out_dir is None else out_dir
    started = time.monotonic()
    lengths = _lengths() if lengths is None else lengths
    order = sorted(routes, key=lambda route: -lengths.get(str(route), 0))
    problems: list[ToolMessage] = []
    totals: dict[str, dict[int, int]] = {}
    work: list[tuple[str, str, Callable[[], dict[int, int]]]] = []
    for route in order:
        name = str(route)
        length = lengths.get(name, 0)
        if not length:
            problems.append(ToolMessage(f"{name}: its length is unknown "
                                        f"(wf/routes.py does not list it)"))
            continue
        trace = trace_of(name)
        if trace is None:
            problems.append(ToolMessage(f"{name}: no recording at {TRACES}/{name}.trace"))
            continue
        totals[name] = {}
        listing = _write_listing(entries, out_dir / name / "entries.txt")
        windows = windows_of(length)
        if _checkpoints_exist(name, length, out_dir):
            for first, frames in windows:
                work.append((name, f"game frames {first}..{first + frames}", partial(
                    _count_window, name, trace, first, frames, listing, out_dir, None)))
        else:
            work.append((name, f"{length} frames", partial(
                _count_chained, name, trace, windows, listing, out_dir, length)))
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [(name, what, pool.submit(job)) for name, what, job in work]
        for name, what, future in futures:
            try:
                totals[name].update(future.result())
            except (OSError, RuntimeError, subprocess.SubprocessError) as problem:
                problems.append(ToolMessage(f"{name}, {what}: {problem}"))
    files: list[ToolMessage] = []
    failed = {problem.split(",")[0].split(":")[0] for problem in problems}
    for name, _what, _future in futures:
        counts = totals[name]
        if name in failed and not (out_dir / f"{name}.tsv").exists():
            continue        # nothing was counted for it and no earlier run left a file
        out = _merge(name, counts, out_dir)
        windows = windows_of(lengths.get(name, 0))
        files.append(ToolMessage(f"{name}: {out.name} "
                                 f"({sum(frames for _first, frames in windows)} frames, "
                                 f"{len(entries)} entries, {len(counts)} entered, "
                                 f"{len(windows)} window(s))"))
    return CountsRun(routes=len(order), entries=len(entries),
                     seconds=round(time.monotonic() - started, 1),
                     files=tuple(files), problems=tuple(problems))
class Counts(NamedTuple):
    """Every counted route's entry counts, as a per-function lookup."""
    by_route: dict[str, dict[int, int]]

    def for_route(self, route: str, address: int) -> int | None:
        """How many times ADDRESS was entered on ROUTE; None when that route was not counted
        (a count nobody measured is not a count of zero)."""
        table = self.by_route.get(route)
        return None if table is None else table.get(address & ~1, 0)


def load_counts(routes: Sequence[str] | None = None, out_dir: Path | None = None
                ) -> Counts:
    """Every counted route's counts, read once. `routes` defaults to the recorded routes."""
    out_dir = counts_dir() if out_dir is None else out_dir
    names = list(routes) if routes is not None else [path.stem for path in sorted(out_dir.glob("*.tsv"))]
    by_route: dict[str, dict[int, int]] = {}
    for name in names:
        path = out_dir / f"{name}.tsv"
        if path.exists():
            by_route[name] = _read_counts(path)
    return Counts(by_route=by_route)