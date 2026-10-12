"""A function's bn6f assembly, and the registers its own instructions read before writing them.

asm_of(src): the lines of `bn6f/asm/FILE.s` from the label at `src` ("FILE.s:LINE", graph.json)
to its `*_func_end` or the next function start.
own_reads(lines): a linear scan (branches ignored, so an approximation): registers r0-r12 an
instruction reads before any earlier instruction wrote them. push/pop are saves, not uses; a `bl`
is not counted (its callee's arguments are the callee's business).
"""
from __future__ import annotations

import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
_files: dict[str, list[str]] = {}
END = re.compile(r"^\s*(thumb_func_end|arm_func_end|thumb_func_start|thumb_local_start|arm_func_start|arm_local_start)\b")
REG = re.compile(r"\b(r1[0-2]|r[0-9]|sb|sl|fp|ip)\b")
ALIAS = {"sb": "r9", "sl": "r10", "fp": "r11", "ip": "r12"}
TWO_OP = {"and", "orr", "eor", "bic", "adc", "sbc", "ror", "mul", "orrs", "ands", "eors", "bics", "adcs", "sbcs",
          "rors", "muls", "lsls", "lsrs", "asrs", "lsl", "lsr", "asr", "add", "sub", "adds", "subs"}
NO_DEST = {"cmp", "cmn", "tst", "bx", "blx", "b", "bl", "beq", "bne", "push", "pop", "stmia", "swi", "svc"}


def asm_of(src: str | None, limit: int = 400) -> list[str]:
    if not src or ":" not in src:
        return []
    f, line = src.split(":")
    path = os.path.join(ROOT, "bn6f", "asm", f)
    if path not in _files:
        if not os.path.exists(path):
            return []
        _files[path] = open(path, encoding="utf-8", errors="replace").read().splitlines()
    lines = _files[path]
    i = int(line) - 1
    out = [lines[i]] if 0 <= i < len(lines) else []
    for j in range(i + 1, min(len(lines), i + limit)):
        if END.match(lines[j]):
            if "func_end" in lines[j]:
                out.append(lines[j])
            break
        out.append(lines[j])
    # a body that ends in a branch to shared code elsewhere in the file: follow it once
    last = next((x.split("//")[0].strip() for x in reversed(out) if x.split("//")[0].strip()
                 and "func_end" not in x), "")
    m = re.fullmatch(r"b\s+(\w+)", last)
    if m and limit > 60:
        k = next((n for n, x in enumerate(lines) if x.startswith(m.group(1) + ":")), None)
        if k is not None and not any(x.startswith(m.group(1) + ":") for x in out):
            out.append(f"// ... continues at {m.group(1)} (shared code):")
            out += asm_of(f"{f}:{k + 1}", 60)
    return out


def _regs(text: str) -> list[str]:
    return [ALIAS.get(r, r) for r in REG.findall(text)]


def scan(lines: list[str], may_write: dict[str, set[str]] | None = None) -> tuple[set[str], list[tuple[str, set[str]]]]:
    """(registers read before written, [(callee, registers written before that call)]).
    A write kills a register only in the straight-line start of the body (before the first label or
    branch other than a call): after a merge point a register may arrive unwritten, so reads stay
    reads. Over-approximates reads, as a narrowing must. A call writes what its callee may write
    (graph.json may_write, when given)."""
    killed: set[str] = set()      # written in the straight-line start: dead on every path after it
    block: set[str] = set()       # written earlier in the current basic block
    frozen = False
    read: set[str] = set()
    calls: list[tuple[str, set[str]]] = []

    def use(regs):
        read.update(r for r in regs if r not in killed and r not in block)

    def kill(regs):
        block.update(regs)
        if not frozen:
            killed.update(regs)

    def merge_point():
        nonlocal frozen
        frozen = True
        block.clear()

    for raw in lines:
        code = raw.split("//")[0].split("@")[0].strip()
        if not code or code.startswith("."):
            continue
        if code.endswith(":") or re.match(r"\w+:", code):
            merge_point()
            continue
        m = re.match(r"(\w+)\s*(.*)", code)
        if not m:
            continue
        op, rest = m.group(1).lower(), m.group(2)
        if op in ("push",):
            continue
        if op == "pop":
            kill(_regs(rest))
            if "pc" in rest:
                merge_point()
            continue
        if op == "bl":
            callee = rest.strip().split()[0] if rest.strip() else ""
            calls.append((callee, killed | block))
            kill((may_write or {}).get(callee, {"r0", "r1", "r2", "r3", "r12"}))
            continue
        if op in ("swi", "svc"):
            use(["r0", "r1", "r2"])
            kill(["r0", "r1", "r3"])
            continue
        if op.startswith("b") and op not in ("bic", "bics"):
            use(_regs(rest))
            if op in ("bx", "blx") and "lr" not in rest:
                calls.append(("<indirect>", killed | block))
            if re.fullmatch(r"b\s+(\w+)", code):
                calls.append((rest.strip(), killed | block))   # a tail branch (to another function or a label)
            merge_point()
            continue
        if op.startswith(("ldm", "stm")):
            use(_regs(rest.split(",")[0]))
            regs = _regs(rest.split(",", 1)[1]) if "," in rest else []
            if op.startswith("stm"):
                use(regs)
            else:
                kill(regs)
            continue
        ops = [o.strip() for o in re.split(r",(?![^\[]*\])", rest)]
        dest = _regs(ops[0]) if ops and ops[0] and not ops[0].startswith("[") else []
        if op.startswith("str") or op in NO_DEST:
            use(_regs(rest))
            continue
        srcs = [r for o in ops[1:] for r in _regs(o)]
        if op in TWO_OP and len(ops) == 2:
            srcs += dest
        use(srcs)
        kill(dest[:1])
        if ops and "pc" in ops[0]:
            merge_point()
    return {r for r in read if r != "r13"}, calls


def own_reads(lines: list[str]) -> set[str]:
    return scan(lines)[0]


def written_before_call(lines: list[str], callee: str) -> set[str] | None:
    """Registers surely written before a call of CALLEE (the least over its calls); None if not called."""
    sets = [k for c, k in scan(lines)[1] if c == callee]
    if not sets:
        return None
    out = sets[0]
    for k in sets[1:]:
        out &= k
    return out


# ── liveness over the function's own control flow ──

COND = {"beq", "bne", "bcs", "bcc", "bhs", "blo", "bmi", "bpl", "bvs", "bvc", "bhi", "bls", "bge", "blt", "bgt", "ble"}


def instructions(lines: list[str]) -> tuple[list[dict], dict[str, int]]:
    """Each instruction as {uses, defs, kind, target}; labels -> instruction index."""
    out: list[dict] = []
    labels: dict[str, int] = {}
    prev_mov_lr_pc = False
    for raw in lines:
        code = raw.split("//")[0].split("@")[0].strip()
        if not code or code.startswith("."):
            continue
        lm = re.match(r"(\w+):\s*(.*)", code)
        if lm:
            labels[lm.group(1)] = len(out)
            code = lm.group(2).strip()
            if not code:
                continue
        m = re.match(r"(\w+)\s*(.*)", code)
        if not m:
            continue
        op, rest = m.group(1).lower(), m.group(2)
        ins = {"uses": set(), "defs": set(), "kind": "op", "target": None}
        if op == "push":
            ins.update(kind="push", uses=set(_regs(rest)))
        elif op == "pop":
            ins["kind"] = "pop"
            ins["popped"] = set(_regs(rest))
            ins["defs"] = set(_regs(rest))
            if "pc" in rest:
                ins["kind"] = "ret"
        elif op == "bl":
            ins.update(kind="call", target=rest.split()[0] if rest.split() else "")
        elif op in ("swi", "svc"):
            ins.update(uses={"r0", "r1", "r2"}, defs={"r0", "r1", "r3"})
        elif op in ("bx", "blx", "mov") and (op != "mov" or rest.replace(" ", "").startswith("pc,")):
            regs = _regs(rest)
            if regs[-1:] == [] and "lr" in rest or (op in ("bx", "mov") and rest.replace(" ", "").endswith("lr")):
                ins["kind"] = "ret"
            elif prev_mov_lr_pc or op == "blx":
                ins.update(kind="icall", uses=set(regs))
            else:
                ins.update(kind="ijump", uses=set(regs))
        elif op.startswith("b") and op not in ("bic", "bics"):
            ins.update(kind="cbranch" if op in COND else "branch", target=rest.strip())
        elif op.startswith(("ldm", "stm")):
            ins["uses"] = set(_regs(rest.split(",")[0]))
            regs = set(_regs(rest.split(",", 1)[1])) if "," in rest else set()
            if op.startswith("stm"):
                ins["uses"] |= regs
            else:
                ins["defs"] = regs
            if "!" in rest:
                ins["defs"] |= set(_regs(rest.split(",")[0]))
        else:
            ops = [o.strip() for o in re.split(r",(?![^\[]*\])", rest)]
            dest = _regs(ops[0]) if ops and ops[0] and not ops[0].startswith("[") else []
            if op.startswith("str") or op in NO_DEST:
                ins["uses"] = set(_regs(rest))
            else:
                srcs = [r for o in ops[1:] for r in _regs(o)]
                if op in TWO_OP and len(ops) == 2:
                    srcs += dest
                ins["uses"] = set(srcs)
                ins["defs"] = set(dest[:1])
                if "]!" in rest or (op.startswith("ldr") and "!" in rest):
                    ins["defs"] |= set(_regs(ops[1])[:1])
        prev_mov_lr_pc = op == "mov" and rest.replace(" ", "").lower() == "lr,pc"
        ins["text"] = code
        out.append(ins)
    # Thumb saves a high register through a low one: `mov r4, r8` ... `push {r4,...}`. Such a copy is
    # a save, not a use of r8 (the value only comes back to r8 through `pop` and `mov r8, r4`).
    for i, x in enumerate(out):
        m = re.fullmatch(r"mov\s+(r[0-7])\s*,\s*(r(?:8|9|1[0-2])|sb|sl|fp|ip)", x["text"].lower())
        if not m:
            continue
        low = m.group(1)
        for y in out[i + 1:i + 6]:
            if y["kind"] == "push" and low in y["uses"]:
                x["uses"] = set()
                break
            if low in y["uses"] or low in y["defs"] or y["kind"] not in ("op",):
                break
    return out, labels


def live_in(lines: list[str], args_of, writes_of) -> set[str] | None:
    """Registers live on entry, a call reading its callee's arguments (args_of(name) -> set) and
    writing what it may write (writes_of(name) -> set). None when control leaves through a jump the
    code computes (a jump table): its targets are unknown here."""
    ins, labels = instructions(lines)
    n = len(ins)
    if not n:
        return set()
    succ: list[list[int]] = []
    for i, x in enumerate(ins):
        k = x["kind"]
        if k == "ijump":
            return None
        if k == "ret":
            succ.append([])
        elif k in ("branch", "cbranch"):
            t = labels.get(x["target"])
            s = [t] if t is not None and t < n else []
            if k == "cbranch" and i + 1 < n:
                s.append(i + 1)
            succ.append(s)
        else:
            succ.append([i + 1] if i + 1 < n else [])
    live = [set() for _ in range(n)]
    changed = True
    while changed:
        changed = False
        for i in range(n - 1, -1, -1):
            x = ins[i]
            out: set[str] = set()
            for s in succ[i]:
                out |= live[s]
            k = x["kind"]
            if k == "call" or (k in ("branch", "cbranch") and x["target"] not in labels):
                use, defs = args_of(x["target"]), writes_of(x["target"])
                if k == "branch":        # a tail call: nothing after it in this function
                    out = set()
            elif k == "icall":
                use, defs = args_of("<indirect>") | x["uses"], writes_of("<indirect>")
            elif k == "push":
                # a saved register is read only if the value comes back (pop) and is read then
                saved = {f"s:{r}" for r in x["uses"]}
                new = (out - saved) | {r for r in x["uses"] if f"s:{r}" in out}
                if new != live[i]:
                    live[i] = new
                    changed = True
                continue
            elif k == "pop":
                use, defs = {f"s:{r}" for r in x["popped"] if r in out}, x["defs"]
            else:
                use, defs = x["uses"], x["defs"]
            new = (out - defs) | use
            if new != live[i]:
                live[i] = new
                changed = True
    return {r for r in live[0] if r not in ("r13", "sp", "pc", "lr") and not r.startswith("s:")}
