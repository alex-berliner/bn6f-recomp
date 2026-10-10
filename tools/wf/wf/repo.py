"""Where things live in the main checkout, and which sources it holds.

The workflow code can live in any worktree; the tools it runs, the ROMs and
the recordings are in the main checkout (WF_REPO, default ~/Code/recomp).
"""
import os
import subprocess
import sys
from pathlib import Path

from wf.types import CommitHash, SourceVersion

REPO = Path(os.environ.get("WF_REPO", Path.home() / "Code" / "recomp"))
sys.path.append(str(REPO / "tools"))
import gamepaths  # noqa: E402  # tools/gamepaths.py: the game profile (tools/paths.env)

CPORT = REPO / "cport"
TRACES = Path(gamepaths.TRACES)
ORIGINAL_ROM = Path(gamepaths.ORIG_ROM)
TOOLCHAIN_BIN = REPO / "toolchain" / "bin"
RUNS = REPO / "build-wf-runs"  # tool logs of workflow runs (ignored by git as build-*/)
STATE_DIR = REPO / "build-wf-state"  # what all runs share, kept apart from them: locks, provider outages,
                                     # the install freeze, the build tally (ignored by git as build-*/)
SNAPSHOTS = REPO / "build-snapshots"  # the original game's captured calls, FUNCTION/snap_ROUTE_N.bin: every
                                     # capture adds to it; the farm and verify read it (ignored as build-*/)


def current_sources(tree: Path = REPO) -> SourceVersion:
    commit = _git(tree, "rev-parse", "HEAD")
    changes = _git(tree, "status", "--porcelain", "--untracked-files=no")
    return SourceVersion(commit=CommitHash(commit), has_uncommitted_changes=changes != "")


def _git(tree: Path, *args: str) -> str:
    done = subprocess.run(["git", "-C", str(tree), *args], capture_output=True, text=True, check=True)
    return done.stdout.strip()
