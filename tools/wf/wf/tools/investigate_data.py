"""Investigating placeholder-named DATA labels of the bn6f disassembly with cheap models
(`wf investigate-data`): what a run selects out of the ~1,400 RAM labels and ~2,100 ROM tables
that still carry a placeholder name (`byte_203F750`, `unk_2017A00`, ...), what each model is shown
and asked, and the symbols/db-format candidate it writes for review. Nothing here touches bn6f,
symbols/db or git: the candidates and the report go under build/investigate_data/RUN_ID, and the
run is resumable (an answer once given is cached there, and labels with a finished candidate are
skipped).

A data label says nothing by itself — `byte_203F750` is 136 anonymous bytes of EWRAM — so the
context a model sees is the CODE around it: up to eight of the sites bn6f references it at (the
literal-pool load `ldr r6, off_80045B8 // =byte_203F750` and what the code then does with the
register, or the `.word byte_203F750` entry of a table), each with the owning function's name, its
symbols/db summary and a plain reading of the use (reads it, writes it, indexes it by r6 << 2,
passes it on to X). For a ROM label its own `.byte`/`.word` rows are shown too, with every
pointer-shaped word resolved to the symbol it points at; for a RAM label its size up to the next
label and its bn6f comment.

Selection (select_targets): placeholder labels with a meaning score of 1 or less (RAM, from
symbols/meaning_scores.tsv) plus the ROM tables bn6f defines with a placeholder name (they have no
score line), ranked by how many of the functions that reference them already have descriptive
names. Skipped: a label no code references at all, and a label that lies inside a structure
symbols/db or bn6f's own struct macros already describe field by field — its name comes from the
structure, not from a use.

Naming follows what bn6f already uses for data, quoted to the models from the ELF itself (the
`e<Struct>_<Field>` of an EWRAM struct field, the `o<Struct>_<Field>` of a struct offset, the
`i<Name>` of an IWRAM copy, the plain `prefix_name` of a table), and a name that repeats the
placeholder's digits, hedges or carries a 0x literal is treated as no name — the same rule the
function run uses. Before a candidate is written the name must also be free: not a symbol of
bn6f.elf, not a symbols/db name, not the name of an asm include file, and not an identifier the
port's own C already uses (a rename would collide with it)."""
import bisect
import re
import subprocess
import threading
import time
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Literal, cast

from wf.repo import REPO
from wf.tools import investigate
from wf.tools.investigate import (EvidenceItem, abandoned, ask, cached, comment_above, db_names,  # noqa: F401
                                  db_summaries, keep, label_index, logged_tokens, out_of_credits,
                                  symdb_check)
from wf.tools.models import Prompt
from wf.types import FunctionName, GuestAddress, ModelSpec, Result, SymbolName, SymbolStatus, ToolMessage

import gamepaths  # noqa: E402  # tools/gamepaths.py: where bn6f.elf is (wf.repo put tools/ on the path)
import funcindex  # noqa: E402  # tools/funcindex.py: the lifted units an address belongs to

Finder = Callable[[int], "str | None"]             # what funcindex.unit_finder makes: address -> unit name

SCORES = investigate.SCORES                        # per address: kind, name, meaning score 0-5
INVESTIGATE_DATA = REPO / "build/investigate_data"  # the runs' output: RUN_ID/NAME.yaml + report.tsv
ELF = Path(gamepaths.ORIG_ELF)                     # bn6f.elf: every symbol, with its nm type
ASM_DIR = investigate.ASM_DIR                      # where the functions' assembly lives
DATA_FILES = investigate.DATA_FILES                # the RAM data files (ewram.s, iwram_data.s, ...)
ROM_FILES = ("data.s",)                             # the ROM data file beside them
USAGE_LOG = investigate.USAGE_LOG

CONTEXT_TOKENS = 4_000                            # the brief a model sees, all sections together
CHARS_PER_TOKEN = investigate.CHARS_PER_TOKEN      # code and assembly, as tools/llm.py measures prompts
MAX_TOKENS = investigate.MAX_TOKENS                # output cap: the reasoning AND the JSON after it
MAX_LISTED = 8                                    # referencing sites shown per label
MAX_USERS = 12                                    # referencing functions named in one line
MAX_BYTES_ROWS = 12                               # .byte/.word rows of a ROM label shown
MAX_BYTES = 96                                    # bytes of a ROM label shown
SITE_INSTRUCTIONS = 15                            # instructions of an owner shown around one site
SITE_CHARS = 1_400                                # chars of one site's snippet, cut at a line
TABLE_ROWS = 3                                    # rows of a table shown around a .word site
STRAGGLER_SECONDS = investigate.STRAGGLER_SECONDS  # a request still unanswered this long is abandoned

DEFAULT_MODEL = ModelSpec("minimax/MiniMax-M3")   # the data run's cheap model proposes
DEFAULT_CHECKER = ModelSpec("minimax/MiniMax-M3")  # a second, independent call cross-checks it
DEFAULT_COUNT = 200                               # labels per run (--count)
DEFAULT_JOBS = 8                                  # model calls in flight (--jobs)

PLACEHOLDER = re.compile(r"^(?:byte|unk|dword|word|off|hword)_[0-9A-Fa-f]+$")
NAME_DIGITS = re.compile(r"[0-9A-Fa-f]{3,}")
DATA_NM_TYPES = frozenset("DBCRdbS")             # nm's data types (not t/T/w/W: code)
ABSOLUTE_NM_TYPES = frozenset("Na")              # the struct macros' absolute field symbols
INDEXED = re.compile(r"\[\s*(r\d+|sp|lr|sl|fp|ip)(?:,\s*(?P<reg>r\d+)(?:\s*,\s*lsl\s*#\s*(?P<shift>\d+))?)?"
                     r"(?P<imm>\s*,\s*#\s*(?P<offset>-?0x[0-9a-fA-F]+|\d+))?\s*\]")
CODE_LABEL = re.compile(r"^(?!\.|loc|locret_|byte_|unk_|word_|dword_|hword_|off_)([A-Za-z_]\w*)::?(?:\s|$)")

INVESTIGATE_SYSTEM = ("You investigate one data label of a GBA game's disassembly (MegaMan Battle "
                      "Network 6 Falzar) from the code that uses it, for the project's symbol "
                      "database. Reply with exactly one JSON object and nothing else.")
CHECK_SYSTEM = ("You check another model's reading of a data label of GBA assembly (MegaMan Battle "
                "Network 6 Falzar) against the code that uses it. Reply with exactly one JSON "
                "object and nothing else.")


# ── results ────────────────────────────────────────────────────────────

class DataTarget(Result):
    """One placeholder data label chosen for investigation, with what ranked it."""
    name: SymbolName
    address: GuestAddress
    score: int                          # its meaning score (0 or 1; ROM labels have none)
    region: str                         # ROM / EWRAM / IWRAM
    size: int                           # bytes up to the next label (0: the next label is unknown)
    source: str                         # bn6f/FILE.s:LINE where the label is defined
    references: int                     # sites bn6f shows the label at
    named_users: int                    # referencing functions that already have descriptive names
    rom: bool = False                   # its bytes are in the ROM (a table), not in RAM


class UseSite(Result):
    """One site that references the label: who owns it, and what the code does with it there."""
    kind: Literal["load", "word"]       # a literal-pool load (=NAME) or a .word entry of a table
    owner: FunctionName | None          # the function the site is in (a table entry has none)
    summary: ToolMessage | None         # the owner's symbols/db summary, when it has one
    file: str                           # bn6f/FILE.s
    line: int                           # the site's own line
    span: tuple[int, int] = (0, 0)      # the owning function's line range (0,0: none)
    table: ToolMessage | None           # the table a .word entry belongs to
    uses: tuple[ToolMessage, ...]       # reads it / writes it / indexes it by r6 << 2 / passes it on


class DataContext(Result):
    """Everything the models are shown about one label. The sites' snippets and the label's bytes
    stay on disk (contexts/NAME.asm, beside the run's candidates), so the run's state stays small."""
    target: DataTarget
    bytes_text: str                     # its .byte/.word rows (ROM only)
    comment: ToolMessage | None         # bn6f's `//` comment above the label
    users: tuple[FunctionName, ...]     # the referencing functions, most named first
    sites: tuple[UseSite, ...] = ()
    sites_text: Path = Path("")         # the snippets around them; empty: no site is known


class Answer(Result):
    """What the investigating model replied (one JSON object)."""
    proposed_name: SymbolName | None
    summary: ToolMessage | None
    kind: ToolMessage | None            # u8 / u16 / u32 / pointer / table of what / struct
    details: ToolMessage | None
    evidence: tuple[ToolMessage, ...]
    self_score: int | None
    open_questions: tuple[ToolMessage, ...]


class DataProposal(Result):
    """A label's investigation: the model's answer, or the problem that left none."""
    name: SymbolName
    address: GuestAddress
    answer: Answer | None
    tokens: int
    seconds: float
    retries: int = 0                   # answers asked again after an unusable reply (0 or 1)
    problem: ToolMessage | None = None
    finished: bool = False             # a candidate for it was already written (skipped, not asked)


CheckVerdict = Literal["agree", "revise", "disagree", "none"]


class CheckReply(Result):
    """What the cross-check model replied."""
    verdict: CheckVerdict
    reason: ToolMessage | None
    revised_name: SymbolName | None
    revised_summary: ToolMessage | None


class Verdict(Result):
    """A label's cross-check: the checker's reply, or `none` for a proposal with no name."""
    name: SymbolName
    address: GuestAddress
    reply: CheckReply | None
    tokens: int
    seconds: float
    retries: int = 0                   # replies asked again after an unusable one (0 or 1)
    problem: ToolMessage | None = None


class DataCandidate(Result):
    """A candidate symbols/db entry in the database's format and field order, with only the
    database's keys: what the models proposed lives in the summary's first words ("Proposed name:
    X. ..."), in the evidence's result text and in report.tsv, because a candidate is not a rename."""
    name: SymbolName
    address: GuestAddress
    kind: str
    status: SymbolStatus
    summary: ToolMessage
    details: ToolMessage | None
    evidence: tuple[EvidenceItem, ...]
    open_questions: tuple[ToolMessage, ...]
    sources: tuple[str, ...]

    @property
    def address_as_db(self) -> str:
        """As the database writes addresses: a hex string (symbols/db/README.md)."""
        return f"0x{self.address:08X}"


class Report(Result):
    """What a run produced: candidates written, how the cross-checks came out, what it cost."""
    selected: int
    written: int
    agreed: int
    revised: int
    disagreed: int
    nulls: int
    rejected: int                     # names the guards refused (taken in bn6f, symbols/db or the port)
    invalid: int                      # candidates tools/symdb.py's check refused
    retries: int                      # answers asked a second time after an unusable reply
    tokens: int
    problems: tuple[ToolMessage, ...]
    stragglers: int = 0               # requests abandoned still unanswered (see STRAGGLER_SECONDS)
    stopped: ToolMessage | None = None   # e.g. the provider ran out of credits mid-run

    def totals(self) -> str:
        per = self.tokens / self.selected if self.selected else 0.0
        return (f"{self.written} candidates of {self.selected} labels: {self.agreed} agreed, "
                f"{self.revised} revised, {self.disagreed} disagreed, {self.nulls} without a name, "
                f"{self.rejected} rejected by the name guards, {self.invalid} invalid; {self.tokens} "
                f"tokens (~{per:,.0f} per label); {self.retries} retries"
                + (f"; {self.stragglers} abandoned" if self.stragglers else "")
                + (f"; {self.stopped}" if self.stopped else ""))


# ── the pool: which labels are worth a model call ───────────────────────

DataScoreRow = tuple[SymbolName, GuestAddress, int]   # (name, address, meaning score) of one RAM label


def data_placeholder(name: str) -> bool:
    """A placeholder data label's name: byte_/unk_/dword_/word_/off_/hword_ and an address."""
    return PLACEHOLDER.fullmatch(name) is not None


def region_of(address: int) -> str:
    """Which GBA memory a label is in (the addresses bn6f's labels use)."""
    if address >= 0x08000000:
        return "ROM"
    if 0x02000000 <= address < 0x03000000:
        return "EWRAM"
    if 0x03000000 <= address < 0x04000000:
        return "IWRAM"
    if 0x04000000 <= address < 0x0E000000:
        return "I/O"
    return "unmapped"


def read_data_scores(path: Path = SCORES) -> tuple[DataScoreRow, ...]:
    """The data labels in symbols/meaning_scores.tsv (kind data only, so the function rows stay out)."""
    rows: list[DataScoreRow] = []
    for line in path.read_text().splitlines():
        if line.startswith("#") or not line.strip():
            continue
        fields = line.split("\t")
        if len(fields) < 4 or fields[1] != "data":
            continue
        try:
            address = int(fields[0], 16)
            score = int(fields[3])
        except ValueError:
            continue
        rows.append((SymbolName(fields[2]), GuestAddress(address), score))
    return tuple(rows)


# ── bn6f.elf and the files it is written in ────────────────────────────

_nm_cache: dict[str, tuple[int, str]] | None = None


def nm_symbols() -> dict[str, tuple[int, str]]:
    """Every bn6f.elf symbol: name -> (address, nm type letter), read once per process. The type
    letter tells code (t/T/w/W) from data (D/B/C/R/S) and the absolute struct symbols (N/a) apart,
    which is what makes a `.word` site's enclosing label a function or a table."""
    global _nm_cache
    if _nm_cache is not None:
        return _nm_cache
    out = subprocess.run(["arm-none-eabi-nm", str(ELF)], capture_output=True, text=True, check=True).stdout
    symbols: dict[str, tuple[int, str]] = {}
    for line in out.splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols.setdefault(fields[2], (int(fields[0], 16) & ~1, fields[1]))
    _nm_cache = symbols
    return symbols


@dataclass(frozen=True)
class Layout:
    """Where every label lives: name -> address, address -> the name to show for it, and the sorted
    data addresses (a label's size is the distance to the next one)."""
    symbols: dict[str, tuple[int, str]]
    names_at: dict[int, str]
    data_addresses: tuple[int, ...]

    def size_of(self, address: int, following: int = 0) -> int:
        """How many bytes from ADDRESS up to the next data label (0: none follows)."""
        index = bisect.bisect_right(self.data_addresses, address)
        nxt = self.data_addresses[index] if index < len(self.data_addresses) else 0
        return nxt - address if nxt and nxt > address else following

    def name_at(self, address: int) -> str | None:
        """The symbol at ADDRESS to show a pointer to (the most descriptive name there)."""
        return self.names_at.get(address)

    def is_code(self, name: str) -> bool:
        entry = self.symbols.get(name)
        return entry is not None and entry[1] in ("t", "T", "w", "W", "V", "v")


def layout() -> Layout:
    """The label layout of bn6f, from its ELF (once per process). At one address several names can
    live (a literal and the function it points at); the most descriptive one is kept: not a
    placeholder, then the shortest, then the first in nm's order."""
    symbols = nm_symbols()
    names_at: dict[int, str] = {}
    data_addresses: set[int] = set()
    for name, (address, kind) in symbols.items():
        if kind not in DATA_NM_TYPES and kind not in ABSOLUTE_NM_TYPES:
            continue                       # code symbols are not data labels
        data_addresses.add(address)
        current = names_at.get(address)
        if current is None or _better_name(name, current):
            names_at[address] = name
    return Layout(symbols=symbols, names_at=names_at,
                  data_addresses=tuple(sorted(data_addresses)))


def _better_name(candidate: str, current: str) -> bool:
    """Is CANDIDATE the name to show at an address where both CANDIDATE and CURRENT live?"""
    return ((data_placeholder(candidate), len(candidate), candidate)
            < (data_placeholder(current), len(current), current))


# ── which labels bn6f describes field by field ─────────────────────────

def struct_fields() -> dict[str, list[tuple[int, str]]]:
    """bn6f's struct fields, by structure: every `e<Struct>_<Field>`-shaped data label in
    symbols/meaning_scores.tsv, grouped by its <Struct> part. These are what the struct macros in
    bn6f/include/structs/*.inc and include/rom_structs/*.inc name, one label per field."""
    out: dict[str, list[tuple[int, str]]] = {}
    for name, address, _score in read_data_scores():
        if data_placeholder(name):
            continue                   # a placeholder is not a described field
        match = re.match(r"^[es]?([A-Za-z][A-Za-z0-9]*)_[A-Za-z0-9_]+$", name)
        if match is not None:
            out.setdefault(match.group(1), []).append((address, name))
    return {structure: sorted(fields) for structure, fields in out.items()}


MAX_PAD = 8                       # bytes a label may sit from a described field to count as its padding


def inside_described_struct(address: GuestAddress, fields: dict[str, list[tuple[int, str]]],
                            max_pad: int = MAX_PAD) -> str | None:
    """The structure whose described fields ADDRESS is the padding between, or None. A label that
    fills a gap of at most MAX_PAD bytes between two fields of one structure (or sits right after
    a described field, the way a structure's trailing padding does) gets its meaning from that
    structure, not from a use: the run leaves it to the structure's own entry. A label further
    from any field — a blob the structure covers but does not name — is still worth a model: no
    field gives it a name."""
    for structure, entries in fields.items():
        addresses = [entry[0] for entry in entries]
        index = bisect.bisect_left(addresses, address)
        below = entries[index - 1] if index > 0 else None
        above = entries[index] if index < len(entries) else None
        if below is not None and address - below[0] <= max_pad:
            return structure
        if above is not None and above[0] - address <= max_pad:
            return structure
    return None


# ── the references bn6f shows, and who owns them ───────────────────────

class RawSite(Result):
    """One reference to a data label as bn6f writes it, with the function whose body it sits in
    (bn6f's own file order, which is what a reader sees) and that function's line range."""
    name: SymbolName
    kind: Literal["load", "word"]
    file: str
    line: int
    owner: FunctionName | None         # the enclosing function; a table outside any function has none
    span: tuple[int, int] = (0, 0)     # the enclosing function's first and last line
    pool: ToolMessage | None          # the literal label a load's operand names
    table: ToolMessage | None         # the label a .word entry sits under
    loaded_into: ToolMessage | None   # the register a load puts the label's address in


def function_ranges(lines: Sequence[str]) -> tuple[tuple[int, int, str], ...]:
    """The functions a bn6f file holds, as (first line, last line, name), in file order: a label
    that bn6f's ELF calls code and that is not a pool or a local label opens one, and the
    matching `thumb_func_end` (or the next function's label) closes it."""
    opens: list[tuple[int, str]] = []
    for number, text in enumerate(lines, start=1):
        start = re.match(r"\s*thumb_func_start\s+(\w+)", text)
        name = start.group(1) if start is not None else \
            CODE_LABEL.match(text).group(1) if CODE_LABEL.match(text) is not None and \
            is_code_label(CODE_LABEL.match(text).group(1)) else None   # pyright: ignore[reportOptionalMemberAccess]
        if name is not None:
            opens.append((number, name))
    out: list[tuple[int, int, str]] = []
    for index, (start, name) in enumerate(opens):
        end = opens[index + 1][0] - 1 if index + 1 < len(opens) else len(lines)
        for number in range(start, min(end, len(lines)) + 1):
            if re.match(rf"\s*thumb_func_end\s+{re.escape(name)}\s*$", lines[number - 1]):
                end = number
                break
        out.append((start, end, name))
    return tuple(out)


def function_at(ranges: Sequence[tuple[int, int, str]], line: int) -> tuple[str, tuple[int, int]] | None:
    """The function whose line range holds LINE, and that range."""
    for start, end, name in ranges:
        if start <= line <= end:
            return name, (start, end)
    return None


def scan_sites(wanted: frozenset[SymbolName]) -> dict[SymbolName, tuple[RawSite, ...]]:
    """Where bn6f references each of WANTED: the literal-pool loads (`ldr r6, off_80045B8 //
    =NAME`) in the asm and data files, and the `.word NAME` entries of the data tables. One pass
    over every file, so the whole run's references are read once."""
    out: dict[SymbolName, list[RawSite]] = {}
    for path in source_files():
        lines = path.read_text(errors="replace").splitlines()
        ranges = function_ranges(lines)
        label: str | None = None
        for number, text in enumerate(lines, start=1):
            definition = re.match(r"^(\w+)::?(?:\s|$)", text)
            if definition is not None:
                label = definition.group(1)
            here = function_at(ranges, number)
            owner, span = (here[0], here[1]) if here is not None else (None, (0, 0))
            code, _, note = text.partition("//")
            if note:
                for name in re.findall(r"=(\w+)", note):
                    if name in wanted:
                        operand = code.partition("//")[0].strip()
                        pool = re.search(r"([.\w]*\w+)\s*$", operand)
                        load = re.match(r"\s*(?:ldr|ldrd)\s+(r\d+|sp|pc|lr)\b", operand)
                        out.setdefault(SymbolName(name), []).append(RawSite(
                            name=SymbolName(name), kind="load", file=file_of(path), line=number,
                            owner=FunctionName(owner) if owner else None, span=span,
                            pool=ToolMessage(pool.group(1)) if pool is not None else None,
                            table=None,
                            loaded_into=ToolMessage(load.group(1)) if load is not None else None))
            for name in re.findall(r"\.word\s+(\w+)", code):
                if name in wanted and label is not None and label != name:
                    out.setdefault(SymbolName(name), []).append(RawSite(
                        name=SymbolName(name), kind="word", file=file_of(path), line=number,
                        owner=FunctionName(owner) if owner else None, span=span,
                        pool=None, table=ToolMessage(label), loaded_into=None))
    return {name: tuple(sites) for name, sites in out.items()}


_source_cache: tuple[Path, ...] | None = None


def source_files() -> tuple[Path, ...]:
    """The bn6f files a data reference can sit in: the RAM data files, the ROM data files and the
    data chunks (functions lifted into a chunk sit beside the data), then the asm files."""
    global _source_cache
    if _source_cache is not None:
        return _source_cache
    files = [REPO / "bn6f" / name for name in DATA_FILES + ROM_FILES]
    files += sorted((REPO / "bn6f/data").glob("*.s"))
    files += sorted(ASM_DIR.glob("*.s"))
    _source_cache = tuple(path for path in files if path.exists())
    return _source_cache


def file_of(path: Path) -> str:
    """A source path as the context and the evidence name it (bn6f/FILE.s)."""
    try:
        return path.relative_to(REPO).as_posix()
    except ValueError:
        return path.as_posix()


# ── how the code uses a label at one site ──────────────────────────────

def uses_of(lines: Sequence[str], register: str | None, table: str | None) -> tuple[ToolMessage, ...]:
    """What the code around a site does with the label's address, in plain words: reads it, writes
    it, indexes it (by what, scaled by what), or hands it to a call. Read off the ~15 instructions
    around the site, so a model does not have to read the assembly itself."""
    if register is None:
        if table is not None:
            return (ToolMessage(f"a .word entry of the table {table}"),)
        return ()
    out: list[str] = []
    found = False
    for line in lines:
        instruction = line.strip()
        if not instruction or instruction.startswith((".", "//", "thumb_")):
            continue
        match = re.match(r"\s*(ldr\w*|str\w*)\s+([^,]+),\s*(\[.*\])\s*(?://.*)?$", line)
        if match is None:
            continue
        access, operand = match.group(1), match.group(3)
        where = INDEXED.fullmatch(operand.strip())
        if where is None or where.group(1) != register:
            continue
        found = True
        index, shift, offset = where.group("reg"), where.group("shift"), where.group("offset")
        scaled = ""
        if shift is not None:
            scaled = " (dwords)" if shift == "2" else f" (<< {shift})" if shift != "0" else ""
        if index is not None:
            out.append(f"indexed: {access} {operand} — {index} << {shift or '0'}{scaled}")
        elif offset:
            out.append(f"{'reads' if access.startswith('ldr') else 'writes'} at {offset}: "
                       f"{access} {operand}")
        else:
            out.append(f"{'reads' if access.startswith('ldr') else 'writes'} it: "
                       f"{access} {operand}")
    if not found:
        # the register may be moved on before any access: the call it is passed to is the use
        for line in lines:
            passed = re.match(r"\s*mov\s+(r\d+|sp),\s*(\w+)\s*$", line)
            if passed is not None and passed.group(2) == register:
                register = passed.group(1)
                break
        for index, line in enumerate(lines):
            call = re.match(r"\s*bl\w*\s+(\w+)", line.strip())
            if call is None:
                continue
            before = "\n".join(lines[max(0, index - 6):index])
            if re.search(rf"\bmov\s+{register}\b|{register}\]", before) or register in ("r0", "r1"):
                out.append(f"passed to {call.group(1)} (in {register})")
        if not out:
            out.append(f"its address is kept in {register} and used below (no direct ldr/str seen)")
    seen: list[str] = []
    for text in out:
        if text not in seen:
            seen.append(text)
    return tuple(ToolMessage(text) for text in seen[:3])


# ── the bytes of a ROM label ───────────────────────────────────────────

class ByteRow(Result):
    """One `.byte`/`.word`/`.space` row of a ROM label, as the assembly writes it."""
    offset: int
    directive: str
    text: str                          # the row as bn6f writes it, pointers resolved to names


def pointer_names(text: str, place: Layout) -> str:
    """The `.word`s of a ROM row that point into the game's memory, each resolved to the symbol it
    names (0x08xxxxxx ROM, 0x02xxxxxx EWRAM, 0x03xxxxxx IWRAM), so a pointer table reads as the
    list of the things it points at."""
    out: list[str] = []
    for value in re.findall(r"0x([0-9a-fA-F]{6,8})\b", text):
        word = int(value, 16)
        name = place.name_at(word)
        if name is None and region_of(word) == "ROM":
            continue
        out.append(f"0x{word:08X}" + (f" = {name}" if name is not None else " (no symbol there)"))
    return ", ".join(out)


def byte_rows(lines: Sequence[str], place: Layout, rows: int = MAX_BYTES_ROWS,
              budget: int = MAX_BYTES) -> tuple[tuple[ByteRow, ...], int]:
    """The `.byte`/`.word` rows a ROM label is written as (up to ROWS rows and BUDGET bytes), and
    how many bytes they cover. A pointer-shaped word is annotated with the symbol it points at."""
    out: list[ByteRow] = []
    offset = shown = 0
    for line in lines:
        text = line.strip()
        match = re.match(r"\.(byte|word|hword|halfword|short|long|dcb|dw)\b(.*)", text, re.I)
        space = re.match(r"\.space\s+(\d+)", text)
        if space is not None:
            count = int(space.group(1))
            out.append(ByteRow(offset=offset, directive="space", text=f".space {count}"))
            offset += count
            continue
        if match is None or len(out) >= rows:
            continue
        values = re.findall(r"0x[0-9a-fA-F]+|\d+", match.group(2))
        width = 4 if match.group(1).lower() in ("word", "long", "dw") else \
            2 if match.group(1).lower() in ("hword", "halfword", "short") else 1
        body = match.group(0)
        pointers = pointer_names(body, place)
        out.append(ByteRow(offset=offset, directive=match.group(1).lower(),
                           text=f"{body}" + (f"   → {pointers}" if pointers else "")))
        offset += width * len(values)
        shown += width * len(values)
        if shown >= budget:
            break
    return tuple(out), shown


# ── the target list ────────────────────────────────────────────────────

def finished_names() -> frozenset[str]:
    """The labels any investigate-data run already wrote a candidate for (earlier runs and this one)."""
    if not INVESTIGATE_DATA.is_dir():
        return frozenset()
    return frozenset(path.stem for path in INVESTIGATE_DATA.glob("*/[a-zA-Z]*.yaml"))


def described_structs() -> dict[str, list[tuple[int, str]]]:
    """The struct fields to skip a label for, cached: symbols/db's own struct-field entries name the
    structures bn6f does not (a field of a structure the macros do not lay out)."""
    global _struct_cache
    if _struct_cache is None:
        fields = struct_fields()
        _struct_cache = fields
    return _struct_cache


_struct_cache: dict[str, list[tuple[int, str]]] | None = None


def ram_pool(path: Path = SCORES) -> tuple[DataScoreRow, ...]:
    """The RAM placeholder labels: a data row of symbols/meaning_scores.tsv whose meaning score is 1
    or less and whose name is still a placeholder (byte_203F750, unk_2017A00, ...)."""
    return tuple((name, address, score) for name, address, score in read_data_scores(path)
                 if score <= 1 and data_placeholder(name)
                 and region_of(address) in ("EWRAM", "IWRAM", "unmapped"))


def rom_pool(place: Layout, place_index: dict[str, tuple[Path, int]]) -> tuple[DataScoreRow, ...]:
    """The ROM placeholder tables: bn6f defines them with a placeholder name in a data file (a
    `.byte`/`.word` table in bn6f/data/*.s), which symbols/meaning_scores.tsv does not carry (it
    holds the RAM labels and the struct fields). No meaning score: they score 0 here."""
    return tuple((SymbolName(name), GuestAddress(address), 0)
                 for name, (address, kind) in place.symbols.items()
                 if kind in DATA_NM_TYPES and address >= 0x08000000
                 and data_placeholder(name) and name in place_index)


def rank(targets: tuple[DataTarget, ...]) -> tuple[DataTarget, ...]:
    """Worth investigating first: the most functions with descriptive names reference it, then the
    most sites reference it at all, then the name (so a run is repeatable)."""
    def key(target: DataTarget) -> tuple[int, int, str]:
        return (-target.named_users, -target.references, target.name)
    return tuple(sorted(targets, key=key))


def select_targets(count: int, only: tuple[SymbolName, ...] = (),
                   done: frozenset[str] | None = None) -> tuple[tuple[DataTarget, ...], tuple[ToolMessage, ...]]:
    """The placeholder data labels worth investigating, ranked, up to COUNT (all of ONLY when given);
    the notes say which --only names were not eligible and why. Skipped: a label no code references
    at all, a label inside a structure bn6f or symbols/db already describes field by field, and a
    label an earlier run already wrote a candidate for."""
    place = layout()
    labels = label_index()
    place_index = {name: entry for name, entry in labels.items() if file_of(entry[0]).startswith("bn6f/")}
    wanted = {name for name, _address, _score in ram_pool()} | {name for name, _a, _s in rom_pool(place, place_index)}
    sites = scan_sites(frozenset(wanted))
    fields = described_structs()
    find = funcindex.unit_finder(funcindex.unit_ranges())
    db = db_names()
    finished = finished_names() if done is None else done
    skipped_struct = no_refs = 0
    targets: list[DataTarget] = []
    seen: set[str] = set()
    for name, address, score in (*ram_pool(), *rom_pool(place, place_index)):
        if name in seen or name in finished:
            continue
        if only and name not in only:
            continue
        seen.add(name)
        if inside_described_struct(address, fields) is not None:
            skipped_struct += 1
            continue
        found = sites.get(name, ())
        if not found:
            no_refs += 1
            continue
        owners: list[str] = []
        for site in found:
            owner = owner_of(site, place, find)
            if owner is not None and owner not in owners:
                owners.append(owner)
        named = sum(1 for owner in owners if not investigate.placeholder(owner) or owner in db)
        index = place_index.get(name)
        targets.append(DataTarget(
            name=name, address=GuestAddress(address), score=score, region=region_of(address),
            size=place.size_of(address, following=1), source=f"{file_of(index[0])}:{index[1]}"
            if index is not None else "",
            references=len(found), named_users=named, rom=region_of(address) == "ROM"))
    ranked = rank(tuple(targets))
    chosen = ranked if only else ranked[:count]
    have = {target.name for target in chosen}
    notes = [ToolMessage(f"{name}: not an eligible placeholder data label "
                         f"(meaning score, an earlier run's candidate, no reference, or a described struct)")
             for name in only if name not in have]
    if not only:
        notes = (ToolMessage(f"pool: {len(ranked)} eligible placeholder labels with references "
                             f"({no_refs} with none, {skipped_struct} inside a described structure)"),
                 *notes)
    return chosen, tuple(notes)


def is_code_label(name: str) -> bool:
    """Does bn6f's ELF call NAME a code symbol? A pool label (off_8003780, dword_80339B4) and a
    local label (loc_801CC2A) sit in the text section too, so the CODE_LABEL pattern already keeps
    them out; this only asks the ELF whether the label it kept is code."""
    entry = nm_symbols().get(name)
    return entry is not None and entry[1] in ("t", "T", "w", "W")


def owner_of(site: RawSite, place: Layout, find: Finder) -> FunctionName | None:
    """The function a site belongs to: the one bn6f's file order puts it in, or — when the site sits
    outside any function bn6f writes — the unit at the literal label a load's operand names."""
    if site.owner is not None:
        return site.owner
    for name in (site.pool, site.table):
        if name is None:
            continue
        entry = place.symbols.get(str(name))
        if entry is None:
            continue
        found = find(entry[0])
        if found is not None and place.is_code(str(name)):
            return FunctionName(found)
    return None


# ── the context a model is shown ───────────────────────────────────────

def gather_context(target: DataTarget, run_dir: Path, labels: dict[str, tuple[Path, int]],
                   summaries: dict[str, str], find: Finder,
                   sites: tuple[RawSite, ...]) -> DataContext:
    """What the models see about one label: its address, region, size and bn6f comment, its bytes
    (ROM only), and up to MAX_LISTED of the sites that reference it, each with the owning function
    and a plain reading of the use. The snippets are written beside the run's candidates."""
    place = layout()
    index = labels.get(target.name)
    comment = comment_above(index[0], index[1], target.name) if index is not None else None
    rows, shown = byte_rows(rows_of(index), place) if target.rom else ((), 0)
    chosen = choose_sites(target, sites, place, find, summaries)
    owners: list[FunctionName] = []
    for site in chosen:
        if site.owner is not None and site.owner not in owners:
            owners.append(site.owner)
    body = sites_text(target, chosen)
    whole = place.size_of(target.address, following=target.size)
    if target.rom:
        text = render_bytes(rows, target, whole, shown)
    else:
        text = (f"(it is {target.region}: {target.size} bytes of RAM the game fills at run time, "
                f"so it has no bytes of its own to show — its meaning is in the uses below)")
    return DataContext(
        target=target, bytes_text=text, comment=comment, users=tuple(owners), sites=chosen,
        sites_text=write_sites(run_dir, target.name, body) if body else Path(""))


def render_bytes(rows: tuple[ByteRow, ...], target: DataTarget, whole: int, shown: int) -> str:
    """A ROM label's own bytes as bn6f writes them, one row per line, with a note of how much of
    the label was left out."""
    if not rows:
        return "(bn6f writes no .byte/.word rows for it: it is a bare label)"
    body = "\n".join(row.text for row in rows)
    more = (f"\n(... {whole - shown} of {whole} bytes cut: a label that large is named for what the "
            f"code does with it, not for its contents)")
    return f"+0x{rows[0].offset:02X}: {body}{more}" if whole > shown else f"+0x{rows[0].offset:02X}: {body}"


def rows_of(index: tuple[Path, int] | None) -> list[str]:
    """The assembly lines a label is written as (its definition up to the next label), for a ROM one."""
    if index is None:
        return []
    lines = index[0].read_text(errors="replace").splitlines()[index[1] - 1:]
    out: list[str] = []
    for line in lines[1:]:
        if re.match(r"^\w+::?(?:\s|$)", line):
            break
        out.append(line)
    return out


def window(site: RawSite | UseSite, lines: Sequence[str], rows: int) -> tuple[int, int]:
    """The line range to show around a site: about ROWS lines, never crossing the owning function's
    own boundaries (a snippet that runs into the next function reads as if that code were the use)."""
    low, high = site.line - 1 - rows, site.line + rows
    start, end = site.span
    if start:
        low = max(low, start - 1)
        high = min(high, end)
    return max(0, low), max(0, high)


def choose_sites(target: DataTarget, sites: tuple[RawSite, ...], place: Layout,
                 find: Finder, summaries: dict[str, str]) -> tuple[UseSite, ...]:
    """The sites the context shows, up to MAX_LISTED: the ones a named function owns first, then
    the rest of the code's, then the table entries."""
    scored: list[tuple[tuple[int, int, int, str, int], UseSite]] = []
    seen: set[tuple[str, int]] = set()
    for site in sites:
        owner = owner_of(site, place, find)
        summary = ToolMessage(summaries[owner]) if owner in summaries else None
        uses = uses_at(site)
        if (site.file, site.line) in seen:
            continue
        seen.add((site.file, site.line))
        key = (0 if owner is not None and not investigate.placeholder(owner) else 1,
               1 if site.kind == "word" else 0,
               0 if summary is not None else 1, site.file, site.line)
        scored.append((key, UseSite(kind=site.kind, owner=FunctionName(owner) if owner else None,
                                    summary=summary, file=site.file, line=site.line,
                                    span=site.span, table=site.table, uses=uses)))
    scored.sort(key=lambda pair: pair[0])
    _ = target
    return tuple(site for _key, site in scored[:MAX_LISTED])


def uses_at(site: RawSite) -> tuple[ToolMessage, ...]:
    """What the code at one site does with the label, from the instruction lines around it."""
    path = REPO / site.file
    try:
        lines = path.read_text(errors="replace").splitlines()
    except OSError:
        return ()
    low, high = window(site, lines, SITE_INSTRUCTIONS // 2)
    around = lines[low:high]
    return uses_of(around, str(site.loaded_into) if site.loaded_into else None,
                   str(site.table) if site.table else None)


def write_sites(run_dir: Path, name: SymbolName, text: str) -> Path:
    """The sites' snippets, one file beside the run's candidates (the prompts read it from there)."""
    path = run_dir / "contexts" / f"{name}.asm"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return path


def sites_text(target: DataTarget, sites: tuple[UseSite, ...]) -> str:
    """The snippets file's text: one section per site, the owning function's code around it and
    what the use is."""
    parts = [f"# how the code uses {target.name}: these sites are the evidence for its meaning"]
    for site in sites:
        path = REPO / site.file
        try:
            lines = path.read_text(errors="replace").splitlines()
        except OSError:
            continue
        who = site.owner if site.owner is not None else (f"table {site.table}" if site.table else "?")
        header = (f"== {site.file}:{site.line} {who}"
                  + (f" — {site.summary}" if site.summary else "")
                  + f"\n   the site {'reads, writes or indexes' if site.kind == 'load' else 'is'}: "
                  + "; ".join(str(use) for use in site.uses))
        if site.kind == "word":
            low, high = window(site, lines, TABLE_ROWS)
        else:
            low, high = window(site, lines, SITE_INSTRUCTIONS // 2)
        text = "\n".join(lines[low:high]).rstrip()
        if len(text) > SITE_CHARS:
            text = text[:SITE_CHARS].rsplit("\n", 1)[0] + "\n(... cut)"
        parts.append(f"{header}\n{text}\n")
    return "\n".join(parts)


def gather_all(targets: tuple[DataTarget, ...], run_dir: Path) -> tuple[DataContext, ...]:
    """Every target's context: the shared indexes once, then one context per label."""
    labels = label_index()
    summaries = db_summaries()
    find = funcindex.unit_finder(funcindex.unit_ranges())
    place = layout()
    wanted = frozenset(target.name for target in targets)
    sites = scan_sites(wanted)
    return tuple(gather_context(target, run_dir, labels, summaries, find, sites.get(target.name, ()))
                 for target in targets)


# ── the brief the models see ───────────────────────────────────────────

def context_text(context: DataContext) -> str:
    """The ~4k-token brief: where the label lives and what it is, its bytes, then the uses (the
    evidence, so the sites come last and the bytes can be cut first)."""
    target = context.target
    lines = [f"{target.name} at 0x{target.address:08X} ({target.region}, {target.size} bytes up to "
             f"the next label, meaning score {target.score}, {target.references} sites reference it, "
             f"{target.named_users} of them in functions with descriptive names)"]
    if target.source:
        lines.append(f"bn6f defines it at {target.source}")
    if context.comment:
        lines.append(f"bn6f's comment above it:\n  {' '.join(str(context.comment).split())[:300]}")
    if context.users:
        listed = ", ".join(context.users[:MAX_USERS])
        more = f" (+{len(context.users) - MAX_USERS} more)" if len(context.users) > MAX_USERS else ""
        lines.append(f"Functions that reference it: {listed}{more}")
    body = "\n".join(lines)
    if context.sites_text.exists():
        sites = context.sites_text.read_text(errors="replace").strip()
    else:
        sites = ""
    head = body + "\n\nIts own bytes:\n" + context.bytes_text
    if sites:
        head += "\n\nUses (the evidence):\n" + sites
    budget = int(CONTEXT_TOKENS * CHARS_PER_TOKEN) - 200
    return cap_sections(head, budget)


def cap_sections(text: str, budget: int) -> str:
    """The brief cut to BUDGET chars: the bytes go first (they are the least load-bearing), then
    the last sites, so the label's identity and its first uses always survive."""
    if len(text) <= budget:
        return text
    head, _, rest = text.partition("\n\nUses (the evidence):")
    bytes_at = head.partition("\n\nIts own bytes:")
    kept = bytes_at[0] + "\n\nIts own bytes:\n(omitted: the brief had no room left; ask for it " \
                         "from the file the label is written in)"
    if len(kept) >= budget:
        return kept[:budget]
    room = budget - len(kept) - 32
    lines = rest.splitlines()
    out: list[str] = []
    used = 0
    for line in lines:
        if used + len(line) + 1 > room:
            out.append(f"(... {len(lines) - len(out)} lines cut)")
            break
        out.append(line)
        used += len(line) + 1
    return kept + "\n\nUses (the evidence):\n" + "\n".join(out)


# ── the naming conventions the models are told about ───────────────────

@dataclass(frozen=True)
class Conventions:
    """What bn6f's own data names look like, with an example of each shape."""
    lines: tuple[str, ...]


def name_conventions(symbols: dict[str, tuple[int, str]] | None = None) -> Conventions:
    """The data naming shapes bn6f already uses, most used first, each with real examples: the
    `e<Struct>_<Field>` of an EWRAM struct field, the `o<Struct>_<Field>` of a struct offset, the
    `i<Name>` of an IWRAM copy, the `s<Name>` of a second copy, and a plain `prefix_name`."""
    table = nm_symbols() if symbols is None else symbols
    shapes: dict[str, list[str]] = {}
    for name in table:
        if data_placeholder(name) or name.startswith(("sub_", "loc_", "nullsub_", "dead_", "unused_",
                                                       "locret_", "asm", "func_")):
            continue
        if re.match(r"^e[A-Z][A-Za-z0-9]*_[A-Za-z0-9_]+$", name):
            shape = "e<Struct>_<Field> — a field of an EWRAM struct (the commonest shape by far)"
        elif re.match(r"^o[A-Z][A-Za-z0-9]*_[A-Za-z0-9_]+$", name):
            shape = "o<Struct>_<Field> — a field's offset inside a struct"
        elif re.match(r"^[si][A-Z][A-Za-z0-9]*$", name):
            shape = "s<Name> / i<Name> — a second copy of a table (s) or an IWRAM copy (i)"
        elif re.match(r"^[a-z][a-zA-Z0-9]*_[A-Za-z0-9_]+$", name):
            shape = "prefix_name — a standalone table or variable (prefix in lower case)"
        elif re.match(r"^[A-Z][A-Za-z0-9]+$", name):
            shape = "CamelCase — a whole table named for what it is (no prefix)"
        else:
            continue
        shapes.setdefault(shape, []).append(name)
    lines: list[str] = []
    for shape, names in sorted(shapes.items(), key=lambda pair: (-len(pair[1]), pair[0])):
        examples = ", ".join(sorted(names)[:6])
        lines.append(f"  {shape} ({len(names)} in bn6f): {examples}")
    return Conventions(lines=tuple(lines))


# ── the names a proposal may carry ─────────────────────────────────────

def name_problem(name: str, placeholder: str | None = None) -> ToolMessage | None:
    """Why a proposed name is not usable — a hedge, a 0x literal, a copy of the placeholder's own
    digits (byte_203F750 must not come back as byte_203F750_HP) or the placeholder itself — or None
    when it reads like a name the project keeps."""
    if "maybe" in name.lower():
        return ToolMessage("it hedges (Maybe)")
    if re.search(r"0x[0-9a-fA-F]", name):
        return ToolMessage("it carries a 0x literal")
    if re.search(r"[0-9]", name):
        if not re.fullmatch(r"\w+", name):
            return ToolMessage("it is not an identifier")
        if placeholder is not None and name == placeholder:
            return ToolMessage("it repeats the placeholder name")
        if placeholder is not None:
            digits = set(NAME_DIGITS.findall(placeholder.upper()))
            for run in NAME_DIGITS.findall(name.upper()):
                for width in range(len(run), 2, -1):
                    for start in range(len(run) - width + 1):
                        piece = run[start:start + width]
                        if any(piece in known for known in digits):
                            return ToolMessage(f"the digit run {piece} is copied from the placeholder "
                                               f"{placeholder}")
        return ToolMessage("it carries digits")
    return None


# ── the guards a name must pass before a candidate is written ──────────

def taken_names() -> frozenset[str]:
    """The names a new data name may not take: every symbol of bn6f.elf and every symbols/db name."""
    return frozenset(nm_symbols()) | db_names()


def asm_include(name: str) -> bool:
    """Is NAME also the name of an asm include file? A rename to it would rewrite the include's
    path instead of the file (the same guard the function landing uses)."""
    return (ASM_DIR / f"{name}.s").exists() or (REPO / "bn6f/data" / f"{name}.s").exists()


_port_ids: frozenset[str] | None = None


def port_identifiers() -> frozenset[str]:
    """Every identifier the port's own C uses (cport/src/game, cport/src, cport/host,
    cport/include, but not the generated lifted.h). A data name that collides with one of them
    would break the port build when the rename lands, so it is refused up front."""
    global _port_ids
    if _port_ids is None:
        token = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
        found: set[str] = set()
        for pattern in ("cport/src/game/*.c", "cport/src/*.c", "cport/host/*.c", "cport/include/*.h"):
            for path in sorted((REPO).glob(pattern)):
                if path.name == "lifted.h":
                    continue
                found.update(token.findall(path.read_text(errors="replace")))
        _port_ids = frozenset(found)
    return _port_ids


def guard_problem(name: str, taken: frozenset[str] | None = None,
                  port: frozenset[str] | None = None) -> ToolMessage | None:
    """Why NAME cannot be used for a data label, or None. It must not be a symbol of bn6f.elf, not
    a symbols/db name, not an asm include file's name, and not an identifier the port's own C
    already uses for something else (a rename would collide with it)."""
    known = taken_names() if taken is None else taken
    if name in known:
        return ToolMessage("the name is already taken (a symbol of bn6f.elf or a symbols/db entry)")
    if asm_include(name):
        return ToolMessage("its name is also an asm include file: a rename would rewrite the "
                           "include path, not the file")
    identifiers = port_identifiers() if port is None else port
    if name in identifiers:
        return ToolMessage("the name is already an identifier in cport/ (grep -w): the rename "
                           "would collide with it")
    return None


# ── the prompts ────────────────────────────────────────────────────────

INVESTIGATE_TASK = """You will be shown one undocumented data label of MegaMan Battle Network 6 Falzar (GBA):
where it lives (address, region, size up to the next label), its own bytes when it is ROM data, and
the code that uses it — up to eight of the sites bn6f references it at, each with the owning
function, what symbols/db knows about that function, and a plain reading of the use (reads it,
writes it, indexes it by what and scaled by what, or passes it to a call). Say what the label is for.

A data label's meaning comes from the USES, not from its own bytes: a `.word` table's rows show
what it is a table of, and the functions that read and write a RAM variable show what it holds.
Name it for what the game uses it for ("the X of ...", "per-panel ...", "count of ..."), and for a
field of a structure the project's convention is the structure's name and the field's
({conventions}). A label that is a whole array gets a plural or a `_table` shape if the project's
existing names do.

Reason briefly, then reply with exactly one JSON object and nothing else — no markdown fence, no
prose before or after it (both spend the reply's token budget and can cut the JSON off):
{{
  "proposed_name": "eBattleState_PanelDamage" or null,
  "summary": "one line, in game terms",
  "type": "u8" | "u16" | "u32" | "pointer" | "table of what" | "struct",
  "details": "what holds it, what writes it, what reads it, quirks — at most 6 sentences",
  "evidence": ["what in the code shows it, one line each, citing the sites by file:line"],
  "self_score": 3,
  "open_questions": ["what is still unknown"]
}}

Keep the JSON small: at most five evidence items, one line each — it shares the reply's token
budget with your reasoning, and a reply cut off before the JSON is wasted.

Naming: follow the shapes above (they are counted from bn6f's own symbols). The name must not
repeat any digit of the placeholder {placeholder} (spell numbers out: playerHp, not byte_203F750),
must not hedge (no "Maybe ..."), and must not carry a 0x literal.
Do not guess. If the uses do not show what the label holds, answer "proposed_name": null and say in
open_questions what is missing (which function writes it, which structure it belongs to, ...).
self_score, 0-5: 0 you could not say anything; 2 a plausible guess; 3 read from the uses and
consistent with every site seen; 4 as 3, and the owners' known summaries back it; 5 certain from
the code alone."""

CHECK_TASK = """You will be shown the same material about one undocumented data label, and another
model's proposal for it (not its reasoning). Check the proposal against the code that uses the
label.

The proposal:
- proposed_name: {name}
- summary: {summary}
- type: {kind}
- details: {details}

Reason briefly, then reply with exactly one JSON object and nothing else — no markdown fence, no
prose around it:
{{
  "verdict": "agree" or "revise" or "disagree",
  "reason": "what in the code agrees or contradicts it, citing the sites by file:line concretely",
  "revised_name": null or "a better name, when revising",
  "revised_summary": null or "a better one-line summary, when revising"
}}

Keep it small: the reason in at most five sentences. A revised_name follows the same naming rules
({conventions}) and repeats no digit of the placeholder {placeholder}, carries no 0x literal and no
"Maybe".

agree = the name and summary fit the uses; revise = close, but you have a better name or summary;
disagree = the code shows something else. Cite the code in the reason either way. Do not guess
either: if the code does not settle it, disagree and say why."""


def prompt_investigate(context: DataContext, conventions: Conventions) -> str:
    """The investigating model's prompt: the task, then the context."""
    listed = "\n".join(conventions.lines) or "  (none found)"
    return f"{INVESTIGATE_TASK.format(conventions=listed, placeholder=context.target.name)}\n\n" \
           f"{context_text(context)}"


def prompt_check(context: DataContext, proposal: DataProposal, conventions: Conventions) -> str:
    """The cross-check model's prompt: the same context and the proposal, not its reasoning."""
    answer = proposal.answer
    assert answer is not None   # the caller only asks with a proposal that has an answer
    listed = "\n".join(conventions.lines) or "  (none found)"
    task = CHECK_TASK.format(name=answer.proposed_name or "(none proposed)",
                             summary=answer.summary or "(none)", kind=answer.kind or "(none)",
                             details=answer.details or "(none)", conventions=listed,
                             placeholder=context.target.name)
    return f"{task}\n\n{context_text(context)}"


# ── asking the models ──────────────────────────────────────────────────

def ask_investigate(model: ModelSpec, context: DataContext, run_dir: Path, run_id: str,
                    conventions: Conventions, cancelled: threading.Event | None = None) -> DataProposal:
    """The investigating model's answer for one label, or the problem that left none. An unusable
    reply (no JSON object in it) is asked once more, with a short follow-up; the retry is counted.
    A request whose pass was abandoned around it (CANCELLED set) is not cached."""
    target = context.target
    if (INVESTIGATE_DATA / run_id / f"{target.name}.yaml").exists():
        return DataProposal(name=target.name, address=target.address, answer=None, tokens=0,
                            seconds=0.0, problem=ToolMessage("already finished"), finished=True)
    earlier = cached(run_dir / "proposals", target.name, DataProposal)
    if earlier is not None:
        return earlier
    started = time.monotonic()
    prompt = prompt_investigate(context, conventions)
    tokens, retries, answer, problem = 0, 0, None, ToolMessage("")
    for attempt in range(2):
        tag = f"{run_id} investigate-data {target.name} {attempt}"
        reply, problem = ask(model, prompt, INVESTIGATE_SYSTEM, tag)
        tokens += logged_tokens(USAGE_LOG, tag)
        if problem is None:
            answer, problem = parse_answer(reply, target.name)
        if problem is None:
            break
        retries = attempt + 1
        prompt = Prompt(f"{prompt}\n\nYour previous reply could not be used: {problem}\n"
                        f"Reply again with only the JSON object, nothing else.")
    proposal = DataProposal(name=target.name, address=target.address, answer=answer, tokens=tokens,
                            seconds=round(time.monotonic() - started, 1), retries=retries, problem=problem)
    if cancelled is None or not cancelled.is_set():
        keep(run_dir / "proposals", target.name, proposal)
    return proposal


def ask_check(model: ModelSpec, context: DataContext, proposal: DataProposal, run_dir: Path,
              run_id: str, conventions: Conventions,
              cancelled: threading.Event | None = None) -> Verdict:
    """The cross-check model's verdict on one proposal; a proposal without a usable name is not
    asked. An unusable reply is asked once more with a short follow-up; the retry is counted."""
    earlier = cached(run_dir / "checkers", proposal.name, Verdict)
    if earlier is not None:
        return earlier
    if proposal.answer is None or proposal.answer.proposed_name is None:
        return Verdict(name=proposal.name, address=proposal.address, reply=None, tokens=0, seconds=0.0,
                       problem=ToolMessage("no proposal to check"))
    started = time.monotonic()
    prompt = prompt_check(context, proposal, conventions)
    tokens, retries, parsed, problem = 0, 0, None, ToolMessage("")
    for attempt in range(2):
        tag = f"{run_id} check-data {proposal.name} {attempt}"
        reply, problem = ask(model, prompt, CHECK_SYSTEM, tag)
        tokens += logged_tokens(USAGE_LOG, tag)
        if problem is None:
            parsed, problem = parse_check(reply, context.target.name)
        if problem is None:
            break
        retries = attempt + 1
        prompt = Prompt(f"{prompt}\n\nYour previous reply could not be used: {problem}\n"
                        f"Reply again with only the JSON object, nothing else.")
    verdict = Verdict(name=proposal.name, address=proposal.address, reply=parsed, tokens=tokens,
                      seconds=round(time.monotonic() - started, 1), retries=retries, problem=problem)
    if cancelled is None or not cancelled.is_set():
        keep(run_dir / "checkers", proposal.name, verdict)
    return verdict


def investigate_all(model: ModelSpec, contexts: tuple[DataContext, ...], run_dir: Path, run_id: str,
                    jobs: int, conventions: Conventions) -> tuple[DataProposal, ...]:
    """Every context investigated, several at a time; a 402 (out of credits) or a daily rate limit
    stops the asking and the rest are reported as skipped, everything done kept."""
    stop = threading.Event()
    cancelled = threading.Event()

    def one(context: DataContext) -> DataProposal:
        if stop.is_set():
            return DataProposal(name=context.target.name, address=context.target.address, answer=None,
                                tokens=0, seconds=0.0,
                                problem=ToolMessage("skipped: the model is out of credits"))
        proposal = ask_investigate(model, context, run_dir, run_id, conventions, cancelled)
        if out_of_credits(proposal.problem):
            stop.set()
        return proposal

    def stub(context: DataContext, why: str) -> DataProposal:
        return DataProposal(name=context.target.name, address=context.target.address, answer=None,
                            tokens=0, seconds=0.0, problem=ToolMessage(why))

    return investigate._run_pass(contexts, jobs, one, stub, cancelled)


def check_all(model: ModelSpec, contexts: tuple[DataContext, ...],
              proposals: tuple[DataProposal, ...], run_dir: Path, run_id: str, jobs: int,
              conventions: Conventions) -> tuple[Verdict, ...]:
    """Every proposal cross-checked, several at a time, the same 402 and straggler rules. The
    checker is the same model as the proposer by default, but it never sees the first reasoning:
    only the context and the finished proposal."""
    by_name = {context.target.name: context for context in contexts}
    stop = threading.Event()
    cancelled = threading.Event()

    def one(proposal: DataProposal) -> Verdict:
        context = by_name[proposal.name]
        if stop.is_set():
            return Verdict(name=proposal.name, address=proposal.address, reply=None, tokens=0,
                           seconds=0.0, problem=ToolMessage("skipped: the model is out of credits"))
        verdict = ask_check(model, context, proposal, run_dir, run_id, conventions, cancelled)
        if out_of_credits(verdict.problem):
            stop.set()
        return verdict

    def stub(proposal: DataProposal, why: str) -> Verdict:
        return Verdict(name=proposal.name, address=proposal.address, reply=None, tokens=0, seconds=0.0,
                       problem=ToolMessage(why))

    return investigate._run_pass(proposals, jobs, one, stub, cancelled)


def status_of(proposal: DataProposal, verdict: Verdict) -> SymbolStatus:
    """What the candidate may claim: probable when the checker agreed, hypothesis otherwise (a
    proposal without a name stays a hypothesis with its open question; symbols/db/README.md)."""
    if proposal.answer is None or proposal.answer.proposed_name is None:
        return SymbolStatus.HYPOTHESIS
    return SymbolStatus.PROBABLE if verdict.reply is not None and verdict.reply.verdict == "agree" \
        else SymbolStatus.HYPOTHESIS


# ── reading the answers ────────────────────────────────────────────────

def elf_symbols(wanted: frozenset[str]) -> dict[str, int]:
    """bn6f.elf's symbols among WANTED (name -> address), the way tools/symdb.py reads them."""
    return investigate.elf_symbols(wanted)

def parse_answer(reply: str, placeholder: str) -> tuple[Answer | None, ToolMessage | None]:
    """The investigating model's answer, or the problem that left none (never guessed into shape).
    A proposed name the name rules refuse is treated as no name: the answer stays (its summary,
    its evidence), and the refusal lands in open_questions, so the label is still documented."""
    parsed = investigate._json_object(reply)
    if parsed is None:
        return None, ToolMessage("the reply held no JSON object")
    name = parsed.get("proposed_name")
    if name is not None and not isinstance(name, str):
        return None, ToolMessage(f"proposed_name {name!r} is not a name or null")
    score = parsed.get("self_score")
    if isinstance(score, bool) or not isinstance(score, (int, float)) or not 0 <= score <= 5:
        score = None
    summary = investigate._text(parsed.get("summary"))
    questions = investigate._texts(parsed.get("open_questions"))
    if isinstance(name, str):
        why = name_problem(name, placeholder)
        if why is not None:
            questions += (ToolMessage(f"the proposed name {name!r} was rejected: {why}"),)
            name = None
    if name is not None and summary is None:
        return None, ToolMessage("a proposed name without a summary")
    evidence = investigate._texts(parsed.get("evidence"))
    if name is not None and not evidence:
        return None, ToolMessage("a proposed name without evidence")
    return Answer(proposed_name=SymbolName(name) if isinstance(name, str) else None,
                  summary=summary, kind=investigate._text(parsed.get("type")),
                  details=investigate._text(parsed.get("details")), evidence=evidence,
                  self_score=int(score) if score is not None else None,
                  open_questions=questions), None


def parse_check(reply: str, placeholder: str) -> tuple[CheckReply | None, ToolMessage | None]:
    """The cross-check model's verdict, or the problem that left none. A revised name has to pass
    the same name rules as a proposal."""
    parsed = investigate._json_object(reply)
    if parsed is None:
        return None, ToolMessage("the reply held no JSON object")
    verdict = parsed.get("verdict")
    if verdict not in ("agree", "revise", "disagree"):
        return None, ToolMessage(f"verdict {verdict!r} is not agree, revise or disagree")
    reason = investigate._text(parsed.get("reason"))
    if reason is None:
        return None, ToolMessage("a verdict without a reason")
    name = parsed.get("revised_name")
    return CheckReply(verdict=cast(CheckVerdict, verdict), reason=reason,
                      revised_name=SymbolName(name) if isinstance(name, str) and name.strip()
                      and name_problem(name, placeholder) is None else None,
                      revised_summary=investigate._text(parsed.get("revised_summary"))), None


# ── the candidates ─────────────────────────────────────────────────────

TODAY = investigate.TODAY
METHOD = "caller-analysis"       # the meaning came from how the code uses the label
KIND = "data"


def make_candidate(context: DataContext, proposal: DataProposal, verdict: Verdict, model: ModelSpec,
                   checker: ModelSpec, run_id: str) -> DataCandidate:
    """The symbols/db-format candidate entry for one label, ready to write and check. Only the
    database's keys: the proposed name opens the summary ("Proposed name: X. ...") and the
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
    if answer is not None and answer.kind:
        summary = ToolMessage(f"{summary} ({answer.kind})")
    refs = [context.target.source] if context.target.source else []
    refs += [f"{site.file}:{site.line}" for site in context.sites[:4]]
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
    return DataCandidate(name=proposal.name, address=context.target.address, kind=KIND,
                         status=status_of(proposal, verdict), summary=summary,
                         details=answer.details if answer is not None else None,
                         evidence=(EvidenceItem(method=METHOD, date=TODAY, detail=ToolMessage(detail),
                                                refs=tuple(refs),
                                                reproduce=ToolMessage(f"grep -n -w {context.target.name} "
                                                                      f"{context.target.source.split(':')[0]}"),
                                                result=ToolMessage(result)),),
                         open_questions=tuple(questions),
                         sources=(f"wf investigate-data {run_id}", "models"))


def write_candidate(candidate: DataCandidate, run_dir: Path) -> Path:
    """The candidate as NAME.yaml beside the run's report, in the database's key order (the address
    a hex string, as symbols/db/README.md writes it)."""
    import yaml  # noqa: PLC0415  (only the writer needs it)

    run_dir.mkdir(parents=True, exist_ok=True)
    path = run_dir / f"{candidate.name}.yaml"
    entry = candidate.model_dump(mode="json", exclude_none=True)
    entry["address"] = candidate.address_as_db
    path.write_text(yaml.safe_dump(entry, sort_keys=False, allow_unicode=True))
    return path


def report_rows(pairs: tuple[tuple[DataProposal, Verdict], ...],
                guarded: dict[SymbolName, ToolMessage] | None = None) -> str:
    """The run's report.tsv: address, old name, proposed name, status, self score, verdict, cost."""
    lines = ["# address\told\tproposed\tstatus\tself_score\tcross_check\ttokens\tretries\tnote"]
    for proposal, verdict in pairs:
        answer = proposal.answer
        proposed = (verdict.reply.revised_name if verdict.reply is not None
                    and verdict.reply.verdict == "revise" and verdict.reply.revised_name else None) \
            or (answer.proposed_name if answer else None) or "-"
        status = status_of(proposal, verdict).value
        score = answer.self_score if answer else None
        check = verdict.reply.verdict if verdict.reply else "none"
        retries = proposal.retries + verdict.retries
        note = (guarded or {}).get(proposal.name, "-")
        lines.append(f"0x{proposal.address:08X}\t{proposal.name}\t{proposed}\t{status}\t"
                     f"{score if score is not None else '-'}\t{check}\t{proposal.tokens + verdict.tokens}"
                     f"\t{retries}\t{note}")
    return "\n".join(lines) + "\n"