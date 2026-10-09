"""The model sources: a request goes to the first source that is up, a provider's failure
moves it on (and is recorded for every run), a wrong answer does not, and when every
source is down it waits for the first one back, within limits (the clock is
conftest's fake one)."""
from wf.tools import sources
from wf.tools.sources import TIERS, Tier, first_up, models_for
from wf.types import ModelSpec

CHEAP = ModelSpec("hyper/cheap")
OTHER = ModelSpec("zai/other")
QUOTA = "hyper/cheap: HTTP 429 (retry after 1200s): hourly limit"


def test_a_failing_source_is_recorded_and_the_next_answers(clock: list[float]) -> None:
    asked: list[ModelSpec] = []

    def ask(model: ModelSpec) -> str | None:
        asked.append(model)
        return QUOTA if model == CHEAP else None

    def failure(reply: str | None) -> str | None:
        return reply

    assert first_up((CHEAP, OTHER), ask, failure) == (OTHER, None)
    assert asked == [CHEAP, OTHER]
    assert sources.down_until(CHEAP) == clock[0] + 1200   # every run now passes it over
    assert sources.down_until(OTHER) is None


def test_a_wrong_answer_is_not_a_reason_to_change_source(clock: list[float]) -> None:
    def ask(model: ModelSpec) -> str:
        return "the reply had no ```c code block"

    def failure(reply: str) -> str | None:
        return reply

    assert first_up((CHEAP, OTHER), ask, failure)[0] == CHEAP
    assert sources.down_until(CHEAP) is None


def test_when_every_source_is_down_it_waits_for_the_first_back(clock: list[float]) -> None:
    started = clock[0]
    sources.report_failure(CHEAP, "hyper/cheap: HTTP 429 (retry after 5000s): limit")
    sources.report_failure(OTHER, "zai/other: HTTP 429 (retry after 1200s): limit")

    def ask(model: ModelSpec) -> None:
        return None

    def failure(reply: None) -> str | None:
        return reply

    assert first_up((CHEAP, OTHER), ask, failure)[0] == OTHER
    assert started + 1200 <= clock[0] < started + 1900


def test_past_the_horizon_it_gives_back_the_last_failure_at_once(clock: list[float]) -> None:
    started = clock[0]

    def ask(model: ModelSpec) -> str:
        return f"{model}: HTTP 429 (retry after 200000s): weekly quota used"

    def failure(reply: str) -> str | None:
        return reply

    model, reply = first_up((CHEAP, OTHER), ask, failure)
    assert model == OTHER and "weekly quota" in reply and clock[0] == started
    assert sources.out_for_long((CHEAP, OTHER))


def test_a_tier_stands_for_its_models_in_order() -> None:
    assert models_for(ModelSpec("tier/strong")) == TIERS[Tier.STRONG]
    assert models_for(CHEAP) == (CHEAP,)
    assert all(model.count("/") == 1 for tier in TIERS.values() for model in tier)


def test_a_model_metered_apart_is_not_stopped_by_its_providers_spent_plan(clock: list[float]) -> None:
    paid, free = ModelSpec("opencode/kimi-k3"), ModelSpec("opencode/longcat-2.5-preview-free")
    sources.report_failure(paid, "opencode/kimi-k3: HTTP 429 (retry after 200000s): weekly cap")

    assert sources.down_until(paid) is not None
    assert sources.down_until(ModelSpec("opencode/deepseek-v4-pro")) is not None   # same plan
    assert sources.down_until(free) is None
