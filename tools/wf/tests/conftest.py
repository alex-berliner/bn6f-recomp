"""Every test tracks its runs in a temporary directory, keeps its own record of which model
sources are down, waits on a fake clock, and sees no local pacer logs: nothing reads or
writes the real state."""
from pathlib import Path

import pytest

from wf.tools import sources
from wf.types import ModelSpec


@pytest.fixture(autouse=True)
def private_tracking_dir(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    """Test runs are tracked in a temporary directory, never in the Burr UI's ~/.burr."""
    monkeypatch.setenv("WF_BURR_DIR", str(tmp_path / "burr"))


@pytest.fixture(autouse=True)
def clock(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> list[float]:
    """The fake time for waiting on sources; sleeping moves it on."""
    now = [1_000_000.0]

    def time_now() -> float:
        return now[0]

    def sleep(seconds: float) -> None:
        now[0] += seconds

    def no_pacer(model: ModelSpec) -> float | None:
        return None

    monkeypatch.setattr(sources.time, "time", time_now)
    monkeypatch.setattr(sources, "sleep", sleep)
    monkeypatch.setattr(sources, "pacer_down_until", no_pacer)
    monkeypatch.setattr(sources, "STATE", tmp_path / "sources.json")
    return now
