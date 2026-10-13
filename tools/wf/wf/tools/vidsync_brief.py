"""The brief an outside model writes a vidsync repair handler from, and its reply (the vidsync-repair workflow).

The brief carries what the model needs and nothing it must guess: the loss and what Claude saw differ, the
handler interface (tools/vidsync/handlers/__init__.py), the autosync helpers and the built-in handlers closest
to the case (their own source), the game's addresses by name (tools/vidsync/syms.py), what symbols/db knows
about the symbols involved, and the routines' assembly from bn6f. The reply is one ```python block, the whole
module tools/vidsync/handlers/NAME.py."""
import ast
import re
import time
from enum import Enum
from pathlib import Path
from types import ModuleType
from typing import NewType, cast

from wf.repo import REPO
from wf.tools.models import FARM, USAGE_LOG
from wf.tools.vidsync import LossCase
from wf.types import FunctionName, ModelSpec, Result, SymbolName, ToolMessage

VIDSYNC = REPO / "tools/vidsync"
HANDLERS = VIDSYNC / "handlers"
DATABASE = REPO / "symbols/db"
Brief = NewType("Brief", str)
HandlerCode = NewType("HandlerCode", str)
HandlerName = NewType("HandlerName", str)   # a handler module's name: handlers/NAME.py, NAME = "NAME"

SYSTEM = ("You write Python for vidsync, a tool that rebuilds a Game Boy Advance speedrun video as a replayable "
          "recording by syncing an emulator to it. You write one repair handler module, as the brief describes. "
          "Use only the helpers and names the brief shows. Reply with one ```python block holding the complete "
          "module, then at most ten lines saying what it writes and why.")


class LossKind(Enum):
    """What went otherwise in the game than in the video, as Claude sees it in the pictures."""
    BATTLE_END = "a battle ended otherwise (an enemy deleted, MegaMan's HP, the reward)"
    HAND = "other chips were dealt, or the folder is in another order"
    OVERLAY = "a stream overlay covers the picture"
    MAP_OBJECT = "a map object or NPC moved otherwise"
    POSITION = "MegaMan stands elsewhere"
    ENCOUNTER = "an encounter came where the video has none, or none where it has one"
    PUZZLE = "a puzzle or comp's state went otherwise"
    RNG_EVENT = "another random event went otherwise"
    VIDEO = "the video itself (a cut, lag, a skip)"
    OTHER = "something else"


# the built-in handlers a brief shows as models, by kind (all briefs show the helpers)
EXAMPLES: dict[LossKind, tuple[str, ...]] = {
    LossKind.BATTLE_END: ("try_kill", "check_reward"),
    LossKind.HAND: ("try_hand",),
    LossKind.OVERLAY: ("try_overlay",),
    LossKind.MAP_OBJECT: ("try_mapobj", "try_tornado"),
    LossKind.POSITION: ("try_position", "try_relocate"),
    LossKind.ENCOUNTER: ("try_encounter", "try_formations"),
    LossKind.PUZZLE: ("try_tornado", "try_mapobj"),
    LossKind.RNG_EVENT: ("try_at",),
    LossKind.VIDEO: ("try_offset",),
    LossKind.OTHER: ("try_position", "try_tornado"),
}
HELPERS = ("set_emu_pokes", "state_at", "ancestor", "path_keys", "path_reach", "ck_file")


class LossClass(Result):
    """Claude's reading of the loss pictures: what differs, and what the handler needs to know."""
    kind: LossKind
    what_differs: str                             # in plain words, from the pictures and the log
    symbols: tuple[SymbolName, ...] = ()          # symbols/db entries the brief carries
    routines: tuple[FunctionName, ...] = ()       # routines whose bn6f assembly the brief carries
    handler_name: HandlerName                     # handlers/NAME.py
    ask_model: bool = True                        # False: no new handler (e.g. Claude adjusts a built-in one)


class HandlerCandidate(Result):
    name: HandlerName
    code: HandlerCode | None
    model: ModelSpec | None
    problems: tuple[ToolMessage, ...]             # why it cannot be tested (no code, does not compile, ...)
    notes: tuple[ToolMessage, ...]                # what the reviewer should look at (hard-coded addresses)
    brief_file: Path
    reply_file: Path | None
    seconds: float


def write_brief(case: LossCase, loss_class: LossClass, earlier: tuple[str, ...], previous: HandlerCode | None) -> Brief:
    """The whole brief, in Markdown."""
    parts = [
        "# A repair handler for vidsync",
        "",
        "vidsync syncs an emulator running Mega Man Battle Network 6 Falzar (GBA) to a speedrun video, frame by "
        "frame (a beam search over keys). When the game goes otherwise than the video (a random event, an "
        "object), the sync loses the video; repair handlers then look for RAM writes ('pokes') that make the "
        "game follow the video again, judging each candidate by how far the sync then holds. The built-in "
        "handlers could not fix this loss; you write a new one.",
        "",
        "## The loss",
        f"Run {case.run}: the sync lost the video near game frame {case.loss} (video frame {case.video}). The "
        f"path so far carries {case.pokes} pokes. The driver's log from there (what the built-in handlers tried):",
        "```", *case.log, "```",
        "",
        "## What differs (from the pictures)",
        f"{loss_class.kind.value}: {loss_class.what_differs}",
        "",
        "## The handler interface (tools/vidsync/handlers/__init__.py)",
        _docstring(HANDLERS / "__init__.py"),
        f"Name the handler `{loss_class.handler_name}` (NAME = \"{loss_class.handler_name}\").",
        "",
        "## autosync.py: its helpers, and the built-in handlers closest to this case",
        "```python", *(_function_source(name) for name in (*HELPERS, *EXAMPLES[loss_class.kind])), "```",
        "",
        "## The game's addresses by name (tools/vidsync/syms.py; use `sync.syms.NAME`)",
        "```python", _syms_constants(), "```",
    ]
    entries = [(name, DATABASE / f"{name}.yaml") for name in loss_class.symbols]
    if entries:
        parts += ["", "## What is known about the symbols involved (symbols/db)"]
        for name, path in entries:
            parts += [f"### {name}", "```yaml", path.read_text().strip() if path.exists() else "(no entry yet)", "```"]
    if loss_class.routines:
        parts += ["", "## The routines' assembly (bn6f)"]
        for routine in loss_class.routines:
            assembly = cast(str | None, FARM.find_asm(routine))  # pyright: ignore[reportAny]
            parts += [f"### {routine}", "```asm", assembly or "(not found in bn6f/asm)", "```"]
    if earlier:
        parts += ["", "## Earlier attempts", "What the reviewer or the test said about the previous handler:",
                  *(f"- {text}" for text in earlier)]
        if previous:
            parts += ["", "The previous handler:", "```python", previous, "```"]
    parts += ["", "## Reply", "One ```python block with the complete module handlers/"
              f"{loss_class.handler_name}.py (NAME and try_fix(D, sync); imports at the top; no other files), "
              "then at most ten lines: what it writes, where, and why that should make the game follow the video. "
              "Every address from sync.syms or a struct offset bn6f names (no bare 0x02... numbers)."]
    return Brief("\n".join(parts) + "\n")


def ask_model_for_handler(model: ModelSpec, brief: Brief, name: HandlerName, tag: str, record_dir: Path) -> HandlerCandidate:
    """One model's handler for the brief (its prompt and reply are kept in record_dir)."""
    llm = cast(ModuleType, FARM.llm)  # pyright: ignore[reportAny]
    record_dir.mkdir(parents=True, exist_ok=True)
    brief_file = record_dir / f"{tag}.brief.md"
    reply_file = record_dir / f"{tag}.reply.md"
    _ = brief_file.write_text(brief)
    started = time.monotonic()
    try:
        reply = cast(str, llm.complete(model, brief, system=SYSTEM, max_tokens=64000, timeout=2700, retries=1,  # pyright: ignore[reportAny]
                                       tag=tag, usage_log=str(USAGE_LOG), wait_out_limits=False, stream=True))
    except Exception as error:  # the provider's own errors: HTTP, timeouts, bad JSON
        return HandlerCandidate(name=name, code=None, model=model, problems=(ToolMessage(f"model error: {error}"[:500]),),
                                notes=(), brief_file=brief_file, reply_file=None,
                                seconds=round(time.monotonic() - started, 1))
    _ = reply_file.write_text(reply)
    code, problems, notes = parse_handler(reply, name)
    return HandlerCandidate(name=name, code=code, model=model, problems=problems, notes=notes, brief_file=brief_file,
                            reply_file=reply_file, seconds=round(time.monotonic() - started, 1))


def parse_handler(reply: str, name: HandlerName) -> tuple[HandlerCode | None, tuple[ToolMessage, ...], tuple[ToolMessage, ...]]:
    """The reply's module (its largest ```python block), what keeps it from a test, and what to review."""
    blocks = re.findall(r"```python\n(.*?)```", reply, re.S)
    if not blocks:
        return None, (ToolMessage("the reply had no ```python block"),), ()
    code = HandlerCode(max(cast(list[str], blocks), key=len))
    problems: list[ToolMessage] = []
    try:
        tree = ast.parse(code)
    except SyntaxError as error:
        return code, (ToolMessage(f"does not compile: {error}"),), ()
    names = {target.id for node in tree.body if isinstance(node, ast.Assign) for target in node.targets
             if isinstance(target, ast.Name)}
    functions = {node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)}
    if "NAME" not in names or f'"{name}"' not in code and f"'{name}'" not in code:
        problems.append(ToolMessage(f"no NAME = \"{name}\" at the top level"))
    fix = functions.get("try_fix")
    if fix is None or [arg.arg for arg in fix.args.args] != ["D", "sync"]:
        problems.append(ToolMessage("no top-level def try_fix(D, sync)"))
    notes = tuple(ToolMessage(f"hard-coded address {literal}: use a syms name") for literal in
                  sorted(set(cast(list[str], re.findall(r"\b0x0?[23]0[0-9A-Fa-f]{5}\b", code)))))
    return code, tuple(problems), notes


def _docstring(path: Path) -> str:
    return ast.get_docstring(ast.parse(path.read_text())) or ""


def _function_source(name: str) -> str:
    source = (VIDSYNC / "autosync.py").read_text()
    for node in ast.parse(source).body:
        if isinstance(node, ast.FunctionDef) and node.name == name:
            return (ast.get_source_segment(source, node) or "") + "\n"
    return f"# ({name}: not in autosync.py)\n"


def _syms_constants() -> str:
    """syms.py's constants section: the names, as bn6f gives them, with their comments."""
    text = (VIDSYNC / "syms.py").read_text()
    start, end = text.index("# ── the frame loop"), text.index("def constants()")
    return text[start:end].strip()
