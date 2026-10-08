"""The adapter's rules for transitions, on a two-way toy workflow."""
import pytest
from pydantic import BaseModel

from wf.burr_typed import Finished, Transition, TransitionError, Workflow
from wf.types import Result


class ToyState(BaseModel):
    number: int
    went: str = ""


class Nothing(Result):
    pass


class Went(Result):
    went: str


toy = Workflow[ToyState](ToyState, project="test")


@toy.step
def start(given: Nothing) -> Nothing:
    return Nothing()


@toy.step
def left(given: Nothing) -> Went:
    return Went(went="left")


@toy.step
def right(given: Nothing) -> Went:
    return Went(went="right")


def is_small(state: ToyState) -> bool:
    return state.number < 10


def is_large(state: ToyState) -> bool:
    return state.number >= 10


def is_even(state: ToyState) -> bool:
    return state.number % 2 == 0


def run(number: int, transitions: list[Transition]) -> ToyState:
    last = left if number < 10 else right
    outcome = toy.graph(first=start, ends=[last], transitions=transitions).start(
        ToyState(number=number), run_id="test", tracked=False)
    assert isinstance(outcome, Finished)
    return outcome.state


def test_exactly_one_decision_holding_picks_the_way() -> None:
    transitions = [Transition(after=start, go_to=left, when=is_small),
                   Transition(after=start, go_to=right, when=is_large)]

    assert run(3, transitions).went == "left"
    assert run(30, transitions).went == "right"


def test_two_decisions_holding_stop_the_run() -> None:
    transitions = [Transition(after=start, go_to=left, when=is_small),
                   Transition(after=start, go_to=right, when=is_even)]

    with pytest.raises(TransitionError, match="2 of these held"):
        run(4, transitions)


def test_no_decision_holding_stops_the_run() -> None:
    transitions = [Transition(after=start, go_to=left, when=is_small),
                   Transition(after=start, go_to=right, when=is_even)]

    with pytest.raises(TransitionError, match="0 of these held"):
        run(31, transitions)


def test_several_transitions_without_decisions_are_refused() -> None:
    transitions = [Transition(after=start, go_to=left, when=is_small),
                   Transition(after=start, go_to=right)]

    with pytest.raises(TransitionError, match="each needs a `when`"):
        run(3, transitions)
