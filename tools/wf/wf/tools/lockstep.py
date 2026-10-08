"""Replay recorded routes on the original game and the port side by side
(tools/lockstep.sh). Its text output is parsed here and nowhere else."""
import os
import re
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from wf.repo import REPO
from wf.tools.errors import ToolOutputError
from wf.types import FrameCount, LockstepFailure, LockstepResult, LogPath, Outcome, RomPath, Route, ToolMessage

LOCKSTEP = REPO / "tools" / "lockstep.sh"

# "400 checkpoints to game frame 7980; hardware frames original 8066, tested 8132"
_PROGRESS = re.compile(r"^(\d+) checkpoints to game frame (\d+)")
# "FAIL: visible state differs at 3 checkpoints, first at game=1234"
_FIRST_BAD = re.compile(r"first at game=(\d+)")


def run_lockstep_routes(rom: RomPath, routes: tuple[Route, ...], log_dir: Path) -> tuple[LockstepResult, ...]:
    """Each route runs two emulators; run several routes at once."""
    workers = max(1, (os.cpu_count() or 2) // 2)
    with ThreadPoolExecutor(max_workers=workers) as pool:
        runs = [pool.submit(run_lockstep, rom, route, log_dir) for route in routes]
        return tuple(run.result() for run in runs)


def run_lockstep(rom: RomPath, route: Route, log_dir: Path) -> LockstepResult:
    work_dir = log_dir / f"lockstep-{route.name}"
    work_dir.mkdir(parents=True, exist_ok=True)
    log = LogPath(work_dir / "lockstep.log")
    started = time.monotonic()
    done = subprocess.run([str(LOCKSTEP), str(rom), str(route.trace), str(route.length)],
                          capture_output=True, text=True, check=False,
                          env={**os.environ, "OUT": str(work_dir)})
    log.write_text(done.stdout + done.stderr)
    seconds = round(time.monotonic() - started, 1)
    return _parse(route, done.returncode, done.stdout, seconds, log)


def _parse(route: Route, exit_code: int, output: str, seconds: float, log: LogPath) -> LockstepResult:
    lines = output.splitlines()
    progress = next((match for line in lines if (match := _PROGRESS.match(line))), None)
    # A route can print "OK: ..." for the screen and then "FAIL: ..." for save memory:
    # the first FAIL line decides, otherwise the OK line.
    failures = [line for line in lines if line.startswith("FAIL:")]
    successes = [line for line in lines if line.startswith("OK:")]
    verdict = (failures or successes or [None])[0]
    if progress is None or verdict is None:
        raise ToolOutputError(f"lockstep {route.name}: unrecognised output (exit {exit_code}), see {log}")

    failure = _failure(verdict)
    first_bad = _FIRST_BAD.search(verdict)
    return LockstepResult(
        route=route.name,
        outcome=Outcome.PASSED if exit_code == 0 and failure is None else Outcome.FAILED,
        checkpoints=int(progress[1]),
        reached_frame=FrameCount(int(progress[2])),
        failure=failure,
        first_bad_frame=FrameCount(int(first_bad[1])) if first_bad else None,
        summary=ToolMessage(verdict),
        seconds=seconds,
        log=log,
    )


def _failure(verdict: str) -> LockstepFailure | None:
    if verdict.startswith("OK:"):
        return None
    if "stopped early" in verdict:
        return LockstepFailure.STOPPED_EARLY
    if "save memory" in verdict:
        return LockstepFailure.SAVE_DIFFERS
    return LockstepFailure.SCREEN_DIFFERS
