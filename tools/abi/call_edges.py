#!/usr/bin/env python3
"""Observe indirect call edges on the recorded routes (docs/C_CALLING_CONVENTION.md 3.2).

    tools/abi/call_edges.py build/abi/graph.json [ROUTE...] [--jobs N] [--mgba build-tools/mgba_run]

Takes the indirect sites (calls through a register and computed jumps) from an export
of `lift.py --export-abi`, replays every route of tools/wf/wf/routes.py (or the ones
named) on the original ROM with `mgba_run --call-edges`, and writes
tools/traces/coverage/ROUTE.edges ("SITE TARGET LR COUNT", hex) next to the coverage
profiles. Then merge them into the export:

    python3 tools/lifter/lift.py --export-abi build/abi/graph.json --call-edges tools/traces/coverage/*.edges

Prints a JSON summary: per route the distinct edges and seconds taken.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TRACES = os.path.join(ROOT, "tools", "traces")
ORIG = os.path.join(ROOT, "bn6f", "bn6f.gba")
BIOS = os.path.join(ROOT, "gbarecomp", "bios", "gba_bios.bin")
GAME_FRAME_ADDR = "0x020093D4"


def routes() -> dict[str, int]:
    """Route name -> game frames, from tools/wf/wf/routes.py (without importing the wf venv)."""
    text = open(os.path.join(ROOT, "tools", "wf", "wf", "routes.py")).read()
    return {name: int(n) for name, n in re.findall(r'_route\("(\w+)", (\d+)\)', text)}


def poke_args(trace: str) -> list[str]:
    """tools/common.sh poke_args for the original ROM: the trace's save and .poke writes."""
    base = trace.removesuffix(".trace")
    out = ["--save", base + ".sav"] if os.path.exists(base + ".sav") else []
    if os.path.exists(base + ".poke"):
        game, spec = open(base + ".poke").read().split(maxsplit=1)
        nm = subprocess.run(["arm-none-eabi-nm", ORIG.removesuffix(".gba") + ".elf"], capture_output=True,
                            text=True, check=True).stdout
        pc = next(line.split()[0] for line in nm.splitlines() if line.endswith(" main_awaitFrame"))
        out += ["--poke-at", f"0x{pc}", game, spec.strip()]
    return out


def write_sites(graph: str, path: str) -> int:
    """The export's indirect and jump sites, one hex address per line."""
    sites = sorted({s["addr"] for s in json.load(open(graph))["sites"] if s["kind"] in ("indirect", "jump")})
    with open(path, "w") as fh:
        fh.write("# indirect call and jump sites from " + os.path.relpath(graph, ROOT) + "\n")
        fh.writelines(s + "\n" for s in sites)
    return len(sites)


def run_route(mgba: str, sites: str, name: str, frames: int, out_dir: str) -> dict:
    trace = os.path.join(TRACES, name + ".trace")
    out = os.path.join(out_dir, name + ".edges")
    started = time.monotonic()
    cmd = [mgba, ORIG, "--quiet", "--frames", "200000", "--bios", BIOS, "--input", trace,
           "--game-frame-addr", GAME_FRAME_ADDR, "--until-game-frame", str(frames),
           "--call-edges", sites, out + ".part", *poke_args(trace)]
    done = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if done.returncode != 0 or not os.path.exists(out + ".part"):
        return {"route": name, "error": (done.stdout + done.stderr)[-400:]}
    os.replace(out + ".part", out)
    last = [line for line in done.stdout.splitlines() if line.startswith(("reached", "stalled", "call-edges"))]
    return {"route": name, "edges": sum(1 for line in open(out) if not line.startswith("#")),
            "seconds": round(time.monotonic() - started), "mgba": last}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("graph", help="export of lift.py --export-abi")
    ap.add_argument("routes", nargs="*", help="route names (default: every route in tools/wf/wf/routes.py)")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--mgba", default=os.path.join(ROOT, "build-tools", "mgba_run"))
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
            results = list(pool.map(lambda n: run_route(args.mgba, sites, n, known[n], args.out_dir), names))
    json.dump({"sites": n_sites, "routes": results}, sys.stdout, indent=1)
    print()
    if any("error" in r for r in results):
        sys.exit(1)


if __name__ == "__main__":
    main()
