"""Replay the recorded battles in lockstep (tools/battlesweep.sh).
Its exit code can miss a screen mismatch (the last line of a failing
lockstep is the screenshots line), so every summary line without " OK:"
counts as a failure."""
import os
import subprocess
import time
from pathlib import Path

from wf.repo import REPO
from wf.tools.errors import ToolOutputError
from wf.types import BattleSweepResult, LogPath, RomPath, ToolMessage

BATTLESWEEP = REPO / "tools/battlesweep.sh"


def run_battle_sweep(rom: RomPath, jobs: int, log_dir: Path) -> BattleSweepResult:
    out = log_dir / "battlesweep"
    log = LogPath(log_dir / "battlesweep.log")
    started = time.monotonic()
    done = subprocess.run([str(BATTLESWEEP), str(rom)], capture_output=True, text=True, check=False,
                          env={**os.environ, "JOBS": str(jobs), "OUT": str(out)})
    log.write_text(done.stdout + done.stderr)
    summary = out / "summary.txt"
    if not summary.exists():
        raise ToolOutputError(f"battlesweep.sh wrote no summary (exit {done.returncode}), see {log}")
    lines = [line for line in summary.read_text().splitlines() if line.strip()]
    failures = tuple(ToolMessage(line) for line in lines if " OK:" not in line)
    return BattleSweepResult(battles=len(lines), identical=len(lines) - len(failures), failures=failures,
                             log=log, seconds=round(time.monotonic() - started, 1))
