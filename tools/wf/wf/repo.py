"""Where things live in the main checkout, and which sources it holds.

The workflow code can live in any worktree; the tools it runs, the ROMs and
the recordings are in the main checkout (WF_REPO, default ~/Code/recomp).
"""
import os
import subprocess
from pathlib import Path

from wf.types import CommitHash, SourceVersion

REPO = Path(os.environ.get("WF_REPO", Path.home() / "Code" / "recomp"))
CPORT = REPO / "cport"
TRACES = REPO / "tools" / "traces"
ORIGINAL_ROM = REPO / "bn6f" / "bn6f.gba"
TOOLCHAIN_BIN = REPO / "toolchain" / "bin"
RUNS = REPO / "build-wf-runs"  # tool logs of workflow runs (ignored by git as build-*/)


def current_sources(tree: Path = REPO) -> SourceVersion:
    commit = _git(tree, "rev-parse", "HEAD")
    changes = _git(tree, "status", "--porcelain", "--untracked-files=no")
    return SourceVersion(commit=CommitHash(commit), has_uncommitted_changes=changes != "")


def _git(tree: Path, *args: str) -> str:
    done = subprocess.run(["git", "-C", str(tree), *args], capture_output=True, text=True, check=True)
    return done.stdout.strip()
