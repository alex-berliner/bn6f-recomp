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


def own_reads(lines: list[str]) -> set[str]:
    written: set[str] = set()
    read: set[str] = set()
    for raw in lines:
        code = raw.split("//")[0].split("@")[0].strip()
        if not code or code.endswith(":") or code.startswith("."):
            continue
        m = re.match(r"(\w+)\s*(.*)", code)
        if not m:
            continue
        op, rest = m.group(1).lower(), m.group(2)
        if op in ("push", "pop") or op.startswith(("b", "swi", "svc")) and op not in ("bic", "bics"):
            if op in ("bx", "blx") or op.startswith("b") and op not in ("bl",) and _regs(rest):
                for r in _regs(rest):
                    if r not in written:
                        read.add(r)
            continue
        if op.startswith(("ldm", "stm")):
            base = _regs(rest.split(",")[0])
            for r in base:
                if r not in written:
                    read.add(r)
            regs = _regs(rest.split(",", 1)[1]) if "," in rest else []
            if op.startswith("stm"):
                read.update(r for r in regs if r not in written)
            else:
                written.update(regs)
            continue
        ops = [o.strip() for o in re.split(r",(?![^\[]*\])", rest)]
        if not ops:
            continue
        dest = _regs(ops[0]) if ops[0] and not ops[0].startswith("[") else []
        if op.startswith("str") or op in NO_DEST:
            srcs = _regs(rest)
            dest = []
        else:
            srcs = [r for o in ops[1:] for r in _regs(o)]
            if op in TWO_OP and len(ops) == 2:
                srcs += dest
        for r in srcs:
            if r not in written:
                read.add(r)
        written.update(dest[:1])
    return {r for r in read if r != "r13"}


def written_before_call(lines: list[str], callee: str) -> set[str] | None:
    """Registers written (linear order) before the first `bl CALLEE`; None if it is not called."""
    upto = []
    for raw in lines:
        code = raw.split("//")[0].strip()
        if re.fullmatch(rf"bl\s+{re.escape(callee)}", code) or re.fullmatch(rf"b\s+{re.escape(callee)}", code):
            return _written(upto)
        upto.append(raw)
    return None


def _written(lines: list[str]) -> set[str]:
    w: set[str] = set()
    for raw in lines:
        code = raw.split("//")[0].split("@")[0].strip()
        m = re.match(r"(\w+)\s*(.*)", code)
        if not m or code.endswith(":") or code.startswith("."):
            continue
        op, rest = m.group(1).lower(), m.group(2)
        if op.startswith(("str", "cmp", "cmn", "tst", "push", "pop", "b", "swi", "stm")) and op not in ("bic", "bics"):
            if op == "pop":
                w.update(r for r in _regs(rest))
            continue
        if op.startswith("ldm"):
            w.update(_regs(rest.split(",", 1)[1]) if "," in rest else [])
            continue
        d = _regs(rest.split(",")[0])
        w.update(d[:1])
    return w
