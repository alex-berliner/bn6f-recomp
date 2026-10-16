"""What the vidsync driver's state means, and what to do about it: the rules the hand-run sync (10-10..10-13,
~/Code/vidsync) settled on, so a check-in can act alone on the routine cases and ask Claude only about the rest.
The log digest comes from remote/digest.py on the driver PC; `triage` is a pure function of it and the status.

Each rule names the incident that set it:
- a loop: one loss, many fixes (run6l: 89 RNG "fixes" at 370434 held on stale remote workers and failed in the
  run; run6g: kill and hand fixes undoing each other at 360462); a fix that came back is not asked again, but the
  next one can loop the same way.
- a suspect fix: a deletion or a write that only scraped past (run6u: kill 2.01, die 1.74 at 377151, a battle
  the video went on with; the right ones scored 0.64-1.2). A loss soon after one is likely its doing: roll back.
- empty fixes: a video offset or overlay window that "holds" in its test and not in the run (run6j/6k at 370434).
- a crash or an internal error: the driver's own code (run6r: a kill past the loss left the path short of
  keys; events() crashed).
- stalled: 30 minutes on one loss with none of the above: look at the pictures (the stalls that needed a new
  handler or a tweak: stream alerts, story battles, the Weather comp)."""
from enum import Enum

from pydantic import BaseModel

from wf.tools.vidsync import DRIVER_PC, DriverEnd, DriverStatus, json_line, remote
from wf.types import FrameCount, Result, RunName, ToolMessage

LOOP_FIXES = 3           # fixes at one loss that make it a loop
EMPTY_FIXES = 2          # empty fixes (no write) at one loss
SUSPECT_COST = {"kill": 1.5, "die": 1.5}   # a fix of these handlers at or above this mean cost is suspect
SUSPECT_COST_ANY = 2.0   # any other handler's
SUSPECT_REACH = 1000     # a loss this soon after a suspect fix (the latest fix) is put down to it
STALL_MINUTES = 30


class FixRecord(Result):
    n: int
    at: str
    handler: str
    mean_cost: float | None
    empty: bool
    writes: str


class LossDigest(Result):
    """remote/digest.py: what the log says about the loss the driver is on."""
    run: RunName
    loss: FrameCount | None
    first_seen: str | None
    minutes_on_loss: float | None
    fixes_at_loss: tuple[FixRecord, ...]
    came_back: tuple[str, ...]
    handlers: dict[str, str]
    search: str | None
    recent_fixes: tuple[FixRecord, ...]
    traceback: str | None


class _DigestLine(BaseModel):
    run: str
    loss: int | None
    first_seen: str | None
    minutes_on_loss: float | None
    fixes_at_loss: list[FixRecord]
    came_back: list[str]
    handlers: dict[str, str]
    search: str | None
    recent_fixes: list[FixRecord]
    traceback: str | None


def loss_digest(run: RunName) -> LossDigest:
    line = _DigestLine.model_validate_json(json_line(remote(DRIVER_PC, "py.sh", "remote/digest.py", run, timeout=300)))
    return LossDigest(run=run, loss=FrameCount(line.loss) if line.loss is not None else None, first_seen=line.first_seen,
                      minutes_on_loss=line.minutes_on_loss, fixes_at_loss=tuple(line.fixes_at_loss),
                      came_back=tuple(line.came_back), handlers=line.handlers, search=line.search,
                      recent_fixes=tuple(line.recent_fixes), traceback=line.traceback)


class TriageKind(Enum):
    PROGRESSING = "the sync is moving"
    SEARCHING = "on a loss, searching, not long yet"
    DONE = "the sync reached the end of the video"
    CRASHED = "the driver's process died"
    INTERNAL_ERROR = "the driver stopped on an internal error"
    LOOP = "fixes at one loss keep coming back to it"
    SUSPECT_FIX = "a recent fix only scraped past; the loss after it is likely its doing"
    STALLED = "on one loss too long with no fix"
    STUCK = "the driver gave up at a loss"


class Triage(Result):
    kind: TriageKind
    loss: FrameCount | None
    why: str                          # in plain words, from the log
    suggestion: str                   # what the hand-run sync did in this case
    evidence: tuple[ToolMessage, ...]
    needs_claude: bool                # False: the check-in acts alone (a restart) or only reports
    suspect: FixRecord | None = None  # SUSPECT_FIX: the fix to roll back before


def suspect_fix(fix: FixRecord) -> bool:
    if fix.mean_cost is None:
        return False
    return fix.mean_cost >= SUSPECT_COST.get(fix.handler, SUSPECT_COST_ANY)


def triage(status: DriverStatus, digest: LossDigest | None) -> Triage:
    """The driver's state in one of TriageKind, with what to do (the rules in this module's docstring, in order:
    the driver ended, a suspect fix, a loop, gave up, stalled, searching, moving)."""
    loss = digest.loss if digest is not None else status.loss
    evidence: list[ToolMessage] = [status.last_event]
    if not status.alive:
        if status.ended is DriverEnd.DONE:
            return Triage(kind=TriageKind.DONE, loss=None, why="the run ended at the video's end", suggestion=(
                "export it (wf vidsync-ready --import)"), evidence=tuple(evidence), needs_claude=False)
        if status.ended is DriverEnd.ERROR:
            return Triage(kind=TriageKind.INTERNAL_ERROR, loss=loss, why=status.last_event, suggestion=(
                "a slot or ring entry the driver relied on was overwritten: read the code path the message names, "
                "fix it, restart from the newest checkpoint before the loss"), evidence=tuple(evidence), needs_claude=True)
        if status.ended is DriverEnd.CRASHED:
            tb = digest.traceback if digest is not None else None
            return Triage(kind=TriageKind.CRASHED, loss=loss, why=tb or "the process died without a traceback",
                          suggestion=("a traceback is a bug in the driver or a handler: fix it, then restart from the "
                                      "newest checkpoint; none: restart from the newest checkpoint"),
                          evidence=tuple(evidence + ([ToolMessage(tb)] if tb else [])), needs_claude=tb is not None)
    if digest is not None and loss is not None:
        # the latest fix only: one followed by a good fix and a long match was not the loss's doing (run7y, 10-16:
        # an encounter fix at 482567, a good hand fix after it, an ordinary encounter at 484645)
        latest = digest.recent_fixes[-1] if digest.recent_fixes else None
        bad = latest if latest is not None and suspect_fix(latest) else None
        if bad is not None:
            frame = fix_frame(bad)
            if frame is not None and 0 <= loss - frame <= SUSPECT_REACH:
                return Triage(kind=TriageKind.SUSPECT_FIX, loss=loss, why=(
                    f"fix {bad.n} ({bad.handler}, mean cost {bad.mean_cost}) at {frame}, the loss {loss - frame} "
                    "frames after it"), suggestion=(
                    "render the stretch: if the fix cut short what the video goes on with (a battle), restart from the "
                    "checkpoint before it, and tighten the handler's acceptance"),
                    evidence=(ToolMessage(f"fix {bad.n} {bad.handler} cost {bad.mean_cost} {bad.writes[:120]}"),),
                    needs_claude=True, suspect=bad)
        at = digest.fixes_at_loss
        if len(at) >= LOOP_FIXES or sum(f.empty for f in at) >= EMPTY_FIXES:
            handlers = sorted({f.handler for f in at})
            return Triage(kind=TriageKind.LOOP, loss=loss, why=(
                f"{len(at)} fixes at {loss} ({', '.join(handlers)}; {sum(f.empty for f in at)} empty), each lost "
                "again there"), suggestion=(
                "the fixes hold in their tests and not in the run: compare one fix's test with the run (a stale "
                "remote state, a mask or window the run applies and the test does not, a later handler undoing an "
                "earlier fix). Restart from the checkpoint before the first of them once the cause is fixed"),
                evidence=tuple(ToolMessage(f"fix {f.n} {f.handler} cost {f.mean_cost} {f.writes[:80]}") for f in at[-6:]),
                needs_claude=True)
    if not status.alive and status.ended is DriverEnd.STUCK:
        return Triage(kind=TriageKind.STUCK, loss=loss, why=f"gave up at {loss}", suggestion=(
            "render the loss and look at what differs: a tweak to a built-in handler (most stalls), a new handler "
            "(wf vidsync-repair with the class), or, past the time box, ask the user"),
            evidence=tuple(_handler_lines(digest)), needs_claude=True)
    on_loss = digest.minutes_on_loss if digest is not None and loss is not None else None
    if loss is not None and on_loss is not None and on_loss >= STALL_MINUTES:
        return Triage(kind=TriageKind.STALLED, loss=loss, why=f"{on_loss:.0f} minutes on {loss}", suggestion=(
            "render the loss and look at what differs; the RNG search rarely fixes what it has not fixed in "
            "30 minutes"), evidence=tuple(_handler_lines(digest)), needs_claude=True)
    if loss is not None:
        return Triage(kind=TriageKind.SEARCHING, loss=loss, why=f"{on_loss or 0:.0f} minutes on {loss}",
                      suggestion="wait", evidence=tuple(evidence), needs_claude=False)
    return Triage(kind=TriageKind.PROGRESSING, loss=None, why=f"at {status.furthest}", suggestion="wait",
                  evidence=tuple(evidence), needs_claude=False)


def fix_frame(fix: FixRecord) -> int | None:
    """The game frame of a fix's first write ("write SPEC at game G; ...")."""
    for word, nxt in zip(fix.writes.split(), fix.writes.split()[1:]):
        if word == "game":
            digits = nxt.rstrip(";")
            if digits.isdigit():
                return int(digits)
    return None


def _handler_lines(digest: LossDigest | None) -> list[ToolMessage]:
    if digest is None:
        return []
    lines = [ToolMessage(f"{name}: {said}") for name, said in digest.handlers.items()]
    if digest.search:
        lines.append(ToolMessage(f"rng search: {digest.search}"))
    return lines
