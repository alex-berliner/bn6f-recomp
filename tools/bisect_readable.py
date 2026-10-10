#!/usr/bin/env python3
"""Which readable function makes the port diverge from the original on a route.

  tools/wf/.venv/bin/python tools/bisect_readable.py ROUTE BAD_GAME_FRAME [--window N] [--candidates FILE] [--rev REV]

ROUTE: a recorded route (tools/traces/ROUTE.trace). BAD_GAME_FRAME: a game
frame whose palette/VRAM/OAM differ, ideally the first (tools/lockstep.sh
reports the first failing checkpoint; mgba_run --dump-at-game narrows it).
REV: the commit to bisect in (default HEAD).

Candidates (unless --candidates names them, one per line): the readable
functions (cport/src/game) the original executes in the N game frames
before the first bad one (default 30). In a worktree of HEAD
(build-wf-worktrees/bisect), a subset is switched back to its lifted
original by renaming `Next NAME(void) {`; the tree is lifted, the ROM built
(build-bisect) and its palette/VRAM/OAM at the first bad frame and 16 frames
later compared with the original's. Halving finds one function whose lifted
original alone makes those frames identical. Assumes a single culprit:
when both halves are needed, the halving says so and stops.

Found tryStartNotificationCutscene_8035054 on story4 at frame 32905 (2026-10-09).
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/wf"))
from wf.repo import TOOLCHAIN_BIN  # noqa: E402
from wf.tools.build import committed_tree, link_shared  # noqa: E402
import funcindex  # noqa: E402  # tools/funcindex.py: the function index (lifted units, readable functions)
import gamepaths  # noqa: E402  # tools/gamepaths.py: the game profile (tools/paths.env)

ENV = {**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}"}
NEXT = r"^((?:IWRAM_\w+\s+|ARM_CODE\s+)*Next\s+){name}(\s*\(\s*void\s*\)\s*\{{)"
ORIGINAL = Path(gamepaths.ORIG_ROM)


def run_rom(rom: Path, trace: Path, until: int, extra: list[str], cwd: Path | None = None) -> None:
    poke = subprocess.run(["bash", "-c", f'ROOT="{ROOT}"; . "{ROOT}/tools/common.sh"; poke_args "{rom}" "{trace}"'],
                          capture_output=True, text=True, check=True).stdout.split()
    subprocess.run([gamepaths.MGBA_RUN, str(rom), "--frames", "400000", "--bios",
                    gamepaths.GBA_BIOS, "--input", str(trace), "--game-frame-addr",
                    gamepaths.GAME_FRAME_ADDR, "--until-game-frame", str(until), "--quiet", *extra, *poke],
                   check=True, stdout=subprocess.DEVNULL, cwd=cwd)


def readable_names(game: Path) -> set[str]:
    return set(funcindex.readable_functions(str(game)))


def executed_units(trace: Path, first: int, last: int, work: Path) -> set[str]:
    """Lifted units (cport/src/lifted header names) the original executes in game frames first..last."""
    run_rom(ORIGINAL, trace, last + 1, ["--profile-game", str(first), str(last)], cwd=work)
    unit_at = funcindex.unit_finder(funcindex.unit_ranges(str(ROOT / "cport/src/lifted")))
    hit: set[str] = set()
    for line in open(work / "profile.txt"):
        if line.startswith("#"):
            continue
        name = unit_at(int(line.split()[0], 16))
        if name is not None:
            hit.add(name)
    return hit


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("route")
    ap.add_argument("first_bad", type=int)
    ap.add_argument("--window", type=int, default=30)
    ap.add_argument("--candidates")
    ap.add_argument("--rev", default="HEAD")
    args = ap.parse_args()
    trace = Path(gamepaths.TRACES) / f"{args.route}.trace"
    frames = (args.first_bad, args.first_bad + 16)
    work = Path(tempfile.mkdtemp(prefix="bisect-"))

    tree = committed_tree("bisect")
    if args.rev != "HEAD":
        subprocess.run(["git", "-C", str(tree), "checkout", "--quiet", "--detach", "--force", args.rev], check=True)
        link_shared(tree)   # the checkout puts the submodule directories back in place of the links
    game = tree / "cport/src/game"
    pristine = {p: p.read_text() for p in game.glob("*.c")}
    if args.candidates:
        cands = [line.strip() for line in open(args.candidates) if line.strip()]
    else:
        ran = executed_units(trace, max(0, args.first_bad - args.window), args.first_bad + 1, work)
        cands = sorted(readable_names(game) & ran)
    print(f"{len(cands)} candidates; work files in {work}", flush=True)
    run_rom(ORIGINAL, trace, max(frames) + 1, ["--dump-at-game", ",".join(map(str, frames)), str(work / "orig")])

    def switch_off(names: set[str]) -> None:
        for p, text in pristine.items():
            new = text
            for n in names:
                new = re.sub(NEXT.format(name=n), rf"\g<1>{n}__bisect_off\g<2>", new, flags=re.M)
            if new != p.read_text():
                p.write_text(new)

    def matches(names: set[str]) -> bool:
        names = set(names)
        while True:
            switch_off(names)
            lift = subprocess.run(["python3", str(tree / "tools/lifter/lift.py"), "--bn6f", gamepaths.BN6F_DIR,
                                   "--out", str(tree / "cport/src/lifted"), "--inc", str(tree / "cport/include"),
                                   "--overrides", str(game)], capture_output=True, text=True, env=ENV)
            m = re.search(r"must define all its entries: missing \[(.*?)\]", lift.stdout + lift.stderr)
            if not m:
                break
            names |= set(re.findall(r"'(\w+)'", m.group(1)))   # a unit's entries go back together
        if lift.returncode != 0:
            sys.exit(lift.stdout[-2000:] + lift.stderr[-2000:])
        started = time.monotonic()
        make = subprocess.run(["make", "-C", str(tree / "cport"), "BUILD=build-bisect", f"-j{os.cpu_count()}"],
                              capture_output=True, text=True, env=ENV)
        if make.returncode != 0:
            sys.exit(make.stdout[-3000:] + make.stderr[-3000:])
        run_rom(tree / "cport/build-bisect/bn6f_lifted.gba", trace, max(frames) + 1,
                ["--dump-at-game", ",".join(map(str, frames)), str(work / "test")])
        ok = all((work / f"test_{g}.bin").read_bytes() == (work / f"orig_{g}.bin").read_bytes() for g in frames)
        print(f"  {len(names)} switched off -> {'identical' if ok else 'differs'} "
              f"({time.monotonic() - started:.0f}s)", flush=True)
        return ok

    try:
        if matches(set()):
            sys.exit(f"{args.route} at game frames {frames} is already identical at {args.rev}: nothing to bisect")
        if not matches(set(cands)):
            sys.exit("switching off every candidate does not make the frames identical: the cause is elsewhere "
                     "(a wider --window, or code that ran earlier)")
        rest = cands
        while len(rest) > 1:
            half, other = rest[: len(rest) // 2], rest[len(rest) // 2:]
            print(f"first half ({len(half)}): {' '.join(half[:6])}{' …' if len(half) > 6 else ''}", flush=True)
            if matches(set(half)):
                rest = half
            elif matches(set(other)):
                rest = other
            else:
                sys.exit(f"neither half alone is enough: more than one culprit among {' '.join(rest)}")
        print(f"culprit: {rest[0]}")
    finally:
        switch_off(set())


if __name__ == "__main__":
    main()
