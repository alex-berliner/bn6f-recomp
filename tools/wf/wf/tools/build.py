"""Lift bn6f to C (so readable functions replace their lifted originals),
build the port ROM and the host difftest. Both builds go to directories of
the workflow's own, so a run never touches cport/build or cport/build-host,
which other sessions use. (The lifted sources in cport/src/lifted are shared:
they are what the readable code in cport/src/game makes them.)"""
import os
import subprocess
import time
from pathlib import Path

from wf.repo import CPORT, REPO, TOOLCHAIN_BIN
from wf.types import BuildFailure, BuiltRom, LogPath, RomPath, ToolMessage

ROM_BUILD_DIR = "build-wf"
HOST_BUILD_DIR = "../build-wf-host"  # relative to cport/host


def lift_and_build(log_dir: Path) -> BuiltRom | BuildFailure:
    log = LogPath(log_dir / "build.log")
    started = time.monotonic()
    jobs = f"-j{os.cpu_count()}"
    commands = [
        ["python3", str(REPO / "tools/lifter/lift.py"), "--bn6f", str(REPO / "bn6f"),
         "--out", str(CPORT / "src/lifted"), "--inc", str(CPORT / "include"), "--overrides", str(CPORT / "src/game")],
        ["make", "-C", str(CPORT), f"BUILD={ROM_BUILD_DIR}", jobs],
        ["make", "-C", str(CPORT / "host"), f"BUILD={HOST_BUILD_DIR}", jobs],
    ]
    env = {**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}"}
    with open(log, "w") as out:
        for command in commands:
            out.write(f"$ {' '.join(command)}\n")
            out.flush()
            done = subprocess.run(command, stdout=out, stderr=subprocess.STDOUT, env=env, check=False)
            if done.returncode != 0:
                return BuildFailure(log=log, last_lines=_last_lines(log, 12))
    return BuiltRom(
        rom=RomPath(CPORT / ROM_BUILD_DIR / "bn6f_lifted.gba"),
        difftest=CPORT / "build-wf-host" / "difftest",
        seconds=round(time.monotonic() - started, 1),
        log=log,
    )


def _last_lines(log: Path, count: int) -> tuple[ToolMessage, ...]:
    lines = log.read_text(errors="replace").splitlines()[-count:]
    return tuple(ToolMessage(line) for line in lines)


def build_host_difftest(log_dir: Path, build_dir: str = "build-wf-host") -> Path | BuildFailure:
    """Only the host difftest (what model candidates are tested with), from the current sources,
    into cport/BUILD_DIR (a farm batch uses its own, so it can run while a verify rebuilds)."""
    log = LogPath(log_dir / "build-difftest.log")
    env = {**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}"}
    with open(log, "w") as out:
        done = subprocess.run(["make", "-C", str(CPORT / "host"), f"BUILD=../{build_dir}", f"-j{os.cpu_count()}"],
                              stdout=out, stderr=subprocess.STDOUT, env=env, check=False)
    if done.returncode != 0:
        return BuildFailure(log=log, last_lines=_last_lines(log, 12))
    return CPORT / build_dir / "difftest"
