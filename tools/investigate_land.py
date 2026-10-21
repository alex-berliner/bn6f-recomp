#!/usr/bin/env python3
"""Land wf investigate's finished candidates into symbols/db and bn6f.

  tools/investigate_land.py RUN_DIR --count 40 [--dry-run]
  tools/investigate_land.py selfcheck
  tools/investigate_land.py style-fix [--root REPO]
  tools/investigate_land.py dedupe [--root REPO] [--limit N]
                     [--model PROPOSER] [--model-checker CHECKER]
  tools/investigate_land.py reconcile [--root REPO] [--limit N]
  tools/investigate_land.py land-rescued [--root REPO] [--limit N] [--batch N] [--dry-run]

The four rescue the candidates earlier runs' land runs dropped (style-fix and dedupe from
each run's land_skipped.tsv; reconcile from the runs' cross-check revise verdicts):

  style-fix: the naming-style rejects, without a model. The style rule is relaxed where
    bn6f already uses the shape (a prefix the elf's symbols or the database spell with an
    upper-case letter — battleObject_ — is allowed), and every other break is normalized
    mechanically (ai_attackStep_CommitMoveAndHit -> ai_attackStepCommitMoveAndHit). What it
    rescues goes to build/investigate/rescue/style.tsv, what it cannot fix it reports.
  dedupe: the candidates whose proposed name was proposed for several functions (across ALL
    finished runs, also against names already landed in symbols/db or bn6f.elf). One call to
    hyper/glm-5.3-flash per group (the members' summaries and the same context builder as
    investigate: gather_context) gives every member a DISTINCT name that says what differs,
    as JSON; a reply that repeats a name, copies an address or breaks the style rule is asked
    once more and otherwise dropped. minimax/MiniMax-M3 then checks each group's naming
    against the code; only agreed groups are kept. Cached under build/investigate/dedupe/
    (resumable); a 402 or a daily rate limit stops it cleanly. Agreed members go to
    build/investigate/rescue/dedupe.tsv.
  reconcile: the candidates the cross-check revised with a `revised_name` (a second opinion
    on the checker's name). The function is still a placeholder, has no symbols/db entry,
    and the revised name passes the relaxed style rule, copies no part of the function's
    address, is not taken in bn6f.elf or symbols/db, and is unique among all the runs'
    revise candidates (a shared revised name drops every claimant). One call per function to
    hyper/glm-5.3-flash shows the assembly context (the same context builder as investigate:
    gather_context), the original proposal, the checker's objection and revised name and
    summary, and asks which to keep — accept_revised, keep_original or neither. Only
    accept_revised agrees with the checker; both models then back the revised name. Cached
    under build/investigate/reconcile/NAME.json (resumable); a 402 or a daily rate limit
    stops the pass cleanly. Agreed functions go to build/investigate/rescue/reconcile.tsv.
  land-rescued: writes the rescued candidates' symbols/db entries the way land does (under the
    old placeholder name, status probable, the rescue named in the evidence) and runs the
    record workflow once per batch of at most --batch (200). A record failure — the port's ROM
    changed — is reported, never retried. Reads three lists (style, dedupe, reconcile).

RUN_DIR is a finished investigate run (build/investigate/RUN_ID) with its
report.tsv and one candidate symbols/db-format YAML per function. The tool
selects the candidates the cross-check agreed with, writes their symbols/db
entries and runs the record workflow ONCE for the batch (which renames the
symbols in bn6f, writes the // symdb: comments, renders SYMBOLS.md and
commits). The record workflow renames entries itself: each symbols/db entry is
written under the function's current placeholder name, and record's rename
(turning OLD into NEW in bn6f) moves it to symbols/db/NEWNAME.yaml with
previous_names [old placeholder] and a change record — the README's format.
Only RUN_DIR, symbols/db and this file are touched; the record run commits its
own work and sweeps nothing of the other sessions'.

Selection (from report.tsv): status probable AND cross-check agree. Dropped
candidates go to RUN_DIR/land_skipped.tsv with the reason:
  - the proposed name is proposed for more than one function in the run;
  - it already exists as a symbol of bn6f/bn6f.elf or as a symbols/db name;
  - it breaks the naming style: lowerCamel after a prefix_ (a prefix is
    lower-case, one underscore, then lowerCamel with no underscore in it), and
    no digit run copied from the function's address;
  - the function already has a symbols/db entry (skipped, not dropped);
  - the run folder has no candidate YAML for it.
The --count best by self_score are taken, ties by more named neighbours (the
text symbols around the function in bn6f.elf), then the lower address.

Before the record run every written entry is validated with tools/symdb.py's
own check, and the whole database is checked (the record run checks it again).
On a record failure or a changed ROM the tool stops and says the error; it
never retries.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import threading
from collections.abc import Iterable, Sequence
from pathlib import Path
from typing import Any, NamedTuple

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, "tools")
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)
import gamepaths  # noqa: E402  (tools/gamepaths.py: the game profile)
import llm  # noqa: E402  (tools/llm.py: the model providers)

DB = os.path.join(ROOT, "symbols", "db")
ELF = gamepaths.ORIG_ELF
WF = os.path.join(ROOT, "wf")
BN6F_DIR = gamepaths.BN6F_DIR


def use_root(repo: str) -> None:
    """Work against another checkout (a worktree's tool runs the main tree's data and record
    workflow: --root). Paths and the modules already imported from this tree's tools are redone
    against REPO's."""
    global ROOT, TOOLS, DB, ELF, WF, BN6F_DIR
    ROOT = os.path.abspath(repo)
    TOOLS = os.path.join(ROOT, "tools")
    DB = os.path.join(ROOT, "symbols", "db")
    WF = os.path.join(ROOT, "wf")
    sys.path.insert(0, TOOLS)
    sys.modules.pop("gamepaths", None)
    import gamepaths  # noqa: PLC0415  (the checkout --root names)
    globals()["gamepaths"] = gamepaths
    ELF = gamepaths.ORIG_ELF
    BN6F_DIR = gamepaths.BN6F_DIR
PLACEHOLDER_PREFIXES = ("sub_", "nullsub_", "dead_", "unused_", "asm")
GENERIC_PLACEHOLDER = re.compile(r"^[a-z]+_[0-9A-Fa-f]{6,8}$")
DIGIT_RUN = re.compile(r"[0-9a-fA-F]{3,}")
HEX_LITERAL = re.compile(r"0x[0-9a-fA-F]")
TRAILING_ADDRESS = re.compile(r"_?[0-9a-fA-F]{7,}$")
PROPOSED_PREFIX = re.compile(r"^Proposed name: (\S+?)\.\s*")
NAMED_NEIGHBOURS = 4          # text symbols on each side that count for the tie-break
ELF_TYPES = "tTwW"            # nm types that can be code


# ── the report and the candidates ──────────────────────────────────────

class Row:
    """One report.tsv line (without the '#' header)."""

    def __init__(self, line: str) -> None:
        fields = (line.split("\t") + [""] * 8)[:8]
        (self.address, self.old, self.proposed, self.status, self.score,
         self.check, self.tokens, self.retries) = (f.strip() for f in fields)

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


# ── the naming rules ───────────────────────────────────────────────────

def style_problem(name: str, allowed: frozenset[str] = frozenset()) -> str | None:
    """Why NAME breaks the naming style (lowerCamel after a prefix_), or None. A prefix in
    ALLOWED is exempt: the project already spells it with its upper-case letters."""
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
        if not rest or not re.fullmatch(r"[a-z][A-Za-z0-9]*", rest):
            return f"{rest!r} after the prefix is not lowerCamel"
    elif not re.fullmatch(r"[a-z][A-Za-z0-9]*", name):
        return "it is not lowerCamel"
    return None


def address_digit_problem(name: str, address: str) -> str | None:
    """Why NAME carries digits copied from its ADDRESS (0x-prefixed hex), or None.

    A run of three or more hex characters in the name counts when a piece of it
    (three characters or more) is a piece of the address: models copy the whole
    address or a slice of it, and letters next to the copied digits join their run."""
    hexdigits = address.removeprefix("0x").removeprefix("0X").lstrip("0").upper() or "0"
    for run in DIGIT_RUN.findall(name):
        for width in range(len(run), 2, -1):
            piece = next((run[i:i + width].upper() for i in range(len(run) - width + 1)
                          if run[i:i + width].upper() in hexdigits), None)
            if piece is not None:
                return f"the digit run {piece} is copied from the address {address}"
    return None


def placeholder(name: str) -> bool:
    """A name that says nothing yet (the investigate workflow's rule)."""
    return name.startswith(PLACEHOLDER_PREFIXES) or GENERIC_PLACEHOLDER.fullmatch(name) is not None


# ── the style rule, relaxed where bn6f already uses a shape ────────────

def prefixes_in_use(names: Iterable[str]) -> frozenset[str]:
    """The prefixes bn6f already spells with an upper-case letter (battleObject_ of
    battleObject_runPhase): a proposed name may keep such a prefix, because the project
    already uses it. A name's PREFIX (everything before its first underscore) counts when
    some symbol or database name starts with 'PREFIX_'."""
    used: set[str] = set()
    for name in names:
        prefix, sep, _ = name.partition("_")
        if sep and any(c.isupper() for c in prefix):
            used.add(prefix)
    return frozenset(used)


_ACRONYM = re.compile(r"[A-Z]{2,}")


def _join_camel(parts: Sequence[str], lead_lower: bool = False) -> str:
    """PARTS as one lowerCamel word: later parts capitalised, an ALL-CAPS part (an acronym,
    UNI of rfu_UNI_processRecvChunk) folded to Uni — and, with LEAD_LOWER, the first part
    lower-case (uni, sChipEntryAt), because a prefix's rest may not start upper."""
    out: list[str] = []
    for index, part in enumerate(parts):
        if index == 0:
            if lead_lower:
                out.append(part.lower() if _ACRONYM.fullmatch(part) else part[:1].lower() + part[1:])
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
    """NAME with its style break fixed mechanically (no model): a second underscore's parts
    joined lowerCamel, an upper-case start lowered, a prefix the project does not use folded
    into one word — or None when nothing mechanical fits (a Maybe, a 0x literal)."""
    if "maybe" in name.lower() or HEX_LITERAL.search(name):
        return None
    parts = name.split("_")
    if len(parts) == 1:
        if re.fullmatch(r"[A-Za-z][A-Za-z0-9]*", parts[0]):
            return parts[0][:1].lower() + parts[0][1:]   # GiveAllChips -> giveAllChips
        return None
    prefix = parts[0]
    if prefix not in allowed and not re.fullmatch(r"[a-z][a-z0-9]*", prefix):
        return _join_camel(parts)      # writeChipParams_chip74 -> writeChipParamsChip74
    tail = _join_camel(parts[1:], lead_lower=True)
    if tail[:1].isdigit():             # a rest may not start with a digit: fold everything
        return _join_camel(parts)      # copy_32ByteEntries... -> copy32ByteEntries...
    return f"{prefix}_{tail}"


# ── bn6f.elf: the symbols and the named neighbours ─────────────────────

def elf_symbols() -> dict[str, int]:
    """bn6f.elf's symbols (name -> address, Thumb bit ignored)."""
    out = subprocess.run(["arm-none-eabi-nm", ELF], capture_output=True, text=True, check=True).stdout
    syms: dict[str, int] = {}
    for line in out.splitlines():
        fields = line.split()
        if len(fields) == 3:
            syms.setdefault(fields[2], int(fields[0], 16) & ~1)
    return syms


def text_symbols(syms: dict[str, int]) -> list[tuple[int, str]]:
    """The ELF's code symbols by address (nm's type tells code from data)."""
    out = subprocess.run(["arm-none-eabi-nm", ELF], capture_output=True, text=True, check=True).stdout
    code = [(int(f[0], 16) & ~1, f[2]) for f in (l.split() for l in out.splitlines())
            if len(f) == 3 and f[1] in ELF_TYPES]
    return sorted(code)


def named_neighbours(address: int, code: list[tuple[int, str]]) -> int:
    """How many of the code symbols nearest ADDRESS (NAMED_NEIGHBOURS a side) are named."""
    addresses = [a for a, _ in code]
    lo, hi = 0, len(addresses)
    while lo < hi:                       # first index with address >= target
        mid = (lo + hi) // 2
        if addresses[mid] < address:
            lo = mid + 1
        else:
            hi = mid
    count = 0
    for i in list(range(max(0, lo - NAMED_NEIGHBOURS), lo)) + \
             list(range(min(len(code), lo + 1), min(len(code), lo + 1 + NAMED_NEIGHBOURS))):
        if code[i][1] != _at(address, code) and not placeholder(code[i][1]):
            count += 1
    return count


def _at(address: int, code: list[tuple[int, str]]) -> str:
    for a, name in code:
        if a == address:
            return name
    return ""


# ── the symbols/db names ───────────────────────────────────────────────

def db_names() -> set[str]:
    return {f[:-5] for f in os.listdir(DB) if f.endswith(".yaml")}


# ── selection ──────────────────────────────────────────────────────────

def select(rows: list[Row], run_dir: str, syms: dict[str, int], code: list[tuple[int, str]],
           count: int, allowed: frozenset[str] = frozenset()) \
        -> tuple[list[tuple[Row, dict[str, Any]]], list[tuple[str, str, str, str]]]:
    """The candidates to land and the dropped/skipped ones with their reasons.

    Returns ((row, candidate), ...) in landing order and
    (address, old, proposed, reason) for every probable+agree row not landed. ALLOWED is
    the prefixes bn6f already spells with upper-case letters (style_problem's exemption)."""
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
            skipped.append((row.address, row.old, row.proposed, "the function already has a symbols/db entry"))
            continue
        if os.path.exists(os.path.join(ROOT, "bn6f", "asm", f"{row.old}.s")):
            skipped.append((row.address, row.old, row.proposed,
                            "its name is also an asm include file: the rename would rewrite the include path, not the file"))
            continue
        if counts[row.proposed] > 1:
            skipped.append((row.address, row.old, row.proposed,
                            f"proposed for {counts[row.proposed]} functions in the run"))
            continue
        if row.proposed in syms:
            skipped.append((row.address, row.old, row.proposed, "the name is already a symbol of bn6f.elf"))
            continue
        if row.proposed in names:
            skipped.append((row.address, row.old, row.proposed, "the name is already a symbols/db entry name"))
            continue
        problem = style_problem(row.proposed, allowed)
        if problem is None:
            problem = address_digit_problem(row.proposed, row.address)
        if problem is not None:
            skipped.append((row.address, row.old, row.proposed, f"naming style: {problem}"))
            continue
        eligible.append((row, candidate))
    eligible.sort(key=lambda pair: (-pair[0].score_int(),
                                    -named_neighbours(int(pair[0].address, 16), code),
                                    int(pair[0].address, 16)))
    return eligible[:count], skipped


# ── the symbols/db entry ───────────────────────────────────────────────

def entry_of(candidate: dict[str, Any], run_id: str, rescue: str | None = None) -> dict[str, Any]:
    """The symbols/db entry for a candidate, in the README's key order.

    Written under the candidate's current (placeholder) name; the record run's
    rename turns it into NEWNAME.yaml with previous_names [old placeholder].
    RESCUE, when given, is named in the first evidence item's result (how the
    candidate's name was rescued after the run had dropped it)."""
    summary = str(candidate.get("summary") or "")
    proposed = PROPOSED_PREFIX.match(summary)
    if proposed is not None:
        summary = summary[proposed.end():]
    evidence = []
    for index, item in enumerate(candidate.get("evidence") or []):
        item = dict(item)
        result = str(item.get("result") or "")
        if f"wf investigate {run_id}" not in result:
            item["result"] = f"{result}; run wf investigate {run_id}"
        if rescue and index == 0:
            item["result"] = f"{item['result']}; {rescue}"
        evidence.append(item)
    entry: dict[str, Any] = {"name": candidate["name"], "address": candidate["address"],
                             "kind": candidate.get("kind", "function"), "status": "probable",
                             "summary": summary}
    if candidate.get("interface"):
        entry["interface"] = candidate["interface"]
    if candidate.get("details"):
        entry["details"] = candidate["details"]
    entry["evidence"] = evidence
    if candidate.get("coverage"):
        entry["coverage"] = candidate["coverage"]
    if candidate.get("open_questions"):
        entry["open_questions"] = candidate["open_questions"]
    entry["sources"] = [f"wf investigate {run_id}"]
    return entry


def write_entry(candidate: dict[str, Any], run_id: str, rescue: str | None = None) -> str:
    """Write the entry as symbols/db/OLD.yaml (the current name); its path."""
    entry = entry_of(candidate, run_id, rescue)
    path = os.path.join(DB, f"{entry['name']}.yaml")
    with open(path, "w", encoding="utf-8") as f:
        yaml.safe_dump(entry, f, sort_keys=False, allow_unicode=True, width=100)
    return path


def symdb_entry_problems(path: str) -> list[str]:
    """What tools/symdb.py's own check says about one written entry."""
    sys.path.insert(0, TOOLS)
    import symdb  # noqa: PLC0415  (the repo's checker, cached)
    where = os.path.relpath(path, ROOT)
    loaded = symdb.load_full([where])[0]
    if loaded.get("_unparsed"):
        return ["not a YAML mapping"]
    syms = symdb.elf_symbols(symdb.Cache(), {loaded.get("name")})
    entry = symdb.slim(loaded)
    entry["_file"] = where
    return [f"{problem.message}" for problem in symdb.problems([entry], syms, symdb.Tree())]


def symdb_check_all() -> tuple[bool, list[str]]:
    """The whole database's check (the record run runs it again): (passed, problem lines)."""
    done = subprocess.run([sys.executable, os.path.join(TOOLS, "symdb.py"), "check"],
                          capture_output=True, text=True, cwd=ROOT)
    problems = [line for line in done.stdout.splitlines() if ": " in line and not line.endswith(" problems")]
    return done.returncode == 0 and not problems, problems


# ── the record run ─────────────────────────────────────────────────────

def run_record(pairs: list[tuple[str, str]], found_by: str, run_dir: str, log_path: str) -> dict[str, Any]:
    """`./wf record --symbol OLD ... --rename OLD NEW ... --message-file FILE`, once. Its report.
    FOUND_BY is the commit message's "found by ..." phrase (wf investigate RUN_ID, or the
    rescue's own)."""
    message = (f"symbols: name {len(pairs)} functions found by {found_by} "
               f"(proposing and checking models: see each entry's evidence)\n"
               f"\n"
               f"Candidates selected from the run's report (status probable, cross-check agree); "
               f"entries written by tools/investigate_land.py, checked with tools/symdb.py, "
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
        sys.exit(f"the record run left no report, see {log_path}:\n" + "\n".join(text.splitlines()[-30:]))
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
    """ROM identical, the database clean, the commits in, the investigate listing unaffected."""
    ok, problems = symdb_check_all()
    if not ok:
        sys.exit("symbols/db check fails after the record run:\n" + "\n".join(problems))
    print(f"symbols/db check: ok")
    commit = report.get("commit")
    log = subprocess.run(["git", "log", "-1", "--format=oneline"], cwd=ROOT, capture_output=True, text=True).stdout
    if commit and commit not in log:
        sys.exit(f"the record run's commit {commit} is not HEAD ({log.strip()})")
    bn6f = subprocess.run(["git", "log", "-3", "--format=oneline"], cwd=BN6F_DIR,
                          capture_output=True, text=True).stdout
    print(f"recomp commit: {log.strip()}")
    print(f"bn6f log:\n  " + "\n  ".join(bn6f.strip().splitlines()))
    listing = subprocess.run([WF, "investigate", "--count", "0"], cwd=ROOT, capture_output=True, text=True)
    if listing.returncode != 0:
        sys.exit(f"./wf investigate --count 0 failed:\n{listing.stdout[-2000:]}{listing.stderr[-2000:]}")
    print(f"./wf investigate --count 0: ok "
          f"({next((l for l in listing.stdout.splitlines() if 'finished' in l), '').strip()})")
    print(f"record log: {log_path}")


# ── the skipped rows of the finished runs ─────────────────────────────

class SkippedRow(NamedTuple):
    """One land_skipped.tsv line, with the run it came from."""
    run_id: str
    address: str
    old: str
    proposed: str
    reason: str


DUP_REASON = re.compile(r"^proposed for (\d+) functions in the run$")
TAKEN_ELF = "the name is already a symbol of bn6f.elf"
TAKEN_DB = "the name is already a symbols/db entry name"
STYLE_REASON = "naming style: "


def finished_run_dirs(base: str | None = None) -> list[str]:
    """The finished investigate runs' directories (report.tsv and land_skipped.tsv both there;
    a run the provider refused, *.failed-429, is not finished)."""
    base = base or os.path.join(ROOT, "build", "investigate")
    out: list[str] = []
    for name in sorted(os.listdir(base)):
        if not name.startswith("investigate-") or name.endswith(".failed-429"):
            continue
        run_dir = os.path.join(base, name)
        if all(os.path.exists(os.path.join(run_dir, f)) for f in ("report.tsv", "land_skipped.tsv")):
            out.append(run_dir)
    return out


def any_run_dirs(base: str | None = None) -> list[str]:
    """The investigate runs that wrote a report.tsv (their investigation finished, regardless of
    whether land ran). The reconcile pass wants every revise verdict, even runs that were never
    landed (they still wrote checkers/NAME.json). A run the provider refused, *.failed-429,
    is not included."""
    base = base or os.path.join(ROOT, "build", "investigate")
    out: list[str] = []
    for name in sorted(os.listdir(base)):
        if not name.startswith("investigate-") or name.endswith(".failed-429"):
            continue
        run_dir = os.path.join(base, name)
        if os.path.exists(os.path.join(run_dir, "report.tsv")):
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
    whose sibling in one run was dropped for another reason (already a symbol of bn6f.elf, no
    candidate yaml) still forms the group: its one row says the name was contested there."""
    groups: dict[str, list[SkippedRow]] = {}
    for row in rows:
        if DUP_REASON.match(row.reason):
            groups.setdefault(row.proposed, []).append(row)
    return groups


def other_claimants(rows: Sequence[SkippedRow], name: str,
                    members: Sequence[SkippedRow]) -> list[SkippedRow]:
    """The other rows that proposed NAME (any reason, any run), for the dedupe prompt."""
    member_keys = {(row.run_id, row.old) for row in members}
    return [row for row in rows if row.proposed == name and (row.run_id, row.old) not in member_keys]


# ── the style fixes ───────────────────────────────────────────────────

def style_fix_plan(rows: Sequence[SkippedRow], allowed: frozenset[str],
                   taken: frozenset[str] = frozenset()) \
        -> tuple[list[tuple[SkippedRow, str]], list[tuple[SkippedRow, str]]]:
    """The style rejects a mechanical fix rescues, and the ones nothing mechanical fits, with
    why: the fix must pass the relaxed style rule, must not copy its function's address and
    must not collide with a name bn6f already uses or with another fix."""
    fixes: list[tuple[SkippedRow, str]] = []
    stuck: list[tuple[SkippedRow, str]] = []
    seen: dict[str, SkippedRow] = {}
    for row in rows:
        fixed = style_fixed(row.proposed, allowed)
        why: str | None
        if fixed is None:
            why = "no mechanical fix (it hedges or carries a 0x literal)"
        elif (problem := style_problem(fixed, allowed)) is not None:
            why = f"the fix still breaks the style: {problem}"
        elif (problem := address_digit_problem(fixed, row.address)) is not None:
            why = f"the fix still copies the address: {problem}"
        elif "_" not in fixed:
            why = "the fix lost the prefix_ underscore (its prefix is not one bn6f uses)"
        elif fixed in taken:
            why = "the fixed name is already taken in bn6f"
        elif fixed in seen:
            why = f"the fix collides with {seen[fixed].old} at {seen[fixed].address}"
        else:
            seen[fixed] = row
            fixes.append((row, fixed))
            continue
        stuck.append((row, why or ""))
    return fixes, stuck


# ── the rescue lists and the batches ──────────────────────────────────

class RescueRow(NamedTuple):
    """One rescued candidate: run, function, its new name, how it was rescued."""
    run_id: str
    address: str
    old: str
    new: str
    how: str            # "style" or "dedupe"
    group: str          # the dedupe group's shared name, or "-"


RESCUE_HEADER = "# run_id\taddress\told\tnew\thow\tgroup\n"
RESCUE_DIR = os.path.join("build", "investigate", "rescue")


def rescue_path(kind: str) -> str:
    return os.path.join(ROOT, RESCUE_DIR, f"{kind}.tsv")


def write_rescue_rows(kind: str, rows: Sequence[RescueRow]) -> str:
    path = rescue_path(kind)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(RESCUE_HEADER)
        for row in rows:
            f.write("\t".join((row.run_id, row.address, row.old, row.new, row.how, row.group)) + "\n")
    return path


def read_rescue_rows(kind: str) -> list[RescueRow]:
    path = rescue_path(kind)
    rows: list[RescueRow] = []
    if not os.path.exists(path):
        return rows
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        fields = (line.split("\t") + [""] * 6)[:6]
        rows.append(RescueRow(*fields))
    return rows


def merge_round_robin(first: Sequence[RescueRow], second: Sequence[RescueRow]) -> list[RescueRow]:
    """Both lists' rows, alternating (a dedupe row, a style row, ...): a pilot batch taken from
    the head of the merged list exercises both kinds."""
    out: list[RescueRow] = []
    for index in range(max(len(first), len(second))):
        if index < len(first):
            out.append(first[index])
        if index < len(second):
            out.append(second[index])
    return out


def batches(items: Sequence[Any], size: int) -> list[list[Any]]:
    """The items in order, at most SIZE per batch."""
    return [list(items[start:start + size]) for start in range(0, len(items), max(1, size))]


# ── the dedupe answers ────────────────────────────────────────────────

_JSON_BLOCK = re.compile(r"```(?:json)?\s*(\{.*?\})\s*```", re.S)


def json_object(reply: str) -> dict[str, Any] | None:
    """The JSON object in a model's reply (a ```json block, or the first {...} span), or None —
    the same reading tools/wf's investigate does of its replies."""
    match = _JSON_BLOCK.search(reply)
    candidates = [match[1]] if match else []
    first, last = reply.find("{"), reply.rfind("}")
    if first >= 0 and last > first:
        candidates.append(reply[first: last + 1])
    for candidate in candidates:
        try:
            loaded = json.loads(candidate)
        except json.JSONDecodeError:
            continue
        if isinstance(loaded, dict):
            return loaded
    return None


def parse_names(reply: str) -> list[str] | None:
    """The dedupe reply's names (one JSON object with a names list of distinct names), or None."""
    parsed = json_object(reply)
    if parsed is None:
        return None
    names = parsed.get("names")
    if not isinstance(names, list) or not all(isinstance(n, str) and n.strip() for n in names):
        return None
    return [n.strip() for n in names]


def names_problem(names: Sequence[str], members: Sequence[SkippedRow], shared: str,
                  taken: frozenset[str], allowed: frozenset[str]) -> str | None:
    """Why the proposed names cannot be kept (a repeat, an address copy, a style break), or None."""
    if len(names) != len(members):
        return f"{len(names)} names for {len(members)} functions"
    if len(set(names)) != len(names):
        return "a name is proposed twice"
    for name, member in zip(names, members):
        if name == shared:
            return f"{name} repeats the shared name"
        if name in taken:
            return f"{name} is already taken in bn6f"
        if placeholder(name):
            return f"{name} is still a placeholder"
        if (problem := style_problem(name, allowed)) is not None:
            return f"{name}: {problem}"
        if (problem := address_digit_problem(name, member.address)) is not None:
            return f"{name}: {problem}"
    return None


def parse_group_check(reply: str) -> tuple[str | None, str | None, str | None]:
    """The dedupe check's (verdict, reason, problem): one JSON object, agree or revise."""
    parsed = json_object(reply)
    if parsed is None:
        return None, None, "the reply held no JSON object"
    verdict = parsed.get("verdict")
    if verdict not in ("agree", "revise"):
        return None, None, f"verdict {verdict!r} is not agree or revise"
    reason = parsed.get("reason")
    if not isinstance(reason, str) or not reason.strip():
        return None, None, "a verdict without a reason"
    return str(verdict), " ".join(reason.split()), None


# ── asking the models (the rescue's own pass) ─────────────────────────

DEDUPE_SYSTEM = ("You name undocumented functions of a GBA game's disassembly (MegaMan Battle "
                 "Network 6 Falzar) for the project's symbol database. Reply with exactly one "
                 "JSON object and nothing else.")
DEDUPE_CHECK_SYSTEM = ("You check another model's naming of undocumented functions of a GBA game's "
                       "disassembly (MegaMan Battle Network 6 Falzar) against the code. Reply with "
                       "exactly one JSON object and nothing else.")
DEDUPE_TASK = """Several placeholder functions were each proposed the same name, so none of them may
keep it: give every one of them a DISTINCT name that says what differs between them (for example
ai_attackStepWindup for the one that winds up versus ai_attackStepStrike for the one that strikes).
You are shown what each function's investigation concluded (its summary, interface, details) and
the same context that investigation saw (its callers, their call sites, the data it touches, its
assembly).

The project's naming rules:
- lowerCamel, with a subsystem prefix where one fits. Prefixes in use: {prefixes}.
- a prefix may keep upper-case letters only when the project already spells it that way
  (battleObject_ is in use; writeChipParams_ is not).
- the part after the prefix carries no underscore; no digit run copied from the function's own
  address; no "Maybe"; no 0x literal.

The shared name was {shared}{shared_note}. Every new name must differ from it, from every other
name in use, and from the other functions' new names.
{claimants}
{members}

Reason briefly, then reply with exactly one JSON object and nothing else — no markdown fence, no
prose around it (both spend the reply's token budget and can cut the JSON off):
{{"names": ["...", "..."]}}
One name per function above, in the same order, all distinct."""

DEDUPE_CHECK_TASK = """Another model gave these placeholder functions distinct names (they had all been
proposed the same name). Check each name against its function's code: does the name say what THIS
function does, and do the names say what differs between the functions?

{members}

The proposed names, in the same order:
{names}

Reason briefly, then reply with exactly one JSON object and nothing else — no markdown fence, no
prose around it:
{{"verdict": "agree" or "revise",
  "reason": "what in the code agrees with or contradicts the names, citing asm lines, callers or callees"}}
agree = every name fits its function and the set says what differs; revise = one or more do not.
Do not guess: if the code does not settle a name, revise and say which names fail and why."""

PROPOSER = "hyper/glm-5.3-flash"
CHECKER = "minimax/MiniMax-M3"
HANG_SECONDS = 330.0             # a call still unanswered this long is abandoned (move on)


def ask_once(spec: str, prompt: str, system: str, tag: str) -> tuple[str, str | None]:
    """(the reply, or "" and the problem). tools/llm.py waits out rate limits itself; a call still
    unanswered HANG_SECONDS in is abandoned — a hung provider call must not hold the pass."""
    import wf.tools.investigate as inv  # noqa: PLC0415  (the usage log and the output cap)
    result: dict[str, str] = {}
    done = threading.Event()

    def call() -> None:
        try:
            result["reply"] = llm.complete(spec, prompt, system=system, max_tokens=inv.MAX_TOKENS,
                                           timeout=180, retries=3, tag=tag, usage_log=str(inv.USAGE_LOG))
        except RuntimeError as error:
            result["problem"] = str(error)[:300]
        finally:
            done.set()

    thread = threading.Thread(target=call, daemon=True)
    thread.start()
    if not done.wait(HANG_SECONDS):
        return "", f"hung: still unanswered after {HANG_SECONDS:.0f} s, moved on"
    if "reply" in result:
        return result["reply"], None
    return "", result.get("problem") or "the call failed"


def load_json(path: str) -> dict[str, Any] | None:
    """A cached answer (the dedupe pass is resumable), or None."""
    if not os.path.exists(path):
        return None
    try:
        loaded = json.loads(open(path, encoding="utf-8").read())
    except (OSError, json.JSONDecodeError):
        return None
    return loaded if isinstance(loaded, dict) else None


def save_json(path: str, data: dict[str, Any]) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
        f.write("\n")


def _targets(inv: Any, members: Sequence[SkippedRow]) -> tuple[list[Any], list[SkippedRow]]:
    """An inv.Target per member (what gather_context needs), the way a run's select_targets
    builds them; the members without a lifted unit come back separately."""
    units = inv.funcindex.lifted_units()
    find = inv.funcindex.unit_finder(inv.funcindex.unit_ranges())
    neighbours = inv.call_graph() if inv.GRAPH.exists() else {}
    routes = inv.coverage_routes()
    scores = {name: score for name, _address, score in inv.read_scores()}
    db = inv.db_names()
    targets: list[Any] = []
    missing: list[SkippedRow] = []
    for member in members:
        address = int(member.address, 16)
        unit = inv.unit_of(member.old, address, units, find)
        if unit is None:
            missing.append(member)
            continue
        callers, callees, _sites = neighbours.get(address & ~1, (frozenset(), frozenset(), ()))
        named = sum(1 for n in (*callers, *callees)
                    if (found := find(n)) is not None and (not inv.placeholder(found) or found in db))
        covered = tuple(route for route, addresses in routes.items()
                        if unit.lo in addresses or unit.lo + 1 in addresses)
        targets.append(inv.Target(name=member.old, address=address, score=scores.get(member.old, 0),
                                  instructions=(unit.hi - unit.lo) // 2,
                                  captured=(inv.SNAPSHOTS / member.old).is_dir(), routes=covered,
                                  named_neighbours=named, source=unit.asm or ""))
    return targets, missing


def member_briefs(inv: Any, members: Sequence[SkippedRow], candidates: dict[str, dict[str, Any]],
                  contexts: Sequence[Any]) -> str:
    """The group's members as the dedupe prompt shows them: what the run concluded, then the
    context the run saw (the brief is context_text's, the investigation's own)."""
    parts: list[str] = []
    for index, (member, context) in enumerate(zip(members, contexts), 1):
        candidate = candidates[member.old]
        proposed = PROPOSED_PREFIX.match(str(candidate.get("summary") or ""))
        summary = candidate.get("summary")
        if proposed is not None and isinstance(summary, str):
            summary = summary[proposed.end():]
        lines = [f"Function {index} of {len(members)}: {member.old} at {member.address} "
                 f"(investigated in run {member.run_id})", f"- summary: {summary}"]
        if candidate.get("interface"):
            lines.append(f"- interface: {candidate['interface']}")
        if candidate.get("details"):
            lines.append(f"- details: {candidate['details']}")
        parts.append("\n".join(lines) + "\n\n" + inv.context_text(context))
    return "\n\n".join(parts)


def claimants_text(claimants: Sequence[SkippedRow]) -> str:
    """What else proposed the shared name (the prompt says, so the model knows why it is contested)."""
    if not claimants:
        return "No other function anywhere proposed this name."
    lines = [f"  {row.old} at {row.address} (run {row.run_id}: {row.reason})" for row in claimants]
    return "It was also proposed for (these are not renamed here):\n" + "\n".join(lines)


# ── the commands ──────────────────────────────────────────────────────

def style_fix() -> None:
    """The style rejects, decided from bn6f's own names: prefixes the project already spells
    with upper-case letters are allowed, everything else is normalized mechanically."""
    style_rows = [row for row in read_skipped() if row.reason.startswith(STYLE_REASON)]
    syms = elf_symbols()
    allowed = prefixes_in_use([*syms, *db_names()])
    fixes, stuck = style_fix_plan(style_rows, allowed, frozenset(syms) | db_names())
    print(f"style rejects in the finished runs: {len(style_rows)}; "
          f"mechanical fixes: {len(fixes)}, stuck: {len(stuck)}")
    print("prefixes in use (allowed): " + ", ".join(sorted(allowed)))
    print("30 before/after examples:")
    for row, fixed in fixes[:30]:
        print(f"  {row.old}  {row.proposed} -> {fixed}")
    if stuck:
        print(f"stuck ({len(stuck)}):")
        for row, why in stuck:
            print(f"  {row.old}  {row.proposed}: {why}")
    path = write_rescue_rows("style", [RescueRow(row.run_id, row.address, row.old, fixed, "style", "-")
                                       for row, fixed in fixes])
    print(f"rescued: {len(fixes)} rows -> {path}")


def run_dedupe(limit: int | None, only: str | None, dry: bool,
               proposer: str = PROPOSER, checker: str = CHECKER) -> None:
    """One PROPOSER call per group of candidates that share a proposed name, every member a
    distinct name; CHECKER then reads the naming against the code. Cached (resumable); agreed
    groups' rows go to build/investigate/rescue/dedupe.tsv. PROPOSER/CHECKER default to
    hyper/glm-5.3-flash and minimax/MiniMax-M3 — the same two models investigate used — and
    can be overridden with --model/--model-checker (e.g. while Hyper is rate-limited)."""
    sys.path.insert(0, os.path.join(TOOLS, "wf"))
    import wf.tools.investigate as inv  # noqa: PLC0415  (the context builder)
    rows = read_skipped()
    groups = dup_groups(rows)
    if only is not None:
        if only not in groups:
            sys.exit(f"no group for the name {only!r} (known: {', '.join(sorted(groups)[:10])}, ...)")
        chosen = [only]
    else:
        order = sorted(groups, key=lambda name: (len(groups[name]), name))
        chosen = order[:limit] if limit is not None else order
    if not chosen:
        print("no groups to dedupe")
        return
    syms = elf_symbols()
    taken = frozenset(syms) | db_names()
    allowed = prefixes_in_use([*syms, *taken])
    cache = os.path.join(ROOT, "build", "investigate", "dedupe")
    contexts_dir = Path(os.path.join(cache, "contexts"))
    labels = inv.label_index()
    summaries = inv.db_summaries()
    find = inv.funcindex.unit_finder(inv.funcindex.unit_ranges())
    units = inv.funcindex.lifted_units()
    neighbours = inv.call_graph() if inv.GRAPH.exists() else {}
    rescued: list[RescueRow] = []
    stopped: str | None = None
    print(f"groups: {len(groups)}, {sum(len(members) for members in groups.values())} members; "
          f"this pass: {len(chosen)}")
    for shared in chosen:
        members = groups[shared]
        candidates: dict[str, dict[str, Any]] = {}
        incomplete: list[str] = []
        for member in members:
            run_dir = os.path.join(ROOT, "build", "investigate", member.run_id)
            candidate = load_candidate(run_dir, member.old)
            if candidate is None:
                incomplete.append(member.old)
            else:
                candidates[member.old] = candidate
        if incomplete:
            print(f"  {shared}: skipped, no candidate yaml for {', '.join(sorted(incomplete))}")
            continue
        targets, missing = _targets(inv, members)
        if missing:
            print(f"  {shared}: skipped, no lifted unit for {', '.join(m.old for m in missing)}")
            continue
        contexts = [inv.gather_context(target, contexts_dir, labels, summaries, find, units, neighbours)
                    for target in targets]
        briefs = member_briefs(inv, members, candidates, contexts)
        claimants = other_claimants(rows, shared, members)
        if dry:
            print(DEDUPE_TASK.format(prefixes=", ".join(inv.name_prefixes()), shared=shared,
                                     shared_note=" (it is taken: a symbol of bn6f.elf or a "
                                                 "symbols/db entry)" if shared in taken else "",
                                     claimants=claimants_text(claimants), members=briefs))
            print(f"\n--- end of prompt for {shared} (dry run: no model called, use --group X --dry-run to inspect another) ---")
            return

        # the proposal: one call per group, asked again once when the reply cannot be used
        record = load_json(os.path.join(cache, f"{shared}.propose.json"))
        names: list[str] | None = None
        problem: str | None = None
        retries = 0
        if record is not None and isinstance(record.get("names"), list):
            names = [str(n) for n in record["names"]]
            problem = record.get("problem")
        else:
            prompt = DEDUPE_TASK.format(prefixes=", ".join(inv.name_prefixes()), shared=shared,
                                        shared_note=" (it is taken: a symbol of bn6f.elf or a "
                                                    "symbols/db entry)" if shared in taken else "",
                                        claimants=claimants_text(claimants), members=briefs)
            tokens = 0
            for attempt in range(2):
                tag = f"dedupe {shared} propose {attempt}"
                reply, ask_problem = ask_once(proposer, prompt, DEDUPE_SYSTEM, tag)
                tokens += inv.logged_tokens(inv.USAGE_LOG, tag)
                if ask_problem is None:
                    names, problem = parse_names(reply), None
                    if names is None:
                        problem = "the reply held no JSON object with a names list"
                else:
                    names, problem = None, ask_problem
                if problem is None:
                    break
                retries = attempt + 1
                prompt += (f"\n\nYour previous reply could not be used: {problem}\n"
                           "Reply again with only the JSON object, nothing else.")
            record = {"group": shared,
                      "members": [{"run": m.run_id, "old": m.old, "address": m.address}
                                                   for m in members],
                      "names": names, "problem": problem, "retries": retries, "tokens": tokens,
                      "seconds": None}
            save_json(os.path.join(cache, f"{shared}.propose.json"), record)
        if names is not None:
            problem = names_problem(names, members, shared, taken, allowed)
            if problem is not None:
                names = None
        if problem is not None:
            print(f"  {shared}: no names ({problem}); members: " + ", ".join(m.old for m in members))
            if problem.startswith("hung"):
                continue
            if inv.out_of_credits(problem):
                stopped = f"{shared}: {problem}"
                break
            continue
        assert names is not None

        # the check: the checker reads the group's naming against the code
        check = load_json(os.path.join(cache, f"{shared}.check.json"))
        if check is None or check.get("verdict") not in ("agree", "revise"):
            prompt = DEDUPE_CHECK_TASK.format(members=briefs, names="\n".join(f"  {n}" for n in names))
            verdict, reason, problem = None, None, None
            for attempt in range(2):
                tag = f"dedupe {shared} check {attempt}"
                reply, ask_problem = ask_once(checker, prompt, DEDUPE_CHECK_SYSTEM, tag)
                if ask_problem is None:
                    verdict, reason, problem = parse_group_check(reply)
                else:
                    verdict, reason, problem = None, None, ask_problem
                if problem is None:
                    break
                prompt += (f"\n\nYour previous reply could not be used: {problem}\n"
                           "Reply again with only the JSON object, nothing else.")
            save_json(os.path.join(cache, f"{shared}.check.json"),
                      {"group": shared, "verdict": verdict, "reason": reason, "problem": problem})
        else:
            verdict, reason, problem = check.get("verdict"), check.get("reason"), check.get("problem")
        if problem is not None:
            print(f"  {shared}: not checked ({problem}); members: " + ", ".join(m.old for m in members))
            if inv.out_of_credits(problem):
                stopped = f"{shared}: {problem}"
                break
            continue
        if verdict != "agree":
            print(f"  {shared}: dropped, the check revised ({reason}); members: "
                  + ", ".join(f"{m.old}->{n}" for m, n in zip(members, names)))
            continue
        for member, name in zip(members, names):
            rescued.append(RescueRow(member.run_id, member.address, member.old, name, "dedupe", shared))
        print(f"  {shared}: agreed ({reason}); members: "
              + ", ".join(f"{m.old}->{n}" for m, n in zip(members, names)))
    path = write_rescue_rows("dedupe", rescued)
    print(f"rescued: {len(rescued)} rows -> {path}")
    if stopped is not None:
        print(f"stopped: {stopped}")


# ── the reconcile pass: a second opinion on the checker's revised name ──

RECONCILE_SYSTEM = ("You give a second opinion on a function's name in a GBA game's disassembly "
                   "(MegaMan Battle Network 6 Falzar). Reply with exactly one JSON object and "
                   "nothing else.")
RECONCILE_JOBS = 8                       # model calls in flight
RECONCILE_TAG = "reconcile"
RECONCILE_PROMPT_TOKENS = 3_500          # the budget per call (a fit Hyper keeps warm)


class ReviseCandidate(NamedTuple):
    """One function the cross-check revised: which run, the function, what the check said."""
    run_id: str
    name: str                     # the placeholder, e.g. "sub_80C4766"
    address: str                  # "0x080C4766" (the placeholders live here, untouched)
    original_name: str            # the original proposer's name (the run's report.tsv proposed)
    original_summary: str         # the original proposer's one-line summary (no Proposed name: prefix)
    original_interface: str | None
    checker_reason: str           # why the checker revised (the reason field)
    revised_name: str             # the checker's proposed name
    revised_summary: str          # the checker's proposed one-line summary


def _read_revise_candidate(run_id: str, name: str) -> ReviseCandidate | None:
    """One checkers/NAME.json: the revise verdict's data, the original proposal's, the address."""
    check_path = os.path.join(ROOT, "build", "investigate", run_id, "checkers", f"{name}.json")
    proposal_path = os.path.join(ROOT, "build", "investigate", run_id, "proposals", f"{name}.json")
    if not (os.path.exists(check_path) and os.path.exists(proposal_path)):
        return None
    try:
        check = json.loads(open(check_path, encoding="utf-8").read())
        proposal = json.loads(open(proposal_path, encoding="utf-8").read())
    except (OSError, json.JSONDecodeError):
        return None
    reply = check.get("reply") if isinstance(check, dict) else None
    answer = proposal.get("answer") if isinstance(proposal, dict) else None
    if not isinstance(reply, dict) or reply.get("verdict") != "revise":
        return None
    if not isinstance(answer, dict):
        return None
    revised_name = reply.get("revised_name")
    if not isinstance(revised_name, str) or not revised_name.strip():
        return None
    original_name = answer.get("proposed_name")
    if not isinstance(original_name, str) or not original_name.strip():
        return None
    original_summary = str(answer.get("summary") or "")
    original_interface = answer.get("interface")
    return ReviseCandidate(run_id=run_id, name=name,
                           address=f"0x{check['address']:08X}" if isinstance(check.get("address"), int)
                                   else str(check.get("address") or ""),
                           original_name=original_name, original_summary=original_summary,
                           original_interface=str(original_interface) if original_interface else None,
                           checker_reason=str(reply.get("reason") or ""),
                           revised_name=revised_name,
                           revised_summary=str(reply.get("revised_summary") or ""))


def _collect_revise_candidates(base: str | None = None) -> list[ReviseCandidate]:
    """Every revise verdict across the runs that wrote a report.tsv (skipping *.failed-429) —
    even runs that were never landed, since their checkers/ is still there."""
    out: list[ReviseCandidate] = []
    for run_dir in any_run_dirs(base):
        run_id = os.path.basename(run_dir)
        checkers = os.path.join(run_dir, "checkers")
        if not os.path.isdir(checkers):
            continue
        for name in sorted(os.listdir(checkers)):
            if not name.endswith(".json"):
                continue
            candidate = _read_revise_candidate(run_id, name[:-5])
            if candidate is not None:
                out.append(candidate)
    return out


def _reconcile_filter(candidates: list[ReviseCandidate], syms: dict[str, int], names: set[str],
                      allowed: frozenset[str], rescued_new: set[str],
                      base: str | None = None) \
        -> tuple[list[ReviseCandidate], list[tuple[ReviseCandidate, str]]]:
    """The revise candidates worth a second opinion and the ones dropped, with why.

    Kept: still a placeholder, no symbols/db entry yet, no bn6f symbol under that name, has a
    candidate yaml in the run, the revised name passes the relaxed style rule and copies no
    part of the function's own address, the revised name is not taken in bn6f or symbols/db
    or any of the rescue lists, and the revised name is unique across all revise candidates
    (a shared revised name drops every claimant). BASE, when given, is the directory the
    candidate yaml and the contexts asm are looked up under (a test's temp dir)."""
    counts: dict[str, int] = {}
    for candidate in candidates:
        counts[candidate.revised_name] = counts.get(candidate.revised_name, 0) + 1
    base_dir = base or os.path.join(ROOT, "build", "investigate")
    keep: list[ReviseCandidate] = []
    dropped: list[tuple[ReviseCandidate, str]] = []
    for candidate in candidates:
        why: str | None
        if not placeholder(candidate.name):
            why = "the function is no longer a placeholder"
        elif candidate.name not in syms:
            why = "the placeholder name no longer exists in bn6f.elf"
        elif candidate.name in names:
            why = "the function already has a symbols/db entry"
        elif not os.path.exists(os.path.join(base_dir, candidate.run_id, f"{candidate.name}.yaml")):
            why = "no candidate yaml in the run folder"
        elif not os.path.exists(os.path.join(base_dir, candidate.run_id, "contexts",
                                            f"{candidate.name}.asm")):
            why = "no contexts/<name>.asm in the run folder"
        elif (problem := style_problem(candidate.revised_name, allowed)) is not None:
            why = f"naming style: {problem}"
        elif (problem := address_digit_problem(candidate.revised_name, candidate.address)) is not None:
            why = f"naming style: {problem}"
        elif candidate.revised_name in syms:
            why = "the revised name is already a symbol of bn6f.elf"
        elif candidate.revised_name in names:
            why = "the revised name is already a symbols/db entry name"
        elif candidate.revised_name in rescued_new:
            why = "the revised name is already in a rescue list"
        elif counts[candidate.revised_name] > 1:
            why = f"the revised name was proposed for {counts[candidate.revised_name]} functions"
        else:
            keep.append(candidate)
            continue
        dropped.append((candidate, why or ""))
    return keep, dropped


RECONCILE_TASK = """You are giving a second opinion on one function's name. The first model proposed
a name, and the cross-check model disagreed with that name and proposed its own. You see the
function's assembly and the surrounding context; you see BOTH the original proposal and the
checker's revised name, summary and objection. Decide which name to keep.

Read the assembly carefully. Cite specific asm lines or known callees/callers in your reason
so the decision is grounded. The decision is between three options:
- accept_revised: the checker's revised name says what THIS function does. The two models
  then agree on the revised name.
- keep_original: the original proposer's name says what THIS function does. The two models
  agree on the original name (the cross-check was wrong to revise).
- neither: both names miss what the function does (or one does and the other breaks the
  naming rules); reject both.

Naming rules the project enforces: lowerCamel after a prefix_, an allowed prefix list
(prefixes the project uses with upper-case letters: {prefixes}), no digit run copied from
the function's address, no "Maybe", no 0x literal.

The function (placeholder still in bn6f.elf, run {run_id}, score {score}):
{context}

The original proposal ({original_name}): {original_summary}
interface: {original_interface}

The cross-check objected: {checker_reason}
The cross-check's revised name: {revised_name}
The cross-check's revised summary: {revised_summary}

Reason briefly, then reply with exactly one JSON object and nothing else — no markdown
fence, no prose around it:
{{"decision": "accept_revised" | "keep_original" | "neither",
  "name": "<the name you are keeping, exactly as proposed>",
  "reason": "what in the code (asm lines, callers, callees) supports your decision, at most five sentences"}}
"""


def _reconcile_brief(candidate: ReviseCandidate, inv: Any, contexts_dir: Path,
                     labels: dict[str, tuple[Path, int]], summaries: dict[str, str],
                     find: Any, units: dict[str, Any],
                     neighbours: dict[int, tuple[frozenset[int], frozenset[int],
                                                tuple[tuple[int, int], ...]]]) -> str | None:
    """The compact context for one candidate: the assembly (capped tight), the call sites if
    the run had them, and what bn6f documents above the label. None when the function has no
    lifted unit (the filter also drops those, but defence in depth)."""
    address = int(candidate.address, 16)
    unit = inv.unit_of(candidate.name, address, units, find)
    if unit is None:
        return None
    target = inv.Target(name=candidate.name, address=address, score=0,
                        instructions=(unit.hi - unit.lo) // 2,
                        captured=(inv.SNAPSHOTS / candidate.name).is_dir(), routes=(),
                        named_neighbours=0, source=unit.asm or "")
    context = inv.gather_context(target, contexts_dir, labels, summaries, find, units, neighbours)
    if not context.assembly.exists():
        return None
    assembly = context.assembly.read_text(errors="replace")
    # The reconcile prompt budget is small; cap the assembly at ~1.5k tokens worth of lines.
    budget = 1_500 * 3
    out: list[str] = []
    used = 0
    for line in assembly.splitlines():
        if used + len(line) + 1 > budget:
            out.append(f"({len(assembly.splitlines()) - len(out)} lines cut)")
            break
        out.append(line)
        used += len(line) + 1
    head: list[str] = [f"{candidate.name} at {candidate.address}"]
    if context.comment:
        head.append(f"bn6f comment above it: {context.comment}")
    head.append("Assembly:")
    return "\n".join(head) + "\n```\n" + "\n".join(out) + "\n```"


def parse_reconcile(reply: str) -> tuple[str | None, str | None, str | None]:
    """(decision, name, reason) from the second-opinion reply, or (None, None, problem)."""
    parsed = json_object(reply)
    if parsed is None:
        return None, None, "the reply held no JSON object"
    decision = parsed.get("decision")
    if decision not in ("accept_revised", "keep_original", "neither"):
        return None, None, f"decision {decision!r} is not accept_revised, keep_original or neither"
    name = parsed.get("name")
    if not isinstance(name, str) or not name.strip():
        return None, None, "a decision without a name"
    reason = parsed.get("reason")
    if not isinstance(reason, str) or not reason.strip():
        return None, None, "a decision without a reason"
    return str(decision), name.strip(), " ".join(reason.split())


def _reconcile_targets(candidates: list[ReviseCandidate], inv: Any,
                       contexts_dir: Path, labels: dict[str, tuple[Path, int]],
                       summaries: dict[str, str], find: Any, units: dict[str, Any],
                       neighbours: dict[int, tuple[frozenset[int], frozenset[int],
                                                  tuple[tuple[int, int], ...]]]) \
        -> tuple[list[tuple[ReviseCandidate, str]], list[ReviseCandidate]]:
    """(candidate, brief) for every candidate, in candidates order; missing ones separately."""
    scores = {name: score for name, _address, score in inv.read_scores()}
    briefs: list[tuple[ReviseCandidate, str]] = []
    missing: list[ReviseCandidate] = []
    for candidate in candidates:
        brief = _reconcile_brief(candidate, inv, contexts_dir, labels, summaries, find, units, neighbours)
        if brief is None:
            missing.append(candidate)
            continue
        briefs.append((candidate, brief))
    _ = scores        # the score column is read by gather_context already; kept for completeness
    return briefs, missing


def _ask_reconcile_one(spec: str, candidate: ReviseCandidate, brief: str,
                       inv: Any, prefixes: tuple[str, ...]) -> dict[str, Any]:
    """One Hyper call, with one retry on an unusable reply (the run_dedupe pattern)."""
    prompt = RECONCILE_TASK.format(prefixes=", ".join(prefixes) if prefixes else "(none)",
                                   run_id=candidate.run_id, score="n/a",
                                   context=brief,
                                   original_name=candidate.original_name,
                                   original_summary=candidate.original_summary or "(none)",
                                   original_interface=candidate.original_interface or "(none)",
                                   checker_reason=candidate.checker_reason or "(none)",
                                   revised_name=candidate.revised_name,
                                   revised_summary=candidate.revised_summary or "(none)")
    decision, name, reason, problem, tokens = None, None, None, None, 0
    for attempt in range(2):
        tag = f"{RECONCILE_TAG} {candidate.name} {attempt}"
        reply, ask_problem = ask_once(spec, prompt, RECONCILE_SYSTEM, tag)
        tokens += inv.logged_tokens(inv.USAGE_LOG, tag)
        if ask_problem is None:
            decision, name, reason = parse_reconcile(reply)
            if decision is None:
                problem = reason
                reason = None
            else:
                problem = None
        else:
            decision, name, problem = None, None, ask_problem
        if problem is None:
            break
        prompt += (f"\n\nYour previous reply could not be used: {problem}\n"
                   "Reply again with only the JSON object, nothing else.")
    return {"candidate": candidate.name, "run": candidate.run_id, "address": candidate.address,
            "original_name": candidate.original_name, "revised_name": candidate.revised_name,
            "decision": decision, "name": name, "reason": reason, "problem": problem,
            "tokens": tokens, "retries": attempt}


def _run_reconcile_pass(spec: str, work: list[tuple[ReviseCandidate, str]],
                        inv: Any, prefixes: tuple[str, ...]) -> list[dict[str, Any]]:
    """Run one worker per item, at most RECONCILE_JOBS at a time; a 402 or daily limit stops
    the pass cleanly; hung calls return None (the cache tells the run what was already done)."""
    results: list[dict[str, Any] | None] = [None] * len(work)
    state = {"next": 0, "stopped": False}
    lock = threading.Lock()

    def worker() -> None:
        while True:
            with lock:
                if state["stopped"]:
                    return
                index = state["next"]
                if index >= len(work):
                    return
                state["next"] = index + 1
            candidate, brief = work[index]
            try:
                out = _ask_reconcile_one(spec, candidate, brief, inv, prefixes)
            except Exception as error:
                out = {"candidate": candidate.name, "run": candidate.run_id,
                       "address": candidate.address, "original_name": candidate.original_name,
                       "revised_name": candidate.revised_name, "decision": None, "name": None,
                       "problem": f"the request itself failed: {error}", "tokens": 0, "retries": 0}
            with lock:
                results[index] = out
                problem = out.get("problem")
                if problem and ("HTTP 402" in problem or "daily rate limit" in problem):
                    state["stopped"] = True

    threads = [threading.Thread(target=worker, daemon=True) for _ in range(min(RECONCILE_JOBS, len(work)))]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    return [r if r is not None else
            {"candidate": c.name, "run": c.run_id, "address": c.address,
             "original_name": c.original_name, "revised_name": c.revised_name,
             "decision": None, "name": None, "problem": "abandoned: the pass stopped cleanly",
             "tokens": 0, "retries": 0}
            for (c, _), r in zip(work, results)]


def run_reconcile(limit: int | None) -> None:
    """One Hyper glm-5.3-flash call per revise candidate (8 workers, resumable cache, stops
    cleanly on HTTP 402 or a daily rate limit). Only `accept_revised` (name == the checker's
    revised_name) counts as agreement; both models then back the revised name. Agreed
    functions go to build/investigate/rescue/reconcile.tsv."""
    sys.path.insert(0, os.path.join(TOOLS, "wf"))
    import wf.tools.investigate as inv  # noqa: PLC0415  (the context builder)
    syms = elf_symbols()
    names = db_names()
    allowed = prefixes_in_use([*syms, *names])
    rescued_new: set[str] = set()
    for row in read_rescue_rows("style"):
        rescued_new.add(row.new)
    for row in read_rescue_rows("dedupe"):
        rescued_new.add(row.new)
    all_revise = _collect_revise_candidates()
    kept, dropped = _reconcile_filter(all_revise, syms, names, allowed, rescued_new)
    dropped_summary: dict[str, int] = {}
    for _, why in dropped:
        dropped_summary[why] = dropped_summary.get(why, 0) + 1
    print(f"revise candidates: {len(all_revise)} total, {len(dropped)} dropped, {len(kept)} kept")
    if dropped:
        for why, count in sorted(dropped_summary.items(), key=lambda pair: (-pair[1], pair[0])):
            print(f"  dropped {count}: {why}")
    if not kept:
        print("nothing to reconcile")
        return
    if limit is not None:
        kept = kept[:limit]
    contexts_dir = Path(os.path.join(ROOT, "build", "investigate", "reconcile", "contexts"))
    labels = inv.label_index()
    summaries = inv.db_summaries()
    find = inv.funcindex.unit_finder(inv.funcindex.unit_ranges())
    units = inv.funcindex.lifted_units()
    neighbours = inv.call_graph() if inv.GRAPH.exists() else {}
    prefixes = inv.name_prefixes()
    briefs, missing = _reconcile_targets(kept, inv, contexts_dir, labels, summaries, find, units,
                                         neighbours)
    if missing:
        print(f"no assembly context for: " + ", ".join(f"{m.name} ({m.run_id})" for m in missing))
        kept = [c for c in kept if c not in missing]
    if not kept:
        print("nothing to reconcile after the context check")
        return
    cache = os.path.join(ROOT, "build", "investigate", "reconcile")
    os.makedirs(cache, exist_ok=True)
    rescued: list[RescueRow] = []
    accepted = kept_original = neither = 0
    examples: list[tuple[ReviseCandidate, str, str, str, str]] = []
    print(f"asking {RECONCILE_JOBS} workers in parallel for {len(kept)} candidates...")
    work = list(zip(kept, briefs))
    work = [(c, b) for c, b in work if load_json(os.path.join(cache, f"{c.name}.json")) is None]
    if not work:
        print(f"all {len(kept)} cached; re-aggregating")
    else:
        results = _run_reconcile_pass(PROPOSER, work, inv, prefixes)
        for result in results:
            candidate_name = result["candidate"]
            save_json(os.path.join(cache, f"{candidate_name}.json"), result)
    for candidate in kept:
        record = load_json(os.path.join(cache, f"{candidate.name}.json"))
        if record is None:
            print(f"  {candidate.name}: no record (the worker was stopped or failed)")
            continue
        decision = record.get("decision")
        name = record.get("name")
        problem = record.get("problem")
        if problem is not None:
            print(f"  {candidate.name} ({candidate.run_id}): {problem}")
            continue
        # Only accept_revised counts as agreement (the second opinion backs the checker's name).
        # keep_original means the second opinion disagrees with the checker (not agreement);
        # neither means both names miss the function.
        if decision == "accept_revised" and name == candidate.revised_name:
            rescued.append(RescueRow(candidate.run_id, candidate.address, candidate.name,
                                     candidate.revised_name, "reconcile", "-"))
            accepted += 1
            outcome = "accept_revised"
        elif decision == "keep_original" and name == candidate.original_name:
            kept_original += 1
            outcome = "keep_original"
        else:
            neither += 1
            outcome = "neither"
        if len(examples) < 8:
            examples.append((candidate, candidate.original_name, candidate.revised_name, outcome,
                             record.get("reason") or ""))
    path = write_rescue_rows("reconcile", rescued)
    print(f"rescued: {len(rescued)} rows (agreement, accept_revised) -> {path}")
    print(f"decisions: {accepted} accept_revised, {kept_original} keep_original, {neither} neither, "
          f"{len(kept) - accepted - kept_original - neither} unparsed")
    print("8 examples (placeholder, original, revised, decision, reason):")
    for candidate, original, revised, dec, reason in examples:
        print(f"  {candidate.name}  {original}  {revised}  {dec}  {reason[:120]}")


_PORT_IDENTIFIERS: frozenset[str] | None = None


def port_identifiers() -> frozenset[str]:
    """Every identifier in the port's own C (cport/src, cport/host, cport/include except the generated
    lifted.h). A rename to one of them breaks the port build, which only the record run's verify sees."""
    global _PORT_IDENTIFIERS
    if _PORT_IDENTIFIERS is None:
        import glob
        token = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
        found: set[str] = set()
        for pattern in ("cport/src/game/*.c", "cport/src/*.c", "cport/host/*.c", "cport/include/*.h"):
            for path in glob.glob(os.path.join(ROOT, pattern)):
                if path.endswith("lifted.h"):
                    continue
                with open(path, errors="replace") as handle:
                    found.update(token.findall(handle.read()))
        _PORT_IDENTIFIERS = frozenset(found)
    return _PORT_IDENTIFIERS


def land_rescued(limit: int | None, dry: bool, batch_size: int) -> None:
    """The rescued candidates into symbols/db and bn6f, the way land does, the record workflow
    once per batch of at most BATCH_SIZE. A record failure is reported, never retried. Reads
    three lists (style, dedupe, reconcile); the rescue named in the evidence follows the kind."""
    merged = merge_round_robin(merge_round_robin(read_rescue_rows("style"), read_rescue_rows("dedupe")),
                              read_rescue_rows("reconcile"))
    if not merged:
        sys.exit("no rescued rows: run style-fix, dedupe and reconcile first")
    syms = elf_symbols()
    names = db_names()
    allowed = prefixes_in_use([*syms, *names])
    keep: list[tuple[RescueRow, dict[str, Any]]] = []
    seen_new: set[str] = set()
    for row in merged:
        run_dir = os.path.join(ROOT, "build", "investigate", row.run_id)
        candidate = load_candidate(run_dir, row.old)
        why: str | None
        if candidate is None:
            why = "no candidate yaml in the run folder"
        elif row.old in names:
            why = "the function already has a symbols/db entry"
        elif row.new == row.old:
            why = "the new name is the old placeholder"
        elif os.path.exists(os.path.join(ROOT, "bn6f", "asm", f"{row.old}.s")):
            why = "its name is also an asm include file: the rename would rewrite the include path, not the file"
        elif row.new in syms or row.new in names or row.new in seen_new:
            why = f"the name {row.new} is already taken"
        elif row.new in port_identifiers():
            why = f"the port's own C already uses the name {row.new} (record-20261019-225651 failed on start_cutscene)"
        elif (problem := style_problem(row.new, allowed)) is not None:
            why = f"naming style: {problem}"
        elif (problem := address_digit_problem(row.new, row.address)) is not None:
            why = problem
        else:
            seen_new.add(row.new)
            keep.append((row, candidate))
            continue
        print(f"  dropped {row.old} -> {row.new}: {why}")
    if limit is not None:
        keep = keep[:limit]
    if not keep:
        sys.exit("nothing left to land")
    print(f"rescued rows: {len(merged)} read, {len(keep)} to land"
          + (f" (limit {limit})" if limit is not None else ""))
    for row, candidate in keep:
        print(f"  {row.address} {row.old} -> {row.new} ({row.how}, run {row.run_id})")
    if dry:
        print("dry run: nothing written to symbols/db, no record run")
        return
    rescue_run_dir = os.path.join(ROOT, "build", "investigate", "land-rescue")
    os.makedirs(rescue_run_dir, exist_ok=True)
    for number, batch in enumerate(batches(keep, batch_size), 1):
        pairs: list[tuple[str, str]] = []
        for row, candidate in batch:
            rescue = _rescue_evidence(row)
            path = write_entry(candidate, row.run_id, rescue)
            problems = symdb_entry_problems(path)
            if problems:
                sys.exit(f"{path}: tools/symdb.py rejects the entry:\n  " + "\n  ".join(problems))
            pairs.append((row.old, row.new))
            print(f"  {row.address} {row.old} -> {row.new}: entry written and checked")
        ok, problems = symdb_check_all()
        if not ok:
            sys.exit("tools/symdb.py check fails on the database; the record run would abort:\n"
                     + "\n".join(problems))
        run_ids = sorted({row.run_id for row, _ in batch})
        found_by = f"wf investigate {run_ids[0]}" if len(run_ids) == 1 \
            else f"wf investigate runs {', '.join(run_ids)}"
        log_path = os.path.join(rescue_run_dir, f"land_record-{number}.log")
        report = run_record(pairs, found_by, rescue_run_dir, log_path)
        print(f"batch {number}: record {report['verdict']}, commit {report.get('commit')}, "
              f"bn6f renames {report.get('renamed_in_bn6f')}, comments {report.get('comments_in_bn6f')}, "
              f"port ROM unchanged: {report.get('port_rom_unchanged')}")
        check_afterwards(report, log_path)
        names = db_names()      # the record run renamed the entries just landed


def _rescue_evidence(row: RescueRow) -> str:
    """The first evidence item's result text for a rescued row, naming the kind of rescue."""
    if row.how == "style":
        return ("its name was fixed mechanically by tools/investigate_land.py style-fix "
                "(the run's proposal broke the naming style)")
    if row.how == "dedupe":
        return (f"the run's proposed name {row.group} was contested; tools/investigate_land.py "
                f"dedupe gave this distinct name (hyper/glm-5.3-flash proposed, "
                f"minimax/MiniMax-M3 agreed)")
    return (f"proposed by Hyper glm-5.3-flash, revised by MiniMax-M3, "
            f"accepted by glm-5.3-flash (tools/investigate_land.py reconcile)")


# ── main ───────────────────────────────────────────────────────────────

def land(run_dir: str, count: int, dry: bool) -> None:
    run_id = os.path.basename(os.path.normpath(run_dir))
    rows = read_report(run_dir)
    syms = elf_symbols()
    code = text_symbols(syms)
    allowed = prefixes_in_use([*syms, *db_names()])
    chosen, skipped = select(rows, run_dir, syms, code, count, allowed)
    with open(os.path.join(run_dir, "land_skipped.tsv"), "w", encoding="utf-8") as f:
        f.write("# address\told\tproposed\treason\n")
        for address, old, proposed, reason in skipped:
            f.write(f"{address}\t{old}\t{proposed}\t{reason}\n")
    print(f"candidates: {len(rows)} rows, {sum(1 for r in rows if r.status == 'probable' and r.check == 'agree')} "
          f"probable+agree; {len(skipped)} dropped or skipped (land_skipped.tsv); taking {len(chosen)}")
    pairs: list[tuple[str, str]] = []
    for row, candidate in chosen:
        proposed = PROPOSED_PREFIX.match(str(candidate.get("summary") or ""))
        summary_name = proposed.group(1) if proposed else None
        if summary_name != row.proposed:
            sys.exit(f"{row.old}: the candidate's proposed name ({summary_name}) is not the report's "
                     f"({row.proposed})")
        if dry:
            print(f"  {row.address} {row.old} -> {row.proposed} (score {row.score})")
            continue
        path = write_entry(candidate, run_id)
        problems = symdb_entry_problems(path)
        if problems:
            sys.exit(f"{path}: tools/symdb.py rejects the entry:\n  " + "\n  ".join(problems))
        pairs.append((row.old, row.proposed))
        print(f"  {row.address} {row.old} -> {row.proposed} (score {row.score}): entry written and checked")
    if dry:
        print("dry run: nothing written to symbols/db, no record run")
        return
    ok, problems = symdb_check_all()
    if not ok:
        sys.exit("tools/symdb.py check fails on the database; the record run would abort:\n"
                 + "\n".join(problems))
    log_path = os.path.join(run_dir, "land_record.log")
    report = run_record(pairs, f"wf investigate {run_id}", run_dir, log_path)
    print(f"record: {report['verdict']}, commit {report.get('commit')}, "
          f"bn6f renames {report.get('renamed_in_bn6f')}, comments {report.get('comments_in_bn6f')}, "
          f"port ROM unchanged: {report.get('port_rom_unchanged')}")
    check_afterwards(report, log_path)


def selfcheck() -> None:
    """The tool's own logic, checked (no game, no builds, no models)."""
    import tempfile
    bad: list[str] = []

    def eq(what: str, got: object, want: object) -> None:
        if got != want:
            bad.append(f"{what}: got {got!r}, want {want!r}")

    eq("style: prefix and lowerCamel", style_problem("sprite_queueOamEntry"), None)
    eq("style: plain lowerCamel", style_problem("setBGTileIds"), None)
    eq("style: prefix with a digit", style_problem("mp3_decode"), None)
    eq("style: two underscores", style_problem("ai_attackStep_CommitMoveAndHit"),
       "more than one underscore")
    eq("style: upper in the prefix", style_problem("battleObject_initVisualsFromRelatedObject"),
       "the prefix 'battleObject' is not lower-case")
    eq("style: no prefix, underscore first", style_problem("_init"), "the prefix '' is not lower-case")
    eq("style: upper start after prefix", style_problem("ai_InitVars"), "'InitVars' after the prefix is not lowerCamel")
    eq("style: upper start, no prefix", style_problem("InitVars"), "it is not lowerCamel")
    eq("style: 0x literal", style_problem("t3_0x65_init"), "it carries a 0x literal")
    eq("style: trailing address run", style_problem("handler_80C4766"),
       "it ends in a digit run copied from the placeholder's address")
    eq("style: Maybe hedges", style_problem("maybeLoadSprite"), "it hedges (Maybe)")

    eq("digits: the address's own run", address_digit_problem("init_80BFFC0", "0x080BFFC0"),
       "the digit run 80BFFC0 is copied from the address 0x080BFFC0")
    eq("digits: a slice of the address counts from three on",
       address_digit_problem("loadFFC", "0x080BFFC0"),
       "the digit run FFC is copied from the address 0x080BFFC0")
    eq("digits: a run the address does not contain stays",
       address_digit_problem("loadFF1", "0x080BFFC0"), None)
    eq("digits: a two-digit coincidence stays", address_digit_problem("load80", "0x080BFFC0"), None)
    eq("digits: digits the address does not have stay", address_digit_problem("step4Handler", "0x080BFFC0"), None)
    eq("digits: mixed hex letters", address_digit_problem("readBFF", "0x080BFFC0"),
       "the digit run BFF is copied from the address 0x080BFFC0")

    eq("placeholder: sub_", placeholder("sub_80BFFC0"), True)
    eq("placeholder: asm copy", placeholder("asm38"), True)
    eq("placeholder: named", placeholder("sprite_queueOamEntry"), False)

    code = [(0x100, "sub_80BFFC0"), (0x104, "namedOne"), (0x108, "sub_80C4766"), (0x10C, "namedTwo"),
            (0x110, "sub_80D0000"), (0x114, "nullsub_12"), (0x118, "namedThree"), (0x11C, "sub_80D0004")]
    eq("neighbours: around 0x104 one named symbol counts (namedTwo; the placeholders do not)",
       named_neighbours(0x104, code), 1)
    eq("neighbours: around the first, the two named to the right count",
       named_neighbours(0x100, code), 2)
    eq("neighbours: around the last, namedTwo and namedThree count",
       named_neighbours(0x11C, code), 2)
    eq("neighbours: between two symbols both sides count",
       named_neighbours(0x106, code), 3)

    rows = [Row("0x08000002\tsub_A\tbad__name\tprobable\t5\tagree\t1\t0"),
            Row("0x08000004\tsub_B\tdupName\tprobable\t5\tagree\t1\t0"),
            Row("0x08000006\tsub_C\tdupName\thypothesis\t5\tagree\t1\t0"),
            Row("0x08000008\tsub_D\tlowScore\tprobable\t3\tagree\t1\t0"),
            Row("0x08000100\tsub_E\ttieNamed\tprobable\t4\tagree\t1\t0"),
            Row("0x08000104\tsub_F\ttieBare\tprobable\t4\tagree\t1\t0"),
            Row("0x0800000E\tsub_G\tanyName\thypothesis\t5\tdisagree\t1\t0"),
            Row("0x08000010\tsub_H\t-\thypothesis\t5\tnone\t1\t0")]
    with tempfile.TemporaryDirectory() as run_dir:
        for old in ("sub_A", "sub_B", "sub_C", "sub_D", "sub_E", "sub_F"):
            with open(os.path.join(run_dir, f"{old}.yaml"), "w", encoding="utf-8") as f:
                yaml.safe_dump({"name": old, "address": "0x08000000", "kind": "function", "status": "probable",
                                "summary": f"Proposed name: x. Something."},
                               f, sort_keys=False)
        chosen, skipped = select(rows, run_dir, {}, code, 10)
        eq("select: the style break, the duplicate (both rows), the hypothesis and the no-name stay out",
           sorted(r.old for r, _ in chosen), ["sub_D", "sub_E", "sub_F"])
        reasons = {old: reason for _, old, _, reason in skipped}
        eq("select: the duplicate is named once", reasons.get("sub_B"), "proposed for 2 functions in the run")
        eq("select: the style break's reason", reasons.get("sub_A", "").startswith("naming style:"), True)
        eq("select: a hypothesis is not reported as dropped", "sub_G" in reasons, False)
        eq("select: ties go to the more named neighbours",
           [r.old for r, _ in chosen][:2], ["sub_E", "sub_F"])
        chosen, _ = select(rows, run_dir, {}, code, 2)
        eq("select: --count cuts", [r.old for r, _ in chosen], ["sub_E", "sub_F"])

        candidate = {"name": "sub_3006F8C", "address": "0x03006F8C", "kind": "function", "status": "probable",
                     "summary": "Proposed name: chatbox_blitGlyph. Blits a glyph.",
                     "interface": "(r1 = index) -> r0", "details": "Reads a table.",
                     "evidence": [{"method": "assembly-reading", "date": "2026-10-18", "detail": "callers",
                                   "refs": ["bn6f/asm/asm38.s:2934"],
                                   "reproduce": "python3 tools/asmcat.py 0x03006F8C",
                                   "result": "proposed by hyper/glm-5.3-flash, self_score 4; "
                                             "cross-checked by minimax/MiniMax-M3: agree"}],
                     "coverage": ["battle"], "open_questions": ["the buffer's layout"],
                     "sources": ["wf investigate investigate-20261018-155607", "models"]}
        entry = entry_of(candidate, "investigate-20261018-155607")
        eq("entry: the name is the current placeholder", entry["name"], "sub_3006F8C")
        eq("entry: probable", entry["status"], "probable")
        eq("entry: the summary without the Proposed name: prefix", entry["summary"], "Blits a glyph.")
        eq("entry: previous_names wait for the record run's rename", "previous_names" in entry, False)
        eq("entry: the run id is in the evidence's result",
           "run wf investigate investigate-20261018-155607" in entry["evidence"][0]["result"], True)
        eq("entry: both models kept",
           "hyper/glm-5.3-flash" in entry["evidence"][0]["result"]
           and "minimax/MiniMax-M3" in entry["evidence"][0]["result"], True)
        eq("entry: sources name the run", entry["sources"], ["wf investigate investigate-20261018-155607"])
        eq("entry: the rest is kept", (entry["interface"], entry["details"], entry["coverage"],
                                       entry["open_questions"]),
           ("(r1 = index) -> r0", "Reads a table.", ["battle"], ["the buffer's layout"]))
        twice = entry_of(candidate, "investigate-20261018-155607")
        eq("entry: writing it twice changes nothing", entry_of(candidate, "investigate-20261018-155607"), twice)
        rescued = entry_of(candidate, "investigate-20261018-155607",
                           rescue="deduped by tools/investigate_land.py dedupe")
        eq("entry: the rescue is named in the evidence",
           "deduped by tools/investigate_land.py dedupe" in rescued["evidence"][0]["result"], True)
        eq("entry: the rescue leaves the rest alone",
           {k: v for k, v in rescued.items() if k != "evidence"},
           {k: v for k, v in entry.items() if k != "evidence"})

    # ── the rescues (style-fix, dedupe, land-rescued) ──
    eq("prefixes: a camel prefix bn6f uses counts", "battleObject" in
       prefixes_in_use(["battleObject_runPhase", "sprite_queueOamEntry", "sub_1"]), True)
    eq("prefixes: an unused camel prefix stays out",
       "writeChipParams" in prefixes_in_use(["sprite_queueOamEntry"]), False)
    eq("prefixes: a lower prefix needs no allowance", "sprite" in prefixes_in_use(["sprite_x"]), False)

    relax = frozenset({"battleObject"})
    eq("fix: the two-underscore example", style_fixed("ai_attackStep_CommitMoveAndHit", relax),
       "ai_attackStepCommitMoveAndHit")
    eq("fix: an acronym folds", style_fixed("rfu_UNI_processRecvChunk", relax), "rfu_uniProcessRecvChunk")
    eq("fix: an acronym late in the name capitalises", style_fixed("chatbox_step_UNI_end", relax),
       "chatbox_stepUniEnd")
    eq("fix: an allowed prefix stays", style_fixed("battleObject_initVisualsFromRelatedObject", relax),
       "battleObject_initVisualsFromRelatedObject")
    eq("fix: an unused camel prefix folds in", style_fixed("writeChipParams_chip74", relax),
       "writeChipParamsChip74")
    eq("fix: an upper start after the prefix lowers", style_fixed("battle_EndRelatedObjectAnims", relax),
       "battle_endRelatedObjectAnims")
    eq("fix: a plain name lowers", style_fixed("GiveAllChips", relax), "giveAllChips")
    eq("fix: a digit-leading rest folds in", style_fixed("copy_32ByteEntriesZeroTerminated", relax),
       "copy32ByteEntriesZeroTerminated")
    eq("fix: three underscores", style_fixed("chatbox_step_runScript12_ifFlag80Clear", relax),
       "chatbox_stepRunScript12IfFlag80Clear")
    eq("fix: the fixed name passes the rule it was fixed for",
       style_problem(style_fixed("ai_attackStep_CommitMoveAndHit", relax), relax), None)
    eq("fix: a Maybe stays out", style_fixed("maybeLoadSprite_x", relax), None)
    eq("fix: a 0x literal stays out", style_fixed("t3_0x65_init", relax), None)

    style_rows = [SkippedRow("r1", "0x08000000", "sub_A", "ai_attackStep_CommitMoveAndHit",
                             "naming style: more than one underscore"),
                  SkippedRow("r1", "0x08000004", "sub_B", "battleObject_initVisualsFromRelatedObject",
                             "naming style: the prefix 'battleObject' is not lower-case"),
                  SkippedRow("r1", "0x08000008", "sub_C", "maybeLoadSprite",
                             "naming style: it hedges (Maybe)"),
                  SkippedRow("r1", "0x0800000C", "sub_D", "handler_80C4766",
                             "naming style: it ends in a digit run copied from the placeholder's address")]
    fixes, stuck = style_fix_plan(style_rows, relax)
    eq("plan: the two mechanical fixes", [(row.old, fixed) for row, fixed in fixes],
       [("sub_A", "ai_attackStepCommitMoveAndHit"),
        ("sub_B", "battleObject_initVisualsFromRelatedObject")])
    eq("plan: the Maybe and the address copy stay out", [row.old for row, _ in stuck], ["sub_C", "sub_D"])
    fixes, stuck = style_fix_plan(
        [SkippedRow("r", "0x08000000", "sub_A", "ai_x_y", "naming style: more than one underscore"),
         SkippedRow("r", "0x08000004", "sub_B", "ai_x_y", "naming style: more than one underscore")],
        frozenset())
    eq("plan: two fixes of one name collide", [row.old for row, _ in fixes], ["sub_A"])
    eq("plan: the collision's reason", stuck[0][1], "the fix collides with sub_A at 0x08000000")
    fixes, _ = style_fix_plan(
        [SkippedRow("r", "0x08000000", "sub_A", "battle_runPhase", "naming style: x")], frozenset(),
        taken=frozenset({"battle_runPhase"}))
    eq("plan: a fix onto a taken name stays out", fixes, [])

    skip_rows = [SkippedRow("r1", "0x1", "sub_A", "dupName", "proposed for 2 functions in the run"),
                 SkippedRow("r2", "0x2", "sub_B", "dupName", "proposed for 2 functions in the run"),
                 SkippedRow("r2", "0x3", "sub_C", "dupName", TAKEN_ELF),
                 SkippedRow("r3", "0x4", "sub_D", "other", "the function already has a symbols/db entry")]
    groups = dup_groups(skip_rows)
    eq("groups: two runs' rows in one group", [row.old for row in groups["dupName"]], ["sub_A", "sub_B"])
    eq("groups: a function that already has an entry forms no group", "other" in groups, False)
    eq("claimants: the elf-symbol sibling is one",
       [row.old for row in other_claimants(skip_rows, "dupName", groups["dupName"])], ["sub_C"])

    with tempfile.TemporaryDirectory() as base:
        for name in ("investigate-a", "investigate-b.failed-429", "investigate-c"):
            os.makedirs(os.path.join(base, name), exist_ok=True)
        for name, files in (("investigate-a", ("report.tsv", "land_skipped.tsv")),
                            ("investigate-c", ("report.tsv",))):
            for name_of_file in files:
                open(os.path.join(base, name, name_of_file), "w").close()
        with open(os.path.join(base, "investigate-a", "land_skipped.tsv"), "w", encoding="utf-8") as f:
            f.write("# address\told\tproposed\treason\n")
            f.write("0x1\tsub_A\tname\tproposed for 2 functions in the run\n")
        eq("runs: only the finished ones (the failed-429 and the reportless out)",
           [os.path.basename(d) for d in finished_run_dirs(base)], ["investigate-a"])
        eq("skipped: the one row", [(row.old, row.proposed) for row in read_skipped(base)],
           [("sub_A", "name")])

    members = [SkippedRow("r", "0x080BFFC0", "sub_A", "dup", "proposed for 2 functions in the run"),
               SkippedRow("r", "0x080BFFC4", "sub_B", "dup", "proposed for 2 functions in the run")]
    eq("names: two distinct names pass",
       names_problem(["ai_attackWindup", "ai_attackStrike"], members, "dup", frozenset(), frozenset()), None)
    eq("names: a repeat refused",
       names_problem(["ai_x", "ai_x"], members, "dup", frozenset(), frozenset()), "a name is proposed twice")
    eq("names: the shared name again refused",
       names_problem(["dup", "ai_strike"], members, "dup", frozenset(), frozenset()),
       "dup repeats the shared name")
    eq("names: a taken name refused",
       names_problem(["ai_x", "battle_runPhase"], members, "dup", frozenset({"battle_runPhase"}), frozenset()),
       "battle_runPhase is already taken in bn6f")
    eq("names: a placeholder refused",
       names_problem(["ai_x", "sub_80BFFC4"], members, "dup", frozenset(), frozenset()),
       "sub_80BFFC4 is still a placeholder")
    eq("names: an address copy refused",
       names_problem(["ai_x", "ai_loadFFC"], members, "dup", frozenset(), frozenset()),
       "ai_loadFFC: the digit run FFC is copied from the address 0x080BFFC4")
    eq("names: a wrong count refused", names_problem(["ai_x"], members, "dup", frozenset(), frozenset()),
       "1 names for 2 functions")
    eq("names: an allowed camel prefix passes",
       names_problem(["battleObject_windup", "ai_strike"], members, "dup", frozenset(), relax), None)
    eq("names: an unused camel prefix refused",
       names_problem(["writeChipParams_x", "ai_strike"], members, "dup", frozenset(), frozenset()),
       "writeChipParams_x: the prefix 'writeChipParams' is not lower-case")

    eq("dedupe reply: the names list", parse_names('{"names": ["a", "b"]}'), ["a", "b"])
    eq("dedupe reply: fenced", parse_names('```json\n{"names": ["a"]}\n```'), ["a"])
    eq("dedupe reply: no json", parse_names("no json here"), None)
    eq("dedupe reply: not a list", parse_names('{"names": "a"}'), None)
    eq("check: agree", parse_group_check('{"verdict": "agree", "reason": "the names fit"}'),
       ("agree", "the names fit", None))
    eq("check: revise", parse_group_check('{"verdict": "revise", "reason": "the second does not"}')[0],
       "revise")
    eq("check: a junk verdict", parse_group_check('{"verdict": "meh", "reason": "x"}')[2],
       "verdict 'meh' is not agree or revise")
    eq("check: no reason", parse_group_check('{"verdict": "agree"}')[2], "a verdict without a reason")

    eq("merge: round robin alternates the kinds",
       [row.new for row in merge_round_robin(
           [RescueRow("r", "0x1", "a", "s1", "style", "-"), RescueRow("r", "0x2", "c", "s2", "style", "-")],
           [RescueRow("r", "0x3", "b", "d1", "dedupe", "g")])], ["s1", "d1", "s2"])
    eq("batches: five in twos", [len(b) for b in batches(list(range(5)), 2)], [2, 2, 1])

    # ── the reconcile pass ──
    eq("reconcile reply: accept_revised", parse_reconcile(
        '{"decision": "accept_revised", "name": "ai_attackStep", '
        '"reason": "the code at lines 4-7 confirms it"}'),
       ("accept_revised", "ai_attackStep", "the code at lines 4-7 confirms it"))
    eq("reconcile reply: keep_original", parse_reconcile(
        '{"decision": "keep_original", "name": "ai_x", "reason": "ok"}')[0], "keep_original")
    eq("reconcile reply: neither", parse_reconcile(
        '{"decision": "neither", "name": "ai_x", "reason": "both miss it"}')[0], "neither")
    eq("reconcile reply: fenced", parse_reconcile(
        '```json\n{"decision": "keep_original", "name": "ai_x", "reason": "ok"}\n```')[0],
       "keep_original")
    eq("reconcile reply: no json", parse_reconcile("no json here")[2],
       "the reply held no JSON object")
    eq("reconcile reply: bad decision", parse_reconcile(
        '{"decision": "agree", "name": "x", "reason": "y"}')[2],
       "decision 'agree' is not accept_revised, keep_original or neither")
    eq("reconcile reply: no name", parse_reconcile(
        '{"decision": "accept_revised", "reason": "ok"}')[2], "a decision without a name")
    eq("reconcile reply: no reason", parse_reconcile(
        '{"decision": "accept_revised", "name": "x"}')[2], "a decision without a reason")

    # the filter: a revise candidate that fails every rule, and one that passes them all
    with tempfile.TemporaryDirectory() as filter_base:
        # every test candidate has its candidate yaml and contexts asm laid down here
        for run_id, name in (("r", "sub_801DEEE"), ("r", "sub_801E000")):
            run_dir = os.path.join(filter_base, run_id)
            os.makedirs(os.path.join(run_dir, "contexts"), exist_ok=True)
            with open(os.path.join(run_dir, f"{name}.yaml"), "w") as f:
                f.write("name: probe\n")
            with open(os.path.join(run_dir, "contexts", f"{name}.asm"), "w") as f:
                f.write("\tpush {lr}\n")
        # the elf has both placeholders (no renames yet)
        elf = {"sub_801DEEE": 0x0801DEEE, "sub_801E000": 0x0801E000}
        good = ReviseCandidate(run_id="r", name="sub_801DEEE", address="0x0801DEEE",
                               original_name="battle_x", original_summary="summary",
                               original_interface=None, checker_reason="the original misidentifies X",
                               revised_name="battle_dispatchX", revised_summary="a real dispatch")
        bad_style = ReviseCandidate(run_id="r", name="sub_801DEEE", address="0x0801DEEE",
                                    original_name="battle_x", original_summary="summary",
                                    original_interface=None, checker_reason="r",
                                    revised_name="battle_X_dispatchY", revised_summary="")
        bad_address = ReviseCandidate(run_id="r", name="sub_801DEEE", address="0x0801DEEE",
                                      original_name="x", original_summary="", original_interface=None,
                                      checker_reason="r", revised_name="battle_loadDEEE",
                                      revised_summary="")
        bad_taken = ReviseCandidate(run_id="r", name="sub_801DEEE", address="0x0801DEEE",
                                    original_name="x", original_summary="", original_interface=None,
                                    checker_reason="r", revised_name="battle_takenName",
                                    revised_summary="")
        bad_duplicate = ReviseCandidate(run_id="r2", name="sub_801E000", address="0x0801E000",
                                        original_name="x", original_summary="", original_interface=None,
                                        checker_reason="r", revised_name="battle_dispatchX",
                                        revised_summary="")
        eq("reconcile filter: the well-formed revise passes",
           [c.name for c in _reconcile_filter([good], elf, set(), frozenset(), set(), filter_base)[0]],
           ["sub_801DEEE"])
        eq("reconcile filter: a name with a style error is dropped",
           [c.name for c in _reconcile_filter([bad_style], elf, set(), frozenset(), set(),
                                             filter_base)[0]], [])
        eq("reconcile filter: a name that copies the address is dropped",
           [c.name for c in _reconcile_filter([bad_address], elf, set(), frozenset(), set(),
                                             filter_base)[0]], [])
        eq("reconcile filter: a name taken in bn6f is dropped",
           [c.name for c in _reconcile_filter([bad_taken], {"battle_takenName": 0}, set(),
                                             frozenset(), set(), filter_base)[0]], [])
        eq("reconcile filter: a name taken in symbols/db is dropped",
           [c.name for c in _reconcile_filter([bad_taken], elf, {"battle_takenName"}, frozenset(),
                                             set(), filter_base)[0]], [])
        eq("reconcile filter: a name already rescued is dropped",
           [c.name for c in _reconcile_filter([bad_taken], elf, set(), frozenset(),
                                             {"battle_takenName"}, filter_base)[0]], [])
        eq("reconcile filter: a name shared by two revise candidates drops both",
           [c.name for c in _reconcile_filter([good, bad_duplicate], elf, set(), frozenset(), set(),
                                             filter_base)[0]], [])
        eq("reconcile filter: only the first reason is reported for a duplicate",
           _reconcile_filter([good, bad_duplicate], elf, set(), frozenset(), set(), filter_base)[1][0][1],
           "the revised name was proposed for 2 functions")

    eq("rescue evidence: style", _rescue_evidence(
       RescueRow("r", "0x1", "a", "b", "style", "-")).startswith(
       "its name was fixed mechanically"), True)
    eq("rescue evidence: dedupe", _rescue_evidence(
       RescueRow("r", "0x1", "a", "b", "dedupe", "shared")).startswith(
       "the run's proposed name shared was contested"), True)
    eq("rescue evidence: reconcile", _rescue_evidence(
       RescueRow("r", "0x1", "a", "b", "reconcile", "-")),
       "proposed by Hyper glm-5.3-flash, revised by MiniMax-M3, "
       "accepted by glm-5.3-flash (tools/investigate_land.py reconcile)")

    if bad:
        for b in bad:
            print("selfcheck: " + b)
        sys.exit(f"selfcheck: {len(bad)} failures")
    print("selfcheck: ok")


def main() -> None:
    argv = sys.argv[1:]
    if argv[:1] == ["selfcheck"]:
        selfcheck()
        return
    root: str | None = None
    limit: int | None = None
    count, batch = 40, 200
    dry, only = False, None
    proposer, checker = PROPOSER, CHECKER
    rest: list[str] = []
    index = 0
    while index < len(argv):
        arg = argv[index]
        if arg == "--root":
            root, index = argv[index + 1], index + 2
        elif arg == "--limit":
            limit, index = int(argv[index + 1]), index + 2
        elif arg == "--batch":
            batch, index = int(argv[index + 1]), index + 2
        elif arg == "--group":
            only, index = argv[index + 1], index + 2
        elif arg == "--count":
            count, index = int(argv[index + 1]), index + 2
        elif arg == "--model":
            proposer, index = argv[index + 1], index + 2
        elif arg == "--model-checker":
            checker, index = argv[index + 1], index + 2
        elif arg == "--dry-run":
            dry, index = True, index + 1
        else:
            rest.append(arg)
            index += 1
    command = rest[0] if rest else ""
    if command in ("style-fix", "dedupe", "reconcile", "land-rescued"):
        if root is not None:
            use_root(root)
        if command == "style-fix":
            style_fix()
        elif command == "dedupe":
            run_dedupe(limit, only, dry, proposer, checker)
        elif command == "reconcile":
            run_reconcile(limit)
        else:
            land_rescued(limit, dry, batch)
        return
    if not rest:
        sys.exit(__doc__)
    land(rest[0], count, dry)


if __name__ == "__main__":
    main()
