#!/usr/bin/env python3
"""Land wf investigate-data's finished candidates into symbols/db and bn6f.

  tools/investigate_data_land.py RUN_DIR [--count N] [--dry-run] [--selfcheck]
  tools/investigate_data_land.py style-fix [--root REPO]
  tools/investigate_data_land.py dedupe [--root REPO] [--limit N]
                                         [--model PROPOSER] [--model-checker CHECKER]
  tools/investigate_data_land.py reconcile [--root REPO] [--limit N]
                                            [--model PROPOSER] [--model-checker CHECKER]
  tools/investigate_data_land.py land-rescued [--root REPO] [--limit N]
                                              [--batch N] [--dry-run]

The sibling to tools/investigate_land.py, retargeted at data labels. The four
subcommands rescue candidates earlier runs' land runs dropped (mirrors of the
function sibling's rescue lists):

  style-fix: the naming-style rejects (no model). bn6f's data names are
    lowerCamel, PascalCase, or `e<Struct>_<Field>`/`o<Struct>_<Field>` (the upper-case
    prefix `_` pattern the project uses): the data style rule accepts what data
    already uses and rejects what code already rejected (a digit, a hedge, a 0x
    literal, a 0xAABBCCDD digit run copied from the label's address). What it
    rescues goes to build/investigate_data/rescue/style.tsv.
  dedupe: the candidates whose proposed name collides with another label in the run,
    with a name already landed in symbols/db, or with a bn6f.elf data symbol. One
    model call per group gives every member a distinct name; cached under
    build/investigate_data/dedupe/ (resumable).
  reconcile: the candidates the cross-check revised with a `revised_name`. The
    label is still a placeholder; the revised name passes the relaxed style rule,
    copies no part of the label's address, and is unique across all the runs'
    revise candidates. Cached under build/investigate_data/reconcile/NAME.json.
  land-rescued: writes the rescued candidates' symbols/db entries the way the
    land run does (under each candidate's current placeholder name, then the
    record workflow renames them to NEWNAME.yaml). Reads from
    build/investigate_data/rescue/* and runs ./wf record --rename OLD NEW in
    batches.

Selection (from report.tsv): status probable AND cross-check agree. The same
rejects as investigate_land's selection, adapted for data, plus two
data-specific guards:

  - a name proposed for more than one label in the run;
  - it already exists as a data symbol of bn6f/bn6f.elf or as a symbols/db
    name, or as a cport identifier (checked here even though wf investigate-data
    also checks at run time), or as an asm include filename under bn6f/asm or
    bn6f/data;
  - it breaks the naming style (no Maybe, no 0x literal, no digit run copied
    from the label's address; plain names are lowerCamel or PascalCase;
    `prefix_name` is allowed when the prefix is lower-case or already used by
    bn6f);
  - the label already has a symbols/db entry (skipped, not dropped);
  - the run folder has no candidate YAML for it.

The two data-specific guards:
  - offset_expression: a label that appears inside an arithmetic expression in
    bn6f (`byte_203CA74 - 0x203ca70` style comments and `.set` arithmetic).
    A whole-word rename is mechanically fine, but the operator should review
    the offset expression's meaning before the rename goes in, so the label
    is listed in land_skipped.tsv and not auto-landed.
  - port_a_reference: `A_<old>` references in cport/. The record workflow
    handles `A_` and `L_` renames (tools/rename_symbol.py recomp_files
    prefixes); the dry-run greps cport/ to show how many of these `A_<name>`
    references the rename would touch (none for the run, all for the next
    batch).

Before the record run every written entry is validated with tools/symdb.py's
own check, then ONE ./wf record --symbol OLD ... --rename OLD NEW ...
--message-file F per batch (reusing run_record; the Claude attribution
trailer is kept as the function landing uses it).

On a record failure or a changed ROM the tool stops and says the error; it
never retries.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from collections.abc import Iterable, Sequence
from pathlib import Path
from typing import Any, NamedTuple

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, "tools")
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)
import gamepaths   # noqa: E402   (tools/gamepaths.py: the game profile)
DB = os.path.join(ROOT, "symbols", "db")
ELF = gamepaths.ORIG_ELF
WF = os.path.join(ROOT, "wf")
BN6F_DIR = gamepaths.BN6F_DIR


def use_root(repo: str) -> None:
    """Work against another checkout (a worktree's tool runs the main tree's data). Paths and the
    modules already imported from this tree's tools are redone against REPO's."""
    global ROOT, TOOLS, DB, ELF, WF, BN6F_DIR
    ROOT = os.path.abspath(repo)
    TOOLS = os.path.join(ROOT, "tools")
    DB = os.path.join(ROOT, "symbols", "db")
    WF = os.path.join(ROOT, "wf")
    sys.path.insert(0, TOOLS)
    sys.modules.pop("gamepaths", None)
    import gamepaths   # noqa: PLC0415  (the checkout --root names)
    globals()["gamepaths"] = gamepaths
    ELF = gamepaths.ORIG_ELF
    BN6F_DIR = gamepaths.BN6F_DIR
    global _OFFSET_FILES, _PORT_IDS, _OFFSET_GLOBS
    _OFFSET_FILES = None       # paths changed: scan afresh on the next call
    _PORT_IDS = None

# bn6f source files a label's offset expression may live in. The data label's
# rename is mechanically fine even when its name is part of an offset, but the
# operator gets to review the offset expression before the rename goes in.
_OFFSET_GLOBS = ("bn6f/asm/*.s", "bn6f/data/*.s", "bn6f/include/*.inc",
                  "bn6f/include/*.h", "bn6f/include/rom_structs/*.inc",
                  "bn6f/include/structs/*.inc", "bn6f/charmap.inc")
_OFFSET_PATTERN = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\s*[+\-]\s*0x[0-9a-fA-F]+|\b[A-Za-z_][A-Za-z0-9_]*\s*[+\-]\s*[\d$]")
PORT_GLOB = ("cport/src/game/*.c", "cport/src/*.c", "cport/host/*.c", "cport/include/*.h")
_PORT_TOKEN = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
ASM_INCLUDE_DIRS = ("bn6f/asm", "bn6f/data")
PLACEHOLDER_PREFIXES = ("byte_", "unk_", "word_", "dword_", "hword_",
                         "off_", "loc_", "obj_", "jumptable_")
HEX_LITERAL = re.compile(r"0x[0-9a-fA-F]")
TRAILING_ADDRESS = re.compile(r"_?[0-9a-fA-F]{7,}$")
DIGIT_RUN = re.compile(r"[0-9a-fA-F]{3,}")
ANY_DIGIT = re.compile(r"[0-9]")
ACRONYM = re.compile(r"[A-Z]{2,}")
GENERIC_PLACEHOLDER = re.compile(r"^(byte|unk|word|dword|hword|off)_[0-9A-Fa-f]{6,8}$")


def data_placeholder(name: str) -> bool:
    """A name that says nothing yet (the investigate-data workflow's rule)."""
    return any(name.startswith(prefix) for prefix in PLACEHOLDER_PREFIXES) \
        or GENERIC_PLACEHOLDER.fullmatch(name) is not None


# ── the report and the candidates ──────────────────────────────────────

class Row:
    """One report.tsv line (without the '#' header)."""

    def __init__(self, line: str) -> None:
        fields = (line.split("\t") + [""] * 9)[:9]
        (self.address, self.old, self.proposed, self.status, self.score,
         self.check, self.tokens, self.retries, self.note) = (f.strip() for f in fields)

    def score_int(self) -> int:
        try:
            return int(self.score)
        except ValueError:
            return 0


def read_report(run_dir: str) -> list[Row]:
    path = os.path.join(run_dir, "report.tsv")
    rows = []
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        rows.append(Row(line))
    return rows


def load_candidate(run_dir: str, old: str) -> dict[str, Any] | None:
    path = os.path.join(run_dir, f"{old}.yaml")
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8") as f:
        loaded = yaml.safe_load(f)
    return loaded if isinstance(loaded, dict) else None


def write_skipped(skipped: list[tuple[str, str, str, str]], run_dir: str) -> None:
    """The landed run's land_skipped.tsv: address, old, proposed, reason — the same column order
    the function landing uses."""
    path = os.path.join(run_dir, "land_skipped.tsv")
    with open(path, "w", encoding="utf-8") as f:
        f.write("# address\told\tproposed\treason\n")
        for address, old, proposed, reason in skipped:
            f.write(f"{address}\t{old}\t{proposed}\t{reason}\n")


# ── the naming rules ───────────────────────────────────────────────────

def style_problem(name: str, allowed: frozenset[str] = frozenset()) -> str | None:
    """Why NAME breaks the data naming style (the rule investigate_data.py's name_problem and
    name_conventions imply), or None.

    The data shapes bn6f already uses (counted from its symbols): `prefix_name` (lowerCamel,
    7879 symbols in symbols/db), `o<Struct>_<Field>` (318), `CamelCase` (198), `e<Struct>_<Field>`
    (37), `s/i<Name>` (7). The rule:
      - no Maybe hedge, no 0x literal, no trailing 7+ digit run (the function landing's rule);
      - one underscore, the prefix lower-case OR already in ALLOWED (e<Struct>, o<Struct>,
        s<Name>, i<Name>, or any prefix bn6f already spells with an upper-case letter);
      - the prefix's tail is lowerCamel OR PascalCase (data's `e<Struct>_<Field>` shape
        often has a PascalCase tail: `eCutsceneState_CutsceneParam32`,
        `eOWPlayerObject_NextX`, `eLinkState_ResetForNewUse_803DE88`);
      - plain names are lowerCamel or PascalCase (data's CamelCase, 198 symbols).
    The function landing's rule (lowerCamel-only) drops data's PascalCase names. ALLOWED is
    the prefixes the data names bn6f already uses spell with an upper-case letter:
    prefix_in_use computes it from the ELF and symbols/db."""
    if "maybe" in name.lower():
        return "it hedges (Maybe)"
    if HEX_LITERAL.search(name):
        return "it carries a 0x literal"
    if TRAILING_ADDRESS.search(name):
        return "it ends in a digit run copied from the placeholder's address"
    if "_" in name:
        prefix, _, rest = name.partition("_")
        if prefix not in allowed and not re.fullmatch(r"[a-z][a-z0-9]*", prefix):
            return f"the prefix {prefix!r} is not lower-case"
        if "_" in rest:
            return "more than one underscore"
        if not rest or not (
                re.fullmatch(r"[a-z][A-Za-z0-9]*", rest)        # lowerCamel
                or re.fullmatch(r"[A-Z][A-Za-z0-9]+", rest)):   # PascalCase
            return (f"{rest!r} after the prefix is not lowerCamel or PascalCase")
    elif not (re.fullmatch(r"[a-z][A-Za-z0-9]*", name)
              or re.fullmatch(r"[A-Z][A-Za-z0-9]+", name)):
        return "it is not lowerCamel or PascalCase"
    return None


def address_digit_problem(name: str, address: str) -> str | None:
    """Why NAME carries digits copied from its ADDRESS (0x-prefixed hex), or None.

    The data labels' address digits are what the placeholder embeds (byte_203F5A4 -> 0x0203F5A4);
    any 3+ digit run in NAME that has a 3+ substring in the address digits is suspect — the
    investigate-data run's name_problem already enforces the same rule, but the land run
    re-checks because the model can re-propose after the run was written."""
    hexdigits = address.removeprefix("0x").removeprefix("0X").lstrip("0").upper() or "0"
    for run in DIGIT_RUN.findall(name):
        for width in range(len(run), 2, -1):
            piece = next((run[i:i + width].upper() for i in range(len(run) - width + 1)
                          if run[i:i + width].upper() in hexdigits), None)
            if piece is not None:
                return f"the digit run {piece} is copied from the address {address}"
    return None


def prefixes_in_use(names: Iterable[str]) -> frozenset[str]:
    """The prefixes bn6f already spells with an upper-case letter (the function landing's rule):
    a proposed name may keep such a prefix, because the project already uses it. A name's PREFIX
    (everything before its first underscore) counts when some symbol or database name starts with
    `PREFIX_`. For data this lifts the `<Struct>` of `e<Struct>_<Field>` and `o<Struct>_<Field>`,
    and the `<Name>` of `i<Name>` / `s<Name>`."""
    used: set[str] = set()
    for name in names:
        prefix, sep, _ = name.partition("_")
        if sep and any(c.isupper() for c in prefix):
            used.add(prefix)
    return frozenset(used)


def _join_camel(parts: Sequence[str], lead_lower: bool = False) -> str:
    """PARTS as one lowerCamel word, an ALL-CAPS part folded to Uni — and, with LEAD_LOWER, the
    first part lower-case, because a prefix's rest may not start upper."""
    out: list[str] = []
    for index, part in enumerate(parts):
        if index == 0:
            if lead_lower:
                out.append(part.lower() if ACRONYM.fullmatch(part) else part[:1].lower() + part[1:])
            else:
                out.append(part)
        elif part.isalpha() and part.isupper() and len(part) > 1:
            out.append(part.capitalize())
        elif part[:1].isalpha():
            out.append(part[0].upper() + part[1:])
        else:
            out.append(part)
    return "".join(out)


def style_fixed(name: str, allowed: frozenset[str] = frozenset()) -> str | None:
    """NAME with its style break fixed mechanically (no model): a second underscore's parts joined
    lowerCamel, an upper-case start lowered. Returns None when nothing mechanical fits.

    A name that already passes the relaxed data style rule is returned as-is: the rule already
    accepts PascalCase tails (e<Struct>_<Field> shape), so a fix would lower them for nothing."""
    if "maybe" in name.lower() or HEX_LITERAL.search(name):
        return None
    if style_problem(name, allowed) is None:
        return name                        # already valid: nothing to fix
    parts = name.split("_")
    if len(parts) == 1:
        # Plain PascalCase -> lowerCamel (`ChatboxPalette` -> `chatboxPalette`); lowercase stays.
        if re.fullmatch(r"[a-z][A-Za-z0-9]*", parts[0]):
            return parts[0]
        if re.fullmatch(r"[A-Za-z][A-Za-z0-9]*", parts[0]):
            return parts[0][:1].lower() + parts[0][1:]
        return None
    prefix = parts[0]
    if prefix not in allowed and not re.fullmatch(r"[a-z][a-z0-9]*", prefix):
        return _join_camel(parts)
    tail = _join_camel(parts[1:], lead_lower=True)
    if tail[:1].isdigit():
        return _join_camel(parts)
    return f"{prefix}_{tail}"


# ── bn6f.elf, cport and asm-include guards ─────────────────────────────

def elf_symbols() -> dict[str, int]:
    """bn6f.elf's symbols (name -> address, Thumb bit ignored)."""
    out = subprocess.run(["arm-none-eabi-nm", ELF], capture_output=True, text=True, check=True).stdout
    syms: dict[str, int] = {}
    for line in out.splitlines():
        fields = line.split()
        if len(fields) == 3:
            syms.setdefault(fields[2], int(fields[0], 16) & ~1)
    return syms


def db_names() -> set[str]:
    return {f[:-5] for f in os.listdir(DB) if f.endswith(".yaml")}


def asm_include(name: str) -> bool:
    """Is NAME also the name of an asm include file? A rename to it would rewrite the include's
    path instead of the file (the same guard the function landing uses)."""
    return any((Path(ROOT) / sub / f"{name}.s").exists() for sub in ASM_INCLUDE_DIRS)


_PORT_IDS: frozenset[str] | None = None


def port_identifiers() -> frozenset[str]:
    """Every identifier the port's own C uses (cport/src, cport/host, cport/include). The
    function landing caches this; the data landing reads the same C (the same grep behaviour
    the wf investigate-data run uses at run time). A data name that collides with a port
    identifier would break the port build when the rename lands."""
    global _PORT_IDS
    if _PORT_IDS is None:
        found: set[str] = set()
        for pattern in PORT_GLOB:
            for path in sorted(Path(ROOT).glob(pattern)):
                if path.name == "lifted.h":
                    continue
                found.update(_PORT_TOKEN.findall(path.read_text(errors="replace")))
        _PORT_IDS = frozenset(found)
    return _PORT_IDS


def port_a_refs(name: str) -> int:
    """How many lines in cport reference NAME as `A_<name>`. The record workflow's rename
    updates these (tools/rename_symbol.py recomp_files prefixes `A_`/`L_`), so we grep
    cport/ to show how many of them a rename touches (a dry-run diagnostic: a real dry
    build is several thousand `A_<name>` references; a count of N tells the operator the
    scope of the change)."""
    counts = 0
    needle = re.compile(rf"\bA_{re.escape(name)}\b")
    for pattern in PORT_GLOB:
        for path in sorted(Path(ROOT).glob(pattern)):
            counts += sum(1 for line in path.read_text(errors="replace").splitlines() if needle.search(line))
    return counts


# ── the offset-expression guard (data-specific) ────────────────────────

_OFFSET_FILES: tuple[Path, ...] | None = None


def _offset_files() -> tuple[Path, ...]:
    """The bn6f source files a label's offset expression may live in, cached."""
    global _OFFSET_FILES
    if _OFFSET_FILES is None:
        out: set[Path] = set()
        for pattern in _OFFSET_GLOBS:
            for path in Path(ROOT).glob(pattern):
                if path.is_file():
                    out.add(path)
        _OFFSET_FILES = tuple(sorted(out))
    return _OFFSET_FILES


def offset_expression_sites(name: str) -> list[tuple[str, int, str]]:
    r"""The bn6f lines where NAME appears in an offset expression like `byte_203CA74 - 0x203ca70`
    or `.set foo, byte_203CA74 + 4`. The operator should review each one before the rename
    goes in: whole-word renames are mechanically fine, but the offset expression's meaning
    may change in ways only the human eye catches. Returns (file, line, content) triples.

    The detector looks for the placeholder in a line where it sits inside an arithmetic
    expression: `name [+-] (literal)`. The line may end with the literal; only the part
    before the `//` comment marker is scanned (the comment does not change the asm the
    record run renames — the rename is whole-word on the source line, including comments)."""
    if not name or not name.replace("_", "").replace("-", "").isalnum():
        return []                          # an unscannable name can never match
    needle = re.compile(rf"\b{re.escape(name)}\b")
    out: list[tuple[str, int, str]] = []
    for path in _offset_files():
        try:
            rel = str(path.relative_to(ROOT))
        except ValueError:
            rel = str(path)               # a test temp dir: not under ROOT, use the abs path
        for lineno, text in enumerate(path.read_text(errors="replace").splitlines(), 1):
            if not needle.search(text):
                continue
            for run in needle.finditer(text):
                after = text[run.end():run.end() + 32]
                if _OFFSET_PATTERN.match(name + after):
                    out.append((rel, lineno, text.rstrip()))
                    break
    return out


def offset_expression_problem(name: str) -> str | None:
    """The skipped reason when the data label appears in an offset expression, or None when the
    operator's review is unnecessary. Reports one site — the first hit, so the skipped file
    points the operator at the offset expression to look at first."""
    sites = offset_expression_sites(name)
    if not sites:
        return None
    file, line, content = sites[0]
    return f"appears in an offset expression ({file}:{line}); needs manual review"


# ── the symbols/db names for the selection ─────────────────────────────

SYMBOLS_DB_HEADER = ("# run_id\taddress\told\tproposed\treason\n")


# ── selection ──────────────────────────────────────────────────────────

def select(rows: list[Row], run_dir: str, syms: dict[str, int], port: frozenset[str],
           count: int, allowed: frozenset[str] = frozenset()) \
        -> tuple[list[tuple[Row, dict[str, Any]]], list[tuple[str, str, str, str]]]:
    """The candidates to land and the dropped/skipped ones with their reasons.

    Returns ((row, candidate), ...) in landing order (self_score desc, then address asc) and
    (address, old, proposed, reason) for every probable+agree row not landed. ALLOWED is the
    prefixes bn6f already spells with upper-case letters (style_problem's exemption). PORT is
    the cport identifiers (a name already used in the port's C is taken, listed in skipped)."""
    counts: dict[str, int] = {}
    for row in rows:
        if row.proposed not in ("-", ""):
            counts[row.proposed] = counts.get(row.proposed, 0) + 1
    names = db_names()
    eligible: list[tuple[Row, dict[str, Any]]] = []
    skipped: list[tuple[str, str, str, str]] = []
    for row in rows:
        if row.status != "probable" or row.check != "agree" or row.proposed in ("-", ""):
            continue
        candidate = load_candidate(run_dir, row.old)
        if candidate is None:
            skipped.append((row.address, row.old, row.proposed, "no candidate yaml in the run folder"))
            continue
        if row.old in names:
            skipped.append((row.address, row.old, row.proposed,
                            "the label already has a symbols/db entry"))
            continue
        if asm_include(row.proposed):
            skipped.append((row.address, row.old, row.proposed,
                            "its name is also an asm include file: the rename would rewrite the "
                            "include path, not the file"))
            continue
        if counts[row.proposed] > 1:
            skipped.append((row.address, row.old, row.proposed,
                            f"proposed for {counts[row.proposed]} labels in the run"))
            continue
        if row.proposed in syms:
            skipped.append((row.address, row.old, row.proposed,
                            "the name is already a symbol of bn6f.elf"))
            continue
        if row.proposed in names:
            skipped.append((row.address, row.old, row.proposed,
                            "the name is already a symbols/db entry name"))
            continue
        if row.proposed in port:
            skipped.append((row.address, row.old, row.proposed,
                            "the name is already an identifier in cport/ (grep -w): the rename "
                            "would collide with it"))
            continue
        # the data-specific guard: a label used in an offset expression in bn6f is taken out
        # of the auto-land; the operator reviews the offset expression first, then the
        # land runs again with the manual landing.
        if (offset := offset_expression_problem(row.old)) is not None:
            skipped.append((row.address, row.old, row.proposed, offset))
            continue
        problem = style_problem(row.proposed, allowed)
        if problem is None:
            problem = address_digit_problem(row.proposed, row.address)
        if problem is not None:
            skipped.append((row.address, row.old, row.proposed, f"naming style: {problem}"))
            continue
        eligible.append((row, candidate))
    eligible.sort(key=lambda pair: (-pair[0].score_int(), int(pair[0].address, 16)))
    return eligible[:count], skipped


# ── the symbols/db entry ───────────────────────────────────────────────

def entry_of(candidate: dict[str, Any], run_id: str, rescue: str | None = None) -> dict[str, Any]:
    """The symbols/db entry for a candidate, in the README's key order.

    Data labels get `kind: data`. The record run's rename turns OLD.yaml into NEWNAME.yaml
    with previous_names [old placeholder]. RESCUE, when given, is named in the first evidence
    item's result (how the candidate's name was rescued after the run had dropped it)."""
    summary = str(candidate.get("summary") or "")
    evidence = []
    for index, item in enumerate(candidate.get("evidence") or []):
        item = dict(item)
        result = str(item.get("result") or "")
        if f"wf investigate-data {run_id}" not in result:
            item["result"] = f"{result}; run wf investigate-data {run_id}"
        if rescue and index == 0:
            item["result"] = f"{item['result']}; {rescue}"
        evidence.append(item)
    entry: dict[str, Any] = {"name": candidate["name"], "address": candidate["address"],
                             "kind": "data", "status": "probable",
                             "summary": summary}
    if candidate.get("region"):
        entry["region"] = candidate["region"]
    if candidate.get("size") is not None:
        entry["size"] = candidate["size"]
    if candidate.get("interactions"):
        entry["interactions"] = candidate["interactions"]
    if candidate.get("interface"):
        entry["interface"] = candidate["interface"]
    if candidate.get("details"):
        entry["details"] = candidate["details"]
    entry["evidence"] = evidence
    if candidate.get("open_questions"):
        entry["open_questions"] = candidate["open_questions"]
    entry["sources"] = [f"wf investigate-data {run_id}"]
    return entry


def write_entry(candidate: dict[str, Any], run_id: str, rescue: str | None = None) -> str:
    """Write the entry as symbols/db/OLD.yaml (the current placeholder name); its path."""
    entry = entry_of(candidate, run_id, rescue)
    path = os.path.join(DB, f"{entry['name']}.yaml")
    with open(path, "w", encoding="utf-8") as f:
        yaml.safe_dump(entry, f, sort_keys=False, allow_unicode=True, width=100)
    return path


def symdb_entry_problems(path: str) -> list[str]:
    """What tools/symdb.py's own check says about one written entry."""
    sys.path.insert(0, TOOLS)
    import symdb   # noqa: PLC0415  (the repo's checker, cached)
    where = os.path.relpath(path, ROOT)
    loaded = symdb.load_full([where])[0]
    if loaded.get("_unparsed"):
        return ["not a YAML mapping"]
    syms = symdb.elf_symbols(symdb.Cache(), {loaded.get("name")})
    entry = symdb.slim(loaded)
    entry["_file"] = where
    return [f"{problem.message}" for problem in symdb.problems([entry], syms, symdb.Tree())]


def symdb_check_all() -> tuple[bool, list[str]]:
    """The whole database's check (the record run runs it again)."""
    done = subprocess.run([sys.executable, os.path.join(TOOLS, "symdb.py"), "check"],
                          capture_output=True, text=True, cwd=ROOT)
    problems = [line for line in done.stdout.splitlines() if ": " in line and not line.endswith(" problems")]
    return done.returncode == 0 and not problems, problems


# ── the record run ─────────────────────────────────────────────────────

def run_record(pairs: list[tuple[str, str]], found_by: str, run_dir: str, log_path: str) -> dict[str, Any]:
    """`./wf record --symbol OLD ... --rename OLD NEW ... --message-file FILE`, once. The record
    workflow handles `A_<old>` and `L_<old>` references in cport/ (tools/rename_symbol.py
    recomp_files prefixes); FOUND_BY is the commit message's "found by ..." phrase (e.g.
    `wf investigate-data RUN_ID`, or a rescue's own)."""
    message = (f"symbols: name {len(pairs)} data labels found by {found_by} "
               f"(proposed by MiniMax M3, checked by MiniMax M3)\n"
               f"\n"
               f"Candidates selected from the run's report (status probable, cross-check agree); "
               f"entries written by tools/investigate_data_land.py, checked with tools/symdb.py, "
               f"landed through the record workflow.\n"
               f"\n"
               f"Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>\n"
               f"Claude-Session: https://claude.ai/code/session_019re4QNxn4vzb3ZkL7vj4gs\n")
    message_path = os.path.join(run_dir, "land_message.txt")
    with open(message_path, "w", encoding="utf-8") as f:
        f.write(message)
    argv = [WF, "record"]
    for old, _ in pairs:
        argv += ["--symbol", old]
    for old, new in pairs:
        argv += ["--rename", old, new]
    argv += ["--message-file", message_path]
    with open(log_path, "w", encoding="utf-8") as log:
        done = subprocess.run(argv, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
    text = open(log_path, encoding="utf-8").read()
    if done.returncode != 0:
        sys.exit(f"the record run failed (exit {done.returncode}), see {log_path}:\n"
                 + "\n".join(text.splitlines()[-30:]))
    marker = text.rfind(" finished:\n")
    if marker < 0:
        sys.exit(f"the record run left no report, see {log_path}:\n"
                 + "\n".join(text.splitlines()[-30:]))
    report = json.loads(text[marker + len(" finished:\n"):])["report"]
    if report.get("verdict") != "recorded and committed":
        sys.exit(f"the record run did not record: {report.get('verdict')}\n{json.dumps(report, indent=2)}\n"
                 f"see {log_path}")
    if not report.get("port_rom_unchanged", False):
        sys.exit(f"the port's ROM changed with the renames (the record report says so); "
                 f"the commit went in, but this was not expected here:\n{json.dumps(report, indent=2)}\n"
                 f"see {log_path}")
    return report


# ── the checks afterwards ──────────────────────────────────────────────

def check_afterwards(report: dict[str, Any], log_path: str) -> None:
    """ROM identical, the database clean, the commits in, the wf listing unaffected."""
    ok, problems = symdb_check_all()
    if not ok:
        sys.exit("tools/symdb.py check fails after the record run:\n" + "\n".join(problems))
    print(f"symbols/db check: ok")
    commit = report.get("commit")
    log = subprocess.run(["git", "log", "-1", "--format=oneline"], cwd=ROOT,
                         capture_output=True, text=True).stdout
    if commit and commit not in log:
        sys.exit(f"the record run's commit {commit} is not HEAD ({log.strip()})")
    bn6f = subprocess.run(["git", "log", "-3", "--format=oneline"], cwd=BN6F_DIR,
                          capture_output=True, text=True).stdout
    print(f"recomp commit: {log.strip()}")
    print(f"bn6f log:\n  " + "\n  ".join(bn6f.strip().splitlines()))
    print(f"record log: {log_path}")


# ── the skipped rows of finished runs (for the rescue subcommands) ─────

class SkippedRow(NamedTuple):
    """One land_skipped.tsv line, with the run it came from."""
    run_id: str
    address: str
    old: str
    proposed: str
    reason: str


DUP_REASON = re.compile(r"^proposed for (\d+) labels in the run$")
OFFSET_REASON = re.compile(r"^appears in an offset expression \(([^)]+:\d+)\); needs manual review$")


def finished_run_dirs(base: str | None = None) -> list[str]:
    """The finished investigate-data runs' directories (report.tsv and land_skipped.tsv both
    there; a run the provider refused, *.failed-429, is not finished)."""
    base = base or os.path.join(ROOT, "build", "investigate_data")
    out: list[str] = []
    for name in sorted(os.listdir(base)):
        if not name.startswith("investigate-data-") or name.endswith(".failed-429"):
            continue
        run_dir = os.path.join(base, name)
        if all(os.path.exists(os.path.join(run_dir, f)) for f in ("report.tsv", "land_skipped.tsv")):
            out.append(run_dir)
    return out


def read_skipped(base: str | None = None) -> list[SkippedRow]:
    """Every dropped or skipped candidate of every finished run, runs in name order."""
    rows: list[SkippedRow] = []
    for run_dir in finished_run_dirs(base):
        run_id = os.path.basename(run_dir)
        for line in open(os.path.join(run_dir, "land_skipped.tsv"), encoding="utf-8"):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            fields = (line.split("\t") + [""] * 4)[:4]
            rows.append(SkippedRow(run_id, fields[0], fields[1], fields[2], fields[3]))
    return rows


def dup_groups(rows: Sequence[SkippedRow]) -> dict[str, list[SkippedRow]]:
    """Shared proposed name -> the candidates dropped because of it, across ALL runs. A name
    whose sibling in one run was dropped for another reason (taken in bn6f.elf, no candidate
    yaml) still forms the group."""
    groups: dict[str, list[SkippedRow]] = {}
    for row in rows:
        if DUP_REASON.match(row.reason):
            groups.setdefault(row.proposed, []).append(row)
    return groups


# ── the dry-run printing ───────────────────────────────────────────────

def dry_run_report(chosen: list[tuple[Row, dict[str, Any]]],
                   skipped: list[tuple[str, str, str, str]],
                   run_id: str, scan_port: bool = True) -> None:
    """The dry-run output: the exact pairs in landing order, then skipped with their reasons,
    then a sample of `A_<old>` counts in cport/ for the first 20 (the diagnostic the task
    asks for — the record workflow's rename handles `A_` prefixes; the grep count tells the
    operator what the rename's cport/ scope is)."""
    print(f"candidates probable+agree selected: {len(chosen)} (would land in order below)")
    if chosen:
        print(f"\n{len(chosen)} pairs:")
    for index, (row, _candidate) in enumerate(chosen, 1):
        print(f"  {row.address} {row.old} -> {row.proposed} (score {row.score})")
    print(f"\n{len(skipped)} skipped (would write to land_skipped.tsv, see reasons below):")
    summary: dict[str, int] = {}
    for _addr, _old, _proposed, reason in skipped:
        key = reason.split(":", 1)[0] if ":" in reason else reason
        if reason.startswith("naming style:"):
            key = "naming style"
        summary[key] = summary.get(key, 0) + 1
    for key, count in sorted(summary.items(), key=lambda pair: (-pair[1], pair[0])):
        print(f"  {key}: {count}")
    if skipped:
        for addr, old, proposed, reason in skipped:
            print(f"    {addr} {old} -> {proposed}: {reason}")
    if scan_port:
        sample = chosen[:20]
        if sample:
            print(f"\nA_<old> references in cport/ for the first {len(sample)} pairs (the record "
                  f"workflow's `A_` rename handles these — the count is diagnostic, not blocking):")
        for row, _candidate in sample:
            count = port_a_refs(row.old)
            if count:
                print(f"  {row.old}: {count} A_{row.old} hits in cport/")
        if sample:
            other = sum(1 for row, _ in sample if port_a_refs(row.old) == 0)
            if other:
                print(f"  ({other} of {len(sample)}: no A_<old> references in cport/)")


# ── the main land run ──────────────────────────────────────────────────

def land(run_dir: str, count: int, dry: bool) -> None:
    """Select, write entries under the old placeholder name, then ONE record run per batch."""
    run_id = os.path.basename(os.path.normpath(run_dir))
    rows = read_report(run_dir)
    if not rows:
        sys.exit(f"no rows in {os.path.join(run_dir, 'report.tsv')}")
    syms = elf_symbols()
    port = port_identifiers()
    allowed = prefixes_in_use([*syms, *db_names()])
    chosen, skipped = select(rows, run_dir, syms, port, count, allowed)
    write_skipped(skipped, run_dir)
    print(f"run: {run_id}; rows: {len(rows)}, probable+agree: "
          f"{sum(1 for r in rows if r.status == 'probable' and r.check == 'agree')}; "
          f"selected: {len(chosen)}, skipped: {len(skipped)} "
          f"(land_skipped.tsv written)")
    if dry:
        dry_run_report(chosen, skipped, run_id)
        print("dry run: nothing written to symbols/db, no record run")
        return
    if not chosen:
        sys.exit(f"nothing to land; see {run_dir}/land_skipped.tsv")
    pairs: list[tuple[str, str]] = []
    for row, candidate in chosen:
        path = write_entry(candidate, run_id)
        problems = symdb_entry_problems(path)
        if problems:
            sys.exit(f"{path}: tools/symdb.py rejects the entry:\n  " + "\n  ".join(problems))
        pairs.append((row.old, row.proposed))
        print(f"  {row.address} {row.old} -> {row.proposed} (score {row.score}): entry written and checked")
    ok, problems = symdb_check_all()
    if not ok:
        sys.exit("tools/symdb.py check fails on the database; the record run would abort:\n"
                 + "\n".join(problems))
    log_path = os.path.join(run_dir, "land_record.log")
    report = run_record(pairs, f"wf investigate-data {run_id}", run_dir, log_path)
    print(f"record: {report['verdict']}, commit {report.get('commit')}, "
          f"bn6f renames {report.get('renamed_in_bn6f')}, comments {report.get('comments_in_bn6f')}, "
          f"port ROM unchanged: {report.get('port_rom_unchanged')}")
    check_afterwards(report, log_path)


# ── selfcheck ─────────────────────────────────────────────────────────

def selfcheck(*, root: str | None = None, count: int = 30) -> None:
    """The tool's own logic, checked (no game, no builds, no models, no DB writes).

    When ROOT is given, use a temp checkout (root == the worktree) so the prefix
    computation, port_identifiers and offset_expression_sites have the layout to
    read. With the default ROOT (the tool's own checkout) only the pure in-memory
    checks run: style_problem, address_digit_problem, prefix/skeleton parsing,
    selection logic, candidate yaml construction, and the offset-expression
    regex on synthetic lines. COUNT, when smaller than 30, trims the report.tsv
    fixtures the in-memory selection uses."""
    if root is not None:
        use_root(root)

    import tempfile
    bad: list[str] = []

    def eq(what: str, got: object, want: object) -> None:
        if got != want:
            bad.append(f"{what}: got {got!r}, want {want!r}")

    eq("style: lowerCamel plain", style_problem("battleBestTimes"), None)
    eq("style: PascalCase plain", style_problem("ChatboxPalette"), None)
    eq("style: lowercase plain", style_problem("chatboxBlitGlyph"), None)
    eq("style: BBSMessageCounts (capital-acronym start)", style_problem("BBSMessageCounts"), None)
    eq("style: lower prefix with underscore", style_problem("chatbox_textSlots"), None)
    eq("style: upper prefix, allowed", style_problem("eRFUState_CallbackData",
                                                     frozenset({"eRFUState"})), None)
    eq("style: upper prefix, NOT allowed",
       style_problem("eBattle_TrackedTags"),
       "the prefix 'eBattle' is not lower-case")
    eq("style: PascalCase tail under allowed prefix",
       style_problem("eCutsceneState_CutsceneParam32", frozenset({"eCutsceneState"})), None)
    eq("style: more than one underscore", style_problem("battle_step_commitMoveAndHit"),
       "more than one underscore")
    eq("style: lower start in tail (one underscore)",
       style_problem("battle_stepInitVars"), None)
    eq("style: 0x literal", style_problem("pSTWI_0x80"), "it carries a 0x literal")
    eq("style: trailing address run", style_problem("pSTWI_2010CCC"),
       "it ends in a digit run copied from the placeholder's address")
    eq("style: Maybe hedges", style_problem("maybeLoadSprite"), "it hedges (Maybe)")
    eq("style: empty prefix",
       style_problem("_chatbox"), "the prefix '' is not lower-case")

    eq("digits: the address's own run", address_digit_problem("loadFFC_203F5A4", "0x0203F5A4"),
       "the digit run 203F5A4 is copied from the address 0x0203F5A4")
    eq("digits: a slice of the address counts from three on",
       address_digit_problem("loadFFC", "0x080BFFC0"),
       "the digit run FFC is copied from the address 0x080BFFC0")
    eq("digits: no digits stays put",
       address_digit_problem("perChipClassChipCounts", "0x0203F5A4"), None)
    eq("digits: a two-digit coincidence stays", address_digit_problem("load80", "0x080BFFC0"), None)

    eq("prefixes: a camel prefix the project uses counts",
       "eBattle" in prefixes_in_use(["eBattle_runPhase", "chatbox_blitGlyph", "sub_1"]), True)
    eq("prefixes: an unused camel prefix stays out",
       "writeChipParams" in prefixes_in_use(["chatbox_blitGlyph"]), False)
    eq("prefixes: a lower prefix needs no allowance",
       "chatbox" in prefixes_in_use(["chatbox_x"]), False)

    relax = frozenset({"eBattle"})
    eq("fix: PascalCase stays (data allows it)",
       style_fixed("ChatboxPalette", relax), "ChatboxPalette")
    eq("fix: plain lowerCamel stays", style_fixed("chatboxBlitGlyph", relax), "chatboxBlitGlyph")
    eq("fix: upper prefix folds when not allowed", style_fixed("writeChipParams_x", relax),
       "writeChipParamsX")
    eq("fix: allowed prefix stays",
       style_fixed("eBattle_TrackedTags", relax), "eBattle_TrackedTags")
    eq("fix: two-underscore name",
       style_fixed("ai_attackStep_CommitMoveAndHit", relax), "ai_attackStepCommitMoveAndHit")
    eq("fix: a 0x literal stays out", style_fixed("pSTWI_0x80", relax), None)
    eq("fix: a Maybe stays out", style_fixed("maybe_loadSprite", relax), None)

    eq("placeholder: byte_", data_placeholder("byte_203F750"), True)
    eq("placeholder: unk_", data_placeholder("unk_2017A00"), True)
    eq("placeholder: dword_", data_placeholder("dword_2010CCC"), True)
    eq("placeholder: not a code placeholder", data_placeholder("sub_801CB38"), False)
    eq("placeholder: a real name", data_placeholder("eBattleFolder"), False)

    # offset-expression scanner: synthetic lines written to temp files, no real game.
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        (tmp_path / "asm").mkdir()
        (tmp_path / "data").mkdir()
        (tmp_path / "asm" / "test.s").write_text("\n".join([
            "battle_render:",
            "\tpush {lr}",
            "\tldr r0, off_801CA78 // =byte_203CA70",                # plain reference (no offset)
            "\tldr r1, [r0, #-4]  // byte_203CA74 - 0x203ca70",       # comment with arithmetic
            "\tldr r2, [r1, #12]  // base + offset",
            "\t.set STORAGE_BASE, byte_203CA70 + 0x10",               # .set with offset arithmetic
            "\t.set OFFSET, byte_203CA74 - 0x203ca70",                # .set arithmetic
            "\tpop {pc}",
        ]))
        (tmp_path / "data" / "render.s").write_text("byte_203CA74 + 4\n")
        # patch _OFFSET_FILES to point at these
        global _OFFSET_FILES
        _OFFSET_FILES = (tmp_path / "asm" / "test.s", tmp_path / "data" / "render.s")
        eq("offset: byte_203CA74 appears in expression comment",
           offset_expression_problem("byte_203CA74") is not None, True)
        eq("offset: byte_203CA70 appears in .set with arithmetic",
           offset_expression_problem("byte_203CA70") is not None, True)
        eq("offset: a label no expression touches stays out",
           offset_expression_problem("chatboxBlitGlyph") is None, True)
        # check the first hit's file:line format
        problem = offset_expression_problem("byte_203CA74")
        assert problem is not None
        import re as _re
        match = _re.search(r"\(([^)]+:\d+)\)", problem)
        eq("offset: the first hit is in file:line form", match is not None, True)
        _OFFSET_FILES = None   # restore default

    # the selection logic: drop, skip and keep (in-memory only, no real ELF)
    rows = [
        Row("0x0203F5A4\tbyte_203F5A4\tgoodName\tprobable\t5\tagree\t100\t0\t-"),
        Row("0x02010CCC\tdword_2010CCC\tePanelData\tprobable\t4\tagree\t100\t0\t-"),    # db-name collide
        Row("0x0201ABC0\tunk_201ABC0\tmaybeX\tprobable\t4\tagree\t100\t0\t-"),         # maybe hedge
        Row("0x0201ABC4\tunk_201ABC4\tbad__name\tprobable\t4\tagree\t100\t0\t-"),      # style double underscore
        Row("0x0201ABD0\tunk_201ABD0\tdupName\tprobable\t5\tagree\t100\t0\t-"),        # dup
        Row("0x0201ABD4\tunk_201ABD4\tdupName\thypothesis\t4\tagree\t100\t0\t-"),      # dup (hypothesis)
        Row("0x0201ABE0\tunk_201ABE0\tportIdent\tprobable\t4\tagree\t100\t0\t-"),     # port collide
        Row("0x0201ABF0\tunk_201ABF0\tasm\thypothesis\t4\tnone\t100\t0\t-"),           # not probable
    ]
    with tempfile.TemporaryDirectory() as run_dir:
        for old in ("byte_203F5A4", "dword_2010CCC", "unk_201ABC0", "unk_201ABC4",
                     "unk_201ABD0", "unk_201ABD4", "unk_201ABE0"):
            with open(os.path.join(run_dir, f"{old}.yaml"), "w", encoding="utf-8") as f:
                yaml.safe_dump({"name": old, "address": "0x02000000", "kind": "data", "status": "probable",
                                "summary": "Proposed name: x. Something.", "region": "EWRAM",
                                "size": 4}, f, sort_keys=False)
        syms = {"unk_201ABF0": 0x0201ABF0}        # the asm-include collision's elf side
        port = frozenset({"portIdent"})
        chosen, skipped = select(rows, run_dir, syms, port, count, frozenset())
        kept = [r.old for r, _ in chosen]
        kept_names = [r.proposed for r, _ in chosen]
        eq("select: only the clean probable+agree is kept", kept, ["byte_203F5A4"])
        eq("select: the proposal is the new name", kept_names, ["goodName"])
        reasons = {old: reason for _, old, _, reason in skipped}
        eq("select: the dup note for both rows",
           reasons.get("unk_201ABD0", ""), "proposed for 2 labels in the run")
        eq("select: a hypothesis row is not in skipped (it is not eligible at all)",
           reasons.get("unk_201ABD4", ""), "")
        eq("select: the db-name collide reason (single, no dup)",
           reasons.get("dword_2010CCC", ""), "the name is already a symbols/db entry name")
        eq("select: the cport collide reason",
           reasons.get("unk_201ABE0", ""),
           "the name is already an identifier in cport/ (grep -w): the rename would collide with it")
        eq("select: the Maybe hedge says naming style",
           (reasons.get("unk_201ABC0", "").startswith("naming style:")), True)
        eq("select: the double underscore says naming style",
           (reasons.get("unk_201ABC4", "").startswith("naming style:")), True)
        eq("select: a status=hypothesis row is not in skipped",
           "unk_201ABF0" in reasons, False)

    # the entry: data-specific keys, run id in evidence
    candidate = {"name": "byte_203F5A4", "address": "0x0203F5A4", "kind": "data", "status": "probable",
                 "summary": "Proposed name: eBattleLinkPacketEnemy. Per-link enemy packet.",
                 "region": "EWRAM", "size": 64,
                 "interface": "(r1 = linkState) -> r0",
                 "details": "Filled in by buildEnemyLinkPacket.",
                 "evidence": [{"method": "caller-analysis", "date": "2026-10-20", "detail": "ok",
                               "refs": ["bn6f/asm/asm32.s:1234"],
                               "reproduce": "grep -n -w byte_203F5A4 bn6f/ewram.s",
                               "result": "proposed by minimax/MiniMax-M3, self_score 4; "
                                         "cross-checked by minimax/MiniMax-M3: agree"}],
                 "coverage": ["link"], "open_questions": ["bigger?"],
                 "sources": ["wf investigate-data investigate-data-20261020-192515", "models"]}
    entry = entry_of(candidate, "investigate-data-20261020-192515")
    eq("entry: kind is data", entry["kind"], "data")
    eq("entry: status is probable", entry["status"], "probable")
    eq("entry: name is the current placeholder", entry["name"], "byte_203F5A4")
    eq("entry: the run id is in the evidence's result",
       "run wf investigate-data investigate-data-20261020-192515" in entry["evidence"][0]["result"], True)
    eq("entry: region kept", entry["region"], "EWRAM")
    eq("entry: size kept", entry["size"], 64)
    rescued = entry_of(candidate, "investigate-data-20261020-192515",
                       rescue="deduped by tools/investigate_data_land.py dedupe")
    eq("entry: the rescue is named in the evidence",
       "deduped by tools/investigate_data_land.py dedupe" in rescued["evidence"][0]["result"], True)

    if bad:
        for b in bad:
            print("selfcheck: " + b)
        sys.exit(f"selfcheck: {len(bad)} failures")
    print("selfcheck: ok")


# ── the rescue subcommands (stubs mirroring investigate_land.py) ──────
# The function landing has rescue subcommands that lift dropped candidates
# back into landable form. The data sibling needs the same when enough data
# runs have run+land with their own skipped.tsv. They share the same Shape:

def style_fix_main(_args: Any) -> None:
    """The naming-style rescues (the only mechanical ones), without a model. Reads from
    build/investigate_data/<run>/land_skipped.tsv, writes rescue/style.tsv.

    Not implemented: the data run has only just completed one batch; the rescue subcommands
    are written when the second batch lands and produces skipped.tsv worth rescuing."""
    sys.exit("style-fix: TODO")


def dedupe_main(_args: Any) -> None:
    """The candidates whose proposed names collide (across runs and with landed names)."""
    sys.exit("dedupe: TODO")


def reconcile_main(_args: Any) -> None:
    """The candidates the cross-check revised with a `revised_name`."""
    sys.exit("reconcile: TODO")


def land_rescued_main(_args: Any) -> None:
    """Apply the rescue plans; writes the symbols/db entries and runs the record workflow."""
    sys.exit("land-rescued: TODO")


# ── the CLI ────────────────────────────────────────────────────────────

def _main(argv: list[str] | None = None) -> None:
    if argv is None:
        argv = sys.argv[1:]
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return
    if argv[0] == "selfcheck":
        # find optional --root (a temp dir for the in-memory offset/path checks)
        root: str | None = None
        if "--root" in argv:
            root = argv[argv.index("--root") + 1]
        selfcheck(root=root)
        return
    if argv[0] in ("style-fix", "dedupe", "reconcile", "land-rescued"):
        import argparse
        sub = argv[0]
        rest = argv[1:]
        parser = argparse.ArgumentParser(prog=f"investigate_data_land {sub}")
        parser.add_argument("--root")
        parser.add_argument("--limit", type=int, default=200)
        parser.add_argument("--batch", type=int, default=50)
        parser.add_argument("--dry-run", action="store_true")
        parser.add_argument("--model", dest="model", default="hyper/qwen3.8-flash")
        parser.add_argument("--model-checker", dest="model_checker", default="minimax/MiniMax-M3")
        args = parser.parse_args(rest)
        if args.root:
            use_root(args.root)
        if sub == "style-fix":
            style_fix_main(args)
        elif sub == "dedupe":
            dedupe_main(args)
        elif sub == "reconcile":
            reconcile_main(args)
        elif sub == "land-rescued":
            land_rescued_main(args)
        return
    # land RUN_DIR [--count N] [--dry-run] [--root REPO]
    import argparse
    parser = argparse.ArgumentParser(prog="investigate_data_land",
                                     description="Land wf investigate-data candidates into "
                                                 "symbols/db and bn6f.")
    parser.add_argument("--root")
    parser.add_argument("run_dir", nargs=1)
    parser.add_argument("--count", type=int, default=30)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)
    if args.root:
        use_root(args.root)
    land(args.run_dir[0], args.count, args.dry_run)


if __name__ == "__main__":
    _main()
