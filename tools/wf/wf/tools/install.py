"""Put a candidate file into a tree (the main checkout, or a run's worktree) and
take it out again: the file under cport/src/game, and its difftest.cfg lines
between markers. A farm group's file is an inbox (tools/layout.py): its
functions go to the files of their bn6f sources, and the file goes."""
import re
import subprocess
from pathlib import Path

from wf.repo import REPO
from wf.tools.errors import ToolOutputError
from wf.tools.subsystem import CandidateFile, CfgLine
from wf.types import FunctionName
import layout  # tools/layout.py (wf.repo puts tools/ on the path): where each function's part goes

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


def lay_out_group(file_name: str, tree: Path = REPO) -> tuple[Path, ...]:
    """Empty an installed farm group's file into the laid-out files of the tree (tools/layout.py); the
    game files written, which are the ones to commit. Nothing else may move: the tree is laid out. A name
    one of the group's parts defines that a part laid out since the group was assembled also defines or
    reads (a parallel run committed it) is renamed apart in the group's part first, as the assembly does."""
    game = _game_dir(tree)
    files = layout.read_game(game_dir=str(game))
    plan = layout.plan(files, inbox=(file_name,))
    renamed = renamed_apart(files[file_name], plan.conflicts)
    if renamed != files[file_name]:
        _ = (game / file_name).write_text(renamed)
        plan = layout.plan({**files, file_name: renamed}, inbox=(file_name,))
    others = sorted({move.old for move in plan.moves if move.old != file_name})
    if plan.problems or others:
        raise ToolOutputError(f"cannot lay out {file_name}: "
                              + "; ".join(plan.problems or [f"{', '.join(others)} would move too (not laid out)"]))
    layout.write_game(plan, str(game))
    return tuple(game / name for name, text in sorted(plan.files.items()) if text is not None)


def renamed_apart(code: str, conflicts: list[layout.Conflict]) -> str:
    """CODE (an assembled farm group) with each name a conflict is about renamed, in the group's own part
    that defines it, to NAME_FUNCTION: the part then neither clashes with a neighbour's definition nor lends
    its own to a neighbour. A conflict where the group's part only reads a neighbour's name stays."""
    marks = {match[1]: match for match in re.finditer(r"^/\* ── (\w+) ── \*/$", code, re.M)}
    for conflict in conflicts:
        owner = next((owner for owner in conflict.definers if owner in marks), None)
        if owner is None or not conflict.name:
            continue
        start = marks[owner].start()
        later = [match.start() for match in marks.values() if match.start() > start]
        end = min(later, default=len(code))
        part = re.sub(rf"\b{re.escape(conflict.name)}\b", f"{conflict.name}_{owner}", code[start:end])
        part = part.replace(f"/* ── {owner}_{owner} ── */", f"/* ── {owner} ── */")   # (a part named like its name)
        code = code[:start] + part + code[end:]
        marks = {match[1]: match for match in re.finditer(r"^/\* ── (\w+) ── \*/$", code, re.M)}
    return code


def layout_conflicts(file_name: str, code: CandidateFile, functions: tuple[FunctionName, ...],
                     tree: Path) -> dict[FunctionName, str]:
    """The group's functions whose parts could not land in the tree's laid-out files (a name defined
    twice in one file, or a part that would see a neighbour's definition), with why."""
    files = layout.read_game(game_dir=str(_game_dir(tree)))
    files[file_name] = code
    out: dict[FunctionName, str] = {}
    for conflict in layout.plan(files, inbox=(file_name,)).conflicts:
        blamed = next((owner for owner in conflict.blame if owner in functions), None)
        if blamed is not None:
            _ = out.setdefault(FunctionName(blamed), conflict.text)
    return out


def placed(functions: tuple[FunctionName, ...], tree: Path = REPO) -> dict[FunctionName, Path]:
    """The laid-out file each function of a farm group goes to."""
    return {FunctionName(function): _game_dir(tree) / dest for function, dest in layout.where(functions).items()
            if dest is not None}


def names_taken(functions: tuple[FunctionName, ...], tree: Path = REPO) -> dict[FunctionName, frozenset[str]]:
    """For each function of a farm group, every name the parts of the laid-out file it goes to define or
    use (the group's own file-level definitions are renamed apart from them: drafts.candidate_from_farm)."""
    files = layout.read_game(game_dir=str(_game_dir(tree)))
    in_file: dict[str, frozenset[str]] = {}
    out: dict[FunctionName, frozenset[str]] = {}
    for function, dest in layout.where(functions).items():
        if dest is None:
            continue
        if dest not in in_file:
            parts = layout.parts_of(dest, files[dest])[1] if dest in files else []
            in_file[dest] = frozenset(name for part in parts for name in part.defines | part.uses)
        out[FunctionName(function)] = in_file[dest]
    return out


def restore_game_files(tree: Path) -> None:
    """A run's worktree back to its commit's cport/src/game (a farm group's install was laid out into
    many files); the main checkout is never reset."""
    if tree == REPO:
        raise ValueError("only a run's own worktree is restored")
    _ = subprocess.run(["git", "-C", str(tree), "checkout", "HEAD", "--", "cport/src/game"], check=True,
                       capture_output=True)
    tracked = subprocess.run(["git", "-C", str(tree), "ls-files", "cport/src/game"], check=True,
                             capture_output=True, text=True).stdout.split()
    for path in _game_dir(tree).glob("*.c"):
        if f"cport/src/game/{path.name}" not in tracked:
            path.unlink()
