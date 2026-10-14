#!/usr/bin/env python3
"""The subsystem farm: one job per subsystem (tools/sig/subsystems.json), converted in one go by an outside
model from the knowledge layer, checked, pre-reviewed by outside models, and installed after Claude's review.

    tools/sig/sysfarm.py plan [--min 20] [--max 60] [--covered 0.8] [--only NAME...]   add jobs to the ledger
    tools/sig/sysfarm.py run [--workers 2] [--only NAME...] [--hours H]   work open jobs (setsid it: hours)
    tools/sig/sysfarm.py status                                           the ledger, one line a job
    tools/sig/sysfarm.py install NAME                                     land a reviewed job (main-tree lock)
    tools/sig/sysfarm.py reopen NAME [--why TEXT]                         back to open (a review's reason
                                                                          becomes the next repair's report)

A job (build-wf-state/subsystem-jobs.json) works in its own worktree, build-wf-worktrees/sys-SLUG, at the
main tree's commit when it starts:
  1. the prompt (tools/sig/subsys_prompt.py): signatures (build/sig/final.json, the source of
     symbols/signatures.tsv and the db `signature:` blocks), structs, entry points, assembly, today's C;
     members that return elsewhere (signature `none`) and dead code stay as they are and are called
     through wrappers;
  2. the model of its route writes one file, cport/src/game/sys_SLUG.c; every member's old readable part
     is taken out of the laid-out files;
  3. checks, in order, each failure fed back to the same model as a repair (3 rounds, then the route's next
     model): the file compiles with no warning that changes a value; no register-level code outside the
     wrappers and adapters (tools/abi/lint.py); the lifter accepts it and both builds link; the difftest at
     every entry point (captured calls, then random inputs: a random failure — the lifted original
     completed the same trial and the hand-written version crashed or diverged — is a real divergence and
     blocks, unlike out-of-domain trials where the original itself faults); lockstep on the 16 routes;
     the CPU oracle at the entry points (reported);
  4. on a pass: tools/convert/prereview.py on each member's function (outside models), then a commit in the
     worktree with symbols/db (tools/sig/record_subsystem.py, new entries for members that had none),
     tools/abi/retype_queue.tsv rows of the members removed, the lint baseline lowered, stamps for the
     entry points. State `verified`: waiting for Claude's review of what pre-review flagged;
  5. `install`: the commit, rebased onto the main tree's branch, gate, fast-forward under the main-tree lock.
Routes (by members): MiniMax-M3 first, then Hyper (glm-5.3-flash for up to 30, qwen3.8-flash above), then
zai/glm-5.3 (deepseek-v4-pro, the pilot's model, is out of OpenCode Go's monthly quota). Tokens go to build/sig/usage.jsonl, tagged.
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import datetime
import fcntl
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import time
from contextlib import contextmanager

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAIN = "/home/alex/Code/recomp" if os.path.isdir("/home/alex/Code/recomp/.git") else ROOT
LEDGER = os.path.join(MAIN, "build-wf-state/subsystem-jobs.json")
WORKTREES = os.path.join(MAIN, "build-wf-worktrees")
SIG = os.path.join(MAIN, "build/sig")
USAGE = os.path.join(SIG, "usage.jsonl")
BRANCH = "wf-burr-demo"
SHARED = ("bn6f", "roms", "gbarecomp", "build-tools", "toolchain", "build-checkpoints", "build-abi", "build-snapshots")
ROUTES_FILE = "tools/wf/wf/routes.py"
# OpenCode Go (deepseek-v4-pro, the pilot's model) hit its monthly limit on 10-13; Z.ai's week resets 10-14.
SMALL = ("minimax/MiniMax-M3", "hyper/glm-5.3-flash", "hyper/qwen3.8-flash", "zai/glm-5.3")
LARGE = ("minimax/MiniMax-M3", "hyper/qwen3.8-flash", "zai/glm-5.3", "hyper/glm-5.3-flash")
ROUNDS = 5
PROMPT_TAG = "sig-subsystem-1"
sys.path.insert(0, os.path.join(ROOT, "tools"))


def now() -> str:
    return datetime.datetime.now().isoformat(timespec="seconds")


def slug(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]+", "_", name.split("/")[-1]).strip("_")


@contextmanager
def ledger_lock():
    os.makedirs(os.path.dirname(LEDGER), exist_ok=True)
    with open(LEDGER + ".lock", "w") as fh:
        fcntl.flock(fh, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(fh, fcntl.LOCK_UN)


def read() -> dict:
    return json.load(open(LEDGER)) if os.path.exists(LEDGER) else {}


def update(name: str, **fields) -> dict:
    with ledger_lock():
        led = read()
        job = led.setdefault(name, {})
        job.update(fields, updated=now())
        tmp = LEDGER + ".tmp"
        json.dump(led, open(tmp, "w"), indent=1)
        os.replace(tmp, LEDGER)
        return job


def log(job: str, text: str) -> None:
    path = os.path.join(WORKTREES, f"sys-{slug(job)}.log")
    with open(path, "a") as fh:
        fh.write(f"{now()} {text}\n")


def sh(cmd: list[str] | str, cwd: str, timeout: int = 3600, env: dict | None = None) -> tuple[int, str]:
    r = subprocess.run(cmd, cwd=cwd, shell=isinstance(cmd, str), capture_output=True, text=True, timeout=timeout,
                       env={**os.environ, **(env or {})})
    return r.returncode, (r.stdout or "") + (r.stderr or "")


# ── plan ──

def plan(a) -> None:
    subs = json.load(open(os.path.join(ROOT, "tools/sig/subsystems.json")))["subsystems"]
    led = read()
    added = []
    for s in subs:
        if a.only and s["name"] not in a.only:
            continue
        if not a.only and not (a.min <= s["size"] <= a.max and s["covered"] >= a.covered * s["size"]
                               and s["captured"] >= a.covered * s["size"]):
            continue
        if s["name"] in led and led[s["name"]].get("state") not in ("failed",):
            continue
        route = SMALL if s["size"] <= 30 else LARGE
        update(s["name"], state="open", size=s["size"], entries_planned=s["entries"], route=list(route),
               tried=[], rounds=0, spent=0, file=f"sys_{slug(s['name'])}.c", began="", notes=[])
        added.append(s["name"])
    print(f"{len(added)} jobs added: {' '.join(added)}")


def status(_a) -> None:
    for n, j in sorted(read().items(), key=lambda kv: kv[1].get("updated", "")):
        print(f"{j.get('state', '?'):10} {n:42} {j.get('size', '?'):>3} units  tried {','.join(m.split('/')[-1] for m in j.get('tried', [])) or '-'}"
              f"  rounds {j.get('rounds', 0)}  tokens {j.get('spent', 0):,}  {j.get('verdict', '')[:90]}")


# ── one job ──

def worktree(name: str) -> str:
    tree = os.path.join(WORKTREES, f"sys-{slug(name)}")
    head = sh(["git", "rev-parse", BRANCH], MAIN)[1].strip()
    if not os.path.exists(os.path.join(tree, ".git")):
        sh(["git", "worktree", "add", "--detach", tree, head], MAIN)
    else:
        sh(["git", "checkout", "--quiet", "--detach", "--force", head], tree)
        sh(["git", "clean", "-fdq", "cport/src/game"], tree)
    for n in SHARED:
        p = os.path.join(tree, n)
        if os.path.isdir(p) and not os.path.islink(p) and not os.listdir(p):
            os.rmdir(p)
        if not os.path.exists(p):
            os.symlink(os.path.join(MAIN, n), p)
    os.makedirs(os.path.join(tree, "build"), exist_ok=True)
    if not os.path.exists(os.path.join(tree, "build/sig")):
        os.symlink(SIG, os.path.join(tree, "build/sig"))
    for f in glob.glob(os.path.join(MAIN, "tools/traces/coverage/*.prof")):      # git-ignored: the db's repro paths
        p = os.path.join(tree, "tools/traces/coverage", os.path.basename(f))
        if not os.path.exists(p):
            os.symlink(f, p)
    if not os.path.exists(os.path.join(tree, "cport/include/lifted.h")):     # generated headers the checks need
        sh(f"python3 tools/lifter/lift.py --bn6f {MAIN}/bn6f --out cport/src/lifted --inc cport/include "
           f"--overrides cport/src/game", tree, timeout=1800)
    acc = os.path.join(tree, "tools/convert/accepted")
    if not os.path.exists(acc):
        os.symlink(os.path.join(MAIN, "tools/convert/accepted"), acc)
    return tree


FOLDS: dict[str, list[str]] = {}


def members_of(name: str, tree: str) -> tuple[list[str], list[str], list[str], dict]:
    """(members to convert, entry points, skipped members, graph)."""
    sub = next(s for s in json.load(open(os.path.join(tree, "tools/sig/subsystems.json")))["subsystems"]
               if s["name"] == name)
    final = json.load(open(os.path.join(SIG, "final.json")))
    units = [u["unit"] for u in sub["units"]]
    g = json.load(open(os.path.join(tree, "build-abi/graph.json")))
    callers = {}
    for site in g["sites"]:
        if site.get("target"):
            callers.setdefault(site["target"], set()).add(site["caller"])
    for e in g["edges"]:
        callers.setdefault(e["target"], set()).add(e.get("site_unit"))
    unwind = [u for u in units if final.get(u, {}).get("confidence") == "none"]
    # one that returns to its caller's caller, called only from the subsystem: written inside its caller
    fold = [u for u in unwind if callers.get(u) and callers[u] <= set(units)]
    skip = [u for u in units if u.startswith("dead_") or (u in unwind and u not in fold)]
    members = [u for u in units if u not in skip]
    FOLDS[name] = fold
    entries = {u["unit"] for u in sub["units"] if u["entry"] and u["unit"] in members}
    # anything bn6f names outside the members' own code (a table elsewhere, a call the routes never made)
    seen_inside = {e["target"] for e in g["edges"] if e.get("site_unit") in members}
    words = re.compile(r"\b(" + "|".join(map(re.escape, members)) + r")\b")
    for path in glob.glob(os.path.join(tree, "bn6f/asm/*.s")) + glob.glob(os.path.join(tree, "bn6f/data/**/*.s"), recursive=True):
        cur = None
        for line in open(path, errors="replace"):
            m = re.match(r"(\w+):", line)        # a function's label (thumb_local_start names none)
            if m and m.group(1) in g["entries"]:
                cur = g["entries"][m.group(1)]["unit"]
            code = line.split("//")[0]
            for hit in words.findall(code):
                if cur in members or re.match(rf"\s*(?:thumb|arm)_(?:func|local)_(?:start|end)\s+{hit}\b", code):
                    continue
                # a table word outside the members' code is an entry unless the routes saw the members call it
                # (tables sit after whatever function precedes them in the file)
                if code.strip().startswith(".word") and hit in seen_inside:
                    continue
                entries.add(hit)
    for n, e in g["entries"].items():          # second entries of an entry unit
        if e["unit"] in entries and n != e["unit"]:
            entries.add(n)
    return members, sorted(entries), skip, g


def remove_parts(tree: str, names: set[str]) -> list[str]:
    removed = []
    for path in glob.glob(os.path.join(tree, "cport/src/game/*.c")):
        text = open(path).read()
        parts = re.split(r"(?m)^(?=/\* ── \w+ ── \*/$)", text)
        keep = []
        for p in parts:
            m = re.match(r"/\* ── (\w+) ── \*/", p)
            if m and m.group(1) in names:
                removed.append(m.group(1))
            else:
                keep.append(p)
        if len(keep) != len(parts):
            out = "".join(keep)
            if re.search(r"^(?:Next|static|/\* ──)", out, re.M):
                open(path, "w").write(out)
            else:
                os.remove(path)          # a file that held only these
    return removed


def check(job: str, tree: str, file: str, entries: list[str], members: list[str]) -> tuple[list[str], list[str]]:
    """(blocking failures, reported notes)."""
    cport = os.path.join(tree, "cport")
    rel = f"src/game/{file}"
    rc, out = sh(["gcc", "-O1", "-c", "-DTARGET_FLAT", "-DDIFFTEST", "-Iinclude", "-Ihost", "-fno-strict-aliasing",
                  "-Werror=implicit-function-declaration", "-Werror=return-type", "-Werror=int-conversion",
                  "-Werror=incompatible-pointer-types", "-Werror=overflow", "-Werror=int-to-pointer-cast",
                  "-Wall", "-Wno-unused-function", "-Wno-unused-variable", "-o", "/dev/null", rel], cport)
    errs = [l for l in out.splitlines() if "error" in l or "warning" in l]
    if rc or any("error" in l for l in errs):
        hints = [h for k, h in HINTS.items() if k in out]
        static = [f"compile ({len(errs)} problems), with gcc's source lines:\n" + "\n".join(out.splitlines()[:60])
                  + ("\n" + "\n".join(hints) if hints else "")]
    else:
        static = []
    r = subprocess.run([sys.executable, "tools/abi/lint.py", f"cport/{rel}"], cwd=tree, capture_output=True, text=True)
    try:
        lint = json.loads(r.stdout)
        src = open(os.path.join(cport, rel)).read().splitlines()
        bad = [f"{x['name']}" + (" (a `Next` adapter that does work itself: move its body into a static typed "
                                  "function and let the adapter only read registers, call it, write results, "
                                  "RETURN_TO_CALLER)" if x["kind"] == "next" else "") + ": " + "; ".join(f"line {ln}: {src[ln - 1].strip()}" for ln in sorted({n for v in x["lines"].values() for n in v})[:4])
               for xs in lint["files"].values() for x in xs if x["violation"]]
    except (ValueError, KeyError, IndexError):
        bad = [(r.stdout + r.stderr)[-500:]]
    if bad or static:
        return static + (["register-level code (R0..R12, GUEST_CALL, flags) outside the generated wrappers and the entry "
                "adapters (tools/abi/lint.py). A call the generated wrappers do not cover needs a small helper "
                "marked `/* register form: <why> */`; everything else must be typed C:\n" + "\n".join(bad[:20])] if bad else []), []
    env = {"PATH": f"{MAIN}/toolchain/bin:{os.environ['PATH']}"}
    rc, out = sh(f"python3 tools/lifter/lift.py --bn6f {MAIN}/bn6f --out cport/src/lifted --inc cport/include "
                 f"--overrides cport/src/game && nice make -C cport BUILD=build-sys -j12 && "
                 f"nice make -C cport/host BUILD=build-sys-host -j12", tree, env=env, timeout=5400)
    if rc:
        lines = [l for l in out.splitlines() if "error" in l.lower() or "override" in l or "undefined" in l]
        return ["build (lifter or compiler):\n" + "\n".join((lines or out.splitlines())[-30:])], []
    diff = os.path.join(cport, "host/build-sys-host/difftest")
    denv = {"BN6F_ROM": f"{MAIN}/bn6f/bn6f.gba"}
    fails, notes = [], []
    for e in entries:
        snaps = os.path.join(tree, "build-snapshots", e)
        if os.path.isdir(snaps):
            rc, out = sh([diff, "-S", snaps, e], cport, env=denv, timeout=1800)
            summary = [l for l in out.splitlines() if l.startswith(e + " ")]
            if "FAIL" in " ".join(summary):
                fails.append(f"difftest {e} on the original's captured calls: {summary[-1].strip()}\n"
                             + "\n".join(l for l in out.splitlines()[:12]))
            notes.append(f"captured {e}: {(summary or ['?'])[-1].split(None, 1)[-1].strip()}")
        rc, out = sh([diff, "-n", "1000", e], cport, env=denv, timeout=1800)
        summary = [l for l in out.splitlines() if l.startswith(e + " ")]
        notes.append(f"random {e}: {(summary or ['?'])[-1].split(None, 1)[-1].strip()}")
        if "FAIL" in " ".join(summary):
            # A random trial only counts as failed when the lifted original completed it (a trial the
            # original itself faults on is out of domain, not a failure). So a failure is a state the
            # original survives and the hand-written version crashed on or diverged from — a faithful
            # port can never produce one, so it blocks even though the state may be one the game never
            # holds (sys chatbox_803FEB4: an invented entry dispatcher passed every captured snapshot
            # — all 40 had the chatbox hidden — and failed only random trial 142, as a note; lockstep
            # then burned a whole-file bisect to find it).
            fails.append(f"difftest {e} on random inputs: {summary[-1].strip()}\n"
                         + "\n".join(l.strip() for l in out.splitlines()[:8]))
            notes.append("  " + " | ".join(l.strip() for l in out.splitlines()[:6]))
    if fails:
        return fails, notes
    table_path = os.path.join(tree, "build/sys/protos.json")
    if os.path.exists(table_path) and read().get(job, {}).get("mode") == "chunked":
        path = os.path.join(tree, "cport/src/game", file)
        code = open(path).read()
        g = json.load(open(os.path.join(tree, "build-abi/graph.json")))
        covered = ({m for m in members if m not in entries}
                   | set(re.findall(r"^Next (\w+)\(void\)", code, re.M)))
        unit_entries: dict[str, set[str]] = {}
        for n, e in g["entries"].items():
            unit_entries.setdefault(e["unit"], set()).add(n)
        extra, names = adapters_from_protos(json.load(open(table_path)), [m for m in members if m not in entries],
                                            unit_entries, covered)
        pf, note = probe_names(tree, path, code, extra, names, env, denv)
        sh(f"python3 tools/lifter/lift.py --bn6f {MAIN}/bn6f --out cport/src/lifted --inc cport/include "
           f"--overrides cport/src/game", tree, env=env, timeout=1800)
        pn = [note]
    else:
        pf, pn = probe(tree, file, entries, env, denv, members)
    notes += pn
    if pf:
        return pf, notes
    # lockstep, the 16 routes
    routes = re.findall(r'_route\("(\w+)", (\d+)\)', open(os.path.join(tree, ROUTES_FILE)).read())
    routes = [r for r in routes if r[0] != "beastsp"]
    rom = os.path.join(cport, "build-sys/bn6f_lifted.gba")
    lenv = {"LOCKSTEP_CACHE": f"{MAIN}/build-wf-cache/lockstep"}
    bad = []

    def one(r):
        rc, out = sh(["tools/lockstep.sh", rom, f"tools/traces/{r[0]}.trace", r[1]], tree, env=lenv, timeout=7200)
        lines = out.strip().splitlines()
        verdict = next((l for l in reversed(lines) if l.startswith(("OK", "FAIL", "MISMATCH")) or "differ" in l), lines[-1] if lines else "?")
        return r[0], rc, verdict
    with cf.ThreadPoolExecutor(6) as ex:
        for name, rc, last in ex.map(one, routes):
            if rc != 0 or not last.startswith("OK"):
                bad.append(f"{name}: {last}")
    notes.append(f"lockstep: {len(routes) - len(bad)}/{len(routes)} routes identical")
    if bad:
        where = stall_site(tree, rom, routes, bad) + "\n" + first_divergence(tree, rom, routes, bad)
        return [f"lockstep (whole-game replay against the original; palette, VRAM and OAM): " + "; ".join(bad)
                + (f"\n{where}" if where else "")], notes
    rc, out = sh([sys.executable, "tools/oracle.py", "--base", "build-snapshots", "--states", "24", "--out",
                  f"build/sys-oracle", *entries], tree, env={"ORACLE_DIFFTEST": diff}, timeout=3600)
    rep = os.path.join(tree, "build/sys-oracle/report.txt")
    if os.path.exists(rep):
        for line in open(rep):
            regs = re.findall(r"REG: ([^|]*)", line)
            other = [r for r in regs if re.sub(r"r14 lifted \S+ original \S+;?", "", r).strip()]
            notes.append(f"oracle {line.split()[0]}: {' '.join(line.split()[2:5])}"
                         + (f"; beyond r14: {other[0][:160]}" if other else "; only r14 (the oracle's link-register artifact) or none"))
    return [], notes


def assemble(pre: str, body: str) -> str:
    """The tool's preamble, then the model's code without anything it repeated of the preamble."""
    body = re.sub(r"^#include [<\"](?:stddef|stdint|game)\.h[>\"]\n|^#define G\(T, addr\).*\n", "", body, flags=re.M)
    for m in re.finditer(r"typedef struct (\w+) \{", pre):
        body = re.sub(rf"^(?:/\*[^\n]*\*/\n)?typedef struct {m.group(1)} \{{.*?^\}} {m.group(1)};\n", "", body, flags=re.M | re.S)
        body = re.sub(rf"^_Static_assert\((?:offsetof|sizeof)\({m.group(1)}\b.*\n", "", body, flags=re.M)
    for m in re.finditer(r"static inline (?:[\w ]+?) (call_\w+)\(", pre):
        body = re.sub(rf"^(?:/\*[^\n]*\*/\n)?static inline [\w ]+? {m.group(1)}\(.*?^\}}\n", "", body, flags=re.M | re.S)
    for m in re.finditer(r"typedef struct \{[^}]*\} (\w+);", pre):
        body = re.sub(rf"^typedef struct \{{[^}}]*\}} {m.group(1)};[^\n]*\n", "", body, flags=re.M)
    return pre + "\n" + body.lstrip("\n")


HINTS = {
    "int-to-pointer-cast": "Hint: a guest address is a u32; it becomes a pointer only through G(T, addr) "
                           "(`u8 *script = G(u8, *cursor);`), never a plain cast (the host test build is 64-bit).",
    "pointer-to-int-cast": "Hint: a pointer becomes a guest address with (u32)(uintptr_t)ptr.",
    "redefined": "Hint: a macro the headers already define (bn6f_syms.h, game.h): use theirs, do not #define it again.",
    "conflicting types": "Hint: the game's function names are declared as `Next NAME(void)`; a static C function "
                         "needs its own snake_case name.",
}


RESULTS: dict[str, list[str]] = {}     # probe: the result registers each adapter writes (compared)


def probe_adapters(code: str, skip: set[str], members: list[str]) -> tuple[str, list[str]]:
    """Temporary `Next` adapters for the internal functions, so the difftest can run each against its lifted
    original on that function's own captured calls (only in the probe build, never installed). A C function
    is matched to its game name by a comment naming it right above it, or by its name containing the game's
    name or address; its parameters to the signature's registers in order (a `toolkit` parameter is r10, a
    `T **p` one a cursor: the address of a local holding the register)."""
    final = json.load(open(os.path.join(SIG, "final.json")))
    g = json.load(open(os.path.join(MAIN, "build-abi/graph.json")))["entries"]
    multi = {e["unit"] for n, e in g.items() if n != e["unit"]}      # units with a second entry: left out
    defs = {}
    for m in re.finditer(r"(?:/\*((?:[^*]|\*(?!/))*)\*/\s*\n)?static\s+(?:inline\s+)?([\w\s\*]+?)\s*\b(\w+)\(([^)]*)\)\s*\{", code):
        if m.group(3).startswith("call_"):
            continue
        defs[m.group(3)] = (m.group(1) or "", m.group(4).strip())
    out, names = [], []
    for game in members:
        if game in skip or game not in final or game in multi or re.search(rf"^Next {game}\(void\)", code, re.M):
            continue
        addr = re.search(r"_([0-9A-Fa-f]{7})$", game)
        fn = next((f for f, (c, _p) in defs.items() if re.match(rf"\s*{re.escape(game)}\b", c)), None) or \
            next((f for f in defs if game in f or (addr and f.endswith("_" + addr.group(1)))), None) or \
            next((f for f in defs if addr and f in (game[:-8] + "_impl", game[:-8])), None)
        sig = (final[game].get("chosen") or {}).get("params")
        if not fn or sig is None:
            continue
        ps = [] if defs[fn][1] in ("", "void") else [q.strip() for q in defs[fn][1].split(",")]
        regs = [sp["reg"].upper() for sp in sig]
        args, pre, post, ok = [], [], [], True
        for q in ps:
            t, nm = re.sub(r"\b\w+$", "", q).strip(), re.search(r"(\w+)$", q).group(1)
            if nm == "toolkit":
                args.append(f"({t})A_eToolkit" if "*" not in t else f"G({t[:-1].strip()}, A_eToolkit)")
                continue
            if not regs:
                ok = False
                break
            reg = regs.pop(0)
            if t.endswith("**"):
                base = t[:-2].strip()
                pre.append(f"{base} *c_{reg} = G({base.replace('const ', '')}, {reg});")
                args.append(f"&c_{reg}")
                post.append(f"{reg} = (u32)(uintptr_t)c_{reg};")
            elif t.endswith("*"):
                args.append(f"G({t[:-1].strip().replace('const ', '')}, {reg})")
            else:
                args.append(f"({t}){reg}")
        if not ok or regs:
            continue
        rt = re.search(rf"static\s+(?:inline\s+)?([\w\s\*]+?)\s*\b{fn}\(", code).group(1).strip()
        ret = ((final[game].get("chosen") or {}).get("ret") or {}).get("reg", "none")
        call = f"{fn}({', '.join(args)})"
        if rt != "void" and ret in ("r0", "r0:r1") and rt in ("u8", "u16", "u32", "s8", "s16", "s32", "bool", "int", "unsigned"):
            call = f"R0 = (u32){call}"
            RESULTS[game] = ["r0"]
        elif rt == "U32Pair" and ret == "r0:r1":
            call = f"U32Pair p_ = {call}; R0 = p_.r0; R1 = p_.r1"
            RESULTS[game] = ["r0", "r1"]
        out.append(f"Next {game}(void) {{ {' '.join(pre)} {call}; {' '.join(post)} RETURN_TO_CALLER(); }}")
        names.append(game)
    return "\n#ifdef DIFFTEST\n" + "\n".join(out) + "\n#endif\n", names


def probe(tree: str, file: str, entries: list[str], env: dict, denv: dict, members: list[str]) -> tuple[list[str], list[str]]:
    """Each internal function against its lifted original on its own captured calls: only memory, the
    return target and SP count (an internal function's registers are its callers' business, now C)."""
    path = os.path.join(tree, "cport/src/game", file)
    code = open(path).read()
    extra, names = probe_adapters(code, set(entries), members)
    names = [n for n in names if os.path.isdir(os.path.join(tree, "build-snapshots", n))]
    if not names:
        return [], ["probe: no internal function with captured calls"]
    open(path, "w").write(code + extra)
    try:
        rc, out = sh(f"python3 tools/lifter/lift.py --bn6f {MAIN}/bn6f --out cport/src/lifted --inc cport/include "
                     f"--overrides cport/src/game && nice make -C cport/host BUILD=build-probe-host -j12", tree,
                     env=env, timeout=5400)
        if rc:
            return [], [f"probe: did not build ({out.strip().splitlines()[-1][:200] if out.strip() else '?'})"]
        diff = os.path.join(tree, "cport/host/build-probe-host/difftest")
        bad, ok = [], 0
        for n in names:
            rc, out = sh([diff, "-S", os.path.join(tree, "build-snapshots", n), n], os.path.join(tree, "cport"),
                         env=denv, timeout=1800)
            lines = out.splitlines()
            real = [l.strip() for l in lines if re.search(r"trial \d+: (guest memory|return target|SP) differs", l)]
            for i, l in enumerate(lines):      # a wrong result: the result register differs
                if re.search(r"trial \d+: register differs", l):
                    regs = [re.match(r"\s+(r\d+): lifted", x) for x in lines[i + 1:i + 4]]
                    if any(r and r.group(1) in RESULTS.get(n, []) for r in regs):
                        real.append(l.strip() + " (its result)")
            if real:
                detail = [l.strip() for l in out.splitlines() if "first diff" in l or "differs" in l
                          or re.match(r"\s+r\d+: lifted", l)][:6]
                bad.append(f"{n} (captured calls of the original): {len(real)} calls write memory differently or return "
                           f"elsewhere; " + " | ".join(detail))
            else:
                ok += 1
        note = f"probe: {ok}/{len(names)} internal functions match their originals on their own captured calls"
        return ([f"Internal functions that do not do what their assembly does (each run alone from the original "
                 f"game's captured calls of it, against its lifted original):\n" + "\n".join(bad)] if bad else []), [note]
    finally:
        open(path, "w").write(code)
        sh(f"python3 tools/lifter/lift.py --bn6f {MAIN}/bn6f --out cport/src/lifted --inc cport/include "
           f"--overrides cport/src/game", tree, env=env, timeout=1800)


def first_divergence(tree: str, rom: str, routes: list, bad: list[str]) -> str:
    """The first checkpoint where the screen differs from the original's, on the shortest failing route."""
    failing = sorted((int(dict(routes)[b.split(":")[0]]), b.split(":")[0]) for b in bad if b.split(":")[0] in dict(routes))
    if not failing:
        return ""
    n, route = failing[0]
    out_dir = os.path.join(tree, "build/sys/lockstep-" + route)
    shutil.rmtree(out_dir, ignore_errors=True)
    sh(["tools/lockstep.sh", rom, f"tools/traces/{route}.trace", str(n)], tree,
       env={"LOCKSTEP_CACHE": f"{MAIN}/build-wf-cache/lockstep", "OUT": out_dir}, timeout=3600)
    try:
        o = {l.split()[1]: l.split()[4:] for l in open(os.path.join(out_dir, "orig.txt"))}
        t = {l.split()[1]: l.split()[4:] for l in open(os.path.join(out_dir, "test.txt"))}
    except OSError:
        return ""
    for k in o:
        if k in t and o[k] != t[k]:
            what = [x.split("=")[0] for x, y in zip(o[k], t[k]) if x != y]
            return (f"On route {route} the screen first differs from the original's at {k.replace('=', ' frame ')}: "
                    f"{', '.join(what)} differ ({'only sprites' if what == ['oam'] else 'tiles or maps' if 'vram' in what else 'colours'}).")
    return ""


def stall_site(tree: str, rom: str, routes: list, bad: list[str]) -> str:
    """Where the tested ROM hangs on the first route that stopped early: the host PC and LR, as C functions."""
    first = next((b.split(":")[0] for b in bad if "stopped early" in b), None)
    if not first:
        return ""
    n = dict(routes)[first]
    rc, out = sh([os.path.join(MAIN, "build-tools/mgba_run"), rom, "--frames", str(int(n) * 2), "--game-frame-addr",
                  "0x020093D4", "--bios", os.path.join(MAIN, "roms/gba_bios.bin"), "--input", f"tools/traces/{first}.trace",
                  "--until-game-frame", n, "--stall-frames", "3000", "--quiet"], tree, timeout=1800)
    m = re.search(r"stalled: game frame (\d+).*?\nhost: (.*)", out, re.S)
    if not m:
        return ""
    regs = dict(re.findall(r"(\w+)=([0-9a-f]+)", m.group(2)))
    elf = rom[:-4] + ".elf"
    rc, names = sh(["arm-none-eabi-addr2line", "-f", "-e", elf, "0x" + regs.get("pc", "0"), "0x" + regs.get("lr", "0")], tree)
    fns = [l.split(".")[0] for l in names.splitlines()[::2]]
    return (f"On route {first} the game stops advancing at game frame {m.group(1)} (a hang: the main loop never "
            f"finishes a frame); the CPU is in {fns[0] if fns else '?'} (called from {fns[1] if len(fns) > 1 else '?'}). "
            f"A loop that never ends there, or a result that sends its caller round again.")


def model_call(model: str, prompt_path: str, out: str, tag: str) -> tuple[bool, str]:
    rc, text = sh([sys.executable, "tools/sig/convert.py", model, prompt_path, out, "--tag", tag], ROOT, timeout=4000)
    return rc == 0 and os.path.exists(out), text.strip()[-300:]


def tokens(tag_prefix: str) -> int:
    n = 0
    if os.path.exists(USAGE):
        for line in open(USAGE):
            d = json.loads(line)
            if str(d.get("tag", "")).startswith(tag_prefix):
                n += (d.get("in") or 0) + (d.get("out") or 0)
    return n


def summaries(code: str) -> dict[str, str]:
    out = {}
    for m in re.finditer(r"/\*\s*(\w+)(?:\s*/\s*\w+)?\s*:\s*(.*?)\*/", code, re.S):
        text = " ".join(m.group(2).split())
        out.setdefault(m.group(1), text.split(". ")[0].rstrip(".")[:200])
    return out


def run_job(name: str) -> None:
    job = read()[name]
    update(name, state="converting", began=job.get("began") or now())
    tree = worktree(name)
    members, entries, skip, g = members_of(name, tree)
    file = job["file"]
    sl = slug(name)
    base = os.path.join(tree, "build/sys")
    os.makedirs(base, exist_ok=True)
    prompt = os.path.join(base, "prompt.txt")
    rc, out = sh([sys.executable, os.path.join(ROOT, "tools/sig/subsys_prompt.py"), name, "--file", file, "--skip", *skip,
                  "--entries", *entries, "--fold", *FOLDS.get(name, []), "--preamble", os.path.join(base, "preamble.c")], tree)
    if rc:
        update(name, state="failed", verdict=f"prompt: {out[-300:]}")
        return
    open(prompt, "w").write(out)
    slim = os.path.join(base, "prompt-repair.txt")       # the same without today's readable C
    rc2, out2 = sh([sys.executable, os.path.join(ROOT, "tools/sig/subsys_prompt.py"), name, "--file", file, "--skip", *skip,
                    "--entries", *entries, "--fold", *FOLDS.get(name, []), "--no-readable",
                    "--preamble", os.path.join(base, "preamble.c")], tree)
    open(slim, "w").write(out2 if rc2 == 0 else out)
    log(name, f"members {len(members)}, entries {entries}, skipped {skip}; prompt {len(out)} chars")
    update(name, members=len(members), entries=entries, skipped=skip)
    removed = remove_parts(tree, set(members) | {n for n, e in g["entries"].items() if e["unit"] in members})
    log(name, f"took out {len(removed)} readable parts")
    latest = os.path.join(base, "latest.c")       # the job's newest candidate, whichever model wrote it
    for model in [m for m in job["route"] if m not in job.get("tried", [])]:
        reason = read()[name].get("review_reason")
        cand = os.path.join(base, f"{file}.{model.split('/')[-1]}")
        feedback = None
        for rnd in range(ROUNDS + 1):
            tag = f"sys {sl} {model} r{rnd}"
            if rnd == 0 and not reason and os.path.exists(latest) and read()[name].get("resume"):
                ok, msg = True, "kept from the run before"
                shutil.copy(latest, cand)
                update(name, resume=False)
            elif rnd == 0 and not (reason and os.path.exists(latest)):
                ok, msg = model_call(model, prompt, cand, tag)
            else:
                rp = os.path.join(base, f"repair-{model.split('/')[-1]}-{rnd}.txt")
                prev = open(latest).read() if os.path.exists(latest) else ""
                why = feedback or [reason]
                open(rp, "w").write(open(os.path.join(base, "prompt-repair.txt")).read() + REPAIR.format(problems="\n".join(f"{i + 1}. {p}" for i, p in enumerate(why)),
                                                                     code=prev))
                ok, msg = model_call(model, rp, cand + ".patch", tag)
                if reason:
                    reason = None
                    update(name, review_reason=None)
                if ok:
                    patch = open(cand + ".patch").read()
                    # a whole file back (it repeated everything) replaces; a patch is spliced in
                    whole = len(FUNC.findall(patch)) > 0.8 * max(1, len(FUNC.findall(prev)))
                    open(cand, "w").write(patch if whole else splice(prev, patch))
            if ok:
                shutil.copy(cand, latest)
            update(name, rounds=read()[name].get("rounds", 0) + 1, spent=tokens(f"sys {sl} "))
            if not ok:
                log(name, f"{model} round {rnd}: no file ({msg})")
                if rnd == 0:
                    break
                continue
            open(os.path.join(tree, "cport/src/game", file), "w").write(
                assemble(open(os.path.join(base, "preamble.c")).read(), open(cand).read()))
            update(name, state="checking", model=model)
            fails, notes = check(name, tree, file, entries, members)
            log(name, f"{model} round {rnd}: {'pass' if not fails else 'fail: ' + fails[0][:200]}")
            log(name, "  notes: " + " | ".join(notes)[:1500])
            if not fails:
                finish(name, tree, file, members, entries, model, notes)
                return
            feedback = fails   # random-input difftest failures are among them (they block, in check)
            update(name, state="converting", verdict=fails[0][:200])
        update(name, tried=read()[name].get("tried", []) + [model])
    update(name, state="failed", verdict="every model of its route tried")


REPAIR = """
## Your previous file, and what the checks found
Your previous version (the code after the tool's preamble) is below. Fix these problems:
{problems}

Answer with ONLY the definitions you change or add, each complete (a function from its comment to its closing
brace, a prototype, a #define, a typedef), in one ```c block. The tool puts each one in place of the
definition with the same name, and adds new ones; everything you leave out stays as it is.
```c
{code}```
"""

FUNC = re.compile(r"(?:^/\*(?:[^*]|\*(?!/))*\*/\n)?^(?:static|Next|const|u8|u16|u32|s8|s16|s32|bool|void|int)\b[^\n;{]*?\b(\w+)\s*\([^;{]*?\)\s*\{.*?^\}[^\n]*\n", re.M | re.S)
PROTO = re.compile(r"^static\b[^\n{]*?\b(\w+)\s*\([^)]*\)\s*;[^\n]*\n", re.M)
OTHER = re.compile(r"^#define (\w+).*\n|^typedef [^;]*?\b(\w+);[^\n]*\n", re.M)


def splice(current: str, patch: str) -> str:
    """CURRENT with each definition of PATCH put in place of the one with its name (new ones added)."""
    out = current
    for m in FUNC.finditer(patch):
        name = m.group(1)
        old = next((x for x in FUNC.finditer(out) if x.group(1) == name), None)
        if old:
            out = out[:old.start()] + m.group(0) + out[old.end():]
        else:
            at = re.search(r"^Next \w+\(void\)", out, re.M)
            k = at.start() if at else len(out)
            out = out[:k] + m.group(0) + "\n" + out[k:]
    for m in list(PROTO.finditer(patch)) + list(OTHER.finditer(patch)):
        name = m.group(1) or (m.group(2) if m.re is OTHER else None)
        pat = PROTO if m.re is PROTO else OTHER
        old = next((x for x in pat.finditer(out) if (x.group(1) or (x.group(2) if pat is OTHER else None)) == name), None)
        if old:
            out = out[:old.start()] + m.group(0) + out[old.end():]
        else:
            first = FUNC.search(out)
            k = first.start() if first else len(out)
            out = out[:k] + m.group(0) + out[k:]
    return out


def finish(name: str, tree: str, file: str, members: list[str], entries: list[str], model: str, notes: list[str]) -> None:
    code = open(os.path.join(tree, "cport/src/game", file)).read()
    # pre-review: one file per member function, as tools/convert/prereview.py expects (NAME.TAG.c)
    pre = os.path.join(tree, "build/sys/prereview")
    shutil.rmtree(pre, ignore_errors=True)
    os.makedirs(pre)
    for m in members:
        hit = re.search(rf"/\*\s*{re.escape(m)}\b.*?\*/\s*\n(static [^\n]*\{{.*?\n\}}\n)", code, re.S)
        adapter = re.search(rf"^Next {re.escape(m)}\(void\) \{{.*?^\}}\n", code, re.S | re.M)
        if hit:
            open(os.path.join(pre, f"{m}.sys.c"), "w").write(hit.group(0) + (adapter.group(0) if adapter else ""))
    out = os.path.join(tree, "build/sys/prereview.jsonl")
    sh([sys.executable, "tools/convert/prereview.py", "run", *sorted(glob.glob(os.path.join(pre, "*.c"))),
        "--out", out, "--jobs", "6"], tree, timeout=5400)
    flagged = sorted({json.loads(l)["name"] for l in open(out) if json.loads(l)["verdict"] == "flag"}) if os.path.exists(out) else []
    # records: retype queue, lint baseline, symbols/db, then stamps
    rq = os.path.join(tree, "tools/abi/retype_queue.tsv")
    rows = open(rq).read().splitlines(keepends=True)
    gone = [r.split("\t")[0] for r in rows if not r.startswith("#") and r.split("\t")[0] in members]
    open(rq, "w").write("".join(r for r in rows if r.startswith("#") or r.split("\t")[0] not in members))
    sh([sys.executable, "tools/abi/lint.py", "--write-baseline", "tools/abi/lint_baseline.json"], tree)
    sums = summaries(code)
    cnames = {}
    for m in re.finditer(r"/\*\s*(\w+)(?:\s*/\s*(\w+))?\s*(?::[^*]*?)?\*/\s*\nstatic [^(]*?\b(\w+)\(", code, re.S):
        cnames[m.group(1)] = m.group(3)
        if m.group(2):
            cnames[m.group(2)] = m.group(3)
    import yaml
    g = json.load(open(os.path.join(tree, "build-abi/graph.json")))
    names = members + [n for n, e in g["entries"].items() if e["unit"] in members and n != e["unit"]]
    for n in names:
        p = os.path.join(tree, "symbols/db", n + ".yaml")
        if not os.path.exists(p):
            e = {"name": n, "address": g["entries"][n]["addr"], "kind": "function", "status": "probable",
                 "summary": sums.get(n) or f"part of the {name} subsystem (see its readable C)",
                 "interface": "see `signature:`",
                 "evidence": [{"method": "assembly-reading", "date": now()[:10],
                               "detail": f"Read by {model} from bn6f's assembly while converting the {name} subsystem "
                                         f"as a whole (tools/sig/sysfarm.py); checked below.", "refs": ["bn6f/asm"]}],
                 "sources": [f"tools/sig/sysfarm.py {name}"]}
            open(p, "w").write(yaml.safe_dump(e, sort_keys=False, allow_unicode=True, width=110))
    sl = slug(name)
    rel = f"cport/src/game/{file}"
    spec = {"subsystem": name, "file": rel, "members": names, "entries": entries, "names": cnames,
            "readable": f"Converted with its whole subsystem ({len(members)} functions) in one go by {model} from the "
                        f"knowledge layer's signatures (tools/sig/sysfarm.py, prompt {PROMPT_TAG}); outside-model "
                        f"pre-review flagged {len(flagged)}; reviewed by Claude before the install.",
            "difftest_detail": "At the subsystem's boundary (its entry points), readable vs lifted original, on the "
                               "original's captured calls and on random inputs.",
            "difftest": "; ".join(n for n in notes if n.startswith(("captured", "random"))),
            "oracle_detail": "The original (mGBA, real BIOS) vs the readable version from the same real entry states.",
            "oracle": "; ".join(n for n in notes if n.startswith("oracle")) or "not run",
            "lockstep": next((n for n in notes if n.startswith("lockstep")), "?"),
            "reproduce": {"readable": f"python3 tools/sig/sysfarm.py plan --only {name}\npython3 tools/sig/sysfarm.py run --only {name}",
                          "difftest": f"./wf verify --function {entries[0]} --snapshots build-snapshots",
                          "oracle": f"python3 tools/oracle.py --base build-snapshots --states 24 --out build/repro/oracle {' '.join(entries)}",
                          "lockstep": f"./wf verify --function {entries[0]} --snapshots build-snapshots"},
            "commit": "<pending>", "date": now()[:10]}
    sp = os.path.join(tree, "build/sys/record.json")
    json.dump(spec, open(sp, "w"), indent=1)
    sh([sys.executable, "tools/sig/record_subsystem.py", sp], tree)
    msg = (f"{name}: the whole subsystem in idiomatic C ({file}, {len(members)} functions, "
           f"{len(entries)} entry points)\n\nConverted in one go by {model} (tools/sig/sysfarm.py); boundary difftest, "
           f"lockstep 16 routes, oracle; pre-review flagged {len(flagged)}."
           + (f" Off the retype queue: {' '.join(gone)}." if gone else "")
           + "\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n"
             "Claude-Session: https://claude.ai/code/session_01MV9x1XXyanCy3FkvpEo7b7")
    sh(["git", "add", "-A", "cport/src/game", "symbols/db", "tools/abi/retype_queue.tsv", "tools/abi/lint_baseline.json"], tree)
    rc, out = sh(["git", "commit", "-qm", msg], tree, timeout=1800)
    if rc:
        update(name, state="failed", verdict=f"commit (gate): {out[-400:]}")
        log(name, f"commit failed:\n{out[-3000:]}")
        return
    c = sh(["git", "rev-parse", "--short", "HEAD"], tree)[1].strip()
    sh(f"sed -i 's/where: recomp <pending>/where: recomp {c}/' symbols/db/*.yaml", tree)
    sh([sys.executable, "tools/symdb.py", "stamp", "--model", model, "--run", f"tools/sig/sysfarm.py {name}",
        "--prompt", PROMPT_TAG, "--installed", c, *entries], tree)
    sh(["git", "add", "symbols/db"], tree)
    sh(["git", "commit", "-qm", f"symbols/db: {name}'s commit in its entries; stamps for its entry points\n\n"
        "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n"
        "Claude-Session: https://claude.ai/code/session_01MV9x1XXyanCy3FkvpEo7b7"], tree, timeout=1800)
    update(name, state="verified", commit=c, flagged=flagged, notes=notes, spent=tokens(f"sys {sl} "),
           verdict=f"passed; pre-review flagged {len(flagged)}: {' '.join(flagged)[:200]}")
    log(name, "verified")


CHUNK_SIZE, CHUNK_ROUNDS = 10, 3
REPAIR_CHUNK = """
## Your previous answer for these functions, and what the checks found
{problems}

Answer again with these functions only (complete, corrected), in one ```c block:
```c
{code}```
"""


def callees_first(members: list[str], g: dict) -> list[str]:
    inside = set(members)
    calls: dict[str, list[str]] = {m: [] for m in members}
    for site in g["sites"]:
        if site["caller"] in inside and site.get("target") in inside and site["target"] != site["caller"]:
            calls[site["caller"]].append(site["target"])
    for e in g["edges"]:
        if e.get("site_unit") in inside and e.get("target") in inside and e["target"] != e["site_unit"]:
            calls[e["site_unit"]].append(e["target"])
    order, seen = [], set()

    def visit(m: str, stack: set[str]) -> None:
        if m in seen or m in stack:
            return
        stack.add(m)
        for c in calls[m]:
            visit(c, stack)
        stack.discard(m)
        seen.add(m)
        order.append(m)
    for m in sorted(members, key=lambda x: g["entries"][x]["addr"]):
        visit(m, set())
    return order


def adapters_from_protos(table: dict, names: list[str], unit_entries: dict[str, set[str]] | None = None,
                         covered: set[str] | None = None) -> tuple[str, list[str]]:
    """Probe adapters (DIFFTEST only) built from the tool's own prototypes: no guessing. A member whose
    unit has more entries than the adapters at hand cover (a second entry the subsystem never enters from
    outside) is left out: the lifter refuses an override that does not define every entry of its unit."""
    out, done = [], []
    for game in names:
        t = table.get(game)
        if not t:
            continue
        if unit_entries and covered is not None and unit_entries.get(game, {game}) - covered:
            continue
        pre, args, post = [], [], []
        for q in t["params"]:
            reg, ty = q["reg"].upper(), q["type"]
            val = f"G({ty[:-1].strip().replace('const ', '')}, {reg})" if ty.endswith("*") else f"({ty}){reg}"
            if q["io"]:
                pre.append(f"{ty} v_{reg} = {val};")
                args.append(f"&v_{reg}")
                post.append(f"{reg} = (u32)(uintptr_t)v_{reg};" if ty.endswith("*") else f"{reg} = (u32)v_{reg};")
            else:
                args.append(val)
        call = f"{t['cname']}({', '.join(args)})"
        rt, r = t["ret"]["type"], t["ret"]["reg"]
        if r in ("r0",) and rt not in ("void", "U32Pair"):
            call = f"R0 = (u32)(uintptr_t){call}" if rt.endswith("*") else f"R0 = (u32){call}"
            RESULTS[game] = ["r0"]
        elif rt == "U32Pair":
            call = f"U32Pair p_ = {call}; R0 = p_.r0; R1 = p_.r1"
            RESULTS[game] = ["r0", "r1"]
        RESULTS.setdefault(game, [])
        RESULTS[game] = RESULTS[game] + [q["reg"] for q in t["params"] if q["io"]]
        out.append(f"Next {game}(void) {{ {' '.join(pre)} {call}; {' '.join(post)} RETURN_TO_CALLER(); }}")
        done.append(game)
    return "\n#ifdef DIFFTEST\n" + "\n".join(out) + "\n#endif\n", done


def stubs_from_protos(table: dict, pending: list[str], code: str) -> str:
    """DIFFTEST-only forwarders for the members not written yet (a later chunk's): the callees-first order
    comes from the captured calls and the routes' edges, which do not see every call the assembly makes, so
    a chunk's code may call a function a later chunk writes. Each stub has the tool's own prototype and
    hands its arguments to the lifted original the way the generated wrappers do (GUEST_CALL), so the
    chunk's probe build links and tests what is written so far against the original's behaviour."""
    out = []
    for game in pending:
        t = table.get(game)
        if not t or re.search(rf"\b{re.escape(t['cname'])}\s*\([^;{{}}]*\)\s*\{{", code):
            continue
        sets, posts = [], []
        for q in t["params"]:
            reg, ty, nm = q["reg"].upper(), q["type"], q["name"]
            if reg in ("R8", "R9", "R10", "R11", "R12"):
                continue          # pass-through: the C prototype has no such parameter
            if q["io"]:
                sets.append(f"{reg} = (u32)(uintptr_t)*{nm};" if ty.endswith("*") else f"{reg} = (u32)*{nm};")
                base = ty[:-1].strip().replace("const ", "")
                posts.append(f"*{nm} = G({base}, {reg});" if ty.endswith("*") else f"*{nm} = ({ty}){reg};")
            elif ty.endswith("*"):
                sets.append(f"{reg} = (u32)(uintptr_t){nm};")
            else:
                sets.append(f"{reg} = (u32){nm};")
        r, rt = t["ret"]["reg"], t["ret"]["type"]
        ret = ""
        if r == "r0" and rt == "U32Pair":
            ret = "return (U32Pair){ R0, R1 };"
        elif r == "r0":
            ret = f"return ({rt})(uintptr_t)R0;" if rt.endswith("*") else f"return ({rt})R0;"
        elif r.startswith("flag:"):
            ret = "return ZERO_FLAG();"
        out.append(f"{t['decl']} {{ {' '.join(sets)} GUEST_CALL({game}); {' '.join(posts)} {ret} }}")
    if not out:
        return ""
    return "\n#ifdef DIFFTEST\n" + "\n".join(out) + "\n#endif\n"


def probe_names(tree: str, path: str, code: str, extra: str, names: list[str], env: dict, denv: dict) -> tuple[list[str], str]:
    """The probe on given adapters (see probe())."""
    names = [n for n in names if os.path.isdir(os.path.join(tree, "build-snapshots", n))]
    if not names:
        return [], "probe: none of them has captured calls"
    open(path, "w").write(code + extra)
    try:
        rc, out = sh(f"python3 tools/lifter/lift.py --bn6f {MAIN}/bn6f --out cport/src/lifted --inc cport/include "
                     f"--overrides cport/src/game && nice make -C cport/host BUILD=build-probe-host -j12", tree,
                     env=env, timeout=5400)
        if rc:
            lines = [l for l in out.splitlines() if "error" in l.lower() or "override" in l]
            return ["the probe build failed:\n" + "\n".join((lines or out.splitlines())[-15:])], "probe: no build"
        diff = os.path.join(tree, "cport/host/build-probe-host/difftest")
        bad = []
        for n in names:
            rc, out = sh([diff, "-S", os.path.join(tree, "build-snapshots", n), n], os.path.join(tree, "cport"),
                         env=denv, timeout=1800)
            lines = out.splitlines()
            real = [l.strip() for l in lines if re.search(r"trial \d+: (guest memory|return target|SP) differs", l)]
            for i, l in enumerate(lines):
                if re.search(r"trial \d+: register differs", l):
                    regs = [re.match(r"\s+(r\d+): lifted", x) for x in lines[i + 1:i + 4]]
                    if any(r and r.group(1) in RESULTS.get(n, []) for r in regs):
                        real.append(l.strip() + " (its result or an in/out register)")
            if real:
                detail = [l.strip() for l in lines if "first diff" in l or "differs" in l or re.match(r"\s+r\d+: lifted", l)
                          or "inputs:" in l][:8]
                bad.append(f"{n}: {len(real)} of its captured calls end differently from the original: " + " | ".join(detail))
        return ([f"Functions that do not do what their assembly does (each run alone from the original game's "
                 f"captured calls of it, against its lifted original; `lifted` is the original's value, `hand` "
                 f"yours):\n" + "\n".join(bad)] if bad else []), f"probe: {len(names) - len(bad)}/{len(names)} match"
    finally:
        open(path, "w").write(code)


def static_checks(tree: str, rel: str) -> list[str]:
    """Compile (a prototype not yet defined is fine) and lint, reported together."""
    cport = os.path.join(tree, "cport")
    rc, out = sh(["gcc", "-O1", "-c", "-DTARGET_FLAT", "-DDIFFTEST", "-Iinclude", "-Ihost", "-fno-strict-aliasing",
                  "-Werror=implicit-function-declaration", "-Werror=return-type", "-Werror=int-conversion",
                  "-Werror=incompatible-pointer-types", "-Werror=overflow", "-Werror=int-to-pointer-cast",
                  "-Wall", "-Wno-unused-function", "-Wno-unused-variable", "-o", "/dev/null", rel], cport)
    fails = []
    if rc or re.search(r"\berror\b", out):
        hints = [h for k, h in HINTS.items() if k in out]
        fails.append("compile, with gcc's source lines:\n" + "\n".join(out.splitlines()[:50]) + ("\n" + "\n".join(hints) if hints else ""))
    r = subprocess.run([sys.executable, "tools/abi/lint.py", f"cport/{rel}"], cwd=tree, capture_output=True, text=True)
    try:
        lint = json.loads(r.stdout)
        src = open(os.path.join(cport, rel)).read().splitlines()
        bad = [f"{x['name']}" + (" (a `Next` adapter that does work itself: move the work into its typed function)"
                                 if x["kind"] == "next" else "") + ": " + "; ".join(
                   f"line {ln}: {src[ln - 1].strip()}" for ln in sorted({n for v in x["lines"].values() for n in v})[:4])
               for xs in lint["files"].values() for x in xs if x["violation"]]
    except (ValueError, KeyError, IndexError):
        bad = []
    if bad:
        fails.append("register-level code (R0..R12, GUEST_CALL, flags) outside the generated wrappers and the entry "
                     "adapters; everything else must be typed C (call the wrappers):\n" + "\n".join(bad[:15]))
    return fails


def run_job_chunked(name: str) -> None:
    job = read()[name]
    update(name, state="converting", began=job.get("began") or now(), mode="chunked")
    tree = worktree(name)
    members, entries, skip, g = members_of(name, tree)
    file, sl = job["file"], slug(name)
    base = os.path.join(tree, "build/sys")
    os.makedirs(base, exist_ok=True)
    pre_path, protos_path = os.path.join(base, "preamble.c"), os.path.join(base, "protos.json")
    folds = FOLDS.get(name, [])
    common = [sys.executable, os.path.join(ROOT, "tools/sig/subsys_prompt.py"), name, "--file", file, "--skip", *skip,
              "--entries", *entries, "--fold", *folds, "--preamble", pre_path, "--protos-json", protos_path]
    rc, out = sh(common + ["--only", members[0]], tree)
    if rc:
        update(name, state="failed", verdict=f"prompt: {out[-300:]}")
        return
    table = json.load(open(protos_path))
    pre = open(pre_path).read()
    removed = remove_parts(tree, set(members) | {n for n, e in g["entries"].items() if e["unit"] in members})
    order = [m for m in callees_first(members, g) if m not in folds]
    chunks = [order[i:i + CHUNK_SIZE] for i in range(0, len(order), CHUNK_SIZE)]
    log(name, f"chunked: {len(order)} functions in {len(chunks)} chunks; entries {entries}; took out {len(removed)} parts")
    update(name, members=len(members), entries=entries, skipped=skip, chunks=len(chunks))
    env = {"PATH": f"{MAIN}/toolchain/bin:{os.environ['PATH']}"}
    denv = {"BN6F_ROM": f"{MAIN}/bn6f/bn6f.gba"}
    member_set = set(members)
    path = os.path.join(tree, "cport/src/game", file)
    body_path = os.path.join(base, "body.c")
    body = open(body_path).read() if os.path.exists(body_path) and job.get("resume") else ""
    done = set(job.get("chunks_done", [])) if job.get("resume") else set()
    route = [m for m in job["route"] if m not in job.get("tried", [])]
    for k, chunk in enumerate(chunks):
        if str(k) in done:
            continue
        with_folds = chunk + [f for f in folds if any(c["caller"] in chunk for c in g["sites"] if c.get("target") == f)]
        rc, prompt = sh(common + ["--only", *with_folds], tree)
        passed = False
        for model in route:
            feedback, code = None, ""
            for rnd in range(CHUNK_ROUNDS + 1):
                text = prompt if not feedback else prompt + REPAIR_CHUNK.format(
                    problems="\n".join(f"{i + 1}. {p}" for i, p in enumerate(feedback)), code=code)
                pp = os.path.join(base, f"chunk{k}-{model.split('/')[-1]}-{rnd}.txt")
                open(pp, "w").write(text)
                ok, msg = model_call(model, pp, pp[:-4] + ".c", f"sys {sl} {model} c{k} r{rnd}")
                update(name, rounds=read()[name].get("rounds", 0) + 1, spent=tokens(f"sys {sl} "))
                if not ok:
                    log(name, f"chunk {k} {model} round {rnd}: no answer ({msg[-120:]})")
                    break
                code = assemble(pre, open(pp[:-4] + ".c").read())[len(pre):].strip("\n") + "\n"
                code = PROTO.sub(lambda m: "" if m.group(1) in {t["cname"] for t in table.values()} else m.group(0), code)
                # the tool owns the probe adapters: a `Next` the model wrote for a member that is not an
                # entry point duplicates the tool's (and an override that misses its unit's other entries
                # makes the lifter refuse), so it comes out here; entry points' own adapters stay
                for n, e in g["entries"].items():
                    if e["unit"] in member_set and n not in entries:
                        code = re.sub(rf"^Next {n}\(void\) \{{.*?^\}}\n", "", code, flags=re.M | re.S)
                cur = pre + body + "\n" + code
                open(path, "w").write(cur)
                fails = static_checks(tree, f"src/game/{file}")
                note = ""
                if not fails:
                    in_code = set(re.findall(r"^Next (\w+)\(void\)", cur, re.M))
                    covered = set(chunk) | in_code
                    unit_entries: dict[str, set[str]] = {}
                    for n, e in g["entries"].items():
                        unit_entries.setdefault(e["unit"], set()).add(n)
                    pending = [n for n in table if n not in chunk]
                    extra, names = adapters_from_protos(table, [n for n in chunk if n not in entries],
                                                        unit_entries, covered)
                    pf, note = probe_names(tree, path, cur, extra + stubs_from_protos(table, pending, cur),
                                           names, env, denv)
                    fails += pf
                log(name, f"chunk {k} ({len(chunk)}) {model} round {rnd}: {'pass' if not fails else 'fail: ' + fails[0][:160]} {note}")
                if not fails:
                    passed = True
                    break
                feedback = fails
            if passed:
                break
        if not passed:
            update(name, state="failed", verdict=f"chunk {k} did not pass with any model of its route")
            return
        body += "\n" + code
        open(body_path, "w").write(body)
        done.add(str(k))
        update(name, chunks_done=sorted(done))
    # the whole file, once: build, entry difftest, probe of everything, lockstep, oracle
    open(path, "w").write(pre + body)
    update(name, state="checking")
    fails, notes = check(name, tree, file, entries, members)
    log(name, f"whole file: {'pass' if not fails else 'fail: ' + fails[0][:300]}")
    log(name, "  notes: " + " | ".join(notes)[:1500])
    if fails:
        update(name, state="failed", verdict="whole file: " + fails[0][:300], whole_file_failures=fails)
        return
    finish(name, tree, file, members, entries, route[0], notes)


def run(a) -> None:
    deadline = time.time() + a.hours * 3600
    def todo():
        return [n for n, j in read().items() if j.get("state") == "open" and (not a.only or n in a.only)]
    with cf.ThreadPoolExecutor(a.workers) as ex:
        futures = {}
        while time.time() < deadline:
            for n in todo():
                if len(futures) >= a.workers:
                    break
                if n not in futures.values():
                    update(n, state="converting")
                    futures[ex.submit(run_job_chunked if a.chunked else run_job, n)] = n
            if not futures:
                break
            done, _ = cf.wait(futures, timeout=60, return_when=cf.FIRST_COMPLETED)
            for f in done:
                n = futures.pop(f)
                if f.exception():
                    update(n, state="failed", verdict=f"crash: {f.exception()!r}"[:300])


def reopen(a) -> None:
    update(a.name, state="open", review_reason=a.why or None, tried=[])
    print(f"{a.name}: open")


def install(a) -> None:
    job = read()[a.name]
    tree = os.path.join(WORKTREES, f"sys-{slug(a.name)}")
    rc, out = sh(["git", "rebase", BRANCH], tree, timeout=1800)
    if rc:
        sh(["git", "rebase", "--abort"], tree)
        sys.exit(f"rebase onto {BRANCH} failed:\n{out[-1500:]}")
    rc, out = sh([sys.executable, "tools/gate.py", "--all"], tree, timeout=3600)
    if rc:
        sys.exit(f"gate:\n{out[-2000:]}")
    with open(os.path.join(MAIN, "build-wf-state/main-tree.lock"), "w") as fh:
        fcntl.flock(fh, fcntl.LOCK_EX)
        rc, out = sh(["git", "merge", "--ff-only", sh(["git", "rev-parse", "HEAD"], tree)[1].strip()], MAIN)
    if rc:
        sys.exit(f"fast-forward failed:\n{out}")
    update(a.name, state="installed", installed=sh(["git", "rev-parse", "--short", "HEAD"], MAIN)[1].strip())
    print(f"{a.name}: installed at {read()[a.name]['installed']}")


def main() -> None:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("plan")
    p.add_argument("--min", type=int, default=20)
    p.add_argument("--max", type=int, default=60)
    p.add_argument("--covered", type=float, default=0.8)
    p.add_argument("--only", nargs="*")
    r = sub.add_parser("run")
    r.add_argument("--workers", type=int, default=2)
    r.add_argument("--only", nargs="*")
    r.add_argument("--hours", type=float, default=24)
    r.add_argument("--chunked", action="store_true", help="about 10 functions a request, callees first")
    sub.add_parser("status")
    i = sub.add_parser("install")
    i.add_argument("name")
    o = sub.add_parser("reopen")
    o.add_argument("name")
    o.add_argument("--why")
    a = ap.parse_args()
    {"plan": plan, "run": run, "status": status, "install": install, "reopen": reopen}[a.cmd](a)


if __name__ == "__main__":
    main()
