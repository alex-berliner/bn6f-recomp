"""The retype queue (tools/abi/retype_queue.tsv): functions installed in register form from the farm's old
prompt (plan B after the prompt benchmark, docs/reviews/2026-10-10-benchmark.md), waiting to be typed
(stages S3/S4 of docs/reviews/2026-10-10-4-joint-plan.md).

Only a convert run installing old-prompt candidates (`--farm-model MODEL+old`) adds rows: one per function
whose code has register-level hits (tools/abi/lint.py), with the lint keys those hits are counted under (the
function, and `FILE:helper` for its static helpers) and its prompt hash and model; the same commit raises
the lint baseline by exactly those keys' counts. tools/gate.py lets the baseline grow by the keys of rows a
commit adds here and by nothing else. A row goes when its function is typed (always allowed); renames
(tools/rename_symbol.py) rename its function and keys as they do the baseline's."""
import csv
import functools
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from pydantic import TypeAdapter

from wf.repo import REPO
from wf.tools.drafts import function_at_line
from wf.types import CommitHash, FunctionName, ModelSpec, Result

QUEUE = REPO / "tools/abi/retype_queue.tsv"
BASELINE = REPO / "tools/abi/lint_baseline.json"
LINT = REPO / "tools/abi/lint.py"
LEFTOVERS = REPO / "tools/abi/leftovers.py"
COLUMNS = ("function", "lint_keys", "prompt_hash", "model", "install_run", "install_commit", "why")
HEADER = ("# tools/abi/retype_queue.tsv: functions installed in register form from the farm's old prompt, waiting "
          "to be typed (tools/wf/wf/tools/retype.py says how rows come and go; tools/gate.py lets the lint baseline "
          "grow only by the lint_keys of rows a commit adds)")
PENDING = "-"   # install_commit until the run's next commit fills it in (a commit cannot name itself)
WHY = "register form, old prompt; queued for retype"


class QueueRow(Result):
    function: FunctionName
    lint_keys: tuple[str, ...]   # tools/abi/lint_baseline.json keys of its code: the function, FILE:helper
    prompt_hash: str
    model: ModelSpec             # as filed: minimax/MiniMax-M3+old
    install_run: str             # the convert run that installed it
    install_commit: str          # the install commit, or PENDING
    why: str


def read_queue(path: Path | None = None) -> list[QueueRow]:
    path = path or QUEUE
    if not path.exists():
        return []
    lines = [line for line in path.read_text().splitlines() if line and not line.startswith("#")]
    rows = list(csv.reader(lines[1:], delimiter="\t"))
    return [QueueRow(function=FunctionName(cells[0]), lint_keys=tuple(key for key in cells[1].split(",") if key),
                     prompt_hash=cells[2], model=ModelSpec(cells[3]), install_run=cells[4], install_commit=cells[5],
                     why=cells[6]) for cells in rows]


def write_queue(rows: list[QueueRow], path: Path | None = None) -> None:
    path = path or QUEUE
    body = ["\t".join((row.function, ",".join(row.lint_keys), row.prompt_hash, row.model, row.install_run,
                       row.install_commit, row.why)) for row in rows]
    _ = path.write_text("\n".join([HEADER, "\t".join(COLUMNS), *body]) + "\n")


class LintedFunction(Result):
    """One function of tools/abi/lint.py's report."""
    name: str
    kind: str
    line: int
    static: bool
    counts: dict[str, int]
    violation: bool


_REPORT = TypeAdapter(dict[str, list[LintedFunction]])


class BaselineFile(Result):
    comment: str
    functions: dict[str, dict[str, int]]   # key -> hits per category


_BASELINE_FILE = TypeAdapter(BaselineFile)


def lint_counts(files: tuple[Path, ...]) -> dict[str, tuple[Path, LintedFunction]]:
    """Baseline key -> (file, function) for each function of FILES that tools/abi/lint.py counts as a
    violation (the keys and counts `lint.py --write-baseline` would write)."""
    done = subprocess.run([sys.executable, str(LINT), "--quiet", *map(str, files)], capture_output=True, text=True,
                          check=False)
    if done.returncode != 0 or not done.stdout.strip():
        raise RuntimeError(f"tools/abi/lint.py failed: {done.stderr.strip()[-500:]}")
    report = _REPORT.validate_python(json.loads(done.stdout)["files"])
    by_name = {path.name: path for path in files}
    found: dict[str, tuple[Path, LintedFunction]] = {}
    for file_name, functions in report.items():
        for function in functions:
            if function.violation:
                key = f"{file_name}:{function.name}" if function.static or function.kind == "file" else function.name
                found[key] = (by_name.get(file_name, Path(file_name)), function)
    return found


def queue_install(functions: tuple[FunctionName, ...], assembled: str, laid_out: tuple[Path, ...], model: ModelSpec,
                  prompt_hash: str, run: str) -> tuple[QueueRow, ...]:
    """After a group of old-prompt candidates is laid out in the main tree: raise the lint baseline by the
    hits its code added (keys new to the baseline or above it, in the laid-out files) and queue each function
    that has any, with its keys. ASSEMBLED is the group's file before the layout, whose section markers say
    which function a static helper belongs to. Returns the rows added."""
    baseline_doc = _BASELINE_FILE.validate_json(BASELINE.read_text())
    baseline = dict(baseline_doc.functions)
    added: dict[str, dict[str, int]] = {}
    for key, (_, linted) in lint_counts(laid_out).items():
        before = baseline.get(key, {})
        if any(count > before.get(category, 0) for category, count in linted.counts.items()):
            added[key] = linted.counts
    owners = _owners(assembled, functions)
    keys_of: dict[FunctionName, list[str]] = {}
    for key in sorted(added):
        name = key.rpartition(":")[2]
        owner = FunctionName(name) if name in functions else owners.get(name)
        if owner is None:
            raise RuntimeError(f"new register-level code under {key} belongs to no function of the group")
        keys_of.setdefault(owner, []).append(key)
    if not keys_of:
        return ()
    baseline.update(added)
    # as `lint.py --write-baseline` writes it: one line per key, sorted
    rows = ",\n".join(f"  {json.dumps(key)}: {json.dumps(counts)}" for key, counts in sorted(baseline.items()))
    _ = BASELINE.write_text('{\n "comment": ' + json.dumps(baseline_doc.comment) + ',\n "functions": {\n'
                            + rows + "\n }\n}\n")
    queued = tuple(QueueRow(function=function, lint_keys=tuple(keys_of[function]), prompt_hash=prompt_hash,
                            model=model, install_run=run, install_commit=PENDING, why=WHY)
                   for function in functions if function in keys_of)
    kept = [row for row in read_queue() if row.function not in keys_of]
    write_queue(kept + list(queued))
    return queued


def _owners(assembled: str, functions: tuple[FunctionName, ...]) -> dict[str, FunctionName]:
    """Each name defined at the start of a line in the assembled file (a static helper) -> whose part it is in."""
    owners: dict[str, FunctionName] = {}
    for number, line in enumerate(assembled.splitlines(), start=1):
        stripped = line.split("(")[0].split()
        if line[:1].isspace() or "(" not in line or not stripped:
            continue
        owner = function_at_line(assembled, number)
        if owner is not None and owner in functions:
            _ = owners.setdefault(stripped[-1].lstrip("*"), owner)
    return owners


def fill_install_commit(functions: tuple[FunctionName, ...], commit: CommitHash) -> bool:
    """The install commit into the rows of FUNCTIONS still waiting for it; whether any changed."""
    rows = read_queue()
    changed = False
    for index, row in enumerate(rows):
        if row.function in functions and row.install_commit == PENDING:
            rows[index] = row.model_copy(update={"install_commit": commit[:10]})
            changed = True
    if changed:
        write_queue(rows)
    return changed


class LeftoverSite(Result):
    """A read of a register or flag a callee leaves behind but does not return (tools/abi/leftovers.py)."""
    line: int
    function: str
    callee: str
    reg: str
    kind: str
    noted: bool


class _LeftoverReport(Result):
    sites: tuple[LeftoverSite, ...]


def leftover_reads(code: str, file_name: str) -> dict[FunctionName, str]:
    """The functions of an assembled farm file with a leftover read the gate would refuse (unnoted, not the
    r0-for-Z kind), and the first one's description. Register-form code mirrors the original's registers, so
    it reads them more often than typed code does."""
    with tempfile.TemporaryDirectory(prefix="wf-leftovers-") as scratch:
        path = Path(scratch) / file_name
        _ = path.write_text(code)
        done = subprocess.run([sys.executable, str(LEFTOVERS), "--quiet", str(path)], capture_output=True, text=True,
                              check=False)
    if not done.stdout.strip():
        raise RuntimeError(f"tools/abi/leftovers.py failed: {done.stderr.strip()[-500:]}")
    found: dict[FunctionName, str] = {}
    for site in _LeftoverReport.model_validate_json(done.stdout).sites:
        owner = function_at_line(code, site.line)
        if site.noted or site.kind == "zflag" or owner is None:
            continue
        _ = found.setdefault(owner, f"reads {site.reg} after {site.callee}, which does not return it ({site.kind}; "
                                    f"line {site.line}, tools/abi/leftovers.py)")
    return found


# `LR = <an address>`: GUEST_CALL keeps LR, so RETURN_TO_CALLER later returns to that address, into the middle
# of the original (sub_80498F8, old-prompt set 3: the port's main_ returned on menus3)
_LR_CONSTANT = re.compile(r"^[ \t]*LR\s*=\s*(?:0x[0-9A-Fa-f]+|[0-9]+)\b[^;]*;", re.M)
_CALL = re.compile(r"(?<![\w.>])(\w+)\s*\(\s*\)")


@functools.cache
def _next_functions() -> frozenset[str]:
    """Every guest function the port declares (cport/include/lifted.h): `Next NAME(void)`."""
    path = REPO / "cport/include/lifted.h"
    return frozenset(re.findall(r"\bNext\s+(\w+)\s*\(void\)", path.read_text())) if path.exists() else frozenset()


def register_form_hazards(code: str) -> dict[FunctionName, str]:
    """The functions of an assembled farm file whose register-form code does what the tests can miss but the game
    does not survive: LR set to an address (RETURN_TO_CALLER returns there), or a guest function called as a C
    function instead of through GUEST_CALL (its continuation is dropped: a tail call or a multi-step return is cut
    short; startScreen_AnimatePressStart_803E938, old-prompt set 1, stopped the port at the title screen)."""
    bare = re.sub(r"/\*.*?\*/|//[^\n]*", lambda m: re.sub(r"[^\n]", " ", m[0]), code, flags=re.S)
    defined = set(re.findall(r"\bNext\s+(\w+)\s*\(\s*void\s*\)\s*\{", bare))
    found: dict[FunctionName, str] = {}
    for match in _LR_CONSTANT.finditer(bare):
        owner = function_at_line(code, bare.count("\n", 0, match.start()) + 1)
        if owner is not None:
            _ = found.setdefault(owner, f"sets LR to an address ({match[0].strip()}), so it returns there")
    nexts = _next_functions()
    for match in _CALL.finditer(bare):
        if match[1] in nexts and match[1] not in defined:
            owner = function_at_line(code, bare.count("\n", 0, match.start()) + 1)
            if owner is not None:
                _ = found.setdefault(owner, f"calls the guest function {match[1]} as a C function, not through GUEST_CALL")
    return found


def queued() -> frozenset[FunctionName]:
    return frozenset(row.function for row in read_queue())
