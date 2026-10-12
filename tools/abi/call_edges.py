#!/usr/bin/env python3
"""Observe indirect call edges on the recorded routes (docs/C_CALLING_CONVENTION.md 3.2).

    tools/abi/call_edges.py build-abi/graph.json [ROUTE...] [--jobs N] [--mgba build-tools/mgba_run]

Takes the indirect sites (calls through a register and computed jumps) from an export
of `lift.py --export-abi`, replays every route of tools/wf/wf/routes.py (or the ones
named) on the original ROM with `mgba_run --call-edges`, and writes
tools/traces/coverage/ROUTE.edges ("SITE TARGET LR COUNT", hex, sorted; committed) next
to the coverage profiles, stamped with what they were observed on (hashes of the trace, its
save and pokes, and the original ROM; the number of sites). Then merge them into the export:

    python3 tools/lifter/lift.py --export-abi build-abi/graph.json --call-edges tools/traces/coverage/*.edges

(build-abi/graph.json is ABI_GRAPH in tools/paths.env, where the ROM, BIOS and mgba_run come from too.)

Prints a JSON summary: per route the distinct edges and seconds taken.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import gamepaths  # noqa: E402  # tools/gamepaths.py: the game profile (tools/paths.env)

TRACES = gamepaths.TRACES
ORIG = gamepaths.ORIG_ROM
BIOS = gamepaths.GBA_BIOS
GAME_FRAME_ADDR = gamepaths.GAME_FRAME_ADDR


def routes() -> dict[str, int]:
    """Route name -> game frames, from tools/wf/wf/routes.py (without importing the wf venv)."""
    text = open(os.path.join(ROOT, "tools", "wf", "wf", "routes.py")).read()
    return {name: int(n) for name, n in re.findall(r'_route\("(\w+)", (\d+)\)', text)}


def poke_args(trace: str) -> list[str]:
    """tools/common.sh poke_args for the original ROM: the trace's save and .poke writes."""
    base = trace.removesuffix(".trace")
    out = ["--save", base + ".sav"] if os.path.exists(base + ".sav") else []
    if os.path.exists(base + ".poke"):
        nm = subprocess.run(["arm-none-eabi-nm", gamepaths.ORIG_ELF], capture_output=True,
                            text=True, check=True).stdout
        pc = next(line.split()[0] for line in nm.splitlines() if line.endswith(" main_awaitFrame"))
        for line in open(base + ".poke"):   # "GAME_FRAME ADDR=VALUE/SIZE,...", one game frame per line
            if line.strip() and not line.startswith("#"):
                game, spec = line.split(maxsplit=1)   # "GAME_FRAME:LANDED": the original goes by GAME_FRAME
                out += ["--poke-at", f"0x{pc}", game.split(":")[0], spec.strip()]
    return out


def write_sites(graph: str, path: str) -> int:
    """The export's indirect and jump sites, one hex address per line."""
    sites = sorted({s["addr"] for s in json.load(open(graph))["sites"] if s["kind"] in ("indirect", "jump")})
    with open(path, "w") as fh:
        fh.write("# indirect call and jump sites from " + os.path.relpath(graph, ROOT) + "\n")
        fh.writelines(s + "\n" for s in sites)
    return len(sites)


def stamp(trace: str, n_sites: int) -> str:
    """The header line saying what a route's edges were observed on."""
    def sha1(path: str) -> str:
        return hashlib.sha1(open(path, "rb").read()).hexdigest()[:16]
    base = trace.removesuffix(".trace")
    parts = [f"trace {sha1(trace)}"] + [f"{ext} {sha1(f'{base}.{ext}')}" for ext in ("sav", "poke")
                                        if os.path.exists(f"{base}.{ext}")]
    return "# observed on: " + ", ".join(parts + [f"original ROM {sha1(ORIG)}", f"{n_sites} sites"]) + "\n"


def finish(part: str, out: str, trace: str, n_sites: int) -> None:
    """mgba_run's output, stamped and with the edges sorted (its order is a hash table's), as OUT."""
    lines = open(part).read().splitlines(keepends=True)
    head = [line for line in lines if line.startswith("#")]
    edges = sorted(line for line in lines if line.strip() and not line.startswith("#"))
    with open(part, "w") as fh:
        fh.writelines(head + [stamp(trace, n_sites)] + edges)
    os.replace(part, out)


def run_route(mgba: str, sites: str, n_sites: int, name: str, frames: int, out_dir: str, graph: str) -> dict:
    trace = os.path.join(TRACES, name + ".trace")
    out = os.path.join(out_dir, name + ".edges")
    started = time.monotonic()
    if os.path.isdir(os.path.join(gamepaths.CHECKPOINTS, name)):
        # a long route with checkpoints (tools/import_route.py): its windows in parallel
        import import_route
        import_route.observe(name, trace, frames, 6, profiles=False, edges_out=out, graph=graph)
        return {"route": name, "edges": sum(1 for line in open(out) if not line.startswith("#")),
                "seconds": round(time.monotonic() - started), "windows": len(import_route.windows(frames))}
    cmd = [mgba, ORIG, "--quiet", "--frames", "200000", "--bios", BIOS, "--input", trace,
           "--game-frame-addr", GAME_FRAME_ADDR, "--until-game-frame", str(frames),
           "--call-edges", sites, out + ".part", *poke_args(trace)]
    done = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if done.returncode != 0 or not os.path.exists(out + ".part"):
        return {"route": name, "error": (done.stdout + done.stderr)[-400:]}
    finish(out + ".part", out, trace, n_sites)
    last = [line for line in done.stdout.splitlines() if line.startswith(("reached", "stalled", "call-edges"))]
    return {"route": name, "edges": sum(1 for line in open(out) if not line.startswith("#")),
            "seconds": round(time.monotonic() - started), "mgba": last}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("graph", help="export of lift.py --export-abi")
    ap.add_argument("routes", nargs="*", help="route names (default: every route in tools/wf/wf/routes.py)")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--mgba", default=gamepaths.MGBA_RUN)
    ap.add_argument("--out-dir", default=os.path.join(TRACES, "coverage"))
    args = ap.parse_args()
    known = routes()
    names = args.routes or list(known)
    unknown = [n for n in names if n not in known]
    if unknown:
        sys.exit(f"unknown routes: {unknown}")
    os.makedirs(args.out_dir, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        sites = os.path.join(tmp, "sites.txt")
        n_sites = write_sites(args.graph, sites)
        with ThreadPoolExecutor(args.jobs) as pool:
            results = list(pool.map(lambda n: run_route(args.mgba, sites, n_sites, n, known[n], args.out_dir, args.graph), names))
    json.dump({"sites": n_sites, "routes": results}, sys.stdout, indent=1)
    print()
    if any("error" in r for r in results):
        sys.exit(1)


if __name__ == "__main__":
    main()
