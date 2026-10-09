"""A strong outside model writes a whole subsystem's readable C file, and the
quick test checks it per function against the lifted originals without
touching the tree. Built from the model farm's own pieces
(tools/convert/farm.py: system prompt, conventions, compiler flags, lifted-style
filter, register comparison) and tools/llm.py (providers, pacers, token log).

The model is asked for C call and return semantics: typed functions with real
parameters and results, and `Next NAME(void)` only as a thin register adapter.
Where that cannot work it must say so with a `/* register form: <why> */`
comment, which the review and the symbol database carry forward."""
import os
import re
import subprocess
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from types import ModuleType
from typing import Annotated, NewType, cast

import yaml
from pydantic import BeforeValidator, Field, TypeAdapter, ValidationError, model_validator

from wf.repo import CPORT, ORIGINAL_ROM
from wf.tools.models import FARM as _farm
from wf.tools.models import USAGE_LOG, ConversionContext, Prompt
from wf.types import FunctionName, ModelSpec, Result, SnapshotDir, SymbolName, SymbolStatus, ToolMessage

CandidateFile = NewType("CandidateFile", str)   # a whole readable C file a model proposed
CfgLine = NewType("CfgLine", str)               # a cport/host/difftest.cfg line (or its "# reason" comment)

REFERENCE_FILES = (CPORT / "src/game/bg_scroll.c", CPORT / "src/game/event_flags.c")

SUBSYSTEM_RULES = r"""
## This conversion: a whole subsystem, with C call and return semantics
The conventions above describe the register world the rest of the game still lives in. Your file
goes further: do your best to move each function's call and return semantics out of its
registers-as-arguments origin.
- Write each function's work as a typed C function: real parameters and a real return value
  (a small struct when it returns several values), typed with the game's structures where the
  struct offsets allow. Name things in game terms.
- `Next NAME(void)` must still exist for each function (the rest of the game calls it through
  registers), but only as a thin adapter: read the argument registers, call the typed function,
  set the result registers. Leftover registers or flags the original leaves (see "outputs") are
  set there, and only where a caller outside the subsystem still needs them.
- Calls between functions of your file are direct C calls to the typed functions, never
  GUEST_CALL. GUEST_CALL / GUEST_CALL_ADDR only for functions outside the subsystem.
- Where C semantics truly cannot work (e.g. the function returns somewhere other than its
  caller, or shares its caller's stack frame), keep the register form for that function and say
  so in a comment right above it: `/* register form: <why C semantics cannot work here> */`.
- A function may promise fewer output registers than its contract only when every caller is
  inside your file or provably ignores them; then give a difftest.cfg line for it
  (`NAME compare=r5,r10` lists what it still promises) with a `# reason` comment line above it.
- Struct field offsets not in the shared header: a local `enum` taken from bn6f
  include/structs/*.inc, as the reference files do.
"""

REPLY_FORMAT = """
## Reply format (three blocks, nothing else)
1. One ```c block: the complete file cport/src/game/{file}: a header comment saying what the
   subsystem does (no authorship line: the commit records who wrote the file), `#include "game.h"`
   and `#include "runtime.h"`, then the code. Define exactly
   these functions as `Next NAME(void)`: {names}. No other `Next ...(void)` definitions.
2. One ```cfg block: difftest.cfg lines for narrowed promises, each after a `# reason` line
   (empty block if none).
3. One ```yaml block: a list with one entry per function:
   - function: its current name
     proposed_name: a name in game terms that keeps the current name's address suffix
       (sub_80342DC -> setStatePhase4_80342DC), or the current name
     status: probable or hypothesis (how sure the meaning is from the code)
     summary: one line in game terms
     interface: the typed function's C signature, and the adapter's registers in and out
     details: what it does, structures it touches, quirks
     open_questions: [list]
     register_form: null, or why it keeps the register form
"""


def _as_text(value: object) -> object:
    """Models sometimes structure a text field ({typed: ..., adapter: ...}); keep its content as text."""
    if isinstance(value, dict):
        return "; ".join(f"{key}: {_as_text(item)}" for key, item in cast(dict[object, object], value).items())
    if isinstance(value, list):
        return "; ".join(str(_as_text(item)) for item in cast(list[object], value))
    return value


Text = Annotated[str, BeforeValidator(_as_text)]


class SymbolDraft(Result):
    """A model's draft of a function's symbols/db entry (the workflow adds the evidence)."""
    function: FunctionName
    proposed_name: SymbolName

    @model_validator(mode="before")
    @classmethod
    def _address_suffix_kept(cls, data: object) -> object:
        """A proposed name keeps the current name's address suffix (README: the meaning is partly
        inferred), and a literal `_ADDRESS` (models copied the instruction) becomes the address."""
        fields = cast(dict[str, object], data) if isinstance(data, dict) else None
        if fields is None:
            return cast(object, data)
        function, proposed = fields.get("function"), fields.get("proposed_name")
        if not isinstance(function, str) or not isinstance(proposed, str):
            return cast(object, data)
        suffix = re.search(r"_([0-9A-Fa-f]{7,8})$", function)
        name = re.sub(r"_?ADDRESS$", f"_{suffix[1]}" if suffix else "", proposed.strip()) or function
        if suffix and not name.lower().endswith(f"_{suffix[1].lower()}"):   # its own address, not a RAM one
            name = f"{name}_{suffix[1]}"
        return {**fields, "proposed_name": name}
    status: SymbolStatus
    summary: Text
    interface: Text
    details: Text = ""
    open_questions: tuple[Text, ...] = ()
    register_form: Text | None = None


class FileReply(Result):
    """What a model (or a given candidate file) produced for one attempt."""
    file: CandidateFile | None
    cfg_lines: tuple[CfgLine, ...]
    drafts: tuple[SymbolDraft, ...]
    problems: tuple[ToolMessage, ...]  # reply blocks missing or malformed
    seconds: float
    error: ToolMessage | None = None   # the provider failed
    prompt_file: Path | None = None    # the prompt and raw reply, kept for reading later
    reply_file: Path | None = None


class FunctionTest(Result):
    function: FunctionName
    captured: ToolMessage
    random: ToolMessage
    passed: bool
    compared: bool = True  # False: no captured calls and every random input out of domain (review sees it)


class FileTest(Result):
    """The quick test: the file's functions against their lifted originals, nothing installed."""
    passed: bool
    report: ToolMessage
    functions: tuple[FunctionTest, ...]
    register_use_outside_adapters: tuple[ToolMessage, ...]  # "name: n uses" (review sees it)
    register_form_notes: tuple[ToolMessage, ...]            # "name: why"


# ── asking ─────────────────────────────────────────────────────────────

def subsystem_prompt(subsystem: str, file_name: str, contexts: tuple[ConversionContext, ...]) -> Prompt:
    references = "\n\n".join(f"### {path.relative_to(CPORT.parent)}\n```c\n{_without_authorship(path.read_text())}```"
                             for path in REFERENCE_FILES)
    functions = "\n\n".join(_function_section(context) for context in contexts)
    names = ", ".join(context.function for context in contexts)
    return Prompt(
        cast(str, _farm.CONVENTIONS) + SUBSYSTEM_RULES  # pyright: ignore[reportAny]
        + f"\n## Reference files (already converted, the style wanted)\n{references}\n"
        + f"\n## The subsystem to convert: {subsystem}\n{functions}\n"
        + REPLY_FORMAT.format(file=file_name, names=names))


def _without_authorship(code: str) -> str:
    """A reference file without its "Written by ..." line: models copied it into their own files
    (the commit message says who wrote a file)."""
    return re.sub(r"^ \*\s*\n \* Written by [^\n]*\n|^ \* Written by [^\n]*\n", "", code, flags=re.M)


def repair_prompt(base: Prompt, what_failed: str, report: str, previous: FileReply | None) -> Prompt:
    parts = [base, f"\n## {what_failed}\n```\n{report[:6000]}\n```"]
    if previous is not None and previous.file is not None:
        parts.append(f"\n## Your previous file\n```c\n{previous.file}```")
        if previous.cfg_lines:
            parts.append("```cfg\n" + "\n".join(previous.cfg_lines) + "\n```")
    parts.append("\nFix it and reply again with all three blocks of the reply format: the ```c block with the "
                 "complete file, the ```cfg block (empty if none) and the ```yaml block with an entry for every "
                 "function.")
    return Prompt("\n".join(parts))


def ask_model_for_file(model: ModelSpec, prompt: Prompt, tag: str, record_dir: Path) -> FileReply:
    llm = cast(ModuleType, _farm.llm)  # pyright: ignore[reportAny]
    record_dir.mkdir(parents=True, exist_ok=True)
    prompt_file = record_dir / f"{tag.replace(' ', '-')}.prompt.md"
    reply_file = record_dir / f"{tag.replace(' ', '-')}.reply.md"
    prompt_file.write_text(prompt)
    started = time.monotonic()
    try:
        reply = _complete(llm, model, prompt, tag)
    except Exception as error:  # the provider's own errors: HTTP, timeouts, bad JSON
        return FileReply(file=None, cfg_lines=(), drafts=(), problems=(), seconds=round(time.monotonic() - started, 1),
                         error=ToolMessage(f"model error: {error}"[:500]), prompt_file=prompt_file)
    reply_file.write_text(reply)
    if not reply.strip():
        return FileReply(file=None, cfg_lines=(), drafts=(), problems=(), seconds=round(time.monotonic() - started, 1),
                         error=ToolMessage("empty reply: the model may have spent its whole output budget on reasoning"),
                         prompt_file=prompt_file, reply_file=reply_file)
    parsed = parse_reply(reply)
    return parsed.model_copy(update={"seconds": round(time.monotonic() - started, 1),
                                     "prompt_file": prompt_file, "reply_file": reply_file})


def _complete(llm: ModuleType, model: ModelSpec, prompt: Prompt, tag: str) -> str:
    """The reply, with room for long reasoning: the models spent 35-48k tokens reasoning on a subsystem,
    so 48k cut replies off; a provider refusing 64k is asked again with 48k."""
    def ask(max_tokens: int) -> str:
        return cast(str, llm.complete(model, prompt, system=cast(str, _farm.SYSTEM), max_tokens=max_tokens,  # pyright: ignore[reportAny]
                                      timeout=900, retries=1, tag=tag, usage_log=str(USAGE_LOG)))
    try:
        return ask(64000)
    except RuntimeError as error:
        if "HTTP 400" not in str(error):
            raise
        return ask(48000)


def candidate_from_file(path: Path) -> FileReply:
    """A file given by hand (e.g. reviewed farm candidates), parsed as if a model had sent it."""
    text = path.read_text()
    return parse_reply(text if "```" in text else f"```c\n{text}```\n```cfg\n```\n```yaml\n[]\n```")


def parse_reply(reply: str) -> FileReply:
    blocks = {kind: _blocks(reply, kind) for kind in ("c", "cfg", "yaml")}
    problems: list[ToolMessage] = []
    code = max(blocks["c"], key=len, default=None)
    if code is None:
        problems.append(ToolMessage("the reply had no ```c block"))
    cfg = tuple(CfgLine(line) for block in blocks["cfg"] for line in block.splitlines() if line.strip())
    drafts, yaml_problems = parse_drafts(blocks["yaml"][0]) if blocks["yaml"] else ((), ())
    problems += yaml_problems
    return FileReply(file=CandidateFile(code) if code else None, cfg_lines=cfg, drafts=drafts,
                     problems=tuple(problems), seconds=0.0)


_PLAIN_VALUE = re.compile(r"^(\s*(?:-\s+)?\w+:\s+)([^'\"\[{|>&*!#\s].*(?::\s|\s#).*)$", re.M)


def _load_yaml(text: str) -> object:
    """The yaml block; when it does not parse, again with plain values that contain `: ` or ` #`
    quoted (models write `summary: redraws: the window`, which yaml reads as a mapping)."""
    try:
        return cast(object, yaml.safe_load(text))
    except yaml.YAMLError:
        quoted = _PLAIN_VALUE.sub(lambda match: match[1] + '"' + match[2].replace("\\", "\\\\").replace('"', '\\"') + '"',
                                  text)
        return cast(object, yaml.safe_load(quoted))


def parse_drafts(text: str) -> tuple[tuple[SymbolDraft, ...], tuple[ToolMessage, ...]]:
    """A yaml list of symbol entries, or why it is not one."""
    try:
        return TypeAdapter(tuple[SymbolDraft, ...]).validate_python(_load_yaml(text) or []), ()
    except (yaml.YAMLError, ValidationError) as error:
        return (), (ToolMessage(f"the ```yaml block is not a valid list of entries: {str(error)[:400]}"),)


def yaml_blocks(reply: str) -> list[str]:
    return _blocks(reply, "yaml")


def _blocks(reply: str, kind: str) -> list[str]:
    return re.findall(rf"```{kind}[ \t]*\n(.*?)```", reply, re.S)


def _function_section(context: ConversionContext) -> str:
    if context.entry_of is not None:
        return (f"### {context.function} (at {context.address:#010x}): a second entry into {context.entry_of}\n"
                f"Callers enter {context.entry_of}'s code at {context.address:#010x} (see its assembly above), with "
                f"the same register contract. The lifted build requires a file replacing {context.entry_of} to "
                f"define `Next {context.function}(void)` too.")
    contract = "\n".join(f"  {part}: {' '.join(names) or '-'}" for part, names in context.contract.by_part().items())
    return (f"### {context.function} (at {context.address:#010x})\n"
            f"Register contract (the lifter's analysis of every caller):\n{contract}\n"
            f"Assembly:\n```\n{context.assembly}\n```\nLifted C (exact, unreadable):\n```c\n{context.lifted}\n```")


# ── the quick test ─────────────────────────────────────────────────────

def test_candidate_file(contexts: tuple[ConversionContext, ...], reply: FileReply, snapshots: SnapshotDir,
                        difftest: Path) -> FileTest:
    names = tuple(context.function for context in contexts)
    if reply.error is not None or reply.file is None:
        return _failed([*reply.problems, reply.error or ToolMessage("no file")])
    code = reply.file
    problems = _shape_problems(code, names)
    if problems:
        return _failed(problems)
    drafted = {draft.function for draft in reply.drafts}
    missing_drafts = [name for name in names if name not in drafted]
    block_problems = [*reply.problems] + ([ToolMessage(f"no ```yaml entry for: {', '.join(missing_drafts)}")]
                                          if missing_drafts else [])

    with tempfile.TemporaryDirectory() as workdir:
        library, compile_errors = _compile(code, names, Path(workdir))
        if library is None:
            return _failed([ToolMessage("compile errors:\n" + compile_errors)])
        promises = _promises(reply.cfg_lines)

        def test_one(context: ConversionContext) -> FunctionTest:
            return _test_function(context, library, snapshots, difftest, promises.get(context.function))

        with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:   # one difftest process per function
            tests = tuple(pool.map(test_one, contexts))
    code_passed = all(test.passed for test in tests)
    report = "\n".join(f"{test.function}: {'ok' if test.passed else 'FAIL'}\n  captured calls: {test.captured}\n"
                       f"  random inputs: {test.random}" for test in tests)
    if code_passed and block_problems:
        report = ("The code passed every test; keep it as it is and fix only the other blocks:\n"
                  + "\n".join(block_problems) + "\n\n" + report)
    elif block_problems:
        report += "\n\nAlso wrong in the reply:\n" + "\n".join(block_problems)
    return FileTest(passed=code_passed and not block_problems, report=ToolMessage(report), functions=tests,
                    register_use_outside_adapters=register_use_outside_adapters(code, names),
                    register_form_notes=register_form_notes(code))


def _failed(problems: list[ToolMessage]) -> FileTest:
    return FileTest(passed=False, report=ToolMessage("\n".join(problems)), functions=(),
                    register_use_outside_adapters=(), register_form_notes=())


def _shape_problems(code: str, names: tuple[FunctionName, ...]) -> list[ToolMessage]:
    problems: list[ToolMessage] = []
    defined = re.findall(r"^\s*(?:IWRAM_\w+\s+|ARM_CODE\s+)*Next\s+(\w+)\s*\(\s*void\s*\)\s*\{", _strip_comments(code), re.M)
    for name in names:
        if defined.count(name) != 1:
            problems.append(ToolMessage(f"`Next {name}(void)` must be defined exactly once (found {defined.count(name)})"))
    others = sorted(set(defined) - set(names))
    if others:
        problems.append(ToolMessage(f"defines functions outside the subsystem: {', '.join(others)}"))
    lifted_style = sorted({match.group(0) for match in
                           cast(re.Pattern[str], _farm.LIFTED_STYLE).finditer(_strip_comments(code))})  # pyright: ignore[reportAny]
    if lifted_style:
        problems.append(ToolMessage("still machine-translated style: " + ", ".join(lifted_style)[:200]))
    return problems


def _compile(code: str, names: tuple[FunctionName, ...], workdir: Path) -> tuple[Path | None, str]:
    renamed = code
    for name in names:
        renamed = re.sub(rf"\bNext\s+{name}\s*\(\s*void\s*\)(\s*\{{)", rf"Next cand_{name}(void)\1", renamed)
    source, library = workdir / "cand.c", workdir / "cand.so"
    source.write_text(renamed)
    flags = cast(list[str], _farm.CFLAGS)  # pyright: ignore[reportAny]
    done = subprocess.run(["gcc", *flags, "-o", str(library), str(source)], capture_output=True, text=True, check=False)
    if done.returncode != 0:
        errors = [line.replace(str(source) + ":", "line ") for line in done.stderr.splitlines() if "error" in line]
        return None, "\n".join(errors[:12])
    return library, ""


def _promises(cfg_lines: tuple[CfgLine, ...]) -> dict[str, tuple[list[str], list[str]]]:
    """A model's narrowed promises: NAME -> (registers, flags) from `NAME ... compare=...`."""
    promises: dict[str, tuple[list[str], list[str]]] = {}
    for line in cfg_lines:
        match = re.match(r"^\s*(\w+)\b.*\bcompare=([\w,]+)\s*$", line)
        if match and not line.lstrip().startswith("#"):
            items = match[2].split(",")
            promises[match[1]] = ([item for item in items if item.startswith("r")],
                                  [item for item in items if item in ("nz", "c", "v")])
    return promises


def _test_function(context: ConversionContext, library: Path, snapshots: SnapshotDir, difftest: Path,
                   promise: tuple[list[str], list[str]] | None) -> FunctionTest:
    contract = {part: list(names) for part, names in context.contract.by_part().items()}
    registers, flags = promise or cast(tuple[list[str], list[str]], _farm.compare_args(contract))  # pyright: ignore[reportAny]
    base = [str(difftest), "-L", str(library), "-R", ",".join(registers)] + (["-F", ",".join(flags)] if flags else [])
    env = {**os.environ, "BN6F_ROM": str(ORIGINAL_ROM)}
    results: list[ToolMessage] = []
    passed = True
    captured = snapshots / context.function
    if not captured.is_dir():   # an extra entry nothing was seen calling
        results.append(ToolMessage("no captured calls (random inputs only)"))
    for extra in ([["-S", str(captured)]] if captured.is_dir() else []) + [["-n", "300"]]:
        try:
            done = subprocess.run([*base, *extra, context.function], capture_output=True, text=True, cwd=CPORT,
                                  env=env, timeout=900, check=False)
        except subprocess.TimeoutExpired:
            results.append(ToolMessage("timed out (900 s)"))
            passed = False
            continue
        line = (done.stdout.strip().splitlines() or [""])[-1]
        detail = "\n".join(done.stderr.strip().splitlines()[:8])
        results.append(ToolMessage((line + ("\n" + detail if detail else "")).strip()))
        if done.returncode != 0 or " ok " not in f" {line} ":
            passed = False
        nothing = re.search(r"\((\d+) snapshots, (\d+) runs, \d+ failures, (\d+) out-of-domain\)", line)
        if nothing and int(nothing[2]) == int(nothing[3]):
            results[-1] = ToolMessage(results[-1] + "\nevery captured call was out of domain: nothing was compared")
            passed = False
    random = results[1] if len(results) > 1 else ToolMessage("not run")
    trials = re.search(r"\((\d+) trials, \d+ failures, (\d+) out-of-domain\)", random)
    compared = captured.is_dir() or not (trials and trials[1] == trials[2])
    return FunctionTest(function=context.function, captured=results[0], random=random, passed=passed,
                        compared=compared)


# ── what review looks at ───────────────────────────────────────────────

_REGISTER_USE = re.compile(r"\bR(?:1[0-2]|[0-9])\b|\bSET_FLAGS_\w+|\bGUEST_CALL(?:_ADDR)?\b|\bcpu\.\w+")
_DEFINITION = re.compile(r"^[A-Za-z_][\w \t\*]*?\b(\w+)\s*\(([^;{}]*)\)\s*\{", re.M)


def register_use_outside_adapters(code: str, names: tuple[FunctionName, ...]) -> tuple[ToolMessage, ...]:
    """Register-world uses (R0..R12, flags, GUEST_CALL, cpu.) in functions that are not the
    `Next NAME(void)` adapters: the typed functions should have none."""
    counts: list[ToolMessage] = []
    for name, is_adapter, body in _top_level_functions(_strip_comments(code), names):
        uses = len(_REGISTER_USE.findall(body))
        if not is_adapter and uses:
            counts.append(ToolMessage(f"{name}: {uses} register-world uses"))
    return tuple(counts)


def register_form_notes(code: str) -> tuple[ToolMessage, ...]:
    """`/* register form: why */` notes, each with the function defined after it."""
    notes: list[ToolMessage] = []
    for match in re.finditer(r"/\*\s*register form:\s*(.*?)\*/", code, re.S):
        following = _DEFINITION.search(code, match.end())
        where = following.group(1) if following else "?"
        notes.append(ToolMessage(f"{where}: {' '.join(match[1].split())}"))
    return tuple(notes)


def _top_level_functions(code: str, names: tuple[FunctionName, ...]) -> list[tuple[str, bool, str]]:
    functions: list[tuple[str, bool, str]] = []
    for match in _DEFINITION.finditer(code):
        depth, end = 1, match.end()
        while end < len(code) and depth:
            depth += {"{": 1, "}": -1}.get(code[end], 0)
            end += 1
        is_adapter = match.group(1) in names and re.fullmatch(r"\s*void\s*", match.group(2)) is not None
        functions.append((match.group(1), is_adapter, code[match.end():end]))
    return functions


def _strip_comments(code: str) -> str:
    return re.sub(r"/\*.*?\*/|//[^\n]*", " ", code, flags=re.S)


# ── tokens ─────────────────────────────────────────────────────────────

class TokenUse(Result):
    tokens_in: int
    tokens_out: int  # reasoning included


class _UsageRow(Result):
    tag: str = ""
    tokens_in: int = Field(default=0, alias="in")
    tokens_out: int = Field(default=0, alias="out")
    reasoning: int | None = 0


def tokens_for(tag: str) -> TokenUse | None:
    """What tools/llm.py logged for the call with this tag (tools/convert/usage.jsonl)."""
    if not USAGE_LOG.exists():
        return None
    for line in reversed(USAGE_LOG.read_text().splitlines()):
        if f'"{tag}"' in line:
            row = _UsageRow.model_validate_json(line)
            return TokenUse(tokens_in=row.tokens_in, tokens_out=row.tokens_out + (row.reasoning or 0))
    return None
