"""Locks for what parallel runs share, and the install freeze. Each convert run works in a worktree of
its own, but they all commit to the main tree, and lifting reads the bn6f
disassembly that a record run renames symbols in.

The install freeze (joint plan S1): while build-wf-state/install-freeze exists, installs, renames
and commits wait, and the models keep writing candidates. Structural moves (the db rewrite, the
regroup) are made in such a window. `wf freeze --reason WHY` starts one and returns once the
installs already under way have finished; `wf thaw` ends it. A run waits at its next install,
rename or commit, so one that renamed before the freeze waits with its renames uncommitted
(`wf status` lists the runs still going)."""
import fcntl
import sys
import time
from collections.abc import Generator
from contextlib import contextmanager
from datetime import datetime

from pydantic import ValidationError

from wf.repo import STATE_DIR
from wf.types import Result

MAIN_TREE = "main-tree"   # installing a file into the main tree and committing it (difftest.cfg is shared)
BN6F = "bn6f"             # lifting reads bn6f (shared); renames and comments write it (exclusive)
CHANGE_THE_TREE = (MAIN_TREE, BN6F)   # held exclusively, these wait while installs are frozen
FREEZE = STATE_DIR / "install-freeze"
FREEZE_POLL_SECONDS = 60.0


class Freeze(Result):
    since: datetime
    reason: str


@contextmanager
def held(name: str, exclusive: bool = True) -> Generator[None]:
    """Hold the named lock (shared, or exclusive) for the duration; waits until it can. A lock
    that changes the tree also waits while installs are frozen (checked once it is held, so a
    freeze that starts while a run waits for the lock still stops the run)."""
    told = False
    while True:
        with _locked(name, exclusive):
            freeze = install_freeze() if exclusive and name in CHANGE_THE_TREE else None
            if freeze is None:
                yield
                return
        if not told:
            print(f"installs are frozen since {freeze.since:%m-%d %H:%M} ({freeze.reason}): "
                  f"waiting for `wf thaw` to take the {name} lock", file=sys.stderr, flush=True)
            told = True
        time.sleep(FREEZE_POLL_SECONDS)


@contextmanager
def _locked(name: str, exclusive: bool) -> Generator[None]:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    with open(STATE_DIR / f"{name}.lock", "a") as handle:
        fcntl.flock(handle, fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH)
        try:
            yield
        finally:
            fcntl.flock(handle, fcntl.LOCK_UN)


def install_freeze() -> Freeze | None:
    """The install freeze, while one is on. A bare `touch build-wf-state/install-freeze` is one too."""
    try:
        text = FREEZE.read_text()
    except FileNotFoundError:
        return None
    try:
        return Freeze.model_validate_json(text)
    except ValidationError:
        return Freeze(since=datetime.fromtimestamp(FREEZE.stat().st_mtime), reason=text.strip() or "no reason given")


def freeze_installs(reason: str) -> Freeze:
    """Start an install freeze; returns once the installs, renames and commits under way have finished."""
    freeze = install_freeze()
    if freeze is None:
        STATE_DIR.mkdir(parents=True, exist_ok=True)
        freeze = Freeze(since=datetime.now(), reason=reason)
        _ = FREEZE.write_text(freeze.model_dump_json(indent=1) + "\n")
    for name in CHANGE_THE_TREE:
        with _locked(name, exclusive=True):
            pass
    return freeze


def thaw_installs() -> Freeze | None:
    """End the install freeze (the one that was on, if any)."""
    freeze = install_freeze()
    FREEZE.unlink(missing_ok=True)
    return freeze
