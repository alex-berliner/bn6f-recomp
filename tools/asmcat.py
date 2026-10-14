"""Print a function's bn6f assembly by address, surviving later renames.

The symbol names in symbols/db entries change as renames land, so evidence
reproduce commands should not hardcode a label. Instead they pass the address:
this tool resolves the function's current name from the lifter's header
comments in cport/src/lifted (which tools/import_symbols.sh refreshes after
every rename) and prints the body from bn6f/asm.

    python3 tools/asmcat.py 080CA568

Standard library only: a scratch-friendly utility.
"""
from __future__ import annotations

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from funcindex import lifted_units  # noqa: E402


def find_unit(addr: str) -> tuple[str, int, int] | None:
    a = int(addr, 16)
    for name, unit in lifted_units().items():
        if unit.lo <= a < unit.hi:
            return name, unit.lo, unit.hi
    return None


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(__doc__)
        return 2
    hit = find_unit(argv[1].lower().removeprefix("0x"))
    if hit is None:
        print(f"no lifted unit contains {argv[1]}", file=sys.stderr)
        return 1
    name, lo, hi = hit
    for entry in sorted(os.listdir(os.path.join(ROOT, "bn6f", "asm"))):
        full = os.path.join(ROOT, "bn6f", "asm", entry)
        text = open(full, errors="replace").read()
        m = re.search(rf"(?m)^{re.escape(name)}:", text)
        if not m:
            continue
        endm = re.search(rf"(?m)^\s*(?:thumb_func_end|arm_func_end)\s+{re.escape(name)}\s*$", text[m.start():])
        end_off = m.start() + (endm.end() if endm else len(text))
        print(f"; {name}: {lo:08x}-{hi:08x}")
        print(text[m.start():end_off].rstrip())


if __name__ == "__main__":
    main(sys.argv)
