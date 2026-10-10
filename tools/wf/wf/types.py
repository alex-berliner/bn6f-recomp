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
    difftest: Path  # the host difftest built from the same sources
    seconds: float
    log: LogPath
    compiled: int = 0       # C files the two builds compiled (the rest were up to date)
    ccache_hits: int = 0    # of those, how many ccache had already compiled


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


# ── more vocabulary ────────────────────────────────────────────────────

SnapshotDir = NewType("SnapshotDir", Path)   # capture.sh output: one sub-directory of calls per function
ModelSpec = NewType("ModelSpec", str)        # an outside model as "provider/model", e.g. "hyper/glm-5.3-flash"
SymbolName = NewType("SymbolName", str)      # any bn6f symbol (function, data, RAM, ...)
GuestAddress = NewType("GuestAddress", int)  # an address on the GBA bus


class NotRun(Result):
    """A check that did not apply to this run, and why."""
    reason: ToolMessage


# ── capture ────────────────────────────────────────────────────────────

class CapturedCalls(Result):
    """The original game's own calls of a function, saved for difftest and the oracle."""
    function: FunctionName
    calls: int
    directory: Path | None  # None when the function was never called in the window


# ── checks of readable functions ───────────────────────────────────────

class CapturedDifftestResult(Result):
    """A readable function compared with its lifted original on the original's own calls."""
    function: FunctionName
    outcome: Outcome
    snapshots: int
    runs: int
    failures: int
    out_of_domain: int

    @property
    def passed(self) -> bool:
        return self.outcome is Outcome.PASSED


class OracleResult(Result):
    """A function run on the original game (mGBA) and on the port from the same real states."""
    function: FunctionName
    address: GuestAddress
    compared: int
    differed: int
    out_of_domain: int
    differences: tuple[ToolMessage, ...]

    @property
    def outcome(self) -> Outcome:
        if self.differed > 0:
            return Outcome.FAILED
        if self.compared == 0:
            return Outcome.COMPARED_NOTHING
        return Outcome.PASSED


class BattleSweepResult(Result):
    """The recorded battles (pokes into the battle settings) replayed in lockstep."""
    battles: int
    identical: int
    failures: tuple[ToolMessage, ...]
    log: LogPath
    seconds: float

    @property
    def passed(self) -> bool:
        return self.identical == self.battles and self.battles > 0


# ── symbol database ────────────────────────────────────────────────────

class SymbolStatus(Enum):
    CONFIRMED = "confirmed"
    PROBABLE = "probable"
    HYPOTHESIS = "hypothesis"


class SymbolEntry(Result):
    """The parts of a symbols/db entry the workflows decide on."""
    name: SymbolName
    status: SymbolStatus
    file: Path


class Rename(Result):
    old: SymbolName
    new: SymbolName
