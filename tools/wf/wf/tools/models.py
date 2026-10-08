"""Outside models converting lifted functions, through the model farm's own
pieces (tools/convert/farm.py: prompt, code extraction, candidate test) and
tools/llm.py (providers, pacers, token log). Runs keep farm.py's log and
accepted-file formats, so tools/convert/scorecard.py keeps counting them."""
import json
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from types import ModuleType
from typing import NewType, cast

from wf.repo import REPO
from wf.types import FunctionName, GuestAddress, ModelSpec, Result, SnapshotDir, ToolMessage

sys.path.insert(0, str(REPO / "tools/convert"))
import farm  # noqa: E402  # pyright: ignore[reportMissingImports]

FARM = cast(ModuleType, farm)  # farm.py's own pieces: prompts, flags, test
_farm = FARM

AssemblyText = NewType("AssemblyText", str)   # a function's bn6f assembly
LiftedC = NewType("LiftedC", str)             # a function's machine-translated C, with its contract header
Prompt = NewType("Prompt", str)               # what a model is asked
CandidateC = NewType("CandidateC", str)       # the readable C a model proposed
Register = NewType("Register", str)           # "r0".."r12", "lr", or a flag ("nz", "c", "v")

LOG_DIR = REPO / "tools/convert/log"
ACCEPTED_DIR = REPO / "tools/convert/accepted"
USAGE_LOG = REPO / "tools/convert/usage.jsonl"


class Contract(Result):
    """The lifter's register contract for a function (from its lifted header)."""
    inputs: tuple[Register, ...]
    outputs: tuple[Register, ...]
    clobbers: tuple[Register, ...]
    preserved: tuple[Register, ...]


    def by_part(self) -> dict[str, tuple[Register, ...]]:
        return {"inputs": self.inputs, "outputs": self.outputs, "clobbers": self.clobbers, "preserved": self.preserved}


class ConversionContext(Result):
    """Everything a model is shown about a function."""
    function: FunctionName
    address: GuestAddress
    assembly: AssemblyText
    lifted: LiftedC
    contract: Contract


class ModelReply(Result):
    candidate: CandidateC | None  # None when the reply had no ```c block
    seconds: float
    error: ToolMessage | None = None  # the provider failed (HTTP error, timeout)


class CandidateTest(Result):
    passed: bool
    report: ToolMessage


def conversion_context(function: FunctionName) -> ConversionContext | None:
    lifted, address = cast(tuple[str | None, int | None], _farm.find_lifted(function))  # pyright: ignore[reportAny]
    assembly = cast(str | None, _farm.find_asm(function))  # pyright: ignore[reportAny]
    if lifted is None or address is None or assembly is None:
        return None
    contract = cast(dict[str, list[str]], _farm.contract_of(lifted))  # pyright: ignore[reportAny]
    return ConversionContext(
        function=function, address=GuestAddress(address), assembly=AssemblyText(assembly), lifted=LiftedC(lifted),
        contract=Contract(**{part: tuple(Register(name) for name in names) for part, names in contract.items()}),
    )


def first_prompt(context: ConversionContext) -> Prompt:
    contract_text = "\n".join(f"  {part}: {' '.join(names) or '-'}" for part, names in context.contract.by_part().items())
    template = cast(str, _farm.TEMPLATE)  # pyright: ignore[reportAny]
    return Prompt(template.format(conventions=cast(str, _farm.CONVENTIONS), name=context.function,  # pyright: ignore[reportAny]
                                  addr=context.address, contract=contract_text, asm=context.assembly,
                                  lifted=context.lifted))


def repair_prompt(context: ConversionContext, failure: ToolMessage, candidate: CandidateC | None) -> Prompt:
    repair = cast(str, _farm.REPAIR)  # pyright: ignore[reportAny]
    return Prompt(first_prompt(context) + "\n\n" + repair.format(failure=failure[:3000], code=candidate or ""))


def ask_model(model: ModelSpec, prompt: Prompt, tag: str) -> ModelReply:
    llm = cast(ModuleType, _farm.llm)  # pyright: ignore[reportAny]
    started = time.monotonic()
    try:
        reply = cast(str, llm.complete(model, prompt, system=cast(str, _farm.SYSTEM), max_tokens=24000,  # pyright: ignore[reportAny]
                                       tag=tag, usage_log=str(USAGE_LOG)))
    except Exception as error:  # the provider's own errors: HTTP, timeouts, bad JSON
        return ModelReply(candidate=None, seconds=round(time.monotonic() - started, 1),
                          error=ToolMessage(f"model error: {error}"[:500]))
    code = cast(str | None, _farm.extract_code(reply))  # pyright: ignore[reportAny]
    return ModelReply(candidate=CandidateC(code) if code else None, seconds=round(time.monotonic() - started, 1))


def test_candidate(context: ConversionContext, candidate: CandidateC, snapshots: SnapshotDir,
                   difftest: Path) -> CandidateTest:
    """farm.py's acceptance test: compiles, not lifted style, matches the lifted original
    on the captured calls (some compared) and on random inputs."""
    setattr(_farm, "DIFFTEST", str(difftest))
    contract = {part: list(names) for part, names in context.contract.by_part().items()}
    with tempfile.TemporaryDirectory() as workdir:
        try:
            passed, report = cast(tuple[bool, str], _farm.test(context.function, candidate, str(snapshots),  # pyright: ignore[reportAny]
                                                               contract, workdir))
        except subprocess.TimeoutExpired:
            return CandidateTest(passed=False, report=ToolMessage("difftest timed out (900 s)"))
    return CandidateTest(passed=passed, report=ToolMessage(report))


def log_round(function: FunctionName, model: ModelSpec, round_number: int, reply: ModelReply,
              test: CandidateTest | None) -> None:
    """One row in tools/convert/log/FUNC.MODEL.jsonl, as farm.py writes them."""
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    report = reply.error or (test.report if test else "the reply had no ```c code block")
    row = {"round": round_number, "ok": bool(test and test.passed), "seconds": round(reply.seconds),
           "report": report, "code": reply.candidate}
    with open(LOG_DIR / f"{function}.{_tag(model)}.jsonl", "a") as log:
        log.write(json.dumps(row) + "\n")


def accept(function: FunctionName, model: ModelSpec, candidate: CandidateC) -> Path:
    ACCEPTED_DIR.mkdir(parents=True, exist_ok=True)
    path = ACCEPTED_DIR / f"{function}.{_tag(model)}.c"
    path.write_text(candidate.strip() + "\n")
    return path


def already_accepted(function: FunctionName, model: ModelSpec) -> Path | None:
    path = ACCEPTED_DIR / f"{function}.{_tag(model)}.c"
    return path if path.exists() else None


def _tag(model: ModelSpec) -> str:
    return re.sub(r"/", "_", model)


def out_of_rounds_before(function: FunctionName, model: ModelSpec, rounds: int) -> bool:
    """farm.py's resume rule: a pair whose log reached the last round is finished."""
    log = LOG_DIR / f"{function}.{_tag(model)}.jsonl"
    if not log.exists():
        return False
    rows = [FarmLogRow.model_validate_json(line) for line in log.read_text().splitlines() if line.strip()]
    return any(row.round >= rounds for row in rows)


class FarmLogRow(Result):
    """The fields of a tools/convert/log row the resume rule reads."""
    round: int
    ok: bool
