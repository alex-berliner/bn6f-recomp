#!/usr/bin/env python3
"""Generate the C-ABI glue from `abi:` signature specs (docs/C_CALLING_CONVENTION.md §2, §3.4).

    python3 tools/abi/gen.py [--out DIR] [SPEC.yaml | DIR ...]   (default: symbols/db; JSON summary)
    python3 tools/abi/gen.py --test                              (fixtures in tools/abi/tests, compiled)

A spec is a symbols/db-style YAML entry (or a list of them) with an `abi:` block;
files without one are skipped:

    name: setBGTileIdsBlocks1And2_8030628
    abi:
      c: void setBGTileIdsBlocks1And2_8030628(u16 tileId1, u16 tileId2, u32 col, u32 row)
      args: [r0, r1, r3, r4]     # the register each parameter arrives in; "&r6" (quoted: & is YAML) is an
                                 # in/out register passed as a u32 * and written back
      ret: none                  # r0 | r0:r1 (returns U32Pair) | flag:z | flag:!z | flag:c (return bool) | none
      globals: [toolkit]         # r10 = A_eToolkit (0x020093B0)
      keep: [{reg: r2, expr: "row * 32 + col", caller: NAME}]   # leftovers a lifted caller reads; expr is C
                                 # over the parameters and `ret`
      convention: c              # c | registers (with reason:)

`convention` decides which side of the boundary the function is on:
  - `c`: the function is (or is being rewritten as) the C function in `c:`. Generated: its
    prototype (abi.h), the register adapter `abi_NAME` that lifted code and dispatch tables call
    (abi_thunks.c: arguments from R0..R12, result to R0 / R0:R1 / a flag, `keep:` leftovers,
    every other register and flag as on entry, IWRAM attribute from tools/lifter/iwram.txt), and
    its difftest.cfg line (compare = result registers + flags + keep, r10 pinned).
  - `registers`: the function keeps the register convention (still lifted, or an exception).
    Generated: a typed wrapper `call_NAME(...)` in lifted_calls.h for readable callers: sets the
    argument registers (and r10), GUEST_CALL(NAME), returns R0, U32Pair{R0, R1} or a bool from
    the flag the spec names (TestEventFlag answers in Z, not r0).

Outputs go to DIR (default cport/build-abi/gen): abi.h, lifted_calls.h, abi_thunks.c,
difftest.cfg. Nothing builds them yet: the lifter changes (§3.7) that point dispatch slots at
abi_NAME and stop declaring `Next NAME(void)` for C functions come first. abi.h includes
gen/structs.h (tools/abi/gen_structs.py), so C signatures can use the game's struct types.
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HERE = os.path.dirname(os.path.abspath(__file__))
TOOLKIT = 0x020093B0
RETS = ("r0", "r0:r1", "flag:z", "flag:!z", "flag:c", "none")
RESERVED = {"ret", "saved_"}
REG_RE = re.compile(r"(&?)r(1[0-2]|[0-9])")


@dataclass
class Param:
    ctype: str
    name: str
    reg: int
    inout: bool

    @property
    def pointer(self) -> bool:
        return "*" in self.ctype


@dataclass
class Spec:
    name: str
    ret_type: str
    params: list[Param]
    ret: str
    globals: list[str]
    keep: list[dict[str, str]]
    convention: str
    source: str
    reason: str = ""
    outputs: list[str] = field(default_factory=list)    # registers/flags the caller gets back


class SpecError(Exception):
    pass


def show(path: str) -> str:
    """A path relative to the repository when it is inside it."""
    rel = os.path.relpath(path, ROOT)
    return path if rel.startswith("..") else rel


def parse_c(sig: str) -> tuple[str, str, list[tuple[str, str]]]:
    """`RET NAME(T a, T *b)` -> (RET, NAME, [(T, a), (T *, b)])."""
    m = re.fullmatch(r"\s*(.+?)\s*\b(\w+)\s*\((.*)\)\s*;?\s*", sig)
    if not m:
        raise SpecError(f"not a C signature: {sig!r}")
    ret, name, plist = m.groups()
    params = []
    if plist.strip() not in ("", "void"):
        for p in plist.split(","):
            pm = re.fullmatch(r"\s*(.+?[\s*])(\w+)\s*", p)
            if not pm:
                raise SpecError(f"parameter without a name: {p.strip()!r}")
            params.append((re.sub(r"\s+", " ", pm.group(1)).strip().replace(" *", " *"), pm.group(2)))
    return ret.strip(), name, params


def load_spec(entry: dict, source: str) -> Spec:
    abi = entry["abi"]
    ret_type, cname, cparams = parse_c(abi.get("c", ""))
    name = entry.get("name", cname)
    if cname != name:
        raise SpecError(f"{source}: c: names {cname}, the entry is {name}")
    args = [str(a) for a in abi.get("args", [])]
    if len(args) != len(cparams):
        raise SpecError(f"{name}: {len(cparams)} parameters but {len(args)} args")
    params = []
    for (ctype, pname), a in zip(cparams, args):
        m = REG_RE.fullmatch(a)
        if not m:
            raise SpecError(f"{name}: arg {a!r} is not r0..r12 or \"&rN\"")
        if pname in RESERVED:
            raise SpecError(f"{name}: parameter name {pname} is reserved in adapters")
        inout = bool(m.group(1))
        if inout and ctype.replace(" ", "") != "u32*":
            raise SpecError(f"{name}: in/out register {a} needs a u32 * parameter, not {ctype}")
        params.append(Param(ctype, pname, int(m.group(2)), inout))
    regs = [p.reg for p in params]
    if len(set(regs)) != len(regs):
        raise SpecError(f"{name}: a register is used for two parameters")
    ret = str(abi.get("ret", "none"))
    want = {"none": "void", "r0:r1": "U32Pair", "flag:z": "bool", "flag:!z": "bool", "flag:c": "bool"}
    if ret not in RETS:
        raise SpecError(f"{name}: ret {ret!r} is not one of {', '.join(RETS)}")
    if ret in want and ret_type != want[ret]:
        raise SpecError(f"{name}: ret {ret} needs C return type {want[ret]}, not {ret_type}")
    if ret == "r0" and ret_type in ("void", "U32Pair"):
        raise SpecError(f"{name}: ret r0 needs a scalar or pointer return type")
    glob_ = [str(g) for g in abi.get("globals") or []]
    if set(glob_) - {"toolkit"}:
        raise SpecError(f"{name}: unknown globals {sorted(set(glob_) - {'toolkit'})}")
    convention = str(abi.get("convention", "c"))
    if convention not in ("c", "registers"):
        raise SpecError(f"{name}: convention {convention!r} is not c or registers")
    keep = [dict(k) for k in abi.get("keep") or []]
    for k in keep:
        if not REG_RE.fullmatch(str(k.get("reg", ""))) or str(k["reg"]).startswith("&") or not k.get("expr"):
            raise SpecError(f"{name}: keep entries need reg: rN and expr:")
    if keep and convention != "c":
        raise SpecError(f"{name}: keep: only applies to C functions (their adapter produces it)")
    s = Spec(name, ret_type, params, ret, glob_, keep, convention, source, str(abi.get("reason", "")))
    s.outputs = {"r0": ["r0"], "r0:r1": ["r0", "r1"], "flag:z": ["nz"], "flag:!z": ["nz"], "flag:c": ["c"],
                 "none": []}[ret] + [f"r{p.reg}" for p in params if p.inout] + [str(k["reg"]) for k in keep]
    if len(set(s.outputs)) != len(s.outputs):
        raise SpecError(f"{name}: a register is both a result and a keep/in-out register")
    return s


def load_specs(paths: list[str]) -> tuple[list[Spec], list[str]]:
    files: list[str] = []
    for p in paths:
        files += sorted(glob.glob(os.path.join(p, "*.yaml"))) if os.path.isdir(p) else [p]
    specs, errors = [], []
    for f in files:
        data = yaml.safe_load(open(f))
        for entry in data if isinstance(data, list) else [data]:
            if not isinstance(entry, dict) or "abi" not in entry:
                continue
            try:
                specs.append(load_spec(entry, show(f)))
            except (SpecError, KeyError, TypeError) as e:
                errors.append(f"{show(f)}: {e}")
    names = [s.name for s in specs]
    errors += [f"{n}: two specs" for n in sorted({n for n in names if names.count(n) > 1})]
    return specs, errors


# ── generated text ──────────────────────────────────────────────────────────

def decl(ctype: str, name: str) -> str:
    """`u32 x`, `u32 *x`."""
    return f"{ctype}{name}" if ctype.endswith("*") else f"{ctype} {name}"


def signature(s: Spec, name: str | None = None) -> str:
    params = ", ".join(decl(p.ctype, p.name) for p in s.params) or "void"
    return decl(s.ret_type, f"{name or s.name}({params})")


def contract(s: Spec) -> str:
    args = ", ".join(f"{'&' if p.inout else ''}r{p.reg} = {p.name}" for p in s.params) or "no arguments"
    if s.globals:
        args += "; r10 = toolkit"
    return f"{s.name}({args}) -> {s.ret}"


def to_reg(p: Param) -> str:
    """A parameter's value as the u32 its register holds."""
    return f"(u32)(uintptr_t){p.name}" if p.pointer else f"(u32){p.name}"


def from_reg(ctype: str, expr: str) -> str:
    return f"({ctype})(uintptr_t){expr}" if "*" in ctype else f"({ctype}){expr}"


def abi_h(specs: list[Spec]) -> str:
    out = ["/* Generated by tools/abi/gen.py from the abi: specs: do not edit.",
           " * C prototypes of readable functions in the C convention (docs/C_CALLING_CONVENTION.md). */",
           "#pragma once", "#include <stdbool.h>", "#include <stdint.h>", '#include "cpu.h"',
           '#include "gen/structs.h"', "",
           "/* Two results in r0:r1. */",
           "typedef struct U32Pair { u32 first, second; } U32Pair;", ""]
    for s in specs:
        if s.convention == "c":
            out.append(f"/* {contract(s)} */")
            out.append(f"{signature(s)};")
            out.append(f"Next abi_{s.name}(void);")
    out.append("")
    return "\n".join(out)


def iwram_modes() -> dict[str, str]:
    modes = {}
    for line in open(os.path.join(ROOT, "tools", "lifter", "iwram.txt")):
        f = line.split("#")[0].split()
        if f:
            modes[f[0]] = f[1] if len(f) > 1 else "thumb"
    return modes


def adapter(s: Spec, modes: dict[str, str]) -> list[str]:
    attr = {"arm": "IWRAM_CODE ", "thumb": "IWRAM_THUMB "}.get(modes.get(s.name, ""), "")
    out = [f"/* {contract(s)} */", f"{attr}Next abi_{s.name}(void) {{",
           "    AbiSaved saved_;", "    abi_save(&saved_);"]
    args = []
    for p in s.params:
        # an in/out register is a local (not &cpu.r[N]: a register-convention call inside may change R<N>)
        args.append(f"&{p.name}" if p.inout else p.name)
        out.append(f"    u32 {p.name} = R{p.reg};" if p.inout else
                   f"    {decl(p.ctype, p.name)} = {from_reg(p.ctype, f'R{p.reg}')};")
    call = f"{s.name}({', '.join(args)})"
    out.append(f"    {call};" if s.ret == "none" else f"    {decl(s.ret_type, 'ret')} = {call};")
    out.append("    abi_restore(&saved_);")
    out += [f"    R{p.reg} = {p.name};" for p in s.params if p.inout]
    if s.ret == "r0":
        out.append("    R0 = (u32)(uintptr_t)ret;" if "*" in s.ret_type else "    R0 = (u32)ret;")
    elif s.ret == "r0:r1":
        out.append("    R0 = ret.first;")
        out.append("    R1 = ret.second;")
    elif s.ret == "flag:z":
        out.append("    cpu.nz = ret ? 0 : 1;          /* Z set: true */")
    elif s.ret == "flag:!z":
        out.append("    cpu.nz = ret ? 1 : 0;          /* Z clear: true */")
    elif s.ret == "flag:c":
        out.append("    cpu.c = ret ? 1 : 0;")
    for k in s.keep:
        out.append(f"    R{str(k['reg'])[1:]} = (u32)({k['expr']});   /* keep: read by {k.get('caller', '?')} */")
    out += ["    RETURN_TO_CALLER();", "}", ""]
    return out


THUNKS_HEAD = """\
/* Generated by tools/abi/gen.py from the abi: specs: do not edit.
 * Register adapters: lifted code and dispatch tables call abi_NAME, which takes the
 * arguments from the guest registers, calls the C function and puts its result back.
 * Every register and flag that is not a result is left as on entry (the C function may
 * call register-convention code through lifted_calls.h, which changes them). */
#include "game.h"
#include "abi.h"

typedef struct AbiSaved { u32 r[13], nz, c, v; } AbiSaved;

static inline void abi_save(AbiSaved *s) {
    for (int i_ = 0; i_ < 13; i_++) s->r[i_] = cpu.r[i_];
    s->nz = cpu.nz; s->c = cpu.c; s->v = cpu.v;
}

static inline void abi_restore(const AbiSaved *s) {
    for (int i_ = 0; i_ < 13; i_++) cpu.r[i_] = s->r[i_];
    cpu.nz = s->nz; cpu.c = s->c; cpu.v = s->v;
}

"""


def abi_thunks_c(specs: list[Spec]) -> str:
    modes = iwram_modes()
    out = [THUNKS_HEAD]
    for s in specs:
        if s.convention == "c":
            out += adapter(s, modes)
    return "\n".join(out)


def wrapper(s: Spec) -> list[str]:
    out = [f"/* {contract(s)}" + (f"; {s.reason}" if s.reason else "") + " */",
           f"Next {s.name}(void);",
           f"static inline {signature(s, 'call_' + s.name)} {{"]
    for p in s.params:
        out.append(f"    R{p.reg} = *{p.name};" if p.inout else f"    R{p.reg} = {to_reg(p)};")
    if s.globals:
        out.append("    R10 = ABI_TOOLKIT;")
    out.append(f"    GUEST_CALL({s.name});")
    for p in s.params:
        if p.inout:
            out.append(f"    *{p.name} = R{p.reg};")
    out.append({"none": "", "r0": f"    return {from_reg(s.ret_type, 'R0')};",
                "r0:r1": "    return (U32Pair){ R0, R1 };",
                "flag:z": "    return ZERO_FLAG();", "flag:!z": "    return !ZERO_FLAG();",
                "flag:c": "    return CARRY_FLAG();"}[s.ret])
    out = [ln for ln in out if ln]
    out += ["}", ""]
    return out


CONTRACT_FLAGS = {"nz": (("z", "ZERO_FLAG()"), ("n", "NEGATIVE_FLAG()")), "c": (("c", "CARRY_FLAG()"),),
                  "v": (("v", "OVERFLOW_FLAG()"),)}


@dataclass
class ContractWrapper:
    """A typed call of a register-convention function made from its lifted register contract alone (no
    `abi:` spec): the farm's generated callee helpers (review 3 Area 4.2; tools/convert/farm.py)."""
    name: str               # the callee
    declaration: str        # what a prompt shows: the prototype, with the result's fields in its comment
    definition: str         # the C: the result struct (if any) and the static inline wrapper


def contract_wrapper(name: str, inputs: list[str], outputs: list[str], note: str,
                     as_struct: bool = False) -> ContractWrapper:
    """`call_NAME(u32 rN, ...)`: one parameter per input register in register order (r10 is left as on
    entry: the toolkit; sp and lr are the call's own), GUEST_CALL(NAME), and the result: void without
    outputs, R0 as a u32 when r0 is the only one (unless AS_STRUCT), else a struct `call_NAME_result`
    holding every output register (.r0 ...) and flag (.z .n from nz, .c, .v). OUTPUTS are what the wrapper
    copies back: the farm gives the callee's outputs its caller's original reads (AS_STRUCT then keeps the
    struct its whole contract has, so a caller's .r0 means the same everywhere). NOTE goes in the wrapper's
    `register form:` comment, so tools/abi/lint.py counts it as the exception it is."""
    regs = [f"r{n}" for n in range(13)]
    params = [r for r in regs if r in inputs and r != "r10"]
    out_regs = [r for r in regs if r in outputs]
    out_flags = [field for flag, fields in CONTRACT_FLAGS.items() if flag in outputs for field in fields]
    shown = f"{' '.join(params + (['r10'] if 'r10' in inputs else [])) or '-'} -> {' '.join(out_regs + [f for f in ('nz', 'c', 'v') if f in outputs]) or '-'}"
    plist = ", ".join(f"u32 {r}" for r in params) or "void"
    head = []
    if not out_regs and not out_flags:
        ret, give = "void", ""
        fields = ""
    elif out_regs == ["r0"] and not out_flags and not as_struct:
        ret, give = "u32", "    return R0;"
        fields = ""
    else:
        ret = f"call_{name}_result"
        parts = ([f"u32 {', '.join(out_regs)};"] if out_regs else []) + (
            [f"bool {', '.join(f for f, _ in out_flags)};"] if out_flags else [])
        fields = " ".join(parts)
        head.append(f"typedef struct {{ {fields} }} {ret};")
        values = [r.upper() for r in out_regs] + [read for _, read in out_flags]
        give = f"    return ({ret}){{ {', '.join(values)} }};"
    declaration = f"{ret} call_{name}({plist});   /* {name}: {shown}" + (f"; returns {{ {fields} }}" if fields else "") + " */"
    lines = head + [f"/* register form: {note} */",
                    f"static inline {ret} call_{name}({plist}) {{"]
    lines += [f"    {r.upper()} = {r};" for r in params]
    lines.append(f"    GUEST_CALL({name});")
    if give:
        lines.append(give)
    lines.append("}")
    return ContractWrapper(name, declaration, "\n".join(lines) + "\n")


def lifted_calls_h(specs: list[Spec]) -> str:
    out = ["/* Generated by tools/abi/gen.py from the abi: specs: do not edit.",
           " * Typed calls from readable C into functions that keep the register convention: set the",
           " * argument registers, run the guest function, return its result as the spec says (a flag",
           " * result comes from the flag, never from r0). */",
           "#pragma once", '#include "game.h"', '#include "abi.h"', "",
           f"#define ABI_TOOLKIT 0x{TOOLKIT:08X}u   /* r10: A_eToolkit */", ""]
    for s in specs:
        if s.convention == "registers":
            out += wrapper(s)
    return "\n".join(out)


def difftest_cfg(specs: list[Spec]) -> str:
    out = ["# Generated by tools/abi/gen.py: difftest.cfg lines for the C-convention functions",
           "# (compare = result registers, flags and keep; r10 pinned to the toolkit)."]
    for s in specs:
        if s.convention != "c":
            continue
        line = s.name
        if s.globals:
            line += f" r10=0x{TOOLKIT:X}"
        if s.outputs:
            line += " compare=" + ",".join(s.outputs)
        out.append(line)
    return "\n".join(out) + "\n"


def generate(specs: list[Spec], out_dir: str) -> list[str]:
    os.makedirs(out_dir, exist_ok=True)
    files = {"abi.h": abi_h(specs), "lifted_calls.h": lifted_calls_h(specs),
             "abi_thunks.c": abi_thunks_c(specs), "difftest.cfg": difftest_cfg(specs)}
    for fname, text in files.items():
        with open(os.path.join(out_dir, fname), "w") as fh:
            fh.write(text)
    return [os.path.join(out_dir, f) for f in files]


# ── --test ──────────────────────────────────────────────────────────────────

ARM_CC = ["arm-none-eabi-gcc", "-mcpu=arm7tdmi", "-mthumb", "-mthumb-interwork", "-ffreestanding",
          "-fno-strict-aliasing", "-fomit-frame-pointer", "-DTARGET_GBA", "-Os"]
HOST_CC = ["gcc", "-O1", "-DTARGET_FLAT", "-DDIFFTEST", "-fno-strict-aliasing"]
WARN = ["-Wall", "-Wextra", "-Werror"]


def self_test() -> dict:
    """Generate from tools/abi/tests into cport/build-abi/gen-test and compile abi_thunks.c and
    tests/use_abi.c for the GBA and the 64-bit host. The lifter still declares `Next NAME(void)`
    for every function (until §3.7), so the test uses a copy of cport/include/lifted.h without
    the C-convention functions' declarations."""
    specs, errors = load_specs([os.path.join(HERE, "tests")])
    if errors:
        return {"ok": False, "errors": errors}
    tmp = os.path.join(ROOT, "cport", "build-abi", "gen-test")
    inc = os.path.join(tmp, "inc")
    generate(specs, inc)
    subprocess.run([sys.executable, os.path.join(HERE, "gen_structs.py"), "--out",
                    os.path.join(inc, "gen", "structs.h")], check=True, capture_output=True)
    lifted = os.path.join(ROOT, "cport", "include", "lifted.h")
    if not os.path.exists(lifted):
        return {"ok": False, "errors": ["cport/include/lifted.h is missing: lift once (tools/build_lifted.sh)"]}
    cfuncs = {s.name for s in specs if s.convention == "c"}
    # game.h's #include "lifted.h" looks next to game.h first: a copy sits next to the filtered lifted.h
    with open(os.path.join(inc, "game.h"), "w") as fh:
        fh.write(open(os.path.join(ROOT, "cport", "include", "game.h")).read())
    with open(os.path.join(inc, "lifted.h"), "w") as fh:
        for line in open(lifted):
            m = re.match(r"Next (\w+)\(void\);", line)
            if not (m and m.group(1) in cfuncs):
                fh.write(line)
    results = []
    for target, cc in (("gba", ARM_CC), ("host", HOST_CC)):
        for src in (os.path.join(inc, "abi_thunks.c"), os.path.join(HERE, "tests", "use_abi.c")):
            cmd = cc + WARN + ["-I" + inc, "-I" + os.path.join(ROOT, "cport", "include"), "-c", src,
                               "-o", os.path.join(tmp, f"{target}_{os.path.basename(src)}.o")]
            done = subprocess.run(cmd, capture_output=True, text=True)
            results.append({"target": target, "file": show(src),
                            "ok": done.returncode == 0, "command": " ".join(cmd),
                            **({"errors": done.stderr.strip().splitlines()[:20]} if done.returncode else {})})
    return {"ok": all(r["ok"] for r in results), "out": tmp, "specs": [s.name for s in specs],
            "compiled": results}


def main() -> None:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("specs", nargs="*", help="spec YAML files or directories (default: symbols/db)")
    ap.add_argument("--out", default=os.path.join(ROOT, "cport", "build-abi", "gen"))
    ap.add_argument("--test", action="store_true", help="generate from tools/abi/tests and compile (GBA + host)")
    args = ap.parse_args()
    if args.test:
        report = self_test()
        json.dump(report, sys.stdout, indent=1)
        print()
        sys.exit(0 if report["ok"] else 1)
    specs, errors = load_specs(args.specs or [os.path.join(ROOT, "symbols", "db")])
    report: dict = {"specs": len(specs), "c": sum(s.convention == "c" for s in specs),
                    "registers": sum(s.convention == "registers" for s in specs), "errors": errors}
    if not errors:
        report["files"] = [show(f) for f in generate(specs, args.out)]
    json.dump(report, sys.stdout, indent=1)
    print()
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
