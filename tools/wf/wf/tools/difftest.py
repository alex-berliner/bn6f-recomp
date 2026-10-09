"""Compare readable functions with their lifted originals (cport/host
difftest): on random inputs, and on the original game's own captured calls.
Its text output is parsed here and nowhere else."""
import os
import re
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from wf.repo import CPORT, ORIGINAL_ROM
from wf.tools.errors import ToolOutputError
from wf.types import CapturedDifftestResult, DifftestResult, FunctionName, Outcome, SnapshotDir

# "setBGTileIdsBlocks1And2_8030628   ok    (300 trials, 0 failures, 299 out-of-domain)"
_RANDOM = re.compile(r"^(\w+)\s+(ok|FAIL)\s+\((\d+) trials, (\d+) failures, (\d+) out-of-domain\)")
# "setBGTileIdsBlocks1And2_8030628   ok    (40 snapshots, 40 runs, 0 failures, 0 out-of-domain)"
_CAPTURED = re.compile(r"^(\w+)\s+(ok|FAIL)\s+\((\d+) snapshots, (\d+) runs, (\d+) failures, (\d+) out-of-domain\)")
JOBS = os.cpu_count() or 4   # difftest runs one function at a time: functions are spread over processes


def run_difftest(difftest: Path, functions: tuple[FunctionName, ...], trials: int,
                 log_dir: Path) -> tuple[DifftestResult, ...]:
    shares = [functions[start::JOBS] for start in range(min(JOBS, len(functions)))]

    def run_share(number: int) -> str:
        log = log_dir / ("difftest-random.log" if len(shares) == 1 else f"difftest-random-{number}.log")
        return _difftest(difftest, ["-n", str(trials), *shares[number]], log)

    with ThreadPoolExecutor(max_workers=max(1, len(shares))) as pool:
        outputs = list(pool.map(run_share, range(len(shares))))
    results = {result.function: result for output in outputs for result in _parse_random(output)}
    return tuple(_result_for(function, results, "random") for function in functions)


def run_captured_difftest(difftest: Path, functions: tuple[FunctionName, ...], snapshots: SnapshotDir,
                          log_dir: Path) -> tuple[CapturedDifftestResult, ...]:
    def run_one(function: FunctionName) -> str:
        return _difftest(difftest, ["-S", str(snapshots / function), function],
                         log_dir / f"difftest-captured-{function}.log")

    with ThreadPoolExecutor(max_workers=JOBS) as pool:
        outputs = list(pool.map(run_one, functions))
    results = {result.function: result for output in outputs for result in _parse_captured(output)}
    return tuple(_result_for(function, results, "captured") for function in functions)


def _difftest(difftest: Path, arguments: list[str], log: Path) -> str:
    done = subprocess.run([str(difftest), *arguments], cwd=CPORT, capture_output=True, text=True,
                          env={**os.environ, "BN6F_ROM": str(ORIGINAL_ROM)}, check=False)
    log.write_text(done.stdout + done.stderr)
    return done.stdout


def _result_for[ResultT](function: FunctionName, results: dict[FunctionName, ResultT], kind: str) -> ResultT:
    if function not in results:
        raise ToolOutputError(f"difftest ({kind}) printed no result for {function}")
    return results[function]


def _parse_random(output: str) -> list[DifftestResult]:
    results: list[DifftestResult] = []
    for match in filter(None, map(_RANDOM.match, output.splitlines())):
        trials, failures, out_of_domain = int(match[3]), int(match[4]), int(match[5])
        results.append(DifftestResult(function=FunctionName(match[1]), outcome=_outcome(trials, failures, out_of_domain),
                                      trials=trials, failures=failures, out_of_domain=out_of_domain))
    return results


def _parse_captured(output: str) -> list[CapturedDifftestResult]:
    results: list[CapturedDifftestResult] = []
    for match in filter(None, map(_CAPTURED.match, output.splitlines())):
        snapshots, runs, failures, out_of_domain = int(match[3]), int(match[4]), int(match[5]), int(match[6])
        results.append(CapturedDifftestResult(function=FunctionName(match[1]),
                                              outcome=_outcome(runs, failures, out_of_domain),
                                              snapshots=snapshots, runs=runs, failures=failures,
                                              out_of_domain=out_of_domain))
    return results


def _outcome(tries: int, failures: int, out_of_domain: int) -> Outcome:
    if failures > 0:
        return Outcome.FAILED
    if out_of_domain == tries:
        return Outcome.COMPARED_NOTHING
    return Outcome.PASSED
