#!/usr/bin/env python3
"""Rename disassembly symbols everywhere, keeping the ROM byte-identical.

  tools/rename_symbol.py OLD NEW [OLD NEW ...] [--no-regen]

For each pair:
  - the symbol database must have an entry for it (symbols/db/OLD.yaml or
    NEW.yaml): every rename is documented. The entry is renamed to NEW.yaml,
    its name set to NEW, OLD added to previous_names and a change recorded;
  - bn6f (the disassembly): every whole-word OLD in its sources (.s, .inc,
    .h, .c, .ld; like bn6f's replace.sh) becomes NEW;
  - this repo: hand-written C (cport/src/game), cport/host/difftest.cfg and
    tools/lifter/iwram.txt, including the A_OLD / L_OLD forms.
Then bn6f is rebuilt (its Makefile checks bn6f.sha1: the ROM must be
identical; on any failure every change is undone), committed on its current
branch, and the lifted C is regenerated (tools/build_lifted.sh) unless
--no-regen. The recomp repo is left for you to commit.
"""
import datetime, os, re, subprocess, sys
import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BN6F = os.path.join(ROOT, "bn6f")
DB = os.path.join(ROOT, "symbols", "db")
BN6F_EXT = (".s", ".inc", ".h", ".c", ".ld")
SKIP_DIRS = {".git", "build", "tools", "temp"}
RECOMP_FILES = ["cport/host/difftest.cfg", "cport/host/difftest_gen.c", "tools/lifter/iwram.txt"]


def word(name, prefixes=("",)):
    alts = "|".join(re.escape(p + name) for p in prefixes)
    return re.compile(rf"(?<![A-Za-z0-9_])({alts})(?![A-Za-z0-9_])")


def bn6f_files():
    for d, dirs, files in os.walk(BN6F):
        dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
        for f in files:
            if f.endswith(BN6F_EXT):
                yield os.path.join(d, f)


def recomp_files():
    yield from (os.path.join(ROOT, f) for f in RECOMP_FILES)
    game = os.path.join(ROOT, "cport", "src", "game")
    yield from (os.path.join(game, f) for f in sorted(os.listdir(game)) if f.endswith((".c", ".h")))


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    regen = "--no-regen" not in sys.argv
    if not args or len(args) % 2:
        sys.exit(__doc__)
    pairs = list(zip(args[::2], args[1::2]))
    syms = set(run(["arm-none-eabi-nm", os.path.join(BN6F, "bn6f.elf")]).stdout.split())
    for old, new in pairs:
        if old not in syms:
            sys.exit(f"{old}: not a symbol of bn6f.elf")
        if new in syms:
            sys.exit(f"{new}: already a symbol of bn6f.elf")
        if not any(os.path.exists(os.path.join(DB, f"{n}.yaml")) for n in (old, new)):
            sys.exit(f"{old}: no symbols/db entry; document it first (symbols/db/README.md)")
    if run(["git", "status", "--porcelain"], cwd=BN6F).stdout.strip():
        sys.exit("bn6f has uncommitted changes; commit or stash them first")

    touched = {}
    def edit(path, prefixes):
        text = open(path, encoding="utf-8", errors="surrogateescape").read()
        new_text = text
        for old, new in pairs:
            new_text = word(old, prefixes).sub(lambda m: m.group(1)[:len(m.group(1)) - len(old)] + new, new_text)
        if new_text != text:
            touched.setdefault(path, text)
            open(path, "w", encoding="utf-8", errors="surrogateescape").write(new_text)
    for p in bn6f_files():
        edit(p, ("",))
    for p in recomp_files():
        if os.path.exists(p):
            edit(p, ("", "A_", "L_"))

    def undo(msg):
        for p, text in touched.items():
            open(p, "w", encoding="utf-8", errors="surrogateescape").write(text)
        sys.exit(msg + " (all edits undone)")

    tc = os.path.join(ROOT, "toolchain", "bin")   # the agbasm binutils (tools/build_toolchain.sh)
    r = run(["make", f"-j{os.cpu_count()}"] + [f"{v}={tc}/arm-none-eabi-{t}" for v, t in
                                              (("AS", "as"), ("LD", "ld"), ("OBJCOPY", "objcopy"), ("OBJDUMP", "objdump"))],
            cwd=BN6F)
    if r.returncode:
        undo("bn6f build failed or the ROM is no longer identical:\n" + (r.stdout + r.stderr)[-3000:])
    print(f"bn6f rebuilt, ROM identical ({len([p for p in touched if p.startswith(BN6F)])} files changed)")

    msg = "Rename " + ", ".join(f"{o} -> {n}" for o, n in pairs) + \
          "\n\nDocumented in the recomp repo's symbols/db (tools/rename_symbol.py). ROM unchanged.\n"
    run(["git", "add", "-A"], cwd=BN6F)
    r = run(["git", "commit", "-q", "-m", msg], cwd=BN6F)
    if r.returncode:
        undo("bn6f commit failed:\n" + r.stderr)
    commit = run(["git", "log", "-1", "--format=%h"], cwd=BN6F).stdout.strip()
    print(f"bn6f commit {commit}")

    today = datetime.date.today().isoformat()
    for old, new in pairs:
        src = os.path.join(DB, f"{old}.yaml")
        if not os.path.exists(src):
            src = os.path.join(DB, f"{new}.yaml")
        e = yaml.safe_load(open(src))
        e["name"] = new
        prev = e.get("previous_names") or []
        if old not in prev:
            prev.append(old)
        # renaming back to an earlier name: that name is current again, not a previous one
        e["previous_names"] = [name for name in prev if name != new]
        e.setdefault("changes", []).append({"date": today, "what": f"rename {old} -> {new}",
                                            "where": f"bn6f {commit}", "tool": "tools/rename_symbol.py"})
        dst = os.path.join(DB, f"{new}.yaml")
        with open(dst, "w") as f:
            yaml.safe_dump(e, f, sort_keys=False, allow_unicode=True, width=100)
        if src != dst:
            os.remove(src)
    if regen:
        r = run([os.path.join(ROOT, "tools", "build_lifted.sh")])
        print(r.stdout[-500:], r.stderr[-1000:])


if __name__ == "__main__":
    main()
