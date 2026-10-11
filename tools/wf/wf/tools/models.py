"""Outside models converting lifted functions, through the model farm's own
pieces (tools/convert/farm.py: prompt, code extraction, candidate test) and
tools/llm.py (providers, pacers, token log). Runs keep farm.py's log and
accepted-file formats, so tools/convert/scorecard.py keeps counting them."""
import hashlib
import json
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from datetime import datetime
from enum import Enum
from types import ModuleType
from typing import NewType, cast

from wf.repo import REPO
from wf.types import FunctionName, GuestAddress, ModelSpec, Result, SnapshotDir, ToolMessage

sys.path.insert(0, str(REPO / "tools/convert"))
import farm  # noqa: E402  # pyright: ignore[reportMissingImports]
sys.path.insert(0, str(REPO / "tools/convert/bench"))
import prompt_old  # noqa: E402  # pyright: ignore[reportMissingImports]
import prompt_typed1  # noqa: E402  # pyright: ignore[reportMissingImports]

FARM = cast(ModuleType, farm)  # farm.py's own pieces: prompts, flags, test
# the typed prompt before typed-2, verbatim: the convert workflow's subsystem prompt keeps its conventions
# (tools/wf/wf/tools/subsystem.py) until that prompt is benchmarked on its own
TYPED1 = cast(ModuleType, prompt_typed1)
_farm = FARM


class PromptKind(Enum):
    """Which prompt the farm asks with (`wf farm --prompt`; docs/reviews/2026-10-10-benchmark.md, plan B)."""
    TYPED = "typed"   # farm.py's live texts: typed functions and thin adapters, register check on
    OLD = "old"       # the benchmark's copy of the prompt before 329fbfd (tools/convert/bench/prompt_old.py):
                      # register-form C allowed, no register check; installs go to tools/abi/retype_queue.tsv

    @property
    def texts(self) -> ModuleType:
        """SYSTEM, CONVENTIONS, TEMPLATE and REPAIR."""
        return FARM if self is PromptKind.TYPED else cast(ModuleType, prompt_old)

    @property
    def checks_registers(self) -> bool:
        return self is PromptKind.TYPED

    @property
    def first_failure_only(self) -> bool:
        """The old prompt keeps the test the benchmark's old arm ran: typed-1's, which reports only the first
        failing check (farm.test(..., legacy=True)); the typed prompt's reports every problem."""
        return self is PromptKind.OLD


# A model's candidates under another prompt than the typed one carry the prompt in their model tag, so every
# consumer of tools/convert/log and tools/convert/accepted tells them apart: FUNC.minimax_MiniMax-M3+old.c.
# The same tagged spec names them elsewhere (wf convert --farm-model minimax/MiniMax-M3+old, commit messages).
PROMPT_MARK = "+"


def with_prompt(model: ModelSpec, prompt: PromptKind) -> ModelSpec:
    """The model as its candidates under PROMPT are filed: minimax/MiniMax-M3+old (the typed prompt: unchanged)."""
    return model if prompt is PromptKind.TYPED else ModelSpec(f"{model}{PROMPT_MARK}{prompt.value}")


def prompt_of(model: str) -> PromptKind:
    """The prompt a tagged model spec or file tag names (no mark: typed)."""
    base, mark, kind = model.rpartition(PROMPT_MARK)
    return PromptKind(kind) if mark and base and kind in {k.value for k in PromptKind} else PromptKind.TYPED


def base_model(model: ModelSpec) -> ModelSpec:
    """The model to ask, without a prompt mark."""
    return model if prompt_of(model) is PromptKind.TYPED else ModelSpec(model.rpartition(PROMPT_MARK)[0])

AssemblyText = NewType("AssemblyText", str)   # a function's bn6f assembly
LiftedC = NewType("LiftedC", str)             # a function's machine-translated C, with its contract header
Prompt = NewType("Prompt", str)               # what a model is asked
CandidateC = NewType("CandidateC", str)       # the readable C a model proposed
Register = NewType("Register", str)           # "r0".."r12", "lr", or a flag ("nz", "c", "v")

LOG_DIR = REPO / "tools/convert/log"
ACCEPTED_DIR = REPO / "tools/convert/accepted"
USAGE_LOG = REPO / "tools/convert/usage.jsonl"
MAX_TOKENS = 40000   # output cap per call (tools/convert/experiments/max_tokens-20261010.md)
HEADERS = tuple(REPO / "cport/include" / name for name in
                ("game.h", "cpu.h", "runtime.h", "bn6f_syms.h", "bn6f_equates.h"))


class Outcome(Enum):
    """How one round ended, in the log (the farm's resume rule reads RUNAWAY)."""
    PASSED = "passed"
    FAILED = "failed"            # the candidate failed a check
    RUNAWAY = "runaway"          # no code: the reply spent its output on reasoning; not asked again
    MODEL_ERROR = "model error"  # the provider failed (HTTP error, timeout)


def prompt_hash(prompt: PromptKind = PromptKind.TYPED) -> str:
    """Which prompt text a round used (its system prompt, conventions, template and repair text; the
    benchmark's prompt_hash gives the same for the same texts)."""
    module = prompt.texts
    texts = (cast(str, module.SYSTEM), cast(str, module.CONVENTIONS), cast(str, module.TEMPLATE),  # pyright: ignore[reportAny]
             cast(str, module.REPAIR))  # pyright: ignore[reportAny]
    return hashlib.sha1("\0".join(texts).encode()).hexdigest()[:12]


def prompt_version(prompt: PromptKind = PromptKind.TYPED) -> str:
    """The prompt's version (farm.py PROMPT_VERSION for the typed one: typed-2 since 2026-10-10), logged
    next to its hash."""
    return cast(str, _farm.PROMPT_VERSION) if prompt is PromptKind.TYPED else prompt.value  # pyright: ignore[reportAny]


_env_hashes: dict[tuple[str, float], str] = {}


def environment_hash(difftest: Path | None) -> str:
    """What a candidate was tested against: the headers it compiles with and the difftest binary."""
    digest = hashlib.sha1()
    for path in (*HEADERS, *((difftest,) if difftest is not None else ())):
        if not path.exists():
            continue
        key = (str(path), path.stat().st_mtime)
        if key not in _env_hashes:
            _env_hashes[key] = hashlib.sha1(path.read_bytes()).hexdigest()
        digest.update(_env_hashes[key].encode())
    return digest.hexdigest()[:12]


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
    entry_of: FunctionName | None = None  # a label callers enter in the middle of this function's code


class ModelReply(Result):
    candidate: CandidateC | None  # None when the reply had no ```c block
    seconds: float
    error: ToolMessage | None = None  # the provider failed (HTTP error, timeout)
    model: ModelSpec | None = None    # who answered (a farm run on a tier asks whichever source is up)


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


# `Next sub_X(void) { return U_0800XXXX(0x800XXXX); }`: one entry of lifted code with several
# (an override of the code defines all of them as L_ names in DIFFTEST builds)
_ENTRY_WRAPPER = re.compile(r"^Next (?:L_)?(\w+)\(void\) \{ return (U_[0-9A-F]{8})\(0x([0-9A-Fa-f]+)\); \}$", re.M)


def other_entries(context: ConversionContext) -> tuple[ConversionContext, ...]:
    """The labels callers enter in the middle of the function's code. A file replacing the code must
    define them all (the lifter refuses otherwise), so they belong to its conversion; each shares the
    function's code and register contract."""
    for path in sorted((REPO / "cport/src/lifted").glob("*.c")):
        text = path.read_text()
        if f"{context.function}(void) {{ return U_" not in text:
            continue
        wrappers = [(FunctionName(match[1]), match[2], GuestAddress(int(match[3], 16)))
                    for match in _ENTRY_WRAPPER.finditer(text)]
        unit = next((unit for name, unit, _ in wrappers if name == context.function), None)
        return tuple(context.model_copy(update={"function": name, "address": address, "entry_of": context.function})
                     for name, entry_unit, address in wrappers if entry_unit == unit and name != context.function)
    return ()


def provider_blocked_until(model: ModelSpec) -> float | None:
    """When the model's provider is rate-limited behind a local pacer, when that lifts (epoch seconds):
    the pacer would hold a request that long without saying so (tools/llm.py reads its log)."""
    llm = cast(ModuleType, _farm.llm)  # pyright: ignore[reportAny]
    return cast(float | None, llm.blocked_until(model))  # pyright: ignore[reportAny]


def first_prompt(context: ConversionContext, prompt: PromptKind = PromptKind.TYPED) -> Prompt:
    """The prompt's TEMPLATE for the function; the typed one also lists the generated wrappers of the
    functions it calls (farm.py callees_section)."""
    contract_text = "\n".join(f"  {part}: {' '.join(names) or '-'}" for part, names in context.contract.by_part().items())
    template = cast(str, prompt.texts.TEMPLATE)  # pyright: ignore[reportAny]
    callees = (cast(str, _farm.callees_section(context.function, context.lifted))  # pyright: ignore[reportAny]
               if "{callees}" in template else "")
    return Prompt(template.format(conventions=cast(str, prompt.texts.CONVENTIONS), name=context.function,  # pyright: ignore[reportAny]
                                  addr=context.address, contract=contract_text, asm=context.assembly,
                                  lifted=context.lifted, callees=callees))


def repair_prompt(context: ConversionContext, failure: ToolMessage, candidate: CandidateC | None,
                  prompt: PromptKind = PromptKind.TYPED, whose: str = "your last attempt", history: str = "") -> Prompt:
    """The first prompt plus REPAIR: the attempt to start from (WHOSE it is: the typed prompt's repair
    names it) and its report, and (the typed prompt) the one-line HISTORY of the other attempts."""
    repair = cast(str, prompt.texts.REPAIR)  # pyright: ignore[reportAny]
    lines = cast(str, _farm.HISTORY).format(attempts=history) if history else ""  # pyright: ignore[reportAny]
    return Prompt(first_prompt(context, prompt) + "\n\n" + repair.format(failure=failure[:3000], code=candidate or "",
                                                                         whose=whose, history=lines))


def ask_model(model: ModelSpec, prompt: Prompt, tag: str, wait_out_limits: bool = True,
              kind: PromptKind = PromptKind.TYPED) -> ModelReply:
    """wait_out_limits=False: a rate limit comes back as an error at once (another source can answer).
    KIND: whose system prompt goes with it."""
    llm = cast(ModuleType, _farm.llm)  # pyright: ignore[reportAny]
    started = time.monotonic()
    try:
        reply = cast(str, llm.complete(model, prompt, system=cast(str, kind.texts.SYSTEM), max_tokens=MAX_TOKENS,  # pyright: ignore[reportAny]
                                       tag=tag, usage_log=str(USAGE_LOG), wait_out_limits=wait_out_limits))
    except Exception as error:  # the provider's own errors: HTTP, timeouts, bad JSON
        return ModelReply(candidate=None, seconds=round(time.monotonic() - started, 1),
                          error=ToolMessage(f"model error: {error}"[:500]), model=model)
    code = cast(str | None, _farm.extract_code(reply))  # pyright: ignore[reportAny]
    return ModelReply(candidate=CandidateC(code) if code else None, seconds=round(time.monotonic() - started, 1),
                      model=model)


def test_candidate(context: ConversionContext, candidate: CandidateC, snapshots: SnapshotDir,
                   difftest: Path, check_registers: bool = True, first_failure_only: bool = False) -> CandidateTest:
    """farm.py's acceptance test: compiles, not lifted style, matches the lifted original
    on the captured calls (some compared) and on random inputs; and (CHECK_REGISTERS) uses guest
    registers only in thin adapters and noted helpers."""
    setattr(_farm, "DIFFTEST", str(difftest))
    contract = {part: list(names) for part, names in context.contract.by_part().items()}
    with tempfile.TemporaryDirectory() as workdir:
        try:
            passed, report = cast(tuple[bool, str], _farm.test(context.function, candidate, str(snapshots),  # pyright: ignore[reportAny]
                                                               contract, workdir, check_registers=check_registers,
                                                               legacy=first_failure_only))
        except subprocess.TimeoutExpired:
            return CandidateTest(passed=False, report=ToolMessage("difftest timed out (900 s)"))
    return CandidateTest(passed=passed, report=ToolMessage(report))


def round_outcome(reply: ModelReply, test: CandidateTest | None) -> Outcome:
    if reply.error is not None:
        return Outcome.MODEL_ERROR
    if test is None:
        return Outcome.RUNAWAY
    return Outcome.PASSED if test.passed else Outcome.FAILED


def log_round(function: FunctionName, model: ModelSpec, round_number: int, reply: ModelReply,
              test: CandidateTest | None, difftest: Path | None = None, prompt: PromptKind = PromptKind.TYPED) -> None:
    """One row in tools/convert/log/FUNC.MODEL.jsonl (FUNC.MODEL+old.jsonl for the old prompt): farm.py's
    fields (round, ok, seconds, report, code) and where it came from (review 3 Area 6: time, prompt kind and
    hashes, test-environment hash, cap, the function's input key, outcome), failed provider calls included."""
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    report = reply.error or (test.report if test else "the reply had no ```c code block")
    row = {"round": round_number, "ok": bool(test and test.passed), "seconds": round(reply.seconds),
           "report": report, "code": reply.candidate,
           "time": datetime.now().isoformat(timespec="seconds"), "model": reply.model or model, "kind": "farm",
           "outcome": round_outcome(reply, test).value, "prompt": prompt.value, "prompt_version": prompt_version(prompt),
           "prompt_hash": prompt_hash(prompt),
           "environment_hash": environment_hash(difftest), "max_tokens": MAX_TOKENS,
           "input_key": cast(str | None, _farm.input_key(function))}  # pyright: ignore[reportAny]
    with open(LOG_DIR / f"{function}.{_tag(with_prompt(model, prompt))}.jsonl", "a") as log:
        log.write(json.dumps(row) + "\n")


def finished(function: FunctionName, candidate: CandidateC) -> CandidateC:
    """A passing typed candidate as it is kept: with the generated wrappers it calls and the tool's
    register-form notes, so it compiles and passes the register check on its own (farm.py finished)."""
    return CandidateC(cast(str, _farm.finished(function, candidate)))  # pyright: ignore[reportAny]


def accept(function: FunctionName, model: ModelSpec, candidate: CandidateC) -> Path:
    """MODEL as filed: with_prompt(model, prompt) for another prompt than the typed one. A typed
    candidate comes finished (finished())."""
    ACCEPTED_DIR.mkdir(parents=True, exist_ok=True)
    path = ACCEPTED_DIR / f"{function}.{_tag(model)}.c"
    path.write_text(candidate.strip() + "\n")
    return path


def already_accepted(function: FunctionName, model: ModelSpec) -> Path | None:
    path = ACCEPTED_DIR / f"{function}.{_tag(model)}.c"
    return path if path.exists() else None


def _tag(model: ModelSpec) -> str:
    return re.sub(r"/", "_", model)


def logged_rounds(function: FunctionName, model: ModelSpec) -> int:
    """How many rounds farm runs have logged for this function and model."""
    log = LOG_DIR / f"{function}.{_tag(model)}.jsonl"
    return sum(1 for line in log.read_text().splitlines() if line.strip()) if log.exists() else 0


def out_of_rounds_before(function: FunctionName, model: ModelSpec, rounds: int) -> bool:
    """farm.py's resume rule: a pair whose log reached the last round is finished; so is one whose
    reply was a runaway (asking the same model the same thing again wastes its cap: review 3, B3)."""
    log = LOG_DIR / f"{function}.{_tag(model)}.jsonl"
    if not log.exists():
        return False
    rows = [FarmLogRow.model_validate_json(line) for line in log.read_text().splitlines() if line.strip()]
    return any(row.round >= rounds or row.outcome == Outcome.RUNAWAY.value for row in rows)


class FarmLogRow(Result):
    """The fields of a tools/convert/log row the resume rule reads."""
    round: int
    ok: bool
    outcome: str | None = None   # rows before 2026-10-10 have none
