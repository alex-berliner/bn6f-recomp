"""Pause points: a run stops before a step that needs an answer, and a later
`resume` (from the tracking log, as a new process would) carries on."""
from pathlib import Path

import pytest
from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Paused, Transition, Workflow
from wf.types import Result


class AskState(BaseModel):
    question: str
    reply: str = ""
    shouted: str = ""


class Question(Result):
    question: str


class Reply(Result):
    reply: str


class Shouted(Result):
    shouted: str


class WrongAnswer(Result):
    number: int


ask = Workflow[AskState](AskState, project="test-pause")


@ask.step
def pose(given: Question) -> Question:
    return given


@ask.pause_point
def wait_for_reply(given: Question, answer: Reply) -> Reply:
    return answer


@ask.step
def shout(given: Reply) -> Shouted:
    return Shouted(shouted=given.reply.upper())


def ask_graph() -> Graph[AskState]:
    return ask.graph(first=pose, last=shout, transitions=[
        Transition(after=pose, go_to=wait_for_reply),
        Transition(after=wait_for_reply, go_to=shout),
    ])


@pytest.fixture(autouse=True)
def private_tracking_dir(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    monkeypatch.setenv("WF_BURR_DIR", str(tmp_path))


def test_a_run_stops_before_a_pause_point_and_says_what_it_needs() -> None:
    outcome = ask_graph().start(AskState(question="name?"), run_id="ask-1")

    assert isinstance(outcome, Paused)
    assert outcome.step == "wait_for_reply"
    assert outcome.request == Question(question="name?")
    assert outcome.answer_type is Reply


def test_resume_carries_on_from_the_saved_run() -> None:
    _ = ask_graph().start(AskState(question="name?"), run_id="ask-2")

    outcome = ask_graph().resume("ask-2", Reply(reply="mega man"))

    assert isinstance(outcome, Finished)
    assert outcome.state.shouted == "MEGA MAN"
    assert ask_graph().final_state("ask-2").shouted == "MEGA MAN"


def test_resume_refuses_the_wrong_kind_of_answer() -> None:
    _ = ask_graph().start(AskState(question="name?"), run_id="ask-3")

    with pytest.raises(TypeError, match="needs a Reply"):
        _ = ask_graph().resume("ask-3", WrongAnswer(number=6))
