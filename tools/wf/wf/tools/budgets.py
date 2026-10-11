"""Reasoning budgets: how much hidden reasoning a farm reply may spend before any code begins. Past it the
reply is stopped in the stream (tools/llm.py reasoning_budget), logged as a runaway with the tokens it
used, and the function goes on to another model (review 3 Area 1; runaways were 41-45% of output tokens).

A budget is the 95th percentile of the reasoning that PASSING replies used, per (model, prompt version,
size band), measured from the farm's own logs (tools/convert/log, one row per round) joined to llm.py's
usage log (tools/convert/usage.jsonl, one row per call, with the provider's reasoning-token count):
  1. the band's own, from 30 passing replies or more;
  2. else the larger of the version's over every band (30 or more) and the band's own (5 or more);
  3. else the same for the prompt kind (typed-1 and typed-2 together);
  4. else no budget: the output cap alone.
A model whose provider reports no reasoning count (MiniMax-M3) gets none; a model never farmed gets review 3's
starting point where it gave one. Budgets are recomputed whenever
a process asks for them after BUDGETS_LAST seconds, so they follow the data."""
import json
import os
import re
import sys
import threading
import time
from collections import defaultdict
from collections.abc import Callable, Iterator
from enum import Enum
from pathlib import Path
from typing import NamedTuple, NewType, Protocol, cast

from wf.repo import REPO
from wf.tools.models import LOG_DIR, USAGE_LOG, PromptKind, prompt_version
from wf.types import FunctionName, ModelSpec, Result

sys.path.insert(0, str(REPO / "tools/convert"))
import scorecard  # noqa: E402  # pyright: ignore[reportMissingImports]  # tools/convert/scorecard.py: sizes, renames

PromptVersion = NewType("PromptVersion", str)   # "old", "typed-1", "typed-2" (farm.py PROMPT_VERSION)
ModelId = NewType("ModelId", str)               # llm.model_id: one name per model across providers

TYPED_PROMPT_SINCE = "2026-10-10 07:39"   # 329fbfd: the farm's prompt became the typed one (typed-1)
MIN_PASSES = 30           # a band's own percentile needs this many passing replies
MIN_BAND_PASSES = 5       # below MIN_PASSES, a band's own percentile still counts from this many
PERCENTILE = 0.95
FLOOR = 4000              # never stop a reply before this much reasoning
# models with no farm rounds yet: review 3 Area 1's starting points (glm and qwen have their own data)
STARTING_POINTS: dict[str, int] = {"deepseek-v4.1-flash": 24_000}
BUDGETS_LAST = 3 * 3600   # seconds before a process recomputes its budgets


class Band(Enum):
    """Function size in assembly instruction lines (tools/convert/scorecard.py's measure)."""
    TINY = "<=10"
    SMALL = "11-20"
    MEDIUM = "21-30"
    LARGE = "31-40"
    LARGER = "41-60"
    HUGE = ">60"

    @staticmethod
    def of(lines: int) -> "Band":
        for top, band in ((10, Band.TINY), (20, Band.SMALL), (30, Band.MEDIUM), (40, Band.LARGE), (60, Band.LARGER)):
            if lines <= top:
                return band
        return Band.HUGE


class FarmRound(Result):
    """One farm round with its call's token counts."""
    model: ModelId
    version: PromptVersion
    function: FunctionName
    passed: bool
    reasoning: int | None   # what the provider reported (None: it reports none)


class Budget(Result):
    tokens: int | None      # None: no budget, the output cap alone
    basis: str              # how it was found, for the logs and the docs


def kind_of(version: PromptVersion) -> PromptKind:
    return PromptKind.OLD if version == "old" else PromptKind.TYPED


# ── the data ───────────────────────────────────────────────────────────

class _UsageKey(NamedTuple):
    spec: str
    function: str
    round: int


def _json_lines(path: Path) -> Iterator[dict[str, object]]:
    try:
        lines = path.read_text(errors="replace").splitlines()
    except OSError:
        return
    for line in lines:
        try:
            row = cast(object, json.loads(line))
        except json.JSONDecodeError:
            continue
        if isinstance(row, dict):
            yield cast(dict[str, object], row)


def _reasoning_by_round(usage_log: Path) -> dict[_UsageKey, list[int | None]]:
    """Each farm call's reported reasoning tokens, in log order, by (model asked, function, round)."""
    found: dict[_UsageKey, list[int | None]] = defaultdict(list)
    for row in _json_lines(usage_log):
        tag, spec = row.get("tag"), row.get("model")
        if not isinstance(tag, str) or not isinstance(spec, str) or row.get("error") or row.get("stopped"):
            continue
        function, _, number = tag.rpartition(" round ")
        if not number.isdigit():
            continue
        reasoning = row.get("reasoning")
        found[_UsageKey(spec, function, int(number))].append(reasoning if isinstance(reasoning, int) else None)
    return found


def _version_of(row: dict[str, object], tagged_old: bool) -> PromptVersion:
    version = row.get("prompt_version")
    if isinstance(version, str):
        return PromptVersion(version)
    if tagged_old or row.get("prompt") == "old":
        return PromptVersion("old")
    when = row.get("time")
    typed = bool(row.get("prompt_hash")) or (isinstance(when, str) and when.replace("T", " ") >= TYPED_PROMPT_SINCE)
    return PromptVersion("typed-1" if typed else "old")   # before the typed prompt: the old one


def farm_rounds(log_dir: Path = LOG_DIR, usage_log: Path = USAGE_LOG) -> list[FarmRound]:
    """Every answered farm round (with code, failed or passed) that a usage row can be matched to: the k-th
    round N of a (function, model) pair, in time order, is its k-th call tagged "FUNC round N"."""
    reasoning = _reasoning_by_round(usage_log)
    taken: dict[_UsageKey, int] = defaultdict(int)
    rounds: list[FarmRound] = []
    logs = sorted(log_dir.glob("*.jsonl"))
    rows_by_pair: dict[tuple[str, str], list[tuple[str, bool, dict[str, object]]]] = defaultdict(list)
    for path in logs:
        function, _, tag = path.name.removesuffix(".jsonl").partition(".")
        spec, _, mark = tag.replace("_", "/", 1).partition("+")
        for row in _json_lines(path):
            kind = row.get("kind")
            if isinstance(kind, str) and kind != "farm":
                continue   # the benchmark's rows: their calls are tagged apart
            when = row.get("time")
            rows_by_pair[(function, spec)].append((when if isinstance(when, str) else "", mark == "old", row))
    for (function, spec), rows in rows_by_pair.items():
        for _, tagged_old, row in sorted(rows, key=lambda item: item[0]):   # rows without a time are the oldest
            number = row.get("round")
            if not isinstance(number, int):
                continue
            key = _UsageKey(spec, function, number)
            calls = reasoning.get(key, [])
            index = taken[key]
            taken[key] += 1
            report = row.get("report")
            if index >= len(calls) or (isinstance(report, str) and report.startswith("model error")):
                continue
            rounds.append(FarmRound(model=ModelId(spec.split("/", 1)[-1].lower()), version=_version_of(row, tagged_old),
                                    function=FunctionName(function), passed=row.get("ok") is True,
                                    reasoning=calls[index]))
    return rounds


# ── the table ──────────────────────────────────────────────────────────

class _Cell(NamedTuple):
    model: ModelId
    group: str          # a prompt version, or a prompt kind's value
    band: Band | None   # None: every band


def _percentile(values: list[int]) -> int:
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, round(PERCENTILE * (len(ordered) - 1)))]


class BudgetTable:
    """Passing replies' reasoning per (model, version or kind, band)."""

    def __init__(self, rounds: list[FarmRound], size_of: Callable[[FunctionName], int | None]):
        self.passes: dict[_Cell, list[int]] = defaultdict(list)
        self.reports: set[ModelId] = set()   # models whose provider reports reasoning
        for found in rounds:
            if found.reasoning is None:
                continue
            self.reports.add(found.model)
            if not found.passed:
                continue
            lines = size_of(found.function)
            band = Band.of(lines) if lines is not None else None
            for group in (found.version, kind_of(found.version).value):
                self.passes[_Cell(found.model, group, None)].append(found.reasoning)
                if band is not None:
                    self.passes[_Cell(found.model, group, band)].append(found.reasoning)

    def budget(self, model: ModelId, version: PromptVersion, band: Band | None) -> Budget:
        if model not in self.reports:
            if model in STARTING_POINTS:
                return Budget(tokens=STARTING_POINTS[model], basis=f"{model}: no farm data yet, review 3's starting point")
            return Budget(tokens=None, basis=f"{model}: no farm round with a reasoning count (none reported)")
        for group in (version, kind_of(version).value):
            own = self.passes.get(_Cell(model, group, band), []) if band is not None else []
            every = self.passes.get(_Cell(model, group, None), [])
            where = f"{model} {group} {band.value if band else 'size unknown'}"
            if len(own) >= MIN_PASSES:
                return self._made(_percentile(own), f"{where}: p95 of {len(own)} passes")
            found: list[tuple[int, str]] = []
            if len(every) >= MIN_PASSES:
                found.append((_percentile(every), f"{model} {group} every band: p95 of {len(every)} passes"))
            if len(own) >= MIN_BAND_PASSES:
                found.append((_percentile(own), f"{where}: p95 of {len(own)} passes"))
            if found:
                return self._made(*max(found))
        return Budget(tokens=None, basis=f"{model} {version}: too few passing replies; the cap alone")

    @staticmethod
    def _made(tokens: int, basis: str) -> Budget:
        return Budget(tokens=max(FLOOR, tokens), basis=basis + (f" (floor {FLOOR})" if tokens < FLOOR else ""))


# ── what the farm asks ─────────────────────────────────────────────────

_lock = threading.Lock()
_table_lock = threading.Lock()
_cached: tuple[float, BudgetTable] | None = None
_sizes: dict[FunctionName, int] = {}


class _Assembly(Protocol):
    """scorecard.Assembly: a function of bn6f/asm, as the scorecard counts its size."""
    @property
    def lines(self) -> int: ...


assembly = cast(Callable[[], dict[str, _Assembly]], scorecard.assembly)  # pyright: ignore[reportUnknownMemberType]
renames = cast(Callable[[], dict[str, str]], scorecard.renames)  # pyright: ignore[reportUnknownMemberType]


def size_of(function: FunctionName) -> int | None:
    """Assembly instruction lines, under today's name or an old one (symbols/db's previous_names)."""
    with _lock:
        if not _sizes:
            _sizes.update({FunctionName(name): size.lines for name, size in assembly().items()})
            _sizes.update({FunctionName(old): _sizes[FunctionName(new)] for old, new in renames().items()
                           if FunctionName(new) in _sizes})
    return _sizes.get(function)


def table() -> BudgetTable:
    global _cached
    with _table_lock:
        if _cached is None or time.time() - _cached[0] > BUDGETS_LAST:
            _cached = (time.time(), BudgetTable(farm_rounds(), size_of))
        return _cached[1]


def reasoning_budget(model: ModelSpec, prompt: PromptKind, function: FunctionName) -> Budget:
    """The budget for one farm call ($WF_NO_BUDGETS=1: none)."""
    if os.environ.get("WF_NO_BUDGETS"):
        return Budget(tokens=None, basis="budgets switched off ($WF_NO_BUDGETS)")
    lines = size_of(function)
    return table().budget(ModelId(re.sub(r"\+\w+$", "", model).split("/", 1)[-1].lower()),
                          PromptVersion(prompt_version(prompt)), Band.of(lines) if lines is not None else None)
