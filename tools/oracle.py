#!/usr/bin/env python3
"""CPU-oracle test of lifted functions, without gameplay.

  tools/oracle.py --base SNAPDIR [--states K] [--jobs N] [--out DIR] [FUNC...]

For every lifted function (or the ones named), K entry states are built
from real game memory (snapshots captured in the original game, under
SNAPDIR/*/snap_*.bin) with registers drawn from values seen in real calls
(r10 stays the game's toolkit pointer, SP a real stack). mGBA runs the
original function from each state to its return (build-tools/mgba_run
--call-snaps); the port's lifted (or readable) function runs from the same
state on the host (cport difftest -G) and must leave the same memory and the
same registers and flags that its callers rely on (the lifter's contract).
States the function does not return from, or where the host faults on an
address the GBA would treat as open bus, are not counted. Nor are states
that differ only because the random input left the domain a port is held to
(`explained`): the original made a misaligned load or read open bus, an
input register points into the free stack below SP, or the port's lifted
original cannot run the state either (a jump through a word past its table).

Writes DIR/report.txt (one line per function) and keeps the snapshots of
every failing call in DIR/fail/; the explained states are listed in
DIR/explained.txt, their snapshots in DIR/explained/.
"""
import argparse, glob, os, random, re, shutil, struct, subprocess, sys, tempfile
from concurrent.futures import ProcessPoolExecutor

import gamepaths  # tools/gamepaths.py: the game profile (tools/paths.env)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HOST = os.path.join(ROOT, "cport")
MAGIC_LR = 0x080000C1           # ROM header: never executed by the game
IO = 0x40000 + 0x8000           # offset of the I/O registers in a snapshot's memory (after EWRAM, IWRAM)
FREE_STACK = 0x03007500         # lowest stack address, as cport/host/difftest.c's dead stack (0x03007900 - 0x400)


def units():
    out = []
    for path in glob.glob(os.path.join(HOST, "src/lifted/*.c")):
        text = open(path).read()
        for m in re.finditer(r"^/\* (\w+): ([0-9A-F]{8})-([0-9A-F]{8}) (THUMB|ARM).*?\n"
                             r"/\*\s+inputs:\s*([^\n]*)\n \*\s+outputs:\s*([^\n]*)\n"
                             r" \*\s+clobbers[^:]*:\s*([^\n]*)\n", text, re.M):
            name, a, b, mode = m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4)
            if b - a >= 0x10000 or name.startswith("dead_"):
                continue
            outs, clob = m.group(6).split(), m.group(7).split()
            mask = sum(1 << i for i in range(13) if f"r{i}" not in clob)
            flags = (1 if "nz" in outs else 0) | (2 if "c" in outs else 0) | (4 if "v" in outs else 0)
            out.append((name, a, mode == "THUMB", mask, flags))
    return sorted(out, key=lambda u: u[1])


def load_snap(path):
    d = open(path, "rb").read()
    return list(struct.unpack("<19I", d[:76])), d[76:]


def io_template(base):
    """What mGBA's I/O reads back after --call-snaps loads a snapshot (a fresh
    core with only the display registers, below 0x58, from the snapshot): run
    nullsub_58 (bx lr) and keep the exit snapshot's I/O from 0x58 on. The host
    gets the same, so code reading sound, timer, serial or DMA registers sees
    the same values on both sides."""
    hdr, mem = base
    with tempfile.TemporaryDirectory(dir=os.environ.get("ORACLE_TMP")) as d:
        regs = list(hdr[2:17])
        regs[14] = MAGIC_LR
        with open(os.path.join(d, "snap_io_0.bin"), "wb") as f:
            f.write(struct.pack("<19I", 0x53364E42, 1, *regs, 0x0801BFEC, 0x9F | 0x20))
            f.write(mem)
        subprocess.run([gamepaths.MGBA_RUN, gamepaths.ORIG_ROM, "--quiet",
                        "--frames", "1", "--bios", gamepaths.GBA_BIOS,
                        "--call-snaps", d, "1000"], capture_output=True, check=True)
        _, ex = load_snap(os.path.join(d, "snap_io_0.exit.bin"))
        return ex[IO + 0x58:IO + 0x400]


def explained(d, name, k, contract=None, difftest=None, detail=None):
    """Why a differing state is out of the port's domain, or None. Readable code
    is typed C, held to what the game's real calls do; three kinds of random
    state leave that, and the difference they make is the harness's:
      misaligned load: the original loaded a halfword or word from an odd
        address (a random pointer; real ones are aligned). The ARM7 rotates
        it; the port's C reads what its type says (on the host a plain read).
      open bus: the original read an unused or write-only I/O register or
        unmapped memory (mGBA's bad-load messages), which the GBA answers
        with the last prefetched opcode; the host maps those addresses as
        plain memory.
      free stack: an input register points below SP, into memory the
        original's own pushes overwrite before it reads it there; the port's
        stack frames are not the original's.
    The first two come from mgba_run (snap_*.bus, written for the original's
    run), the third from the entry registers; a fourth and a fifth run the port's
    lifted original on the state (lifted_run): it cannot run it either, or it
    differs from the original in exactly the same bytes and registers as the
    readable code (DETAIL: the readable run's difference lines), so the difference
    is the lifted code's or the harness's (e.g. IWRAM data the lifted code reads
    from its ROM source), not the readable function's."""
    bus = os.path.join(d, f"snap_{name}_{k}.bus")
    if os.path.exists(bus):
        note = open(bus).readline().strip()
        return ("misaligned load" if note.startswith("misaligned") else "open bus") + f" ({note})"
    hdr, _ = load_snap(os.path.join(d, f"snap_{name}_{k}.bin"))
    sp = hdr[2 + 13]
    for r in range(13):
        if FREE_STACK <= hdr[2 + r] < sp:
            return f"free stack (r{r} = {hdr[2 + r]:#x}, SP = {sp:#x})"
    lifted = lifted_run(d, name, k, contract, difftest)
    if lifted is None:
        return None
    if re.search(rf"snap_{name}_{k}\.bin: out of domain", lifted):
        return "the lifted original cannot run it either (e.g. a jump-table index past the table)"
    same = re.findall(rf"^\s+snap_{name}_{k}\.bin: (.*)$", lifted, re.M)
    if detail and sorted(same) == sorted(detail):
        return "the lifted original differs the same way (the lifted code's or the harness's difference)"
    return None


def lifted_run(d, name, k, contract=None, difftest=None):
    """For readable functions: the output of the port's lifted original (its executable model of the
    original) on this state, or None. A fourth kind of state: the lifted original faults or leaves the
    domain too, e.g. when a random index sends the original through a word past its jump table, which on
    the GBA happens to land in code; the difftest counts a random input the original cannot run as out of
    domain, and this is the same rule. A fifth: it differs from the original exactly as the readable code
    does (2026-10-12: a random panel type read word 0xA0 of an IWRAM table, which the lifted code takes
    from the ROM source). Not with --lifted (the lifted original is what is being compared then)."""
    if os.environ.get("ORACLE_LIFTED"):
        return None
    with tempfile.TemporaryDirectory(dir=os.environ.get("ORACLE_TMP")) as one:
        for suffix in (".bin", ".exit.bin"):
            src = os.path.join(d, f"snap_{name}_{k}{suffix}")
            if not os.path.exists(src):
                return None
            shutil.copy(src, os.path.join(one, f"snap_{name}_{k}{suffix}"))
        env = dict(os.environ, BN6F_ROM=gamepaths.ORIG_ROM, ORACLE_CONTRACT=contract or CONTRACT, ORACLE_LIFTED="1")
        r = subprocess.run([difftest or os.environ.get("ORACLE_DIFFTEST", os.path.join(HOST, "build-host/difftest")), "-G", one,
                            name], capture_output=True, text=True, cwd=HOST, env=env)
        return r.stdout


def write_states(work, bases, pool, seed, d, io, states):
    """STATES entry states per function of WORK in D (snap_NAME_K.bin), drawn from SEED."""
    rng = random.Random(seed)
    for name, addr, thumb, mask, flags in work:
        for k in range(states):
            hdr, mem = bases[rng.randrange(len(bases))]
            regs = list(hdr[2:17])
            for r in range(13):
                if r == 10:
                    continue                        # the toolkit pointer, as in every call
                x = rng.random()
                if x < 0.5:
                    regs[r] = rng.choice(pool[r])
                elif x < 0.75:
                    regs[r] = rng.randrange(17)
                elif x < 0.9:
                    regs[r] = rng.choice(pool[rng.randrange(13)])
                else:
                    regs[r] = rng.getrandbits(32)
            regs[14] = MAGIC_LR if thumb else MAGIC_LR & ~1
            cpsr = (rng.getrandbits(4) << 28) | 0x9F | (0x20 if thumb else 0)
            h = [0x53364E42, 1] + regs + [addr, cpsr]
            with open(os.path.join(d, f"snap_{name}_{k}.bin"), "wb") as f:
                f.write(struct.pack("<19I", *h))
                f.write(mem)
                # interrupts off on both sides (mgba_run --call-snaps runs with IME = IE = 0)
                for off in (IO + 0x200, IO + 0x202, IO + 0x208):
                    f.seek(76 + off)
                    f.write(b"\0\0")
                # non-display I/O as mGBA reads it back (sound, timers, serial, DMA...)
                f.seek(76 + IO + 0x58)
                f.write(io)
                # write-only display registers read back as 0 (BG offsets and affine
                # parameters, window coordinates, MOSAIC, BLDY)
                for lo, hi in ((0x10, 0x48), (0x4C, 0x50), (0x54, 0x58)):
                    f.seek(76 + IO + lo)
                    f.write(bytes(hi - lo))


def run_original(d):
    """The original game's side: mGBA runs every state in D to its return (snap_*.exit.bin)."""
    try:   # calls without an exit snapshot (no return, or cut off here) are not counted
        subprocess.run([gamepaths.MGBA_RUN, gamepaths.ORIG_ROM, "--quiet",
                        "--frames", "1", "--bios", gamepaths.GBA_BIOS,
                        "--call-snaps", d, "200000"], capture_output=True, text=True,
                       timeout=300)   # seconds normally; a call stuck inside one emulator step never ends
    except subprocess.TimeoutExpired:
        pass


def compare(d, work, keep_dir, contract, difftest=None):
    """The port's side: the host difftest (-G) runs the same states and compares them with the
    original's exits. Returns NAME -> {ok, fail, ood, why, explained}; keeps the snapshots of
    failing and explained states under KEEP_DIR (none kept if KEEP_DIR is None). D is only read,
    so one set of states (and the original's exits) can be compared with several builds."""
    results = {}
    env = dict(os.environ, BN6F_ROM=gamepaths.ORIG_ROM, ORACLE_CONTRACT=contract)
    r = subprocess.run([difftest or os.environ.get("ORACLE_DIFFTEST", os.path.join(HOST, "build-host/difftest")), "-G", d],
                       capture_output=True, text=True,
                       cwd=HOST, env=env)
    detail = {}
    for line in r.stdout.splitlines():
        m = re.match(r"\s+snap_(\w+?)_(\d+)\.bin: (.*)", line)
        if m:
            detail.setdefault(m.group(1), []).append(m.group(3))
            continue
        m = re.match(r"snap_(\w+?)_(\d+)\.bin: (ok|MEM|REG|out of domain)", line)
        if not m:
            continue
        name, k, verdict = m.group(1), m.group(2), m.group(3)
        res = results.setdefault(name, {"ok": 0, "fail": 0, "ood": 0, "why": [], "explained": []})
        why = (explained(d, name, k, contract, difftest, detail.get(name)) if verdict in ("MEM", "REG")
               else None)
        if verdict == "ok":
            res["ok"] += 1
        elif verdict == "out of domain" or why:
            res["ood"] += 1
        else:
            res["fail"] += 1
            if len(res["why"]) < 3:
                res["why"].append(f"{verdict}: " + "; ".join(detail.get(name, [])[:3]))
        if why:
            res["explained"].append(f"{name} {k} {why}: {verdict}: " + "; ".join(detail.get(name, [])[:3]))
        if verdict in ("MEM", "REG") and keep_dir is not None:
            keep = os.path.join(os.path.dirname(keep_dir), "explained") if why else keep_dir
            os.makedirs(keep, exist_ok=True)
            for suffix in (".bin", ".exit.bin", ".bus"):
                src = os.path.join(d, f"snap_{name}_{k}{suffix}")
                if os.path.exists(src):
                    shutil.copy(src, os.path.join(keep, f"snap_{name}_{k}{suffix}"))
        detail.pop(name, None)
    for name, *_ in work:
        results.setdefault(name, {"ok": 0, "fail": 0, "ood": 0, "why": [], "explained": []})
    return results


def batch(args):
    work, bases, pool, seed, keep_dir, io = args
    with tempfile.TemporaryDirectory(dir=os.environ.get("ORACLE_TMP")) as d:
        write_states(work, bases, pool, seed, d, io, STATES)
        run_original(d)
        return compare(d, work, keep_dir, CONTRACT)


def promised_contracts(cfg):
    """Hand-written functions promise what cport/host/difftest.cfg compares (compare=rA,...,nz,c,v),
    not everything the lifted contract lists: NAME -> (register mask, flags)."""
    promised = {}
    for line in open(cfg):
        f = line.split()
        if f and not line.startswith("#") and f[-1].startswith("compare="):
            items = f[-1][len("compare="):].split(",")
            promised[f[0]] = (sum(1 << int(r[1:]) for r in items if r[0] == "r"),
                              (1 if "nz" in items else 0) | (2 if "c" in items else 0) | (4 if "v" in items else 0))
    return promised


def write_contract(path, us, cfg=os.path.join(HOST, "host", "difftest.cfg")):
    """The ORACLE_CONTRACT file for units US: what each function's callers rely on."""
    promised = promised_contracts(cfg)
    with open(path, "w") as f:
        for name, addr, thumb, mask, flags in us:
            if name in promised:
                mask, flags = promised[name]
            f.write(f"{addr:08x} {mask:x} {flags:x}\n")


def bases_and_pool(base_dir):
    """The memory the entry states start from (8 captured calls of the snapshot store, chosen with
    seed 1) and the register values seen in real calls (a sample of about 300), per register."""
    # a linked folder is a function's calls under its new name (tools/snapshot_names.py): counted once
    snaps = sorted(p for p in glob.glob(os.path.join(base_dir, "*", "snap_*.bin"))
                   if not p.endswith(".exit.bin") and not os.path.islink(os.path.dirname(p)))
    rng = random.Random(1)
    base_paths = rng.sample(snaps, min(8, len(snaps)))
    bases = [load_snap(p) for p in base_paths]
    pool = [set() for _ in range(13)]
    for p in snaps[:: max(1, len(snaps) // 300)]:
        hdr, _ = load_snap(p)
        for r in range(13):
            pool[r].add(hdr[2 + r])
    return bases, [sorted(s) for s in pool]


STATES = 4
CONTRACT = ""


def main():
    global STATES, CONTRACT
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--states", type=int, default=4)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--batch", type=int, default=80)
    ap.add_argument("--out", default=os.path.join(ROOT, "tools/oracle_out"))
    ap.add_argument("--lifted", action="store_true",
                    help="run the lifted originals of hand-written functions (baseline for a difference)")
    ap.add_argument("funcs", nargs="*")
    args = ap.parse_args()
    STATES = args.states
    if args.lifted:
        os.environ["ORACLE_LIFTED"] = "1"
    os.makedirs(args.out, exist_ok=True)
    us = units()
    if args.funcs:
        us = [u for u in us if u[0] in set(args.funcs)]
    CONTRACT = os.path.join(args.out, "contract.txt")
    write_contract(CONTRACT, us)
    bases, pool = bases_and_pool(args.base)
    chunks = [us[i:i + args.batch] for i in range(0, len(us), args.batch)]
    print(f"{len(us)} functions, {args.states} states each, {len(chunks)} batches", flush=True)
    totals = {}
    keep = os.path.join(args.out, "fail")
    with ProcessPoolExecutor(args.jobs, initializer=_init, initargs=(STATES, CONTRACT)) as ex:
        io = io_template(bases[0])
        for i, res in enumerate(ex.map(batch, [(c, bases, pool, 1000 + n, keep, io) for n, c in enumerate(chunks)])):
            totals.update(res)
            tested = sum(1 for r in totals.values() if r["ok"] + r["fail"])
            failing = sum(1 for r in totals.values() if r["fail"])
            print(f"batch {i + 1}/{len(chunks)}: {tested} functions compared, {failing} with differences", flush=True)
    with open(os.path.join(args.out, "report.txt"), "w") as f:
        for name, addr, *_ in us:
            r = totals.get(name, {"ok": 0, "fail": 0, "ood": 0, "why": []})
            f.write(f"{name} {addr:08x} ok={r['ok']} fail={r['fail']} ood={r['ood']} {' | '.join(r['why'])}\n")
    notes = [line for r in totals.values() for line in r.get("explained", [])]
    with open(os.path.join(args.out, "explained.txt"), "w") as f:
        f.write("".join(line + "\n" for line in sorted(notes)))
    tested = [n for n, r in totals.items() if r["ok"] + r["fail"]]
    failing = [n for n, r in totals.items() if r["fail"]]
    print(f"{len(notes)} differing states out of the port's domain (misaligned load, open bus, free stack, "
          "lifted original out of domain): "
          f"see {args.out}/explained.txt")
    print(f"{len(tested)} of {len(us)} functions compared on at least one state; {len(failing)} differ: see {args.out}/report.txt")


def _init(states, contract):
    global STATES, CONTRACT
    STATES, CONTRACT = states, contract


if __name__ == "__main__":
    main()
