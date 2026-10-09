"""A farm model drafts the symbols/db entry of each candidate it got accepted,
beside it (tools/convert/accepted/NAME.MODEL.yaml), and a group of accepted
candidates becomes one file for the convert workflow: the code, and an entry
for every function, so review and record never wait on a strong model for
the documentation of small functions."""
import re
import time
from pathlib import Path
from types import ModuleType
from typing import cast

import yaml

from wf.tools.models import ACCEPTED_DIR, FARM, USAGE_LOG, ConversionContext, Prompt, conversion_context
from wf.tools.subsystem import CandidateFile, FileReply, SymbolDraft, parse_drafts, yaml_blocks
from wf.types import FunctionName, ModelSpec, Result, ToolMessage

DRAFT_SYSTEM = "You document functions of a GBA game's disassembly for its symbol database. Reply exactly in the format asked."

# The fixed instructions come first and the function last, so every request shares a long prefix
# that the provider can cache (Hyper caches whole 256-token blocks of a shared prefix).
DRAFT_FORMAT = """You will be shown one function of MegaMan Battle Network 6 Falzar (GBA): its assembly from the
bn6f disassembly, its register contract (the lifter's analysis of every caller: which registers and
flags callers pass in, read back, or ignore), and a readable C version that was checked against the
original game. Say what the function does in game terms, for the project's symbol database.

Reply with one ```yaml block holding a list with exactly one entry:
- function: the function's current name, exactly as given
  proposed_name: a name in game terms in the disassembly's style (camelCase, a subsystem prefix such
    as sprite_, npc_, chatbox_ or battle_ where one fits) that keeps the current name's address
    suffix (sub_80342DC -> setStatePhase4_80342DC); or the current name when nothing better is known
  status: probable or hypothesis
  summary: one line in game terms
  interface: its registers in and out (and flags), as a C-like signature where you can
  details: what it does, the structures and fields it touches, quirks
  open_questions: [list]
  register_form: null
Status: probable = read from the code and consistent with what it touches, not independently
checked; hypothesis = a guess with partial evidence (it gets a comment, not a rename). Quote any
value that contains a colon. Nothing outside the yaml block.

"""

DRAFT_FUNCTION = """### {function} (at {address:#010x})
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
"""


class Draft(Result):
    function: FunctionName
    path: Path | None          # the written NAME.MODEL.yaml
    problem: ToolMessage | None
    seconds: float


def draft_path(candidate: Path) -> Path:
    return candidate.with_suffix(".yaml")


def undrafted(model: ModelSpec, functions: tuple[FunctionName, ...] | None) -> tuple[Path, ...]:
    """The model's accepted candidates (of `functions`, in their order, or all) that have no draft yet."""
    tag = model.replace("/", "_")
    if functions is not None:
        candidates = [ACCEPTED_DIR / f"{function}.{tag}.c" for function in functions]
        return tuple(path for path in candidates if path.exists() and not draft_path(path).exists())
    return tuple(path for path in sorted(ACCEPTED_DIR.glob(f"*.{tag}.c")) if not draft_path(path).exists())


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
    bodies: list[tuple[FunctionName, str]] = []
    drafts: list[SymbolDraft] = []
    problems: list[ToolMessage] = []
    for function in functions:
        candidate = ACCEPTED_DIR / f"{function}.{tag}.c"
        if not candidate.exists():
            problems.append(ToolMessage(f"no accepted {model} candidate for {function}"))
            continue
        bodies.append((function, candidate.read_text().strip()))
        if draft_path(candidate).exists():
            found, _ = parse_drafts(draft_path(candidate).read_text())
            drafts += found
    header = (f"/*\n * {file_name}: {subsystem}.\n *\n * Small functions converted one at a time by the "
              f"model farm, each checked against\n * the lifted original before it was kept.\n */\n"
              '#include "game.h"\n#include "runtime.h"\n')
    sections = [f"{SECTION.format(function=function)}\n{body}" for function, body in _names_kept_apart(bodies)]
    code = header + "\n" + "\n\n".join(sections) + "\n" if sections else None
    return FileReply(file=CandidateFile(code) if code else None, cfg_lines=(), drafts=tuple(drafts),
                     problems=tuple(problems), seconds=0.0)


SECTION = "/* ── {function} ── */"   # starts each function's part of an assembled file


def function_at_line(code: str, line: int) -> FunctionName | None:
    """Whose part of an assembled farm file a line (1-based) is in."""
    owner: FunctionName | None = None
    for number, text in enumerate(code.splitlines(), start=1):
        if number > line:
            break
        match = _SECTION_LINE.match(text)
        if match:
            owner = FunctionName(match[1])
    return owner


_SECTION_LINE = re.compile(r"^/\* ── (\w+) ── \*/$")

# names a candidate defines at file level: static functions and variables, macros, types, enum constants
_DEFINED = (
    re.compile(r"^static\s+(?:inline\s+)?(?:const\s+)?[\w\s*]*?\b(\w+)\s*\(", re.M),
    re.compile(r"^static\s+(?:const\s+)?[\w\s*]*?\b(\w+)\s*(?:\[[^\]]*\]\s*)*(?:=|;)", re.M),
    re.compile(r"^#define\s+(\w+)", re.M),
    re.compile(r"^typedef\b[^;{]*?\b(\w+)\s*;", re.M),
    re.compile(r"^(?:typedef\s+)?(?:struct|union|enum)\s+(\w+)\s*\{", re.M),
    re.compile(r"^}\s*(\w+)\s*;", re.M),
)
_ENUM_BODY = re.compile(r"\benum\b[^{;]*\{([^}]*)\}", re.S)


def _names_kept_apart(bodies: list[tuple[FunctionName, str]]) -> list[tuple[FunctionName, str]]:
    """Each candidate compiled alone in the farm; in one file, two that define the same file-level
    name (a helper, a macro, an enum constant) would clash, so later ones get the name suffixed with
    their function's name."""
    seen: set[str] = set()
    kept: list[tuple[FunctionName, str]] = []
    for function, body in bodies:
        names = _defined_names(body)
        for name in sorted(names & seen):
            body = re.sub(rf"\b{re.escape(name)}\b", f"{name}_{function}", body)
        seen |= names
        kept.append((function, body))
    return kept


def _defined_names(body: str) -> set[str]:
    names = {match[1] for pattern in _DEFINED for match in pattern.finditer(body)}
    for enum in _ENUM_BODY.finditer(body):
        names |= {item.split("=")[0].strip() for item in enum[1].split(",") if item.split("=")[0].strip().isidentifier()}
    return names - {"Next"}


def _request(context: ConversionContext, code: str) -> Prompt:
    contract = "\n".join(f"  {part}: {' '.join(names) or '-'}" for part, names in context.contract.by_part().items())
    return Prompt(DRAFT_FORMAT + DRAFT_FUNCTION.format(function=context.function, address=context.address,
                                                       contract=contract, assembly=context.assembly,
                                                       code=code.strip()))


def _ask(model: ModelSpec, prompt: Prompt, tag: str) -> str:
    llm = cast(ModuleType, FARM.llm)  # pyright: ignore[reportAny]
    return cast(str, llm.complete(model, prompt, system=DRAFT_SYSTEM, max_tokens=16000,  # pyright: ignore[reportAny]
                                  timeout=600, retries=2, tag=tag, usage_log=str(USAGE_LOG)))
