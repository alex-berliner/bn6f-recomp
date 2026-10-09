"""Run functions on the original game (mGBA, real BIOS) and on the port from
the same entry states built from real game memory (tools/oracle.py).
oracle.py exits 0 even when functions differ; its report file is parsed."""
import os
import re
import subprocess
from pathlib import Path

from wf.repo import REPO
from wf.tools.errors import ToolOutputError
from wf.types import FunctionName, GuestAddress, OracleResult, SnapshotDir, ToolMessage

# "sprite_queueOamEntry 08002f04 ok=20 fail=0 ood=4 MEM: ... | REG: ..."
_REPORT = re.compile(r"^(\w+) ([0-9a-f]{8}) ok=(\d+) fail=(\d+) ood=(\d+) ?(.*)$")


def run_oracle(difftest: Path, functions: tuple[FunctionName, ...], snapshots: SnapshotDir, states: int,
               log_dir: Path, tree: Path = REPO) -> tuple[OracleResult, ...]:
    """tree: whose tools/oracle.py runs (it reads that tree's difftest.cfg)."""
    out = log_dir / "oracle"
    temporary = log_dir / "oracle-tmp"
    temporary.mkdir(parents=True, exist_ok=True)
    done = subprocess.run(
        ["python3", str(tree / "tools/oracle.py"), "--base", str(snapshots), "--states", str(states), "--out", str(out), *functions],
        capture_output=True, text=True, check=False,
        env={**os.environ, "ORACLE_TMP": str(temporary), "ORACLE_DIFFTEST": str(difftest)})
    (log_dir / "oracle.log").write_text(done.stdout + done.stderr)
    report = out / "report.txt"
    if done.returncode != 0 or not report.exists():
        raise ToolOutputError(f"oracle.py failed (exit {done.returncode}), see {log_dir / 'oracle.log'}")
    results = {result.function: result for result in _parse(report.read_text())}
    return tuple(results.get(function, _not_known_to_the_oracle(function)) for function in functions)


def _parse(report: str) -> list[OracleResult]:
    results: list[OracleResult] = []
    for match in filter(None, map(_REPORT.match, report.splitlines())):
        differences = tuple(ToolMessage(part.strip()) for part in match[6].split(" | ") if part.strip())
        results.append(OracleResult(function=FunctionName(match[1]), address=GuestAddress(int(match[2], 16)),
                                    compared=int(match[3]) + int(match[4]), differed=int(match[4]),
                                    out_of_domain=int(match[5]), differences=differences))
    return results


def _not_known_to_the_oracle(function: FunctionName) -> OracleResult:
    """oracle.py silently drops names it does not test (functions with no readable version)."""
    return OracleResult(function=function, address=GuestAddress(0), compared=0, differed=0, out_of_domain=0,
                        differences=(ToolMessage("oracle.py did not test this function (no readable version?)"),))
