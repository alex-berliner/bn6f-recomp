#!/usr/bin/env python3
"""Take a readable function out: the lifted original runs again (the way of 51d348b and 3657a55).

  tools/take_out.py NAME --why TEXT [--tool TEXT]

For NAME:
  - its part (`/* ── NAME ── */` to the next part) leaves its cport/src/game file; a part that also
    defines other entries is refused (take those out together, or split the part first);
  - symbols/db/NAME.yaml: status confirmed -> probable (the checks that confirmed it ran code that
    differs from the original), a changes item saying why, readable_c and the stamp removed;
  - its line leaves tools/abi/lint_baseline.json and tools/abi/retype_queue.tsv.
Then run `python3 tools/symdb.py render` (symbols/SYMBOLS.md) and commit; a verify should follow.
Text edits only, so the db entry's other lines stay as they are.
"""
from __future__ import annotations

import argparse
import datetime
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "cport", "src", "game")
DB = os.path.join(ROOT, "symbols", "db")
BASELINE = os.path.join(ROOT, "tools", "abi", "lint_baseline.json")
QUEUE = os.path.join(ROOT, "tools", "abi", "retype_queue.tsv")
ENTRY = re.compile(r"^(?:IWRAM_\w+\s+|ARM_CODE\s+)*Next\s+(\w+)\s*\(\s*void\s*\)\s*\{", re.M)
PART = re.compile(r"^/\* ── (\w+) ── \*/", re.M)


def remove_part(name: str) -> str:
    for path in sorted(glob.glob(os.path.join(GAME, "*.c"))):
        text = open(path).read()
        if not re.search(rf"\bNext\s+{re.escape(name)}\s*\(\s*void\s*\)\s*\{{", text):
            continue
        marks = list(PART.finditer(text))
        start = next((m for m in marks if m.group(1) == name), None)
        if start is None:
            sys.exit(f"{name}: no `── {name} ──` part in {path}")
        later = [m.start() for m in marks if m.start() > start.start()]
        end = later[0] if later else len(text)
        entries = ENTRY.findall(text[start.start():end])
        if entries != [name]:
            sys.exit(f"{name}: its part in {path} defines {entries}; take them out together or split the part")
        new = text[:start.start()].rstrip("\n") + "\n\n" + text[end:] if later else text[:start.start()].rstrip("\n") + "\n"
        open(path, "w").write(new)
        return os.path.relpath(path, ROOT)
    sys.exit(f"{name}: not a readable function in {GAME}")


def edit_db(name: str, file: str, why: str, tool: str) -> None:
    path = os.path.join(DB, f"{name}.yaml")
    lines = open(path).read().split("\n")
    out = []
    i = 0
    confirmed = any(line == "status: confirmed" for line in lines)
    while i < len(lines):
        line = lines[i]
        if line == "status: confirmed":
            out.append("status: probable")
        elif line.startswith("readable_c:"):
            what = (f"readable C taken out of {file} (the lifted original runs again)"
                    + ("; status confirmed -> probable, as the checks that confirmed it ran code that differs from "
                       "the original" if confirmed else "") + f": {why}")
            out += [f"- date: '{datetime.date.today().isoformat()}'",
                    "  what: '" + what.replace("'", "''") + "'",
                    "  tool: '" + tool.replace("'", "''") + "'"]
        elif line == "stamp:":
            i += 1
            while i < len(lines) and (lines[i].startswith(" ") or lines[i].startswith("-")):
                i += 1
            continue
        else:
            out.append(line)
        i += 1
    open(path, "w").write("\n".join(out))


def drop_lists(name: str) -> None:
    text = open(BASELINE).read()
    data = json.loads(text)
    functions = data.get("functions", data)
    for key in [k for k in functions if k == name or k.endswith(":" + name)]:
        del functions[key]
    lines = [line for line in text.split("\n") if not re.match(rf'\s*"(?:[\w.]+:)?{re.escape(name)}":', line)]
    new = "\n".join(lines)
    try:
        json.loads(new)
    except json.JSONDecodeError:     # the line removed ended the object: let json write it
        new = json.dumps(data, indent=2)
    open(BASELINE, "w").write(new)
    if os.path.exists(QUEUE):
        rows = open(QUEUE).read().split("\n")
        open(QUEUE, "w").write("\n".join(r for r in rows if not r.startswith(name + "\t")))


def main() -> None:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("name")
    ap.add_argument("--why", required=True)
    ap.add_argument("--tool", default="review (Claude)")
    args = ap.parse_args()
    file = remove_part(args.name)
    edit_db(args.name, file, args.why, args.tool)
    drop_lists(args.name)
    print(f"{args.name}: taken out of {file}")


if __name__ == "__main__":
    main()
