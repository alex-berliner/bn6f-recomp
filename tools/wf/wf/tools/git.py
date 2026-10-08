"""Local commits in the main checkout (never pushed: the user keeps git local)."""
import subprocess
from pathlib import Path

from wf.repo import REPO
from wf.tools.errors import ToolOutputError
from wf.types import CommitHash


def commit_files(files: tuple[Path, ...], message: str) -> CommitHash:
    """Commit exactly these files (added or changed) with this message, given in full
    (attribution lines included: they depend on who is committing)."""
    if not files:
        raise ValueError("nothing to commit: no files named")
    _git("add", "--", *map(str, files))
    _git("commit", "-q", "-m", message, "--", *map(str, files))
    return CommitHash(_git("rev-parse", "HEAD"))


def changed_files(under: tuple[Path, ...]) -> tuple[Path, ...]:
    """Files (and submodule pointers) changed or added under these paths."""
    done = subprocess.run(["git", "-C", str(REPO), "status", "--porcelain", "-z", "--untracked-files=all", "--",
                           *map(str, under)], capture_output=True, text=True, check=False)
    if done.returncode != 0:
        raise ToolOutputError(f"git status failed: {done.stderr.strip()}")
    return tuple(REPO / path for path in porcelain_paths(done.stdout))


def porcelain_paths(output: str) -> tuple[str, ...]:
    """The paths in `git status --porcelain -z` output: "XY path" entries, NUL-separated.
    X or Y may be a space (" M bn6f"), so nothing may be stripped."""
    return tuple(entry[3:] for entry in output.split("\0") if len(entry) > 3)


def _git(*arguments: str) -> str:
    done = subprocess.run(["git", "-C", str(REPO), *arguments], capture_output=True, text=True, check=False)
    if done.returncode != 0:
        raise ToolOutputError(f"git {arguments[0]} failed: {done.stderr.strip()}")
    return done.stdout.strip()
