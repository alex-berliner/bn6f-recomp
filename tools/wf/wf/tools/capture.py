"""Save the original game's own calls of functions while it replays a route
(tools/convert/capture.sh), and find which routes execute which functions:
a function ran on a route when its first instruction's address is in that
route's profile (tools/traces/coverage/ROUTE.prof). Reading the profiles
directly takes a second; tools/coverage.py took about 3 minutes."""
import functools
import os
import re
import subprocess
from pathlib import Path

from wf.repo import REPO, TOOLCHAIN_BIN
from wf.tools.errors import ToolOutputError
from wf.types import CapturedCalls, FrameCount, FunctionName, Route, RouteName, SnapshotDir

CAPTURE = REPO / "tools/convert/capture.sh"
LIFTED = REPO / "cport/src/lifted"
PROFILES = REPO / "tools/traces/coverage"

# "sub_8001234: 40 calls"
_CALLS = re.compile(r"^(\S+): (\d+) calls$")


def capture_calls(snapshots: SnapshotDir, route: Route, first_frame: FrameCount, last_frame: FrameCount,
                  calls_per_function: int, functions: tuple[FunctionName, ...], log: Path) -> tuple[CapturedCalls, ...]:
    done = subprocess.run(
        [str(CAPTURE), str(snapshots), str(route.trace), str(first_frame), str(last_frame),
         str(calls_per_function), *functions],
        capture_output=True, text=True, check=False,
        env={**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}"})
    log.write_text(done.stdout + done.stderr)
    if done.returncode != 0:
        raise ToolOutputError(f"capture.sh failed (exit {done.returncode}), see {log}")
    counts = {FunctionName(match[1]): int(match[2])
              for match in filter(None, map(_CALLS.match, done.stdout.splitlines()))}
    return tuple(CapturedCalls(function=function, calls=counts.get(function, 0),
                               directory=snapshots / function if counts.get(function, 0) > 0 else None)
                 for function in functions)


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
