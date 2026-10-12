"""The hand-over (review 3 Area 2 step 4 and B31; part of the typed prompt typed-2): a repair, and the next
model to try a function, start from the best attempt so far rather than from the last one, with a one-line
history of the others. The ranking is farm.py's rank(): passed > behaves the same but breaks a form rule >
differs only on random inputs > differs on fewer captured calls > compares nothing > does not compile >
no entry point; a reply without code is never an attempt to start from."""
import functools
import json
import re
import sys
import tempfile
from collections.abc import Callable, Sequence
from pathlib import Path
from types import ModuleType
from typing import cast

import yaml

from wf.repo import REPO, SNAPSHOTS
from wf.tools.models import (ACCEPTED_DIR, FARM, LOG_DIR, CandidateC, ConversionContext, Prompt, PromptKind,
                             first_prompt, repair_prompt)
from wf.types import FunctionName, Result, ToolMessage

HISTORY_LENGTH = 5     # attempts named in the history line, best first


class Tried(Result):
    """One attempt at a function, as today's acceptance test sees it."""
    code: CandidateC
    passed: bool
    report: ToolMessage
    whose: str                         # "your last attempt", "your round 0", "hyper/glm-5.3-flash's round 2"
    rank: tuple[int, float, int]       # farm.py rank(): smaller is better

    @property
    def verdict(self) -> str:
        return cast(str, FARM.verdict(self.passed, self.report))  # pyright: ignore[reportAny]


def tried(code: CandidateC, passed: bool, report: str, whose: str) -> Tried:
    """An attempt with its test result, ranked."""
    rank = cast(tuple[int, float, int], FARM.rank(passed, report))  # pyright: ignore[reportAny]
    return Tried(code=code, passed=passed, report=ToolMessage(report), whose=whose, rank=rank)


class Start(Result):
    """Where the next round starts: the best attempt so far, and the others in one line, best first."""
    best: Tried
    history: str


def best_start(attempts: Sequence[Tried]) -> Start | None:
    """The best attempt and a history of the others (at most HISTORY_LENGTH); None when there is none."""
    if not attempts:
        return None
    ordered = sorted(attempts, key=lambda a: a.rank)    # stable: an earlier attempt first among equals
    return Start(best=ordered[0], history="; ".join(f"{a.whose}: {a.verdict}" for a in ordered[1:HISTORY_LENGTH + 1]))


def start_prompt(context: ConversionContext, attempts: Sequence[Tried], prompt: PromptKind = PromptKind.TYPED) -> Prompt:
    """The first prompt when there is nothing to start from; else the repair prompt on the best attempt,
    with the history line (what the farm's ask step sends; the benchmark asks with it too)."""
    start = best_start(attempts)
    if start is None:
        return first_prompt(context, prompt)
    return repair_prompt(context, start.best.report, start.best.code, prompt, whose=start.best.whose,
                         history=start.history)


# ── what other models (and earlier runs) wrote ─────────────────────────────────────────────────────

def earlier_attempts(context: ConversionContext, difftest: Path, leave_out: frozenset[str] = frozenset(),
                     snapshots: Path = SNAPSHOTS) -> tuple[Tried, ...]:
    """Every distinct candidate written for the function before (tools/convert/accepted and the farm's logs,
    under its name and its earlier ones, renamed to today's names, as tools/convert/pool.py reads them),
    except those filed under a model tag in LEAVE_OUT ("minimax_MiniMax-M3", "minimax_MiniMax-M3+old"),
    tested with today's acceptance test, best first. One that passes it is left out: it is pool.py's
    (pending, for an install), not a start for a repair."""
    found: dict[str, tuple[str, str]] = {}            # code -> (model tag, where), newest first
    current, today = _names()
    rename = cast(Callable[[str, dict[str, str], set[str]], str], _pool().renamed)  # pyright: ignore[reportAny]
    for name in (context.function, *_previous_names(context.function)):
        for model, where, code in _written(name):
            if model in leave_out:
                continue
            _ = found.setdefault(rename(code, current, today).strip(), (model, where))
    setattr(FARM, "DIFFTEST", str(difftest))
    contract = {part: list(names) for part, names in context.contract.by_part().items()}
    attempts: list[Tried] = []
    for code, (model, where) in found.items():
        with tempfile.TemporaryDirectory() as workdir:
            try:
                passed, report = cast(tuple[bool, str], FARM.test(context.function, code, str(snapshots),  # pyright: ignore[reportAny]
                                                                  contract, workdir, leftovers=True))
            except Exception as error:   # a difftest timeout: not a start
                passed, report = False, f"not tested: {error}"[:300]
        if passed:
            continue
        attempts.append(tried(CandidateC(code), passed, report, f"{_model_spec(model)}'s {where}"))
    return tuple(sorted(attempts, key=lambda a: a.rank))


def _written(name: str) -> list[tuple[str, str, str]]:
    """(model tag, where, code) of every candidate written for NAME: its accepted files, then its log rows,
    newest first."""
    out: list[tuple[str, str, str]] = []
    for path in sorted(ACCEPTED_DIR.glob(f"{name}.*.c"), key=lambda p: p.stat().st_mtime, reverse=True):
        out.append((path.stem.split(".", 1)[1], "accepted candidate", path.read_text()))
    for path in sorted(LOG_DIR.glob(f"{name}.*.jsonl")):
        model = path.name.removesuffix(".jsonl").split(".", 1)[1]
        rows: list[tuple[str, str, str]] = []
        for line in path.read_text().splitlines():
            try:
                row = cast(dict[str, object], json.loads(line))
            except json.JSONDecodeError:
                continue
            code = row.get("code")
            if isinstance(code, str) and code.strip():
                rows.append((model, f"round {row.get('round')}", code))
        out += reversed(rows)
    return out


def _model_spec(tag: str) -> str:
    return tag.replace("_", "/", 1)


def _previous_names(function: FunctionName) -> list[str]:
    path = REPO / "symbols/db" / f"{function}.yaml"
    if not path.exists():
        return []
    entry = cast(dict[str, object], yaml.safe_load(path.read_text()) or {})
    return [str(name) for name in cast(list[object], entry.get("previous_names") or [])
            if re.fullmatch(r"\w+", str(name))]


@functools.cache
def _pool() -> ModuleType:
    sys.path.insert(0, str(REPO / "tools/convert"))
    import pool  # pyright: ignore[reportMissingImports]
    return cast(ModuleType, pool)


@functools.cache
def _names() -> tuple[dict[str, str], set[str]]:
    """pool.py's rename record (old name -> today's) and today's lifted names, read once per process."""
    pool = _pool()
    current = cast(dict[str, str], pool.renames())  # pyright: ignore[reportAny]
    today = cast(set[str], pool.lifted_names(REPO / "cport"))  # pyright: ignore[reportAny]
    return current, today
