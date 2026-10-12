#!/usr/bin/env python3
"""Branch coverage of the readable functions' originals, and inputs for the blocks no check reaches.

  tools/blockcov/blockcov.py build                 the coverage build (build-blockcov/work)
  tools/blockcov/blockcov.py measure [--jobs J]    which blocks each check's inputs run: coverage.tsv
  tools/blockcov/blockcov.py fuzz [--jobs J]       step 2: dictionary inputs for the uncovered blocks
  tools/blockcov/blockcov.py synth --functions F   step 3: outside models write inputs (declarative specs)
  tools/blockcov/blockcov.py compare               the new inputs through the difftest and the oracle
  tools/blockcov/blockcov.py report

Blocks are the lifted original's (tools/blockcov/instrument.py). A block counts as covered by a check
when one of the check's inputs that the check compares (in domain) runs it:
  captured  the original game's captured calls, build-snapshots/NAME (`difftest -S`, with its mutators)
  random    verify's 300 random inputs (`difftest -n 300`)
  oracle    the CPU oracle's 24 entry states (tools/oracle.py's draw, seed 1000), counted when mGBA returns
            from the state and the oracle compares it (not out of domain, not `explained`)

New inputs are machine states in the snapshot format (tools/mgba_run --snap: "BN6S", r0-r15, cpsr,
then EWRAM, IWRAM, I/O, palette, VRAM, OAM, SRAM). One counts only when the lifted original runs it in
domain and mGBA runs the original to its return, as the oracle requires; then both the difftest
(`-S`: readable vs lifted) and the oracle (`-G`: readable vs the original on mGBA) compare on it.
Standard library only.
"""
from __future__ import annotations

import argparse
import collections
import concurrent.futures
import datetime
import json
import os
import random
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
TREE = os.path.dirname(TOOLS)
sys.path.insert(0, HERE)
sys.path.insert(0, TOOLS)
import instrument  # noqa: E402
import gamepaths  # noqa: E402
import oracle  # noqa: E402

GAME = os.path.join(TREE, "cport", "src", "game")
LIFTED = os.path.join(TREE, "cport", "src", "lifted")


def main_checkout() -> str:
    common = subprocess.run(["git", "-C", TREE, "rev-parse", "--path-format=absolute", "--git-common-dir"],
                            capture_output=True, text=True, check=True).stdout.strip()
    return os.path.dirname(common)


MAIN = main_checkout()
SNAPSHOTS = os.path.join(MAIN, "build-snapshots")
TOOLCHAIN_BIN = os.path.join(MAIN, "toolchain", "bin")
OUT = os.environ.get("BLOCKCOV_OUT", os.path.join(TREE, "build-blockcov"))
WORK = os.path.join(OUT, "work")
DIFFTEST = os.path.join(WORK, "cport", "build-cov", "difftest")
ENV = {**os.environ, "PATH": f"{TOOLCHAIN_BIN}:{os.environ['PATH']}", "BN6F_ROM": gamepaths.ORIG_ROM}
TRIALS = 300
ORACLE_STATES = 24
REGIONS = [(0x02000000, 0x40000), (0x03000000, 0x8000), (0x04000000, 0x400), (0x05000000, 0x400),
           (0x06000000, 0x18000), (0x07000000, 0x400), (0x0E000000, 0x8000)]   # cport/host/memmap.c


def now() -> str:
    return datetime.datetime.now().isoformat(timespec="seconds")


def read_json(path: str):
    with open(path) as fh:
        return json.load(fh)


def write_json(path: str, data) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path + ".tmp", "w") as fh:
        json.dump(data, fh)
    os.replace(path + ".tmp", path)


_lock = threading.Lock()


def append_jsonl(path: str, record: dict) -> None:
    with _lock, open(path, "a") as fh:
        fh.write(json.dumps(record) + "\n")


def read_jsonl(path: str) -> list[dict]:
    if not os.path.exists(path):
        return []
    with open(path) as fh:
        return [json.loads(line) for line in fh if line.strip()]


# ── build ───────────────────────────────────────────────────────────────

def cmd_build(_args) -> None:
    """build-blockcov/work: cport with the instrumented originals, compiled with blockcov.h."""
    cport = os.path.join(WORK, "cport")
    if os.path.isdir(WORK):
        shutil.rmtree(WORK)
    os.makedirs(os.path.join(cport, "src", "lifted"))
    blocks = instrument.instrument(LIFTED, os.path.join(OUT, "instrumented"))
    write_json(os.path.join(OUT, "blocks.json"), blocks)
    for name in os.listdir(LIFTED):
        src = os.path.join(OUT, "instrumented", name) if name.startswith("difftest_lifted_") else os.path.join(LIFTED, name)
        os.symlink(src, os.path.join(cport, "src", "lifted", name))
    tree_cport = os.path.join(TREE, "cport")
    os.symlink(os.path.join(tree_cport, "include"), os.path.join(cport, "include"))
    for name in ("runtime", "gba"):
        os.symlink(os.path.join(tree_cport, "src", name), os.path.join(cport, "src", name))
    shutil.copytree(os.path.join(tree_cport, "src", "game"), os.path.join(cport, "src", "game"))
    shutil.copy(os.path.join(HERE, "blockcov.c"), os.path.join(cport, "src", "game", "zz_blockcov.c"))
    shutil.copytree(os.path.join(tree_cport, "host"), os.path.join(cport, "host"))
    os.symlink(os.path.join(MAIN, "bn6f"), os.path.join(WORK, "bn6f"))
    cflags = ("-O1 -g0 -DTARGET_FLAT -DDIFFTEST -DDIFFTEST_COVERAGE -I../include -I. -fno-strict-aliasing "
              f"-w -Werror=implicit-function-declaration -include {os.path.join(HERE, 'blockcov.h')}")
    done = subprocess.run(["make", "-C", os.path.join(cport, "host"), "BUILD=../build-cov", "-j8", f"CFLAGS={cflags}"],
                          capture_output=True, text=True, env=ENV)
    if done.returncode:
        sys.exit("coverage build failed:\n" + (done.stdout + done.stderr)[-3000:])
    print(f"{now()} coverage build ready: {DIFFTEST}; {len(blocks)} originals, "
          f"{sum(len(b['blocks']) for b in blocks.values())} blocks")


# ── running the coverage build ──────────────────────────────────────────

COV_LINE = re.compile(r"^cov (\S+) (\S+) (-?\d+) hits=(\S*)(?: loads=(\S*))?$")


def run_cov(args: list[str], extra_env: dict | None = None, timeout: int = 1800) -> tuple[str, list[dict]]:
    """Run the coverage difftest; its output and its `cov` lines (one per compared trial / state)."""
    done = subprocess.run([DIFFTEST, *args], cwd=os.path.join(WORK, "cport"), capture_output=True, text=True,
                          env={**ENV, **(extra_env or {})}, timeout=timeout)
    trials = []
    for line in done.stdout.splitlines():
        m = COV_LINE.match(line)
        if m:
            loads = []
            if m.group(5):
                for item in m.group(5).split(","):
                    a, s, v = item.split(":")
                    loads.append((int(a, 16), int(s), int(v, 16)))
            trials.append({"name": m.group(1), "label": m.group(2), "t": int(m.group(3)),
                           "hits": [int(x, 16) for x in m.group(4).split(",") if x], "loads": loads})
    return done.stdout, trials


def blocks_of(hits: list[int], leaders: set[int]) -> list[int]:
    return [h for h in hits if h in leaders]


# ── snapshots ───────────────────────────────────────────────────────────

def load_snap(path: str) -> tuple[list[int], bytearray]:
    with open(path, "rb") as fh:
        data = fh.read()
    return list(struct.unpack("<19I", data[:76])), bytearray(data[76:])


def save_snap(path: str, hdr: list[int], mem: bytes) -> None:
    with open(path, "wb") as fh:
        fh.write(struct.pack("<19I", *hdr))
        fh.write(mem)


def mem_offset(addr: int) -> int | None:
    off = 0
    for base, size in REGIONS:
        if base <= addr < base + size:
            return off + addr - base
        off += size
    return None


def poke(mem: bytearray, addr: int, size: int, value: int) -> bool:
    off = mem_offset(addr)
    if off is None or mem_offset(addr + size - 1) is None:
        return False
    mem[off:off + size] = (value & ((1 << (8 * size)) - 1)).to_bytes(size, "little")
    return True


def snapshot_files(folder: str) -> list[str]:
    if not os.path.isdir(folder):
        return []
    return sorted(os.path.join(folder, f) for f in os.listdir(folder)
                  if f.startswith("snap_") and f.endswith(".bin") and not f.endswith(".exit.bin"))


# ── measure ─────────────────────────────────────────────────────────────

_units: dict[str, tuple] = {}
_bases = None
_bases_lock = threading.Lock()


def oracle_dir(name: str) -> str | None:
    """build-blockcov/oracle/NAME: the oracle's 24 states for NAME and the original's exits (made once)."""
    global _bases
    unit = _units.get(name)
    if unit is None:
        return None
    d = os.path.join(OUT, "oracle", name)
    if os.path.exists(os.path.join(d, "done")):
        return d
    with _bases_lock:
        if _bases is None:
            bases, pool = oracle.bases_and_pool(SNAPSHOTS)
            _bases = (bases, pool, oracle.io_template(bases[0]))
    bases, pool, io = _bases
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    oracle.write_contract(os.path.join(d, "contract.txt"), [unit])
    oracle.write_states([unit], bases, pool, 1000, d, io, ORACLE_STATES)
    oracle.run_original(d)
    open(os.path.join(d, "done"), "w").close()
    return d


def oracle_counted(d: str, name: str, output: str, contract: str) -> set[str]:
    """The states the oracle compares: an exit exists, the lifted original runs them in domain (-G with
    ORACLE_LIFTED prints a verdict), and none of oracle.explained's reasons applies."""
    counted = set()
    for line in output.splitlines():
        m = re.match(r"(snap_\w+?_(\d+))\.bin: (ok|MEM|REG)", line)
        if not m:
            continue
        k = m.group(2)
        why = oracle.explained(d, name, k, contract, DIFFTEST) if m.group(3) != "ok" else None
        if why is None:
            counted.add(m.group(1) + ".bin")
    return counted


def measure_one(name: str, blocks: dict) -> dict:
    leaders = set(blocks["blocks"])
    covered: dict[str, set[int]] = {"captured": set(), "random": set(), "oracle": set()}
    _, trials = run_cov(["-n", str(TRIALS), name])
    for t in trials:
        covered["random"].update(blocks_of(t["hits"], leaders))
    snaps = os.path.join(SNAPSHOTS, name)
    if snapshot_files(snaps):
        _, trials = run_cov(["-S", snaps, name])
        for t in trials:
            covered["captured"].update(blocks_of(t["hits"], leaders))
    d = oracle_dir(name)
    if d:
        contract = os.path.join(d, "contract.txt")
        out, trials = run_cov(["-G", d], {"ORACLE_LIFTED": "1", "ORACLE_CONTRACT": contract})
        counted = oracle_counted(d, name, out, contract)
        for t in trials:
            if t["label"] in counted:
                covered["oracle"].update(blocks_of(t["hits"], leaders))
    any_check = set().union(*covered.values())
    return {"name": name, "blocks": sorted(leaders), **{k: sorted(v) for k, v in covered.items()},
            "uncovered": sorted(leaders - any_check)}


def cmd_measure(args) -> None:
    blocks = read_json(os.path.join(OUT, "blocks.json"))
    for unit in oracle.units():
        _units[unit[0]] = unit
    readable = set(re.findall(r"\bNext\s+(\w+)\s*\(\s*void\s*\)\s*\{", "\n".join(
        open(os.path.join(GAME, f)).read() for f in os.listdir(GAME) if f.endswith(".c"))))
    names = sorted(n for n in blocks if n in readable)
    if args.functions:
        names = [n for n in names if n in set(args.functions)]
    out_path = os.path.join(OUT, "coverage.jsonl")
    done = {r["name"] for r in read_jsonl(out_path)}
    todo = [n for n in names if n not in done]
    print(f"{now()} measure: {len(todo)} functions ({len(done)} done), {args.jobs} jobs", flush=True)
    count = {"n": 0}

    def one(name: str) -> None:
        try:
            append_jsonl(out_path, measure_one(name, blocks[name]))
        except Exception as ex:
            print(f"{now()} {name}: {ex!r}", flush=True)
        count["n"] += 1
        if count["n"] % 100 == 0:
            print(f"{now()} {count['n']}/{len(todo)}", flush=True)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        list(pool.map(one, todo))
    write_tsv()


def write_tsv() -> None:
    """build-blockcov/coverage.tsv: one row per function."""
    rows = read_jsonl(os.path.join(OUT, "coverage.jsonl"))
    with open(os.path.join(OUT, "coverage.tsv"), "w") as fh:
        fh.write("function\tblocks\tcaptured\trandom\toracle\tany\tuncovered\tuncovered_blocks\n")
        for r in sorted(rows, key=lambda r: r["name"]):
            n = len(r["blocks"])
            fh.write(f"{r['name']}\t{n}\t{len(r['captured'])}\t{len(r['random'])}\t{len(r['oracle'])}\t"
                     f"{n - len(r['uncovered'])}\t{len(r['uncovered'])}\t{','.join(f'{b:08X}' for b in r['uncovered'])}\n")


# ── new inputs: shared by step 2 (fuzz) and step 3 (synth) ──────────────

IO = 0x40000 + 0x8000          # the I/O registers' offset in a snapshot's memory (after EWRAM, IWRAM)
FREE_STACK = 0x03007500
_io = None


def normalize(mem: bytearray) -> None:
    """As tools/oracle.py's states: interrupts off, non-display I/O as mGBA reads it back, write-only display
    registers 0, so the host and mGBA see the same machine."""
    global _io
    with _bases_lock:
        if _io is None:
            bases, _ = oracle.bases_and_pool(SNAPSHOTS)
            _io = oracle.io_template(bases[0])
    for off in (IO + 0x200, IO + 0x202, IO + 0x208):
        mem[off:off + 2] = b"\0\0"
    mem[IO + 0x58:IO + 0x400] = _io
    for lo, hi in ((0x10, 0x48), (0x4C, 0x50), (0x54, 0x58)):
        mem[IO + lo:IO + hi] = bytes(hi - lo)


def seeds_of(name: str) -> list[tuple[list[int], bytearray]]:
    """The function's real entry states: its captured calls and its oracle states (normalized)."""
    out = []
    for path in snapshot_files(os.path.join(SNAPSHOTS, name))[:40]:
        hdr, mem = load_snap(path)
        normalize(mem)
        out.append((hdr, mem))
    d = os.path.join(OUT, "oracle", name)
    for path in snapshot_files(d):
        out.append(load_snap(path))
    return out


def run_inputs(name: str, states: list[tuple[list[int], bytearray]], folder: str) -> dict[int, dict]:
    """Write STATES as FOLDER/snap_NAME_K.bin and run the coverage difftest on them (`-S`, no mutators):
    K -> its compared trial (hits, loads); states out of domain are missing."""
    os.makedirs(folder, exist_ok=True)
    for f in os.listdir(folder):
        os.remove(os.path.join(folder, f))
    for k, (hdr, mem) in enumerate(states):
        save_snap(os.path.join(folder, f"snap_{name}_{k}.bin"), hdr, mem)
    _, trials = run_cov(["-S", folder, "-m", "0", name], {"BLOCKCOV_LOADS": "1"})
    out = {}
    for t in trials:
        m = re.search(r"snap_\w+?_(\d+)\.bin$", t["label"])
        if m:
            out[int(m.group(1))] = t
    return out


def validate(name: str, folder: str) -> dict[int, str]:
    """mGBA runs the original on every state of FOLDER (snap_NAME_K.bin): K -> "ok" when it returns
    cleanly (an exit, no misaligned load or open-bus read, no input register into the free stack), else
    why not."""
    oracle.run_original(folder)
    verdicts = {}
    for path in snapshot_files(folder):
        k = int(re.search(r"_(\d+)\.bin$", path).group(1))
        hdr, _ = load_snap(path)
        if not os.path.exists(path[:-4] + ".exit.bin"):
            verdicts[k] = "the original does not return"
        elif os.path.exists(path[:-4] + ".bus"):
            verdicts[k] = "the original " + open(path[:-4] + ".bus").readline().strip()
        elif any(FREE_STACK <= hdr[2 + r] < hdr[2 + 13] for r in range(13)):
            verdicts[k] = "an input register points into the free stack"
        else:
            verdicts[k] = "ok"
    return verdicts


def compare(name: str, folder: str) -> dict:
    """The readable function against the lifted original (difftest -S) and against the original on mGBA
    (the oracle, -G) on the states of FOLDER that have an exit."""
    done = subprocess.run([DIFFTEST, "-S", folder, "-m", "0", name], cwd=os.path.join(WORK, "cport"),
                          capture_output=True, text=True, env=ENV, timeout=1800)
    line = next((l for l in done.stdout.splitlines() if l.startswith(name + " ")), "")
    unit = _units.get(name)
    contract = os.path.join(folder, "..", f"{name}.contract")
    oracle_lines = []
    if unit is not None:
        oracle.write_contract(contract, [unit])
        g = subprocess.run([DIFFTEST, "-G", folder], cwd=os.path.join(WORK, "cport"), capture_output=True, text=True,
                           env={**ENV, "ORACLE_CONTRACT": contract}, timeout=1800)
        oracle_lines = [l for l in g.stdout.splitlines() if re.match(r"\s*snap_\w+\.bin: ", l) and not l.startswith("cov ")]
    differ = [l for l in oracle_lines if re.match(r"snap_\w+\.bin: (MEM|REG)", l)]
    return {"difftest": line, "difftest_detail": done.stderr.strip().splitlines()[:12],
            "oracle_differ": differ, "oracle_detail": [l for l in oracle_lines if l.startswith(" ")][:12]}


# ── step 2: a fuzzer's dictionary ───────────────────────────────────────

GENERIC = [0, 1, 2, 0xFF, 0x100, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]


def fuzz_one(name: str, cov: dict, blocks: dict, inputs: tuple[str, ...], rounds: int, batch: int, seed: int) -> dict:
    rng = random.Random(seed)
    target = set(cov["uncovered"])
    leaders = set(blocks["blocks"])
    dictionary = sorted(set(blocks["dict"]) | set(GENERIC))
    work = os.path.join(OUT, "tmp", name)
    keep_dir = os.path.join(OUT, "fuzz", name)
    seeds = seeds_of(name)
    if not seeds:
        return {"name": name, "target": sorted(target), "reached": [], "note": "no seed states"}
    runs = run_inputs(name, seeds, work)
    pool = [(seeds[k], t) for k, t in runs.items()]           # states the lifted original runs in domain
    if not pool:
        return {"name": name, "target": sorted(target), "reached": [], "note": "no seed runs in domain"}
    seen = {frozenset(blocks_of(t["hits"], leaders)) for _, t in pool}
    reached: dict[int, tuple] = {}
    tried = 0
    regs = [int(r[1:]) for r in inputs if re.fullmatch(r"r\d+", r) and r != "r10"]
    for _ in range(rounds):
        if not target - set(reached):
            break
        cands = []
        for _ in range(batch):
            (hdr, mem), trial = rng.choice(pool)
            hdr, mem = list(hdr), bytearray(mem)
            kind = rng.random()
            if regs and kind < 0.6:
                for r in rng.sample(regs, min(len(regs), rng.choice((1, 1, 2)))):
                    hdr[2 + r] = rng.choice(dictionary)
            loads = trial["loads"]
            if loads and kind > 0.3:
                for addr, size, _value in rng.sample(loads, min(len(loads), rng.choice((1, 1, 2, 3)))):
                    poke(mem, addr, size, rng.choice(dictionary))
            normalize(mem)
            cands.append((hdr, mem))
        tried += len(cands)
        results = run_inputs(name, cands, work)
        for k, t in results.items():
            got = frozenset(blocks_of(t["hits"], leaders))
            new = (got & target) - set(reached)
            for b in new:
                reached[b] = cands[k]
            if got not in seen:
                seen.add(got)
                pool.append((cands[k], t))
    # the inputs that reached new blocks: validated on mGBA, then compared
    record = {"name": name, "target": sorted(target), "tried": tried, "reached_host": sorted(reached)}
    if not reached:
        return {**record, "reached": []}
    states = list({id(v): v for v in reached.values()}.values())
    run_inputs(name, states, keep_dir)                         # writes keep_dir/snap_NAME_K.bin
    verdicts = validate(name, keep_dir)
    ok = [k for k, v in verdicts.items() if v == "ok"]
    final = run_inputs_kept(name, keep_dir, ok)
    good = set()
    for k in ok:
        t = final.get(k)
        if t:
            good |= set(blocks_of(t["hits"], leaders)) & target
    return {**record, "reached": sorted(good), "inputs": len(states), "clean": len(ok),
            "rejected": {str(k): v for k, v in verdicts.items() if v != "ok"},
            "compare": compare(name, keep_dir) if ok else None}


def run_inputs_kept(name: str, folder: str, keep: list[int]) -> dict[int, dict]:
    """Drop FOLDER's states not in KEEP (with their exits), then their coverage once more."""
    for path in snapshot_files(folder):
        k = int(re.search(r"_(\d+)\.bin$", path).group(1))
        if k not in keep:
            for suffix in (".bin", ".exit.bin", ".bus"):
                if os.path.exists(path[:-4] + suffix):
                    os.remove(path[:-4] + suffix)
    _, trials = run_cov(["-S", folder, "-m", "0", name])
    out = {}
    for t in trials:
        m = re.search(r"snap_\w+?_(\d+)\.bin$", t["label"])
        if m:
            out[int(m.group(1))] = t
    return out


def contract_inputs() -> dict[str, tuple[str, ...]]:
    import funcindex
    return {n: u.contract.inputs for n, u in funcindex.lifted_units(LIFTED).items()}


def cmd_fuzz(args) -> None:
    blocks = read_json(os.path.join(OUT, "blocks.json"))
    for unit in oracle.units():
        _units[unit[0]] = unit
    inputs = contract_inputs()
    covs = {r["name"]: r for r in read_jsonl(os.path.join(OUT, "coverage.jsonl"))}
    names = sorted(n for n, r in covs.items() if r["uncovered"])
    if args.functions:
        names = [n for n in names if n in set(args.functions)]
    out_path = os.path.join(OUT, "fuzz.jsonl")
    done = {r["name"] for r in read_jsonl(out_path)}
    todo = [n for n in names if n not in done]
    print(f"{now()} fuzz: {len(todo)} functions with uncovered blocks ({len(done)} done)", flush=True)
    count = {"n": 0}

    def one(name: str) -> None:
        try:
            rec = fuzz_one(name, covs[name], blocks[name], inputs.get(name, ()), args.rounds, args.batch, 1)
            append_jsonl(out_path, rec)
        except Exception as ex:
            print(f"{now()} {name}: {ex!r}", flush=True)
        count["n"] += 1
        if count["n"] % 50 == 0:
            print(f"{now()} {count['n']}/{len(todo)}", flush=True)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        list(pool.map(one, todo))


# ── step 3: outside models write inputs (declarative specs) ─────────────

RETURNS = re.compile(r"^(pop \{[^}]*pc\}|bx lr|mov pc, lr|bx r\d+)")
SYSTEM = ("You write test inputs for functions of the Game Boy Advance game Mega Man Battle Network 6 (ARM7TDMI, "
          "THUMB code). An input is the machine state at the function's entry: a real state from the game, with "
          "some registers and memory changed. Your input must make the original function reach a given basic "
          "block, and the original must still run it normally to its return (no crash, no misaligned load, no "
          "read of unmapped memory). Reply with the input as JSON in a ```json block, nothing else is used.")
SPEC_HELP = """The input format (JSON):
{"base": N,                                   which base state to start from (below)
 "registers": {"r0": "0x...", ...},           r0-r12 only (r10 is the game's toolkit pointer: keep it)
 "memory": [{"addr": "0x0203A000", "size": 1, "value": "0x05"}, ...]}   sizes 1, 2 or 4, little endian;
                                              EWRAM 0x02000000-0x0203FFFF, IWRAM 0x03000000-0x03007FFF
Everything you do not set keeps the base state's value. Memory the function reads through pointers must stay
consistent: change the bytes it loads, not the pointers it follows, unless you also lay out what they point to."""


def cfg(name: str, blocks: dict, branch_list: list) -> tuple[dict[int, list], dict[int, int]]:
    """Successors per block (leader -> [(next leader, condition or None, taken)]), and each instruction's block."""
    insns = [a for a, _ in blocks["insns"]]
    asm = {a: t for a, t in blocks["insns"]}
    leaders = set(blocks["blocks"])
    block_of, current = {}, insns[0] if insns else 0
    for a in insns:
        if a in leaders:
            current = a
        block_of[a] = current
    gotos = collections.defaultdict(list)
    for at, target, cond in branch_list:
        gotos[at].append((target, cond))
    succ: dict[int, list] = collections.defaultdict(list)
    for i, a in enumerate(insns):
        b = block_of[a]
        for target, cond in gotos.get(a, ()):
            if target in leaders:
                succ[b].append((target, cond, True))
        nxt = insns[i + 1] if i + 1 < len(insns) else None
        ends = RETURNS.match(asm[a]) or any(cond is None for _, cond in gotos.get(a, ()))
        if nxt is not None and nxt in leaders and not ends:
            conds = [cond for _, cond in gotos.get(a, ()) if cond]
            succ[b].append((nxt, " and ".join(conds) if conds else None, False if conds else None))
    return succ, block_of


def path_to(succ: dict, entry: int, target: int) -> list[tuple[int, int, str | None, bool | None]] | None:
    """The branch decisions on a shortest path from ENTRY to TARGET: [(from, to, condition, taken)]."""
    prev = {entry: None}
    todo = collections.deque([entry])
    while todo:
        b = todo.popleft()
        if b == target:
            break
        for nb, cond, taken in succ.get(b, ()):
            if nb not in prev:
                prev[nb] = (b, cond, taken)
                todo.append(nb)
    if target not in prev:
        return None
    out = []
    b = target
    while prev[b] is not None:
        a, cond, taken = prev[b]
        out.append((a, b, cond, taken))
        b = a
    return list(reversed(out))


def describe_base(k: int, hdr: list[int], trial: dict, leaders: set[int]) -> str:
    regs = " ".join(f"r{r}=0x{hdr[2 + r]:08X}" for r in range(13))
    reached = ", ".join(f"0x{b:08X}" for b in blocks_of(trial["hits"], leaders))
    loads = []
    seen = set()
    for addr, size, value in trial["loads"]:
        if (addr, size) in seen:
            continue
        seen.add((addr, size))
        loads.append(f"[0x{addr:08X}]{size}=0x{value:X}")
        if len(loads) == 40:
            break
    return (f"base {k}: {regs}\n  reaches blocks: {reached}\n"
            f"  the original's loads on this run ([address]size=value): {' '.join(loads) or 'none'}")


def build_state(spec: dict, bases: list[tuple[list[int], bytearray]]) -> tuple[list[int], bytearray] | str:
    """The state a spec describes, or why it is not one."""
    try:
        k = int(spec.get("base", 0))
        hdr, mem = list(bases[k][0]), bytearray(bases[k][1])
        for reg, value in (spec.get("registers") or {}).items():
            r = int(str(reg).lower().lstrip("r"))
            if not 0 <= r <= 12:
                return f"register {reg}: only r0-r12 can be set"
            hdr[2 + r] = int(str(value), 0) & 0xFFFFFFFF
        for w in spec.get("memory") or []:
            addr, size, value = int(str(w["addr"]), 0), int(w.get("size", 4)), int(str(w["value"]), 0)
            if size not in (1, 2, 4) or not (0x02000000 <= addr < 0x02040000 or 0x03000000 <= addr < 0x03008000):
                return f"memory write at {w['addr']}: only EWRAM or IWRAM, sizes 1, 2, 4"
            poke(mem, addr, size, value)
    except (ValueError, KeyError, IndexError, TypeError) as ex:
        return f"not a valid input: {ex!r}"
    normalize(mem)
    return hdr, mem


def parse_spec(reply: str) -> dict | None:
    m = re.findall(r"```(?:json)?\s*(\{.*?\})\s*```", reply, re.S)
    for text in reversed(m or [reply]):
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            continue
    return None


def usage_of(tag: str) -> dict:
    """The usage line llm.py wrote for TAG (tokens in and out)."""
    path = os.path.join(MAIN, "tools", "convert", "usage.jsonl")
    with open(path, "rb") as fh:
        fh.seek(0, 2)
        fh.seek(max(0, fh.tell() - 4_000_000))
        lines = fh.read().decode(errors="replace").splitlines()
    for line in reversed(lines):
        if f'"tag": "{tag}"' in line:
            try:
                return json.loads(line)
            except json.JSONDecodeError:
                return {}
    return {}


def synth_block(name: str, target: int, model: str, rounds: int, blocks: dict, branch_list: list,
                bases: list, base_runs: dict, folder: str) -> dict:
    import llm  # tools/llm.py
    import uuid
    leaders = set(blocks["blocks"])
    succ, block_of = cfg(name, blocks, branch_list)
    entry = blocks["insns"][0][0]
    path = path_to(succ, entry, target)
    asm_lines = "\n".join(f"{'>>' if block_of.get(a) == target else '  '} {a:08X}: {t}" for a, t in blocks["insns"])
    if path:
        decisions = "\n".join(f"  block 0x{a:08X} -> 0x{b:08X}: " +
                              (f"the branch `{cond}` {'taken' if taken else 'not taken'}" if cond else "falls through")
                              for a, b, cond, taken in path)
    else:
        decisions = "  (no static path found: the block may be reached through a jump table or a callee's result)"
    # the bases that get furthest along the path
    on_path = {a for a, *_ in path} | {target} if path else {target}
    ranked = sorted(base_runs.items(), key=lambda kv: -len(set(blocks_of(kv[1]["hits"], leaders)) & on_path))[:3]
    bases_text = "\n".join(describe_base(k, bases[k][0], t, leaders) for k, t in ranked)
    history = []
    record = {"name": name, "block": target, "model": model, "rounds": [], "reached": False}
    for rnd in range(rounds):
        prompt = (f"Function {name}. Its instructions (the target block marked >>):\n{asm_lines}\n\n"
                  f"Target: the block at 0x{target:08X}. A path to it from the entry:\n{decisions}\n\n"
                  f"Base states you can start from (real calls of the function):\n{bases_text}\n\n{SPEC_HELP}\n"
                  + ("".join(history)) +
                  "\nWrite the input that reaches the target block, as JSON in a ```json block.")
        tag = f"blockcov {name} {target:08X} round {rnd} {uuid.uuid4().hex[:8]}"
        try:
            reply = llm.complete(model, prompt, system=SYSTEM, max_tokens=16000, timeout=600,
                                 usage_log=os.path.join(MAIN, "tools", "convert", "usage.jsonl"), tag=tag,
                                 fields={"function": name, "round": rnd, "prompt": "blockcov"})
        except Exception as ex:
            record["rounds"].append({"error": repr(ex)[:300]})
            break
        used = usage_of(tag)
        step = {"in": used.get("in"), "out": used.get("out"), "seconds": used.get("seconds")}
        spec = parse_spec(reply)
        if spec is None:
            step["result"] = "no JSON input in the reply"
            history.append(f"\nYour earlier reply had no JSON input.\n")
            record["rounds"].append(step)
            continue
        step["spec"] = spec
        state = build_state(spec, [(h, m) for h, m in bases])
        if isinstance(state, str):
            step["result"] = state
            history.append(f"\nYour earlier input {json.dumps(spec)} was refused: {state}\n")
            record["rounds"].append(step)
            continue
        runs = run_inputs(name, [state], folder)
        trial = runs.get(0)
        if trial is None:
            step["result"] = "the original crashed or did not return to its caller on it (out of domain)"
            history.append(f"\nYour earlier input {json.dumps(spec)}: {step['result']}. Change less.\n")
            record["rounds"].append(step)
            continue
        got = blocks_of(trial["hits"], leaders)
        if target in got:
            verdict = validate(name, folder).get(0, "?")
            step["result"] = "reached" if verdict == "ok" else f"reached, but on mGBA: {verdict}"
            record["rounds"].append(step)
            if verdict == "ok":
                record["reached"] = True
                record["compare"] = compare(name, folder)
                keep = os.path.join(OUT, "synth", name)
                os.makedirs(keep, exist_ok=True)
                for f in os.listdir(folder):
                    shutil.copy(os.path.join(folder, f), os.path.join(keep, f.replace(f"_{name}_0", f"_{name}_{target:X}")))
                break
            history.append(f"\nYour earlier input {json.dumps(spec)} reached the block on the port's model of the "
                           f"original, but the original on the real machine model {verdict}. Change it so it runs "
                           "cleanly.\n")
            continue
        loads = " ".join(f"[0x{a:08X}]{s}=0x{v:X}" for a, s, v in trial["loads"][:40])
        step["result"] = f"not reached; last block 0x{got[-1]:08X}" if got else "not reached"
        history.append(f"\nYour earlier input {json.dumps(spec)} did not reach 0x{target:08X}. It reached blocks "
                       f"{', '.join(f'0x{b:08X}' for b in got)} (the last one 0x{got[-1]:08X}); the original loaded "
                       f"{loads}\n" if got else f"\nYour earlier input {json.dumps(spec)} reached no block.\n")
        record["rounds"].append(step)
    return record


def cmd_synth(args) -> None:
    blocks = read_json(os.path.join(OUT, "blocks.json"))
    branch_map = instrument.branches(LIFTED)
    for unit in oracle.units():
        _units[unit[0]] = unit
    covs = {r["name"]: r for r in read_jsonl(os.path.join(OUT, "coverage.jsonl"))}
    fuzzed = {r["name"]: r for r in read_jsonl(os.path.join(OUT, "fuzz.jsonl"))}
    with open(args.functions) as fh:
        names = [line.split()[0] for line in fh if line.strip() and not line.startswith("#")]
    out_path = os.path.join(OUT, "synth.jsonl")
    done = {(r["name"], r["block"]) for r in read_jsonl(out_path)}
    jobs = []
    for name in names:
        left = set(covs[name]["uncovered"]) - set(fuzzed.get(name, {}).get("reached", []))
        for b in sorted(left)[:args.blocks]:
            if (name, b) not in done:
                jobs.append((name, b))
    print(f"{now()} synth: {len(jobs)} blocks in {len(names)} functions, model {args.model}", flush=True)
    sys.path.insert(0, TOOLS)

    def one(job: tuple[str, int]) -> None:
        name, target = job
        try:
            bases = seeds_of(name)
            folder = os.path.join(OUT, "tmp-synth", f"{name}-{target:X}")
            base_runs = run_inputs(name, bases, folder)
            if not base_runs:
                append_jsonl(out_path, {"name": name, "block": target, "model": args.model, "rounds": [],
                                        "reached": False, "note": "no base state runs in domain"})
                return
            rec = synth_block(name, target, args.model, args.rounds, blocks[name], branch_map.get(name, []),
                              bases, base_runs, folder)
            append_jsonl(out_path, rec)
            print(f"{now()} {name} {target:08X}: {'reached' if rec['reached'] else 'not reached'} "
                  f"in {len(rec['rounds'])} rounds", flush=True)
        except Exception as ex:
            print(f"{now()} {name} {target:08X}: {ex!r}", flush=True)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        list(pool.map(one, jobs))


# ── grading with the mutation survey ────────────────────────────────────

def merge_inputs(name: str) -> str | None:
    """build-blockcov/inputs/NAME: the clean new inputs of steps 2 and 3 (snapshots and the original's exits)."""
    out = os.path.join(OUT, "inputs", name)
    if os.path.isdir(out):
        shutil.rmtree(out)
    k = 0
    for src in (os.path.join(OUT, "fuzz", name), os.path.join(OUT, "synth", name)):
        for path in snapshot_files(src):
            exit_path = path[:-4] + ".exit.bin"
            if not os.path.exists(exit_path) or os.path.exists(path[:-4] + ".bus"):
                continue
            os.makedirs(out, exist_ok=True)
            shutil.copy(path, os.path.join(out, f"snap_{name}_{k}.bin"))
            shutil.copy(exit_path, os.path.join(out, f"snap_{name}_{k}.exit.bin"))
            k += 1
    return out if k else None


def cmd_grade(args) -> None:
    """Kill the survey's surviving mutants (a reanchored plan: tools/mutate/mutate.py reanchor) with the new
    inputs: difftest -S and the oracle (-G) on build-blockcov/inputs/ENTRY, mutant against unmutated."""
    os.environ["MUTATE_OUT"] = args.mutate_out
    sys.path.insert(0, os.path.join(TOOLS, "mutate"))
    import mutate   # noqa: E402  # tools/mutate/mutate.py
    import report   # noqa: E402
    for unit in oracle.units():
        _units[unit[0]] = unit
    plan_, base_, results_, ls_ = report.load(args.survey)
    survivors = {r["id"] for r in results_ if r["status"] == "tested"
                 and not any(report.verdicts(r, base_, ls_)[c] == "killed" for c in report.CHEAP)}
    plan = read_json(os.path.join(args.mutate_out, "plan.json"))
    cands = [c for c in plan["candidates"] if c["id"] in survivors]
    entries = sorted({e for c in cands for e in c["touched"]})
    inputs = {e: merge_inputs(e) for e in entries}
    todo = [c for c in cands if any(inputs.get(e) for e in c["touched"])]
    print(f"{now()} grade: {len(cands)} surviving mutants carried over, {len(todo)} in functions with new inputs", flush=True)
    w = mutate.Worker(70)
    w.setup()

    def checks(entries: list[str]) -> dict:
        out = {}
        for e in entries:
            d = inputs.get(e)
            if not d:
                continue
            ds = subprocess.run([w.difftest, "-S", d, "-m", "0", e], cwd=w.cport, capture_output=True, text=True,
                                env=w.env, timeout=900)
            line = next((l for l in ds.stdout.splitlines() if l.startswith(e + " ")), "")
            m = re.search(r"(\d+) failures", line)
            contract = os.path.join(d, "..", f"{e}.contract")
            oracle.write_contract(contract, [_units[e]])
            g = subprocess.run([w.difftest, "-G", d], cwd=w.cport, capture_output=True, text=True,
                               env={**w.env, "ORACLE_CONTRACT": contract}, timeout=900)
            differ = len(re.findall(r"^snap_\w+\.bin: (?:MEM|REG)", g.stdout, re.M))
            out[e] = {"difftest": int(m.group(1)) if m else -1, "oracle": differ}
        return out

    base_by_file: dict[str, dict] = {}
    out_path = os.path.join(OUT, "grade.jsonl")
    done = {r["id"] for r in read_jsonl(out_path)}
    for c in todo:
        if c["id"] in done:
            continue
        if c["file"] not in base_by_file:
            w.restore(c["file"])
            w.build_host()
            base_by_file[c["file"]] = checks(sorted({e for x in todo if x["file"] == c["file"] for e in x["touched"]}))
        with open(os.path.join(w.game, c["file"]), "w", encoding="utf-8") as fh:
            fh.write(mutate.mutant_text(c))
        if w.build_host():
            append_jsonl(out_path, {"id": c["id"], "status": "build failed"})
            continue
        got = checks(c["touched"])
        base = base_by_file[c["file"]]
        killed = {e: v for e, v in got.items() if v["difftest"] > base.get(e, {}).get("difftest", 0)
                  or v["oracle"] > base.get(e, {}).get("oracle", 0)}
        append_jsonl(out_path, {"id": c["id"], "op": c["op"], "cls": c["cls"], "entries": c["touched"],
                                "killed": bool(killed), "by": killed, "base": {e: base.get(e) for e in got}, "got": got})
        w.restore(c["file"])
    print(f"{now()} grade done", flush=True)


def main() -> None:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("build")
    p = sub.add_parser("measure")
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("functions", nargs="*")
    p = sub.add_parser("fuzz")
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--rounds", type=int, default=8)
    p.add_argument("--batch", type=int, default=48)
    p.add_argument("functions", nargs="*")
    p = sub.add_parser("synth")
    p.add_argument("--functions", required=True, help="file of function names")
    p.add_argument("--model", default="hyper/glm-5.3-flash")
    p.add_argument("--rounds", type=int, default=3)
    p.add_argument("--blocks", type=int, default=2, help="uncovered blocks per function to try")
    p.add_argument("--jobs", type=int, default=4)
    p = sub.add_parser("grade")
    p.add_argument("--survey", default=os.path.join(TREE, "build-mutate"), help="the survey's build-mutate")
    p.add_argument("--mutate-out", default=os.path.join(TREE, "build-mutate-pilot"),
                   help="its mutants reanchored on this tree (mutate.py reanchor)")
    args = ap.parse_args()
    {"build": cmd_build, "measure": cmd_measure, "fuzz": cmd_fuzz, "synth": cmd_synth, "grade": cmd_grade}[args.cmd](args)


if __name__ == "__main__":
    main()
