from pathlib import Path

import pytest


@pytest.fixture(autouse=True)
def private_tracking_dir(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    """Test runs are tracked in a temporary directory, never in the Burr UI's ~/.burr."""
    monkeypatch.setenv("WF_BURR_DIR", str(tmp_path / "burr"))
