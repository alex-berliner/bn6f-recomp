#!/usr/bin/env python3
"""The farm's pool: every candidate the model farm ever wrote, gathered per function under its
current name, re-tested without any model call, and listed in symbols/pool.tsv (stage S0 of
docs/reviews/2026-10-10-4-joint-plan.md; review 1 Area 1, review 3 Area 2 A8).

  tools/wf/.venv/bin/python tools/convert/pool.py [--jobs 16] [--snaps DIR] [--no-retest]

Candidates: tools/convert/accepted/FUNC.MODEL.c (each passed when it was written) and every
distinct code a failed round logged in tools/convert/log/FUNC.MODEL.jsonl. MODEL carries the prompt when
it is not the typed one (`wf farm --prompt old`: minimax_MiniMax-M3+old); the models column shows it, and
a chosen old-prompt candidate gets a note.

Names: functions were renamed after the farm saw them. The rename record used is symbols/db:
tools/rename_symbol.py, which makes every rename, moves the entry to NEW.yaml and adds OLD to its
`previous_names` (and the bn6f commit to its `changes`; that commit's "Rename OLD -> NEW" subject
on bn6f's recomp-fixes branch is the same pair). An entry renamed back drops the name again, so
`previous_names` maps each old name to the current one directly, with no chains to follow. (Checked
2026-10-10: those bn6f commits replayed in order give the same 749 pairs, plus sub_811FA0C, which
its entry lacks and no candidate uses.)

States: installed (cport/src/game defines `Next NAME(void)`), pending (a candidate passed, not
installed), failed (no candidate ever passed).

Re-test: each candidate of a function that is not installed, with the code renamed to today's
names, through the farm's own acceptance test (farm.test) against a difftest built from the last
commit in a worktree of its own (build-wf-worktrees/pool), so the live farm's build is never
touched. The register check is skipped there (these candidates predate the typed convention); its
result is kept apart so the choice can prefer typed code: passes, then typed, then fewest
register references, then newest. A function that passes gets its chosen candidate written to
tools/convert/accepted/CURRENT.MODEL.c, unless an accepted file exists under that name already,
so the farm counts the pair as done. Such a file is known later by its log having no passing row,
so running again changes nothing and still counts the function as failed when farmed. The build
log and the snapshot links (captures are named as the function was then) go to build-wf-runs/pool-*.

The farm may be running: logs and accepted files are read once at the start, a function whose log
changed in the last 10 minutes is left alone, and no log or existing accepted file is written.

Drops: when a farm group goes through the convert workflow, the functions a check names are taken out
of the group (drop_failing_functions). Why lived only in the Burr tracking logs
(~/.burr/recomp-convert/RUN/log.jsonl, with the verify runs' own checks in ~/.burr/recomp-verify), which
are read, never written. Every drop goes to symbols/pool_drops.tsv (one row per drop: run, the step whose
check named it, reason, the check's own report; an oracle difference sorted into tools/oracle_triage.md's
classes where it matches). Rows of runs no longer in ~/.burr are kept, so the ledger outlives the logs.
pool.tsv gets each function's count and latest drop, its size (assembly instruction lines) and, when
installed, the model its install commit names (both counted as tools/convert/scorecard.py counts them).
"""
import argparse
import json
import re
import subprocess
import sys
import tempfile
import time
from collections import Counter
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor
from datetime import date
from enum import Enum
from pathlib import Path
from typing import Callable, NamedTuple, NewType, Protocol, cast

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/wf"))   # before farm.py puts tools/ first (tools/wf would shadow wf)
sys.path.insert(0, str(REPO / "tools/abi"))
sys.path.append(str(REPO / "tools/convert"))

import yaml  # noqa: E402

import lint  # noqa: E402  # pyright: ignore[reportMissingImports]  # tools/abi/lint.py: its register categories
from wf.burr_typed import burr_dir  # noqa: E402
from wf.repo import RUNS  # noqa: E402
from wf.tools.build import build_host_difftest, committed_tree  # noqa: E402
from wf.tools.models import (ACCEPTED_DIR, AFTER_REVIEW, FARM, LOG_DIR, CandidateC, ConversionContext,  # noqa: E402
                             PromptKind, conversion_context, prompt_of)
from wf.types import BuildFailure, FunctionName, Result  # noqa: E402
import scorecard  # noqa: E402  # pyright: ignore[reportMissingImports]  # tools/convert/scorecard.py: size and writer

# tools/abi/lint.py's pieces: code with comments and strings blanked, and its register-level categories
blank_comments_and_strings = cast(Callable[[str], tuple[str, object]], lint.strip)  # pyright: ignore[reportUnknownMemberType]
REGISTER_CATEGORIES = cast(dict[str, re.Pattern[str]], lint.CATEGORIES)  # pyright: ignore[reportUnknownMemberType]


class Assembly(Protocol):
    """scorecard.Assembly: a function of bn6f/asm, as the scorecard counts its size."""
    @property
    def lines(self) -> int: ...      # assembly instruction lines
    @property
    def calls(self) -> frozenset[str]: ...
    @property
    def bios(self) -> frozenset[int]: ...


# tools/convert/scorecard.py's pieces, so pool.tsv and the scorecard count sizes and writers alike
assembly = cast(Callable[[], dict[str, Assembly]], scorecard.assembly)  # pyright: ignore[reportUnknownMemberType]
writers = cast(Callable[[dict[FunctionName, FunctionName]], dict[str, str]], scorecard.writers)  # pyright: ignore[reportUnknownMemberType]

ModelTag = NewType("ModelTag", str)    # a model as file names carry it: "hyper_glm-5.3-flash"
InputKey = NewType("InputKey", str)    # farm.input_key: a hash of the function's original bytes

POOL = REPO / "symbols/pool.tsv"
DATABASE = REPO / "symbols/db"
GAME = REPO / "cport/src/game"
REJECTED_DIR = REPO / "tools/convert/rejected"
SNAPSHOTS = REPO / "build-snapshots"
FARM_QUIET_SECONDS = 600   # a log written more recently than this belongs to a live farm run


class State(Enum):
    INSTALLED = "installed"   # cport/src/game defines `Next NAME(void)`
    PENDING = "pending"       # a candidate passed, the function is not installed
    FAILED = "failed"         # no candidate ever passed


class Verdict(Enum):
    """How close a re-tested candidate came, best first (review 3 Area 2's ranking)."""
    PASSED = "passed"
    RANDOM_INPUTS = "differs on random inputs"
    CAPTURED_CALLS = "differs on captured calls"
    NOTHING_COMPARED = "compared nothing"
    COMPILE = "does not compile"
    STYLE = "lifted style or a review rule"
    NO_ENTRY = "no Next NAME(void)"
    OTHER = "did not run"


class Candidate(Result):
    written_as: FunctionName        # the function's name when the farm wrote it
    model: ModelTag
    code: CandidateC                # renamed to today's names
    source: str                     # where it is kept: an accepted file, or a log row
    passed_when_written: bool
    report_then: str                # a failed one: the farm's report when it was written
    newness: tuple[float, int]      # (file time, row): larger is newer


class Function(Result):
    name: FunctionName                       # today's name
    written_as: tuple[FunctionName, ...]     # every name its candidates were written under
    candidates: tuple[Candidate, ...]
    installed_in: Path | None
    farm_busy: bool                          # the farm wrote one of its logs in the last 10 minutes

    @property
    def state(self) -> State:
        if self.installed_in is not None:
            return State.INSTALLED
        return State.PENDING if any(c.passed_when_written for c in self.candidates) else State.FAILED


class Retest(Result):
    candidate: Candidate
    verdict: Verdict
    reason: str                  # one short line
    failing_share: float         # of the comparisons made, the share that differed
    register_violations: int     # functions tools/abi/lint.py flags (0: typed, as the convention wants)
    register_references: int     # register-level hits anywhere in the code, adapters included

    def preference(self) -> tuple[int, float, bool, int, tuple[float, int]]:
        """Sort key, best first: closest to passing, then typed, fewest register references, newest."""
        newest_first = (-self.candidate.newness[0], -self.candidate.newness[1])
        return (list(Verdict).index(self.verdict), self.failing_share, self.register_violations > 0,
                self.register_references, newest_first)


class Outcome(Result):
    """What became of one function in this run."""
    function: Function
    input_key: InputKey | None
    retests: tuple[Retest, ...] = ()
    not_run: str = ""                 # why it was not re-tested
    installed_share: float | None = None   # installed: how much of its closest candidate is in its file
    written: Path | None = None       # the accepted file a re-test wrote for it
    written_now: bool = False         # by this run
    notes: tuple[str, ...] = ()

    @property
    def best(self) -> Retest | None:
        return min(self.retests, key=Retest.preference) if self.retests else None

    @property
    def passes_now(self) -> bool:
        return any(r.verdict is Verdict.PASSED for r in self.retests)

    @property
    def state_after(self) -> State:
        before = self.function.state
        return State.PENDING if before is State.FAILED and self.passes_now else before


# ── reading what the farm left ─────────────────────────────────────────

YAML_LOADER = cast(type[yaml.SafeLoader], getattr(yaml, "CSafeLoader", yaml.SafeLoader))   # the C one when built: 10x faster


def renames() -> dict[FunctionName, FunctionName]:
    """OLD -> today's name, from symbols/db's previous_names (see the module docstring)."""
    current: dict[FunctionName, FunctionName] = {}
    for path in sorted(DATABASE.glob("*.yaml")):
        entry = cast(dict[str, object], yaml.load(path.read_text(), Loader=YAML_LOADER))
        for old in cast(list[str], entry.get("previous_names") or []):
            current[FunctionName(old)] = FunctionName(str(entry["name"]))
    return current


def installed_functions() -> dict[FunctionName, Path]:
    """Each function cport/src/game defines a `Next NAME(void)` body for, and its file."""
    found: dict[FunctionName, Path] = {}
    for path in sorted(GAME.glob("*.c")):
        for name in cast(list[str], re.findall(r"\bNext\s+(\w+)\s*\(\s*void\s*\)\s*\{", path.read_text())):
            found.setdefault(FunctionName(name), path)
    return found


def renamed(code: str, current: dict[FunctionName, FunctionName], today: set[FunctionName]) -> CandidateC:
    """The code with every old name (also as A_OLD / L_OLD, as tools/rename_symbol.py renames them)
    replaced by today's; a name that is current again is left alone."""
    def rename(match: re.Match[str]) -> str:
        prefix, name = match[1] or "", FunctionName(match[2])
        return prefix + current[name] if name in current and name not in today else match[0]
    return CandidateC(re.sub(r"\b(A_|L_)?([A-Za-z_]\w*)\b", rename, code))


class Pool(Result):
    """What the farm left, read once."""
    candidates: dict[FunctionName, list[Candidate]]   # by today's name
    farm_busy: set[FunctionName]                      # functions whose logs changed in the last 10 minutes
    from_retests: set[Path]   # accepted files an earlier re-test wrote: their own log has no passing row


def farm_passed(log: Path) -> bool:
    """Whether the farm's log has a passing row (it writes that row before the accepted file)."""
    try:
        lines = log.read_text().splitlines()
    except FileNotFoundError:
        return True   # no log to go by: trust the accepted file
    for line in lines:
        try:
            if cast(dict[str, object], json.loads(line)).get("ok"):
                return True
        except json.JSONDecodeError:
            continue
    return False


def read_candidates(current: dict[FunctionName, FunctionName], today: set[FunctionName]) -> Pool:
    """Every candidate per today's name. Files may appear or grow while this reads: a vanished file or
    a half-written last row is skipped. An accepted file a re-test wrote is not a candidate of its own
    (its code is a failed log row's), so the function still counts as failed when farmed. Code a review
    rejected (tools/convert/rejected/NAME.MODEL.c, with its .review.txt) is no candidate either, wherever it
    turns up again (its log row, or an accepted file a later re-test wrote), so it is never chosen again; nor is
    anything else that model wrote for that function before: the review is a verdict on the model's attempt
    (pool.py once wrote an earlier round's code of a reviewed pair back to accepted). What a retry given the
    review wrote counts again: its log rows have `after_review`, its accepted file a .after-review.txt note."""
    started = time.time()
    found: dict[FunctionName, list[Candidate]] = {}
    busy: set[FunctionName] = set()
    from_retests: set[Path] = set()
    rejected: set[tuple[str, str, str]] = set()   # (written_as, model, code)
    for path in sorted(REJECTED_DIR.glob("*.c")):
        written_as, model = path.stem.split(".", 1)
        try:
            rejected.add((written_as, model, path.read_text().strip()))
        except FileNotFoundError:
            continue

    reviewed = {tuple(path.name.removesuffix(".review.txt").split(".", 1)) for path in REJECTED_DIR.glob("*.review.txt")}

    def add(written_as: str, model: str, code: str, source: str, report: str | None, newness: tuple[float, int],
            after_review: bool = False) -> None:
        if (written_as, model, code.strip()) in rejected or ((written_as, model) in reviewed and not after_review):
            return
        name = FunctionName(written_as)
        found.setdefault(current.get(name, name), []).append(Candidate(
            written_as=name, model=ModelTag(model), code=renamed(code, current, today), source=source,
            passed_when_written=report is None, report_then=report or "", newness=newness))

    for path in sorted(ACCEPTED_DIR.glob("*.c")):
        written_as, model = path.stem.split(".", 1)
        if not farm_passed(LOG_DIR / f"{path.stem}.jsonl"):
            from_retests.add(path)
            continue
        try:
            add(written_as, model, path.read_text(), str(path.relative_to(REPO)), None, (path.stat().st_mtime, 0),
                after_review=(ACCEPTED_DIR / f"{path.stem}{AFTER_REVIEW}").exists())
        except FileNotFoundError:
            continue
    for path in sorted(LOG_DIR.glob("*.jsonl")):
        written_as, model = path.name.removesuffix(".jsonl").split(".", 1)
        try:
            written, lines = path.stat().st_mtime, path.read_text().splitlines()
        except FileNotFoundError:
            continue
        if written > started - FARM_QUIET_SECONDS:
            busy.add(current.get(FunctionName(written_as), FunctionName(written_as)))
        for number, line in enumerate(lines):
            try:
                row = cast(dict[str, object], json.loads(line))
            except json.JSONDecodeError:
                continue
            code = row.get("code")
            if not row.get("ok") and isinstance(code, str) and code.strip():
                add(written_as, model, code, f"{path.relative_to(REPO)} round {row.get('round')}",
                    str(row.get("report") or ""), (written, number), after_review=row.get("after_review") is True)
    return Pool(candidates=found, farm_busy=busy, from_retests=from_retests)


def distinct(candidates: list[Candidate]) -> tuple[Candidate, ...]:
    """One candidate per code (after renaming): a passed one before a failed one, then the newest."""
    kept: dict[str, Candidate] = {}
    for c in sorted(candidates, key=lambda c: (c.passed_when_written, c.newness), reverse=True):
        _ = kept.setdefault(c.code.strip(), c)
    return tuple(kept.values())


def lifted_names(cport: Path) -> set[FunctionName]:
    """Every function of the lifted code (its `/* NAME: START-END` headers)."""
    pattern = re.compile(r"^/\* (\w+): [0-9A-F]{8}-", re.M)
    return {FunctionName(n) for path in (cport / "src/lifted").glob("*.c")
            for n in cast(list[str], pattern.findall(path.read_text()))}


# ── drops: why convert runs took functions out of farm groups ─────────

DROPS = REPO / "symbols/pool_drops.tsv"
DROP_COLUMNS = ("time", "run", "step", "function", "written_as", "kind", "cause", "reason", "report")


class DropKind(Enum):
    """Which check took a function out of its farm group (convert.py's failing_functions)."""
    LOCKSTEP = "lockstep set-aside"       # a route differed and no check named a function: half the group went
    ORACLE = "oracle"                     # the CPU oracle in the full verify
    DIFFTEST = "difftest in the verify"
    QUICK_TEST = "quick test"
    COMPILE = "compile with the group"
    BUILD = "full build"
    REVIEW = "review"
    OTHER_ENTRIES = "other entries"       # its code defined entries the farm did not write (an earlier rule)
    DEFINED_TWICE = "defined twice"
    NO_DRAFT = "no drafted entry"
    NO_CALLS = "no captured calls"
    LEFTOVERS = "leftover read"           # an old-prompt group: reads a register a callee leaves behind
    OTHER = "other"


_DROP_KINDS = ((r"^set aside", DropKind.LOCKSTEP), (r"CPU oracle", DropKind.ORACLE),
               (r"^difftest found", DropKind.DIFFTEST), (r"^quick test", DropKind.QUICK_TEST),
               (r"^does not compile", DropKind.COMPILE), (r"^the full build failed", DropKind.BUILD),
               (r"^taken out at review", DropKind.REVIEW), (r"other entries the farm did not write", DropKind.OTHER_ENTRIES),
               (r"must be defined exactly once|also defines", DropKind.DEFINED_TWICE),
               (r"no drafted", DropKind.NO_DRAFT), (r"no captured calls", DropKind.NO_CALLS),
               (r"^leftover read", DropKind.LEFTOVERS))


class Drop(Result):
    time: str                  # when the convert run took it out
    run: str                   # the convert run: ~/.burr/recomp-convert/RUN
    step: str                  # the step whose check named it: quick_test, install_and_verify, review_conversion
    function: FunctionName     # today's name
    written_as: FunctionName
    kind: DropKind
    cause: str                 # an oracle drop: its class in tools/oracle_triage.md, or what the difference looks like
    reason: str                # as the run logged it
    report: str                # the failing check's own report (oracle, verify difftest, lockstep), one line


def drop_kind(reason: str) -> DropKind:
    return next((kind for pattern, kind in _DROP_KINDS if re.search(pattern, reason)), DropKind.OTHER)


# tools/oracle_triage.md's classes, by the functions its table names (names of 2026-10-08; renames are followed)
TRIAGE_NAMES: dict[str, tuple[str, ...]] = {
    "BIOS leftovers (one of the three unexplained: r2 = 0x500, r3 = -1)": ("ho_8034218", "sub_8034268", "sub_8039658"),
    "sound hardware": ("CgbSound", "SoundMain", "m4aSoundMain", "call_m4aSoundMain", "sub_3005D78"),
    "sub_30068E8's flags (fixed)": ("sub_30068E8", "sub_801C416", "sub_801C9E4", "sub_801CDEC", "sub_801D564",
                                    "sub_802FE28", "sub_802AEB8", "sub_811FA98", "sub_81267A8", "sub_8127374",
                                    "sub_812907C", "sub_8134548"),
    "hand-written, narrower contract": ("sub_3006028", "sub_30061E8", "sub_3006440", "sub_3006920",
                                        "copyTo_iObjectAttr3001D70_3006814", "sub_814E260"),
    "callers of the hand-written ones": ("sub_8002694", "sub_8003E18", "sub_8004218", "sub_8004510"),
    "serial I/O": ("libSIOControlStart", "libSIO814469C"),
    "IWRAM data relocated to ROM": ("sub_802C5E6", "sub_802C646"),
    "random input out of domain": ("getPrimaryElementWeaknessMultipler_3007432", "sub_814E4CC"),
}
BIOS_LEFTOVERS = {0x08: "Sqrt", 0x0A: "ArcTan2", 0x0B: "CpuSet", 0x0C: "CpuFastSet", 0x11: "LZ77UnCompWram",
                  0x12: "LZ77UnCompVram"}   # the BIOS routines whose r1/r3 the host's SWI emulation left different
_REGISTER = re.compile(r"\b(r\d+|lr|sp) lifted (0x[0-9a-f]+|\d+) original (0x[0-9a-f]+|\d+)")


def oracle_cause(function: FunctionName, differences: list[str], differed_too: set[FunctionName],
                 code: dict[str, Assembly], current: dict[FunctionName, FunctionName]) -> str:
    """The class in tools/oracle_triage.md that matches, else "unmatched: " and what the difference looks like.
    DIFFERED_TOO: the other functions the same verify run's oracle saw differ."""
    def named(names: tuple[str, ...]) -> set[str]:
        return {current.get(FunctionName(n), n) for n in names}
    for cause, names in TRIAGE_NAMES.items():
        if function in named(names):
            return cause
    registers = [(m[1], int(m[2], 0), int(m[3], 0)) for text in differences for m in _REGISTER.finditer(text)]
    memory = any(text.startswith("MEM:") for text in differences)
    flags = any("flags lifted" in text for text in differences)
    if registers and not memory and {*registers} <= {("r2", 0x500, 0), ("r3", 0xFFFFFFFF, 0)}:
        return "BIOS leftovers (r2 = 0x500, r3 = -1, as the three unexplained ones)"
    own = code.get(function)
    callees: set[str] = set(own.calls) if own else set()
    bios = sorted({BIOS_LEFTOVERS[n] for a in (own, *map(code.get, callees)) if a for n in a.bios if n in BIOS_LEFTOVERS})
    if bios and registers and not memory:
        return f"BIOS leftovers (registers after {', '.join(bios)})"
    hand_written = sorted(callees & named(TRIAGE_NAMES["hand-written, narrower contract"]))
    if hand_written and registers and not memory:
        return f"callers of the hand-written ones (calls {', '.join(hand_written)})"
    sound = sorted(callees & named(TRIAGE_NAMES["sound hardware"]))
    if sound:
        return f"sound hardware (calls {', '.join(sound)})"
    if flags and not registers and not memory and callees & named(("sub_30068E8", "sub_802FE28")):
        return "sub_30068E8's flags (fixed)"
    if any(0x08000000 <= port < 0x0A000000 and 0x03000000 <= original < 0x03008000 for _, port, original in registers):
        return "IWRAM data relocated to ROM"
    also = sorted(callees & differed_too)
    if also:
        return f"unmatched: a callee differed too ({', '.join(also)})"
    if registers and not memory and all(port in (original & 0xFF, original & 0xFFFF) for _, port, original in registers):
        return "unmatched: upper bits only (the readable code returns a narrower value)" + (", and flags" if flags else "")
    if flags and not registers and not memory:
        return "unmatched: flags only"
    return "unmatched: memory differs" if memory else "unmatched: registers differ"


_END_ENTRY = re.compile(rb'^\{"type": ?"end_entry"')
_ACTION = re.compile(rb'"action": ?"(\w+)"')


def _json(value: object) -> dict[str, object]:
    return cast(dict[str, object], value) if isinstance(value, dict) else {}


def _list(value: object) -> list[object]:
    return cast(list[object], value) if isinstance(value, list) else []


_verify_states: dict[str, dict[str, object]] = {}


def verify_state(run: str) -> dict[str, object]:
    """A verify run's last state (every check's per-function results), from its Burr log."""
    if run not in _verify_states:
        state: dict[str, object] = {}
        try:
            with open(burr_dir() / "recomp-verify" / run / "log.jsonl", "rb") as log:
                for line in log:
                    if _END_ENTRY.match(line):
                        try:
                            state = _json(_json(cast(object, json.loads(line))).get("state")) or state
                        except json.JSONDecodeError:
                            continue
        except FileNotFoundError:
            pass
        _verify_states[run] = state
    return _verify_states[run]


def check_report(kind: DropKind, function: str, attempt: dict[str, object]) -> tuple[str, list[str], set[FunctionName]]:
    """The failing check's report for one dropped function (one line), the oracle's differences, and which
    functions the same oracle run saw differ."""
    verification = _json(attempt.get("verification"))
    verify = verify_state(str(attempt.get("verify_run") or ""))
    if kind is DropKind.LOCKSTEP:
        failures = [str(f) for f in _list(verification.get("route_failures"))]
        return "; ".join(failures) or "routes differed: " + ", ".join(str(r) for r in _list(verification.get("failed_routes"))), [], set()
    if kind is DropKind.DIFFTEST:
        parts: list[str] = []
        for field, what in (("difftests", "random inputs"), ("captured_difftests", "captured calls")):
            for result in map(_json, _list(verify.get(field))):
                if result.get("function") == function:
                    total = result.get("trials", result.get("runs"))
                    parts.append(f"{what}: {result.get('outcome')}, {result.get('failures')} of {total} differ "
                                 f"({result.get('out_of_domain')} out of domain)")
        return "; ".join(parts) or "(the verify run's log has no difftest result for it)", [], set()
    if kind is DropKind.ORACLE:
        results = [_json(r) for r in _list(verify.get("oracle"))]
        differed = {FunctionName(str(r.get("function"))) for r in results if cast(int, r.get("differed") or 0) > 0}
        mine = next((r for r in results if r.get("function") == function), None)
        if mine is None:
            return "(the verify run's log has no oracle result for it)", [], differed
        differences = [str(d) for d in _list(mine.get("differences"))]
        return (f"{mine.get('differed')} of {mine.get('compared')} states differ: " + " | ".join(differences[:2]),
                differences, differed - {FunctionName(function)})
    return "-", [], set()


def burr_drops(current: dict[FunctionName, FunctionName], code: dict[str, Assembly]) -> list[Drop]:
    """Every function a convert run took out of a farm group, from the Burr logs. Only the drop steps' lines
    are parsed (one log is over a gigabyte); a step's state lists every drop of the run so far, so a drop is
    new the first time its function and reason appear in the run."""
    found: list[Drop] = []
    for log in sorted((burr_dir() / "recomp-convert").glob("*/log.jsonl")):
        run, previous, seen = log.parent.name, "", set[tuple[str, str]]()
        with open(log, "rb") as lines:
            for line in lines:
                if not _END_ENTRY.match(line):
                    continue
                action = _ACTION.search(line[:300])
                if action is None or action[1] != b"drop_failing_functions":
                    previous = action[1].decode() if action else previous
                    continue
                try:
                    entry = _json(cast(object, json.loads(line)))
                except json.JSONDecodeError:
                    continue   # a step being written now
                state = _json(entry.get("state"))
                attempts = [_json(a) for a in _list(state.get("attempts"))]
                failing = attempts[-2] if len(attempts) > 1 else (attempts[-1] if attempts else {})
                for dropped in map(_json, _list(_json(entry.get("result")).get("dropped") or state.get("dropped"))):
                    written_as, reason = FunctionName(str(dropped.get("function"))), str(dropped.get("reason") or "")
                    if (written_as, reason) in seen:
                        continue
                    seen.add((written_as, reason))
                    kind, today = drop_kind(reason), current.get(written_as, written_as)
                    report, differences, differed = check_report(kind, written_as, failing)
                    cause = (oracle_cause(today, differences, {current.get(f, f) for f in differed}, code, current)
                             if kind is DropKind.ORACLE and differences else "-")
                    found.append(Drop(time=str(entry.get("end_time") or "")[:19], run=run, step=previous or "?",
                                      function=today, written_as=written_as, kind=kind, cause=cause,
                                      reason=reason, report=report))
    return found


def one_line(text: str, limit: int = 400) -> str:
    text = re.sub(r"\s+", " ", text).strip()
    return text if len(text) <= limit else text[:limit - 3] + "..."


def read_drop_ledger(current: dict[FunctionName, FunctionName]) -> list[Drop]:
    """symbols/pool_drops.tsv as written last time, under today's names."""
    if not DROPS.exists():
        return []
    rows = [line.split("\t") for line in DROPS.read_text().splitlines() if line and not line.startswith("#")]
    drops: list[Drop] = []
    for cells in rows[1:]:
        fields = dict(zip(DROP_COLUMNS, cells))
        written_as = FunctionName(fields["written_as"])
        drops.append(Drop(time=fields["time"], run=fields["run"], step=fields["step"],
                          function=current.get(written_as, written_as), written_as=written_as,
                          kind=DropKind(fields["kind"]), cause=fields["cause"], reason=fields["reason"],
                          report=fields["report"]))
    return drops


def drop_ledger(current: dict[FunctionName, FunctionName], code: dict[str, Assembly]) -> list[Drop]:
    """The ledger's rows with every drop the Burr logs still hold (theirs win: read again, classified anew),
    written back to symbols/pool_drops.tsv. Running again changes nothing."""
    fresh = burr_drops(current, code)
    runs = {d.run for d in fresh}
    drops = sorted([d for d in read_drop_ledger(current) if d.run not in runs] + fresh,
                   key=lambda d: (d.time, d.run, d.written_as))
    header = ("# symbols/pool_drops.tsv: every function a convert run took out of a farm group, and why (from the "
              "Burr logs, ~/.burr/recomp-convert and recomp-verify; rows of runs no longer there are kept); made by "
              "tools/convert/pool.py")
    rows = ["\t".join(one_line(cell) for cell in (d.time, d.run, d.step, d.function, d.written_as, d.kind.value, d.cause,
                                                  d.reason, d.report)) for d in drops]
    _ = DROPS.write_text("\n".join([header, "\t".join(DROP_COLUMNS), *rows]) + "\n")
    return drops


# ── re-testing ─────────────────────────────────────────────────────────

def use_tree(cport: Path, difftest: Path) -> None:
    """Point farm.py at one tree: it reads lifted C, headers and host/difftest.cfg from its HOST, so
    candidates compile against the same commit the difftest was built from."""
    old = cast(str, FARM.HOST)
    setattr(FARM, "CFLAGS", [flag.replace(f"-I{old}/", f"-I{cport}/") for flag in cast(list[str], FARM.CFLAGS)])
    setattr(FARM, "HOST", str(cport))
    setattr(FARM, "DIFFTEST", str(difftest))


def facts(name: FunctionName, want_context: bool) -> tuple[InputKey | None, ConversionContext | None]:
    """farm.input_key and (for a re-test) the conversion context: both scan every lifted file, so a
    process pool runs them."""
    key = cast(str | None, FARM.input_key(name))  # pyright: ignore[reportAny]
    return (InputKey(key) if key else None), (conversion_context(name) if want_context else None)


def retest(function: FunctionName, candidate: Candidate, context: ConversionContext, snapshots: Path) -> Retest:
    """farm.test without its register check, and the register check on its own."""
    contract = {part: list(names) for part, names in context.contract.by_part().items()}
    with tempfile.TemporaryDirectory(prefix="pool-") as workdir:
        try:
            passed, report = cast(tuple[bool, str], FARM.test(function, candidate.code, str(snapshots),  # pyright: ignore[reportAny]
                                                              contract, workdir, check_registers=False,
                                                              leftovers=True))
        except subprocess.TimeoutExpired:
            passed, report = False, "difftest timed out (900 s)"
        problems = cast(str, FARM.register_problems(function, candidate.code, workdir))  # pyright: ignore[reportAny]
    verdict, reason, share = judge(passed, report)
    bare = blank_comments_and_strings(candidate.code)[0]
    return Retest(candidate=candidate, verdict=verdict, reason=reason, failing_share=share,
                  register_violations=sum(1 for line in problems.splitlines() if line.startswith("- ")),
                  register_references=sum(len(p.findall(bare)) for p in REGISTER_CATEGORIES.values()))


_CALLS = re.compile(r"\((\d+) snapshots, (\d+) runs, (\d+) failures, (\d+) out-of-domain\)")
_TRIALS = re.compile(r"\((\d+) trials, (\d+) failures, (\d+) out-of-domain\)")


def judge(passed: bool, report: str) -> tuple[Verdict, str, float]:
    """farm.test's report as a verdict, one short line, and the share of comparisons that differed. The
    report lists every problem since the typed-2 test; the verdict is still that of the first failing check
    in the order the test once stopped at (farm.first_failure), so verdicts keep their meaning."""
    kind, first = cast(tuple[str | None, str], FARM.first_failure(report))  # pyright: ignore[reportAny]
    if kind is not None:
        report = first
    lines = report.splitlines() or [""]
    if report.startswith("compile errors:"):
        error = re.sub(r"^line \d+: (?:fatal )?error: ", "", lines[1] if len(lines) > 1 else "?")
        return Verdict.COMPILE, f"compile: {error}", 1.0
    if report.startswith("the code must define"):
        return Verdict.NO_ENTRY, lines[0], 1.0
    if report.startswith("guest registers used"):
        return Verdict.STYLE, "register check", 1.0
    if report.startswith("leftover reads:"):
        return Verdict.STYLE, "leftover check (tools/abi/leftovers.py --deep)", 1.0
    if report.startswith("the code still uses"):
        return Verdict.STYLE, lines[0].replace("the code still uses machine-translated style", "lifted style"), 1.0
    if re.match(r"line \d+: ", report):
        return Verdict.STYLE, "review rule: " + re.sub(r"^line \d+: ", "", lines[0]), 1.0
    captured = next((line for line in lines if line.startswith("captured calls:")), "")
    random = next((line for line in lines if line.startswith("random inputs:")), "")
    calls, trials = _CALLS.search(captured), _TRIALS.search(random)
    if passed:
        return Verdict.PASSED, (f"ok on {calls[2]} captured calls and {trials[1]} random inputs"
                                if calls and trials else "ok"), 0.0
    if "every captured call was out of domain" in report:
        return Verdict.NOTHING_COMPARED, "every captured call was out of domain", 1.0
    if calls and int(calls[3]):
        return Verdict.CAPTURED_CALLS, f"{calls[3]} of {calls[2]} captured calls differ", int(calls[3]) / max(1, int(calls[2]))
    if trials and int(trials[2]):
        return Verdict.RANDOM_INPUTS, f"{trials[2]} of {trials[1]} random inputs differ", int(trials[2]) / max(1, int(trials[1]))
    other = re.sub(r"/\S+/cand\.so: ", "", captured or random or lines[0])
    return Verdict.OTHER, re.sub(r"^(captured calls|random inputs): \w+: ", "", other)[:120], 1.0


def failure_kind(report: str) -> str:
    """A failure report in a few words: its verdict, or the kind of compile error, style or other failure."""
    verdict, reason, _ = judge(False, report)
    return reason.split(":")[0] if verdict in (Verdict.COMPILE, Verdict.STYLE, Verdict.OTHER) else verdict.value


def link_snapshots(functions: list[Function], current: dict[FunctionName, FunctionName], captured: Path,
                   into: Path) -> Path:
    """INTO/NAME -> the captured calls of NAME in CAPTURED, found under its name or any earlier one
    (captures are named as the function was named then)."""
    into.mkdir(parents=True, exist_ok=True)
    earlier: dict[FunctionName, list[FunctionName]] = {}
    for old, new in current.items():
        earlier.setdefault(new, []).append(old)
    for f in functions:
        found = next((captured / n for n in (f.name, *f.written_as, *earlier.get(f.name, [])) if (captured / n).is_dir()),
                     None)
        if found is not None and not (into / f.name).exists():
            (into / f.name).symlink_to(found.resolve())
    return into


def write_accepted(outcome: Outcome, started: float, from_retests: set[Path]) -> Outcome:
    """The chosen passing candidate as tools/convert/accepted/NAME.MODEL.c, unless an accepted file
    exists under NAME (the farm then already counts the function done) or the farm has started on it.
    An earlier re-test's file with the same code counts as this run's (running again changes nothing)."""
    function, chosen = outcome.function, outcome.best
    if chosen is None or chosen.verdict is not Verdict.PASSED:
        return outcome
    # as the farm keeps a passing candidate: with the generated wrappers it calls and the tool's register-form
    # notes (a typed-2 candidate calls call_NAME wrappers its own code does not define)
    code = cast(str, FARM.finished(function.name, chosen.candidate.code)).strip()  # pyright: ignore[reportAny]
    existing = sorted(ACCEPTED_DIR.glob(f"{function.name}.*.c"))
    earlier = [p for p in existing if p in from_retests and p.read_text().strip() in (code, chosen.candidate.code.strip())]
    if earlier:
        return outcome.model_copy(update={"written": earlier[0]})
    if existing:
        names = [str(p.relative_to(REPO)) for p in existing]
        kept = () if chosen.candidate.source in names else (f"kept the accepted file {names[0]}",)
        return outcome.model_copy(update={"notes": (*outcome.notes, *kept)})
    if any(p.stat().st_mtime > started - FARM_QUIET_SECONDS for n in (function.name, *function.written_as)
           for p in LOG_DIR.glob(f"{n}.*.jsonl")):
        return outcome.model_copy(update={"notes": (*outcome.notes, "not written: the farm started on it")})
    path = ACCEPTED_DIR / f"{function.name}.{chosen.candidate.model}.c"
    try:
        with open(path, "x") as out:
            _ = out.write(code + "\n")
    except FileExistsError:
        return outcome
    return outcome.model_copy(update={"written": path, "written_now": True})


# ── the ledger ─────────────────────────────────────────────────────────

COLUMNS = ("current_name", "original_names", "input_key", "state", "candidates", "retest", "chosen",
           "register_violations", "models", "notes", "asm_lines", "installed_by", "drops", "last_drop", "drop_kind",
           "drop_cause")


class Beside(NamedTuple):
    """What pool.tsv shows beside the re-test: sizes, who wrote what is installed, and the drops."""
    sizes: dict[str, Assembly]
    written_by: dict[str, str]
    drops: dict[FunctionName, list[Drop]]   # today's name -> its drops, oldest first


def installed_share(function: Function, installed: Path) -> float:
    """How much of the function's closest candidate (a passing one if any) is in the file it is
    installed in, line by line: a low share means it went in with another body."""
    def lines(code: str) -> set[str]:
        bare = blank_comments_and_strings(code)[0]
        return {" ".join(line.split()) for line in bare.splitlines() if line.strip() not in ("", "{", "}")}
    there = lines(installed.read_text())
    pool = [c for c in function.candidates if c.passed_when_written] or list(function.candidates)
    return max((len(lines(c.code) & there) / max(1, len(lines(c.code))) for c in pool), default=0.0)


def row(outcome: Outcome, beside: Beside) -> list[str]:
    function, best = outcome.function, outcome.best
    if best is None:
        retest_text = f"not run: {outcome.not_run}"
    else:
        passing = sum(1 for r in outcome.retests if r.verdict is Verdict.PASSED)
        word = "passed" if passing else "failed"
        retest_text = f"{word} ({passing}/{len(outcome.retests)}): {best.reason}"
    chosen = best if best is not None and best.verdict is Verdict.PASSED else None
    chosen_path = (str(outcome.written.relative_to(REPO)) if outcome.written is not None
                   else chosen.candidate.source if chosen else "-")
    models = sorted({c.model.replace("_", "/", 1) for c in function.candidates})
    notes = list(outcome.notes)
    if chosen is not None and prompt_of(chosen.candidate.model) is not PromptKind.TYPED:
        notes.append(f"chosen candidate: {prompt_of(chosen.candidate.model).value} prompt, register form allowed "
                     "(an install queues it for retype: tools/abi/retype_queue.tsv)")
    if function.installed_in is not None and outcome.installed_share is not None:
        notes.insert(0, f"installed in {function.installed_in.name}; {outcome.installed_share:.0%} of the "
                        "closest candidate's lines are there")
    size, drops = beside.sizes.get(function.name), beside.drops.get(function.name, [])
    last = drops[-1] if drops else None
    cells = [function.name, ",".join(function.written_as), outcome.input_key or "-", outcome.state_after.value,
             str(len(function.candidates)), retest_text, chosen_path,
             str(chosen.register_violations) if chosen else "-", ",".join(models), "; ".join(notes) or "-",
             str(size.lines) if size else "-",
             beside.written_by.get(function.name, "-") if function.installed_in is not None else "-",
             str(len(drops)) if drops else "-", one_line(f"{last.run} {last.step}: {last.reason}", 300) if last else "-",
             last.kind.value if last else "-", last.cause if last else "-"]
    return [re.sub(r"\s+", " ", cell).strip() for cell in cells]


def write_pool(outcomes: list[Outcome], tested_commit: str | None, beside: Beside) -> None:
    how = (f"re-tested against {tested_commit} without model calls" if tested_commit else "not re-tested")
    header = (f"# symbols/pool.tsv: the model farm's candidates per function under today's name (renames from "
              f"symbols/db), {how}; made by tools/convert/pool.py on {date.today().isoformat()}")
    rows = sorted((row(o, beside) for o in outcomes), key=lambda cells: (cells[0].lower(), cells[0]))
    _ = POOL.write_text("\n".join([header, "\t".join(COLUMNS), *("\t".join(r) for r in rows)]) + "\n")


def summarise(outcomes: list[Outcome], seconds: float) -> None:
    before = Counter(o.function.state.value for o in outcomes)
    after = Counter(o.state_after.value for o in outcomes)
    print(f"\n{len(outcomes)} functions with farm candidates ({sum(len(o.function.candidates) for o in outcomes)} "
          f"distinct candidates), {seconds / 60:.1f} min")
    for state in State:
        print(f"  {state.value:10s} before {before[state.value]:5d}   after {after[state.value]:5d}")
    tested = [o for o in outcomes if o.retests]
    skipped = Counter(o.not_run for o in outcomes if o.not_run and o.function.state is not State.INSTALLED)
    print(f"re-tested: {len(tested)} functions, {sum(len(o.retests) for o in tested)} candidates; not re-tested: "
          + (", ".join(f"{count} ({why})" for why, count in skipped.most_common()) or "none"))
    failed = [o for o in tested if o.function.state is State.FAILED]
    pending = [o for o in tested if o.function.state is State.PENDING]
    gained = [o for o in failed if o.passes_now and o.best]
    print(f"yield: {len(gained)} of {len(failed)} failed functions pass now "
          f"({sum(o.best.register_violations == 0 for o in gained if o.best)} with a typed candidate); "
          f"{sum(o.passes_now for o in pending)} of {len(pending)} pending ones still pass")
    then = Counter(failure_kind(o.best.candidate.report_then) for o in gained if o.best)
    print("  what the chosen candidates of the newly passing functions failed on when written: "
          + ", ".join(f"{count} {why}" for why, count in then.most_common()))
    print(f"accepted files from re-tests: {sum(o.written is not None for o in outcomes)} "
          f"({sum(o.written_now for o in outcomes)} written by this run)")
    typed = [o for o in tested if o.passes_now and o.best and o.best.register_violations == 0]
    print(f"passing functions whose chosen candidate is typed (no register violations): {len(typed)}")
    still = [o.best for o in tested if not o.passes_now and o.best]
    print("why the rest still fail (best candidate per function):")
    for verdict, count in Counter(r.verdict for r in still).most_common():
        print(f"  {count:4d}  {verdict.value}")
    errors = Counter(re.sub(r"'[^']*'|‘[^’]*’", "'X'", r.reason) for r in still if r.verdict is Verdict.COMPILE)
    for reason, count in errors.most_common(5):
        print(f"         {count:4d}  {reason[:100]}")
    lost = [o.function.name for o in pending if not o.passes_now]
    if lost:
        print(f"pending functions with no candidate passing now: {', '.join(lost[:12])}{' ...' if len(lost) > 12 else ''}")
    unlike = [o for o in outcomes if o.installed_share is not None and o.installed_share < 0.5]
    print(f"installed functions with under half of their closest candidate's lines in the installed file: {len(unlike)}")


def summarise_drops(outcomes: list[Outcome], beside: Beside) -> None:
    """The drop ledger in counts: every drop by kind, and why the functions not installed were last dropped."""
    every = [d for ds in beside.drops.values() for d in ds]
    print(f"\ndrops (symbols/pool_drops.tsv): {len(every)} from {len({d.run for d in every})} convert runs, "
          f"{len(beside.drops)} functions")
    functions = {o.function.name: o for o in outcomes}
    for kind, count in Counter(d.kind for d in every).most_common():
        names = {d.function for d in every if d.kind is kind}
        waiting = sum(1 for n in names if n in functions and functions[n].state_after is not State.INSTALLED)
        print(f"  {count:4d} drops of {len(names):4d} functions  {kind.value}  ({waiting} not installed now)")
    last = {name: ds[-1] for name, ds in beside.drops.items()
            if name in functions and functions[name].state_after is not State.INSTALLED}
    print(f"not installed now, by the latest drop: " + ", ".join(
        f"{count} {kind.value}" for kind, count in Counter(d.kind for d in last.values()).most_common()))
    oracle = {d.function: d for d in every if d.kind is DropKind.ORACLE}   # each function's latest oracle drop
    print(f"oracle drops by cause ({len(oracle)} functions; classes of tools/oracle_triage.md where they match):")
    for cause, count in Counter(re.sub(r" \(calls .*|\(.*", "", d.cause).strip() for d in oracle.values()).most_common():
        print(f"  {count:4d}  {cause}")
    missing = sorted(n for n in beside.drops if n not in functions)
    if missing:
        print(f"dropped functions with no farm candidate in the pool: {', '.join(missing[:10])}")


# ── the run ────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=(__doc__ or "").split("\n\n")[0])
    _ = parser.add_argument("--jobs", type=int, default=16)
    _ = parser.add_argument("--snaps", type=Path, default=SNAPSHOTS)
    _ = parser.add_argument("--no-retest", action="store_true", help="only list the pool")
    args = parser.parse_args()
    jobs, retesting, captured = cast(int, args.jobs), not cast(bool, args.no_retest), cast(Path, args.snaps).resolve()
    started = time.time()
    run_dir = RUNS / f"pool-{time.strftime('%Y%m%d-%H%M%S')}"
    run_dir.mkdir(parents=True)

    cport, difftest, commit = REPO / "cport", REPO / "cport/build-host/difftest", None
    if retesting:
        tree = committed_tree("pool")
        built = build_host_difftest(run_dir, build_dir="build-wf-pool-host", tree=tree)
        if isinstance(built, BuildFailure):
            sys.exit(f"the difftest did not build, see {built.log}")
        cport, difftest = tree / "cport", built
        commit = subprocess.run(["git", "-C", str(tree), "rev-parse", "--short", "HEAD"], capture_output=True,
                                text=True, check=True).stdout.strip()
        use_tree(cport, difftest)

    current, installed, today = renames(), installed_functions(), lifted_names(cport)
    code = assembly()
    drops: dict[FunctionName, list[Drop]] = {}
    for drop in drop_ledger(current, code):
        drops.setdefault(drop.function, []).append(drop)
    beside = Beside(sizes=code, written_by=writers(current), drops=drops)
    pool = read_candidates(current, today)
    functions = [Function(name=name, written_as=tuple(sorted({c.written_as for c in cs})), candidates=distinct(cs),
                          installed_in=installed.get(name), farm_busy=name in pool.farm_busy)
                 for name, cs in pool.candidates.items()]
    print(f"{len(functions)} functions, {len(pool.farm_busy)} busy with the live farm; reading their lifted code",
          flush=True)

    snapshots = link_snapshots(functions, current, captured, run_dir / "snapshots")
    def why_not(f: Function) -> str:
        if f.state is State.INSTALLED:
            return "installed"
        if not retesting:
            return "--no-retest"
        if f.farm_busy:
            return "the farm wrote its log in the last 10 minutes"
        if f.name not in today:
            return "not a function of the lifted code"
        return "" if (snapshots / f.name).is_dir() else "no captured calls"
    not_run = {f.name: why_not(f) for f in functions}

    with ProcessPoolExecutor(jobs, initializer=use_tree, initargs=(cport, difftest)) as processes:
        known = dict(zip((f.name for f in functions),
                         processes.map(facts, [f.name for f in functions], [not not_run[f.name] for f in functions],
                                  chunksize=8)))
    for f in functions:
        if not not_run[f.name] and known[f.name][1] is None:
            not_run[f.name] = "no lifted code or assembly"

    work = [(f, c) for f in functions if not not_run[f.name] for c in f.candidates]
    print(f"re-testing {len(work)} candidates of {len({f.name for f, _ in work})} functions, {jobs} at a time", flush=True)
    done: dict[FunctionName, list[Retest]] = {}
    def run(pair: tuple[Function, Candidate]) -> Retest:
        function, candidate = pair
        return retest(function.name, candidate, cast(ConversionContext, known[function.name][1]), snapshots)
    with ThreadPoolExecutor(jobs) as threads:
        tests = threads.map(run, work)
        for count, ((f, _), result) in enumerate(zip(work, tests), 1):
            done.setdefault(f.name, []).append(result)
            if count % 200 == 0:
                print(f"  {count}/{len(work)} ({time.time() - started:.0f} s)", flush=True)

    rejected = Counter(p.name.split(".", 1)[0] for p in REJECTED_DIR.glob("*.c"))
    outcomes: list[Outcome] = []
    for f in functions:
        notes: list[str] = []
        if any(rejected[n] for n in (f.name, *f.written_as)):
            notes.append(f"{sum(rejected[n] for n in (f.name, *f.written_as))} rejected at review (tools/convert/rejected)")
        outcome = Outcome(function=f, input_key=known[f.name][0], retests=tuple(done.get(f.name, ())),
                          not_run=not_run[f.name], notes=tuple(notes),
                          installed_share=installed_share(f, f.installed_in) if f.installed_in else None)
        if outcome.retests and f.state is State.FAILED and outcome.passes_now:
            outcome = outcome.model_copy(update={"notes": (*outcome.notes, "newly passing (was failed)")})
        if outcome.retests and f.state is State.PENDING and not outcome.passes_now:
            outcome = outcome.model_copy(update={"notes": (*outcome.notes, "no candidate passes against the last commit")})
        outcomes.append(write_accepted(outcome, started, pool.from_retests))
    write_pool(outcomes, commit, beside)
    summarise(outcomes, time.time() - started)
    summarise_drops(outcomes, beside)
    print(f"\nwrote {POOL.relative_to(REPO)}; run files in {run_dir.relative_to(REPO)}")


if __name__ == "__main__":
    main()
