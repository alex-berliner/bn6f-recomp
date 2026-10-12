"""git status parsing (a stripped " M bn6f" once became "n6f" and failed a record run's commit), and a commit
the gate refuses putting its files back."""
import subprocess
from pathlib import Path

import pytest

from wf.tools import git as git_tools
from wf.tools.errors import ToolOutputError
from wf.tools.git import porcelain_paths


def test_a_leading_space_is_part_of_the_status_not_the_path() -> None:
    assert porcelain_paths(" M bn6f\0M  symbols/SYMBOLS.md\0?? symbols/db/new.yaml\0") == (
        "bn6f", "symbols/SYMBOLS.md", "symbols/db/new.yaml")


def test_a_commit_the_hook_refuses_puts_the_files_back(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    def git(*arguments: str) -> None:
        _ = subprocess.run(["git", "-C", str(tmp_path), *arguments], check=True, capture_output=True)

    git("init", "-q")
    git("config", "user.email", "test@example.com")
    git("config", "user.name", "test")
    (tmp_path / "game.c").write_text("committed\n")
    (tmp_path / "shared.cfg").write_text("committed\n")
    git("add", ".")
    git("commit", "-q", "-m", "first")
    hook = tmp_path / ".git/hooks/pre-commit"
    hook.write_text("#!/bin/sh\necho 'gate: leftovers' >&2\nexit 1\n")
    hook.chmod(0o755)
    monkeypatch.setattr(git_tools, "REPO", tmp_path)
    (tmp_path / "game.c").write_text("half-done\n")
    (tmp_path / "new.c").write_text("half-done\n")
    (tmp_path / "shared.cfg").write_text("committed\nthis run's line\n")

    with pytest.raises(ToolOutputError):
        _ = git_tools.commit_or_restore((tmp_path / "game.c", tmp_path / "new.c", tmp_path / "shared.cfg"), "install\n",
                                        keep=(tmp_path / "shared.cfg",))

    assert (tmp_path / "game.c").read_text() == "committed\n" and not (tmp_path / "new.c").exists()
    assert (tmp_path / "shared.cfg").read_text() == "committed\nthis run's line\n"   # the caller's to put right
    staged = subprocess.run(["git", "-C", str(tmp_path), "diff", "--cached", "--name-only"], capture_output=True,
                            text=True, check=True).stdout
    assert staged == ""
