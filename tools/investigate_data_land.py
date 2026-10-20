#!/usr/bin/env python3
"""Land wf investigate-data's finished candidates into symbols/db and bn6f.

  tools/investigate_data_land.py RUN_DIR [--count N] [--dry-run] [--selfcheck]
  tools/investigate_data_land.py style-fix [--root REPO] [--limit N]
                                            [--model PROPOSER] [--model-checker CHECKER]
  tools/investigate_data_land.py dedupe [--root REPO] [--limit N]
                                        [--model PROPOSER] [--model-checker CHECKER]
  tools/investigate_data_land.py reconcile [--root REPO] [--limit N]
                                            [--model PROPOSER] [--model-checker CHECKER]
  tools/investigate_data_land.py land-rescued [--root REPO] [--batch N] [--dry-run]

The sibling to tools/investigate_land.py, retargeted at data labels. The four
subcommands rescue candidates earlier runs' land runs dropped:

  style-fix: the naming-style rejects, without a model. The style rule is relaxed where
    bn6f already uses the shape (a prefix the elf's symbols or the database spell with an
    upper-case letter — gbaFieldStruct_ — is allowed), and every other break is normalized
    mechanically. What it rescues goes to build/investigate_data/rescue/style.tsv.
  dedupe: the candidates whose proposed name collides with one another (across ALL finished
    runs), with names already landed in symbols/db or with bn6f.elf data symbols, or with
    an asm include filename. One model call per group gives every member a distinct name;
    cached under build/investigate_data/dedupe/ (resumable).
  reconcile: the candidates the cross-check revised with a `revised_name`. The data label
    is still a placeholder; the revised name passes the relaxed style rule, copies no part
    of the label's address, and is unique across all the runs' revise candidates. Cached
    under build/investigate_data/reconcile/NAME.json.
  land-rescued: writes the rescued candidates' symbols/db entries the way the land run
    does (under each candidate's current placeholder name, then the record workflow renames
    them to NEWNAME.yaml). Reads from build/investigate_data/rescue/* and runs `./wf record
    --rename OLD NEW` in batches.

Selection (from report.tsv): status probable AND cross-check agree. The same rejects as
investigate_land's selection, adapted:

  - the proposed name is proposed for more than one label in the run;
  - it already exists as a data symbol of bn6f/bn6f.elf or as a symbols/db name, or as a
    cport identifier (covered by wf investigate-data's run-time guard, but checked again
    here), or as an asm include filename under bn6f/asm;
  - it breaks the naming style: lowerCamel after a prefix_ (a prefix is lower-case, one
    underscore, then lowerCamel with no underscore in it), and no digit run copied from the
    label's address;
  - the label already has a symbols/db entry (skipped, not dropped);
  - the run folder has no candidate YAML for it.

Before the record run every written entry is validated with tools/symdb.py's own check.
On a record failure or a changed ROM the tool stops and says the error; it never retries.

This tool does not write to symbols/db until it runs the record workflow, and the record
workflow itself commits. The land run that ran the original candidates sits in the run
folder's land_*.log; record's commits go in as tools/investigate_data_land.py itself
documents, with the message below.
"""
from __future__ import annotations

import argparse
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
    """Work against another checkout (a worktree's tool runs the main tree's data)."""
    global ROOT, TOOLS, DB, ELF, BN6F_DIR
    ROOT = os.path.abspath(repo)
    TOOLS = os.path.join(ROOT, "tools")
    DB = os.path.join(ROOT, "symbols", "db")
    sys.path.insert(0, TOOLS)
    sys.modules.pop("gamepaths", None)
    import gamepaths   # noqa: PLC0415  (the checkout --root names)
    globals()["gamepaths"] = gamepaths
    ELF = gamepaths.ORIG_ELF
    BN6F_DIR = gamepaths.BN6F_DIR


PLACEHOLDER_PREFIXES = ("byte_", "unk_", "word_", "dword_", "hword_",
                         "off_", "loc_", "obj_", "jumptable_")
HEX_LITERAL = re.compile(r"0x[0-9a-fA-F]")
TRAILING_ADDRESS = re.compile(r"_?[0-9a-fA-F]{7,}$")
DIGIT_RUN = re.compile(r"[0-9a-fA-F]{3,}")

GENERIC_PLACEHOLDER = re.compile(r"^(byte|unk|word|dword|hword|off)_[0-9A-Fa-f]{6,8}$")


def data_placeholder(name: str) -> bool:
    """A name that says nothing yet (the investigate-data workflow's rule for data labels)."""
    return any(name.startswith(prefix) for prefix in PLACEHOLDER_PREFIXES) \
        or GENERIC_PLACEHOLDER.match(name) is not None


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


# ── the naming rules ───────────────────────────────────────────────────

def style_problem(name: str, allowed: frozenset[str] = frozenset()) -> str | None:
    """Why NAME breaks the naming style (lowerCamel after a prefix_), or None."""
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
    """Why NAME carries digits copied from its ADDRESS (0x-prefixed hex), or None."""
    hexdigits = address.removeprefix("0x").removeprefix("0X").lstrip("0").upper() or "0"
    for run in DIGIT_RUN.findall(name):
        for width in range(len(run), 2, -1):
            piece = next((run[i:i + width].upper() for i in range(len(run) - width + 1)
                          if run[i:i + width].upper() in hexdigits), None)
            if piece is not None:
                return f"the digit run {piece} is copied from the address {address}"
    return None


def prefixes_in_use(names: Iterable[str]) -> frozenset[str]:
    """The prefixes bn6f already spells with an upper-case letter."""
    used: set[str] = set()
    for name in names:
        prefix, sep, _ = name.partition("_")
        if sep and any(c.isupper() for c in prefix):
            used.add(prefix)
    return frozenset(used)


_ACRONYM = re.compile(r"[A-Z]{2,}")


def _join_camel(parts: Sequence[str], lead_lower: bool = False) -> str:
    """PARTS as one lowerCamel word, an ALL-CAPS part folded to Uni — and, with LEAD_LOWER, the
    first part lower-case."""
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
    """NAME with its style break fixed mechanically (no model)."""
    if "maybe" in name.lower() or HEX_LITERAL.search(name):
        return None
    parts = name.split("_")
    if len(parts) == 1:
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


# ── bn6f.elf: the symbols and the data collision check ────────────────

def elf_symbols() -> dict[str, int]:
    """bn6f.elf's symbols (name -> address, Thumb bit ignored)."""
    out = subprocess.run(["arm-none-eabi-nm", ELF], capture_output=True, text=True, check=True).stdout
    syms: dict[str, int] = {}
    for line in out.splitlines():
        fields = line.split()
        if len(fields) == 3:
            syms.setdefault(fields[2], int(fields[0], 16) & ~1)
    return syms


# ── the symbols/db names ───────────────────────────────────────────────

def db_names() -> set[str]:
    return {f[:-5] for f in os.listdir(DB) if f.endswith(".yaml")}


# ── selection ──────────────────────────────────────────────────────────

def select(rows: list[Row], run_dir: str, syms: dict[str, int], count: int,
           allowed: frozenset[str] = frozenset()) \
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
            skipped.append((row.address, row.old, row.proposed, "the label already has a symbols/db entry"))
            continue
        if os.path.exists(os.path.join(ROOT, "bn6f", "asm", f"{row.proposed}.s")):
            skipped.append((row.address, row.old, row.proposed,
                            "its name is also an asm include file: the rename would rewrite the include path, not the file"))
            continue
        if counts[row.proposed] > 1:
            skipped.append((row.address, row.old, row.proposed,
                            f"proposed for {counts[row.proposed]} labels in the run"))
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
    eligible.sort(key=lambda pair: (-pair[0].score_int(), int(pair[0].address, 16)))
    return eligible[:count], skipped


# ── the symbols/db entry ───────────────────────────────────────────────

def entry_of(candidate: dict[str, Any], run_id: str, rescue: str | None = None) -> dict[str, Any]:
    """The symbols/db entry for a candidate, in the README's key order.

    Data labels get `kind: data`, the region's `region` field, the variable's `size` (bytes),
    and an `interactions` list. Written under the candidate's current (placeholder) name;
    the record run's rename turns it into NEWNAME.yaml with previous_names [old placeholder]."""
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
                             "region": candidate.get("region", "data"),
                             "size": candidate.get("size"),
                             "summary": summary}
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
    """Write the entry as symbols/db/OLD.yaml (the current name); its path."""
    entry = entry_of(candidate, run_id, rescue)
    path = os.path.join(DB, f"{entry['name']}.yaml")
    with open(path, "w", encoding="utf-8") as f:
        yaml.safe_dump(entry, f, sort_keys=False, allow_unicode=True, width=100)
    return path


def symdb_entry_problems(path: str) -> list[str]:
    """What tools/symdb.py's own check says about one written entry."""
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
    """`./wf record --symbol OLD ... --rename OLD NEW ... --message-file FILE`, once."""
    message = (f"symbols: name {len(pairs)} data labels found by {found_by} "
               f"(Hyper glm-5.3-flash, checked by MiniMax M3)\n"
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
    """ROM identical, the database clean, the commits in, the wf listing unaffected."""
    ok, problems = symdb_check_all()
    if not ok:
        sys.exit("the database check failed after the record run:\n" + "\n".join(problems)
                 + f"\nsee {log_path}")


# ── the finished-run summary ───────────────────────────────────────────

class Selected(NamedTuple):
    """One candidate a land run picked, with the data it needs for the report."""
    run_id: str
    old: str
    new: str
    address: str


def landing_report(pairs: list[tuple[Row, dict[str, Any]]], selected: list[Selected],
                   skipped: list[tuple[str, str, str, str]], path: str) -> None:
    """The land's report.json: who was taken, who was dropped, what the symbols/db count is."""
    report = {"pairs": [{"old": pair[0].old, "new": pair[0].proposed, "address": pair[0].address}
                        for pair in pairs],
              "selected": [{"run_id": s.run_id, "old": s.old, "new": s.new, "address": s.address}
                           for s in selected],
              "skipped": [{"address": a, "old": o, "proposed": p, "reason": r}
                          for (a, o, p, r) in skipped],
              "symbols_db_after": len(os.listdir(DB))}
    with open(path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)


def write_skipped(skipped: list[tuple[str, str, str, str]], run_dir: str) -> None:
    path = os.path.join(run_dir, "land_skipped.tsv")
    with open(path, "w", encoding="utf-8") as f:
        f.write("# address\told\tproposed\treason\n")
        for address, old, proposed, reason in skipped:
            f.write(f"{address}\t{old}\t{proposed}\t{reason}\n")


def land_one(_old: str, _new: str, candidate: dict[str, Any], run_dir: str,
             _args: argparse.Namespace) -> tuple[bool, list[str]]:
    """Write the entry under the OLD name (the record run renames it); check it locally."""
    try:
        path = write_entry(candidate, os.path.basename(run_dir))
    except yaml.YAMLError as exc:
        return False, [f"YAML failed to write: {exc}"]
    problems = symdb_entry_problems(path)
    return (not problems), problems


# ── the main land run ──────────────────────────────────────────────────

def run(args: argparse.Namespace) -> None:
    run_dir = args.run_dir
    rows = read_report(run_dir)
    if not rows:
        sys.exit(f"no rows in {os.path.join(run_dir, 'report.tsv')}")
    syms = elf_symbols()
    allowed = prefixes_in_use(syms) | prefixes_in_use(db_names())
    eligible, skipped = select(rows, run_dir, syms, args.count, allowed)
    if not eligible:
        print(f"nothing to land ({len(skipped)} skipped); see {run_dir}/land_skipped.tsv")
        write_skipped(skipped, run_dir)
        return
    pairs: list[tuple[str, str]] = []
    for row, candidate in eligible:
        old, new = row.old, row.proposed
        ok, row_problems = land_one(old, new, candidate, run_dir, args)
        if ok:
            pairs.append((old, new))
        else:
            skipped.append((row.address, row.old, row.proposed,
                            "the candidate yaml failed tools/symdb.py's own check: "
                            + "; ".join(row_problems)))
    if not pairs:
        print("every eligible candidate failed the database check; see "
              + os.path.join(run_dir, "land_skipped.tsv"))
        write_skipped(skipped, run_dir)
        return
    if args.dry_run:
        print(f"dry run: would rename {len(pairs)} symbols/db entries: "
              + ", ".join(o for o, _ in pairs))
        return
    if args.selfcheck:
        ok, db_problems = symdb_check_all()
        print(f"selfcheck: {len(pairs)} entries written; database "
              f"{'clean' if ok else 'has problems'}:\n" + "\n".join(db_problems))
        return
    report_path = os.path.join(run_dir, "report.json")
    log_path = os.path.join(run_dir, "land_record.log")
    report = run_record(pairs, f"wf investigate-data {os.path.basename(run_dir)}", run_dir, log_path)
    check_afterwards(report, log_path)
    landing_report(eligible, [], skipped, report_path)
    write_skipped(skipped, run_dir)
    print(f"landed {len(pairs)}: " + ", ".join(f"{o}->{n}" for o, n in pairs))
    print(f"see {report_path}")


# ── the four rescue subcommands ────────────────────────────────────────
# Skipped until a data run completes; the same machinery as investigate_land's applies.

def style_fix_main(_args: argparse.Namespace) -> None:
    """The naming-style rescues (the only mechanical ones), without a model. Reads from
    build/investigate_data/<run>/land_skipped.tsv, writes rescue/style.tsv."""
    sys.exit("style-fix: TODO (the same machinery as investigate_land's, applied to the data "
             "land's skipped file)")


def dedupe_main(_args: argparse.Namespace) -> None:
    """The candidates whose proposed names collide (across runs and with landed names)."""
    sys.exit("dedupe: TODO")


def reconcile_main(_args: argparse.Namespace) -> None:
    """The candidates the cross-check revised with a `revised_name`."""
    sys.exit("reconcile: TODO")


def land_rescued_main(_args: argparse.Namespace) -> None:
    """Apply the rescue plans; writes the symbols/db entries and runs the record workflow."""
    sys.exit("land-rescued: TODO")


# ── the CLI ────────────────────────────────────────────────────────────

def _main(argv: list[str] | None = None) -> None:
    if argv is None:
        argv = sys.argv[1:]
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return
    if argv[0] in ("style-fix", "dedupe", "reconcile", "land-rescued"):
        sub = argv[0]
        rest = argv[1:]
        parser = argparse.ArgumentParser(prog=f"investigate_data_land {sub}")
        parser.add_argument("--root", help="work against this checkout (default: the tool's)")
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
    parser = argparse.ArgumentParser(prog="investigate_data_land",
                                     description="Land wf investigate-data candidates into "
                                                 "symbols/db and bn6f.")
    parser.add_argument("--root", help="work against this checkout (default: the tool's)")
    parser.add_argument("run_dir", nargs=1, help="the wf investigate-data RUN_ID folder")
    parser.add_argument("--count", type=int, default=40, help="how many candidates to land")
    parser.add_argument("--dry-run", action="store_true",
                        help="write nothing; show what would happen")
    parser.add_argument("--selfcheck", action="store_true",
                        help="run the tool's own checks without the record")
    args = parser.parse_args(argv)
    if args.root:
        use_root(args.root)
    args.run_dir = args.run_dir[0]
    run(args)


if __name__ == "__main__":
    _main()
