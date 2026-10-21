"""The shared work queue's pieces (review 3 Area 5.3-5, joint plan S2): which functions are left to farm,
which prompt and which models each one goes to by its size, and the ledger of jobs that every queue run
shares (build-wf-state/queue-jobs.json).

A job is one function under one prompt version: it keeps that prompt for every model it goes to, and is
finished when a candidate passes, when every model of its route has tried it, or when it has spent its
token cap (review 3 Area 2: about 60k a function; past it the function waits for the mechanical pass or
the strong tier). When the farm's prompt changes version, a job still open under the old one is retired
and the function gets a new job.

A candidate a review rejected (tools/convert/rejected, with its .review.txt) is a verdict on that model's
attempt: the model is never asked afresh for the function (its candidate is not silently replaced), but gets
one retry that starts from the rejected code with the reviewer's reason as the failure report, under the
job's caps; a job that had ended is opened again for it.

Routes: the typed prompt for every size (plan B, the old prompt for 11-30 lines, ended 2026-10-12: docs/reviews/
2026-10-12-knowledge-first.md). Models within a route are in the order to try; a provider's lane takes
first the jobs whose next model is its own, then (rather than idle) jobs where its model comes later."""
import fcntl
import json
import os
import re
import time
from collections.abc import Generator, Iterable
from contextlib import contextmanager
from enum import Enum
from pathlib import Path
from typing import cast

from pydantic import BaseModel, TypeAdapter

from wf.repo import CPORT, SNAPSHOTS, STATE_DIR
from wf.tools.budgets import Band, PromptVersion, renames, size_of
from wf.tools.locks import held
from wf.tools.models import (ACCEPTED_DIR, LOG_DIR, PromptKind, base_model, prompt_version, rejected_at_review,
                             with_prompt)
from wf.tools.sources import provider
from wf.types import FunctionName, ModelSpec, Result

LEDGER = STATE_DIR / "queue-jobs.json"
STOP = STATE_DIR / "queue-stop"        # `wf queue --stop`: lanes take no new jobs; the run ends once they finish
_LOCK = "queue"
GAME = CPORT / "src/game"
CAP = 60_000                            # tokens a job may spend over all its models


def _models(*names: str) -> tuple[ModelSpec, ...]:
    return tuple(ModelSpec(name) for name in names)


class Route(Result):
    prompt: PromptKind
    models: tuple[ModelSpec, ...]   # in the order to try


# Hyper's credit lapses every day at 10:13 and is lost if unused, MiniMax's plan ends 10-13, Z.ai's week resets
# 10-14, OpenCode's 10-16: within each route, the cheapest good model first, then quota that lapses soonest.
ROUTES: dict[Band, Route] = {
    Band.TINY: Route(prompt=PromptKind.TYPED, models=_models(
        "hyper/glm-5.3-flash", "minimax/MiniMax-M3", "hyper/qwen3.8-flash", "hyper/deepseek-v4.1-flash", "hyper/kimi-k2.5", "hyper/minimax-m3",
        "zai/glm-5.3-flash", "opencode/glm-5.3-flash")),
    Band.SMALL: Route(prompt=PromptKind.TYPED, models=_models(
        "minimax/MiniMax-M3", "hyper/glm-5.3-flash", "hyper/qwen3.8-flash", "hyper/deepseek-v4.1-flash", "hyper/kimi-k2.5", "hyper/minimax-m3",
        "zai/glm-5.3-flash", "opencode/glm-5.3-flash")),
    Band.MEDIUM: Route(prompt=PromptKind.TYPED, models=_models(
        "minimax/MiniMax-M3", "hyper/glm-5.3-flash", "hyper/qwen3.8-flash", "hyper/deepseek-v4.1-flash", "hyper/kimi-k2.5", "hyper/minimax-m3",
        "zai/glm-5.3-flash", "opencode/qwen3.8-flash")),
    Band.LARGE: Route(prompt=PromptKind.TYPED, models=_models(
        "hyper/qwen3.8-flash", "hyper/glm-5.3-flash", "minimax/MiniMax-M3", "hyper/deepseek-v4.1-flash", "hyper/kimi-k2.5", "hyper/minimax-m3",
        "zai/glm-5.3", "opencode/qwen3.8-flash")),
    Band.LARGER: Route(prompt=PromptKind.TYPED, models=_models(
        "hyper/qwen3.8-flash", "minimax/MiniMax-M3", "hyper/glm-5.3-flash", "zai/glm-5.3",
        "hyper/deepseek-v4.1-flash", "opencode/qwen3.8-flash")),
    Band.HUGE: Route(prompt=PromptKind.TYPED, models=_models(   # review 3's strong tier
        "minimax/MiniMax-M3", "zai/glm-5.3", "hyper/qwen3.8-flash")),
}


class Lane(Result):
    """One provider's workers: how many farm runs it keeps going at once."""
    provider: str
    workers: int


# Hyper has no pacer (its hourly limit answers 429 "try again in ..."); the others' pacers let 2 requests at a
# time reach the provider, so a few more workers only keep their queue full.
LANES: tuple[Lane, ...] = (Lane(provider="hyper", workers=16), Lane(provider="minimax", workers=4),
                          Lane(provider="zai", workers=2), Lane(provider="opencode", workers=2))


class JobState(Enum):
    OPEN = "open"
    ACCEPTED = "accepted"            # a candidate passed (tools/convert/accepted)
    EVERY_MODEL_TRIED = "every model tried"
    OVER_CAP = "spent its token cap"
    NOTHING_TO_CONVERT = "nothing to convert"   # no captured calls, lifted code or assembly
    RETIRED = "prompt version retired"          # the farm's prompt changed version while it was open


class Job(BaseModel):
    function: FunctionName
    prompt: PromptKind
    version: PromptVersion
    lines: int
    models: tuple[ModelSpec, ...]          # its route's, when it began
    tried: tuple[ModelSpec, ...] = ()      # models that finished a farm run on it
    errors: int = 0                        # farm runs that ended in a provider failure (not counted as tried)
    names: tuple[str, ...] = ()            # today's name and the earlier ones: where a review may be filed
    review_retries: tuple[ModelSpec, ...] = ()   # models a review rejected, owed their one retry with it
    reviewed: tuple[ModelSpec, ...] = ()   # models that had that retry
    spent: int = 0                         # tokens over every call (input and output, estimates included)
    state: JobState = JobState.OPEN
    accepted: Path | None = None
    began: str = ""
    updated: str = ""

    def remaining(self) -> tuple[ModelSpec, ...]:
        """The models still to ask, in route order: those not tried, and those owed a retry with a review."""
        return tuple(model for model in self.models if model not in self.tried or model in self.review_retries)

    @property
    def band(self) -> Band:
        return Band.of(self.lines)


Ledger = dict[FunctionName, Job]
_LEDGER = TypeAdapter(dict[FunctionName, Job])


def read_ledger(path: Path | None = None) -> Ledger:
    try:
        return _LEDGER.validate_json((path or LEDGER).read_text())
    except (OSError, ValueError):
        return {}


def update_ledger(jobs: Iterable[Job], path: Path | None = None) -> Ledger:
    """Write these jobs into the shared ledger (under its lock); the ledger as it is now."""
    path = path or LEDGER
    with held(_LOCK):
        ledger = read_ledger(path)
        ledger.update({job.function: job for job in jobs})
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix(".tmp")
        _ = temporary.write_bytes(_LEDGER.dump_json(ledger, indent=1))
        temporary.replace(path)
        return ledger


@contextmanager
def only_one_queue() -> Generator[None]:
    """Hold the queue's run lock for the run (a second `wf queue` would double every job): exits if held."""
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    with open(STATE_DIR / "queue-run.lock", "a") as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise SystemExit("a queue is running already (build-wf-state/queue-run.lock): `wf queue --stop` first")
        yield


def now() -> str:
    return time.strftime("%Y-%m-%d %H:%M:%S")


# ── what is left to farm ───────────────────────────────────────────────

class Leftover(Result):
    """A function no candidate has passed for, with captured calls to test one against."""
    function: FunctionName
    lines: int
    attempts: int                  # (function, model, prompt) pairs the farm has logged for it, under any name
    logs: tuple[Path, ...] = ()    # those pairs' logs (tools/convert/log/NAME.MODEL[+old].jsonl)
    names: tuple[str, ...] = ()    # today's name and the earlier ones (symbols/db previous_names)


def installed_functions(game: Path) -> set[FunctionName]:
    """Each function cport/src/game defines a `Next NAME(void)` body for."""
    pattern = re.compile(r"\bNext\s+(\w+)\s*\(\s*void\s*\)\s*\{")
    return {FunctionName(match[1]) for path in game.glob("*.c") for match in pattern.finditer(path.read_text())}


def leftovers(game: Path, snapshots: Path = SNAPSHOTS, accepted_dir: Path = ACCEPTED_DIR,
              log_dir: Path = LOG_DIR) -> list[Leftover]:
    """Functions with captured calls that are neither installed nor accepted under any of their names (a
    passing candidate waits for install: symbols/pool.tsv's pending), with their size and how often the farm
    has tried them; fewest tries first, then smallest."""
    current = {FunctionName(old): FunctionName(new) for old, new in renames().items()}

    def today(name: str) -> FunctionName:
        return current.get(FunctionName(name), FunctionName(name))

    done = installed_functions(game) | {today(path.name.split(".", 1)[0]) for path in accepted_dir.glob("*.c")}
    logs: dict[FunctionName, list[Path]] = {}
    for path in log_dir.glob("*.jsonl"):
        logs.setdefault(today(path.name.split(".", 1)[0]), []).append(path)
    earlier: dict[FunctionName, list[str]] = {}
    for old, new in current.items():
        earlier.setdefault(new, []).append(old)
    found: list[Leftover] = []
    for folder in snapshots.iterdir():
        name = FunctionName(folder.name)
        if name in done or name != today(name) or not folder.is_dir():
            continue
        lines = size_of(name)
        if lines is None or not any(folder.iterdir()):
            continue
        found.append(Leftover(function=name, lines=lines, attempts=len(logs.get(name, [])),
                              logs=tuple(logs.get(name, [])), names=(name, *sorted(earlier.get(name, [])))))
    return sorted(found, key=lambda leftover: (leftover.attempts, leftover.lines, leftover.function))


def new_job(leftover: Leftover, routes: dict[Band, Route] = ROUTES) -> Job:
    route = routes[Band.of(leftover.lines)]
    version = PromptVersion(prompt_version(route.prompt))
    tried = tried_in_logs(leftover.logs, route.models, route.prompt, version)
    job = Job(function=leftover.function, prompt=route.prompt, version=version, lines=leftover.lines,
              models=route.models, tried=tried, names=leftover.names, began=now(), updated=now())
    job = with_reviews(job)
    return job.model_copy(update={"state": JobState.OPEN if job.remaining() else JobState.EVERY_MODEL_TRIED})


def with_reviews(job: Job, cap: int = CAP) -> Job:
    """The job with the retries it owes models a review rejected (each once): a model with a review counts
    as tried (it is never asked afresh) and is owed its retry; a job that had ended without a pass is opened
    again for one, under its token cap."""
    owed = tuple(model for model in job.models if model not in job.reviewed
                 and rejected_at_review(job.names or (job.function,), with_prompt(model, job.prompt)) is not None)
    if owed == job.review_retries:
        return job
    tried = (*job.tried, *(model for model in owed if model not in job.tried))
    updated = job.model_copy(update={"review_retries": owed, "tried": tried, "updated": now()})
    if owed and job.state in (JobState.ACCEPTED, JobState.EVERY_MODEL_TRIED) and job.spent < cap:
        updated = updated.model_copy(update={"state": JobState.OPEN, "accepted": None})
    return updated


def tried_in_logs(logs: Iterable[Path], models: Iterable[ModelSpec], prompt: PromptKind,
                  version: PromptVersion) -> tuple[ModelSpec, ...]:
    """The models of a route that some farm run (any process: `wf farm` batches too) has already asked about
    the function under this prompt version: a logged round that is not a provider failure."""
    by_file = {path.name.split(".", 1)[1].removesuffix(".jsonl"): path for path in logs}
    found: list[ModelSpec] = []
    for model in models:
        path = by_file.get(str(with_prompt(model, prompt)).replace("/", "_", 1))
        if path is None:
            continue
        for row in _rows(path):
            report = row.get("report")
            if isinstance(report, str) and report.startswith("model error"):
                continue
            logged = row.get("prompt_version")
            if prompt is PromptKind.OLD or logged == version:   # an old-prompt log holds only old-prompt rounds
                found.append(model)
                break
    return tuple(found)


def _rows(path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    try:
        lines = path.read_text(errors="replace").splitlines()
    except OSError:
        return rows
    for line in lines:
        try:
            row = cast(object, json.loads(line))
        except json.JSONDecodeError:
            continue
        if isinstance(row, dict):
            rows.append(cast(dict[str, object], row))
    return rows


def plan(ledger: Ledger, found: list[Leftover], routes: dict[Band, Route] = ROUTES) -> tuple[list[Job], list[Job]]:
    """(the open jobs in the order to take them, the jobs that changed: new ones and retired ones). A job
    still open under a prompt version that is no longer the farm's is retired; a function whose job ended
    without a pass under another version than its route's today gets a new job."""
    changed: list[Job] = []
    order: list[Job] = []
    for leftover in found:
        job = ledger.get(leftover.function)
        route_prompt = routes[Band.of(leftover.lines)].prompt
        if job is not None and job.state is JobState.OPEN and (job.version != prompt_version(job.prompt)
                                                                or job.prompt is not route_prompt):
            job = job.model_copy(update={"state": JobState.RETIRED, "updated": now()})
            changed.append(job)
        today = prompt_version(routes[Band.of(leftover.lines)].prompt)
        if job is None or job.state not in (JobState.OPEN, JobState.ACCEPTED) and job.version != today:
            job = new_job(leftover, routes)
            changed.append(job)
        else:
            before = job
            if job.state is JobState.OPEN:   # other farms may have asked some of its models since
                elsewhere = tried_in_logs(leftover.logs, tuple(m for m in job.models if m not in job.tried),
                                          job.prompt, job.version)
                if elsewhere:
                    job = job.model_copy(update={"tried": (*job.tried, *elsewhere), "updated": now()})
            job = with_reviews(job.model_copy(update={"names": job.names or leftover.names}))
            if job.state is JobState.OPEN and not job.remaining():
                job = job.model_copy(update={"state": JobState.EVERY_MODEL_TRIED})
            if job != before:
                changed.append(job)
        if job.state is JobState.OPEN and job.remaining():
            order.append(job)
    return order, changed


# ── who is busy with what ──────────────────────────────────────────────

def busy_elsewhere(own_pid: int | None = None) -> set[FunctionName]:
    """Functions other running farms are on: every `wf farm` process's function arguments and the
    benchmark's --only lists (read from /proc), so the queue never doubles them."""
    own_pid = own_pid or os.getpid()
    busy: set[FunctionName] = set()
    for status in Path("/proc").glob("[0-9]*/cmdline"):
        try:
            argv = status.read_bytes().split(b"\0")
        except OSError:
            continue
        if int(status.parent.name) == own_pid:
            continue
        words = [word.decode(errors="replace") for word in argv if word]
        if "farm" in words and any(word.endswith("wf") or word == "wf" for word in words[:4]):
            busy.update(FunctionName(word) for word in words[words.index("farm") + 1:]
                        if not word.startswith("-") and re.fullmatch(r"\w+", word))
        if any(word.endswith("bench.py") for word in words) and "--only" in words:
            only = words[words.index("--only") + 1] if words.index("--only") + 1 < len(words) else ""
            busy.update(FunctionName(name) for name in only.split(",") if name)
    return busy


def recently_logged(log_dir: Path = LOG_DIR, seconds: float = 600) -> set[FunctionName]:
    """Functions whose farm log changed lately (a run of another process may be on them)."""
    since = time.time() - seconds
    found: set[FunctionName] = set()
    for path in log_dir.glob("*.jsonl"):
        try:
            if path.stat().st_mtime > since:
                found.add(FunctionName(path.name.split(".", 1)[0]))
        except FileNotFoundError:
            continue
    return found


def pick(order: list[Job], lane: str, in_flight: set[FunctionName], busy: set[FunctionName],
         up: set[str]) -> tuple[Job, ModelSpec] | None:
    """The next job for LANE's provider, and the model to ask: first a job whose next model (skipping
    providers that are down) is the lane's own, then any job with one of its models left."""
    for strict in (True, False):
        for job in order:
            if job.function in in_flight or job.function in busy:
                continue
            remaining = [model for model in job.remaining() if provider(model) in up]
            ours = [model for model in remaining if provider(model) == lane]
            if not ours:
                continue
            if strict and remaining[0] != ours[0]:
                continue
            return job, ours[0]
    return None


def after_run(job: Job, model: ModelSpec, accepted: Path | None, provider_failed: bool, spent: int,
              nothing_to_convert: bool = False, cap: int = CAP, outage: bool = False) -> Job:
    """The job once MODEL's farm run on it is over. PROVIDER_FAILED: the run ended on a provider error, so the
    model has not had its try; OUTAGE: that error was the provider's quota or availability (wf/tools/sources.py
    is_source_failure), which does not count against the job."""
    tried = job.tried if provider_failed or model in job.tried else (*job.tried, model)
    errors = job.errors + 1 if provider_failed and not outage else job.errors
    if provider_failed and not outage and errors % 3 == 0 and model not in tried:
        tried = (*tried, model)   # a model that keeps failing on this job (not a quota outage) is passed over
    retried = model in job.review_retries and not provider_failed   # its one retry with the review is spent
    updated = job.model_copy(update={
        "tried": tried, "errors": errors, "spent": job.spent + spent, "updated": now(),
        "review_retries": tuple(m for m in job.review_retries if not (retried and m == model)),
        "reviewed": (*job.reviewed, model) if retried else job.reviewed})
    if accepted is not None:
        state = JobState.ACCEPTED
    elif nothing_to_convert:
        state = JobState.NOTHING_TO_CONVERT
    elif updated.spent >= cap:
        state = JobState.OVER_CAP
    elif not updated.remaining():
        state = JobState.EVERY_MODEL_TRIED
    else:
        state = JobState.OPEN
    return updated.model_copy(update={"state": state, "accepted": accepted})


# ── what a run spent ───────────────────────────────────────────────────

def usage_offset(usage_log: Path) -> int:
    try:
        return usage_log.stat().st_size
    except FileNotFoundError:
        return 0


def tokens_spent(usage_log: Path, since_offset: int, function: FunctionName, model: ModelSpec) -> int:
    """Input and output tokens of the calls logged for FUNCTION by MODEL since the offset (stopped and cut-off
    calls by their estimates)."""
    spent = 0
    asked = str(base_model(model))
    try:
        with open(usage_log, "rb") as log:
            _ = log.seek(since_offset)
            lines = log.read().decode(errors="replace").splitlines()
    except FileNotFoundError:
        return 0
    for line in lines:
        try:
            row = cast(object, json.loads(line))
        except json.JSONDecodeError:
            continue
        if not isinstance(row, dict):
            continue
        fields = cast(dict[str, object], row)
        tag = fields.get("tag")
        if fields.get("model") != asked or not (fields.get("function") == function or
                                                (isinstance(tag, str) and tag.startswith(f"{function} round "))):
            continue
        for key in ("in", "out"):
            value = fields.get(key)
            spent += value if isinstance(value, int) else 0
    return spent
