#!/usr/bin/env python3
"""Land wf investigate's finished candidates into symbols/db and bn6f.

  tools/investigate_land.py RUN_DIR --count 40 [--dry-run]
  tools/investigate_land.py selfcheck

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
from typing import Any

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, "tools")
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)
import gamepaths  # noqa: E402  (tools/gamepaths.py: the game profile)

DB = os.path.join(ROOT, "symbols", "db")
ELF = gamepaths.ORIG_ELF
WF = os.path.join(ROOT, "wf")
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

def style_problem(name: str) -> str | None:
    """Why NAME breaks the naming style (lowerCamel after a prefix_), or None."""
    if "maybe" in name.lower():
        return "it hedges (Maybe)"
    if HEX_LITERAL.search(name):
        return "it carries a 0x literal"
    if TRAILING_ADDRESS.search(name):
        return "it ends in a digit run copied from the placeholder's address"
    if "_" in name:
        prefix, _, rest = name.partition("_")
        if not re.fullmatch(r"[a-z][a-z0-9]*", prefix):
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
           count: int) -> tuple[list[tuple[Row, dict[str, Any]]], list[tuple[str, str, str, str]]]:
    """The candidates to land and the dropped/skipped ones with their reasons.

    Returns ((row, candidate), ...) in landing order and
    (address, old, proposed, reason) for every probable+agree row not landed."""
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
        problem = style_problem(row.proposed)
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

def entry_of(candidate: dict[str, Any], run_id: str) -> dict[str, Any]:
    """The symbols/db entry for a candidate, in the README's key order.

    Written under the candidate's current (placeholder) name; the record run's
    rename turns it into NEWNAME.yaml with previous_names [old placeholder]."""
    summary = str(candidate.get("summary") or "")
    proposed = PROPOSED_PREFIX.match(summary)
    if proposed is not None:
        summary = summary[proposed.end():]
    evidence = []
    for item in candidate.get("evidence") or []:
        item = dict(item)
        result = str(item.get("result") or "")
        if f"wf investigate {run_id}" not in result:
            item["result"] = f"{result}; run wf investigate {run_id}"
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


def write_entry(candidate: dict[str, Any], run_id: str) -> str:
    """Write the entry as symbols/db/OLD.yaml (the current name); its path."""
    entry = entry_of(candidate, run_id)
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

def run_record(pairs: list[tuple[str, str]], run_id: str, run_dir: str, log_path: str) -> dict[str, Any]:
    """`./wf record --symbol OLD ... --rename OLD NEW ... --message-file FILE`, once. Its report."""
    message = (f"symbols: name {len(pairs)} functions found by wf investigate {run_id} "
               f"(Hyper glm-5.3-flash, checked by MiniMax M3)\n"
               f"\n"
               f"Candidates selected from the run's report (status probable, cross-check agree); "
               f"entries written by tools/investigate_land.py, checked with tools/symdb.py, "
               f"landed through the record workflow.\n"
               f"\n"
               f"Co-Authored-By: pi agent (hyper/glm-5.3-flash)\n"
               f"Pi-Session: 01a15098-714b-71d3-a0b3-d9fb7b804fb5\n")
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
    bn6f = subprocess.run(["git", "log", "-3", "--format=oneline"], cwd=gamepaths.BN6F_DIR,
                          capture_output=True, text=True).stdout
    print(f"recomp commit: {log.strip()}")
    print(f"bn6f log:\n  " + "\n  ".join(bn6f.strip().splitlines()))
    listing = subprocess.run([WF, "investigate", "--count", "0"], cwd=ROOT, capture_output=True, text=True)
    if listing.returncode != 0:
        sys.exit(f"./wf investigate --count 0 failed:\n{listing.stdout[-2000:]}{listing.stderr[-2000:]}")
    print(f"./wf investigate --count 0: ok "
          f"({next((l for l in listing.stdout.splitlines() if 'finished' in l), '').strip()})")
    print(f"record log: {log_path}")


# ── main ───────────────────────────────────────────────────────────────

def land(run_dir: str, count: int, dry: bool) -> None:
    run_id = os.path.basename(os.path.normpath(run_dir))
    rows = read_report(run_dir)
    syms = elf_symbols()
    code = text_symbols(syms)
    chosen, skipped = select(rows, run_dir, syms, code, count)
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
    report = run_record(pairs, run_id, run_dir, log_path)
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
    dry = "--dry-run" in argv
    argv = [a for a in argv if a != "--dry-run"]
    count = 40
    if "--count" in argv:
        i = argv.index("--count")
        count = int(argv[i + 1])
        del argv[i:i + 2]
    if not argv:
        sys.exit(__doc__)
    land(argv[0], count, dry)


if __name__ == "__main__":
    main()
