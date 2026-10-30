#!/usr/bin/env python3
"""Flag call sites where the asm keeps a value in a register that the callee reads implicitly, and the C
neither passes it as a wrapper argument nor stores it in the global register (R5 = ...).

Bug class (c1b67e74, b8b4fb44): the asm does `ldr r5, =x; ldr r5, [r5]` once and calls routines that read
r5 (or r0-r3 left over from an earlier call); the C keeps the value in a local, the callee sees stale R5.

For each readable function (funcindex.readable_functions) the asm is walked with a forward dataflow:
LIVE = registers the function itself wrote and did not lose (pop, or a `bl` clobbering r0-r3/r12).
At every `bl CALLEE`, LIVE & INPUTS(CALLEE) are the registers that must reach the callee; INPUTS is
"read before written on some path from the callee's entry", transitively through its own bl and tail
branches (push is not a read). The C side: all call_* wrappers of CALLEE (params named rN, plus rN
assigned in the wrapper body) used in the function's C region, plus any `RN = ...` outside wrappers in
that region. A register not covered is flagged. Conservative: false positives are expected; review by hand.

usage: tools/check_live_regs.py [--all] [ASM_DIR]    (--all also lists callees with no C call found)
"""
from __future__ import annotations

import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funcindex  # noqa: E402

START = re.compile(r"^\s*(?:thumb|arm)_(?:func|local)_start\b")
END = re.compile(r"^\s*(?:thumb|arm)_func_end\b")
LABEL = re.compile(r"^(\w+):")
REG = re.compile(r"\b(r(?:1[0-2]|[0-9])|sp|lr|pc)\b")
COND = {"beq", "bne", "bcs", "bcc", "bhs", "blo", "bmi", "bpl", "bvs", "bvc", "bhi", "bls", "bge", "blt", "bgt", "ble"}
TRACK = [f"r{i}" for i in range(13)]
CALL_CLOBBER = {"r0", "r1", "r2", "r3", "r12"}


class Ins:
    __slots__ = ("op", "args", "line", "file")

    def __init__(self, op: str, args: str, line: int, file: str) -> None:
        self.op, self.args, self.line, self.file = op, args, line, file


class Fn:
    def __init__(self, name: str) -> None:
        self.name = name
        self.ins: list[Ins] = []
        self.labels: dict[str, int] = {}


def regs_of(text: str) -> list[str]:
    return [r for r in REG.findall(text) if r in TRACK]


def reglist(text: str) -> list[str]:
    m = re.search(r"\{([^}]*)\}", text)
    out: list[str] = []
    if not m:
        return out
    for part in m.group(1).split(","):
        part = part.strip()
        mm = re.fullmatch(r"(r\d+)-(r\d+)", part)
        if mm:
            out += [f"r{i}" for i in range(int(mm.group(1)[1:]), int(mm.group(2)[1:]) + 1)]
        elif part in TRACK:
            out.append(part)
    return out


def parse_asm(asm_dir: str) -> dict[str, Fn]:
    fns: dict[str, Fn] = {}
    for path in sorted(glob.glob(os.path.join(asm_dir, "*.s"))):
        cur: Fn | None = None
        pending = False
        with open(path, encoding="utf-8", errors="replace") as fh:
            for n, raw in enumerate(fh, 1):
                if START.match(raw):
                    pending, cur = True, None
                    continue
                if END.match(raw):
                    cur = None
                    continue
                line = raw.split("//")[0].strip()
                if not line or line.startswith((".", "@")):
                    continue
                m = LABEL.match(line)
                if m:
                    if pending:
                        cur, pending = Fn(m.group(1)), False
                        fns[cur.name] = cur
                    if cur is not None:
                        cur.labels[m.group(1)] = len(cur.ins)
                    continue
                if cur is None:
                    continue
                parts = line.split(None, 1)
                cur.ins.append(Ins(parts[0].lower(), parts[1] if len(parts) > 1 else "", n, os.path.basename(path)))
    return fns


def effects(i: Ins) -> tuple[list[str], list[str]]:
    """(reads, writes) of one non-branch instruction, over r0-r12."""
    op, a = i.op, i.args
    if op == "movflag":
        return [], ["r0", "r1"]
    if op == "push":
        return [], []                       # saving, not using
    if op == "pop":
        return [], reglist(a)
    if op in ("cmp", "cmn", "tst"):
        return regs_of(a), []
    if op in ("str", "strb", "strh"):
        return regs_of(a), []
    if op == "stmia":
        base = a.split(",")[0].replace("!", "").strip()
        return regs_of(a), [base] if base in TRACK else []
    if op == "ldmia":
        base = a.split(",")[0].replace("!", "").strip()
        return [base] if base in TRACK else [], reglist(a) + ([base] if base in TRACK else [])
    ops = [x.strip() for x in a.split(",", 1)]
    if not ops or ops[0] not in TRACK:
        return [], []
    rd = ops[0]
    rest = regs_of(ops[1]) if len(ops) > 1 else []
    if op in ("mov", "mvn", "neg", "ldr", "ldrb", "ldrh", "ldrsb", "ldrsh", "adr"):
        return rest, [rd]
    if op in ("add", "sub", "lsl", "lsr", "asr", "ror", "orr", "and", "bic", "eor", "mul", "adc", "sbc"):
        nops = len(a.split(","))
        if nops >= 3:                         # rd, rs, rt/#imm
            return rest, [rd]
        return rest + [rd], [rd]              # rd, rt/#imm (read-modify-write)
    return [], []


def targets(i: Ins, fn: Fn) -> tuple[bool, list[int | str]]:
    """(falls_through, successors) for a branch; successor is an index or a foreign label (tail call)."""
    op = i.op
    lab = i.args.split()[0] if i.args else ""
    t: int | str = fn.labels.get(lab, lab)
    if op == "b":
        return False, [t]
    return True, [t]


_inputs_memo: dict[str, frozenset[str]] = {}


def inputs(fns: dict[str, Fn], name: str, stack: tuple[str, ...] = ()) -> frozenset[str]:
    """Registers the function reads before writing them, on some path (transitive through bl / tail b)."""
    if name in _inputs_memo:
        return _inputs_memo[name]
    fn = fns.get(name)
    if fn is None or name in stack:
        return frozenset()
    stack = stack + (name,)
    found: set[str] = set()
    seen: set[tuple[int, frozenset[str]]] = set()
    work: list[tuple[int, frozenset[str]]] = [(0, frozenset())]
    while work:
        idx, wr = work.pop()
        while 0 <= idx < len(fn.ins):
            if (idx, wr) in seen:
                break
            seen.add((idx, wr))
            i = fn.ins[idx]
            op = i.op
            if op == "bl":
                callee = i.args.split()[0] if i.args else ""
                found |= {r for r in inputs(fns, callee, stack) if r not in wr}
                wr = wr | CALL_CLOBBER
                idx += 1
                continue
            if op == "blx" or op == "bx" or (op == "pop" and "pc" in i.args):
                if op == "pop":
                    wr = wr | frozenset(reglist(i.args))
                elif op == "blx":
                    found |= {r for r in regs_of(i.args) if r not in wr}
                    idx += 1
                    continue
                else:
                    found |= {r for r in regs_of(i.args) if r not in wr}
                break
            if op == "b" or op in COND:
                fall, succ = targets(i, fn)
                for s in succ:
                    if isinstance(s, int):
                        work.append((s, wr))
                    else:                       # tail call into another function
                        found |= {r for r in inputs(fns, s, stack) if r not in wr}
                if not fall:
                    break
                idx += 1
                continue
            rd, wrt = effects(i)
            found |= {r for r in rd if r not in wr}
            wr = wr | frozenset(wrt)
            idx += 1
    res = frozenset(found)
    if not stack[:-1]:
        _inputs_memo[name] = res
    return res


def live_at_calls(fns: dict[str, Fn], fn: Fn) -> list[tuple[Ins, frozenset[str], frozenset[str]]]:
    """For every bl: (registers the function wrote itself on some path and still holds, the subset whose
    value is not just a `mov rN, #0` on some path)."""
    State = tuple[frozenset[str], frozenset[str]]       # (live, zero-valued)
    state: dict[int, set[State]] = {}
    out: dict[int, tuple[set[str], set[str]]] = {}
    work: list[tuple[int, State]] = [(0, (frozenset(), frozenset()))]
    while work:
        idx, st = work.pop()
        live, zero = st
        while 0 <= idx < len(fn.ins):
            if (live, zero) in state.setdefault(idx, set()):
                break
            state[idx].add((live, zero))
            i = fn.ins[idx]
            op = i.op
            if op == "bl":
                o = out.setdefault(idx, (set(), set()))
                o[0].update(live)
                o[1].update(live - zero)
                live, zero = live - CALL_CLOBBER, zero - CALL_CLOBBER
                idx += 1
                continue
            if op in ("bx", "blx") or (op == "pop" and "pc" in i.args):
                break
            if op == "b" or op in COND:
                fall, succ = targets(i, fn)
                for s_ in succ:
                    if isinstance(s_, int):
                        work.append((s_, (live, zero)))
                if not fall:
                    break
                idx += 1
                continue
            rd, wrt = effects(i)
            if op == "pop":
                live, zero = live - frozenset(wrt), zero - frozenset(wrt)
            else:
                live = live | frozenset(wrt)
                is0 = op == "mov" and re.fullmatch(r"\w+,\s*#0(x0+)?\s*", i.args) is not None
                zero = (zero | frozenset(wrt)) if is0 else (zero - frozenset(wrt))
            idx += 1
    return [(fn.ins[k], frozenset(v[0]), frozenset(v[1])) for k, v in sorted(out.items())]


# ── C side

WRAP = re.compile(r"static\s+inline\s+[\w\s\*]*?\b(call_\w+)\s*\(([^)]*)\)\s*\{(.*?)^\}", re.S | re.M)
GUEST = re.compile(r"GUEST_CALL\((\w+)\)")
RASSIGN = re.compile(r"\bR(\d+)\s*=[^=]")


PARAMS: dict[str, list[str]] = {}


def split_args(text: str, open_idx: int) -> list[str]:
    """The top-level arguments of the call whose '(' is at OPEN_IDX."""
    depth, cur, out = 0, "", []
    for ch in text[open_idx:]:
        if ch in "([{":
            depth += 1
            if depth == 1:
                continue
        elif ch in ")]}":
            depth -= 1
            if depth == 0:
                if cur.strip():
                    out.append(cur.strip())
                return out
        if ch == "," and depth == 1:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    return out


def nonzero_writes(fn: Fn) -> set[str]:
    out: set[str] = set()
    for i in fn.ins:
        if i.op == "bl":
            continue
        for r in effects(i)[1]:
            if not (i.op == "mov" and re.fullmatch(r"\w+,\s*#0(x0+)?\s*", i.args)):
                out.add(r)
    return out


def load_c(game_dir: str) -> tuple[dict[str, tuple[str, set[str]]], dict[str, str]]:
    wrappers: dict[str, tuple[str, set[str]]] = {}
    texts: dict[str, str] = {}
    for path in sorted(glob.glob(os.path.join(game_dir, "*.c"))):
        with open(path, encoding="utf-8", errors="replace") as fh:
            texts[path] = fh.read()
        for m in WRAP.finditer(texts[path]):
            g = GUEST.search(m.group(3))
            if not g:
                continue
            covered = {p for p in re.findall(r"\b(r\d+)\b", m.group(2))}
            covered |= {f"r{x}" for x in RASSIGN.findall(m.group(3))}
            wrappers[m.group(1)] = (g.group(1), covered)
            PARAMS[m.group(1)] = re.findall(r"\b(r\d+)\b", m.group(2))
    return wrappers, texts


def strip_wrappers(text: str) -> str:
    return WRAP.sub(lambda m: "\n" * m.group(0).count("\n"), text)


def region(text: str, name: str) -> tuple[str, int] | None:
    """The C text from the end of the previous readable function (wrappers blanked) to the end of NAME."""
    text = strip_wrappers(text)       # same line numbers; indices below are into the stripped text
    m = re.search(r"^(?:IWRAM_\w+\s+|ARM_CODE\s+)*Next\s+%s\s*\(\s*void\s*\)\s*\{" % re.escape(name), text, re.M)
    if not m:
        return None
    end = text.find("\n}\n", m.end())
    end = len(text) if end < 0 else end + 3
    prev = [x.end() for x in funcindex.READABLE.finditer(text, 0, m.start())]
    start = 0
    if prev:
        e = text.find("\n}\n", prev[-1])
        start = e + 3 if e >= 0 else prev[-1]
    return text[start:end], text.count("\n", 0, start) + 1


PRESERVES = {}


def preserved(fns: dict[str, Fn], callee: str) -> set[str]:
    """Registers the callee pushes (and so restores): a wrapper's R_n result after it is the value before."""
    fn = fns.get(callee)
    if fn is None:
        return set()
    if callee not in PRESERVES:
        pushed: set[str] = set()
        for i in fn.ins:
            if i.op == "push":
                pushed |= set(reglist(i.args))
            elif i.op not in ("pop",) and not pushed:
                pass
        popped: set[str] = set()
        for i in fn.ins:
            if i.op == "pop" and "pc" in i.args:
                popped |= set(reglist(i.args))
        PRESERVES[callee] = pushed & popped
    return PRESERVES[callee]


STALE = re.compile(r"\b(\w+)\s*=\s*(call_\w+)\s*\(")


def stale_returns(fns: dict[str, Fn], wrappers: dict[str, tuple[str, set[str]]], body: str,
                  first_line: int, fn: Fn) -> list[tuple[str, str, str, int]]:
    """`x = call_W(...)` then `x.rN` where W's callee preserves rN (push/pop): that is the stale R_N from
    before the call (the second c1b67e74 shape: `dataPtr = init.r5` after a callee that restores r5)."""
    out = []
    mine = {r for i in fn.ins if i.op != "bl" for r in effects(i)[1] if i.op != "pop"}      # written by the function
    covered = {f"r{x}" for x in RASSIGN.findall(body)}
    for m in STALE.finditer(body):
        var, w = m.group(1), m.group(2)
        if w not in wrappers:
            continue
        callee = wrappers[w][0]
        for u in re.finditer(r"\b%s\.(r\d+)\b" % re.escape(var), body[m.end():]):
            if u.group(1) in preserved(fns, callee) and u.group(1) in mine and u.group(1) not in covered:
                line = first_line + body.count("\n", 0, m.end() + u.start())
                out.append((var, callee, u.group(1), line))
    return out


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    show_all = "--all" in sys.argv
    asm_dir = args[0] if args else os.path.join(funcindex.ROOT, "bn6f", "asm")
    fns = parse_asm(asm_dir)
    wrappers, texts = load_c(funcindex.GAME_DIR)
    by_callee: dict[str, list[str]] = {}
    for w, (c, _) in wrappers.items():
        by_callee.setdefault(c, []).append(w)
    sites = flagged = 0
    for name, path in sorted(funcindex.readable_functions().items()):
        fn = fns.get(name)
        if fn is None:
            continue
        reg = region(texts[path], name)
        if reg is None:
            continue
        body, first_line = reg
        assigned = {f"r{x}" for x in RASSIGN.findall(body)}
        for var, callee, r, line in stale_returns(fns, wrappers, body, first_line, fn):
            print(f"STALE {name} {var}.{r} after {callee} (preserves {r}) {os.path.basename(path)}:{line}")
        calls = live_at_calls(fns, fn)
        livebycallee: dict[str, set[str]] = {}
        for ins, live, nonzero in calls:
            livebycallee.setdefault(ins.args.split()[0], set()).update(nonzero)
        for w, (callee, _) in sorted(wrappers.items()):
            for m in re.finditer(r"\b%s\s*\(" % w, body):
                for reg_, arg in zip(PARAMS[w], split_args(body, m.end() - 1)):
                    if re.fullmatch(r"0[uU]?|0x0+[uU]?", arg) and reg_ in livebycallee.get(callee, ()):
                        line = first_line + body.count("\n", 0, m.start())
                        print(f"ZERO {name} {callee} {reg_}=0 (asm has a non-zero value there) {os.path.basename(path)}:{line}")
        for ins, live, _nz in calls:
            callee = ins.args.split()[0]
            need = (live & inputs(fns, callee)) if callee in fns else frozenset()
            if not need:
                continue
            sites += 1
            used = [w for w in by_callee.get(callee, []) if re.search(r"\b%s\s*\(" % w, body)]
            direct = re.search(r"\b%s\s*\(" % re.escape(callee), body)
            if not used and not direct:
                if show_all:
                    print(f"NOCALL {name} bl {callee} {ins.file}:{ins.line} regs {' '.join(sorted(need, key=lambda r: int(r[1:])))}")
                continue
            covered = set(assigned)
            for w in used:
                covered |= wrappers[w][1]
            miss = sorted(need - covered, key=lambda r: int(r[1:]))
            if direct and not used:
                miss = []        # typed C call: args are the C parameters, left to manual review
                continue
            if miss:
                flagged += 1
                cl = ""
                for w in used:
                    mm = re.search(r"\b%s\s*\(" % w, body)
                    if mm:
                        cl = f"{os.path.basename(path)}:{first_line + body.count(chr(10), 0, mm.start())}"
                        break
                print(f"FLAG {name} bl {callee} {' '.join(miss)} asm {ins.file}:{ins.line} C {cl}")
    print(f"# sites with live callee-read registers: {sites}, flagged: {flagged}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
