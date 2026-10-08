"""Save the original game's own calls of functions while it replays a route
(tools/convert/capture.sh), and find which routes execute which functions
(tools/coverage.py over the routes' profiles)."""
import os
import re
import subprocess
from pathlib import Path

from wf.repo import REPO, TOOLCHAIN_BIN
from wf.tools.errors import ToolOutputError
from wf.types import CapturedCalls, FrameCount, FunctionName, Route, RouteName, SnapshotDir

CAPTURE = REPO / "tools/convert/capture.sh"
COVERAGE = REPO / "tools/coverage.py"
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
    executed: dict[RouteName, frozenset[FunctionName]] = {}
    wanted = set(functions)
    for route in routes:
        profile = PROFILES / f"{route.name}.prof"
        if not profile.exists():
            continue
        listing = log_dir / f"coverage-{route.name}.txt"
        done = subprocess.run(["python3", str(COVERAGE), str(profile), "--list", str(listing)],
                              capture_output=True, text=True, check=False)
        if done.returncode != 0 or not listing.exists():
            raise ToolOutputError(f"coverage.py failed for {route.name}: {done.stderr[-300:]}")
        units = {FunctionName(line.strip()) for line in listing.read_text().splitlines() if line.strip()}
        executed[route.name] = frozenset(units & wanted)
    return executed
