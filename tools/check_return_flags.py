#!/usr/bin/env python3
"""Sweep for "the caller branches on the flags this function returns, but the C does not set them"
(bug class of cdf7c1f4: init_beastSpScenario ended `mov r0, #0` (movs, Z set), its indirect caller
CutsceneCmd_call_native_with_return_value does `bx r4; bne .halt`, the C returned with stale flags).

Side A (consumers): every conditional branch in bn6f/asm whose flags come, with no flag-setting
instruction on any path in between (CFG walk backwards), from
  - `bl X`                         -> X is flags-returning (direct)
  - `mov lr, pc; bx rN`            -> an indirect call site; targets are listed per site, and for the
                                      cutscene `cs_call_native_with_return_value ptr1=X+1` script macro the
                                      targets X are extracted and marked flags-returning (indirect)
plus callers whose C reads the z/n/c/v field of a call_X result. The set is closed over pass-through:
a flags-returning function whose return path has a `bl Y` / tail `b Y` as the last flag source makes Y
flags-returning too.

Side B (producers): for each flags-returning function with a readable C body, every return path is walked
back to the instruction that last set the flags (movs/adds/cmp/tst/shift/..., or a bl that passes them
through). The C region is searched for flag-setting constructs (SET_FLAGS_*, cpu.nz/c/v, nz =, FLAG_*,
result struct z/n/c/v). Printed: asm setters (file:line), C file:line of the region and its flag sets,
and a mark: NOFLAGS (asm leaves flags, C sets none), FEWER (C sets fewer than the distinct asm setters),
PASS (flags only pass through a callee). The verdict is manual (docs/reviews/return-flags-sweep.md).

usage: tools/check_return_flags.py [ASM_DIR] [--consumers] [--sites]
"""
from __future__ import annotations

import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funcindex  # noqa: E402

COND = {"beq", "bne", "bcs", "bcc", "bhs", "blo", "bmi", "bpl", "bvs", "bvc", "bhi", "bls", "bge", "blt", "bgt", "ble"}
START = re.compile(r"^\s*(?:thumb|arm)_(?:func|local)_start\b")
END = re.compile(r"^\s*(?:thumb|arm)_func_end\b")
LABEL = re.compile(r"^(\.?\w+):?\s*$|^(\w+):")
LOCAL = re.compile(r"^\.\w+$")
HI = {"r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc"}
# ops that never touch the flags (Thumb-1)
NOFLAG = {"ldr", "ldrb", "ldrh", "ldrsb", "ldrsh", "str", "strb", "strh", "push", "stmia", "ldmia", "nop", "pop"}
SETS = {"cmp", "cmn", "tst", "lsl", "lsr", "asr", "ror", "orr", "and", "bic", "eor", "mul", "neg", "mvn", "adc",
        "sbc", "ands", "orrs", "eors", "bics", "movs", "adds", "subs", "movflag", "svc"}
MACRO_CALL = re.compile(r"^cs_call_native_with_return_value\s+ptr1=(\w+)\+1")


class Ins:
    __slots__ = ("op", "args", "line", "file", "comment")

    def __init__(self, op: str, args: str, line: int, file: str, comment: str = "") -> None:
        self.op, self.args, self.line, self.file, self.comment = op, args, line, file, comment

    def loc(self) -> str:
        return f"{self.file}:{self.line}"

    def text(self) -> str:
        return f"{self.op} {self.args}".strip()


class Fn:
    def __init__(self, name: str, file: str, line: int) -> None:
        self.name, self.file, self.line = name, file, line
        self.ins: list[Ins] = []
        self.labels: dict[str, int] = {}
        self._preds: list[list[int]] | None = None

    def preds(self) -> list[list[int]]:
        if self._preds is None:
            p: list[list[int]] = [[] for _ in self.ins]
            for i, ins in enumerate(self.ins):
                if i + 1 < len(self.ins) and (not ends_flow(ins) or is_indirect_call(self, i)):
                    p[i + 1].append(i)
                if ins.op == "b" or ins.op in COND:
                    t = self.labels.get(ins.args.split()[0]) if ins.args else None
                    if t is not None and t < len(self.ins):
                        p[t].append(i)
            self._preds = p
        return self._preds


def ends_flow(i: Ins) -> bool:
    return i.op in ("b", "bx") or (i.op == "pop" and "pc" in i.args) or (i.op == "mov" and i.args.replace(" ", "").startswith("pc,"))


def sets_flags(i: Ins) -> bool:
    """Conservative: True when the instruction may write N/Z (so it is a flag source)."""
    op, a = i.op, i.args
    if op in NOFLAG or op in COND or op in ("b", "bx", "bl", "blx"):
        return False
    if op in SETS:
        return True
    ops = [x.strip() for x in a.split(",")]
    if op in ("mov", "add", "sub"):
        if any(x in HI for x in ops[:3]):
            return False                      # hi-register / sp / pc forms leave the flags
        return True
    return False


TABLES: dict[str, list[str]] = {}      # data label -> the functions its `.word sym+1` entries name
WORD = re.compile(r"^\.word\s+(\w+)\+1\b")


def parse_asm(asm_dir: str) -> tuple[dict[str, Fn], dict[str, list[tuple[str, int, str]]]]:
    fns: dict[str, Fn] = {}
    curlab = ""
    macro: dict[str, list[tuple[str, int, str]]] = {}   # target -> [(file, line, enclosing fn)]
    for path in sorted(glob.glob(os.path.join(asm_dir, "*.s"))):
        base = os.path.basename(path)
        cur: Fn | None = None
        pending = False
        last: str = ""
        with open(path, encoding="utf-8", errors="replace") as fh:
            for n, raw in enumerate(fh, 1):
                if START.match(raw):
                    pending, cur = True, None
                    continue
                if END.match(raw):
                    cur = None
                    continue
                line = raw.split("//")[0].strip()
                comment = raw.split("//", 1)[1].strip() if "//" in raw else ""
                if not line or line.startswith("@"):
                    continue
                mw = WORD.match(line)
                if mw and curlab:
                    TABLES.setdefault(curlab, []).append(mw.group(1))
                m = MACRO_CALL.match(line)
                if m:
                    macro.setdefault(m.group(1), []).append((base, n, last))
                    continue
                if line.startswith("."):
                    if LOCAL.match(line):
                        if cur is not None:
                            cur.labels[line] = len(cur.ins)
                    elif line.endswith(":") and " " not in line:
                        if cur is not None:
                            cur.labels[line[:-1]] = len(cur.ins)
                    continue
                m2 = re.match(r"^(\w+):", line)
                if m2:
                    curlab = m2.group(1)
                if m2:
                    last = m2.group(1)
                    if pending:
                        cur, pending = Fn(m2.group(1), base, n), False
                        fns[cur.name] = cur
                    if cur is not None:
                        cur.labels[m2.group(1)] = len(cur.ins)
                    continue
                if cur is None:
                    continue
                parts = line.split(None, 1)
                cur.ins.append(Ins(parts[0].lower(), parts[1] if len(parts) > 1 else "", n, base, comment))
    return fns, macro


def is_indirect_call(fn: Fn, idx: int) -> bool:
    i = fn.ins[idx]
    if i.op != "bx" or i.args.strip() in ("lr",):
        return False
    return idx > 0 and fn.ins[idx - 1].op == "mov" and fn.ins[idx - 1].args.replace(" ", "") == "lr,pc"


def sources(fn: Fn, start: int) -> tuple[list[int], bool]:
    """Walk back from instruction `start` (exclusive) over paths with no flag-setting instruction.
    Returns (indexes of bl / indirect-call instructions the flags come from, reaches_entry)."""
    found: list[int] = []
    entry = False
    seen: set[int] = set()
    work = list(fn.preds()[start])
    if start == 0:
        entry = True
    while work:
        j = work.pop()
        if j in seen:
            continue
        seen.add(j)
        i = fn.ins[j]
        if i.op in ("bl", "blx") or is_indirect_call(fn, j):
            found.append(j)
            continue
        if sets_flags(i):
            continue
        if j == 0:
            entry = True
        work.extend(fn.preds()[j])
    return found, entry


def indirect_targets(fn: Fn, j: int) -> tuple[list[str], str]:
    """Functions an indirect call `bx rN` at index j can reach, from the `// =sym` literal comments of the
    loads just before it (a function pointer `=f+1` or a jump table `=TABLE`). ([], why) if unresolved."""
    out: list[str] = []
    how = []
    for k in list(range(j - 1, max(-1, j - 14), -1)) + list(range(0, j)):
        i = fn.ins[k]
        lit = i.comment if i.comment.startswith("=") else ("=" + i.args.split("=", 1)[1] if "=" in i.args else "")
        if i.op == "ldr" and lit.startswith("="):
            sym = lit[1:].split()[0] if len(lit) > 1 else ""
            if sym.endswith("+1"):
                out.append(sym[:-2])
                how.append(f"ptr {sym}")
                break
            if sym in TABLES:
                out += TABLES[sym]
                how.append(f"table {sym} ({len(TABLES[sym])})")
                break
    return out, ", ".join(how) or "unresolved"


def target_of(i: Ins) -> str | None:
    if i.op == "bl" and i.args:
        return i.args.split()[0]
    return None


def returns(fn: Fn) -> list[int]:
    out = []
    for k, i in enumerate(fn.ins):
        if (i.op == "pop" and "pc" in i.args) or (i.op == "bx" and i.args.strip() == "lr") or \
                (i.op == "mov" and i.args.replace(" ", "").startswith("pc,")):
            out.append(k)
        elif i.op == "b" and i.args and i.args.split()[0] not in fn.labels:
            out.append(k)                         # tail branch
        elif i.op == "bx" and not is_indirect_call(fn, k) and i.args.strip() != "lr":
            out.append(k)
    return out


def last_setters(fn: Fn, ret: int) -> tuple[list[int], list[int], bool]:
    """(flag-setting instructions, bl/indirect calls whose flags pass through, entry passthrough) for a return."""
    setters: list[int] = []
    calls: list[int] = []
    entry = ret == 0
    seen: set[int] = set()
    work = list(fn.preds()[ret])
    if fn.ins[ret].op == "b" and fn.ins[ret].args.split()[0] not in fn.labels:
        pass
    while work:
        j = work.pop()
        if j in seen:
            continue
        seen.add(j)
        i = fn.ins[j]
        if i.op in ("bl", "blx") or is_indirect_call(fn, j):
            calls.append(j)
            continue
        if sets_flags(i):
            setters.append(j)
            continue
        if j == 0:
            entry = True
        work.extend(fn.preds()[j])
    return sorted(setters), sorted(calls), entry


C_FLAG = re.compile(r"SET_FLAGS_\w+|\bcpu\.(?:nz|c|v)\b|\b(?:nz|cf)\s*=|\bFLAG_[NZCV]\b|\.(?:z|n|c|v)\s*=|"
                    r"\b(?:z|n)\s*=\s*[^=]|\b\w*_result\s*\{[^}]*\b(?:z|n|c|v)\b|\.z\b|\.n\b")


def c_region(path: str, name: str) -> tuple[int, list[tuple[int, str]]] | None:
    lines = open(path, encoding="utf-8", errors="replace").read().split("\n")
    hdr = next((i for i, l in enumerate(lines) if l.startswith("/* ── " + name + " ──")), None)
    if hdr is not None:
        end = next((i for i in range(hdr + 1, len(lines)) if lines[i].startswith("/* ── ")), len(lines))
        return hdr + 1, [(i + 1, lines[i]) for i in range(hdr, end)]
    pat = re.compile(r"\bNext\s+" + re.escape(name) + r"\s*\(")
    s = next((i for i, l in enumerate(lines) if pat.search(l)), None)
    if s is None:
        return None
    e = next((i for i in range(s + 1, len(lines)) if lines[i].startswith("}")), len(lines))
    return s + 1, [(i + 1, lines[i]) for i in range(s, min(e + 1, len(lines)))]


CALLRES = re.compile(r"\bcall_(\w+?)\s*\(")


def c_result_readers(game_dir: str) -> dict[str, list[str]]:
    """callee -> ['file:line', ...] where C code reads .z/.n/.c/.v of a call_<callee>() result."""
    out: dict[str, list[str]] = {}
    for f in sorted(glob.glob(os.path.join(game_dir, "*.c"))):
        lines = open(f, encoding="utf-8", errors="replace").read().split("\n")
        for k, l in enumerate(lines):
            for m in CALLRES.finditer(l):
                callee = m.group(1)
                # direct: call_X(...).z (balanced parens on the line)
                depth, p = 0, m.end() - 1
                while p < len(l):
                    depth += (l[p] == "(") - (l[p] == ")")
                    p += 1
                    if depth == 0:
                        break
                if re.match(r"\s*\.(?:z|n|c|v)\b", l[p:]):
                    out.setdefault(callee, []).append(f"{os.path.basename(f)}:{k + 1}")
                    continue
                a = re.search(r"(\w+)\s*=\s*$", l[:m.start()])
                if a:
                    var = a.group(1)
                    for kk in range(k + 1, min(k + 60, len(lines))):
                        if re.search(r"\b" + var + r"\.(?:z|n|c|v)\b", lines[kk]):
                            out.setdefault(callee, []).append(f"{os.path.basename(f)}:{kk + 1}")
                            break
                        if re.search(r"\b" + var + r"\s*=", lines[kk]):
                            break
    return out


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    asmdir = args[0] if args else os.path.join(funcindex.ROOT, "bn6f", "asm")
    if not glob.glob(os.path.join(asmdir, "*.s")):
        asmdir = os.path.join(funcindex.ROOT, "..", "..", "bn6f", "asm")
    fns, macro = parse_asm(asmdir)
    readable = funcindex.readable_functions()

    # consumer sites: callee -> [(kind, loc, branch)]
    flagret: dict[str, list[str]] = {}
    conds: dict[str, set[str]] = {}       # callee -> the condition branches that consume its flags
    indirect_sites: list[tuple[str, str, str]] = []
    for fn in fns.values():
        for k, i in enumerate(fn.ins):
            if i.op not in COND:
                continue
            srcs, _ = sources(fn, k)
            for j in srcs:
                call = fn.ins[j]
                t = target_of(call)
                why = f"direct: {call.loc()} `bl {t}` -> {i.loc()} `{i.text()}` in {fn.name}"
                if t:
                    flagret.setdefault(t, []).append(why)
                    conds.setdefault(t, set()).add(i.op)
                elif is_indirect_call(fn, j):
                    ts, how = indirect_targets(fn, j)
                    indirect_sites.append((fn.name, call.loc(), f"{i.loc()} `{i.text()}` targets: {how}"))
                    for t2 in ts:
                        conds.setdefault(t2, set()).add(i.op)
                        flagret.setdefault(t2, []).append(
                            f"indirect: {call.loc()} `bx` ({how}) -> {i.loc()} `{i.text()}` in {fn.name}")
    direct = {k for k, v in flagret.items() if v[0].startswith("direct")}
    indirect = {k for k, v in flagret.items() if v[0].startswith("indirect")}
    # the cutscene native-call macro: targets are the script pointers
    cs_consumers = [s for s in indirect_sites if s[0] == "CutsceneCmd_call_native_with_return_value"]
    cs_targets = set(macro) if cs_consumers else set()
    for t in sorted(cs_targets):
        for f, n, enc in macro[t]:
            flagret.setdefault(t, []).append(f"indirect: cs_call_native_with_return_value at {f}:{n} (in {enc}) -> "
                                             f"CutsceneCmd_call_native_with_return_value `bne .halt`")
    # callers whose C reads the result's flags
    readers = c_result_readers(funcindex.GAME_DIR)
    for callee, locs in readers.items():
        if callee in fns:
            flagret.setdefault(callee, []).append("C: reads result flag at " + ", ".join(locs[:4]))
    # pass-through closure
    work = list(flagret)
    while work:
        name = work.pop()
        fn = fns.get(name)
        if not fn:
            continue
        for r in returns(fn):
            _, calls, _ = last_setters(fn, r)
            ts = [target_of(fn.ins[c]) for c in calls]
            if fn.ins[r].op == "b":
                ts.append(fn.ins[r].args.split()[0])
            for t in ts:
                if t and t not in flagret and t in fns:
                    flagret[t] = [f"passthrough: {name} returns the flags of {t} ({fn.ins[r].loc()})"]
                    work.append(t)

    if "--sites" in sys.argv:
        for n, loc, br in indirect_sites:
            print(f"INDIRECT {n} {loc} -> {br}")
    n_direct = len(direct)
    n_cs = len(cs_targets - direct)
    print(f"# flags-returning: {len(flagret)} (direct {n_direct}, cutscene-indirect {n_cs}, "
          f"C-result readers {len([c for c in readers if c in fns])}, indirect sites {len(indirect_sites)})", file=sys.stderr)
    nflag = 0
    for name in sorted(flagret):
        fn = fns.get(name)
        cpath = readable.get(name)
        if not cpath or not fn:
            continue
        reg = c_region(cpath, name)
        if not reg:
            continue
        cl, body = reg
        cflags = [(l, t.strip()) for l, t in body if C_FLAG.search(t) and not t.lstrip().startswith(("/*", "*", "//"))]
        marks = []
        paths = []
        for r in returns(fn):
            setters, calls, entry = last_setters(fn, r)
            paths.append((fn.ins[r], setters, calls, entry))
        distinct = {s for _, ss, _, _ in paths for s in ss}
        only_pass = not distinct
        if only_pass:
            marks.append("PASS")
        elif not cflags:
            marks.append("NOFLAGS")
        elif len(cflags) < len(distinct):
            marks.append("FEWER")
        needs_cv = bool(conds.get(name, set()) - {"beq", "bne", "bmi", "bpl"})
        arith = any(fn.ins[x].op in ("cmp", "cmn", "add", "sub", "adc", "sbc", "neg") for x in distinct)
        has_cv = any(re.search(r"SET_FLAGS_(?:CMP|ADD)|cpu\.[cv]\b|\.(?:c|v)\b|flags_sub|\b[cv]\s*=", t) for _, t in cflags)
        if needs_cv and arith and not has_cv and "NOFLAGS" not in marks:
            marks.append("CV")
        if not marks:
            marks.append("HAS")
        # every C return with no flag-setting construct since the previous return (or the function start)
        retnf: list[str] = []
        prev_ret = 0
        for bi, (l, t) in enumerate(body):
            if re.search(r"RETURN_TO_CALLER\(\)|RETURN_TO\(|return DONE", t) and not t.lstrip().startswith(("/*", "*", "//")):
                seg = body[prev_ret:bi]
                if not any(C_FLAG.search(x) and not x.lstrip().startswith(("/*", "*", "//")) for _, x in seg):
                    guest = any("GUEST_CALL" in x or "run(" in x for _, x in seg[-6:])
                    retnf.append(f"{l}{'(after call)' if guest else ''}")
                prev_ret = bi
        # a call between the last flag-setting construct and a return overwrites those flags
        callafter: list[str] = []
        r0after: list[str] = []      # R0 assigned after the last flag set: the flags may not describe it
        lastflag = -1
        for bi, (l, t) in enumerate(body):
            if t.lstrip().startswith(("/*", "*", "//")):
                continue
            if "SET_FLAGS_" in t or re.search(r"\bcpu\.(?:nz|c|v)\s*=", t):
                lastflag = bi
            if re.search(r"RETURN_TO_CALLER\(\)|RETURN_TO\(|return DONE", t) and lastflag >= 0:
                mid = [x for _, x in body[lastflag + 1:bi]
                       if re.search(r"GUEST_CALL|\bcall_\w+\(|\brun\(", x) and not x.lstrip().startswith(("/*", "*", "//"))]
                if mid:
                    callafter.append(f"{l}")
                r0 = [x for _, x in body[lastflag + 1:bi] if re.search(r"\bR0\s*=[^=]", x)
                      and not x.lstrip().startswith(("/*", "*", "//"))]
                if r0:
                    r0after.append(f"{l}")
        if r0after and "NOFLAGS" not in marks:
            marks.insert(0, "R0AFTER")
        if callafter and "NOFLAGS" not in marks:
            marks.insert(0, "CALLAFTER")
        if retnf and "NOFLAGS" not in marks and "PASS" not in marks:
            marks.insert(0, "RETNF")
        nflag += 1
        print(f"{name} [{'/'.join(marks)}]  {fn.file}:{fn.line}  C {os.path.relpath(cpath, funcindex.ROOT)}:{cl}  conds {','.join(sorted(conds.get(name, [])))}")
        for w in flagret[name][:3]:
            print(f"    consumer {w}")
        for ret, setters, calls, entry in paths:
            desc = ", ".join(f"{fn.ins[s].loc()} `{fn.ins[s].text()}`" for s in setters[:3])
            if calls:
                desc += " | via call " + ", ".join(f"{fn.ins[c].loc()} `{fn.ins[c].text()}`" for c in calls[:2])
            if entry:
                desc += " | entry flags"
            print(f"    return {ret.loc()}: {desc or '-'}")
        if r0after:
            print(f"      C assigns R0 after the last flag set, return at lines {', '.join(r0after[:6])}")
        if callafter:
            print(f"      C calls between the last flag set and the return at lines {', '.join(callafter[:6])}")
        if retnf:
            print(f"      C returns without a flag set since the previous return: lines {', '.join(retnf[:8])}")
        for l, t in cflags[:6]:
            print(f"      C {l}: {t[:100]}")
    print(f"# {nflag} flags-returning functions with readable C", file=sys.stderr)


if __name__ == "__main__":
    main()
