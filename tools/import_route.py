#!/usr/bin/env python3
"""Bring in, or refresh, a long recorded route made outside this repo: the BN6 Falzar BeastSP
recording rebuilt from a speedrun video (~/Code/vidsync/ready, see its README.md).

    tools/import_route.py [READY_DIR] [--name beastsp] [--source bn6f_beastsp] [--jobs 6]

Run it again whenever READY_DIR grows. It
1. copies READY_DIR/SOURCE.trace, .poke and .sav to tools/traces/NAME.*;
2. replays the trace on the original from power-on (mgba_run), saving a checkpoint at every
   STEP-th game frame into CHECKPOINTS/NAME/ (build-checkpoints: game RAM, never committed),
   and checks the final state against the trace's "# end" line;
3. replays every STEP-frame window from its checkpoint, in parallel, with --profile-game and
   --call-edges; keeps each window's profile next to its checkpoint (`wf capture` runs only
   the windows that execute a function) and merges them into the committed
   tools/traces/coverage/NAME.prof and NAME.edges;
4. sets the route's length in tools/wf/wf/routes.py.
Then, since the call edges changed: python3 tools/lifter/golden.py update, and lockstep the port.

A window is game frames [G, G+STEP) replayed from the checkpoint taken at the start of game frame
G (mgba_run --load-state with --resume-game-frame G): the same machine as a replay from power-on.
"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "tools", "abi"))
import gamepaths  # noqa: E402
import call_edges  # noqa: E402

STEP = 10000
ROUTES_PY = os.path.join(ROOT, "tools", "wf", "wf", "routes.py")
_END = re.compile(r"^# end hw_frames=(\d+) game_frame=(\d+) (ewram=\S+ pal=\S+ vram=\S+ oam=\S+)", re.M)
_FINAL = re.compile(r"^final game=(\d+) hw=(\d+) (ewram=\S+ pal=\S+ vram=\S+ oam=\S+)", re.M)


def checkpoint_dir(name: str) -> str:
    return os.path.join(gamepaths.CHECKPOINTS, name)


def checkpoint(name: str, game: int) -> str:
    """The state saved at the start of game frame `game` (mgba_run --save-state-every)."""
    return os.path.join(checkpoint_dir(name), f"{name}_{game:07d}.state")


def window_profile(name: str, game: int) -> str:
    """The profile of the window [game, game + STEP), kept next to its checkpoint."""
    return os.path.join(checkpoint_dir(name), f"{name}_{game:07d}.prof")


def resume_args(name: str, game: int) -> list[str]:
    """mgba_run arguments to start at game frame `game` (power-on for 0)."""
    return [] if game == 0 else ["--load-state", checkpoint(name, game), "--resume-game-frame", str(game)]


def trace_end(trace: str) -> tuple[int, int, str]:
    """(hardware frames, last game frame, digests) from the trace's "# end" line."""
    match = _END.search(open(trace).read())
    if match is None:
        sys.exit(f"{trace} has no '# end' line")
    return int(match[1]), int(match[2]), match[3]


def replay_args(trace: str) -> list[str]:
    return ["--quiet", "--bios", gamepaths.GBA_BIOS, "--input", trace,
            "--game-frame-addr", gamepaths.GAME_FRAME_ADDR, *call_edges.poke_args(trace)]


def make_checkpoints(name: str, trace: str, length: int) -> None:
    """Replay from power-on to `length`, saving the checkpoints; the final state must be the recorded one."""
    hw, _, want = trace_end(trace)
    out = checkpoint_dir(name)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    done = subprocess.run([gamepaths.MGBA_RUN, gamepaths.ORIG_ROM, *replay_args(trace), "--frames", str(2 * hw),
                           "--until-game-frame", str(length), "--final-digest",
                           "--save-state-every", str(STEP), os.path.join(out, name)],
                          capture_output=True, text=True, check=True)
    final = _FINAL.search(done.stdout)
    if final is None or final[3] != want:
        sys.exit(f"the replay does not reach the recorded state: {final[0] if final else done.stdout[-400:]}\n"
                 f"  recorded: {want}")
    print(f"replay from power-on: {final[0]}")
    mark_landings(trace, done.stdout)


def mark_landings(trace: str, replay: str) -> None:
    """Write where the original's writes landed into the .poke: "G ..." -> "G:L ...", L the game frame
    whose work had just ended (mgba_run's "at="). A port lands them by L (tools/common.sh poke_args)."""
    poke = trace.removesuffix(".trace") + ".poke"
    if not os.path.exists(poke):
        return
    landed = iter(re.findall(r"^poke game=\d+ \S+ at=(\d+)$", replay, re.M))
    lines = []
    for line in open(poke):
        if line.strip() and not line.startswith("#"):
            game, spec = line.split(maxsplit=1)
            ats = [next(landed, None) for _ in spec.strip().split(",")]   # one "poke" line per write
            if None in ats or len(set(ats)) != 1:
                sys.exit(f"{poke}: the writes of game frame {game} did not land together ({ats})")
            line = f"{game.split(':')[0]}:{ats[0]} {spec}"
        lines.append(line)
    open(poke, "w").writelines(lines)
    print(f"{poke}: landing game frames marked")


def observe_window(name: str, trace: str, game: int, length: int, sites: str | None, work: str) -> str:
    """Replay the window [game, game + STEP) with --profile-game (and --call-edges when `sites`);
    returns its directory (profile.txt, edges.txt)."""
    os.makedirs(work, exist_ok=True)
    edges = ["--call-edges", sites, os.path.join(work, "edges.txt")] if sites else []
    subprocess.run([gamepaths.MGBA_RUN, gamepaths.ORIG_ROM, *replay_args(trace), "--frames", str(3 * STEP),
                    *resume_args(name, game), "--profile-game", str(game), str(min(game + STEP, length)), *edges],
                   cwd=work, capture_output=True, text=True, check=True)
    return work


def windows(length: int) -> list[int]:
    return list(range(0, length, STEP))


def merge_edges(parts: list[str], out: str, trace: str, n_sites: int) -> int:
    """Sum the windows' edge counts into OUT, stamped like tools/abi/call_edges.py's."""
    counts: Counter[tuple[str, str, str]] = Counter()
    lost = 0
    for part in parts:
        for line in open(part):
            if line.startswith("#"):
                found = re.search(r"(\d+) edges lost", line)
                lost += int(found[1]) if found else 0
            elif line.strip():
                site, target, lr, n = line.split()
                counts[site, target, lr] += int(n)
    with open(out, "w") as fh:
        fh.write(f"# site target lr count (mgba_run --call-edges in {len(parts)} windows from checkpoints, "
                 f"tools/import_route.py; {lost} edges lost to a full table)\n")
        fh.write(call_edges.stamp(trace, n_sites))
        fh.writelines(sorted(f"{s} {t} {lr} {n}\n" for (s, t, lr), n in counts.items()))
    return len(counts)


def merge_profiles(parts: list[str], out: str, length: int) -> int:
    cycles: Counter[str] = Counter()
    for part in parts:
        for line in open(part):
            if line.strip() and not line.startswith("#"):
                address, n = line.split()
                cycles[address] += int(n)
    with open(out, "w") as fh:
        fh.write(f"# game 0..{length} cycles {sum(cycles.values())} "
                 f"({len(parts)} windows from checkpoints, tools/import_route.py)\n")
        fh.writelines(f"{address} {cycles[address]}\n" for address in sorted(cycles))
    return len(cycles)


def observe(name: str, trace: str, length: int, jobs: int, profiles: bool, edges_out: str | None,
            graph: str = gamepaths.ABI_GRAPH) -> None:
    """Profile (into the checkpoint directory and NAME.prof) and/or observe call edges at the
    indirect sites of `graph` (into edges_out) over the whole route, a window per job."""
    with tempfile.TemporaryDirectory() as tmp:
        sites = None
        n_sites = 0
        if edges_out:
            sites = os.path.join(tmp, "sites.txt")
            n_sites = call_edges.write_sites(graph, sites)
        with ThreadPoolExecutor(jobs) as pool:
            work = list(pool.map(lambda g: observe_window(name, trace, g, length, sites, os.path.join(tmp, str(g))),
                                 windows(length)))
        if profiles:
            for game, w in zip(windows(length), work):
                shutil.copy(os.path.join(w, "profile.txt"), window_profile(name, game))
            out = os.path.join(gamepaths.TRACES, "coverage", f"{name}.prof")
            print(f"{out}: {merge_profiles([os.path.join(w, 'profile.txt') for w in work], out, length)} addresses")
        if edges_out:
            n = merge_edges([os.path.join(w, "edges.txt") for w in work], edges_out, trace, n_sites)
            print(f"{edges_out}: {n} edges")


def set_route_length(name: str, length: int) -> None:
    text = open(ROUTES_PY).read()
    new, n = re.subn(rf'_route\("{name}", \d+\)', f'_route("{name}", {length})', text)
    if n:
        open(ROUTES_PY, "w").write(new)
        print(f"{os.path.relpath(ROUTES_PY, ROOT)}: {name} is {length} game frames")
    else:
        print(f"add _route(\"{name}\", {length}) to {os.path.relpath(ROUTES_PY, ROOT)} once the port passes lockstep")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ready", nargs="?", default=os.path.expanduser("~/Code/vidsync/ready"))
    ap.add_argument("--name", default="beastsp", help="the route's name here")
    ap.add_argument("--source", default="bn6f_beastsp", help="the recording's name in READY")
    ap.add_argument("--jobs", type=int, default=6)
    args = ap.parse_args()
    trace = os.path.join(gamepaths.TRACES, f"{args.name}.trace")
    for ext in ("trace", "poke", "sav"):
        src = os.path.join(args.ready, f"{args.source}.{ext}")
        if os.path.exists(src):
            shutil.copy(src, os.path.join(gamepaths.TRACES, f"{args.name}.{ext}"))
    _, last, _ = trace_end(trace)
    length = last + 1
    print(f"{args.name}: game frames 0..{length}")
    make_checkpoints(args.name, trace, length)
    observe(args.name, trace, length, args.jobs, profiles=True,
            edges_out=os.path.join(gamepaths.TRACES, "coverage", f"{args.name}.edges"))
    set_route_length(args.name, length)
    print("the call edges changed: python3 tools/lifter/golden.py update; then lockstep the port on the route")


if __name__ == "__main__":
    main()
