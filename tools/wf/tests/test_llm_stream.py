"""tools/llm.py's streamed replies with fake server-sent events: the reply put together for both API kinds,
the reasoning budget closing a runaway (with a usage row of estimated tokens), and code that began in time."""
import json
from collections.abc import Callable, Iterator
from pathlib import Path
from types import ModuleType
from typing import cast

import pytest

from wf.tools.models import FARM

llm = cast(ModuleType, FARM.llm)  # pyright: ignore[reportAny]
complete = cast(Callable[..., str], llm.complete)  # pyright: ignore[reportAny, reportExplicitAny]
BudgetSpent = cast(type[Exception], llm.ReasoningBudgetSpent)  # pyright: ignore[reportAny]


class FakeResponse:
    """What urlopen gives back: the event lines, then closed (the test checks that it was)."""

    def __init__(self, lines: list[str]) -> None:
        self.lines = [line.encode() + b"\n" for line in lines]
        self.closed = False

    def __iter__(self) -> Iterator[bytes]:
        return iter(self.lines)

    def __enter__(self) -> "FakeResponse":
        return self

    def __exit__(self, *exc: object) -> None:
        self.closed = True


def chat_events(reasoning: list[str], content: list[str]) -> list[str]:
    def event(delta: dict[str, str]) -> str:
        return "data: " + json.dumps({"model": "glm-5.3-flash", "choices": [{"delta": delta}]})
    usage = {"prompt_tokens": 100, "completion_tokens": len(reasoning) + len(content),
             "completion_tokens_details": {"reasoning_tokens": len(reasoning)}}
    return ([event({"reasoning_content": piece}) for piece in reasoning] + [event({"content": piece}) for piece in content]
            + ["data: " + json.dumps({"choices": [], "usage": usage}), "data: [DONE]"])


def anthropic_events(thinking: list[str], text: list[str]) -> list[str]:
    def event(body: dict[str, object]) -> list[str]:
        return [f"event: {body['type']}", "data: " + json.dumps(body), ""]
    lines = event({"type": "message_start", "message": {"model": "MiniMax-M3", "usage": {"input_tokens": 0}}})
    lines += event({"type": "content_block_start", "index": 0, "content_block": {"type": "thinking", "thinking": ""}})
    for piece in thinking:
        lines += event({"type": "content_block_delta", "index": 0, "delta": {"type": "thinking_delta", "thinking": piece}})
    lines += event({"type": "content_block_start", "index": 1, "content_block": {"type": "text", "text": ""}})
    for piece in text:
        lines += event({"type": "content_block_delta", "index": 1, "delta": {"type": "text_delta", "text": piece}})
    lines += event({"type": "message_delta", "usage": {"input_tokens": 50, "output_tokens": 7}})
    return lines + event({"type": "message_stop"})


def serve(monkeypatch: pytest.MonkeyPatch, lines: list[str], api: str = "openai-completions") -> list[FakeResponse]:
    """complete() talks to the fake; returns the responses it opened (and the request bodies, as their
    first line, are not needed)."""
    opened: list[FakeResponse] = []

    def providers() -> tuple[dict[str, dict[str, str]], dict[str, dict[str, str]]]:
        return {"fake": {"api": api, "baseUrl": "http://fake"}}, {"fake": {"key": "k"}}

    def urlopen(request: object, timeout: float = 0) -> FakeResponse:
        opened.append(FakeResponse(lines))
        return opened[-1]

    monkeypatch.setattr(llm, "_providers", providers)
    monkeypatch.setattr(llm.urllib.request, "urlopen", urlopen)  # pyright: ignore[reportAny]
    return opened


def usage_rows(log: Path) -> list[dict[str, object]]:
    return [cast(dict[str, object], json.loads(line)) for line in log.read_text().splitlines()]


def test_a_streamed_reply_is_put_together_with_its_usage(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    serve(monkeypatch, chat_events(["think"] * 5, ["```c\n", "int f(void);\n", "```"]))
    log = tmp_path / "usage.jsonl"

    reply = complete("fake/glm", "prompt", usage_log=str(log), stream=True, fields={"round": 0})

    assert reply == "```c\nint f(void);\n```"
    row = usage_rows(log)[0]
    assert (row["in"], row["out"], row["reasoning"], row["round"]) == (100, 8, 5, 0)
    assert "estimated" not in row and "stopped" not in row


def test_a_runaway_is_stopped_at_its_budget_and_logged(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    opened = serve(monkeypatch, chat_events(["x"] * 50, ["```c\nlate\n```"]))
    log = tmp_path / "usage.jsonl"

    with pytest.raises(BudgetSpent) as stopped:
        _ = complete("fake/glm", "p" * 260, usage_log=str(log), reasoning_budget=20)

    assert getattr(stopped.value, "reasoning_tokens") == 21   # one event per token: stopped at the first past it
    assert opened[0].closed
    row = usage_rows(log)[0]
    assert row["stopped"] == "reasoning budget" and row["estimated"] is True and row["budget"] == 20
    assert row["out"] == 21 and row["in"] == 100   # 260 prompt characters at 2.6 a token


def test_code_that_began_in_time_is_not_stopped(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    serve(monkeypatch, chat_events(["x"] * 5, ["``", "`c\n", "int f;", "```"]) )   # the fence split in two

    reply = complete("fake/glm", "prompt", usage_log=str(tmp_path / "usage.jsonl"), reasoning_budget=6)

    assert reply.startswith("```c")


def test_inline_think_blocks_count_as_reasoning(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    serve(monkeypatch, chat_events([], ["<think>", "a" * 40, "b" * 40, "</think>", "```c\nok\n```"]))

    with pytest.raises(BudgetSpent):
        _ = complete("fake/m", "prompt", usage_log=str(tmp_path / "usage.jsonl"), reasoning_budget=10)


def test_an_anthropic_stream_is_read_and_budgeted(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    serve(monkeypatch, anthropic_events(["hm"] * 3, ["```c\n", "x\n```"]), api="anthropic-messages")
    log = tmp_path / "usage.jsonl"

    reply = complete("fake/M3", "prompt", usage_log=str(log), stream=True)

    assert reply == "```c\nx\n```"
    row = usage_rows(log)[0]
    assert (row["in"], row["out"], row["answered_by"]) == (50, 7, "MiniMax-M3")

    serve(monkeypatch, anthropic_events(["hm"] * 30, ["```c\nx\n```"]), api="anthropic-messages")
    with pytest.raises(BudgetSpent):
        _ = complete("fake/M3", "prompt", usage_log=str(log), reasoning_budget=10)
