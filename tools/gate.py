#!/usr/bin/env python3
"""The commit gate: fast checks of what a commit contains (review 1 Area 8.2, joint plan S1 "Gate").

  tools/gate.py          pre-commit hook mode: check the index (what is being committed)
  tools/gate.py --all    `make check`: the same checks on the whole working tree

Checks, each only when the commit touches what it checks:
  lint     tools/abi/lint.py --check: no new register-level code in cport/src/game beyond the
           baseline, and the baseline (tools/abi/lint_baseline.json) may only go down: its total
           hits and its number of functions (a fix may move hits between categories; a deliberate
           increase needs GATE_ALLOW_LINT_UP='why'). One exemption: the lint keys of functions the
           commit adds to the retype queue (tools/abi/retype_queue.tsv: installs of the farm's
           old-prompt candidates, model tagged +old, defined in cport/src/game) are left out of
           the comparison; removing a function from the queue is always allowed
  symdb    tools/symdb.py check (about a second): entries valid against bn6f.elf, and the paths
           their reproduce lines name exist in the entries the commit touches; the stamp report
           (tools/stamps.py: what changed since each function was stamped, and the action) is
           shown, not enforced
  leftovers tools/abi/leftovers.py --deep --check (a few seconds): no readable code reads a register or
           flag a callee leaves behind but does not return (R1-A21), unless the site carries a note
  golden   tools/lifter/golden.py check (about 40 s), when tools/lifter changes: the lifter still
           writes the recorded output, or the commit re-records it (`golden.py update`)
  layout   tools/layout.py: cport/src/game is laid out (each function converted on its own in the
           file of its bn6f source, parts self-contained: laying out the tree gives back the same
           tree), and every symbols/db `readable_c:` names the file that holds its function
  wf       the tools/wf tests, when tools/wf changes

It reads files and git only: no model calls, no builds, no lockstep. In hook mode it checks the
index, not the working tree, so other people's unstaged edits never fail someone's commit, and
it fails only on problems in the files the commit changes (others are shown as warnings). Install
the hook with tools/install_hooks.sh; skip it once with `git commit --no-verify`.
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from collections.abc import Callable
from dataclasses import dataclass, field

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
GAME = "cport/src/game/"
BASELINE = "tools/abi/lint_baseline.json"
RETYPE_QUEUE = "tools/abi/retype_queue.tsv"   # tools/wf/wf/tools/retype.py writes it
LINT = "tools/abi/lint.py"
LEFTOVERS = "tools/abi/leftovers.py"
LIFTER = "tools/lifter/"
GOLDEN = "tools/lifter/golden.py"
WF = "tools/wf/"
WF_PYTHON = os.path.join(ROOT, "tools", "wf", ".venv", "bin", "python")


@dataclass
class Result:
    name: str
    failures: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)
    seconds: float = 0.0


def git(*args: str, env: dict[str, str] | None = None, check: bool = True) -> str:
    done = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, env=env, check=False)
    if check and done.returncode != 0:
        raise RuntimeError(f"git {' '.join(args)}: {done.stderr.strip()}")
    return done.stdout


def plain_env() -> dict[str, str]:
    """The environment without the hook's GIT_* variables (for git in the bn6f submodule)."""
    return {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}


def staged_paths() -> list[str]:
    """Paths the commit changes (added, modified, deleted, renamed), from the index git commits."""
    head = "HEAD" if git("rev-parse", "--verify", "-q", "HEAD", check=False).strip() else \
        git("hash-object", "-t", "tree", "/dev/null").strip()
    out = git("diff", "--cached", "--name-only", "-z", "--no-renames", head)
    return [p for p in out.split("\0") if p]


# ── lint ──

def baseline_totals(text: str) -> tuple[dict[str, int], int]:
    funcs = json.loads(text)["functions"]
    totals: dict[str, int] = {}
    for counts in funcs.values():
        for k, n in counts.items():
            totals[k] = totals.get(k, 0) + n
    return totals, len(funcs)


def queue_rows(text: str) -> dict[str, dict[str, str]]:
    """tools/abi/retype_queue.tsv: function -> its row (the header line names the columns)."""
    lines = [line for line in text.splitlines() if line and not line.startswith("#")]
    if not lines:
        return {}
    columns = lines[0].split("\t")
    return {cells[0]: dict(zip(columns, cells)) for cells in (line.split("\t") for line in lines[1:])}


def newly_queued(old_queue: str, new_queue: str, defined: Callable[[str], bool]) -> tuple[set[str], list[str]]:
    """The lint keys of the functions the commit adds to the retype queue, and what is wrong with any added
    row (it must name an old-prompt model, a prompt hash and keys, and its function must be in cport/src/game)."""
    before, after = queue_rows(old_queue), queue_rows(new_queue)
    keys: set[str] = set()
    problems: list[str] = []
    for function, row in after.items():
        if function in before:
            continue
        own = [key for key in row.get("lint_keys", "").split(",") if key]
        if not row.get("model", "").endswith("+old") or not row.get("prompt_hash") or not own:
            problems.append(f"{RETYPE_QUEUE}: {function}: a new row needs an old-prompt model (MODEL+old), a prompt "
                            "hash and its lint keys")
        elif not defined(function):
            problems.append(f"{RETYPE_QUEUE}: {function} is queued but cport/src/game does not define it")
        else:
            keys.update(own)
    return keys, problems


def check_lint(hook: bool, changed: list[str]) -> Result:
    res = Result("lint")
    work = tempfile.mkdtemp(prefix="gate-lint-")
    try:
        if hook:
            game = [p for p in git("ls-files", "-z", "--", GAME).split("\0") if p.endswith(".c")]
            git("checkout-index", f"--prefix={work}/", "--", *game, LINT, BASELINE)
            os.makedirs(f"{work}/cport/include", exist_ok=True)
            lifted_h = os.path.join(ROOT, "cport/include/lifted.h")      # generated: names of Next functions
            if os.path.exists(lifted_h):
                shutil.copy(lifted_h, f"{work}/cport/include/lifted.h")
            base = work
        else:
            base = ROOT
        done = subprocess.run([sys.executable, f"{base}/{LINT}", "--check", "--quiet", "--baseline",
                               f"{base}/{BASELINE}", f"{base}/cport/src/game"], capture_output=True, text=True)
        try:
            report = json.loads(done.stdout)
        except json.JSONDecodeError:
            res.failures.append(f"lint.py did not run: {(done.stderr or done.stdout).strip()[-400:]}")
            return res
        mine = {os.path.basename(p) for p in changed if p.startswith(GAME)}
        for n in report.get("new", []):
            lines = sorted({ln for v in n["lines"].values() for ln in v})
            text = (f"{GAME}{n['file']}:{lines[0] if lines else '?'} {n['function']}: new register-level code "
                    + ", ".join(f"{c} +{k}" for c, k in n["new_hits"].items()))
            (res.failures if not hook or n["file"] in mine or BASELINE in changed else res.warnings).append(text)
        if report.get("improved"):
            res.notes.append(f"{len(report['improved'])} functions have less register-level code than the "
                             f"baseline: python3 {LINT} --write-baseline {BASELINE} locks that in")
        if hook and BASELINE in changed:
            old = git("show", f"HEAD:{BASELINE}", check=False)
            new = git("show", f":{BASELINE}", check=False)
            if old and new:
                # the retype queue's exemption: keys of functions this commit queues are not compared
                game_text = "".join(open(os.path.join(work, p)).read() for p in game) if hook else ""
                exempt, problems = newly_queued(git("show", f"HEAD:{RETYPE_QUEUE}", check=False),
                                                git("show", f":{RETYPE_QUEUE}", check=False),
                                                lambda f: re.search(rf"\bNext\s+{re.escape(f)}\s*\(\s*void\s*\)\s*\{{",
                                                                    game_text) is not None)
                res.failures.extend(problems)
                kept = json.loads(new)
                kept["functions"] = {k: v for k, v in kept["functions"].items() if k not in exempt}
                (ot, on), (nt, nn) = baseline_totals(old), baseline_totals(json.dumps(kept))
                if exempt:
                    grown = sum(sum(v.values()) for k, v in json.loads(new)["functions"].items() if k in exempt)
                    res.notes.append(f"lint baseline: {grown} hits in {len(exempt)} keys of newly queued functions "
                                     f"({RETYPE_QUEUE}) left out of the comparison")
                before, after = sum(ot.values()), sum(nt.values())
                moved = ", ".join(f"{k} {ot.get(k, 0)} -> {n}" for k, n in sorted(nt.items()) if n != ot.get(k, 0))
                if after > before or nn > on:
                    text = (f"the lint baseline may only go down: {before} -> {after} hits in {on} -> {nn} functions"
                            + (f" (besides newly queued functions)" if exempt else "")
                            + (f" ({moved})" if moved else ""))
                    if os.environ.get("GATE_ALLOW_LINT_UP"):
                        res.warnings.append(f"{text}; allowed by GATE_ALLOW_LINT_UP: {os.environ['GATE_ALLOW_LINT_UP']}")
                    else:
                        res.failures.append(text + ". If that is intended, commit with "
                                            "GATE_ALLOW_LINT_UP='why' git commit ... and say why in the message")
                elif moved:
                    res.notes.append(f"lint baseline: {before} -> {after} hits ({moved})")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return res


# ── leftovers ──

def check_leftovers(hook: bool, changed: list[str]) -> Result:
    res = Result("leftovers")
    work = tempfile.mkdtemp(prefix="gate-leftovers-")
    try:
        if hook:
            game = [p for p in git("ls-files", "-z", "--", GAME).split("\0") if p.endswith(".c")]
            git("checkout-index", f"--prefix={work}/", "--", *game)
            paths = [f"{work}/{GAME}"]
        else:
            paths = []
        done = subprocess.run([sys.executable, os.path.join(ROOT, LEFTOVERS), "--quiet", "--deep", *paths],
                              capture_output=True, text=True)
        try:
            report = json.loads(done.stdout)
        except json.JSONDecodeError:
            res.warnings.append(f"leftovers.py did not run: {(done.stderr or done.stdout).strip()[-400:]}")
            return res
        mine = {os.path.basename(p) for p in changed if p.startswith(GAME)}
        for s in report["sites"]:
            if s["noted"] or s["kind"] == "zflag":
                continue
            name = os.path.basename(s["file"])
            text = (f"{GAME}{name}:{s['line']} {s['function']}: reads {s['reg']} after {s['callee']}, which does "
                    f"not return it ({s['kind']}). Read what the original reads, or note the site "
                    f"(see {LEFTOVERS})")
            (res.failures if not hook or name in mine else res.warnings).append(text)
        zflag = sum(1 for s in report["sites"] if not s["noted"] and s["kind"] == "zflag")
        if zflag:
            res.notes.append(f"{zflag} sites test r0 where the original tests Z (listed by {LEFTOVERS}, not failing)")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return res


# ── golden ──

def check_golden(hook: bool) -> Result:
    """The lifter's output against the recorded one. Lifts the working tree's lifter (the bn6f build is
    pinned), so in hook mode a lifter with unstaged edits is only warned about."""
    res = Result("golden")
    if hook and git("diff", "--name-only", "--", LIFTER).strip():
        res.warnings.append(f"{LIFTER} has unstaged edits, so the lifter checked is not the one committed")
    done = subprocess.run([sys.executable, os.path.join(ROOT, GOLDEN), "check"], cwd=ROOT,
                          capture_output=True, text=True, env=plain_env(), timeout=900)
    last = ((done.stdout + done.stderr).strip().splitlines() or ["(no output)"])[-1]
    if done.returncode == 0:
        res.notes.append(last)
    elif done.returncode == 2:
        res.warnings.append(f"{last} (bn6f moved since the recording: python3 {GOLDEN} update)")
    else:
        res.failures.append(f"{last}. If the change is intended: python3 {GOLDEN} diff to see it, then "
                            f"python3 {GOLDEN} update and commit tools/lifter/golden with it")
    return res


# ── symdb ──

def bn6f_in_flux() -> str | None:
    """Why bn6f.elf may not match the names being committed (a rename in progress), or None."""
    staged = git("ls-files", "-s", "--", "bn6f").split()
    sub = os.path.join(ROOT, "bn6f")
    head = subprocess.run(["git", "-C", sub, "rev-parse", "HEAD"], capture_output=True, text=True,
                          env=plain_env()).stdout.strip()
    if len(staged) > 1 and head and staged[1] != head:
        return "bn6f's checkout is not the commit being recorded"
    dirty = subprocess.run(["git", "-C", sub, "status", "--porcelain", "--untracked-files=no"],
                           capture_output=True, text=True, env=plain_env()).stdout.strip()
    if dirty:
        return "bn6f has uncommitted changes"
    if not os.path.exists(os.path.join(sub, "bn6f.elf")):
        return "no bn6f.elf"
    return None


def check_symdb(hook: bool, changed: list[str]) -> Result:
    import stamps
    import symdb
    res = Result("symdb")
    rep = symdb.run_check(stamps.Tree(staged=hook, cache=stamps.Cache()))
    mine = set(changed)
    flux = bn6f_in_flux() if any(p.elf for p in rep.problems) else None
    for p in rep.problems:
        text = f"{p.file}: {p.message}"
        if hook and p.file not in mine:
            res.warnings.append(text + " (not in this commit)")
        elif p.elf and flux:
            res.warnings.append(f"{text} ({flux}; checked again once bn6f settles)")
        else:
            res.failures.append(text)
    paths = {f: v for f, v in rep.missing_paths.items() if not hook or f in mine}
    if paths:
        res.warnings.append(f"{len(paths)} entries name reproduce paths that do not exist: "
                            + "; ".join(f"{f}: {' '.join(sorted(set(v))[:3])}" for f, v in sorted(paths.items())[:3])
                            + (" ..." if len(paths) > 3 else ""))
    res.notes += stamps.summary(rep.findings, rep.counts, rep.notes, limit=4)
    return res


# ── layout ──

LAYOUT = ("tools/layout.py", "tools/layout.txt")


def check_layout(hook: bool, changed: list[str]) -> Result:
    import layout
    import stamps
    import symdb
    res = Result("layout")
    files = layout.read_game(staged=hook)
    p = layout.plan(files)
    mine = set(changed)
    rule_changed = not hook or bool(mine & set(LAYOUT))
    for problem in p.problems:
        (res.failures if rule_changed or any(f"{GAME}{n}" in problem for n in
                                             {os.path.basename(c) for c in mine if c.startswith(GAME)})
         else res.warnings).append(problem)
    if p.files:
        touched = sorted(n for n in p.files if f"{GAME}{n}" in mine)
        text = (f"not laid out: {len(p.moves)} functions would move, {len(p.files)} files would change "
                f"({', '.join(sorted(p.files)[:4])}{' ...' if len(p.files) > 4 else ''}); run tools/layout.py "
                "(it moves the functions and updates symbols/db)")
        (res.failures if rule_changed or touched else res.warnings).append(text)
    tree = stamps.Tree(staged=hook, cache=stamps.Cache())
    for entry, problem in layout.readable_c_mismatches(files, symdb.load(tree, tree.cache)):
        holder = problem.rsplit(" is in ", 1)[-1]
        (res.failures if not hook or entry in mine or holder in mine else res.warnings).append(f"{entry}: {problem}")
    if not res.failures and not res.warnings:
        res.notes.append(f"laid out ({len(layout.owned_names(layout.bn6f(), layout.read_table()) & set(files))} "
                         "files by bn6f source)")
    return res


# ── tools/wf tests ──

def check_wf(hook: bool) -> Result:
    res = Result("wf")
    if not os.path.exists(WF_PYTHON):
        res.warnings.append("tools/wf/.venv is missing: tests not run (cd tools/wf && uv sync)")
        return res
    work = tempfile.mkdtemp(prefix="gate-wf-")
    try:
        if hook:
            files = [p for p in git("ls-files", "-z", "--", WF).split("\0")
                     if p and not p.startswith((WF + ".venv/", WF + "build"))]
            git("checkout-index", f"--prefix={work}/", "--", *files)
            cwd = f"{work}/{WF}"
        else:
            cwd = os.path.join(ROOT, WF)
        env = {**os.environ, "WF_REPO": os.environ.get("WF_REPO", ROOT)}
        for k in [k for k in env if k.startswith("GIT_")]:
            del env[k]
        done = subprocess.run([WF_PYTHON, "-m", "pytest", "-q", "-x", "-p", "no:cacheprovider"], cwd=cwd,
                              capture_output=True, text=True, env=env, timeout=300)
        output = (done.stdout + done.stderr).strip()
        last = (output.splitlines() or ["(no output)"])[-1]
        if done.returncode != 0:
            res.failures.append(f"tools/wf tests failed: {last}\n" + output[-2000:])
        else:
            res.notes.append(f"tests: {last}")
    except subprocess.TimeoutExpired:
        res.failures.append("tools/wf tests took over 300 s")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return res


# ── main ──

def timed(name: str, fn: Callable[[], Result]) -> Result:
    t = time.monotonic()
    try:
        r = fn()
    except Exception as e:     # the gate's own failure says so loudly, but never blocks someone's commit
        r = Result(name, warnings=[f"could not run, so nothing was checked here: {e!r} (tools/gate.py --all to retry)"])
    r.seconds = time.monotonic() - t
    return r


def main() -> int:
    t0 = time.monotonic()
    hook = "--all" not in sys.argv
    changed = staged_paths() if hook else []
    touches = (lambda *prefixes: not hook or any(p.startswith(prefixes) or p in prefixes for p in changed))
    jobs: list[tuple[str, Callable[[], Result]]] = []
    if touches(GAME, BASELINE, LINT, RETYPE_QUEUE):
        jobs.append(("lint", lambda: check_lint(hook, changed)))
    if touches(GAME, LEFTOVERS, "tools/abi/lint.py"):
        jobs.append(("leftovers", lambda: check_leftovers(hook, changed)))
    if touches(LIFTER):
        jobs.append(("golden", lambda: check_golden(hook)))
    if touches("symbols/db/", GAME, "bn6f", "tools/lifter/", "tools/symdb.py", "tools/stamps.py"):
        jobs.append(("symdb", lambda: check_symdb(hook, changed)))
    if touches(GAME, "symbols/db/", "bn6f", *LAYOUT):
        jobs.append(("layout", lambda: check_layout(hook, changed)))
    if touches(WF):
        jobs.append(("wf", lambda: check_wf(hook)))
    results: list[Result] = [Result("?")] * len(jobs)

    def run(i: int, job: tuple[str, Callable[[], Result]]) -> None:
        results[i] = timed(*job)
    threads = [threading.Thread(target=run, args=(i, j)) for i, j in enumerate(jobs)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    failed = False
    for r in results:
        for line in r.failures:
            print(f"gate {r.name}: FAIL {line}", file=sys.stderr)
        for line in r.warnings:
            print(f"gate {r.name}: warning: {line}", file=sys.stderr)
        for line in r.notes:
            print(f"gate {r.name}: {line}", file=sys.stderr)
        failed |= bool(r.failures)
    took = time.monotonic() - t0
    ran = ", ".join(f"{r.name} {r.seconds:.1f} s" for r in results) or "nothing to check"
    if failed:
        print(f"gate: FAILED ({ran}). Fix the above, or commit with --no-verify if it is wrong "
              "(and say why in the commit message).", file=sys.stderr)
        return 1
    print(f"gate: ok in {took:.1f} s ({ran})", file=sys.stderr)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:     # never block a commit because the gate itself broke
        print(f"gate: could not run ({e!r}); nothing was checked. tools/gate.py --all to retry", file=sys.stderr)
        sys.exit(0 if "--all" not in sys.argv else 1)
