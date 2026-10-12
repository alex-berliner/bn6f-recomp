"""Save the original game's own calls of functions while it replays a route
(tools/convert/capture.sh), and find which routes execute which functions:
a function ran on a route when its first instruction's address is in that
route's profile (tools/traces/coverage/ROUTE.prof). Reading the profiles
directly takes a second; tools/coverage.py took about 3 minutes. A long route with checkpoints
(tools/import_route.py) is captured in windows, each replayed from its checkpoint."""
import functools
import os
import re
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import NamedTuple

from wf.repo import CHECKPOINTS, REPO, TOOLCHAIN_BIN
from wf.tools.errors import ToolOutputError
from wf.types import CapturedCalls, FrameCount, FunctionName, Route, RouteName, SnapshotDir

CAPTURE = REPO / "tools/convert/capture.sh"
LIFTED = REPO / "cport/src/lifted"
PROFILES = REPO / "tools/traces/coverage"

# "sub_8001234: 40 calls"
_CALLS = re.compile(r"^(\S+): (\d+) calls$")


def capture_calls(snapshots: SnapshotDir, route: Route, first_frame: FrameCount, last_frame: FrameCount,
                  calls_per_function: int, functions: tuple[FunctionName, ...], log: Path) -> tuple[CapturedCalls, ...]:
    windows = _windows(route, first_frame, last_frame)
    if windows:
        return _capture_in_windows(snapshots, route, windows, calls_per_function, functions, log)
    done = _run_capture(snapshots, route, first_frame, last_frame, calls_per_function, functions, log)
    counts = {FunctionName(match[1]): int(match[2])
              for match in filter(None, map(_CALLS.match, done.stdout.splitlines()))}
    return tuple(CapturedCalls(function=function, calls=counts.get(function, 0),
                               directory=snapshots / function if counts.get(function, 0) > 0 else None)
                 for function in functions)


def _run_capture(snapshots: SnapshotDir, route: Route, first_frame: FrameCount, last_frame: FrameCount,
                 calls_per_function: int, functions: tuple[FunctionName, ...], log: Path,
                 tag: str | None = None) -> subprocess.CompletedProcess[str]:
    done = subprocess.run(
        [str(CAPTURE), str(snapshots), str(route.trace), str(first_frame), str(last_frame),
         str(calls_per_function), *functions],
        capture_output=True, text=True, check=False,
        # KEEP_INTERRUPTED: long functions are nearly always interrupted; their entries still test fine
        env={**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}", "KEEP_INTERRUPTED": "1",
             **({"SNAP_TAG": tag} if tag else {})})
    log.write_text(done.stdout + done.stderr)
    if done.returncode != 0:
        raise ToolOutputError(f"capture.sh failed (exit {done.returncode}), see {log}")
    return done


# ── long routes: windows from checkpoints ──────────────────────────────

WINDOW_JOBS = 6


class _Window(NamedTuple):
    first: FrameCount
    last: FrameCount
    profile: Path


def _windows(route: Route, first_frame: FrameCount, last_frame: FrameCount) -> tuple[_Window, ...]:
    """A route with checkpoints (tools/import_route.py) is replayed in windows, each from its
    checkpoint: NAME_GGGGGGG.prof is the profile of the window that starts at game frame G."""
    starts = sorted(int(path.stem.rsplit("_", 1)[1]) for path in (CHECKPOINTS / route.name).glob(f"{route.name}_*.prof"))
    ends = [*starts[1:], int(route.length)]
    return tuple(_Window(FrameCount(start), FrameCount(end), CHECKPOINTS / route.name / f"{route.name}_{start:07d}.prof")
                 for start, end in zip(starts, ends) if start < last_frame and end > first_frame)


def _capture_in_windows(snapshots: SnapshotDir, route: Route, windows: tuple[_Window, ...], calls_per_function: int,
                        functions: tuple[FunctionName, ...], log: Path) -> tuple[CapturedCalls, ...]:
    """Each function's calls from the first window that runs it; the windows replay in parallel."""
    addresses = _function_addresses()
    assigned: dict[_Window, list[FunctionName]] = {}
    for function in functions:
        window = next((window for window in windows
                       if addresses.get(function) in _executed_addresses(window.profile, window.profile.stat().st_mtime)),
                      None)
        if window is not None:
            assigned.setdefault(window, []).append(function)

    def capture(window: _Window) -> None:
        tag = f"{route.name}_{window.first:07d}"
        _run_capture(snapshots, route, window.first, window.last, calls_per_function, tuple(assigned[window]),
                     log.with_name(f"{log.stem}-{window.first:07d}{log.suffix}"), tag)

    with ThreadPoolExecutor(max_workers=WINDOW_JOBS) as pool:
        list(pool.map(capture, assigned))
    log.write_text("".join(f"window {window.first}..{window.last}: {len(names)} functions\n"
                           for window, names in assigned.items()))
    window_of = {function: window for window, names in assigned.items() for function in names}

    def calls(function: FunctionName) -> int:
        window = window_of.get(function)
        if window is None:
            return 0
        tag = f"snap_{route.name}_{window.first:07d}_"
        return sum(1 for path in (snapshots / function).glob(f"{tag}*.bin") if not path.name.endswith(".exit.bin"))

    return tuple(CapturedCalls(function=function, calls=n, directory=snapshots / function if n > 0 else None)
                 for function in functions for n in [calls(function)])


def routes_executing(functions: tuple[FunctionName, ...], routes: tuple[Route, ...], log_dir: Path
                     ) -> dict[RouteName, frozenset[FunctionName]]:
    """For each route with a coverage profile, which of `functions` it executes."""
    addresses = _function_addresses()
    executed: dict[RouteName, frozenset[FunctionName]] = {}
    for route in routes:
        profile = PROFILES / f"{route.name}.prof"
        if not profile.exists():
            continue
        pcs = _executed_addresses(profile, profile.stat().st_mtime)
        executed[route.name] = frozenset(function for function in functions if addresses.get(function) in pcs)
    return executed


@functools.cache
def _function_addresses() -> dict[FunctionName, int]:
    """Every lifted unit's start address, from the lifted C's header comments."""
    header = re.compile(r"^/\* (\w+): ([0-9A-F]{8})-", re.M)
    return {FunctionName(match[1]): int(match[2], 16)
            for path in LIFTED.glob("*.c") for match in header.finditer(path.read_text())}


@functools.cache
def _executed_addresses(profile: Path, modified: float) -> frozenset[int]:
    """The addresses a profile shows executing (lines `%08x cycles`; `#` lines are headers)."""
    with open(profile) as lines:
        return frozenset(int(line.split()[0], 16) for line in lines if line.strip() and not line.startswith("#"))
