#!/usr/bin/env python3
"""The snapshot store under today's names: a link build-snapshots/NAME for every function whose
captured calls sit under an older name.

  tools/snapshot_names.py [--store DIR] [--check] [--quiet]

The capture workflow keeps a function's calls in build-snapshots/NAME/snap_ROUTE_N.bin, under its name
at the time. A rename (tools/rename_symbol.py, the record workflow) leaves them there, while every reader
(verify's captured-call difftest, the farm, the convert workflow) looks under the current name, so the
captured-call check silently stopped running for 245 renamed functions (docs/reviews/2026-10-11-
mutation-survey.md). No rename history is needed to repair that: each snapshot holds its entry PC
(header word 17), and bn6f.elf names that address today. Where today's name has no folder, the link
NAME -> OLD is made. Nothing moves, so older runs' links into the store stay valid, and later captures
of NAME land beside the old ones (same file names for the same calls).

tools/rename_symbol.py runs this after every rename; the commit gate warns when a link is missing.
--check lists the missing links without making them (exit 1 if any). Standard library only.
"""
from __future__ import annotations

import argparse
import collections
import os
import re
import struct
import subprocess
import sys

import gamepaths  # tools/gamepaths.py: the game profile (tools/paths.env)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STORE = os.path.join(ROOT, "build-snapshots")
LIFTED_H = os.path.join(ROOT, "cport", "include", "lifted.h")
MAGIC = 0x53364E42                       # "BN6S", tools/mgba_run --snap
SNAP = re.compile(r"^snap_.*(?<!\.exit)\.bin$")


def entry_pc(folder: str) -> int | None:
    """The entry address the calls in FOLDER start at (the first snapshot's PC), or None."""
    for name in sorted(os.listdir(folder)):
        if SNAP.match(name):
            with open(os.path.join(folder, name), "rb") as fh:
                head = fh.read(76)
            if len(head) == 76:
                words = struct.unpack("<19I", head)
                if words[0] == MAGIC:
                    return words[17] & ~1
    return None


def names_by_address() -> dict[int, set[str]]:
    """bn6f.elf's code symbols by address. Where several share an address, the entries lifted.h
    declares (the names the port's tools use) win; lifted.h may still have older names right after a
    rename, so it only breaks ties."""
    nm = subprocess.run(["arm-none-eabi-nm", gamepaths.ORIG_ELF], capture_output=True, text=True, check=True).stdout
    entries: set[str] = set()
    if os.path.exists(LIFTED_H):
        with open(LIFTED_H) as fh:
            entries = set(re.findall(r"\bNext\s+(\w+)\s*\(\s*void\s*\)", fh.read()))
    out: dict[int, set[str]] = collections.defaultdict(set)
    for line in nm.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in "Tt":
            out[int(parts[0], 16) & ~1].add(parts[2])
    for address, names in out.items():
        if len(names) > 1 and names & entries:
            out[address] = names & entries
    return out


def missing_links(store: str) -> tuple[list[tuple[str, str]], list[str]]:
    """(today's name, the folder holding its calls) for each name with no folder of its own, and the
    folders whose function today's ELF cannot name (left alone)."""
    if not os.path.isdir(store):
        return [], []
    by_address = names_by_address()
    current = {n for names in by_address.values() for n in names}
    links: list[tuple[str, str]] = []
    unknown: list[str] = []
    for folder in sorted(os.listdir(store)):
        path = os.path.join(store, folder)
        if os.path.islink(path) or not os.path.isdir(path) or folder in current:
            continue
        pc = entry_pc(path)
        names = sorted(by_address.get(pc, ())) if pc is not None else []
        if len(names) != 1:
            unknown.append(folder)
            continue
        if not os.path.lexists(os.path.join(store, names[0])):
            links.append((names[0], folder))
    return links, unknown


def link(store: str = STORE) -> list[tuple[str, str]]:
    """Make the missing links; returns them."""
    links, _ = missing_links(store)
    for name, folder in links:
        os.symlink(folder, os.path.join(store, name))
    return links


def main() -> int:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("--store", default=STORE)
    ap.add_argument("--check", action="store_true", help="only list the missing links (exit 1 if any)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    links, unknown = missing_links(args.store)
    if not args.check:
        for name, folder in links:
            os.symlink(folder, os.path.join(args.store, name))
    if not args.quiet:
        for name, folder in links:
            print(f"{name} -> {folder}")
        verb = "missing" if args.check else "made"
        print(f"{len(links)} links {verb}; {len(unknown)} folders name no function of today's bn6f.elf"
              + (f" (e.g. {', '.join(unknown[:3])})" if unknown else ""), file=sys.stderr)
    return 1 if args.check and links else 0


if __name__ == "__main__":
    sys.exit(main())
