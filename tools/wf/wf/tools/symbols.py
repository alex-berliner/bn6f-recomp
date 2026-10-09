"""The symbol database (symbols/db) and its tools: check, render, rename,
write comments into bn6f (tools/symdb.py, rename_symbol.py, symdoc.py)."""
import os
import re
import subprocess
from pathlib import Path

from wf.repo import REPO, TOOLCHAIN_BIN
from wf.tools.errors import ToolOutputError
from wf.types import CommitHash, LogPath, Rename, SymbolEntry, SymbolName, SymbolStatus, ToolMessage
from wf.tools.locks import BN6F, held

DATABASE = REPO / "symbols/db"

_STATUS = re.compile(r"^status:\s*(\w+)", re.MULTILINE)
_BN6F_COMMIT = re.compile(r"bn6f commit ([0-9a-f]{7,40})")


def entry(name: SymbolName) -> SymbolEntry | None:
    """A symbols/db entry, or None when the symbol has none yet."""
    file = DATABASE / f"{name}.yaml"
    if not file.exists():
        return None
    status = _STATUS.search(file.read_text())
    if status is None:
        raise ToolOutputError(f"{file}: no status")
    return SymbolEntry(name=name, status=SymbolStatus(status[1]), file=file)


def check_database(log: LogPath) -> tuple[ToolMessage, ...]:
    """The problems tools/symdb.py check finds (none when the database is clean)."""
    done = _tool(["python3", "tools/symdb.py", "check"], log)
    problems = [line for line in done.stdout.splitlines() if ": " in line and not line.endswith(" problems")]
    if done.returncode != 0 and not problems:
        raise ToolOutputError(f"symdb.py check failed without naming a problem, see {log}")
    return tuple(ToolMessage(line) for line in problems)


def render_database(log: LogPath) -> None:
    done = _tool(["python3", "tools/symdb.py", "render"], log)
    if done.returncode != 0:
        raise ToolOutputError(f"symdb.py render failed, see {log}")


def rename_symbols(renames: tuple[Rename, ...], log: LogPath) -> CommitHash:
    """Rename in bn6f (ROM must stay identical; bn6f gets a commit) and regenerate the lifted C."""
    pairs = [name for rename in renames for name in (rename.old, rename.new)]
    with held(BN6F):   # no run may lift while bn6f's names change
        done = _tool(["python3", "tools/rename_symbol.py", *pairs], log)
    commit = _BN6F_COMMIT.search(done.stdout)
    if done.returncode != 0 or commit is None:
        raise ToolOutputError(f"rename_symbol.py failed, see {log}")
    return CommitHash(commit[1])


def write_bn6f_comments(log: LogPath) -> CommitHash | None:
    """`// symdb:` blocks at each definition in bn6f, committed there; None when nothing changed."""
    with held(BN6F):
        done = _tool(["python3", "tools/symdoc.py", "--commit"], log)
    if done.returncode != 0:
        raise ToolOutputError(f"symdoc.py failed (bn6f may be left modified), see {log}")
    commit = _BN6F_COMMIT.search(done.stdout)
    return CommitHash(commit[1]) if commit else None


def _tool(command: list[str], log: LogPath) -> subprocess.CompletedProcess[str]:
    done = subprocess.run(command, cwd=REPO, capture_output=True, text=True, check=False,
                          env={**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}"})
    with open(log, "a") as out:
        out.write(f"$ {' '.join(command)}\n{done.stdout}{done.stderr}\n")
    return done


def yaml_path(name: SymbolName) -> Path:
    return DATABASE / f"{name}.yaml"


def refresh_pc_port_symbols(log: LogPath) -> None:
    """After renames: rebuild bn6f and refresh the PC port's symbol overlay (tools/import_symbols.sh)."""
    done = _tool(["tools/import_symbols.sh"], log)
    if done.returncode != 0:
        raise ToolOutputError(f"import_symbols.sh failed, see {log}")
