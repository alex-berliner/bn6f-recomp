"""Locks for what parallel runs share. Each convert run works in a worktree of
its own, but they all commit to the main tree, and lifting reads the bn6f
disassembly that a record run renames symbols in."""
import fcntl
from collections.abc import Generator
from contextlib import contextmanager

from wf.repo import RUNS

MAIN_TREE = "main-tree"   # installing a file into the main tree and committing it (difftest.cfg is shared)
BN6F = "bn6f"             # lifting reads bn6f (shared); renames and comments write it (exclusive)


@contextmanager
def held(name: str, exclusive: bool = True) -> Generator[None]:
    """Hold the named lock (shared, or exclusive) for the duration; waits until it can."""
    RUNS.mkdir(parents=True, exist_ok=True)
    with open(RUNS / f"{name}.lock", "a") as handle:
        fcntl.flock(handle, fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH)
        try:
            yield
        finally:
            fcntl.flock(handle, fcntl.LOCK_UN)
