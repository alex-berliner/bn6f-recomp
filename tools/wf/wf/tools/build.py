"""Build the port ROM into a build directory of the workflow's own, so a run
never touches cport/build, which other sessions use."""
import os
import subprocess
import time
from pathlib import Path

from wf.repo import CPORT, TOOLCHAIN_BIN
from wf.types import BuildFailure, BuiltRom, LogPath, RomPath, ToolMessage

BUILD_DIR = "build-wf"


def build_rom(log_dir: Path) -> BuiltRom | BuildFailure:
    log = LogPath(log_dir / "build.log")
    env = {**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}"}
    started = time.monotonic()
    with open(log, "w") as out:
        done = subprocess.run(["make", "-C", str(CPORT), f"BUILD={BUILD_DIR}", f"-j{os.cpu_count()}"],
                              stdout=out, stderr=subprocess.STDOUT, env=env, check=False)
    if done.returncode != 0:
        return BuildFailure(log=log, last_lines=_last_lines(log, 10))
    rom = RomPath(CPORT / BUILD_DIR / "bn6f_lifted.gba")
    return BuiltRom(rom=rom, seconds=round(time.monotonic() - started, 1), log=log)


def _last_lines(log: Path, count: int) -> tuple[ToolMessage, ...]:
    lines = log.read_text(errors="replace").splitlines()[-count:]
    return tuple(ToolMessage(line) for line in lines)
