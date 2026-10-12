"""The lifted originals, instrumented for block coverage (tools/blockcov).

instrument(LIFTED_DIR, OUT_DIR) writes OUT_DIR/difftest_lifted_*.c: the lifter's files with
`COV_HIT(0xADDR);` before every instruction of every `Next L_NAME(void)` body (the instruction
comments the lifter writes, `/* 0801A436: cmp r1, #0 */`), and returns the block map:

  NAME -> {"insns": [[addr, asm], ...], "blocks": [leader addr, ...], "dict": [constant, ...]}

A block starts at the entry, at a label (`L_0801A45A:;`, a branch target) and after a conditional
branch (the lifter fuses `cmp; beq` into `if (...) goto L_...;` at the cmp, so the probe of the
instruction after the branch runs only on the fall-through path). The dictionary holds the constants
the function compares against and loads into registers, and its jump tables' cases: values worth
putting into registers and loaded bytes to reach other blocks (a fuzzer's dictionary).
"""
from __future__ import annotations

import glob
import os
import re

FUNC = re.compile(r"^Next L_(\w+)\(void\) \{\s*$")
INSN = re.compile(r"^(\s*)/\* ([0-9A-F]{8}): (.*?) \*/\s*$")
LABEL = re.compile(r"^L_([0-9A-F]{8}):;")
COND_BRANCH = re.compile(r"^b(eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)\b")
IF_GOTO = re.compile(r"^\s*if \((.*)\) goto L_[0-9A-F]{8};")
ASSIGN_CONST = re.compile(r"^\s*r\d+ = (0x[0-9A-Fa-f]+|\d+);")
CASE = re.compile(r"\bcase (0x[0-9A-Fa-f]+|\d+):")
NUMBER = re.compile(r"\b(0x[0-9A-Fa-f]+|\d+)\b")
BRACELESS = re.compile(r"^\s*(?:(?:else\s+)?if|for|while)\s*\(.*\)\s*$|^\s*else\s*$")


def _number(text: str) -> int:
    return int(text, 0) & 0xFFFFFFFF


def instrument(lifted_dir: str, out_dir: str) -> dict[str, dict]:
    os.makedirs(out_dir, exist_ok=True)
    blocks: dict[str, dict] = {}
    for path in sorted(glob.glob(os.path.join(lifted_dir, "difftest_lifted_*.c"))):
        out: list[str] = []
        current: dict | None = None
        prev_code = ""
        after_cond = False
        with open(path) as fh:
            lines = fh.read().split("\n")
        for line in lines:
            m = FUNC.match(line)
            if m:
                current = {"name": m.group(1), "insns": [], "leaders": set(), "dict": set(), "labels": set()}
                after_cond = True            # the entry is a leader
                out.append(line)
                prev_code = line
                continue
            if current is not None and line == "}":
                name = current["name"]
                addrs = [a for a, _ in current["insns"]]
                leaders = sorted(a for a in current["leaders"] | current["labels"] if a in set(addrs))
                blocks[name] = {"insns": current["insns"], "blocks": leaders, "dict": sorted(current["dict"])}
                current = None
                out.append(line)
                continue
            if current is None:
                out.append(line)
                continue
            m = LABEL.match(line)
            if m:
                current["labels"].add(int(m.group(1), 16))
            m = INSN.match(line)
            if m:
                addr = int(m.group(2), 16)
                asm = m.group(3)
                if BRACELESS.match(prev_code):
                    raise ValueError(f"{path}: a probe before {m.group(2)} would become the body of `{prev_code.strip()}`")
                if after_cond:
                    current["leaders"].add(addr)
                    after_cond = False
                current["insns"].append([addr, asm])
                if COND_BRANCH.match(asm):
                    after_cond = True
                out.append(f"{m.group(1)}COV_HIT(0x{addr:08X}u); {line.lstrip()}")
                continue
            stripped = line.strip()
            if stripped and not stripped.startswith(("/*", "*")):
                prev_code = line
            m = IF_GOTO.match(line)
            if m:
                for n in NUMBER.findall(m.group(1)):
                    v = _number(n)
                    current["dict"].update({v, (v - 1) & 0xFFFFFFFF, (v + 1) & 0xFFFFFFFF})
            m = ASSIGN_CONST.match(line)
            if m:
                current["dict"].add(_number(m.group(1)))
            for n in CASE.findall(line):
                current["dict"].add(_number(n))
            out.append(line)
        with open(os.path.join(out_dir, os.path.basename(path)), "w") as fh:
            fh.write("\n".join(out))
    return blocks


GOTO = re.compile(r"^\s*(?:if \((.*)\) )?goto L_([0-9A-F]{8});")


def branches(lifted_dir: str) -> dict[str, list[list]]:
    """NAME -> [[instruction address, target address, condition or None], ...]: every goto of the lifted
    original (the lifter writes `if (COND) goto L_T;` at the compare, `goto L_T;` for a plain branch),
    with the C condition under which it is taken: the path to a block, for step 3's prompt."""
    out: dict[str, list[list]] = {}
    for path in sorted(glob.glob(os.path.join(lifted_dir, "difftest_lifted_*.c"))):
        name = None
        last = 0
        with open(path) as fh:
            for line in fh:
                m = FUNC.match(line)
                if m:
                    name = m.group(1)
                    out[name] = []
                    continue
                if name is None:
                    continue
                if line.rstrip("\n") == "}":
                    name = None
                    continue
                m = INSN.match(line)
                if m:
                    last = int(m.group(2), 16)
                    continue
                m = GOTO.match(line)
                if m:
                    out[name].append([last, int(m.group(2), 16), m.group(1)])
    return out
