#!/usr/bin/env python3
"""Mutation testing of the port's checks (review 1 A24; joint plan S3 "start"): how many small,
realistic bugs in readable functions does each check catch?

  tools/mutate/mutate.py catalog                    readable entries: size, captured calls, routes
  tools/mutate/mutate.py plan [--functions N] [--per-function K] [--seed S]
                                                    a stratified sample of functions, K mutants each
  tools/mutate/mutate.py run [--jobs J]             build each mutant, run the cheap checks
  tools/mutate/mutate.py lockstep [--jobs J] [--routes-per-mutant R] [--select FILE]
                                                    rebuild the ROM per mutant, replay the routes
  tools/mutate/mutate.py report                     kill rates per check, per class, combinations
  tools/mutate/mutate.py show ID                    one mutant: its diff and what each check said

A mutant is one edit (tools/mutate/operators.py: the bug classes this project has had) to one
cport/src/game file. Mutants are built in copies of cport under build-mutate/work/wK (never in the
tree's own cport/): the host difftest is relinked per mutant (under a second), the ROM only for
lockstep. A mutant whose GBA object code is identical to the original's is equivalent and not
counted (trivial compiler equivalence); one that does not compile is invalid.

The checks, as the verify workflow runs them (tools/wf/wf/graphs/verify.py), on the `Next`
entries whose own code the edit changes (tools/mutate/csrc.py touched_entries):
  random     host difftest, 300 random inputs (`difftest -n 300 ENTRY...`)
  captured   host difftest on the original's captured calls (`difftest -S build-snapshots/ENTRY`)
  oracle     CPU oracle, 24 entry states from the snapshot store (tools/oracle.py); the original's
             side (mGBA) is computed once per entry and cached in build-mutate/oracle/ENTRY
  leftovers  tools/abi/leftovers.py --check on the file (reads a register a callee does not return)
  warnings   (extra) a new GCC warning in the file at -Wall -Wextra (review 1 A23)
  lockstep   tools/lockstep.sh on the recorded routes that run a touched entry in the original
             (tools/traces/coverage/*.prof; a route that never runs the code cannot see the change)

Every check first runs on the unmutated code of each sampled entry (the baseline); a check kills a
mutant when it reports a difference the baseline did not. Results are appended to
build-mutate/results.jsonl and lockstep.jsonl, so an interrupted run resumes where it stopped.
Standard library only.
"""
from __future__ import annotations

import argparse
import collections
import concurrent.futures
import datetime
import glob
import hashlib
import json
import os
import random
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, TOOLS)
import csrc  # noqa: E402
import operators  # noqa: E402
import funcindex  # noqa: E402  # tools/funcindex.py
import gamepaths  # noqa: E402  # tools/gamepaths.py
import oracle  # noqa: E402  # tools/oracle.py

TREE = os.path.dirname(TOOLS)
GAME = os.path.join(TREE, "cport", "src", "game")
LIFTED = os.path.join(TREE, "cport", "src", "lifted")


def main_checkout() -> str:
    """The main checkout (shared, git-ignored data lives there: the snapshot store, the coverage profiles)."""
    common = subprocess.run(["git", "-C", TREE, "rev-parse", "--path-format=absolute", "--git-common-dir"],
                            capture_output=True, text=True, check=True).stdout.strip()
    return os.path.dirname(common)


MAIN = main_checkout()
SNAPSHOTS = os.path.join(MAIN, "build-snapshots")
COVERAGE = os.path.join(MAIN, "tools", "traces", "coverage")
GRAPH = os.path.join(MAIN, "build-abi", "graph.json")
TOOLCHAIN_BIN = os.path.join(MAIN, "toolchain", "bin")
OUT = os.environ.get("MUTATE_OUT", os.path.join(TREE, "build-mutate"))

# tools/wf/wf/routes.py (kept in step by hand: tools/wf imports pydantic, this tool only the standard library)
ROUTES = {"battles1": 20111, "battles2": 20609, "battle": 20000, "menus2": 23042, "menus3": 12950,
          "story1": 9700, "story2": 33546, "story3": 17576, "story4": 33703, "story5": 52712,
          "story6": 36237, "story7": 12601, "story8": 22494, "story9": 38715, "title_mash_a": 8000,
          "wander": 8000}
TRIALS = 300          # verify's random difftest trials
ORACLE_STATES = 24    # verify's oracle states per function
GBA_CFLAGS = ["-mcpu=arm7tdmi", "-mthumb", "-mthumb-interwork", "-ffreestanding", "-fno-strict-aliasing",
              "-fomit-frame-pointer", "-DTARGET_GBA", "-Iinclude", "-Wall", "-Wno-unused-label",
              "-Wno-unused-variable", "-Wno-array-bounds", "-Wno-comment", "-Werror=return-type",
              "-Wno-unused-but-set-variable", "-Os"]
STRICT = ["-Wextra", "-Wno-unused-parameter"]          # the warnings check (A23)
CHEAP = ("random", "captured", "oracle", "leftovers", "warnings")
LTO_THREADS = 6       # the ROM is byte-identical whatever the count; fewer threads share the machine


def now() -> str:
    return datetime.datetime.now().isoformat(timespec="seconds")


def sha1(text: str) -> str:
    return hashlib.sha1(text.encode()).hexdigest()


def read_json(path: str):
    with open(path) as fh:
        return json.load(fh)


def write_json(path: str, data) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w") as fh:
        json.dump(data, fh, indent=1)
    os.replace(tmp, path)


def read_jsonl(path: str) -> list[dict]:
    if not os.path.exists(path):
        return []
    with open(path) as fh:
        return [json.loads(line) for line in fh if line.strip()]


_append_lock = threading.Lock()


def append_jsonl(path: str, record: dict) -> None:
    with _append_lock:
        with open(path, "a") as fh:
            fh.write(json.dumps(record) + "\n")


# ── catalog ─────────────────────────────────────────────────────────────

def route_coverage(units: dict[str, funcindex.Unit]) -> dict[str, list[str]]:
    """Unit name -> the routes whose run of the original executes it (coverage profiles)."""
    ranges = {n: (u.lo, u.hi) for n, u in units.items() if u.hi - u.lo < funcindex.BOGUS_SPAN}
    find = funcindex.unit_finder(ranges)
    covered: dict[str, set[str]] = collections.defaultdict(set)
    for route in ROUTES:
        prof = os.path.join(COVERAGE, f"{route}.prof")
        if not os.path.exists(prof):
            prof = os.path.join(OUT, "coverage", f"{route}.prof")
        if not os.path.exists(prof):
            prof = make_profile(route)
        with open(prof) as fh:
            for line in fh:
                if line[0] != "#":
                    name = find(int(line.split()[0], 16))
                    if name:
                        covered[name].add(route)
    return {n: sorted(r) for n, r in covered.items()}


def make_profile(route: str) -> str:
    """A route's coverage profile of the original, for routes without one (written to build-mutate/coverage)."""
    out_dir = os.path.join(OUT, "coverage")
    os.makedirs(out_dir, exist_ok=True)
    frames = ROUTES[route]
    trace = os.path.join(MAIN, "tools", "traces", f"{route}.trace")
    poke = subprocess.run(["bash", "-c", f'. "{TOOLS}/common.sh"; poke_args "$1" "$2"', "_", gamepaths.ORIG_ROM, trace],
                          capture_output=True, text=True, check=True).stdout.split()
    with tempfile.TemporaryDirectory() as d:
        subprocess.run([gamepaths.MGBA_RUN, gamepaths.ORIG_ROM, "--quiet", "--frames", "200000",
                        "--bios", gamepaths.GBA_BIOS, "--input", trace, "--game-frame-addr", gamepaths.GAME_FRAME_ADDR,
                        "--until-game-frame", str(frames + 10), "--profile-game", "0", str(frames), *poke],
                       cwd=d, capture_output=True, check=True)
        shutil.move(os.path.join(d, "profile.txt"), os.path.join(out_dir, f"{route}.prof"))
    return os.path.join(out_dir, f"{route}.prof")


def snapshot_count(name: str) -> int:
    d = os.path.join(SNAPSHOTS, name)
    if not os.path.isdir(d):
        return 0
    return sum(1 for e in os.scandir(d) if e.name.startswith("snap_") and not e.name.endswith(".exit.bin"))


def size_band(instructions: int) -> str:
    return "S" if instructions <= 20 else "M" if instructions <= 60 else "L" if instructions <= 200 else "XL"


def cmd_catalog(_args) -> None:
    units = funcindex.lifted_units(LIFTED)
    covered = route_coverage(units)
    entries = []
    for path in sorted(glob.glob(os.path.join(GAME, "*.c"))):
        cf = csrc.parse(path)
        for fn in cf.functions.values():
            if not fn.is_next:
                continue
            unit = units.get(fn.name)
            owned = cf.owned_by(fn.name)
            sites: collections.Counter[str] = collections.Counter()
            for name in owned:
                for e in operators.proposals(cf, cf.functions[name]):
                    sites[e.cls] += 1
            size = (unit.hi - unit.lo) // 2 if unit and unit.hi - unit.lo < funcindex.BOGUS_SPAN else 0   # halfwords (THUMB instructions)
            entries.append({
                "name": fn.name, "file": os.path.basename(path), "part": fn.part,
                "address": f"{unit.lo:08x}" if unit else None, "halfwords": size, "band": size_band(size),
                "lines": cf.line_of(fn.body_close) - cf.line_of(fn.start) + 1,
                "owned": sorted(owned), "sites": dict(sites),
                "captures": snapshot_count(fn.name), "routes": covered.get(fn.name, []),
                "contract": unit.contract.text() if unit else None,
            })
    write_json(os.path.join(OUT, "catalog.json"), {"made": now(), "tree": git_head(), "entries": entries})
    bands = collections.Counter(e["band"] for e in entries)
    print(f"{len(entries)} readable entries; sizes {dict(bands)}; with captured calls "
          f"{sum(1 for e in entries if e['captures'])}; on a route {sum(1 for e in entries if e['routes'])}")


def git_head() -> str:
    return subprocess.run(["git", "-C", TREE, "rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip()


# ── plan ────────────────────────────────────────────────────────────────

# how many functions to draw from each stratum (size band, has captured calls), out of --functions;
# every entry no route runs is added on top
SHARES = {("S", True): 0.18, ("S", False): 0.18, ("M", True): 0.18, ("M", False): 0.16,
          ("L", True): 0.15, ("L", False): 0.08, ("XL", True): 0.05, ("XL", False): 0.02}
CANDIDATES_PER_FUNCTION = 14   # ranked; `run` tests them in order until --per-function compile and differ


def cmd_plan(args) -> None:
    catalog = read_json(os.path.join(OUT, "catalog.json"))
    rng = random.Random(args.seed)
    entries = [e for e in catalog["entries"] if e["address"] and sum(e["sites"].values())]
    strata: dict[tuple[str, bool], list[dict]] = collections.defaultdict(list)
    for e in entries:
        if e["routes"]:
            strata[(e["band"], e["captures"] > 0)].append(e)
    chosen: list[dict] = []
    per_file: collections.Counter[str] = collections.Counter()
    for key, share in SHARES.items():
        pool = strata.get(key, [])
        rng.shuffle(pool)
        want = max(1, round(share * args.functions))
        taken = 0
        for e in pool:                      # spread over files: at most --per-file from one file
            if taken == want:
                break
            if per_file[e["file"]] >= args.per_file:
                continue
            chosen.append({**e, "stratum": f"{key[0]}/{'captured' if key[1] else 'no captures'}"})
            per_file[e["file"]] += 1
            taken += 1
    for e in entries:
        if not e["routes"]:
            chosen.append({**e, "stratum": f"{e['band']}/{'captured' if e['captures'] else 'no captures'}/no route"})
    # mutants: per function, classes in the order that keeps the classes balanced over the sample
    class_count: collections.Counter[str] = collections.Counter()
    candidates = []
    files: dict[str, csrc.CFile] = {}
    for e in chosen:
        path = os.path.join(GAME, e["file"])
        cf = files.setdefault(path, csrc.parse(path))
        by_class: dict[str, list[operators.Edit]] = collections.defaultdict(list)
        for name in sorted(e["owned"]):
            for edit in operators.proposals(cf, cf.functions[name]):
                by_class[edit.cls].append(edit)
        for edits in by_class.values():
            rng.shuffle(edits)
        rank = 0
        while rank < CANDIDATES_PER_FUNCTION and any(by_class.values()):
            open_classes = [c for c in operators.CLASSES if by_class.get(c)]
            # fewest so far first; ties broken at random
            cls = min(open_classes, key=lambda c: (class_count[c], rng.random()))
            edit = by_class[cls].pop()
            # one site per operator per function first, so a function's mutants differ in kind
            class_count[cls] += 1 if rank < args.per_function else 0
            touched = sorted(cf.touched_entries(edit.function))
            candidates.append({
                "id": f"m{len(candidates):04d}", "entry": e["name"], "file": e["file"], "rank": rank,
                "cls": edit.cls, "op": edit.op, "function": edit.function, "start": edit.start, "end": edit.end,
                "replacement": edit.replacement, "note": edit.note, "line": cf.line_of(edit.start),
                "original": cf.text[edit.start:edit.end], "touched": touched, "file_sha1": sha1(cf.text),
            })
            rank += 1
    plan = {"made": now(), "tree": git_head(), "seed": args.seed, "per_function": args.per_function,
            "functions": [{k: e[k] for k in ("name", "file", "band", "halfwords", "captures", "routes", "stratum", "sites")}
                          for e in chosen],
            "candidates": candidates}
    write_json(os.path.join(OUT, "plan.json"), plan)
    strata_count = collections.Counter(e["stratum"] for e in chosen)
    print(f"{len(chosen)} functions from {len(per_file) or len(set(e['file'] for e in chosen))} files, "
          f"{len(candidates)} candidate mutants")
    for k, v in sorted(strata_count.items()):
        print(f"  {k}: {v}")
    print("first-rank classes:", dict(class_count))


# ── workers: copies of cport to build mutants in ────────────────────────

class Worker:
    """build-mutate/work/wK: cport with its own src/game and host/ (copies) and build directories;
    the lifted output, headers and runtime are links to the tree's (read only)."""

    def __init__(self, k: int):
        self.k = k
        self.root = os.path.join(OUT, "work", f"w{k}")
        self.cport = os.path.join(self.root, "cport")
        self.game = os.path.join(self.cport, "src", "game")
        self.difftest = os.path.join(self.cport, "build-host", "difftest")
        self.rom = os.path.join(self.cport, "build", "bn6f_lifted.gba")
        self.env = {**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}", "BN6F_ROM": gamepaths.ORIG_ROM}

    def setup(self) -> None:
        tree_cport = os.path.join(TREE, "cport")
        os.makedirs(os.path.join(self.cport, "src"), exist_ok=True)
        links = {os.path.join(self.root, "bn6f"): os.path.join(MAIN, "bn6f"),
                 os.path.join(self.root, "tools"): TOOLS,
                 os.path.join(self.cport, "include"): os.path.join(tree_cport, "include"),
                 os.path.join(self.cport, "src", "lifted"): LIFTED,
                 os.path.join(self.cport, "src", "runtime"): os.path.join(tree_cport, "src", "runtime"),
                 os.path.join(self.cport, "src", "gba"): os.path.join(tree_cport, "src", "gba")}
        for link, target in links.items():
            if not os.path.islink(link):
                os.symlink(target, link)
        for name in ("Makefile", "gba.ld"):
            shutil.copy2(os.path.join(tree_cport, name), os.path.join(self.cport, name))
        for name in ("game", "host"):
            src = os.path.join(tree_cport, "src", "game") if name == "game" else os.path.join(tree_cport, "host")
            dst = self.game if name == "game" else os.path.join(self.cport, "host")
            if os.path.isdir(dst):
                shutil.rmtree(dst)
            shutil.copytree(src, dst)
            # newer than any object built here before: a run stopped mid-mutant leaves that mutant's
            # objects behind, and copies keeping the tree's older times would let make reuse them
            for name_ in os.listdir(dst):
                os.utime(os.path.join(dst, name_))
        if self.build_host() is not None:
            raise RuntimeError(f"worker {self.k}: the unmutated host difftest does not build")

    def restore(self, file: str) -> None:
        shutil.copy2(os.path.join(GAME, file), os.path.join(self.game, file))
        os.utime(os.path.join(self.game, file))     # newer than the mutant's objects: make rebuilds

    def build_host(self) -> str | None:
        done = subprocess.run(["make", "-C", os.path.join(self.cport, "host"), "BUILD=../build-host", "-j2"],
                              capture_output=True, text=True, env=self.env)
        return None if done.returncode == 0 else (done.stdout + done.stderr)[-2000:]

    def build_rom(self) -> str | None:
        ldflags = (f"-mcpu=arm7tdmi -mthumb -mthumb-interwork -nostartfiles -T gba.ld -Wl,--gc-sections "
                   f"-Wl,-Map,build/bn6f_lifted.map -flto={LTO_THREADS} -O2")
        done = subprocess.run(["make", "-C", self.cport, "BUILD=build", "-j4", f"LDFLAGS={ldflags}"],
                              capture_output=True, text=True, env=self.env)
        return None if done.returncode == 0 else (done.stdout + done.stderr)[-2000:]

    def compile_gba(self, file: str, obj: str) -> tuple[bool, list[str]]:
        """Compile one game file for the GBA without LTO (real code, for the equivalence test), with
        the strict warnings on: (compiled, warnings without their line numbers)."""
        done = subprocess.run(["arm-none-eabi-gcc", *GBA_CFLAGS, *STRICT, "-c", os.path.join("src", "game", file), "-o", obj],
                              cwd=self.cport, capture_output=True, text=True, env=self.env)
        warnings = sorted(re.sub(r"^[^:]*:\d+:\d+: ", "", line) for line in done.stderr.splitlines() if ": warning:" in line)
        return done.returncode == 0, warnings


def object_code(obj: str) -> str:
    """A hash of an object's code and data (the section contents; symbols and names left out)."""
    done = subprocess.run(["arm-none-eabi-objdump", "-s", obj], capture_output=True, text=True,
                          env={**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}"})
    body = "\n".join(line for line in done.stdout.splitlines() if not line.startswith(obj) and "file format" not in line)
    return sha1(body)


# ── the cheap checks ────────────────────────────────────────────────────

RANDOM_LINE = re.compile(r"^(\w+)\s+(ok|FAIL)\s+\((\d+) trials, (\d+) failures, (\d+) out-of-domain\)")
CAPTURED_LINE = re.compile(r"^(\w+)\s+(ok|FAIL)\s+\((\d+) snapshots, (\d+) runs, (\d+) failures, (\d+) out-of-domain\)")


def outcome(tries: int, failures: int, ood: int) -> str:
    return "failed" if failures else "nothing" if ood >= tries else "passed"


def check_random(w: Worker, entries: list[str]) -> dict:
    done = subprocess.run([w.difftest, "-n", str(TRIALS), *entries], cwd=w.cport, capture_output=True, text=True,
                          env=w.env, timeout=1800)
    per = {}
    for m in filter(None, map(RANDOM_LINE.match, done.stdout.splitlines())):
        per[m[1]] = {"outcome": outcome(int(m[3]), int(m[4]), int(m[5])), "failures": int(m[4]), "ood": int(m[5]),
                     "tries": int(m[3])}
    first = next((line.strip() for line in done.stderr.splitlines() if line.startswith("  ") and ":" in line), "")
    return {"entries": per, "why": first[:200]}


def check_captured(w: Worker, entries: list[str]) -> dict:
    per = {}
    why = ""
    for e in entries:
        snaps = os.path.join(SNAPSHOTS, e)
        if not os.path.isdir(snaps) or not snapshot_count(e):
            per[e] = {"outcome": "not run"}
            continue
        done = subprocess.run([w.difftest, "-S", snaps, e], cwd=w.cport, capture_output=True, text=True,
                              env=w.env, timeout=1800)
        m = next(filter(None, map(CAPTURED_LINE.match, done.stdout.splitlines())), None)
        if m is None:
            per[e] = {"outcome": "error", "output": (done.stdout + done.stderr)[-300:]}
            continue
        per[e] = {"outcome": outcome(int(m[4]), int(m[5]), int(m[6])), "failures": int(m[5]), "ood": int(m[6]),
                  "tries": int(m[4])}
        if not why:
            why = next((line.strip() for line in done.stderr.splitlines() if line.startswith("  ") and ":" in line), "")
    return {"entries": per, "why": why[:200]}


_oracle_lock = threading.Lock()
_oracle_ready: dict[str, threading.Event] = {}
_units: dict[str, tuple] = {}
_bases = None


def oracle_states(entry: str) -> str | None:
    """build-mutate/oracle/ENTRY: the oracle's 24 entry states for ENTRY (as `tools/oracle.py
    --states 24 ENTRY` draws them) and the original's exits; made once."""
    global _bases
    unit = _units.get(entry)
    if unit is None:
        return None
    d = os.path.join(OUT, "oracle" if ORACLE_STATES == 24 else f"oracle-{ORACLE_STATES}", entry)
    with _oracle_lock:
        event = _oracle_ready.get(entry)
        mine = event is None
        if mine:
            event = _oracle_ready[entry] = threading.Event()
    if not mine:
        event.wait()
        return d if os.path.exists(os.path.join(d, "done")) else None
    try:
        if not os.path.exists(os.path.join(d, "done")):
            with _oracle_lock:
                if _bases is None:
                    bases, pool = oracle.bases_and_pool(SNAPSHOTS)
                    _bases = (bases, pool, oracle.io_template(bases[0]))
            bases, pool, io = _bases
            if os.path.isdir(d):
                shutil.rmtree(d)
            os.makedirs(d)
            oracle.write_contract(os.path.join(d, "contract.txt"), [unit])
            oracle.write_states([unit], bases, pool, 1000, d, io, ORACLE_STATES)   # seed: oracle.py's first batch
            oracle.run_original(d)
            open(os.path.join(d, "done"), "w").close()
    finally:
        event.set()
    return d


def check_oracle(w: Worker, entries: list[str]) -> dict:
    per = {}
    why = ""
    for e in entries:
        d = oracle_states(e)
        if d is None:
            per[e] = {"outcome": "not run"}
            continue
        contract = os.path.join(OUT, "contracts", f"{e}.txt")
        if not os.path.exists(contract):
            os.makedirs(os.path.dirname(contract), exist_ok=True)
            oracle.write_contract(contract, [_units[e]])
        r = oracle.compare(d, [_units[e]], None, contract, w.difftest)[e]
        compared = r["ok"] + r["fail"]
        per[e] = {"outcome": "failed" if r["fail"] else "nothing" if not compared else "passed",
                  "failures": r["fail"], "ood": r["ood"], "tries": compared + r["ood"]}
        if r["why"] and not why:
            why = r["why"][0]
    return {"entries": per, "why": why[:200]}


_leftovers: dict = {}
_leftovers_lock = threading.Lock()


def check_leftovers(w: Worker, file: str) -> dict:
    """tools/abi/leftovers.py --check on the file, in this process (its contracts and graph.json,
    31 MB, are read once rather than once per mutant)."""
    with _leftovers_lock:
        if not _leftovers:
            sys.path.insert(0, os.path.join(TOOLS, "abi"))
            import leftovers   # noqa: E402  # tools/abi/leftovers.py
            _leftovers["module"] = leftovers
            _leftovers["contracts"] = leftovers.load_contracts(graph=GRAPH)
        sites = _leftovers["module"].scan([os.path.join(w.game, file)], _leftovers["contracts"])
    hits = [f"{s.function}: {s.reg} after {s.callee} ({s.kind})" for s in sites if not s.noted and s.kind != "zflag"]
    return {"outcome": "failed" if hits else "passed", "sites": sorted(hits)[:20], "count": len(hits)}


def run_cheap(w: Worker, entries: list[str], file: str, checks: tuple[str, ...]) -> tuple[dict, dict]:
    results, seconds = {}, {}
    for name in checks:
        started = time.monotonic()
        if name == "random":
            results[name] = check_random(w, entries)
        elif name == "captured":
            results[name] = check_captured(w, entries)
        elif name == "oracle":
            results[name] = check_oracle(w, entries)
        elif name == "leftovers":
            results[name] = check_leftovers(w, file)
        seconds[name] = round(time.monotonic() - started, 2)
    return results, seconds


# ── run ─────────────────────────────────────────────────────────────────

def load_units() -> None:
    for name, addr, thumb, mask, flags in oracle.units():
        _units[name] = (name, addr, thumb, mask, flags)


def baseline_path() -> str:
    return os.path.join(OUT, "baseline.json")


def cmd_run(args) -> None:
    plan = read_json(os.path.join(OUT, "plan.json"))
    checks = tuple(c for c in args.checks.split(",") if c)
    load_units()
    done_ids = {r["id"] for r in read_jsonl(os.path.join(OUT, "results.jsonl"))}
    by_function: dict[str, list[dict]] = collections.defaultdict(list)
    for c in plan["candidates"]:
        by_function[c["entry"]].append(c)
    for f, cs in by_function.items():
        cs.sort(key=lambda c: c["rank"])
    results = read_jsonl(os.path.join(OUT, "results.jsonl"))
    tested = collections.Counter(r["entry"] for r in results if r["status"] == "tested")
    todo = [f for f in by_function if tested[f] < plan["per_function"] and
            any(c["id"] not in done_ids for c in by_function[f])]
    if args.limit:
        todo = todo[:args.limit]
    print(f"{now()} {len(todo)} functions to do ({len(done_ids)} mutants already done), {args.jobs} workers", flush=True)
    workers = [Worker(k) for k in range(args.jobs)]
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        list(pool.map(lambda w: w.setup(), workers))
    print(f"{now()} workers ready", flush=True)
    baseline = read_json(baseline_path()) if os.path.exists(baseline_path()) else {}
    base_lock = threading.Lock()
    free = collections.deque(workers)
    free_lock = threading.Lock()
    counter = {"done": 0}

    def do_function(f: str) -> None:
        with free_lock:
            w = free.popleft()
        try:
            cs = by_function[f]
            file = cs[0]["file"]
            entries_needed = sorted({e for c in cs for e in c["touched"]})
            # the baseline: every check on the unmutated code of every entry these mutants touch
            with base_lock:
                missing = [e for e in entries_needed if e not in baseline]
            if missing:
                w.restore(file)
                err = w.build_host()
                if err:
                    raise RuntimeError(f"baseline build of {file} failed: {err}")
                res, secs = run_cheap(w, missing, file, ("random", "captured", "oracle"))
                left = check_leftovers(w, file)
                ok, warnings = w.compile_gba(file, os.path.join(w.root, "base.o"))
                with base_lock:
                    for e in missing:
                        baseline[e] = {name: res[name]["entries"].get(e, {"outcome": "missing"}) for name in res}
                    baseline.setdefault("_files", {})[file] = {"leftovers": left, "warnings": warnings,
                                                               "code": object_code(os.path.join(w.root, "base.o"))}
                    write_json(baseline_path(), baseline)
            with base_lock:
                if file not in baseline.get("_files", {}):
                    w.restore(file)
                    left = check_leftovers(w, file)
                    ok, warnings = w.compile_gba(file, os.path.join(w.root, "base.o"))
                    baseline.setdefault("_files", {})[file] = {"leftovers": left, "warnings": warnings,
                                                               "code": object_code(os.path.join(w.root, "base.o"))}
                    write_json(baseline_path(), baseline)
                base_file = baseline["_files"][file]
            got = tested[f]
            for c in cs:
                if got >= plan["per_function"]:
                    break
                if c["id"] in done_ids:
                    continue
                record = run_mutant(w, c, checks, base_file)
                append_jsonl(os.path.join(OUT, "results.jsonl"), record)
                got += record["status"] == "tested"
            w.restore(file)
        finally:
            with free_lock:
                free.append(w)
            counter["done"] += 1
            if counter["done"] % 5 == 0 or counter["done"] == len(todo):
                print(f"{now()} {counter['done']}/{len(todo)} functions", flush=True)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for fut in [pool.submit(do_function, f) for f in todo]:
            try:
                fut.result()
            except Exception as ex:    # one function's trouble must not stop the survey
                print(f"{now()} error: {ex!r}", flush=True)


def mutant_text(c: dict) -> str:
    with open(os.path.join(GAME, c["file"]), encoding="utf-8") as fh:
        text = fh.read()
    if sha1(text) != c["file_sha1"]:
        raise RuntimeError(f"{c['file']} changed since the plan was made")
    return text[:c["start"]] + c["replacement"] + text[c["end"]:]


def run_mutant(w: Worker, c: dict, checks: tuple[str, ...], base_file: dict) -> dict:
    record = {"id": c["id"], "entry": c["entry"], "file": c["file"], "cls": c["cls"], "op": c["op"],
              "touched": c["touched"], "when": now()}
    with open(os.path.join(w.game, c["file"]), "w", encoding="utf-8") as fh:
        fh.write(mutant_text(c))
    obj = os.path.join(w.root, "mutant.o")
    compiled, warnings = w.compile_gba(c["file"], obj)
    if not compiled:
        return {**record, "status": "invalid", "why": "does not compile for the GBA"}
    if object_code(obj) == base_file["code"]:
        return {**record, "status": "equivalent"}
    err = w.build_host()
    if err:
        return {**record, "status": "invalid", "why": "host build: " + err[-300:]}
    if not c["touched"]:
        return {**record, "status": "invalid", "why": "no entry runs this code"}
    res, secs = run_cheap(w, c["touched"], c["file"], tuple(x for x in checks if x != "warnings"))
    if "warnings" in checks:
        before = collections.Counter(base_file["warnings"])
        new = list((collections.Counter(warnings) - before).elements())
        res["warnings"] = {"outcome": "failed" if new else "passed", "new": new[:5]}
    return {**record, "status": "tested", "checks": res, "seconds": secs}


# ── recheck: the same mutants again, with a changed check ────────────────

def cmd_recheck(args) -> None:
    """Run CHECKS again on mutants (default: every tested one) and on their baseline, into
    build-mutate/recheck-NAME.jsonl and recheck-NAME.baseline.json: to measure a change to a check
    (a new difftest, more random trials, more oracle states) or to repeat a run at a quieter time."""
    global TRIALS, ORACLE_STATES
    TRIALS, ORACLE_STATES = args.trials, args.oracle_states
    checks = tuple(c for c in args.checks.split(",") if c)
    plan = read_json(os.path.join(OUT, "plan.json"))
    cands = {c["id"]: c for c in plan["candidates"]}
    tested = [r for r in read_jsonl(os.path.join(OUT, "results.jsonl")) if r["status"] == "tested"]
    if args.ids:
        with open(args.ids) as fh:
            want = {line.split()[0] for line in fh if line.strip() and not line.startswith("#")}
        tested = [r for r in tested if r["id"] in want]
    out_path = os.path.join(OUT, f"recheck-{args.name}.jsonl")
    base_path = os.path.join(OUT, f"recheck-{args.name}.baseline.json")
    done = {r["id"] for r in read_jsonl(out_path)}
    by_file: dict[str, list[dict]] = collections.defaultdict(list)
    for r in tested:
        if r["id"] not in done:
            by_file[r["file"]].append(cands[r["id"]])
    load_units()
    baseline = read_json(base_path) if os.path.exists(base_path) else {}
    base_lock = threading.Lock()
    workers = [Worker(args.worker_base + k) for k in range(args.jobs)]
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        list(pool.map(lambda w: w.setup(), workers))
    free = collections.deque(workers)
    free_lock = threading.Lock()
    groups = [cs[i:i + 8] for cs in by_file.values() for i in range(0, len(cs), 8)]
    print(f"{now()} recheck {args.name}: {sum(len(g) for g in groups)} mutants, checks {checks}", flush=True)
    count = {"n": 0}

    def one(group: list[dict]) -> None:
        with free_lock:
            w = free.popleft()
        try:
            file = group[0]["file"]
            entries = sorted({e for c in group for e in c["touched"]})
            with base_lock:
                missing = [e for e in entries if e not in baseline]
            if missing:
                w.restore(file)
                if w.build_host():
                    raise RuntimeError(f"baseline build of {file} failed")
                res, _ = run_cheap(w, missing, file, checks)
                with base_lock:
                    for e in missing:
                        baseline[e] = {name: res[name]["entries"].get(e, {"outcome": "missing"}) for name in res}
                    write_json(base_path, baseline)
            for c in group:
                with open(os.path.join(w.game, file), "w", encoding="utf-8") as fh:
                    fh.write(mutant_text(c))
                record = {"id": c["id"], "entry": c["entry"], "file": file, "cls": c["cls"], "op": c["op"],
                          "touched": c["touched"], "when": now()}
                err = w.build_host()
                if err:
                    record.update(status="invalid", why=err[-300:])
                else:
                    res, secs = run_cheap(w, c["touched"], file, checks)
                    record.update(status="tested", checks=res, seconds=secs)
                append_jsonl(out_path, record)
            w.restore(file)
        finally:
            with free_lock:
                free.append(w)
            count["n"] += 1
            if count["n"] % 20 == 0 or count["n"] == len(groups):
                print(f"{now()} {count['n']}/{len(groups)} groups", flush=True)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for fut in [pool.submit(one, g) for g in groups]:
            try:
                fut.result()
            except Exception as ex:
                print(f"{now()} error: {ex!r}", flush=True)


# ── the leftover lint with --deep (static: no build) ─────────────────────

def cmd_lint_deep(_args) -> None:
    """tools/abi/leftovers.py with and without --deep on every tested mutant, against the
    unmutated file: build-mutate/leftovers-deep.jsonl ("fail", "listed" (zflag only) or "-")."""
    sys.path.insert(0, os.path.join(TOOLS, "abi"))
    import leftovers   # noqa: E402
    contracts = leftovers.load_contracts(graph=GRAPH)
    plan = read_json(os.path.join(OUT, "plan.json"))
    cands = {c["id"]: c for c in plan["candidates"]}
    out_path = os.path.join(OUT, "leftovers-deep.jsonl")
    done = {r["id"] for r in read_jsonl(out_path)}
    tested = [r for r in read_jsonl(os.path.join(OUT, "results.jsonl")) if r["status"] == "tested" and r["id"] not in done]

    def reads(path: str, deep: bool) -> collections.Counter:
        return collections.Counter((s.function, s.callee, s.reg, s.kind)
                                   for s in leftovers.scan([path], contracts, deep) if not s.noted)

    base: dict[tuple[str, bool], collections.Counter] = {}
    with tempfile.TemporaryDirectory() as d:
        for r in tested:
            c = cands[r["id"]]
            path = os.path.join(d, c["file"])
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(mutant_text(c))
            record = {"id": c["id"]}
            for deep in (False, True):
                key = (c["file"], deep)
                if key not in base:
                    base[key] = reads(os.path.join(GAME, c["file"]), deep)
                new = reads(path, deep) - base[key]
                kinds = {k[3] for k in new}
                record["deep" if deep else "default"] = "fail" if kinds - {"zflag"} else "listed" if kinds else "-"
            append_jsonl(out_path, record)


# ── reanchor: an earlier survey's mutants on today's sources ─────────────

def cmd_reanchor(args) -> None:
    """Carry the tested mutants of an earlier survey (OLD_OUT, its sources in OLD_GAME) onto the
    current cport/src/game under the same ids, where the mutated function's text is unchanged
    (wherever layout has put it) and its touched entries are still readable. Writes OUT/plan.json."""
    old_plan = read_json(os.path.join(args.old_out, "plan.json"))
    tested = {r["id"] for r in read_jsonl(os.path.join(args.old_out, "results.jsonl")) if r["status"] == "tested"}
    old_files: dict[str, csrc.CFile] = {}
    new_files = {os.path.basename(path): csrc.parse(path) for path in sorted(glob.glob(os.path.join(GAME, "*.c")))}
    readable = {n for cf in new_files.values() for n, f in cf.functions.items() if f.is_next}
    kept, why = [], collections.Counter()
    for c in old_plan["candidates"]:
        if c["id"] not in tested:
            continue
        ocf = old_files.setdefault(c["file"], csrc.parse(os.path.join(args.old_game, c["file"])))
        ofn = ocf.functions[c["function"]]
        # helpers of the same name in several parts: find the one whose text matches
        text = ocf.text[ofn.start:ofn.body_close + 1]
        found = None
        for name, cf in new_files.items():
            fn = cf.functions.get(c["function"])
            if fn is not None and cf.text[fn.start:fn.body_close + 1] == text:
                found = (name, cf, fn)
                break
        if found is None:
            why["function text changed or gone"] += 1
            continue
        if not all(e in readable for e in c["touched"]):
            why["a touched entry is no longer readable"] += 1
            continue
        name, cf, fn = found
        shift = fn.start - ofn.start
        touched = sorted(cf.touched_entries(fn.name))
        kept.append({**c, "file": name, "start": c["start"] + shift, "end": c["end"] + shift,
                     "line": cf.line_of(c["start"] + shift), "touched": touched, "file_sha1": sha1(cf.text),
                     "rank": 0})
        why["kept"] += 1
    plan = {**old_plan, "made": now(), "tree": git_head(), "reanchored_from": args.old_out,
            "per_function": 10 ** 6, "candidates": kept}
    write_json(os.path.join(OUT, "plan.json"), plan)
    print(dict(why))


# ── reach probes ────────────────────────────────────────────────────────

def cmd_probe(args) -> None:
    """For each tested mutant's site, a probe in place of the mutation (operators.probe_edit): which
    checks run that statement on an input they compare. Sites shared by several mutants run once."""
    plan = read_json(os.path.join(OUT, "plan.json"))
    cands = {c["id"]: c for c in plan["candidates"]}
    tested = [r for r in read_jsonl(os.path.join(OUT, "results.jsonl")) if r["status"] == "tested"]
    if args.ids:
        with open(args.ids) as fh:
            want = {line.split()[0] for line in fh if line.strip() and not line.startswith("#")}
        tested = [r for r in tested if r["id"] in want]
    out_path = os.path.join(OUT, "probes.jsonl")
    done = {i for r in read_jsonl(out_path) for i in r["ids"]}
    load_units()
    baseline = read_json(baseline_path())
    files: dict[str, csrc.CFile] = {}
    sites: dict[tuple, dict] = {}
    for r in tested:
        if r["id"] in done:
            continue
        c = cands[r["id"]]
        cf = files.setdefault(c["file"], csrc.parse(os.path.join(GAME, c["file"])))
        e = operators.probe_edit(cf, cf.functions[c["function"]], c["start"])
        if e is None:
            continue
        key = (c["file"], e.start, e.end, e.replacement)
        site = sites.setdefault(key, {"file": c["file"], "start": e.start, "end": e.end, "replacement": e.replacement,
                                      "note": e.note, "ids": [], "touched": set(), "file_sha1": c["file_sha1"]})
        site["ids"].append(r["id"])
        site["touched"].update(c["touched"])
    todo = list(sites.values())
    print(f"{now()} {len(todo)} probe sites for {sum(len(s['ids']) for s in todo)} mutants, {args.jobs} workers", flush=True)
    workers = [Worker(args.worker_base + k) for k in range(args.jobs)]
    for w in workers:      # always afresh: a stopped run leaves a mutant in its copy
        w.setup()
    free = collections.deque(workers)
    free_lock = threading.Lock()
    count = {"n": 0}

    def one(site: dict) -> None:
        with free_lock:
            w = free.popleft()
        try:
            entries = sorted(site["touched"])
            with open(os.path.join(w.game, site["file"]), "w", encoding="utf-8") as fh:
                fh.write(mutant_text(site))
            err = w.build_host()
            record = {"ids": site["ids"], "file": site["file"], "note": site["note"], "when": now()}
            if err:
                record["status"] = "invalid"
                record["why"] = err[-300:]
            else:
                res, _ = run_cheap(w, entries, site["file"], ("random", "captured", "oracle"))
                record["status"] = "tested"
                record["reached"] = {}
                for check in ("random", "captured", "oracle"):
                    per = res[check]["entries"]
                    record["reached"][check] = any(
                        m.get("outcome") == "failed" and m.get("failures", 0) > baseline.get(e, {}).get(check, {}).get("failures", 0)
                        for e, m in per.items())
                record["checks"] = res
            w.restore(site["file"])
            append_jsonl(out_path, record)
        finally:
            with free_lock:
                free.append(w)
            count["n"] += 1
            if count["n"] % 25 == 0 or count["n"] == len(todo):
                print(f"{now()} {count['n']}/{len(todo)}", flush=True)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for fut in [pool.submit(one, s) for s in todo]:
            try:
                fut.result()
            except Exception as ex:
                print(f"{now()} error: {ex!r}", flush=True)


# ── lockstep ────────────────────────────────────────────────────────────

LOCKSTEP = os.path.join(TOOLS, "lockstep.sh")


def run_route(rom: str, route: str, out_dir: str, procs: list, procs_lock: threading.Lock) -> dict:
    os.makedirs(out_dir, exist_ok=True)
    started = time.monotonic()
    p = subprocess.Popen([LOCKSTEP, rom, os.path.join(MAIN, "tools", "traces", f"{route}.trace"), str(ROUTES[route])],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                         env={**os.environ, "OUT": out_dir}, start_new_session=True)
    with procs_lock:
        procs.append(p)
    out, _ = p.communicate()
    lines = out.splitlines()
    verdict = next((line for line in lines if line.startswith("FAIL:")), None) or \
        next((line for line in lines if line.startswith("OK:")), None)
    status = "killed" if p.returncode < 0 else "passed" if p.returncode == 0 and verdict and verdict.startswith("OK") \
        else "failed" if verdict else "error"
    return {"route": route, "status": status, "verdict": (verdict or (lines[-1] if lines else ""))[:200],
            "seconds": round(time.monotonic() - started, 1)}


def lockstep_routes(rom: str, routes: list[str], out_dir: str, parallel: int, stop_on_fail: bool) -> list[dict]:
    """Replay ROUTES (longest first) with up to PARALLEL at once; after a failure, stop the rest."""
    procs: list = []
    procs_lock = threading.Lock()
    results = []
    failed = threading.Event()
    order = sorted(routes, key=lambda r: -ROUTES[r])

    def one(route: str) -> dict | None:
        if failed.is_set() and stop_on_fail:
            return {"route": route, "status": "skipped"}
        r = run_route(rom, route, os.path.join(out_dir, route), procs, procs_lock)
        if r["status"] == "failed" and stop_on_fail:
            failed.set()
            with procs_lock:
                for p in procs:
                    if p.poll() is None:
                        try:
                            os.killpg(p.pid, signal.SIGTERM)
                        except ProcessLookupError:
                            pass
        return r

    with concurrent.futures.ThreadPoolExecutor(parallel) as pool:
        for r in pool.map(one, order):
            results.append(r)
    return results


def cmd_lockstep(args) -> None:
    plan = read_json(os.path.join(OUT, "plan.json"))
    cands = {c["id"]: c for c in plan["candidates"]}
    catalog = {e["name"]: e for e in read_json(os.path.join(OUT, "catalog.json"))["entries"]}
    results = {r["id"]: r for r in read_jsonl(os.path.join(OUT, "results.jsonl")) if r["status"] == "tested"}
    out_path = os.path.join(OUT, "lockstep-probe.jsonl" if args.probe else "lockstep.jsonl")
    done = {r["id"] for r in read_jsonl(out_path)}
    files: dict[str, csrc.CFile] = {}
    if args.select:
        with open(args.select) as fh:
            ids = [line.split()[0] for line in fh if line.strip() and not line.startswith("#")]
    else:
        ids = sorted(results)
    ids = [i for i in ids if i not in done and (i in results or i == "baseline")]
    print(f"{now()} lockstep: {len(ids)} mutants, {args.jobs} at once, {args.parallel} routes each", flush=True)
    workers = [Worker(args.worker_base + k) for k in range(args.jobs)]
    for w in workers:      # always afresh: a stopped run leaves a mutant in its copy
        w.setup()
    free = collections.deque(workers)
    free_lock = threading.Lock()
    count = {"n": 0}

    def one(mid: str) -> None:
        with free_lock:
            w = free.popleft()
        try:
            if mid == "baseline":
                c = None
                routes = list(ROUTES)
            else:
                c = cands[mid]
                routes = sorted({r for e in c["touched"] for r in catalog.get(e, {}).get("routes", [])})
                if args.all_routes:
                    routes = list(ROUTES)
                text = mutant_text(c)
                if args.probe:     # the site with a probe that hangs the game: does a route run it?
                    cf = files.setdefault(c["file"], csrc.parse(os.path.join(GAME, c["file"])))
                    e = operators.probe_edit(cf, cf.functions[c["function"]], c["start"], operators.PROBE_GBA)
                    text = cf.text[:e.start] + e.replacement + cf.text[e.end:]
                with open(os.path.join(w.game, c["file"]), "w", encoding="utf-8") as fh:
                    fh.write(text)
            started = time.monotonic()
            err = w.build_rom()
            build_s = round(time.monotonic() - started, 1)
            record = {"id": mid, "when": now(), "routes_covering": routes, "build_seconds": build_s,
                      "all_routes": bool(args.all_routes)}
            if err:
                record.update(status="build failed", why=err[-300:])
            elif not routes:
                record.update(status="no route")
            else:
                rs = lockstep_routes(w.rom, routes, os.path.join(OUT, "lockstep", mid), args.parallel,
                                     stop_on_fail=not args.all_routes)
                record["routes"] = rs
                record["status"] = "failed" if any(r["status"] == "failed" for r in rs) else \
                    "error" if any(r["status"] == "error" for r in rs) else "passed"
            if c is not None:
                w.restore(c["file"])
            append_jsonl(out_path, record)
            shutil.rmtree(os.path.join(OUT, "lockstep", mid), ignore_errors=True)   # screenshots and digests: large
        finally:
            with free_lock:
                free.append(w)
            count["n"] += 1
            print(f"{now()} {count['n']}/{len(ids)} {mid}", flush=True)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for fut in [pool.submit(one, i) for i in ids]:
            try:
                fut.result()
            except Exception as ex:
                print(f"{now()} error: {ex!r}", flush=True)


# ── show ────────────────────────────────────────────────────────────────

def cmd_show(args) -> None:
    plan = read_json(os.path.join(OUT, "plan.json"))
    c = next(c for c in plan["candidates"] if c["id"] == args.id)
    with open(os.path.join(GAME, c["file"]), encoding="utf-8") as fh:
        text = fh.read()
    lo = text.rfind("\n", 0, c["start"]) + 1
    hi = text.find("\n", c["end"])
    print(f"{c['id']} {c['cls']}/{c['op']} in {c['function']} ({c['file']}:{c['line']}), touches {', '.join(c['touched'])}")
    print(f"- {text[lo:hi]}")
    print(f"+ {text[lo:c['start']] + c['replacement'] + text[c['end']:hi]}")
    for path in ("results.jsonl", "lockstep.jsonl"):
        for r in read_jsonl(os.path.join(OUT, path)):
            if r["id"] == args.id:
                print(json.dumps(r, indent=1))


def main() -> None:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("catalog")
    p = sub.add_parser("plan")
    p.add_argument("--functions", type=int, default=120)
    p.add_argument("--per-function", type=int, default=5)
    p.add_argument("--per-file", type=int, default=6)
    p.add_argument("--seed", type=int, default=1)
    p = sub.add_parser("run")
    p.add_argument("--jobs", type=int, default=10)
    p.add_argument("--checks", default=",".join(CHEAP))
    p.add_argument("--limit", type=int, default=0, help="only the first N functions (a trial run)")
    p = sub.add_parser("lockstep")
    p.add_argument("--jobs", type=int, default=3, help="mutants at once (each builds its own ROM)")
    p.add_argument("--parallel", type=int, default=4, help="routes at once per mutant")
    p.add_argument("--select", help="file of mutant ids (first word per line); `baseline` replays the unmutated ROM")
    p.add_argument("--all-routes", action="store_true", help="every route, not only those that run the code; no early stop")
    p.add_argument("--probe", action="store_true",
                   help="replay each site with a probe that hangs the game instead of the mutation (lockstep-probe.jsonl): "
                        "a failing route runs the statement")
    p.add_argument("--worker-base", type=int, default=100)
    p = sub.add_parser("recheck", help="the same mutants with a changed check, into recheck-NAME.jsonl")
    p.add_argument("name")
    p.add_argument("--checks", default="random,captured,oracle")
    p.add_argument("--ids", help="file of mutant ids (default: every tested mutant)")
    p.add_argument("--trials", type=int, default=TRIALS)
    p.add_argument("--oracle-states", type=int, default=ORACLE_STATES)
    p.add_argument("--jobs", type=int, default=2)
    p.add_argument("--worker-base", type=int, default=20, help="worker numbers from here (others may be busy)")
    sub.add_parser("lint-deep", help="the leftover lint with --deep on every tested mutant (static)")
    p = sub.add_parser("reanchor", help="an earlier survey's tested mutants on today's sources (same ids)")
    p.add_argument("old_out", help="the earlier survey's build-mutate directory")
    p.add_argument("old_game", help="a copy of cport/src/game as that survey saw it")
    p = sub.add_parser("probe", help="reach probes: which checks run each mutated statement")
    p.add_argument("--jobs", type=int, default=4)
    p.add_argument("--ids", help="file of mutant ids (default: every tested mutant)")
    p.add_argument("--worker-base", type=int, default=40, help="worker numbers from here (others may be busy)")
    p = sub.add_parser("show")
    p.add_argument("id")
    sub.add_parser("report")
    p = sub.add_parser("tsv", help="one row per tested mutant with every verdict (for git: tools/mutate/survey-*.tsv)")
    p.add_argument("path")
    p.add_argument("--review", default=os.path.join(HERE, "review-20261011.tsv"))
    p.add_argument("--later", help="a reanchored survey of the same mutants on a later tree (its OUT)")
    args = ap.parse_args()
    if args.cmd in ("report", "tsv"):
        import report
        if args.cmd == "report":
            report.main(OUT)
        else:
            report.export(OUT, args.path, args.review, args.later)
        return
    {"catalog": cmd_catalog, "plan": cmd_plan, "run": cmd_run, "lockstep": cmd_lockstep, "show": cmd_show,
     "probe": cmd_probe, "recheck": cmd_recheck, "lint-deep": cmd_lint_deep,
     "reanchor": cmd_reanchor}[args.cmd](args)


if __name__ == "__main__":
    main()
