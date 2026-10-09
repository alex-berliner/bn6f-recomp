"""The register-use gate (docs/C_CALLING_CONVENTION.md §3.8): a candidate file may use guest
registers only in thin `Next` adapters, or where a `/* register form: <why> */` note says why C
semantics cannot work. tools/abi/lint.py does the checking; this runs it on a candidate file
(before any test) and turns its report into what the model is shown."""
import json
import subprocess
import tempfile
from pathlib import Path

from pydantic import TypeAdapter

from wf.repo import REPO
from wf.types import Result, ToolMessage

LINT = REPO / "tools/abi/lint.py"


class LintedFunction(Result):
    """One function as tools/abi/lint.py reports it."""
    name: str
    kind: str                         # typed | next | adapter | file
    line: int
    counts: dict[str, int]            # hits per category (regs, flags, cpu, guest_call, ...)
    lines: dict[str, list[int]]
    exception: str | None             # the `register form:` reason, if any
    violation: bool


class RegisterCheck(Result):
    violations: tuple[LintedFunction, ...]
    exceptions: tuple[ToolMessage, ...]   # "NAME: why", for the review and the symbol entries
    report: str                           # what the model is shown when there are violations

    @property
    def clean(self) -> bool:
        return not self.violations


_REPORT = TypeAdapter(dict[str, list[LintedFunction]])


def check_register_use(code: str, file_name: str) -> RegisterCheck:
    """Lint the candidate file as cport/src/game/FILE_NAME would be linted."""
    with tempfile.TemporaryDirectory(prefix="wf-lint-") as scratch:
        path = Path(scratch) / file_name
        _ = path.write_text(code)
        done = subprocess.run(["python3", str(LINT), "--quiet", str(path)], capture_output=True, text=True,
                              check=False)
    if done.returncode != 0 or not done.stdout.strip():
        raise RuntimeError(f"tools/abi/lint.py failed on the candidate file: {done.stderr.strip()[-500:]}")
    files = _REPORT.validate_python(json.loads(done.stdout)["files"])
    functions = files.get(file_name, [])
    violations = tuple(function for function in functions if function.violation)
    exceptions = tuple(ToolMessage(f"{function.name}: {function.exception}") for function in functions
                       if function.exception)
    return RegisterCheck(violations=violations, exceptions=exceptions, report=_report(violations))


def _report(violations: tuple[LintedFunction, ...]) -> str:
    if not violations:
        return ""
    lines = ["These functions use guest registers (R0..R12, SP, LR, flags, cpu., GUEST_CALL, RETURN_*) "
             "where the target convention does not allow it:"]
    for function in violations:
        where = "; ".join(f"{category} on line{'s' if len(numbers) > 1 else ''} "
                          f"{', '.join(str(number) for number in sorted(set(numbers)))}"
                          for category, numbers in sorted(function.lines.items()))
        lines.append(f"- {function.name} (line {function.line}, {_KIND[function.kind]}): {where}")
    lines.append(
        "Fix: typed functions take parameters and return results and never touch registers. Each "
        "`Next NAME(void)` is a thin adapter: argument registers in, one call of the typed function, result "
        "registers and flags out (no memory access, no branches, no GUEST_CALL). A call to a function "
        "outside this file goes through a small helper that sets its registers, GUEST_CALLs it and returns "
        "its result, with `/* register form: calls NAME outside this file, which has no typed C interface "
        "yet */` right above it. Any other register use needs a `/* register form: <why> */` note that "
        "really rules C out.")
    return "\n".join(lines)


_KIND = {
    "typed": "a typed function",
    "next": "a register body, not a thin adapter",
    "adapter": "an adapter",
    "file": "file scope",
}
