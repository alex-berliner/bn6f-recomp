"""Investigating placeholder-named functions with cheap models (`wf investigate`): what a run
selects, what each model is shown and asked, how the answers are read, and the symbols/db-format
candidate it writes for review. Nothing here touches bn6f, symbols/db or git: candidates and the
report go under build/investigate/RUN_ID, and the run is resumable (a proposal or cross-check once
asked is cached there, and functions with a finished candidate are skipped — `--retry-unnamed RUN_ID`
re-asks exactly the ones an earlier run's proposals left without a name).

The models are asked through tools/llm.py `complete` (their token use lands in the shared log,
tools/convert/usage.jsonl, read back per call by tag). A reply that holds no JSON object is asked
once more with a short \"reply with only the JSON object\" follow-up (the counted retry); a proposed
name that carries an address or a digit run from the placeholder, or hedges, is treated as no name.
The written candidate files are checked with tools/symdb.py's own loader and entry check."""
import json
import re
import subprocess
import sys
import threading
import time
from collections.abc import Callable, Sequence
from datetime import date
from pathlib import Path
from typing import Literal, cast

import yaml
from pydantic import field_serializer

from wf.tools.models import Prompt
from wf.repo import REPO, SNAPSHOTS
from wf.types import FunctionName, GuestAddress, ModelSpec, Result, RouteName, SymbolName, SymbolStatus, ToolMessage

import funcindex  # tools/funcindex.py (wf.repo puts tools/ on the path): units and call neighbours
import llm  # noqa: E402  # tools/llm.py: the model providers
import symdb  # noqa: E402  # tools/symdb.py: the database's loader and entry check

SCORES = REPO / "symbols/meaning_scores.tsv"       # per address: kind, name, meaning score 0-5
INVESTIGATE = REPO / "build/investigate"           # the runs' output: RUN_ID/NAME.yaml + report.tsv
GRAPH = REPO / "build-abi/graph.json"              # the call graph (the export's call sites)
DATABASE = REPO / "symbols/db"                     # whose entries' summaries the context quotes
COVERAGE = REPO / "tools/traces/coverage"          # ROUTE.prof: every address a route executed
ASM_DIR = REPO / "bn6f/asm"                        # where the functions' assembly lives
DATA_FILES = ("data.s", "ewram.s", "iwram_data.s", "iwram.s", "rom.s", "vram.s")
USAGE_LOG = REPO / "tools/convert/usage.jsonl"     # where tools/llm.py logs every call
QUOTA = REPO / "tools/convert/quota.py"            # the providers' own meters (Hyper: credits)

CONTEXT_TOKENS = 6_000                             # the brief a model sees, all sections together
CHARS_PER_TOKEN = 2.6                              # code and assembly, as tools/llm.py measures prompts
SMALL_INSTRUCTIONS = 120                           # a small function ranks first
MAX_LISTED = 6                                     # callers/callees named per section, most relevant first
MAX_QUOTED = 200                                   # chars of a quoted summary or data comment
MAX_TOKENS = 10_000                                # output cap: room for the reasoning AND the JSON after it
MAX_CALL_SITES = 3                                 # callers whose code around the call site is shown
SITE_INSTRUCTIONS = 25                             # instructions of a caller shown around one call site
SITE_CHARS = 1_600                                 # chars of one call-site snippet, cut at a line
STRAGGLER_SECONDS = 300                            # a request still unanswered this long after the last other
                                                   # request of its pass finished is abandoned

DEFAULT_MODEL = ModelSpec("hyper/glm-5.3-flash")   # the farm's cheap converter proposes
DEFAULT_CHECKER = ModelSpec("hyper/qwen3.8-flash") # a different Hyper model cross-checks
DEFAULT_COUNT = 500                                # functions per run (--count)
DEFAULT_JOBS = 16                                   # model calls in flight (--jobs)

INVESTIGATE_SYSTEM = ("You investigate one function of a GBA game's disassembly (MegaMan Battle "
                      "Network 6 Falzar) from its assembly and its surroundings, for the project's "
                      "symbol database. Reply with exactly one JSON object and nothing else.")
CHECK_SYSTEM = ("You check another model's reading of GBA assembly (MegaMan Battle Network 6 Falzar) "
                "against the code. Reply with exactly one JSON object and nothing else.")

Finder = Callable[[int], "str | None"]             # what funcindex.unit_finder makes: address -> unit name


# ── results ────────────────────────────────────────────────────────────

class Target(Result):
    """One placeholder function chosen for investigation, with what ranked it."""
    name: FunctionName
    address: GuestAddress
    score: int                          # its meaning score (0 or 1)
    instructions: int                   # approximated from the unit's bytes (THUMB: bytes / 2)
    captured: bool                      # the original's own calls of it were captured
    routes: tuple[RouteName, ...]       # recorded routes that execute it
    named_neighbours: int               # callers/callees with a name or a symbols/db entry
    source: str                         # bn6f/asm/FILE.s:LINE (the lifted header's)


class Neighbour(Result):
    """A direct caller or callee, with what the project already knows about it."""
    name: FunctionName
    address: GuestAddress
    summary: ToolMessage | None         # the symbols/db entry's summary, when it has one


class DataSymbol(Result):
    """A data symbol the function reads or writes, with bn6f's comment when it documents one."""
    name: SymbolName
    comment: ToolMessage | None


class CallSite(Result):
    """One caller's call of the function: who calls it, where, and what the project knows about
    the caller (its code around the call site is the evidence the context shows)."""
    caller: FunctionName
    address: GuestAddress              # the caller's own address
    site: GuestAddress                 # the call instruction's address
    summary: ToolMessage | None        # the symbols/db entry's summary, when the caller has one


class Context(Result):
    """Everything the models are shown about one function. The assembly itself and the callers'
    call-site snippets stay on disk (contexts/NAME.asm, contexts/NAME.sites.asm, beside the run's
    candidates), so the run's state stays small."""
    target: Target
    assembly: Path
    comment: ToolMessage | None
    callers: tuple[Neighbour, ...]
    callees: tuple[Neighbour, ...]
    sites: tuple[CallSite, ...] = ()       # the callers' calls (up to MAX_CALL_SITES)
    sites_text: Path = Path("")            # the snippets around them; empty: no call site is known
    data: tuple[DataSymbol, ...] = ()


class Answer(Result):
    """What the investigating model replied (one JSON object)."""
    proposed_name: SymbolName | None
    summary: ToolMessage | None
    interface: ToolMessage | None
    details: ToolMessage | None
    evidence: tuple[ToolMessage, ...]
    self_score: int | None
    open_questions: tuple[ToolMessage, ...]


class Proposal(Result):
    """A function's investigation: the model's answer, or the problem that left none."""
    name: FunctionName
    address: GuestAddress
    answer: Answer | None
    tokens: int
    seconds: float
    retries: int = 0                   # answers asked again after an unusable reply (0 or 1)
    problem: ToolMessage | None = None
    finished: bool = False              # a candidate for it was already written (skipped, not asked)


CheckVerdict = Literal["agree", "revise", "disagree", "none"]


class CheckReply(Result):
    """What the cross-check model replied."""
    verdict: CheckVerdict
    reason: ToolMessage | None
    revised_name: SymbolName | None
    revised_summary: ToolMessage | None


class Verdict(Result):
    """A function's cross-check: the checker's reply, or `none` for a proposal with no name."""
    name: FunctionName
    address: GuestAddress
    reply: CheckReply | None
    tokens: int
    seconds: float
    retries: int = 0                   # replies asked again after an unusable one (0 or 1)
    problem: ToolMessage | None = None


class EvidenceItem(Result):
    """One symbols/db evidence item as the candidate carries it."""
    method: str
    date: str
    detail: ToolMessage
    refs: tuple[str, ...]
    reproduce: ToolMessage
    result: ToolMessage


class Candidate(Result):
    """A candidate symbols/db entry, in the database's field order and with only the database's keys:
    what the models proposed lives in the summary's first words (\"Proposed name: X. ...\"), in the
    evidence's result text and in report.tsv, because a candidate is not a rename yet."""
    name: FunctionName
    address: GuestAddress
    kind: str
    status: SymbolStatus
    summary: ToolMessage
    interface: ToolMessage | None
    details: ToolMessage | None
    evidence: tuple[EvidenceItem, ...]
    coverage: tuple[RouteName, ...] | None = None   # the recorded routes that execute it (None: none)
    open_questions: tuple[ToolMessage, ...]
    sources: tuple[str, ...]

    @field_serializer("address")
    def address_as_db(self, address: GuestAddress) -> str:
        """As the database writes addresses: a hex string (symbols/db/README.md)."""
        return f"0x{address:08X}"


class Report(Result):
    """What a run produced: candidates written, how the cross-checks came out, what it cost."""
    selected: int
    written: int
    agreed: int
    revised: int
    disagreed: int
    nulls: int
    invalid: int                       # candidates tools/symdb.py's check refused
    retries: int                       # answers asked a second time after an unusable reply
    tokens: int
    problems: tuple[ToolMessage, ...]
    stragglers: int = 0                # requests abandoned still unanswered (see STRAGGLER_SECONDS)
    stopped: ToolMessage | None = None   # e.g. the provider ran out of credits mid-run

    def totals(self) -> str:
        per = self.tokens / self.selected if self.selected else 0.0
        return (f"{self.written} candidates of {self.selected} functions: {self.agreed} agreed, "
                f"{self.revised} revised, {self.disagreed} disagreed, {self.nulls} without a name, "
                f"{self.invalid} invalid; {self.tokens} tokens (~{per:,.0f} per function); "
                f"{self.retries} retries"
                + (f"; {self.stragglers} abandoned" if self.stragglers else "")
                + (f"; {self.stopped}" if self.stopped else ""))


# ── what the run works on ──────────────────────────────────────────────

ScoreRow = tuple[FunctionName, GuestAddress, int]   # (name, address, meaning score) of one function


def placeholder(name: str) -> bool:
    """A name the investigation can improve: sub_* or asm*; the deliberately-empty ones stay out."""
    return (name.startswith("sub_") or name.startswith("asm")) \
        and not name.startswith(("nullsub_", "dead_", "unused_"))


def name_problem(name: str) -> ToolMessage | None:
    """Why a proposed name is not usable — an address or digit run copied from the placeholder
    ("..._80C4766", "0x65"), a Maybe hedge — or None when it reads like a name the project keeps."""
    if "maybe" in name.lower():
        return ToolMessage("it hedges (Maybe)")
    if re.search(r"0x[0-9a-fA-F]", name):
        return ToolMessage("it carries a 0x literal")
    if re.search(r"_?[0-9a-fA-F]{7,}$", name):
        return ToolMessage("it ends in a digit run copied from the placeholder's address")
    return None


def read_scores(path: Path = SCORES) -> tuple[ScoreRow, ...]:
    """The functions in symbols/meaning_scores.tsv (kind function only, so data labels stay out)."""
    rows: list[ScoreRow] = []
    for line in path.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        fields = line.split("\t")
        if len(fields) < 4 or fields[1] != "function":
            continue
        try:
            address = int(fields[0], 16)
            score = int(fields[3])
        except ValueError:
            continue
        rows.append((FunctionName(fields[2]), GuestAddress(address), score))
    return tuple(rows)


def coverage_routes() -> dict[RouteName, frozenset[int]]:
    """Each recorded route's executed addresses (tools/traces/coverage/ROUTE.prof)."""
    out: dict[RouteName, frozenset[int]] = {}
    for prof in sorted(COVERAGE.glob("*.prof")):
        addresses: set[int] = set()
        for line in prof.read_text().splitlines():
            if line.startswith("#") or not line.strip():
                continue
            fields = line.split()
            try:
                addresses.add(int(fields[0], 16))
            except (ValueError, IndexError):
                continue
        out[RouteName(prof.stem)] = frozenset(addresses)
    return out


def db_names() -> frozenset[str]:
    """The symbols that have a symbols/db entry."""
    return frozenset(path.stem for path in DATABASE.glob("*.yaml"))


def db_summaries() -> dict[str, str]:
    """Each symbols/db entry's name -> its one-line summary (the context quotes it for neighbours)."""
    out: dict[str, str] = {}
    for path in DATABASE.glob("*.yaml"):
        try:
            loaded = cast(object, yaml.safe_load(path.read_text()))
        except yaml.YAMLError:
            continue
        if not isinstance(loaded, dict):
            continue
        entry = cast(dict[str, object], loaded)
        name, summary = entry.get("name"), entry.get("summary")
        if isinstance(name, str) and isinstance(summary, str):
            out[name] = " ".join(summary.split())
    return out


def finished_names() -> frozenset[str]:
    """The functions any investigate run already wrote a candidate for (earlier runs and this one)."""
    if not INVESTIGATE.is_dir():
        return frozenset()
    return frozenset(path.stem for path in INVESTIGATE.glob("*/[a-zA-Z]*.yaml"))


def unit_of(name: FunctionName, address: GuestAddress, units: dict[str, funcindex.Unit],
            find: Finder) -> funcindex.Unit | None:
    """The function's lifted unit (its size, bn6f source and range), by name or address."""
    unit = units.get(name)
    if unit is not None:
        return unit
    found = find(address & ~1)
    return units.get(found) if found else None


def rank(targets: tuple[Target, ...]) -> tuple[Target, ...]:
    """Worth investigating first: covered by the game's own play (captured calls or a recorded
    route), then the most named neighbours to read it by, then the small ones."""
    def key(target: Target) -> tuple[int, int, int, int, str]:
        active = 0 if (target.captured or target.routes) else 1
        small = 0 if target.instructions <= SMALL_INSTRUCTIONS else 1
        return (active, -target.named_neighbours, small, target.instructions, target.name)
    return tuple(sorted(targets, key=key))


def select_targets(run_dir: Path, count: int, only: tuple[FunctionName, ...],
                   retry: frozenset[str] = frozenset()) -> tuple[Target, ...]:
    """The placeholder functions worth investigating, ranked, up to COUNT (all of ONLY when given,
    only the RETRY names when a --retry-unnamed run selects from an earlier run's unanswered ones);
    none that an earlier run already wrote a candidate for — except exactly the RETRY names, whose
    earlier candidate held no name."""
    units = funcindex.lifted_units()
    find = funcindex.unit_finder(funcindex.unit_ranges())
    neighbours = call_graph() if GRAPH.exists() else {}
    routes = coverage_routes()
    db = db_names()
    done = finished_names()
    targets: list[Target] = []
    for name, address, score in read_scores():
        if score > 1 or not placeholder(name):
            continue
        if retry and name not in retry:      # a retry run takes the earlier run's unnamed ones
            continue
        if name in done and name not in retry:
            continue
        if only and name not in only:
            continue
        unit = unit_of(name, address, units, find)
        if unit is None:
            continue
        covered = tuple(route for route, addresses in routes.items()
                        if unit.lo in addresses or unit.lo + 1 in addresses)
        callers, callees, _sites = neighbours.get(address & ~1, (frozenset(), frozenset(), ()))
        named = sum(1 for n in (*callers, *callees)
                    if (found := find(n)) is not None and (not placeholder(found) or found in db))
        targets.append(Target(name=name, address=address, score=score,
                              instructions=(unit.hi - unit.lo) // 2,   # THUMB halfwords (ARM: half as many)
                              captured=(SNAPSHOTS / name).is_dir(), routes=covered, named_neighbours=named,
                              source=unit.asm or ""))
    ranked = rank(tuple(targets))
    return ranked if only else ranked[:count]


def not_selectable(only: tuple[FunctionName, ...], selected: tuple[Target, ...]) -> tuple[ToolMessage, ...]:
    """An --only name the run cannot take, and why it was not eligible."""
    have = {target.name for target in selected}
    return tuple(ToolMessage(f"{name}: not an eligible placeholder in meaning_scores.tsv "
                             "(score, existing candidate or lifted unit)") for name in only if name not in have)


def unnamed_names(run_id: str) -> frozenset[str]:
    """The functions of an earlier run whose proposal got no usable name (no answer at all, or no
    proposed name in it) and whose candidate was not landed since: what --retry-unnamed selects
    from. The run's own proposal caches are read, so the run need not have finished cleanly."""
    folder = INVESTIGATE / run_id / "proposals"
    if not folder.is_dir():
        return frozenset()
    out: set[str] = set()
    for path in folder.glob("*.json"):
        try:
            loaded = cast(object, json.loads(path.read_text()))
        except (json.JSONDecodeError, OSError):
            continue
        if not isinstance(loaded, dict):
            continue
        row = cast(dict[str, object], loaded)
        answer = cast(dict[str, object] | None, row.get("answer"))
        named = answer is not None and isinstance(answer.get("proposed_name"), str) \
            and bool(answer.get("proposed_name"))
        if named or row.get("finished") is True or not isinstance(row.get("name"), str):
            continue
        out.add(str(row["name"]))
    landed = db_names()
    return frozenset(name for name in out if name not in landed)


_graph_cache: dict[int, tuple[frozenset[int], frozenset[int], tuple[tuple[int, int], ...]]] | None = None


def call_graph() -> dict[int, tuple[frozenset[int], frozenset[int], tuple[tuple[int, int], ...]]]:
    """Address -> (its callers' addresses, its callees' addresses, its call sites as
    (site address, caller address) in address order): the ABI export's call sites with a known
    target, like tools/funcindex.py call_neighbours but with the direction kept. The 70 MB export
    is parsed once per process."""
    global _graph_cache
    if _graph_cache is not None:
        return _graph_cache
    graph = cast(dict[str, object], json.loads(GRAPH.read_text()))
    entries = cast(dict[str, dict[str, object]], graph.get("entries", {}))
    address_of = {name: int(str(entry["addr"]), 16) & ~1 for name, entry in entries.items()}
    unit_of_name: dict[str, set[int]] = {}
    for name, entry in entries.items():
        unit_of_name.setdefault(str(entry["unit"]), set()).add(address_of[name])
    callers: dict[int, set[int]] = {}
    callees: dict[int, set[int]] = {}
    sites: dict[int, set[tuple[int, int]]] = {}
    for site in cast(list[object], graph.get("sites", [])):
        if not isinstance(site, dict):
            continue
        fields = cast(dict[str, object], site)
        target = fields.get("target")
        caller = fields.get("caller")
        if not isinstance(target, str) or target not in address_of or not isinstance(caller, str):
            continue
        callee = address_of[target]
        where = int(str(fields.get("addr", "0x0")), 16) & ~1
        for from_address in unit_of_name.get(caller, ()):
            if from_address == callee:
                continue
            callees.setdefault(from_address, set()).add(callee)
            callers.setdefault(callee, set()).add(from_address)
            sites.setdefault(callee, set()).add((where, from_address))
    _graph_cache = {address: (frozenset(callers.get(address, ())), frozenset(callees.get(address, ())),
                              tuple(sorted(sites.get(address, ()))))
                    for address in {*callers, *callees}}
    return _graph_cache


# ── the context a model is shown ───────────────────────────────────────

def label_lines(path: Path) -> dict[str, int]:
    """LABEL -> line number (1-based) of its definition in one bn6f source file."""
    out: dict[str, int] = {}
    for number, line in enumerate(path.read_text(errors="replace").splitlines(), start=1):
        match = re.match(r"^(\w+):{1,2}", line)
        if match:
            out.setdefault(match[1], number)
    return out


def label_index() -> dict[str, tuple[Path, int]]:
    """Where bn6f defines each data label, for comments and refs: the six data files, plus the data
    the asm and data/chunk files define among them (IWRAM data and strings live there, and a
    function's constants are labelled in the same asm file as its code)."""
    files = [REPO / "bn6f" / name for name in DATA_FILES]
    files += sorted((REPO / "bn6f/data").glob("*.s"))
    files += sorted(ASM_DIR.glob("*.s"))
    out: dict[str, tuple[Path, int]] = {}
    for path in files:
        if path.exists():
            out |= {label: (path, line) for label, line in label_lines(path).items()}
    return out


def comment_above(path: Path, line: int, label: str) -> ToolMessage | None:
    """The `//` comment lines bn6f carries directly above a label (and on its line), if any."""
    lines = path.read_text(errors="replace").splitlines()
    comment: list[str] = []
    index = line - 1
    if index >= len(lines):   # a label line from a stale index, past the end of the file
        return None
    if 0 <= index < len(lines):
        inline = lines[index].partition("//")[2].strip()
        if inline and not re.fullmatch(r"0x[0-9a-fA-F]+", inline):   # data labels carry their address
            comment.append(inline)
    index -= 1
    while index >= 0:
        text = lines[index].strip()
        if not text.startswith("//"):
            break
        comment.insert(0, text.removeprefix("//").strip())
        index -= 1
    return ToolMessage("\n".join(comment)) if comment else None


def data_references(assembly: str, labels: dict[str, tuple[Path, int]]) -> tuple[str, ...]:
    """The data symbols the assembly names: literal-pool loads (`// =NAME`) and `.word NAME` entries
    whose label bn6f defines in a data file."""
    names: list[str] = []
    for line in assembly.splitlines():
        code, _, note = line.partition("//")
        names += re.findall(r"=(\w+)", note)
        names += re.findall(r"\.word\s+(\w+)", code)
    seen: list[str] = []
    for name in names:
        if name in labels and name not in seen:
            seen.append(name)
    return tuple(seen)


def label_line(path: Path, name: str) -> int | None:
    """The 1-based line where PATH defines NAME (`NAME:`), the way tools/asmcat.py finds it — the
    lifted header's line is the first instruction, one below the label."""
    text = path.read_text(errors="replace")
    match = re.search(rf"(?m)^{re.escape(name)}:", text)
    return None if match is None else text[:match.start()].count(chr(10)) + 1


def label_before(path: Path, line: int) -> str | None:
    """The label defined at or just above LINE in PATH (`NAME:` or a thumb_func_start NAME), when
    the lifted header's function is not called NAME (an asmNN unit holds several sub_* labels)."""
    lines = path.read_text(errors="replace").splitlines()
    for index in range(min(line, len(lines)) - 1, -1, -1):
        text = lines[index].strip()
        start = re.match(r"(?:thumb_func_start\s+)?(\w+):(?:\s|$)", text)
        if start is not None:
            return start[1]
    return None


def source_path(source: str) -> Path | None:
    """The bn6f file a lifted header names: FILE.s is looked for under bn6f/asm first, then under
    bn6f/data (functions lifted out of the data chunks — function-pointer tables' targets — and
    their neighbours live there), then under bn6f/ itself."""
    name = source.partition(":")[0]
    for base in (ASM_DIR, REPO / "bn6f/data", REPO / "bn6f"):
        path = base / name
        if path.exists():
            return path
    return None


def assembly_of(source: str, name: str) -> str:
    """The bn6f assembly of NAME: from its label definition to its func_end, in the file the lifted
    header names (FILE.s:LINE). The label is found by name, as tools/asmcat.py does it, so the
    function ends where it ends (a header line one past the label must not run to end-of-file).
    A unit name that is not the function's label (asm33's functions are sub_8123198, ...) falls
    back to the label defined just above the header's line."""
    match = re.fullmatch(r"([\w.]+):(\d+)", source)
    if match is None:
        return ""
    path = source_path(source)
    if path is None:
        return ""
    text = path.read_text(errors="replace")
    label = name
    start = re.search(rf"(?m)^{re.escape(name)}:", text)
    if start is None and match[2]:
        above = label_before(path, int(match[2]))
        if above is not None:
            label = above
            start = re.search(rf"(?m)^{re.escape(above)}:", text)
    if start is None:
        return ""
    end = re.search(rf"(?m)^\s*(?:thumb|arm)_func_end\s+{re.escape(label)}\s*$", text[start.start():])
    return text[start.start():start.start() + (end.end() if end else len(text))].rstrip()


def write_context_assembly(run_dir: Path, name: FunctionName, source: str) -> Path:
    """The function's assembly, kept beside the run's candidates (the prompts read it from there)."""
    path = run_dir / "contexts" / f"{name}.asm"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(assembly_of(source, name))
    return path


def label_at(address: GuestAddress, name: FunctionName, find: Finder) -> str:
    """The function's label in bn6f, which is not always the scores' name: meaning_scores.tsv also
    lists unit names (asm33), whose functions are labelled sub_8123198 inside — the unit at the
    address is the label to look for (the empty listings round 15 showed)."""
    return find(address & ~1) or str(name)


def call_lines(assembly: str, label: str, address: GuestAddress,
              units: dict[str, funcindex.Unit]) -> tuple[int, ...]:
    """The 0-based line numbers of ASSEMBLY's `bl` instructions that reach the function at ADDRESS
    (by label name, or by the callee's address when bn6f wrote an alias under the label)."""
    wanted = address & ~1
    out: list[int] = []
    for number, line in enumerate(assembly.splitlines()):
        match = re.match(r"\s*bl\w*\s+(\w+)", line)
        if match is None:
            continue
        if match[1] == label:
            out.append(number)
            continue
        unit = units.get(match[1])
        if unit is not None and unit.lo & ~1 == wanted:
            out.append(number)
    return tuple(out)


def _instruction_lines(lines: Sequence[str], index: int) -> bool:
    """Is LINES[INDEX] a line the window counts (not blank, not an alignment/pool marker)?"""
    text = lines[index].strip()
    return bool(text) and not text.startswith((".balign", ".pool", ".align"))


def snippet_around(lines: Sequence[str], call: int, limit: int = SITE_INSTRUCTIONS) -> str:
    """About LIMIT instructions of the caller around its call of the function (the call line kept):
    how the caller set the arguments up and what it does with the result."""
    before: list[str] = []
    index, taken = call - 1, 0
    while index >= 0 and taken < (limit - 1) // 2:
        if _instruction_lines(lines, index):
            before.insert(0, lines[index])
            taken += 1
        index -= 1
    after: list[str] = []
    index, taken = call + 1, 0
    while index < len(lines) and taken < limit // 2:
        if _instruction_lines(lines, index):
            after.append(lines[index])
            taken += 1
        index += 1
    return "\n".join([*before, lines[call], *after]).rstrip()


def write_site_snippets(run_dir: Path, name: FunctionName, text: str) -> Path:
    """The callers' snippets around their call sites, one file beside the target's assembly."""
    path = run_dir / "contexts" / f"{name}.sites.asm"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return path


def sites_of(target: Target, site_list: tuple[tuple[int, int], ...], find: Finder,
             summaries: dict[str, str]) -> tuple[CallSite, ...]:
    """The function's call sites as the context shows them (up to MAX_CALL_SITES, a caller the
    project knows something about first): who calls it, where, and what the caller is."""
    chosen: list[CallSite] = []
    for site, caller_address in sorted(site_list)[:MAX_CALL_SITES * 4]:
        caller = find(caller_address)
        if caller is None:
            continue
        chosen.append(CallSite(caller=FunctionName(caller), address=GuestAddress(caller_address),
                               site=GuestAddress(site),
                               summary=ToolMessage(summaries[caller]) if caller in summaries else None))
    chosen.sort(key=lambda site: (site.summary is None, placeholder(site.caller), site.caller))
    return tuple(chosen[:MAX_CALL_SITES])


def sites_text_of(target: Target, sites: tuple[CallSite, ...], label: str,
                  units: dict[str, funcindex.Unit]) -> str:
    """The snippets file's text: one section per call site, the caller's code around its call."""
    parts: list[str] = ["# how its callers use it: the call sites are the evidence for the purpose"]
    for site in sites:
        caller_unit = units.get(site.caller)
        source = caller_unit.asm if caller_unit is not None else None
        caller_assembly = assembly_of(source, site.caller) if source else ""
        calls = call_lines(caller_assembly, label, target.address, units)
        lines = caller_assembly.splitlines()
        # a caller's several calls of the target: its sites in address order against the `bl` lines
        # in file order (bn6f writes them in address order)
        own = sorted((s for s in sites if s.caller == site.caller), key=lambda s: s.site)
        position = own.index(site)
        call = calls[position] if position < len(calls) else None
        header = (f"== 0x{site.address:08X} {site.caller} calls it at 0x{site.site:08X}"
                  + (f" — {site.summary}" if site.summary else ""))
        if call is None:
            parts.append(f"{header}\n(the call site is not in the caller's bn6f assembly: "
                         "the call may be through a wrapper)\n")
            continue
        text = snippet_around(lines, call)
        if len(text) > SITE_CHARS:
            text = text[:SITE_CHARS].rsplit("\n", 1)[0] + "\n(... cut)"
        parts.append(f"{header}\n{text}\n")
    return "\n".join(parts)


def gather_context(target: Target, run_dir: Path, labels: dict[str, tuple[Path, int]],
                   summaries: dict[str, str], find: Finder, units: dict[str, funcindex.Unit],
                   neighbours: dict[int, tuple[frozenset[int], frozenset[int],
                                               tuple[tuple[int, int], ...]]]) -> Context:
    """What the models see about one function: its assembly, bn6f's comment above it, its callers
    and callees with the summaries they have, up to three callers' code around their call sites,
    and the data it touches (the assembly and the snippets are kept under the run's directory)."""
    label = label_at(target.address, target.name, find)
    assembly = write_context_assembly(run_dir, target.name, target.source)
    source_file = source_path(target.source)
    label_line_number = label_line(source_file, label) if source_file is not None else None
    comment = comment_above(source_file, label_line_number, label) \
        if source_file is not None and label_line_number else None
    callers, callees, site_list = neighbours.get(target.address & ~1,
                                                 (frozenset(), frozenset(), ()))
    sites = sites_of(target, site_list, find, summaries)
    text = sites_text_of(target, sites, label, units) if sites else ""
    return Context(target=target, assembly=assembly, comment=comment,
                   callers=_neighbours(callers, find, summaries),
                   callees=_neighbours(callees, find, summaries),
                   sites=sites,
                   sites_text=write_site_snippets(run_dir, target.name, text) if sites else Path(""),
                   data=_data_symbols(assembly.read_text(errors="replace") if assembly.exists() else "", labels))


def _neighbours(addresses: frozenset[int], find: Finder, summaries: dict[str, str]) -> tuple[Neighbour, ...]:
    """Direct callers or callees as the context lists them, most relevant first (a summary of the
    project's own, then a name that is not a placeholder), capped at MAX_LISTED."""
    out: list[Neighbour] = []
    for address in sorted(addresses):
        found = find(address)
        if found is None:
            continue
        out.append(Neighbour(name=FunctionName(found), address=GuestAddress(address),
                             summary=ToolMessage(summaries[found]) if found in summaries else None))
    out.sort(key=lambda n: (n.summary is None, placeholder(n.name), n.name))
    return tuple(out[:MAX_LISTED])


def _data_symbols(assembly: str, labels: dict[str, tuple[Path, int]]) -> tuple[DataSymbol, ...]:
    """The data the assembly names, each with bn6f's comment above its label when there is one."""
    out: list[DataSymbol] = []
    for name in data_references(assembly, labels):
        path, line = labels[name]
        out.append(DataSymbol(name=SymbolName(name), comment=comment_above(path, line, name)))
    return tuple(out)


def gather_all(targets: tuple[Target, ...], run_dir: Path) -> tuple[Context, ...]:
    """Every target's context: the shared indexes once, then one context per function."""
    labels = label_index()
    summaries = db_summaries()
    find = funcindex.unit_finder(funcindex.unit_ranges())
    units = funcindex.lifted_units()
    neighbours = call_graph() if GRAPH.exists() else {}
    return tuple(gather_context(target, run_dir, labels, summaries, find, units, neighbours)
                 for target in targets)


# ── the brief the models see ───────────────────────────────────────────

def _trim(text: ToolMessage, limit: int = MAX_QUOTED) -> ToolMessage:
    """A quoted summary or comment, cut to LIMIT chars at a word (the context's budget is small)."""
    flat = " ".join(text.split())
    return ToolMessage(flat if len(flat) <= limit else flat[:flat.rfind(" ", 0, limit)].rstrip(",;:") + " …")


def neighbour_text(kind: str, neighbours: tuple[Neighbour, ...]) -> str:
    """One context section: the callers or callees, named, with what the project knows."""
    if not neighbours:
        return f"{kind}: none known"
    lines = [f"  0x{n.address:08X} {n.name}" + (f" — {_trim(n.summary)}" if n.summary else "")
             for n in neighbours]
    return f"{kind}:\n" + "\n".join(lines)


def sites_text(context: Context) -> str:
    """The call sites' section: the callers' code around each call, how they set it up and use it."""
    if not context.sites or not context.sites_text.exists():
        return ""
    text = context.sites_text.read_text(errors="replace")
    return "Call sites (how its callers use it):\n" + text.strip()


def context_text(context: Context) -> str:
    """The ~6k-token brief: where the function runs, what is around it, its call sites, its
    assembly last (what the rest of the context leaves of the budget)."""
    target = context.target
    lines = [f"{target.name} at 0x{target.address:08X} ({target.instructions} instructions, "
             f"meaning score {target.score})"]
    if target.captured:
        lines.append("The original game's own calls of it were captured (build-snapshots/).")
    if target.routes:
        lines.append(f"Executed by recorded routes: {', '.join(target.routes)}.")
    if context.comment:
        lines.append(f"bn6f's comment above it:\n  {_trim(context.comment, 300)}")
    lines.append(neighbour_text("Callers", context.callers))
    sites = sites_text(context)
    if sites:
        lines.append(sites)
    lines.append(neighbour_text("Callees", context.callees))
    if context.data:
        data = [f"  {d.name}" + (f" — {_trim(d.comment)}" if d.comment else "") for d in context.data]
        more = "" if len(data) <= MAX_LISTED else f"\n  (+ {len(data) - MAX_LISTED} more)"
        lines.append("Data it reads or writes:\n" + "\n".join(data[:MAX_LISTED]) + more)
    head = "\n\n".join(lines)
    assembly = context.assembly.read_text(errors="replace") if context.assembly.exists() else "(none)"
    budget = int(CONTEXT_TOKENS * CHARS_PER_TOKEN) - len(head) - 200   # the framing around the assembly
    return f"{head}\n\nAssembly:\n```\n{_cap(assembly, budget)}\n```"


def _cap(assembly: str, budget: int) -> str:
    """The assembly, cut to what the context's other sections left of the budget."""
    if budget <= 0:
        return "(the assembly was left out: the rest of the context took the budget)"
    out: list[str] = []
    used = 0
    lines = assembly.splitlines()
    for line in lines:
        if used + len(line) + 1 > budget:
            out.append(f"({len(lines) - len(out)} of {len(lines)} lines cut)")
            break
        out.append(line)
        used += len(line) + 1
    return "\n".join(out)


# ── the prompts ────────────────────────────────────────────────────────

PREFIX_MIN = 5                                     # a prefix the prompt lists must be seen this often


def name_prefixes(scores: tuple[ScoreRow, ...] | None = None) -> tuple[str, ...]:
    """The 25 most common subsystem prefixes among bn6f's function names (the prompt quotes them):
    sprite_ of sprite_queueOamEntry, battle_ of battle_dispatchAI_80123AC, and camel-hump names'
    first word (set of setBGTileIdsBlocks1And2_8030628). Placeholder names count for nothing."""
    counts: dict[str, int] = {}
    for row in (read_scores() if scores is None else scores):
        name = row[0]
        if placeholder(name) or name.startswith(("nullsub_", "dead_", "unused_")):
            continue
        match = re.match(r"^([a-z][a-z0-9]*)(?:_|(?=[A-Z]))", name)
        if match:
            counts[match[1]] = counts.get(match[1], 0) + 1
    common = sorted(counts, key=lambda name: (-counts[name], name))
    return tuple(name for name in common if counts[name] >= PREFIX_MIN)[:25]


INVESTIGATE_TASK = """You will be shown one undocumented function of MegaMan Battle Network 6 Falzar (GBA):
its bn6f assembly, the functions around it (callers and callees, with what the project already knows
about them), the callers' code around their call sites where they call it, the data it touches, and
where in the game it runs. Say what the function is for.

The call sites are the evidence for the purpose: how the callers set the arguments up and what they
do with the result tells you what the function is for. A function used only as X by its callers can
be named for that use ("returns the X of ...", "sets the X ...") even when the code alone does not
say why.

Reason briefly, then reply with exactly one JSON object and nothing else — no markdown fence, no
prose before or after it (both spend the reply's token budget and can cut the JSON off):
{{
  "proposed_name": "sprite_queueOamEntry" or null,
  "summary": "one line, in game terms",
  "interface": "(r0 = ..., r1 = ...) -> r0 = ...; flags ...",
  "details": "what it does, the structures and fields it touches, quirks — at most 6 sentences",
  "evidence": ["what in the code shows it, one line each, citing asm lines, callers or callees"],
  "self_score": 3,
  "open_questions": ["what is still unknown"]
}}

Keep the JSON small: at most five evidence items, one line each — it shares the reply's token
budget with your reasoning, and a reply cut off before the JSON is wasted.

Naming: follow the style of the already-renamed symbols — lowerCamel, with a subsystem prefix where
one fits. Prefixes in use: {prefixes}. Example: sprite_queueOamEntry.
The name must not repeat any part of the placeholder name: no address or digit run from it
(no "..._80C4766", no "t3_0x65_init"), and it must not hedge (no "Maybe ...").
Do not guess. If the code does not show the function's purpose, answer "proposed_name": null and say
in open_questions what is missing (what a caller would have to show, which structure, ...).
self_score, 0-5: 0 you could not say anything; 2 a plausible guess; 3 read from the assembly and
consistent with every use seen; 4 as 3, and the callers' or callees' known summaries back it;
5 certain from the code alone."""

CHECK_TASK = """You will be shown the same material about one undocumented function, and another model's
proposal for it (not its reasoning). Check the proposal against the code.

The proposal:
- proposed_name: {name}
- summary: {summary}
- interface: {interface}
- details: {details}

Reason briefly, then reply with exactly one JSON object and nothing else — no markdown fence, no
prose around it:
{{
  "verdict": "agree" or "revise" or "disagree",
  "reason": "what in the code agrees or contradicts it, citing asm lines, callers or callees concretely",
  "revised_name": null or "a better name, when revising",
  "revised_summary": null or "a better one-line summary, when revising"
}}

Keep it small: the reason in at most five sentences. A revised_name follows the same naming rules
(lowerCamel, a prefix from: {prefixes}) and carries no address, digit run from the placeholder, or
"Maybe".

agree = the name and summary fit the code; revise = close, but you have a better name or summary;
disagree = the code shows something else. Cite the code in the reason either way. Do not guess
either: if the code does not settle it, disagree and say why."""


def prompt_investigate(context: Context, prefixes: tuple[str, ...]) -> str:
    """The investigating model's prompt: the task, then the context (assembly last, capped)."""
    listed = ", ".join(prefixes) if prefixes else "(none yet)"
    return f"{INVESTIGATE_TASK.format(prefixes=listed)}\n\n{context_text(context)}"


def prompt_check(context: Context, proposal: Proposal, prefixes: tuple[str, ...]) -> str:
    """The cross-check model's prompt: the same context and the proposal, not its reasoning."""
    answer = proposal.answer
    assert answer is not None   # the caller only asks with a proposal that has an answer
    proposal_text = CHECK_TASK.format(name=answer.proposed_name or "(none proposed)",
                                      summary=answer.summary or "(none)",
                                      interface=answer.interface or "(none)",
                                      details=answer.details or "(none)", prefixes=", ".join(prefixes))
    return f"{proposal_text}\n\n{context_text(context)}"


# ── asking the models ──────────────────────────────────────────────────

_consumed: dict[Path, int] = {}                # usage-log bytes already read, per log
_spent: dict[tuple[Path, str], int] = {}       # tokens per call tag, as the provider reported them
_tokens_lock = threading.Lock()


def logged_tokens(usage_log: Path, tag: str) -> int:
    """Input and output tokens tools/llm.py logged for the call TAG (one JSON line per call; read
    incrementally, so a run of many calls does not re-read the whole log)."""
    with _tokens_lock:
        try:
            size = usage_log.stat().st_size
        except FileNotFoundError:
            return 0
        start = _consumed.get(usage_log, 0)
        if size > start:
            with usage_log.open("rb") as log:
                _ = log.seek(start)
                text = log.read().decode(errors="replace")
            if not text.endswith("\n"):    # a line still being written: leave it for the next read
                text = f"{text.rpartition(chr(10))[0]}\n" if chr(10) in text else ""
            for line in text.splitlines():
                try:
                    loaded = cast(object, json.loads(line))
                except json.JSONDecodeError:
                    continue
                if not isinstance(loaded, dict):
                    continue
                row = cast(dict[str, object], loaded)
                if row.get("tag") == tag:
                    into, out = row.get("in"), row.get("out")
                    if isinstance(into, int) and isinstance(out, int):
                        _spent[(usage_log, tag)] = into + out
            _consumed[usage_log] = start + len(text.encode())
        return _spent.get((usage_log, tag), 0)


def ask(model: ModelSpec, prompt: str, system: str, tag: str) -> tuple[str, ToolMessage | None]:
    """(the reply, or "" and the provider's error). tools/llm.py waits out Hyper's hourly rate limit
    itself; an HTTP 402 (out of credits) comes back as the error and stops the run."""
    try:
        reply = _complete(model, prompt, system=system, max_tokens=MAX_TOKENS, timeout=180, retries=3,
                          tag=tag, usage_log=str(USAGE_LOG))
    except RuntimeError as error:
        return "", ToolMessage(str(error)[:300])
    return reply, None


def _complete(spec: ModelSpec, prompt: str, system: str, max_tokens: int, timeout: int, retries: int,
              tag: str, usage_log: str) -> str:
    """tools/llm.py complete(), its untyped module boundary cast away (the farm does the same)."""
    return cast(str, llm.complete(spec, prompt, system=system, max_tokens=max_tokens, timeout=timeout,  # pyright: ignore[reportUnknownMemberType, reportAny, reportUnnecessaryCast]
                                  retries=retries, tag=tag, usage_log=usage_log))


def out_of_credits(problem: ToolMessage | None) -> bool:
    """The provider refused: its credits are spent (HTTP 402)."""
    return problem is not None and "HTTP 402" in problem


def abandoned(problem: ToolMessage | None) -> bool:
    """The request was given up on: still unanswered STRAGGLER_SECONDS after the pass's last other
    request finished (a resumed run asks again; nothing was cached for it)."""
    return problem is not None and problem.startswith("abandoned:")


def cached[R: Result](path: Path, name: str, made: type[R]) -> R | None:
    """What an earlier attempt of this run already got for NAME (the run is resumable)."""
    file = path / f"{name}.json"
    if not file.exists():
        return None
    try:
        return made.model_validate_json(file.read_text())
    except ValueError:
        return None


def keep(path: Path, name: str, result: Result) -> None:
    """Cache one function's result under the run's directory."""
    path.mkdir(parents=True, exist_ok=True)
    (path / f"{name}.json").write_text(result.model_dump_json())


def ask_investigate(model: ModelSpec, context: Context, run_dir: Path, run_id: str,
                    prefixes: tuple[str, ...], cancelled: threading.Event | None = None) -> Proposal:
    """The investigating model's answer for one function, or the problem that left none. An unusable
    reply (no JSON object in it) is asked once more, with a short follow-up; the retry is counted.
    A request whose pass was abandoned around it (CANCELLED set) is not cached: a later run asks
    again."""
    target = context.target
    if (INVESTIGATE / run_id / f"{target.name}.yaml").exists():
        return Proposal(name=target.name, address=target.address, answer=None, tokens=0, seconds=0.0,
                        problem=ToolMessage("already finished"), finished=True)
    cached_proposal = cached(run_dir / "proposals", target.name, Proposal)
    if cached_proposal is not None:
        return cached_proposal
    started = time.monotonic()
    prompt = prompt_investigate(context, prefixes)
    tokens, retries, answer, problem = 0, 0, None, ToolMessage("")
    for attempt in range(2):
        tag = f"{run_id} investigate {target.name} {attempt}"
        reply, problem = ask(model, prompt, INVESTIGATE_SYSTEM, tag)
        tokens += logged_tokens(USAGE_LOG, tag)
        if problem is None:
            answer, problem = parse_answer(reply)
        if problem is None:
            break
        retries = attempt + 1
        prompt = Prompt(f"{prompt}\n\nYour previous reply could not be used: {problem}\n"
                        f"Reply again with only the JSON object, nothing else.")
    proposal = Proposal(name=target.name, address=target.address, answer=answer, tokens=tokens,
                        seconds=round(time.monotonic() - started, 1), retries=retries, problem=problem)
    if cancelled is None or not cancelled.is_set():
        keep(run_dir / "proposals", target.name, proposal)
    return proposal


def ask_check(model: ModelSpec, context: Context, proposal: Proposal, run_dir: Path, run_id: str,
              prefixes: tuple[str, ...], cancelled: threading.Event | None = None) -> Verdict:
    """The cross-check model's verdict on one proposal; a proposal without a usable name is not
    asked. An unusable reply is asked once more with a short follow-up; the retry is counted. A
    request whose pass was abandoned around it (CANCELLED set) is not cached."""
    cached_verdict = cached(run_dir / "checkers", proposal.name, Verdict)
    if cached_verdict is not None:
        return cached_verdict
    if proposal.answer is None or proposal.answer.proposed_name is None:
        return Verdict(name=proposal.name, address=proposal.address, reply=None, tokens=0, seconds=0.0,
                       problem=ToolMessage("no proposal to check"))
    started = time.monotonic()
    prompt = prompt_check(context, proposal, prefixes)
    tokens, retries, parsed, problem = 0, 0, None, ToolMessage("")
    for attempt in range(2):
        tag = f"{run_id} check {proposal.name} {attempt}"
        reply, problem = ask(model, prompt, CHECK_SYSTEM, tag)
        tokens += logged_tokens(USAGE_LOG, tag)
        if problem is None:
            parsed, problem = parse_check(reply)
        if problem is None:
            break
        retries = attempt + 1
        prompt = Prompt(f"{prompt}\n\nYour previous reply could not be used: {problem}\n"
                        f"Reply again with only the JSON object, nothing else.")
    verdict = Verdict(name=proposal.name, address=proposal.address, reply=parsed, tokens=tokens,
                      seconds=round(time.monotonic() - started, 1), retries=retries,
                      problem=problem)
    if cancelled is None or not cancelled.is_set():
        keep(run_dir / "checkers", proposal.name, verdict)
    return verdict


def _run_pass[Item, Out](items: tuple[Item, ...], jobs: int, one: Callable[[Item], Out],
                         stub: Callable[[Item, str], Out],
                         cancelled: threading.Event) -> tuple[Out, ...]:
    """one(item) for every item, at most JOBS daemon threads at a time, in the items' order. A
    request still unanswered STRAGGLER_SECONDS after the last other request of the pass finished is
    abandoned: the stub stands in for it (counted in the report, nothing cached under its name),
    the workers are told to stop taking work, and the run does not wait for them — daemon threads,
    so a hung provider call cannot hold the exit."""
    if not items:
        return ()
    results: dict[int, Out] = {}
    finished_at: list[float] = []
    state = {"next": 0}
    lock = threading.Lock()

    def worker() -> None:
        while not cancelled.is_set():
            with lock:
                index = state["next"]
                if index >= len(items):
                    return
                state["next"] = index + 1
            try:
                out = one(items[index])
            except Exception as error:            # one worker's crash must not take the pass down
                out = stub(items[index], f"the request itself failed: {error}")
            with lock:
                results[index] = out
                finished_at.append(time.monotonic())

    threads = [threading.Thread(target=worker, daemon=True)
               for _ in range(max(1, min(jobs, len(items))))]
    for thread in threads:
        thread.start()
    while True:
        with lock:
            missing = any(index not in results for index in range(len(items)))
            last = finished_at[-1] if finished_at else None
        if not missing:
            break
        if last is not None and time.monotonic() - last >= STRAGGLER_SECONDS:
            cancelled.set()     # every other request finished long ago: the rest are stragglers
            break
        time.sleep(0.5)
    with lock:
        for index in range(len(items)):
            if index not in results:
                results[index] = stub(items[index], f"abandoned: still unanswered {STRAGGLER_SECONDS} s "
                                                   "after the pass's last other request finished")
        return tuple(results[index] for index in range(len(items)))


def investigate_all(model: ModelSpec, contexts: tuple[Context, ...], run_dir: Path, run_id: str, jobs: int,
                    prefixes: tuple[str, ...]) -> tuple[Proposal, ...]:
    """Every context investigated, several at a time; a 402 (the provider is out of credits) stops
    the asking and the rest are reported as skipped, everything done kept."""
    stop = threading.Event()
    cancelled = threading.Event()

    def one(context: Context) -> Proposal:
        if stop.is_set():
            return Proposal(name=context.target.name, address=context.target.address, answer=None, tokens=0,
                            seconds=0.0, problem=ToolMessage("skipped: the model is out of credits"))
        proposal = ask_investigate(model, context, run_dir, run_id, prefixes, cancelled)
        if out_of_credits(proposal.problem):
            stop.set()
        return proposal

    def stub(context: Context, why: str) -> Proposal:
        return Proposal(name=context.target.name, address=context.target.address, answer=None, tokens=0,
                        seconds=0.0, problem=ToolMessage(why))

    return _run_pass(contexts, jobs, one, stub, cancelled)


def check_all(model: ModelSpec, contexts: tuple[Context, ...], proposals: tuple[Proposal, ...],
              run_dir: Path, run_id: str, jobs: int, prefixes: tuple[str, ...] = ()) -> tuple[Verdict, ...]:
    """Every proposal cross-checked, several at a time, the same 402 and straggler rules."""
    by_name = {context.target.name: context for context in contexts}
    listed = prefixes or name_prefixes()
    stop = threading.Event()
    cancelled = threading.Event()

    def one(proposal: Proposal) -> Verdict:
        context = by_name[proposal.name]
        if stop.is_set():
            return Verdict(name=proposal.name, address=proposal.address, reply=None, tokens=0, seconds=0.0,
                           problem=ToolMessage("skipped: the model is out of credits"))
        verdict = ask_check(model, context, proposal, run_dir, run_id, listed, cancelled)
        if out_of_credits(verdict.problem):
            stop.set()
        return verdict

    def stub(proposal: Proposal, why: str) -> Verdict:
        return Verdict(name=proposal.name, address=proposal.address, reply=None, tokens=0, seconds=0.0,
                       problem=ToolMessage(why))

    return _run_pass(proposals, jobs, one, stub, cancelled)


def status_of(proposal: Proposal, verdict: Verdict) -> SymbolStatus:
    """What the candidate may claim: probable when the checker agreed, hypothesis otherwise (a
    proposal without a name stays a hypothesis with its open question; symbols/db/README.md)."""
    if proposal.answer is None or proposal.answer.proposed_name is None:
        return SymbolStatus.HYPOTHESIS
    return SymbolStatus.PROBABLE if verdict.reply is not None and verdict.reply.verdict == "agree" \
        else SymbolStatus.HYPOTHESIS


# ── reading the answers ────────────────────────────────────────────────

_JSON_BLOCK = re.compile(r"```(?:json)?\s*(\{.*?\})\s*```", re.S)


def _json_object(reply: str) -> dict[str, object] | None:
    """The JSON object in a model's reply (a ```json block, or the first {...} span), or None."""
    match = _JSON_BLOCK.search(reply)
    candidates = [match[1]] if match else []
    first, last = reply.find("{"), reply.rfind("}")
    if first >= 0 and last > first:
        candidates.append(reply[first: last + 1])
    for candidate in candidates:
        try:
            loaded = cast(object, json.loads(candidate))
        except json.JSONDecodeError:
            continue
        if isinstance(loaded, dict):
            return cast(dict[str, object], loaded)
    return None


def _text(value: object) -> ToolMessage | None:
    return ToolMessage(" ".join(str(value).split())) if isinstance(value, str) and value.strip() else None


def _texts(value: object) -> tuple[ToolMessage, ...]:
    items = cast(list[object], value) if isinstance(value, list) else ()
    return tuple(ToolMessage(" ".join(str(item).split())) for item in items
                 if isinstance(item, str) and item.strip())


def parse_answer(reply: str) -> tuple[Answer | None, ToolMessage | None]:
    """The investigating model's answer, or the problem that left none (never guessed into shape).
    A proposed name the name rules refuse is treated as no name: the answer stays (its summary,
    its evidence), and the refusal lands in open_questions, so the function is still documented."""
    parsed = _json_object(reply)
    if parsed is None:
        return None, ToolMessage("the reply held no JSON object")
    name = parsed.get("proposed_name")
    if name is not None and not isinstance(name, str):
        return None, ToolMessage(f"proposed_name {name!r} is not a name or null")
    score = parsed.get("self_score")
    if isinstance(score, bool) or not isinstance(score, (int, float)) or not 0 <= score <= 5:
        score = None
    summary = _text(parsed.get("summary"))
    questions = _texts(parsed.get("open_questions"))
    if isinstance(name, str):
        why = name_problem(name)
        if why is not None:
            questions += (ToolMessage(f"the proposed name {name!r} was rejected: {why}"),)
            name = None
    if name is not None and summary is None:
        return None, ToolMessage("a proposed name without a summary")
    evidence = _texts(parsed.get("evidence"))
    if name is not None and not evidence:
        return None, ToolMessage("a proposed name without evidence")
    return Answer(proposed_name=SymbolName(name) if isinstance(name, str) else None,
                  summary=summary, interface=_text(parsed.get("interface")),
                  details=_text(parsed.get("details")), evidence=evidence,
                  self_score=int(score) if score is not None else None,
                  open_questions=questions), None


def parse_check(reply: str) -> tuple[CheckReply | None, ToolMessage | None]:
    """The cross-check model's verdict, or the problem that left none."""
    parsed = _json_object(reply)
    if parsed is None:
        return None, ToolMessage("the reply held no JSON object")
    verdict = parsed.get("verdict")
    if verdict not in ("agree", "revise", "disagree"):
        return None, ToolMessage(f"verdict {verdict!r} is not agree, revise or disagree")
    reason = _text(parsed.get("reason"))
    if reason is None:
        return None, ToolMessage("a verdict without a reason")
    name = parsed.get("revised_name")
    return CheckReply(verdict=cast(CheckVerdict, verdict), reason=reason,
                      revised_name=SymbolName(name) if isinstance(name, str) and name.strip()
                      and name_problem(name) is None else None,
                      revised_summary=_text(parsed.get("revised_summary"))), None


# ── the candidates ─────────────────────────────────────────────────────

TODAY = date.today().isoformat()
METHOD = "assembly-reading"
KIND = "function"


def make_candidate(context: Context, proposal: Proposal, verdict: Verdict, model: ModelSpec,
                   checker: ModelSpec, run_id: str) -> Candidate:
    """The symbols/db-format candidate entry for one function, ready to write and check. Only the
    database's keys: the proposed name opens the summary (\"Proposed name: X. ...\") and the
    cross-check's verdict and reason land in the evidence's result text and open_questions."""
    answer = proposal.answer
    proposed: SymbolName | None = None
    revise = verdict.reply is not None and verdict.reply.verdict == "revise"
    if answer is not None and answer.proposed_name is not None:
        proposed = (verdict.reply.revised_name if verdict.reply is not None and revise else None) \
            or answer.proposed_name
    summary = (verdict.reply.revised_summary if verdict.reply is not None and revise else None) \
        or (answer.summary if answer is not None else None) \
        or ToolMessage(f"unknown: {'; '.join(answer.open_questions) if answer else proposal.problem or ''}")
    if proposed is not None:
        summary = ToolMessage(f"Proposed name: {proposed}. {summary}")
    refs = [f"bn6f/asm/{context.target.source}"]
    refs += [f"0x{n.address:08X} {n.name}" for n in (*context.callers[:3], *context.callees[:3])]
    detail = "; ".join(answer.evidence) if answer is not None else (proposal.problem or "no answer")
    if not detail.strip():
        detail = "no name proposed: the open questions below say what is missing"
    score = answer.self_score if answer else None
    result = f"proposed by {model}" + (f", self_score {score}" if score is not None else "") \
        + f"; cross-checked by {checker}: " + (verdict.reply.verdict if verdict.reply else "none")
    if verdict.reply is not None and verdict.reply.verdict != "agree":
        result += f" — {verdict.reply.reason}"
    questions = list(answer.open_questions) if answer is not None else []
    if answer is None and proposal.problem:
        questions.append(ToolMessage(f"the investigation itself failed: {proposal.problem}"))
    if verdict.reply is not None and verdict.reply.verdict != "agree":
        questions.append(ToolMessage(verdict.reply.reason or "the cross-check did not agree"))
    return Candidate(name=proposal.name, address=context.target.address, kind=KIND,
                     status=status_of(proposal, verdict),
                     summary=summary, interface=answer.interface if answer is not None else None,
                     details=answer.details if answer is not None else None,
                     evidence=(EvidenceItem(method=METHOD, date=TODAY, detail=ToolMessage(detail),
                                            refs=tuple(refs),
                                            reproduce=ToolMessage(f"python3 tools/asmcat.py 0x{context.target.address:08X}"),
                                            result=ToolMessage(result)),),
                     coverage=context.target.routes or None, open_questions=tuple(questions),
                     sources=(f"wf investigate {run_id}", "models"))


def elf_symbols(wanted: frozenset[str]) -> dict[str, int]:
    """bn6f.elf's symbols among WANTED (name -> address, Thumb bit ignored), the way tools/symdb.py
    reads them (arm-none-eabi-nm, cached per ELF build in build/symdb)."""
    return symdb.elf_symbols(symdb.Cache(), wanted)


def symdb_check(path: Path, syms: dict[str, int]) -> tuple[ToolMessage, ...]:
    """What tools/symdb.py's own check says about one written candidate file: its loader
    (load_full) and its problems() on this one entry — the record built with its slim(), as its
    load() builds them — plus the spec's address-is-a-hex-string rule (symdb's check takes a
    decimal int too, the database's README does not)."""
    try:
        where = path.relative_to(REPO).as_posix()
    except ValueError:                             # outside the repo (a test's temp dir): read it there
        where = str(path)
    try:
        loaded = symdb.load_full([where])[0]
    except Exception as error:                     # the loader's own parse failed
        return (ToolMessage(f"the file does not load: {error}"),)
    if loaded.get("_unparsed"):
        return (ToolMessage("not a YAML mapping"),)
    if not isinstance(loaded.get("address"), str):
        return (ToolMessage("address is not a hex string"),)
    entry = symdb.slim(loaded)
    entry["_file"] = where
    return tuple(ToolMessage(problem.message) for problem in symdb.problems([entry], syms, symdb.Tree()))


def write_candidate(candidate: Candidate, run_dir: Path) -> Path:
    """The candidate as NAME.yaml beside the run's report."""
    run_dir.mkdir(parents=True, exist_ok=True)
    path = run_dir / f"{candidate.name}.yaml"
    text = yaml.safe_dump(candidate.model_dump(mode="json", exclude_none=True), sort_keys=False, allow_unicode=True)
    path.write_text(text)
    return path


def report_rows(pairs: tuple[tuple[Proposal, Verdict], ...]) -> str:
    """The run's report.tsv: address, old name, proposed name, status, self score, verdict, cost."""
    lines = ["# address\told\tproposed\tstatus\tself_score\tcross_check\ttokens\tretries"]
    for proposal, verdict in pairs:
        answer = proposal.answer
        proposed = (verdict.reply.revised_name if verdict.reply is not None
                    and verdict.reply.verdict == "revise" and verdict.reply.revised_name else None) \
            or (answer.proposed_name if answer else None) or "-"
        status = status_of(proposal, verdict).value
        score = answer.self_score if answer else None
        check = verdict.reply.verdict if verdict.reply else "none"
        retries = proposal.retries + verdict.retries
        lines.append(f"0x{proposal.address:08X}\t{proposal.name}\t{proposed}\t{status}\t"
                     f"{score if score is not None else '-'}\t{check}\t{proposal.tokens + verdict.tokens}"
                     f"\t{retries}")
    return "\n".join(lines) + "\n"


# ── what the run cost ──────────────────────────────────────────────────

def quota_snapshot(out: Path) -> float | None:
    """What the providers report as left, in OUT (tools/convert/quota.py takes a snapshot and
    appends it to its own log): Hyper's credit balance, or None when it said nothing."""
    subprocess.run([sys.executable, str(QUOTA), str(out)], capture_output=True, text=True, check=False)
    try:
        loaded = cast(object, json.loads(out.read_text()))
    except (OSError, json.JSONDecodeError):
        return None
    if not isinstance(loaded, dict):
        return None
    snap = cast(dict[str, object], loaded)
    hyper = snap.get("hyper")
    if isinstance(hyper, dict):
        balance = cast(dict[str, object], hyper).get("balance")
        if isinstance(balance, (int, float)):
            return float(balance)
    return None