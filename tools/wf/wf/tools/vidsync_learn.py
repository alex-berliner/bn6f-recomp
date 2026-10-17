"""What the sync's fixes say about the game, into symbols/db (the vidsync-tick workflow's learn step).

Every fix the driver makes is a RAM write that, once made, kept the game matching the video: runtime evidence
that the written field decides what the video shows there (an encounter's formation, a hand, an RNG draw, a
map object's place). This step reads the run's fixes (remote/fixlog.py on the driver PC), names each written
address in bn6f's terms, and adds one `runtime` evidence item per entry and handler (per run) to the
symbols/db entry of that name:

- a field vidsync's own code names (tools/vidsync/syms.py: `X = at("eBase", "oField")`) is that field's
  entry (symbols/db/oField.yaml) when there is one, else the base's;
- any other address: the entry at that address (RAM entries carry their `address`), else the bn6f data label
  at or below it (label+offset).

Entries are not created and nothing is renamed: an address with no entry is reported (build-vidsync/
learned.md) for a repair or a person to name through `record`. The repro is the fix's checkpoint, which
pathcheck.py replays against the video on the driver PC."""
import ast
import datetime
import json
import subprocess
from bisect import bisect_right
from pathlib import Path

import yaml
from pydantic import BaseModel

from wf.repo import REPO
from wf.tools.vidsync import DRIVER_PC, remote
from wf.types import FrameCount, RunName

DB = REPO / "symbols" / "db"
SYMS_PY = REPO / "tools" / "vidsync" / "syms.py"
ELF = REPO / "bn6f" / "bn6f.elf"
REPORT = REPO / "build-vidsync" / "learned.md"
RAM = ((0x02000000, 0x02040000), (0x03000000, 0x03008000))


class Fix(BaseModel):
    dir: str
    n: int
    time: str
    handler: str | None
    game: FrameCount | None
    writes: list[tuple[int, int, int]]
    mean: float | None
    line: str


class Learned(BaseModel):
    entries: tuple[str, ...] = ()       # symbols/db files that got evidence
    unnamed: tuple[str, ...] = ()       # written addresses with no entry (label+offset)
    fixes: int = 0                      # fixes read


def run_fixes(run: RunName) -> list[Fix]:
    out = remote(DRIVER_PC, "py.sh", "remote/fixlog.py", run, timeout=300)
    line = next(x for x in reversed(out.splitlines()) if x.startswith("{"))
    return [Fix.model_validate(f) for f in json.loads(line)["fixes"]]


def elf_symbols(elf: Path = ELF) -> dict[str, int]:
    nm = subprocess.run(["arm-none-eabi-nm", str(elf)], capture_output=True, text=True, check=True).stdout
    syms: dict[str, int] = {}
    for f in (line.split() for line in nm.splitlines()):
        if len(f) == 3:
            syms.setdefault(f[2], int(f[0], 16) & (~1 if f[1] in "tT" else ~0))
    return syms


def syms_py_fields(syms: dict[str, int], path: Path = SYMS_PY) -> dict[int, tuple[str, ...]]:
    """The addresses tools/vidsync/syms.py names: address -> the at() names (base first, fields after)."""
    out: dict[int, tuple[str, ...]] = {}
    for node in ast.walk(ast.parse(path.read_text())):
        if (isinstance(node, ast.Call) and getattr(node.func, "id", "") == "at" and node.args
                and all(isinstance(a, ast.Constant) and isinstance(a.value, str) for a in node.args)):
            names = tuple(a.value for a in node.args)
            if all(n in syms for n in names):
                out.setdefault(sum(syms[n] for n in names), names)
    return out


def db_entries(db: Path = DB) -> tuple[dict[str, Path], dict[int, str]]:
    """symbols/db: name -> file, and address -> name for entries in RAM."""
    by_name: dict[str, Path] = {}
    by_addr: dict[int, str] = {}
    for p in db.glob("*.yaml"):
        by_name[p.stem] = p
        for line in p.read_text().splitlines()[:6]:
            if line.startswith("address:"):
                try:
                    a = int(line.split(":", 1)[1].strip().strip("'\""), 16)
                except ValueError:
                    continue
                if any(lo <= a < hi for lo, hi in RAM):
                    by_addr[a] = p.stem
    return by_name, by_addr


class Namer:
    """bn6f's name for a written address, and the symbols/db entry it belongs to."""

    def __init__(self, syms: dict[str, int], fields: dict[int, tuple[str, ...]], by_name: dict[str, Path],
                 by_addr: dict[int, str]):
        self.fields, self.by_name, self.by_addr = fields, by_name, by_addr
        labels = sorted((v, n) for n, v in syms.items() if any(lo <= v < hi for lo, hi in RAM))
        self.values, self.labels = [v for v, _ in labels], [n for _, n in labels]

    def name(self, addr: int) -> tuple[str, str | None]:
        """(how bn6f names it, the entry name in symbols/db or None)."""
        if addr in self.fields:
            names = self.fields[addr]
            shown = names[0] + "".join(f".{n}" for n in names[1:])
            for n in reversed(names):
                if n in self.by_name:
                    return shown, n
            return shown, None
        if addr in self.by_addr:
            return self.by_addr[addr], self.by_addr[addr]
        i = bisect_right(self.values, addr) - 1
        if i < 0:
            return f"{addr:#x}", None
        label, off = self.labels[i], addr - self.values[i]
        shown = label if off == 0 else f"{label}+{off:#x}"
        return shown, self.owner(label)

    def owner(self, label: str) -> str | None:
        """The entry for a label: its own, else the struct's it is a field of (bn6f names a struct's fields
        BASE_FIELD and an array's elements BASEn: ePanelData4_1_Type -> ePanelData, eStruct2035280_UnkFlags_44
        -> eStruct2035280)."""
        name = label
        while name:
            if name in self.by_name:
                return name
            cut = name.rstrip("0123456789")
            name = cut if cut != name else (name.rsplit("_", 1)[0] if "_" in name else "")
        return None


def evidence_item(run: RunName, handler: str, shown: str, fixes: list[Fix], today: str) -> dict:
    last = fixes[-1]
    games = sorted(f.game for f in fixes if f.game is not None)
    span = f"games {games[0]}..{games[-1]}" if games else "game frames not logged"
    return {
        "method": "runtime",
        "date": today,
        "detail": (f"vidsync run {run}: {len(fixes)} fix(es) by the {handler} handler wrote {shown} ({span}); "
                   "after each, the sync matched the video again (the field decides what the video shows there)."),
        "reproduce": (f"grep -n ' fix {last.n}: ' {last.dir}.out   # in ~/vidsync on the driver PC; "
                      f"remote/py.sh pathcheck.py {last.dir}/fix_{last.n:04d}.json replays the path with it"),
        "result": last.line,
    }


def learn(run: RunName, fixes: list[Fix], namer: Namer, by_name: dict[str, Path], seen: set[str],
          today: str | None = None) -> tuple[Learned, set[str]]:
    """Add evidence for fixes not in `seen` ("run:dir:n" keys); returns what changed and the keys now seen."""
    today = today or datetime.date.today().isoformat()
    groups: dict[tuple[str, str], tuple[str, list[Fix]]] = {}
    unnamed: set[str] = set()
    new_seen = set(seen)
    for f in fixes:
        key = f"{run}:{f.dir}:{f.n}"
        if key in seen or not f.writes:
            continue
        new_seen.add(key)
        for addr, _value, _size in f.writes:
            shown, entry = namer.name(addr)
            if entry is None:
                unnamed.add(shown)
                continue
            g = groups.setdefault((entry, f.handler or "?"), (shown, []))
            if f not in g[1]:
                g[1].append(f)
    changed: list[str] = []
    for (entry, handler), (shown, fs) in sorted(groups.items()):
        path = by_name[entry]
        e = yaml.safe_load(path.read_text())
        ev = e.setdefault("evidence", [])
        tag = f"vidsync run {run}: "
        if any(isinstance(x, dict) and str(x.get("detail", "")).startswith(tag) and f"by the {handler} handler"
               in str(x.get("detail", "")) for x in ev):
            continue                                   # (this run's evidence for this handler is in already)
        ev.append(evidence_item(run, handler, shown, fs, today))
        path.write_text(yaml.safe_dump(e, sort_keys=False, allow_unicode=True, width=110))
        if str(path.relative_to(REPO)) not in changed:
            changed.append(str(path.relative_to(REPO)))
    return Learned(entries=tuple(changed), unnamed=tuple(sorted(unnamed)), fixes=len(fixes)), new_seen


def write_report(run: RunName, learned: Learned, path: Path = REPORT) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [f"# What the sync's fixes wrote, without a symbols/db entry (run {run})", "",
             "Name these through `wf record` (a repair's findings, or by hand) once their meaning is known.", ""]
    lines += [f"- {u}" for u in learned.unnamed]
    path.write_text("\n".join(lines) + "\n")


def learn_from_run(run: RunName, seen: set[str]) -> tuple[Learned, set[str]]:
    syms = elf_symbols()
    by_name, by_addr = db_entries()
    namer = Namer(syms, syms_py_fields(syms), by_name, by_addr)
    learned, now_seen = learn(run, run_fixes(run), namer, by_name, seen)
    write_report(run, learned)
    return learned, now_seen
