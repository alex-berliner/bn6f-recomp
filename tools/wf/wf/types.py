"""The vocabulary every workflow shares.

Domain strings and numbers get their own NewType, so a FunctionName can never
be passed where a RouteName is expected. Results are frozen pydantic models:
validated when they are made, immutable after, and shown field by field in
the Burr UI.
"""
from enum import Enum
from pathlib import Path
from typing import NewType

from pydantic import BaseModel, ConfigDict

FunctionName = NewType("FunctionName", str)  # a bn6f symbol, e.g. "setBGTileIdsBlocks1And2_8030628"
RouteName = NewType("RouteName", str)        # a recorded route in tools/traces, e.g. "wander"
CommitHash = NewType("CommitHash", str)      # a git commit of the main checkout
RomPath = NewType("RomPath", Path)           # a built port ROM
LogPath = NewType("LogPath", Path)           # a tool's full output, kept for reading later
FrameCount = NewType("FrameCount", int)      # game frames, as the game's own frame counter counts them
ToolMessage = NewType("ToolMessage", str)    # one line a tool printed, quoted as evidence


class Result(BaseModel):
    """Base for everything a step produces: validated, then immutable."""
    model_config = ConfigDict(frozen=True)


class Outcome(Enum):
    PASSED = "passed"
    FAILED = "failed"
    COMPARED_NOTHING = "compared nothing"  # every trial was out of domain (docs/PROCESS.md §3)


# ── sources ────────────────────────────────────────────────────────────

class SourceVersion(Result):
    """Which sources a run checked."""
    commit: CommitHash
    has_uncommitted_changes: bool


# ── build ──────────────────────────────────────────────────────────────

class BuiltRom(Result):
    rom: RomPath
    seconds: float
    log: LogPath


class BuildFailure(Result):
    log: LogPath
    last_lines: tuple[ToolMessage, ...]


# ── difftest ───────────────────────────────────────────────────────────

class DifftestResult(Result):
    """A readable function compared with its lifted original on random inputs."""
    function: FunctionName
    outcome: Outcome
    trials: int
    failures: int
    out_of_domain: int

    @property
    def passed(self) -> bool:
        return self.outcome is Outcome.PASSED


# ── lockstep ───────────────────────────────────────────────────────────

class Route(Result):
    name: RouteName
    trace: Path
    length: FrameCount


class LockstepFailure(Enum):
    STOPPED_EARLY = "the port stopped before the original"
    SCREEN_DIFFERS = "palette, VRAM or OAM differ"
    SAVE_DIFFERS = "save memory differs at the end"


class LockstepResult(Result):
    """One recorded route replayed on the original and the port side by side."""
    route: RouteName
    outcome: Outcome
    checkpoints: int
    reached_frame: FrameCount
    failure: LockstepFailure | None = None
    first_bad_frame: FrameCount | None = None
    summary: ToolMessage
    seconds: float
    log: LogPath

    @property
    def passed(self) -> bool:
        return self.outcome is Outcome.PASSED
