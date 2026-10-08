"""Compare readable functions with their lifted originals on random inputs
(cport/build-host/difftest). Its text output is parsed here and nowhere else.

Demo limitation: this uses the host difftest binary already built in the main
checkout; the full workflow builds its own, as it does the ROM.
"""
import os
import re
import subprocess
from pathlib import Path

from wf.repo import CPORT, ORIGINAL_ROM
from wf.tools.errors import ToolOutputError
from wf.types import DifftestResult, FunctionName, Outcome

DIFFTEST = CPORT / "build-host" / "difftest"

# "setBGTileIdsBlocks1And2_8030628   ok    (300 trials, 0 failures, 299 out-of-domain)"
_SUMMARY = re.compile(r"^(\w+)\s+(ok|FAIL)\s+\((\d+) trials, (\d+) failures, (\d+) out-of-domain\)")


def run_difftest(functions: tuple[FunctionName, ...], trials: int, log_dir: Path) -> tuple[DifftestResult, ...]:
    env = {**os.environ, "BN6F_ROM": str(ORIGINAL_ROM)}
    done = subprocess.run([str(DIFFTEST), "-n", str(trials), *functions],
                          cwd=CPORT, capture_output=True, text=True, env=env, check=False)
    (log_dir / "difftest.log").write_text(done.stdout + done.stderr)
    results = {result.function: result for result in _parse(done.stdout)}
    missing = [function for function in functions if function not in results]
    if missing:
        raise ToolOutputError(f"difftest printed no result for {missing} (exit {done.returncode})")
    return tuple(results[function] for function in functions)


def _parse(output: str) -> list[DifftestResult]:
    results: list[DifftestResult] = []
    for line in output.splitlines():
        match = _SUMMARY.match(line)
        if match is None:
            continue
        trials, failures, out_of_domain = int(match[3]), int(match[4]), int(match[5])
        results.append(DifftestResult(
            function=FunctionName(match[1]),
            outcome=_outcome(trials, failures, out_of_domain),
            trials=trials,
            failures=failures,
            out_of_domain=out_of_domain,
        ))
    return results


def _outcome(trials: int, failures: int, out_of_domain: int) -> Outcome:
    if failures > 0:
        return Outcome.FAILED
    if out_of_domain == trials:
        return Outcome.COMPARED_NOTHING
    return Outcome.PASSED
