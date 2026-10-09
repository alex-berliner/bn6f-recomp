"""A farm model drafts the symbols/db entry of each candidate it got accepted,
beside it (tools/convert/accepted/NAME.MODEL.yaml), and a group of accepted
candidates becomes one file for the convert workflow: the code, and an entry
for every function, so review and record never wait on a strong model for
the documentation of small functions."""
import time
from pathlib import Path
from types import ModuleType
from typing import cast

import yaml

from wf.tools.models import ACCEPTED_DIR, FARM, USAGE_LOG, ConversionContext, Prompt, conversion_context
from wf.tools.subsystem import CandidateFile, FileReply, SymbolDraft, parse_drafts, yaml_blocks
from wf.types import FunctionName, ModelSpec, Result, ToolMessage

DRAFT_SYSTEM = "You document functions of a GBA game's disassembly for its symbol database. Reply exactly in the format asked."

DRAFT_REQUEST = """Below is one function of MegaMan Battle Network 6 Falzar (GBA): its assembly from the
bn6f disassembly, its register contract (the lifter's analysis of every caller), and a readable C
version that was checked against the original. Say what it does in game terms.

### {function} (at {address:#010x})
Register contract:
{contract}
Assembly:
```
{assembly}
```
Readable C (verified):
```c
{code}
```

Reply with one ```yaml block holding a list with exactly one entry:
- function: {function}
  proposed_name: a name in game terms, keeping the _ADDRESS suffix when the name has one (or the current name)
  status: probable or hypothesis (how sure the meaning is from the code alone)
  summary: one line in game terms
  interface: its registers in and out (and flags), as a C-like signature where you can
  details: what it does, structures and fields it touches, quirks
  open_questions: [list]
  register_form: null
Quote any value that contains a colon.
"""


class Draft(Result):
    function: FunctionName
    path: Path | None          # the written NAME.MODEL.yaml
    problem: ToolMessage | None
    seconds: float


def draft_path(candidate: Path) -> Path:
    return candidate.with_suffix(".yaml")


def undrafted(model: ModelSpec, functions: tuple[FunctionName, ...] | None) -> tuple[Path, ...]:
    """The model's accepted candidates (of `functions`, or all) that have no draft yet."""
    tag = model.replace("/", "_")
    candidates = sorted(ACCEPTED_DIR.glob(f"*.{tag}.c"))
    wanted = set(functions) if functions is not None else None
    return tuple(path for path in candidates if not draft_path(path).exists()
                 and (wanted is None or path.name.split(".")[0] in wanted))


def draft_entry(model: ModelSpec, candidate: Path, tag: str) -> Draft:
    """Ask the model for the candidate's entry (once more if the yaml is unusable) and write it."""
    started = time.monotonic()
    function = FunctionName(candidate.name.split(".")[0])
    context = conversion_context(function)
    if context is None:
        return Draft(function=function, path=None, problem=ToolMessage("no lifted code or assembly"), seconds=0.0)
    prompt = _request(context, candidate.read_text())
    problem = ToolMessage("")
    for attempt in range(2):
        try:
            reply = _ask(model, prompt, f"{tag} draft {function} {attempt}")
        except RuntimeError as error:
            problem = ToolMessage(f"model error: {error}"[:300])
            continue
        blocks = yaml_blocks(reply)
        drafts, problems = parse_drafts(blocks[0]) if blocks else ((), (ToolMessage("no ```yaml block"),))
        mine = [draft for draft in drafts if draft.function == function]
        if mine:
            path = draft_path(candidate)
            path.write_text(yaml.safe_dump([mine[0].model_dump(mode="json")], sort_keys=False, allow_unicode=True))
            return Draft(function=function, path=path, problem=None, seconds=round(time.monotonic() - started, 1))
        problem = problems[0] if problems else ToolMessage(f"no entry for {function}")
        prompt = Prompt(prompt + f"\n\nYour previous reply could not be used: {problem}\nReply again in the format asked.")
    return Draft(function=function, path=None, problem=problem, seconds=round(time.monotonic() - started, 1))


def candidate_from_farm(functions: tuple[FunctionName, ...], model: ModelSpec, subsystem: str,
                        file_name: str) -> FileReply:
    """The model's accepted candidates of `functions` as one file, with their drafted entries."""
    tag = model.replace("/", "_")
    bodies: list[str] = []
    drafts: list[SymbolDraft] = []
    problems: list[ToolMessage] = []
    for function in functions:
        candidate = ACCEPTED_DIR / f"{function}.{tag}.c"
        if not candidate.exists():
            problems.append(ToolMessage(f"no accepted {model} candidate for {function}"))
            continue
        bodies.append(candidate.read_text().strip())
        if draft_path(candidate).exists():
            found, _ = parse_drafts(draft_path(candidate).read_text())
            drafts += found
    header = (f"/*\n * {file_name}: {subsystem}.\n *\n * Small functions converted one at a time by the "
              f"model farm, each checked against\n * the lifted original before it was kept.\n */\n"
              '#include "game.h"\n#include "runtime.h"\n')
    code = header + "\n" + "\n\n".join(bodies) + "\n" if bodies else None
    return FileReply(file=CandidateFile(code) if code else None, cfg_lines=(), drafts=tuple(drafts),
                     problems=tuple(problems), seconds=0.0)


def _request(context: ConversionContext, code: str) -> Prompt:
    contract = "\n".join(f"  {part}: {' '.join(names) or '-'}" for part, names in context.contract.by_part().items())
    return Prompt(DRAFT_REQUEST.format(function=context.function, address=context.address, contract=contract,
                                       assembly=context.assembly, code=code.strip()))


def _ask(model: ModelSpec, prompt: Prompt, tag: str) -> str:
    llm = cast(ModuleType, FARM.llm)  # pyright: ignore[reportAny]
    return cast(str, llm.complete(model, prompt, system=DRAFT_SYSTEM, max_tokens=16000,  # pyright: ignore[reportAny]
                                  timeout=600, retries=2, tag=tag, usage_log=str(USAGE_LOG)))
