#!/usr/bin/env python3
"""Golden test for the lifter: does it still write exactly what was recorded?

    python3 tools/lifter/golden.py check         # lift into a temporary directory and compare
    python3 tools/lifter/golden.py diff [REV]    # what the working lifter changes against REV's (default HEAD)
    python3 tools/lifter/golden.py update        # record the current output (after an intended change)

The reference, tools/lifter/golden/manifest.tsv, holds a SHA-1 and a line count per
generated file (the C itself is about 40 MB, so it is not stored; `diff` regenerates the
old side with the lifter from git). Its inputs are pinned, so only a lifter change
(tools/lifter, including iwram.txt and not_entries.txt) can change the result:
- the bn6f build it was recorded from (commit and bn6f.elf hash);
- the readable functions in place at the time (golden/overrides.txt), so installs into
  cport/src/game don't move it.

check exits 0 when every file matches; 1 when the output changed (run `diff` to see
how, `update` if it was intended); 2 when bn6f moved since the recording (a rename or
new comments; `update` re-records, after a `diff` if the lifter changed too).
`update` takes the readable functions from cport/src/game anew.
"""
from __future__ import annotations

import argparse
import datetime
import fcntl
import hashlib
import subprocess
import sys
import tempfile
from collections.abc import Iterator
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE.parent))
from lifter.lift import overridden_names  # noqa: E402

GOLDEN = HERE / "golden"
MANIFEST = GOLDEN / "manifest.tsv"
OVERRIDES = GOLDEN / "overrides.txt"
BN6F = ROOT / "bn6f"
GAME = ROOT / "cport" / "src" / "game"
BN6F_LOCK = ROOT / "build-wf-runs" / "bn6f.lock"   # the workflows' lock: renames rewrite bn6f
DIFF_OUT = ROOT / "build" / "lifter-golden.diff"


@dataclass(frozen=True)
class Bn6f:
    commit: str     # git describe --dirty of the submodule
    elf: str        # SHA-1 of bn6f.elf (names, line numbers and bytes the lifter reads)


@dataclass(frozen=True)
class Manifest:
    bn6f: Bn6f
    files: dict[str, tuple[str, int]]   # path under the output root -> (SHA-1, lines)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    sub.add_parser("check", help="lift and compare with the recorded hashes")
    d = sub.add_parser("diff", help="the working lifter's output against the lifter at REV, on the same inputs")
    d.add_argument("rev", nargs="?", default="HEAD")
    d.add_argument("--game", action="store_true",
                   help="with the readable functions now in cport/src/game instead of the pinned list")
    d.add_argument("--output", type=Path, default=DIFF_OUT, help=f"the full diff (default {DIFF_OUT.relative_to(ROOT)})")
    sub.add_parser("update", help="record the current output, with cport/src/game's readable functions")
    args = ap.parse_args()
    if args.command == "check":
        sys.exit(check())
    if args.command == "diff":
        sys.exit(diff(args.rev, args.game, args.output))
    sys.exit(update())


def check() -> int:
    recorded = read_manifest()
    now_bn6f = bn6f_state()
    if now_bn6f != recorded.bn6f:
        print(f"bn6f moved since the recording: {recorded.bn6f.commit} (elf {recorded.bn6f.elf[:12]}) -> "
              f"{now_bn6f.commit} (elf {now_bn6f.elf[:12]}).\n"
              "Re-record with `golden.py update` (if the lifter changed too, `golden.py diff` first).")
        return 2
    with tempfile.TemporaryDirectory(prefix="lifter-golden-") as tmp:
        out = Path(tmp) / "out"
        lift(HERE, out, pinned_overrides(Path(tmp), read_overrides()))
        now = output_hashes(out)
    changed = [path for path in sorted(recorded.files.keys() | now.keys()) if recorded.files.get(path) != now.get(path)]
    if not changed:
        print(f"OK: the lifter wrote the recorded output ({len(now)} files)")
        return 0
    print(f"FAIL: {len(changed)} of {len(recorded.files)} files differ from the recording:")
    for path in changed:
        old, new = recorded.files.get(path), now.get(path)
        what = "new" if old is None else "missing" if new is None else f"{old[1]} -> {new[1]} lines"
        print(f"  {path}: {what}")
    print("`golden.py diff` shows how (against the committed lifter); `golden.py update` records it if intended.")
    return 1


def diff(rev: str, game: bool, output: Path) -> int:
    """Lift with the lifter at REV and with the working one, on the same bn6f and readable functions."""
    with tempfile.TemporaryDirectory(prefix="lifter-golden-") as tmp:
        base = Path(tmp)
        old_lifter = base / "old"
        old_lifter.mkdir()
        archive = subprocess.run(["git", "-C", str(ROOT), "archive", rev, "tools/lifter"], capture_output=True, check=True)
        _ = subprocess.run(["tar", "-x", "-C", str(old_lifter)], input=archive.stdout, check=True)
        overrides = GAME if game else pinned_overrides(base, read_overrides())
        with ThreadPoolExecutor(2) as pool:
            done = list(pool.map(lambda job: lift(*job), [(old_lifter / "tools/lifter", base / "old-out", overrides),
                                                           (HERE, base / "new-out", overrides)]))
        assert len(done) == 2
        old, new = output_hashes(base / "old-out"), output_hashes(base / "new-out")
        changed = [path for path in sorted(old.keys() | new.keys()) if old.get(path) != new.get(path)]
        output.parent.mkdir(parents=True, exist_ok=True)
        with open(output, "w") as fh:
            for path in changed:
                a = base / "old-out" / path if path in old else Path("/dev/null")
                b = base / "new-out" / path if path in new else Path("/dev/null")
                text = subprocess.run(["diff", "-u", "--label", f"{rev}/{path}", "--label", f"working/{path}",
                                       str(a), str(b)], capture_output=True, text=True).stdout
                added = sum(1 for line in text.splitlines() if line.startswith("+") and not line.startswith("+++"))
                removed = sum(1 for line in text.splitlines() if line.startswith("-") and not line.startswith("---"))
                print(f"  {path}: +{added} -{removed}")
                _ = fh.write(text)
    if not changed:
        print(f"the working lifter writes the same {len(new)} files as {rev}'s")
        return 0
    print(f"{len(changed)} files differ from {rev}'s lifter; the full diff is in {output}")
    return 1


def update() -> int:
    names = sorted(overridden_names(str(GAME)))
    GOLDEN.mkdir(exist_ok=True)
    _ = OVERRIDES.write_text("# readable functions in place when tools/lifter/golden.py update last ran\n"
                             + "".join(f"{name}\n" for name in names))
    with tempfile.TemporaryDirectory(prefix="lifter-golden-") as tmp:
        out = Path(tmp) / "out"
        state = bn6f_state()
        lift(HERE, out, pinned_overrides(Path(tmp), names))
        files = output_hashes(out)
    lines = [f"# what tools/lifter writes for these inputs (tools/lifter/golden.py update, "
             f"{datetime.date.today().isoformat()})\n",
             f"# bn6f\t{state.commit}\t{state.elf}\n",
             f"# overrides\t{len(names)}\tgolden/overrides.txt\n",
             "# sha1\tlines\tpath\n"]
    lines += [f"{sha}\t{count}\t{path}\n" for path, (sha, count) in sorted(files.items())]
    _ = MANIFEST.write_text("".join(lines))
    print(f"recorded {len(files)} files for bn6f {state.commit} and {len(names)} readable functions")
    return 0


# ── helpers ────────────────────────────────────────────────────────────

def lift(lifter: Path, out: Path, overrides: Path) -> None:
    """Run the lifter in LIFTER with its output (C and headers) under OUT."""
    (out / "include").mkdir(parents=True)
    command = [sys.executable, str(lifter / "lift.py"), "--bn6f", str(BN6F), "--out", str(out / "src" / "lifted"),
               "--inc", str(out / "include"), "--overrides", str(overrides)]
    with bn6f_read_lock():
        done = subprocess.run(command, capture_output=True, text=True)
    if done.returncode != 0:
        sys.exit(f"lift failed ({lifter}):\n{(done.stdout + done.stderr)[-3000:]}")


@contextmanager
def bn6f_read_lock() -> Iterator[None]:
    """Shared hold of the workflows' bn6f lock, so a rename can't rewrite bn6f mid-lift."""
    if not BN6F_LOCK.parent.is_dir():
        yield
        return
    with open(BN6F_LOCK, "a") as handle:
        fcntl.flock(handle, fcntl.LOCK_SH)
        try:
            yield
        finally:
            fcntl.flock(handle, fcntl.LOCK_UN)


def pinned_overrides(tmp: Path, names: list[str]) -> Path:
    """A directory the lifter reads as readable code defining exactly NAMES (it only looks for the names)."""
    directory = tmp / "overrides"
    directory.mkdir()
    _ = (directory / "pinned.c").write_text("".join(f"Next {name}(void) {{\n}}\n" for name in names))
    return directory


def read_overrides() -> list[str]:
    return [line.strip() for line in OVERRIDES.read_text().splitlines() if line.strip() and not line.startswith("#")]


def output_hashes(out: Path) -> dict[str, tuple[str, int]]:
    hashes: dict[str, tuple[str, int]] = {}
    for path in sorted(p for p in out.rglob("*") if p.is_file()):
        data = path.read_bytes()
        hashes[path.relative_to(out).as_posix()] = (hashlib.sha1(data).hexdigest(), data.count(b"\n"))
    return hashes


def bn6f_state() -> Bn6f:
    commit = subprocess.run(["git", "-C", str(BN6F), "describe", "--always", "--dirty", "--abbrev=12"],
                            capture_output=True, text=True, check=True).stdout.strip()
    return Bn6f(commit=commit, elf=hashlib.sha1((BN6F / "bn6f.elf").read_bytes()).hexdigest())


def read_manifest() -> Manifest:
    if not MANIFEST.exists():
        sys.exit(f"no {MANIFEST.relative_to(ROOT)}: record one with `golden.py update`")
    bn6f = Bn6f(commit="?", elf="?")
    files: dict[str, tuple[str, int]] = {}
    for line in MANIFEST.read_text().splitlines():
        fields = line.split("\t")
        if line.startswith("# bn6f\t"):
            bn6f = Bn6f(commit=fields[1], elf=fields[2])
        elif line and not line.startswith("#"):
            files[fields[2]] = (fields[0], int(fields[1]))
    return Manifest(bn6f=bn6f, files=files)


if __name__ == "__main__":
    main()
