#!/usr/bin/env python3
"""The farm's pool: every candidate the model farm ever wrote, gathered per function under its
current name, re-tested without any model call, and listed in symbols/pool.tsv (stage S0 of
docs/reviews/2026-10-10-4-joint-plan.md; review 1 Area 1, review 3 Area 2 A8).

  tools/wf/.venv/bin/python tools/convert/pool.py [--jobs 16] [--snaps DIR] [--no-retest]

Candidates: tools/convert/accepted/FUNC.MODEL.c (each passed when it was written) and every
distinct code a failed round logged in tools/convert/log/FUNC.MODEL.jsonl.

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
from typing import Callable, NewType, cast

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/wf"))   # before farm.py puts tools/ first (tools/wf would shadow wf)
sys.path.insert(0, str(REPO / "tools/abi"))

import yaml  # noqa: E402

import lint  # noqa: E402  # pyright: ignore[reportMissingImports]  # tools/abi/lint.py: its register categories
from wf.repo import RUNS  # noqa: E402
from wf.tools.build import build_host_difftest, committed_tree  # noqa: E402
from wf.tools.models import ACCEPTED_DIR, FARM, LOG_DIR, CandidateC, ConversionContext, conversion_context  # noqa: E402
from wf.types import BuildFailure, FunctionName, Result  # noqa: E402

# tools/abi/lint.py's pieces: code with comments and strings blanked, and its register-level categories
blank_comments_and_strings = cast(Callable[[str], tuple[str, object]], lint.strip)  # pyright: ignore[reportUnknownMemberType]
REGISTER_CATEGORIES = cast(dict[str, re.Pattern[str]], lint.CATEGORIES)  # pyright: ignore[reportUnknownMemberType]

ModelTag = NewType("ModelTag", str)    # a model as file names carry it: "hyper_glm-5.3-flash"
InputKey = NewType("InputKey", str)    # farm.input_key: a hash of the function's original bytes

POOL = REPO / "symbols/pool.tsv"
DATABASE = REPO / "symbols/db"
GAME = REPO / "cport/src/game"
REJECTED_DIR = REPO / "tools/convert/rejected"
SNAPSHOTS = REPO / "build-wf-runs/snapshots-latest"
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

def renames() -> dict[FunctionName, FunctionName]:
    """OLD -> today's name, from symbols/db's previous_names (see the module docstring)."""
    current: dict[FunctionName, FunctionName] = {}
    for path in sorted(DATABASE.glob("*.yaml")):
        entry = cast(dict[str, object], yaml.safe_load(path.read_text()))
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
    (its code is a failed log row's), so the function still counts as failed when farmed."""
    started = time.time()
    found: dict[FunctionName, list[Candidate]] = {}
    busy: set[FunctionName] = set()
    from_retests: set[Path] = set()

    def add(written_as: str, model: str, code: str, source: str, report: str | None, newness: tuple[float, int]) -> None:
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
            add(written_as, model, path.read_text(), str(path.relative_to(REPO)), None, (path.stat().st_mtime, 0))
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
                    str(row.get("report") or ""), (written, number))
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
                                                              contract, workdir, check_registers=False))
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
    """farm.test's report as a verdict, one short line, and the share of comparisons that differed."""
    lines = report.splitlines() or [""]
    if report.startswith("compile errors:"):
        error = re.sub(r"^line \d+: (?:fatal )?error: ", "", lines[1] if len(lines) > 1 else "?")
        return Verdict.COMPILE, f"compile: {error}", 1.0
    if report.startswith("the code must define"):
        return Verdict.NO_ENTRY, lines[0], 1.0
    if report.startswith("guest registers used"):
        return Verdict.STYLE, "register check", 1.0
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
    existing = sorted(ACCEPTED_DIR.glob(f"{function.name}.*.c"))
    earlier = [p for p in existing if p in from_retests and p.read_text().strip() == chosen.candidate.code.strip()]
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
            _ = out.write(chosen.candidate.code.strip() + "\n")
    except FileExistsError:
        return outcome
    return outcome.model_copy(update={"written": path, "written_now": True})


# ── the ledger ─────────────────────────────────────────────────────────

COLUMNS = ("current_name", "original_names", "input_key", "state", "candidates", "retest", "chosen",
           "register_violations", "models", "notes")


def installed_share(function: Function, installed: Path) -> float:
    """How much of the function's closest candidate (a passing one if any) is in the file it is
    installed in, line by line: a low share means it went in with another body."""
    def lines(code: str) -> set[str]:
        bare = blank_comments_and_strings(code)[0]
        return {" ".join(line.split()) for line in bare.splitlines() if line.strip() not in ("", "{", "}")}
    there = lines(installed.read_text())
    pool = [c for c in function.candidates if c.passed_when_written] or list(function.candidates)
    return max((len(lines(c.code) & there) / max(1, len(lines(c.code))) for c in pool), default=0.0)


def row(outcome: Outcome) -> list[str]:
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
    if function.installed_in is not None and outcome.installed_share is not None:
        notes.insert(0, f"installed in {function.installed_in.name}; {outcome.installed_share:.0%} of the "
                        "closest candidate's lines are there")
    cells = [function.name, ",".join(function.written_as), outcome.input_key or "-", outcome.state_after.value,
             str(len(function.candidates)), retest_text, chosen_path,
             str(chosen.register_violations) if chosen else "-", ",".join(models), "; ".join(notes) or "-"]
    return [re.sub(r"\s+", " ", cell).strip() for cell in cells]


def write_pool(outcomes: list[Outcome], tested_commit: str | None) -> None:
    how = (f"re-tested against {tested_commit} without model calls" if tested_commit else "not re-tested")
    header = (f"# symbols/pool.tsv: the model farm's candidates per function under today's name (renames from "
              f"symbols/db), {how}; made by tools/convert/pool.py on {date.today().isoformat()}")
    rows = sorted((row(o) for o in outcomes), key=lambda cells: (cells[0].lower(), cells[0]))
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
    write_pool(outcomes, commit)
    summarise(outcomes, time.time() - started)
    print(f"\nwrote {POOL.relative_to(REPO)}; run files in {run_dir.relative_to(REPO)}")


if __name__ == "__main__":
    main()
