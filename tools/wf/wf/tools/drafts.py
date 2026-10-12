"""A farm model drafts the symbols/db entry of each candidate it got accepted,
beside it (tools/convert/accepted/NAME.MODEL.yaml), and a group of accepted
candidates becomes one file for the convert workflow: the code, and an entry
for every function, so review and record never wait on a strong model for
the documentation of small functions."""
import functools
import re
import time
from collections.abc import Mapping
from pathlib import Path
from types import ModuleType
from typing import cast

import yaml

from wf.repo import REPO
from wf.tools.models import (ACCEPTED_DIR, AFTER_REVIEW, FARM, USAGE_LOG, ConversionContext, Prompt,
                             conversion_context)
from wf.tools.sources import first_up, models_for
from wf.tools.subsystem import CandidateFile, FileReply, SymbolDraft, parse_drafts, yaml_blocks
from wf.types import FunctionName, ModelSpec, Result, ToolMessage

DATABASE = REPO / "symbols/db"
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
    """The accepted candidates of the model (or of a tier's models) that have no draft yet: of `functions`,
    in their order, or all."""
    tags = [source.replace("/", "_") for source in models_for(model)]
    if functions is not None:
        candidates = [path for function in functions
                      for path in (ACCEPTED_DIR / f"{function}.{tag}.c" for tag in tags) if path.exists()]
    else:
        candidates = sorted(path for tag in tags for path in ACCEPTED_DIR.glob(f"*.{tag}.c"))
    return tuple(path for path in candidates if not draft_path(path).exists())


def draft_entry(model: ModelSpec, candidate: Path, tag: str) -> Draft:
    """Ask the model (or the first source of a tier that is up) for the candidate's entry, once more if the
    yaml is unusable, and write it."""
    started = time.monotonic()
    function = FunctionName(candidate.name.split(".")[0])
    context = conversion_context(function)
    if context is None:
        return Draft(function=function, path=None, problem=ToolMessage("no lifted code or assembly"), seconds=0.0)
    prompt = _request(context, candidate.read_text())
    problem = ToolMessage("")
    sources = models_for(model)
    for attempt in range(2):
        def ask(source: ModelSpec, prompt: Prompt = prompt, attempt: int = attempt) -> tuple[str, str | None]:
            return _ask(source, prompt, f"{tag} draft {function} {attempt}", wait_out_limits=len(sources) == 1)
        def failure(answer: tuple[str, str | None]) -> str | None:
            return answer[1]
        _, (reply, error) = first_up(sources, ask, failure)
        if error is not None:
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
                        file_name: str, taken: Mapping[FunctionName, frozenset[str]] | None = None) -> FileReply:
    """The model's accepted candidates of `functions` as one file, with their drafted entries. TAKEN: per
    function, the names the file it will live in already uses (a farm group's functions go to the laid-out
    files, tools/layout.py); its own file-level definitions are renamed apart from them too."""
    tags = [source.replace("/", "_") for source in models_for(model)]
    bodies: list[tuple[FunctionName, str]] = []
    drafts: list[SymbolDraft] = []
    problems: list[ToolMessage] = []
    notes: list[ToolMessage] = []
    for function in functions:
        candidate = next((path for path in (ACCEPTED_DIR / f"{function}.{tag}.c" for tag in tags) if path.exists()),
                         ACCEPTED_DIR / f"{function}.{tags[0]}.c")
        if not candidate.exists():
            problems.append(ToolMessage(f"no accepted {model} candidate for {function}"))
            continue
        bodies.append((function, candidate.read_text().strip()))
        note = candidate.with_name(candidate.name.removesuffix(".c") + AFTER_REVIEW)
        if note.exists():
            notes.append(ToolMessage(f"{function}: {note.read_text().strip()}"))
        if draft_path(candidate).exists():
            found, _ = parse_drafts(draft_path(candidate).read_text())
            drafts += found
    header = (f"/*\n * {file_name}: {subsystem}.\n *\n * Small functions converted one at a time by the "
              f"model farm, each checked against\n * the lifted original before it was kept.\n */\n"
              '#include "game.h"\n#include "runtime.h"\n')
    renamed = _renamed_since()
    in_headers = _header_names()
    sections = [f"{SECTION.format(function=function)}\n{_with_current_names(_without_fallbacks(body, in_headers), renamed)}"
                for function, body in _names_kept_apart(bodies, taken or {})]
    code = header + "\n" + "\n\n".join(sections) + "\n" if sections else None
    return FileReply(file=CandidateFile(code) if code else None, cfg_lines=(), drafts=tuple(drafts),
                     problems=tuple(problems), seconds=0.0, notes=tuple(notes))


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
    re.compile(r"^typedef\s+(?:struct|union|enum)\s*(?:\w+\s*)?\{[^{}\n]*\}\s*(\w+)\s*;", re.M),   # one line: a wrapper's result
)
_ENUM_BODY = re.compile(r"\benum\b[^{;]*\{([^}]*)\}", re.S)
_IDENT = re.compile(r"\b[A-Za-z_]\w*\b")


def _names_kept_apart(bodies: list[tuple[FunctionName, str]],
                      taken: Mapping[FunctionName, frozenset[str]]) -> list[tuple[FunctionName, str]]:
    """Each candidate compiled alone in the farm; in one file, two that define the same file-level
    name (a helper, a macro, an enum constant) would clash, so later ones get the name suffixed with
    their function's name. So does a name the file it goes to already uses (TAKEN: a helper defined
    there too, or a macro of the shared headers that code there reads), or that another candidate of the
    group uses (it may take that name from the headers and land after this one)."""
    words: list[frozenset[str]] = [frozenset(match[0] for match in _IDENT.finditer(body)) for _, body in bodies]
    seen: set[str] = set()
    kept: list[tuple[FunctionName, str]] = []
    for i, (function, body) in enumerate(bodies):
        names = _defined_names(body)
        others = {word for j, used in enumerate(words) if j != i for word in used & names}
        for name in sorted(names & (seen | others | taken.get(function, frozenset()))):
            body = re.sub(rf"\b{re.escape(name)}\b", f"{name}_{function}", body)
        seen |= names
        kept.append((function, body))
    return kept


_FALLBACK = re.compile(r"^[ \t]*#\s*ifndef\s+(\w+)\s*\n[ \t]*#\s*define\s+\1\b[ \t]*([^\n]*?)[ \t]*\n[ \t]*#\s*endif[^\n]*$",
                       re.M)


def _without_fallbacks(body: str, in_headers: set[str]) -> str:
    """`#ifndef X / #define X V / #endif` (a rule says no): when the shared headers define X the fallback
    was never compiled in, so it goes; otherwise it becomes `enum { X = V };`, which fails loudly if a
    header ever defines X. The tested code is the same either way."""
    def replace(match: re.Match[str]) -> str:
        name, value = match[1], re.sub(r"/\*.*?\*/|//.*$", "", match[2]).strip()
        if name in in_headers or not value:
            return ""
        return f"enum {{ {name} = {value} }};"
    return _FALLBACK.sub(replace, body)


@functools.cache
def _header_names() -> set[str]:
    """Names the port's shared headers define (macros and enum constants)."""
    names: set[str] = set()
    for header in (REPO / "cport/include").glob("*.h"):
        text = header.read_text(errors="replace")
        names |= set(re.findall(r"^\s*#\s*define\s+(\w+)", text, re.M))
        names |= set(re.findall(r"^\s*(\w+)\s*=\s*[^=]", text, re.M))
    return names


def _renamed_since() -> dict[str, str]:
    """Every earlier name of a symbol (symbols/db previous_names) and the name it has now: a candidate
    accepted before a record run renamed one of its callees still calls it by the old name."""
    renamed: dict[str, str] = {}
    for entry in DATABASE.glob("*.yaml"):
        data = cast(object, yaml.safe_load(entry.read_text()))
        if not isinstance(data, dict):
            continue
        fields = cast(dict[str, object], data)
        name, previous = fields.get("name"), fields.get("previous_names")
        if isinstance(name, str) and isinstance(previous, list):
            renamed |= {old: name for old in cast(list[object], previous) if isinstance(old, str) and old != name}
    return renamed


def _with_current_names(body: str, renamed: dict[str, str]) -> str:
    if not renamed:
        return body
    pattern = re.compile(r"\b(" + "|".join(map(re.escape, sorted(renamed, key=len, reverse=True))) + r")\b")
    return pattern.sub(lambda match: renamed[match[1]], body)


_COMMENT_OR_STRING = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)


def _defined_names(body: str) -> set[str]:
    """The file-level names a candidate defines, read from its code only: "the disassembly's enum" in a
    comment once made `enum ... { R0 = position; ...` of the next function body, and the group's assembly
    renamed R0 apart (R0_sub_8003BF4, which no header defines)."""
    body = _COMMENT_OR_STRING.sub(lambda match: re.sub(r"[^\n]", " ", match[0]), body)
    names = {match[1] for pattern in _DEFINED for match in pattern.finditer(body)}
    for enum in _ENUM_BODY.finditer(body):
        names |= {item.split("=")[0].strip() for item in enum[1].split(",") if item.split("=")[0].strip().isidentifier()}
    return names - {"Next"}


def _request(context: ConversionContext, code: str) -> Prompt:
    contract = "\n".join(f"  {part}: {' '.join(names) or '-'}" for part, names in context.contract.by_part().items())
    return Prompt(DRAFT_FORMAT + DRAFT_FUNCTION.format(function=context.function, address=context.address,
                                                       contract=contract, assembly=context.assembly,
                                                       code=code.strip()))


def _ask(model: ModelSpec, prompt: Prompt, tag: str, wait_out_limits: bool) -> tuple[str, str | None]:
    """(the reply, or "" and the provider's error)."""
    llm = cast(ModuleType, FARM.llm)  # pyright: ignore[reportAny]
    try:
        return cast(str, llm.complete(model, prompt, system=DRAFT_SYSTEM, max_tokens=16000,  # pyright: ignore[reportAny]
                                      timeout=600, retries=2, tag=tag, usage_log=str(USAGE_LOG),
                                      wait_out_limits=wait_out_limits)), None
    except RuntimeError as error:
        return "", str(error)
