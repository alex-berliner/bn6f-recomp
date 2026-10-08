"""Put a candidate file into the tree for a full verify, and take it out again:
the file under cport/src/game, and its difftest.cfg lines between markers."""
from pathlib import Path

from wf.repo import CPORT
from wf.tools.subsystem import CandidateFile, CfgLine

GAME_DIR = CPORT / "src/game"
DIFFTEST_CFG = CPORT / "host/difftest.cfg"


def game_file(file_name: str) -> Path:
    return GAME_DIR / file_name


def install_file(file_name: str, code: CandidateFile, cfg_lines: tuple[CfgLine, ...]) -> Path:
    path = game_file(file_name)
    path.write_text(code if code.endswith("\n") else code + "\n")
    _write_cfg_section(file_name, cfg_lines)
    return path


def uninstall_file(file_name: str) -> None:
    game_file(file_name).unlink(missing_ok=True)
    _write_cfg_section(file_name, ())


def _write_cfg_section(file_name: str, cfg_lines: tuple[CfgLine, ...]) -> None:
    """Replace this file's section of difftest.cfg (none, when cfg_lines is empty)."""
    begin, end = f"# begin {file_name}", f"# end {file_name}"
    lines = DIFFTEST_CFG.read_text().splitlines()
    if begin in lines and end in lines:
        lines = lines[:lines.index(begin)] + lines[lines.index(end) + 1:]
    if cfg_lines:
        lines += [begin, *cfg_lines, end]
    DIFFTEST_CFG.write_text("\n".join(lines) + "\n")
