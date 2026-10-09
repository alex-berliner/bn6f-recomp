"""Every model source we pay for, and which of them take requests now. A request
goes to the first source of its tier that is up. When a source fails for reasons
of its own (a rate limit, a spent quota, timeouts, server errors), the failure is
recorded where every run sees it and the request moves on to the next source, so
the quota of every provider gets used before anything is reported as stuck. The
local pacers' logs (tools/llm.py blocked_until) add what the pacers saw."""
import json
import re
import time
from collections.abc import Callable
from enum import Enum
from time import sleep
from types import ModuleType
from typing import cast

from pydantic import TypeAdapter

from wf.repo import RUNS
from wf.tools.locks import held
from wf.tools.models import FARM
from wf.types import ModelSpec, Result


class Tier(Enum):
    STRONG = "strong"   # whole subsystems, written as typed C
    CHEAP = "cheap"     # small functions in volume, symbol drafts


# the order to try: the user's preference first; among the rest, quota that lapses soonest first
TIERS: dict[Tier, tuple[ModelSpec, ...]] = {
    Tier.STRONG: tuple(ModelSpec(model) for model in (
        "opencode/deepseek-v4-pro", "opencode/kimi-k3", "zai/glm-5.3",
        "minimax/MiniMax-M3", "hyper/minimax-m3", "opencode/qwen3.7-plus")),
    Tier.CHEAP: tuple(ModelSpec(model) for model in (
        "hyper/glm-5.3-flash", "hyper/deepseek-v4.1-flash", "hyper/qwen3.8-flash",   # Hyper's quota lapses daily
        "zai/glm-5.3-flash", "minimax/MiniMax-M2.7-highspeed",
        "opencode/glm-5.3-flash", "opencode/deepseek-v4.1-flash", "opencode/qwen3.8-flash")),
}
# models metered apart from their provider's plan: OpenCode's free previews keep answering while its
# weekly cap is spent (checked 2026-10-09), so an outage of the plan does not stop them, nor theirs it
OWN_QUOTA = frozenset(ModelSpec(model) for model in (
    "opencode/longcat-2.5-preview-free", "opencode/step-5-preview-free"))
TIER_PREFIX = "tier/"   # `tier/cheap` names a whole tier where a model is expected (wf farm --model tier/cheap)

STATE = RUNS / "sources.json"   # quota pool -> when it is expected back, and why it went down
_LOCK = "sources"


class Outage(Result):
    until: float   # epoch seconds
    why: str


def tier_of(model: ModelSpec) -> Tier | None:
    return Tier(model[len(TIER_PREFIX):]) if model.startswith(TIER_PREFIX) else None


def models_for(model: ModelSpec) -> tuple[ModelSpec, ...]:
    """The models a spec stands for: a tier's, in order, or the one model."""
    tier = tier_of(model)
    return TIERS[tier] if tier is not None else (model,)


def provider(model: ModelSpec) -> str:
    return model.split("/", 1)[0]


def quota_pool(model: ModelSpec) -> str:
    """What runs out together: the provider's plan, or the model itself when it is metered apart."""
    return model if model in OWN_QUOTA else provider(model)


def down_until(model: ModelSpec) -> float | None:
    """When the model's quota pool is expected to take requests again, if it is down now. (A pacer's log
    is the whole provider's, so it says nothing about a model metered apart.)"""
    now = time.time()
    pacer = None if model in OWN_QUOTA else pacer_down_until(model)
    recorded = _read().get(quota_pool(model))
    seen = [moment for moment in (pacer, recorded.until if recorded else None) if moment is not None and moment > now]
    return max(seen) if seen else None


def out_for_long(models: tuple[ModelSpec, ...], horizon: float = 7200) -> bool:
    """Every model's provider is down for longer than first_up waits."""
    backs = [down_until(model) for model in models]
    return all(back is not None and back - time.time() > horizon for back in backs)


def pacer_down_until(model: ModelSpec) -> float | None:
    """What the provider's local pacer saw (its log): a rate limit, or upstream errors in a row."""
    llm = cast(ModuleType, FARM.llm)  # pyright: ignore[reportAny]
    return cast(float | None, llm.blocked_until(model))  # pyright: ignore[reportAny]


def is_source_failure(error: str) -> bool:
    """A failure of the provider (not of the model's answer): worth another provider."""
    return bool(re.search(r"HTTP (?:429|402|5\d\d)|timed out|upstream error|Connection|quota|insufficient|balance"
                          r"|rate.?limit", error, re.I))


def report_failure(model: ModelSpec, error: str) -> None:
    """Record that the model's quota pool is down (for every run), for as long as the error says, or a while."""
    until = time.time() + _lasts(error)
    with held(_LOCK):
        state = _read()
        known = state.get(quota_pool(model))
        if known is None or known.until < until:
            state[quota_pool(model)] = Outage(until=until, why=error[:300])
        _ = STATE.write_text(json.dumps({name: outage.model_dump() for name, outage in state.items()}, indent=1))


def first_up[ReplyT](models: tuple[ModelSpec, ...], ask: Callable[[ModelSpec], ReplyT],
                     failure: Callable[[ReplyT], str | None], horizon: float = 7200) -> tuple[ModelSpec, ReplyT]:
    """Ask the first of `models` whose provider is up; a provider failure is recorded and the next one asked.
    When every one is down, wait for the provider back soonest, for up to `horizon` in all; past that, give
    back the last failure (or ask the first model anyway, for an error the caller can report)."""
    last: tuple[ModelSpec, ReplyT] | None = None
    deadline = time.time() + horizon
    while True:
        for model in models:
            if down_until(model) is not None:
                continue
            reply = ask(model)
            error = failure(reply)
            if error is None or not is_source_failure(error):
                return model, reply
            report_failure(model, error)
            last = (model, reply)
        backs = [moment for moment in (down_until(model) for model in models) if moment is not None]
        if len(backs) < len(models):
            continue   # a provider came back while the others were asked
        if min(backs) > deadline:
            return last if last is not None else (models[0], ask(models[0]))
        sleep(min(max(min(backs) - time.time(), 1.0) + 5, 600))


def _lasts(error: str) -> float:
    """How long a failure keeps its provider down: what the provider said, or a guess by the kind of failure."""
    said = re.search(r"retry after (\d+)s", error)
    if said:
        return float(said[1])
    llm = cast(ModuleType, FARM.llm)  # pyright: ignore[reportAny]
    lifts = cast(int | None, llm._limit_lifts_in(error))  # pyright: ignore[reportAny, reportPrivateUsage]
    if lifts:
        return float(lifts)
    if re.search(r"HTTP 402|quota|insufficient|balance", error, re.I):
        return 3600.0
    if "HTTP 429" in error:
        return 900.0
    return 600.0   # timeouts, server errors


def _read() -> dict[str, Outage]:
    try:
        return TypeAdapter(dict[str, Outage]).validate_json(STATE.read_text())
    except (OSError, ValueError):
        return {}
