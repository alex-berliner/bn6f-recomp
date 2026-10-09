"""Put a candidate file into a tree (the main checkout, or a run's worktree) and
take it out again: the file under cport/src/game, and its difftest.cfg lines
between markers."""
from pathlib import Path

from wf.repo import REPO
from wf.tools.subsystem import CandidateFile, CfgLine

GAME_DIR = REPO / "cport/src/game"
DIFFTEST_CFG = REPO / "cport/host/difftest.cfg"


def game_file(file_name: str, tree: Path = REPO) -> Path:
    return _game_dir(tree) / file_name


def install_file(file_name: str, code: CandidateFile, cfg_lines: tuple[CfgLine, ...], tree: Path = REPO) -> Path:
    path = game_file(file_name, tree)
    path.write_text(code if code.endswith("\n") else code + "\n")
    _write_cfg_section(file_name, cfg_lines, tree)
    return path


def uninstall_file(file_name: str, tree: Path = REPO) -> None:
    game_file(file_name, tree).unlink(missing_ok=True)
    _write_cfg_section(file_name, (), tree)


def difftest_cfg(tree: Path = REPO) -> Path:
    return DIFFTEST_CFG if tree == REPO else tree / "cport/host/difftest.cfg"


def _game_dir(tree: Path) -> Path:
    return GAME_DIR if tree == REPO else tree / "cport/src/game"


def _write_cfg_section(file_name: str, cfg_lines: tuple[CfgLine, ...], tree: Path) -> None:
    """Replace this file's section of difftest.cfg (none, when cfg_lines is empty)."""
    begin, end = f"# begin {file_name}", f"# end {file_name}"
    cfg = difftest_cfg(tree)
    lines = cfg.read_text().splitlines()
    if begin in lines and end in lines:
        lines = lines[:lines.index(begin)] + lines[lines.index(end) + 1:]
    if cfg_lines:
        lines += [begin, *cfg_lines, end]
    cfg.write_text("\n".join(lines) + "\n")
