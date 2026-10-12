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


def commit_or_restore(files: tuple[Path, ...], message: str, keep: tuple[Path, ...] = ()) -> CommitHash:
    """commit_files; when the commit fails (the commit gate refuses it), FILES go back to the last commit
    first (unstaged; changed ones checked out, new ones removed), except KEEP (shared files the caller puts
    right itself), so nothing half-done stays in the main tree for a parallel run to pick up. The error is
    raised again."""
    try:
        return commit_files(files, message)
    except ToolOutputError:
        restore_to_head(tuple(path for path in files if path not in keep))
        unstage(keep)
        raise


def restore_to_head(files: tuple[Path, ...]) -> None:
    """FILES as the last commit has them: unstaged, changed ones checked out, ones it lacks removed."""
    if not files:
        return
    paths = [str(path.relative_to(REPO)) if path.is_absolute() else str(path) for path in files]
    tracked = set(_git("ls-tree", "-r", "--name-only", "HEAD", "--", *paths).splitlines())
    unstage(files)
    for relative in paths:
        if relative in tracked:
            _ = _git("checkout", "HEAD", "--", relative)
        else:
            (REPO / relative).unlink(missing_ok=True)


def unstage(files: tuple[Path, ...]) -> None:
    if files:
        _ = _git("reset", "-q", "HEAD", "--", *map(str, files))


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
