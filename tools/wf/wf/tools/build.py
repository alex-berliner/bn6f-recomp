"""Lift bn6f to C (so readable functions replace their lifted originals),
build the port ROM and the host difftest. Both builds go to directories of
the workflow's own, so a run never touches cport/build or cport/build-host,
which other sessions use. (The lifted sources in cport/src/lifted are shared:
they are what the readable code in cport/src/game makes them.)"""
import os
import subprocess
import time
from contextlib import nullcontext
from datetime import datetime
from pathlib import Path
from typing import TextIO

from wf.repo import CPORT, REPO, STATE_DIR, TOOLCHAIN_BIN
from wf.tools.locks import BN6F, held
from wf.types import BuildFailure, BuiltRom, LogPath, RomPath, ToolMessage

ROM_BUILD_DIR = "build-wf"
HOST_BUILD_DIR = "../build-wf-host"  # relative to cport/host
WORKTREES = REPO / "build-wf-worktrees"
# not in a worktree (submodules, and built tools git ignores): linked from the main checkout. The ROM
# build links ../bn6f/bn6f.gba, the tools read the BIOS from gbarecomp and run build-tools/mgba_run.
SHARED = ("bn6f", "gbarecomp", "build-tools", "toolchain")
CCACHE_LOG = STATE_DIR / "ccache.tsv"   # every workflow build: when, where, files compiled, ccache hits


def lift_and_build(log_dir: Path, tree: Path = REPO) -> BuiltRom | BuildFailure:
    """Lift and build the tree (the main checkout, or a run's worktree; bn6f is the main one's)."""
    log = LogPath(log_dir / "build.log")
    started = time.monotonic()
    jobs = f"-j{os.cpu_count()}"
    link_shared(tree)
    cport = tree / "cport"
    lift = _lift_command(tree)
    builds = [["make", "-C", str(cport), f"BUILD={ROM_BUILD_DIR}", jobs],
              ["make", "-C", str(cport / "host"), f"BUILD={HOST_BUILD_DIR}", jobs]]
    stats = log_dir / "ccache.stats"
    env = {**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}", "CCACHE_STATSLOG": str(stats)}
    with open(log, "w") as out:
        for command in (lift, *builds):
            out.write(f"$ {' '.join(command)}\n")
            out.flush()
            with held(BN6F, exclusive=False) if command is lift else nullcontext():
                done = subprocess.run(command, stdout=out, stderr=subprocess.STDOUT, env=env, check=False)
            if done.returncode != 0:
                return BuildFailure(log=log, last_lines=_last_lines(log, 12))
        compiled, hits = note_ccache(stats, log_dir, out)
    return BuiltRom(
        rom=RomPath(cport / ROM_BUILD_DIR / "bn6f_lifted.gba"),
        difftest=cport / "build-wf-host" / "difftest",
        seconds=round(time.monotonic() - started, 1),
        log=log,
        compiled=compiled,
        ccache_hits=hits,
    )


def note_ccache(stats: Path, log_dir: Path, out: TextIO) -> tuple[int, int]:
    """How many files a build compiled and how many of those ccache answered (from ccache's per-build
    stats log, CCACHE_STATSLOG): written to the build log and to build-wf-state/ccache.tsv."""
    compiled = hits = 0
    if stats.exists():
        for entry in ("\n" + stats.read_text()).split("\n# ")[1:]:   # "# SOURCE" then one counter per line
            counters = set(entry.splitlines()[1:])
            if counters & {"direct_cache_hit", "preprocessed_cache_hit", "cache_miss"}:
                compiled += 1
                hits += "cache_miss" not in counters
        stats.unlink()
    _ = out.write(f"ccache: {hits} of {compiled} compiled files from the cache\n")
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    with open(CCACHE_LOG, "a") as tsv:
        _ = tsv.write(f"{datetime.now().isoformat(timespec='seconds')}\t{log_dir}\t{compiled}\t{hits}\n")
    return compiled, hits


def _last_lines(log: Path, count: int) -> tuple[ToolMessage, ...]:
    lines = log.read_text(errors="replace").splitlines()[-count:]
    return tuple(ToolMessage(line) for line in lines)


def build_host_difftest(log_dir: Path, build_dir: str = "build-wf-host", tree: Path | None = None) -> Path | BuildFailure:
    """Only the host difftest (what model candidates are tested with), into cport/BUILD_DIR: from the
    main tree's current sources, or, given a committed tree (`committed_tree`), lifted and built there."""
    log = LogPath(log_dir / "build-difftest.log")
    stats = log_dir / "ccache-difftest.stats"
    env = {**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}", "CCACHE_STATSLOG": str(stats)}
    cport = (tree / "cport") if tree is not None else CPORT
    lift = _lift_command(tree) if tree is not None else None
    commands = [["make", "-C", str(cport / "host"), f"BUILD=../{build_dir}", f"-j{os.cpu_count()}"]]
    if lift is not None:
        commands.insert(0, lift)
    with open(log, "w") as out:
        for command in commands:
            out.write(f"$ {' '.join(command)}\n")
            out.flush()
            with held(BN6F, exclusive=False) if command is lift else nullcontext():
                done = subprocess.run(command, stdout=out, stderr=subprocess.STDOUT, env=env, check=False)
            if done.returncode != 0:
                return BuildFailure(log=log, last_lines=_last_lines(log, 12))
        _ = note_ccache(stats, log_dir, out)
    return cport / build_dir / "difftest"


def _lift_command(tree: Path) -> list[str]:
    cport = tree / "cport"
    return ["python3", str(tree / "tools/lifter/lift.py"), "--bn6f", str(REPO / "bn6f"), "--out", str(cport / "src/lifted"),
            "--inc", str(cport / "include"), "--overrides", str(cport / "src/game")]


def remove_tree(tree: Path) -> None:
    """Take a run's worktree away again (its builds with it)."""
    if tree != REPO and tree.is_relative_to(WORKTREES):
        for name in SHARED:   # the links to the main checkout go first, so nothing follows them
            if (tree / name).is_symlink():
                (tree / name).unlink()
        _ = subprocess.run(["git", "-C", str(REPO), "worktree", "remove", "--force", str(tree)],
                           capture_output=True, check=False)


def committed_tree(name: str) -> Path:
    """The last commit checked out at build-wf-worktrees/NAME (a git worktree, made or moved there).
    Builds there never see a file a conversion has put into the main tree for its verify, nor the
    lifting that goes with it (a farm build once linked half of each)."""
    tree = WORKTREES / name
    head = subprocess.run(["git", "-C", str(REPO), "rev-parse", "HEAD"], capture_output=True, text=True,
                          check=True).stdout.strip()
    if (tree / ".git").exists():
        _ = subprocess.run(["git", "-C", str(tree), "checkout", "--quiet", "--detach", "--force", head], check=True)
    else:
        WORKTREES.mkdir(exist_ok=True)
        _ = subprocess.run(["git", "-C", str(REPO), "worktree", "add", "--quiet", "--detach", str(tree), head],
                           check=True)
    link_shared(tree)
    return tree


def link_shared(tree: Path) -> None:
    """Link what a worktree lacks (SHARED) to the main checkout's."""
    if tree == REPO:
        return
    for name in SHARED:
        path = tree / name
        if not path.is_symlink() and (REPO / name).exists():
            if path.is_dir():
                path.rmdir()   # an empty submodule directory
            path.symlink_to(REPO / name)
