#!/usr/bin/env python3
"""What the models cost and what they deliver: tools/convert/scorecard.py [FUNC...]

Sources: the farm's round logs (tools/convert/log/FUNC.MODEL.jsonl, one line per round), llm.py's usage log
(tools/convert/usage.jsonl, one line per model call: farm rounds, symbol drafts and convert files alike),
the plans' meters (tools/convert/quota.jsonl, one tools/convert/quota.py snapshot per line), the git history
of cport/src/game (each install commit says "written by MODEL"), symbols/pool.tsv (tools/convert/pool.py: what
passes and is not installed) and symbols/db (renames).

1. Per model, under one name whichever provider served it (llm.model_id), over the given functions
   (default: every function it attempted):
     converted  functions with an accepted version / functions it finished
     requests   model calls made (first draft + repairs)
     median s   median seconds per call; first byte: median seconds to the first byte of a streamed reply
     tokens     input + output tokens per converted function (output includes reasoning where reported)
2. Cost per INSTALLED function, per provider, in the plan's own units: every call counts (farm rounds,
   drafts, convert files, functions that never passed and accepted ones dropped later), divided by the
   functions installed today whose install commit names one of the provider's models; pending: functions
   whose chosen candidate (pool.tsv) passes but is not installed. Units come from the meters' movement
   between quota snapshots (tokens per unit); without a measurement, Hyper uses review 3's 145k tokens per
   unit and the others show tokens only.
3. Pass rates: first-round pass rate, and tokens per pass if the farm stopped after round 1, 2, 3 or 4,
   by size band (assembly instruction lines of the function in bn6f/asm).
"""
import glob
import json
import os
import re
import statistics
import subprocess
import sys
from collections import Counter, defaultdict
from datetime import datetime, timedelta
from typing import Callable, NamedTuple, cast

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import llm  # noqa: E402  # pyright: ignore[reportMissingImports]

model_id = cast(Callable[[str], str], llm.model_id)  # one name per model across providers  # pyright: ignore[reportUnknownMemberType]
Row = dict[str, object]   # one JSON line of a log

LOG_DIR = os.path.join(ROOT, "tools/convert/log")
USAGE = os.path.join(ROOT, "tools/convert/usage.jsonl")
QUOTA = os.path.join(ROOT, "tools/convert/quota.jsonl")
POOL = os.path.join(ROOT, "symbols/pool.tsv")
GAME = os.path.join(ROOT, "cport/src/game")
FARM_QUIET_SECONDS = 600              # a log written more recently than this belongs to a running farm
TYPED_PROMPT_SINCE = "2026-10-10 07:39"   # 329fbfd: the farm prompt asks for typed functions
BANDS = ((10, "<=10"), (20, "11-20"), (30, "21-30"), (40, "31-40"), (60, "41-60"), (10 ** 9, ">60"))
ROUNDS = 4                            # round 0 and three repairs (farm --rounds 3)


# ── facts shared with tools/convert/pool.py ────────────────────────────

class Assembly(NamedTuple):
    lines: int                 # instruction lines: no labels, directives, func_start/end macros or comments
    calls: frozenset[str]      # `bl` targets
    bios: frozenset[int]       # BIOS calls (`svc` numbers)


_LABEL = re.compile(r"^(\w+)::?\s*(?://.*)?$")
_END = re.compile(r"^\s*(?:thumb|arm)_func_end\s+(\w+)")
_MACRO = re.compile(r"^(?:thumb|arm)_(?:func|local)_(?:start|end)\b")
_CALL = re.compile(r"^bl\s+(\w+)")
_SVC = re.compile(r"^(?:svc|swi)\s+#?(0x[0-9a-fA-F]+|\d+)")


def assembly() -> dict[str, Assembly]:
    """Every function of bn6f/asm, from its label to its func_end: the size measure the reviews agreed on
    (assembly instruction lines) and what it calls."""
    found: dict[str, Assembly] = {}
    for path in sorted(glob.glob(os.path.join(ROOT, "bn6f/asm/*.s"))):
        lines = open(path, errors="replace").read().splitlines()
        starts: dict[str, int] = {}
        for number, line in enumerate(lines):
            label = _LABEL.match(line)
            if label:
                _ = starts.setdefault(label[1], number)
                continue
            end = _END.match(line)
            if not end or end[1] not in starts:
                continue
            body = [re.sub(r"//.*|@.*|/\*.*?\*/", "", text).strip() for text in lines[starts[end[1]] + 1:number]]
            code = [text for text in body if text and not text.startswith(".") and not _MACRO.match(text)
                    and not re.match(r"^\w+:", text)]
            found[end[1]] = Assembly(lines=len(code),
                                     calls=frozenset(m[1] for m in map(_CALL.match, code) if m),
                                     bios=frozenset(int(m[1], 0) for m in map(_SVC.match, code) if m))
    return found


def renames() -> dict[str, str]:
    """OLD -> today's name, from symbols/db's previous_names (tools/rename_symbol.py keeps them direct)."""
    import yaml
    loader = cast(type[yaml.SafeLoader], getattr(yaml, "CSafeLoader", yaml.SafeLoader))
    current: dict[str, str] = {}
    for path in glob.glob(os.path.join(ROOT, "symbols/db/*.yaml")):
        with open(path) as f:
            entry = cast(Row, yaml.load(f, Loader=loader) or {})
        for old in cast(list[str], entry.get("previous_names") or []):
            current[old] = str(entry["name"])
    return current


def installed() -> set[str]:
    """Each function cport/src/game defines a `Next NAME(void)` body for."""
    names: set[str] = set()
    for path in glob.glob(os.path.join(GAME, "*.c")):
        names.update(cast(list[str], re.findall(r"\bNext\s+(\w+)\s*\(\s*void\s*\)\s*\{", open(path).read())))
    return names


def writers(current: dict[str, str]) -> dict[str, str]:
    """Today's name -> the model that wrote its installed body: the newest commit that added its
    `Next NAME(void)` and says "(N functions, written by MODEL)"."""
    log = subprocess.run(["git", "-C", ROOT, "log", "--format=%x00%s", "-p", "-U0", "--no-renames", "--",
                          "cport/src/game"], capture_output=True, text=True, check=True).stdout
    found: dict[str, str] = {}
    for commit in log.split("\x00")[1:]:
        subject, _, diff = commit.partition("\n")
        writer = re.search(r"written by (\S+?)\)?\s*$", subject)
        if writer is None:
            continue
        for name in cast(list[str], re.findall(r"^\+\s*Next\s+(\w+)\s*\(\s*void\s*\)", diff, re.M)):
            _ = found.setdefault(current.get(name, name), writer[1])
    return found


def pending_by_model() -> Counter[str]:
    """Functions symbols/pool.tsv (tools/convert/pool.py) lists as pending (a candidate passes against the last
    commit, not installed), by the model of the candidate it chose."""
    found: Counter[str] = Counter()
    if not os.path.exists(POOL):
        return found
    lines = [line.rstrip("\n").split("\t") for line in open(POOL) if not line.startswith("#")]
    for cells in lines[1:]:
        row = dict(zip(lines[0], cells))
        chosen = re.search(r"/[^/.]+\.([^/]+?)\.(?:c|jsonl)\b", row.get("chosen", ""))
        if row.get("state") == "pending" and chosen:
            found[chosen[1].replace("_", "/", 1)] += 1
    return found


def band(lines: int | None) -> str:
    return "?" if lines is None else next(label for top, label in BANDS if lines <= top)


# ── the logs ───────────────────────────────────────────────────────────

class Call(NamedTuple):
    spec: str            # provider/model as asked
    tag: str
    kind: str            # farm, drafts, convert, other
    seconds: float
    first_byte: float | None
    tokens_in: int
    tokens_out: int
    time: str


def call_kind(tag: str) -> str:
    if tag.startswith("convert-"):
        return "convert"
    if " draft " in tag or tag.startswith("drafts-"):
        return "drafts"
    return "farm" if " round " in tag else "other"


def read_json_lines(path: str) -> list[Row]:
    """Every whole JSON line (a running farm may be half way through the last one)."""
    rows: list[Row] = []
    if not os.path.exists(path):
        return rows
    for line in open(path):
        try:
            row = cast(object, json.loads(line))
        except json.JSONDecodeError:
            continue
        if isinstance(row, dict):
            rows.append(cast(Row, row))
    return rows


def text(row: Row, key: str) -> str:
    value = row.get(key)
    return value if isinstance(value, str) else ""


def amount(row: Row, key: str) -> float | None:
    value = row.get(key)
    return float(value) if isinstance(value, (int, float)) and not isinstance(value, bool) else None


def calls() -> list[Call]:
    return [Call(spec=text(r, "model"), tag=text(r, "tag"), kind=call_kind(text(r, "tag")),
                 seconds=amount(r, "seconds") or 0.0, first_byte=amount(r, "first_byte"),
                 tokens_in=int(amount(r, "in") or 0), tokens_out=int(amount(r, "out") or 0), time=text(r, "time"))
            for r in read_json_lines(USAGE) if text(r, "model")]


class Pair(NamedTuple):
    spec: str
    function: str        # as the farm named it
    rounds: list[Row]    # the log's rows, in order
    quiet: bool          # not written in the last 10 minutes


def pairs(want: set[str]) -> list[Pair]:
    import time
    found: list[Pair] = []
    for path in glob.glob(os.path.join(LOG_DIR, "*.jsonl")):
        function, tag = os.path.basename(path)[:-len(".jsonl")].split(".", 1)
        if want and function not in want:
            continue
        try:
            quiet = os.stat(path).st_mtime < time.time() - FARM_QUIET_SECONDS
        except FileNotFoundError:
            continue
        found.append(Pair(spec=tag.replace("_", "/", 1), function=function, rounds=read_json_lines(path), quiet=quiet))
    return found


def passed(pair: Pair) -> bool:
    return any(r.get("ok") for r in pair.rounds)


def finished(pair: Pair) -> bool:
    """Passed, out of rounds, a runaway (never asked again), or left alone for 10 minutes."""
    return (passed(pair) or len(pair.rounds) >= ROUNDS or pair.quiet
            or any(text(r, "outcome") == "runaway" for r in pair.rounds))


# ── 1. per model ───────────────────────────────────────────────────────

def per_model(farm_pairs: list[Pair], usage: list[Call]) -> None:
    spent: dict[tuple[str, str], list[int]] = defaultdict(lambda: [0, 0, 0])   # (spec, function) -> calls, in, out
    first_bytes: dict[str, list[float]] = defaultdict(list)
    for call in usage:
        if call.kind == "farm":
            row = spent[(call.spec, call.tag.split(" round ")[0])]
            row[0] += 1
            row[1] += call.tokens_in
            row[2] += call.tokens_out
        if call.first_byte is not None:
            first_bytes[model_id(call.spec)].append(call.first_byte)
    by_model: dict[str, list[Pair]] = defaultdict(list)
    for pair in farm_pairs:
        by_model[model_id(pair.spec)].append(pair)
    print(f"{'model':28s} {'providers':22s} {'converted':>10s} {'requests':>9s} {'median s':>9s} {'first byte':>10s} "
          f"{'tokens/conv':>12s} {'in/out tokens':>15s}")
    rows: list[tuple[str, str, int, int, int, float, str, str, str]] = []
    for model, group in by_model.items():
        ok = [p for p in group if passed(p)]
        done = [p for p in group if finished(p)]
        seconds = [amount(r, "seconds") or 0.0 for p in group for r in p.rounds]
        tokens = [spent[(p.spec, p.function)] for p in group]
        logged, tokens_in, tokens_out = (sum(t[i] for t in tokens) for i in range(3))
        per = f"{(tokens_in + tokens_out) / len(ok) / 1000:.0f}k" if ok and logged else "-"
        first = f"{statistics.median(first_bytes[model]):.1f}" if first_bytes[model] else "-"
        rows.append((model, ",".join(sorted({p.spec.split("/")[0] for p in group})), len(ok), len(done),
                     sum(len(p.rounds) for p in group), statistics.median(seconds) if seconds else 0, first, per,
                     f"{tokens_in / 1e6:.1f}M/{tokens_out / 1e6:.1f}M" if logged else "-"))
    for model, providers, ok, done, requests, median, first, per, io in sorted(rows, key=lambda r: (-r[2], r[4])):
        print(f"{model:28s} {providers:22s} {f'{ok}/{done}':>10s} {requests:9d} {median:9.0f} {first:>10s} {per:>12s} "
              f"{io:>15s}")


# ── 2. cost per installed function ─────────────────────────────────────

class Meter(NamedTuple):
    unit: str
    used: Callable[[Row], float | None]   # how much of the period's allowance a snapshot shows used
    default_tokens_per_unit: float | None


def _rows(snap: Row, provider: str) -> list[Row]:
    found = snap.get(provider)
    return [cast(Row, r) for r in cast(list[object], found) if isinstance(r, dict)] if isinstance(found, list) else []


def _hyper(snap: Row) -> float | None:
    hyper = snap.get("hyper")
    balance = amount(cast(Row, hyper), "balance") if isinstance(hyper, dict) else None
    return -balance if balance is not None else None


def _minimax(snap: Row) -> float | None:
    general = [amount(r, "current_weekly_remaining_percent") for r in _rows(snap, "minimax") if text(r, "model_name") == "general"]
    return 100.0 - general[0] if general and general[0] is not None else None


def _zai(snap: Row) -> float | None:
    weekly = [amount(r, "currentValue") for r in _rows(snap, "zai") if r.get("unit") == 6]   # unit 6: the week
    return weekly[0] if weekly else None


METERS: dict[str, Meter] = {
    "hyper": Meter("credit units", _hyper, 145_000),          # review 3: about 145k tokens per unit
    "minimax": Meter("% of the week", _minimax, None),
    "zai": Meter("credits (10,000/week)", _zai, None),
}
HYPER_RESET = (10, 13)   # Hyper's daily credit resets about 10:13 local


class Rate(NamedTuple):
    tokens_per_unit: float
    how: str


def measured_rates(usage: list[Call]) -> dict[str, Rate]:
    """Tokens per plan unit, from consecutive snapshots whose meter moved forward within one period: the
    provider's logged tokens between them over how far the meter moved."""
    snaps = sorted((s for s in read_json_lines(QUOTA) if text(s, "time")), key=lambda s: text(s, "time"))
    rates: dict[str, Rate] = {}
    for provider, meter in METERS.items():
        units, tokens, windows = 0.0, 0, list[str]()
        for before, after in zip(snaps, snaps[1:]):
            start, end = (datetime.strptime(text(s, "time"), "%Y-%m-%d %H:%M:%S") for s in (before, after))
            used_before, used_after = meter.used(before), meter.used(after)
            if used_before is None or used_after is None or used_after < used_before or end - start > timedelta(hours=12):
                continue
            reset = start.replace(hour=HYPER_RESET[0], minute=HYPER_RESET[1], second=0)
            if provider == "hyper" and (start < reset <= end or start < reset + timedelta(days=1) <= end):
                continue
            spent = sum(c.tokens_in + c.tokens_out for c in usage
                        if c.spec.split("/")[0] == provider and text(before, "time") < c.time <= text(after, "time"))
            if spent and used_after > used_before:
                units += used_after - used_before
                tokens += spent
                windows.append(f"{text(before, 'time')[5:16]}-{text(after, 'time')[11:16]}")
        if units:
            rates[provider] = Rate(tokens / units, f"measured: {units:g} {meter.unit} for {tokens / 1e6:.1f}M tokens "
                                                   f"({', '.join(windows)})")
        elif meter.default_tokens_per_unit:
            rates[provider] = Rate(meter.default_tokens_per_unit, "review 3's estimate (no snapshots bracket its use)")
    return rates


def cost_per_installed(usage: list[Call], written_by: dict[str, str], today: set[str]) -> None:
    rates = measured_rates(usage)
    pending = pending_by_model()
    installs: Counter[str] = Counter(written_by[name] for name in today if name in written_by)
    unattributed = sum(1 for name in today if name not in written_by)
    spent: dict[str, Counter[str]] = defaultdict(Counter)   # spec -> kind -> tokens
    for call in usage:
        spent[call.spec][call.kind] += call.tokens_in + call.tokens_out
        spent[call.spec]["calls"] += 1
    print(f"\nCost per installed function (all calls: farm rounds, drafts, convert files, failures and later drops; "
          f"{sum(installs.values())} installed functions name their writer, {unattributed} do not; pending: passing "
          f"candidates not installed, from symbols/pool.tsv)")
    print(f"{'provider / model':36s} {'calls':>6s} {'farm':>7s} {'drafts':>7s} {'convert':>8s} {'tokens':>8s} "
          f"{'plan units':>11s} {'installed':>9s} {'units/inst':>11s} {'tokens/inst':>11s} {'pending':>8s}")
    for provider in sorted({spec.split("/")[0] for spec in (*spent, *installs)}):
        specs = sorted(s for s in {*spent, *installs, *pending} if s.split("/")[0] == provider)
        rate = rates.get(provider)
        unit = METERS[provider].unit if provider in METERS and rate else "tokens"

        def line(label: str, kinds: Counter[str], count: int, waiting: int) -> None:
            tokens = sum(v for k, v in kinds.items() if k != "calls")
            units = f"{tokens / rate.tokens_per_unit:,.1f}" if rate else "-"
            per_units = f"{tokens / rate.tokens_per_unit / count:.2f}" if rate and count else "-"
            per_tokens = f"{tokens / count / 1000:,.0f}k" if count else "none yet"
            print(f"{label:36s} {kinds['calls']:6d} {kinds['farm'] / 1e6:6.1f}M {kinds['drafts'] / 1e6:6.1f}M "
                  f"{kinds['convert'] / 1e6:7.1f}M {tokens / 1e6:7.1f}M {units:>11s} {count:9d} {per_units:>11s} "
                  f"{per_tokens:>11s} {waiting:8d}")
        total: Counter[str] = Counter()
        for spec in specs:
            total.update(spent[spec])
        line(f"{provider} ({unit})", total, sum(installs[s] for s in specs), sum(pending[s] for s in specs))
        for spec in specs:
            line(f"  {spec.split('/', 1)[1]}", spent[spec], installs[spec], pending[spec])
        if rate:
            print(f"  {'':34s} {rate.tokens_per_unit / 1000:,.0f}k tokens per {unit.rstrip('s')}, {rate.how}")


# ── 3. pass rates by round and size ────────────────────────────────────

def pass_rates(farm_pairs: list[Pair], usage: list[Call], sizes: dict[str, Assembly], current: dict[str, str]) -> None:
    """Per model and prompt: first-round pass rate, and tokens per pass if every pair stopped after round k
    (all calls of rounds up to k over the passes by round k), by size band. Only pairs whose every round has a
    usage row count, so tokens and passes cover the same calls."""
    tokens: dict[tuple[str, str, int], int] = defaultdict(int)
    first_time: dict[tuple[str, str], str] = {}
    for call in usage:
        if call.kind == "farm":
            function, _, number = call.tag.rpartition(" round ")
            if not number.isdigit():
                continue
            tokens[(call.spec, function, int(number))] += call.tokens_in + call.tokens_out
            if number == "0":
                _ = first_time.setdefault((call.spec, function), call.time)
    table: dict[tuple[str, str], dict[str, list[tuple[int | None, list[int]]]]] = defaultdict(lambda: defaultdict(list))
    left_out = 0
    for pair in farm_pairs:
        numbers = [int(amount(r, "round") or 0) for r in pair.rounds]
        if not finished(pair) or not numbers or any((pair.spec, pair.function, n) not in tokens for n in set(numbers)):
            left_out += 1
            continue
        if text(pair.rounds[0], "report").startswith("model error"):
            continue   # no answer to judge
        typed = any(text(r, "prompt_hash") for r in pair.rounds) or first_time.get((pair.spec, pair.function), "") >= TYPED_PROMPT_SINCE
        won = next((n for n, r in zip(numbers, pair.rounds) if r.get("ok")), None)
        spend = [sum(tokens[(pair.spec, pair.function, n)] for n in set(numbers) if n <= k) for k in range(ROUNDS)]
        size = sizes.get(current.get(pair.function, pair.function))
        group = table[(model_id(pair.spec), "typed" if typed else "older")]
        group[band(size.lines if size else None)].append((won, spend))
        group["all"].append((won, spend))
    print(f"\nPass rates by size band (assembly instruction lines); tokens per pass if the farm stopped after round k "
          f"(finished pairs whose every round has a usage row; {left_out} left out; prompt: typed since "
          f"{TYPED_PROMPT_SINCE} or a logged prompt hash, else older)")
    header = " ".join(f"{f'pass<=r{k}':>9s}" for k in range(ROUNDS)) + " " + " ".join(f"{f'tok/pass r{k}':>12s}" for k in range(ROUNDS))
    order = [label for _, label in BANDS] + ["?", "all"]
    for (model, prompt), group in sorted(table.items(), key=lambda item: -len(item[1]["all"])):
        if len(group["all"]) < 5:
            continue
        print(f"{model} ({prompt} prompt)")
        print(f"  {'band':6s} {'pairs':>6s} {'first round':>12s} {header}")
        for label in sorted(group, key=order.index):
            rows = group[label]
            cells: list[str] = []
            for k in range(ROUNDS):
                cells.append(f"{sum(1 for won, _ in rows if won is not None and won <= k) / len(rows):9.0%}")
            for k in range(ROUNDS):
                wins = sum(1 for won, _ in rows if won is not None and won <= k)
                cells.append(f"{sum(spend[k] for _, spend in rows) / wins / 1000:11,.0f}k" if wins else f"{'-':>12s}")
            first = sum(1 for won, _ in rows if won == 0)
            print(f"  {label:6s} {len(rows):6d} {f'{first}/{len(rows)}':>12s} {' '.join(cells)}")


def main() -> None:
    want = set(sys.argv[1:])
    usage = calls()
    farm_pairs = pairs(want)
    per_model(farm_pairs, usage)
    current = renames()
    cost_per_installed(usage, writers(current), installed())
    pass_rates(farm_pairs, usage, assembly(), current)


if __name__ == "__main__":
    main()
