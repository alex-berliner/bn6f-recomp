#!/usr/bin/env python3
"""The prompt for converting a whole subsystem in one go, from the knowledge layer.

    python3 tools/sig/subsys_prompt.py SUBSYSTEM --file NAME.c [--skip F...] [--entries F...] > prompt.txt
    python3 tools/sig/subsys_prompt.py SUBSYSTEM --wrappers-only     # the generated wrappers, as C

Inputs: tools/sig/subsystems.json (members, entry points), build/sig/final.json (signatures:
tools/sig/ask.py merge), bn6f's assembly (tools/sig/asm.py), today's readable C of each member
(cport/src/game parts, `/* ── NAME ── */`), and the bn6f structs the signatures name
(tools/abi/gen_structs.py, copied into the file with their offset asserts).
The calls out of the subsystem get generated wrappers: typed (`call_NAME(T a, ...)` -> result)
where the callee's signature is confident, register form (every register its contract may read)
where it is not.
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "sig"))
sys.path.insert(0, os.path.join(ROOT, "tools", "abi"))
import asm as asmsrc  # noqa: E402

FINAL = os.path.join(ROOT, "build/sig/final.json")
SUBS = os.path.join(ROOT, "tools/sig/subsystems.json")
GRAPH = os.path.join(ROOT, "build-abi/graph.json")
STRUCTS_H = os.path.join(ROOT, "build/sig/structs.h")


def chosen(s: dict) -> tuple[list[dict], dict]:
    """The signature to use: the model's when it agreed with the mechanical pass, else the mechanical one."""
    if s.get("chosen"):
        return s["chosen"]["params"], s["chosen"]["ret"]
    ps = [{"reg": p["reg"], "type": p["type"] or "u32", "name": p["name"] or p["reg"]} for p in s["params"]
          if p["reg"] != "r10"]
    return ps, {"reg": s["ret"]["from"], "type": s["ret"]["type"] or "u32"}


def ctype(t: str, local_structs: set[str]) -> str:
    t = (t or "u32").strip()
    m = re.fullmatch(r"(?:const\s+)?(\w+)\s*\*", t)
    if m and m.group(1) not in ("u8", "u16", "u32", "s8", "s16", "s32", "void") and m.group(1) not in local_structs:
        return "u32"
    if "(" in t:
        return "u32"
    return t


def to_reg(name: str, t: str) -> str:
    return f"(u32)(uintptr_t){name}" if "*" in t else name


def wrapper(name: str, s: dict, local_structs: set[str], set_by_callers: set[str] | None = None) -> str:
    """set_by_callers: the argument registers every call site in the subsystem writes before the call;
    the others (high registers always) pass through as the guest has them, as in the original."""
    passed = []
    if s["confidence"] == "confident":
        ps, ret = chosen(s)
        note = f"register form: generated typed wrapper: {name}'s signature is confident (symbols/db {name}.yaml, signature:)"
        keep = [p for p in ps if p["reg"] in {f"r{i}" for i in range(4)} or
                (p["reg"] in {f"r{i}" for i in range(4, 8)} and (set_by_callers is None or p["reg"] in set_by_callers))]
        passed = [p["reg"] for p in ps if p not in keep]
        ps = keep
        if passed:
            note += (f"; {', '.join(passed)} pass through as the guest has them (no call site here sets them: "
                     f"the original passes its own caller's values on)")
    else:
        ps = [{"reg": r, "type": "u32", "name": r} for r in s["upper_args"] if r != "r10"]
        ret = {"reg": s["ret"]["from"], "type": "u32"}
        keep = [p for p in ps if p["reg"] in {f"r{i}" for i in range(4)} or
                (p["reg"] in {f"r{i}" for i in range(4, 8)} and (set_by_callers is None or p["reg"] in set_by_callers))]
        passed = [p["reg"] for p in ps if p not in keep]
        ps = keep
        note = (f"register form: generated wrapper: {name}'s signature is unsure, so every register its "
                f"contract may read is passed" + (f" ({', '.join(passed)} pass through as the guest has them: "
                f"no call site here sets them)" if passed else ""))
    args = ", ".join(f"{ctype(p.get('type'), local_structs)} {p.get('name') or p['reg']}" for p in ps) or "void"
    sets = "".join(f"    {p['reg'].upper()} = {to_reg(p.get('name') or p['reg'], ctype(p.get('type'), local_structs))};\n"
                   for p in ps)
    r = (ret or {}).get("reg", "none")
    conds = set(bounds_of(name).get("flag_conds", []))
    if "nz" in s.get("flags_out", []) and conds & {"eq", "ne"} and not r.startswith("flag:"):
        # callers test Z after it (and may use its registers too): return both
        fields = {"r0": ["r0"], "r0:r1": ["r0", "r1"]}.get(r, [])
        rt = f"{name}_result"
        typedef = f"typedef struct {{ {''.join(f'u32 {x}; ' for x in fields)}bool z; }} {rt};   /* z: the Z flag it leaves */\n"
        back = f"    return ({rt}){{ {''.join(f'R{x[1:]}, ' for x in fields)}ZERO_FLAG() }};\n"
        return (typedef + f"/* {note} */\nstatic inline {rt} call_{name}({args}) {{\n{sets}    GUEST_CALL({name});\n{back}}}\n")
    if r == "r0":
        rt, back = ctype(ret.get("type") or "u32", local_structs), "    return R0;\n"
        if "*" in rt:
            back = f"    return ({rt})(uintptr_t)R0;\n"
        elif rt == "bool":
            back = "    return R0 != 0;\n"
    elif r == "r0:r1":
        rt, back = "U32Pair", "    return (U32Pair){ R0, R1 };\n"
    elif r.startswith("flag:"):
        f = {"z": "ZERO_FLAG()", "c": "CARRY_FLAG()", "n": "NEGATIVE_FLAG()"}.get(r[5:].lstrip("!"), "ZERO_FLAG()")
        rt, back = "bool", f"    return {f};   /* {r[5:]} set */\n"
    else:
        rt, back = "void", ""
    return (f"/* {note} */\nstatic inline {rt} call_{name}({args}) {{\n{sets}    GUEST_CALL({name});\n{back}}}\n")


_BOUNDS: dict = {}


def bounds_of(name: str) -> dict:
    if not _BOUNDS:
        _BOUNDS.update(json.load(open(os.path.join(ROOT, "build/sig/bounds.json"))))
    return _BOUNDS.get(name, {}).get("final", {})


def part_of(name: str) -> str | None:
    for p in glob.glob(os.path.join(ROOT, "cport/src/game/*.c")):
        text = open(p).read()
        m = re.search(rf"^/\* ── {re.escape(name)} ── \*/\n(.*?)(?=^/\* ── |\Z)", text, re.M | re.S)
        if m:
            return m.group(1).rstrip() + "\n"
    return None


def struct_block(name: str) -> str | None:
    if not os.path.exists(STRUCTS_H):
        os.makedirs(os.path.dirname(STRUCTS_H), exist_ok=True)
        os.system(f"python3 {ROOT}/tools/abi/gen_structs.py --out {STRUCTS_H} > /dev/null")
    text = open(STRUCTS_H).read()
    m = re.search(rf"^/\* o{name}: .*?\*/\ntypedef struct {name} \{{.*?^_Static_assert\(sizeof\({name}\).*?\n", text, re.M | re.S)
    return m.group(0) if m else None


def main() -> None:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("subsystem")
    ap.add_argument("--file", default="subsystem.c")
    ap.add_argument("--skip", nargs="*", default=[], help="members left as they are (e.g. dead code)")
    ap.add_argument("--entries", nargs="*", help="entry points (default: tools/sig/subsystems.json's)")
    ap.add_argument("--wrappers-only", action="store_true")
    ap.add_argument("--preamble", help="write the file's start (includes, structs, wrappers) here; the model "
                    "writes only what follows")
    ap.add_argument("--fold", nargs="*", default=[], help="members that return to their caller's caller (they pop "
                    "the caller's frame): written inside their caller's C function, never called on their own")
    a = ap.parse_args()
    sigs = json.load(open(FINAL))
    sub = next(s for s in json.load(open(SUBS))["subsystems"] if s["name"] == a.subsystem)
    g = json.load(open(GRAPH))
    members = [u["unit"] for u in sub["units"] if u["unit"] not in a.skip]
    inside = set(members)
    entries = a.entries if a.entries is not None else [u["unit"] for u in sub["units"] if u["entry"] and u["unit"] in inside]
    calls_out: list[str] = []
    for s in g["sites"]:
        if s["caller"] in inside and s["kind"] in ("call", "tail") and s.get("target") and s["target"] not in inside:
            if s["target"] not in calls_out:
                calls_out.append(s["target"])
    structs = set()
    for n in members + calls_out:
        ps, ret = chosen(sigs[n])
        for t in [p.get("type") or "" for p in ps] + [str(ret.get("type") or "")]:
            m = re.fullmatch(r"(?:const\s+)?(\w+)\s*\*", t.strip())
            if m and struct_block(m.group(1)):
                structs.add(m.group(1))
    # only the structs this subsystem's own functions use are copied in; callees' get u32
    own_structs = set()
    for n in members:
        ps, ret = chosen(sigs[n])
        for t in [p.get("type") or "" for p in ps] + [str(ret.get("type") or "")]:
            m = re.fullmatch(r"(?:const\s+)?(\w+)\s*\*", t.strip())
            if m and m.group(1) in structs:
                own_structs.add(m.group(1))
    sets: dict[str, set[str]] = {}
    for c in calls_out:
        for m in members:
            k = asmsrc.written_before_call(asmsrc.asm_of(g["entries"][m].get("src")), c)
            if k is not None:
                sets[c] = k if c not in sets else sets[c] & k
    wrappers = "\n".join(wrapper(c, sigs[c], own_structs, sets.get(c))
                         for c in sorted(calls_out, key=lambda c: sigs[c]["addr"]))
    if "U32Pair" in wrappers:
        wrappers = "typedef struct { u32 r0, r1; } U32Pair;   /* a result in r0 and r1 */\n\n" + wrappers
    if a.wrappers_only:
        print(wrappers)
        return
    extra_entries = [(n, e["unit"]) for n, e in g["entries"].items() if e["unit"] in inside and n != e["unit"]]
    extra = "".join(f"\n{n} is a second entry into {u} (it starts part way into its code): it needs its own "
                    f"adapter `Next {n}(void)` too, calling the typed function with what that entry sets up."
                    for n, u in extra_entries if u in entries)
    out = [PROMPT_HEAD.format(file=a.file, n=len(members), subsystem=a.subsystem,
                              entries=", ".join(entries), extra=extra)]
    structs_c = "\n".join(struct_block(s) for s in sorted(own_structs))
    if a.preamble:
        pre = (f"#include <stddef.h>\n#include <stdint.h>\n#include \"game.h\"\n\n"
               f"#define G(T, addr) ((T *)(uintptr_t)(u32)(addr))\n\n{structs_c}\n{wrappers}\n"
               f"/* ── {a.subsystem}: the subsystem's own code ── */\n")
        open(a.preamble, "w").write(pre)
        out.append("## The start of the file, written by the tool: the includes, the struct types and the generated "
                   "wrappers of the functions it calls outside the subsystem. It comes before your code: do NOT repeat "
                   "any of it, start right after it.\n```c\n" + pre + "```\n")
    else:
        out.append("## Struct types (copy these into the file as they are)\n```c\n" + structs_c + "```\n")
        out.append("## Generated wrappers of the functions it calls outside the subsystem (copy these as they are)\n"
                   "```c\n" + wrappers + "```\n")
    out.append("## The functions\n")
    for n in members:
        s = sigs[n]
        ps, ret = chosen(s)
        sig = ", ".join(f"{p.get('type')} {p.get('name')} ({p.get('reg')})" for p in ps) or "none"
        callers_out = [x for x in ([c["caller"] for c in g["sites"] if c.get("target") == n] +
                                   [e["site_unit"] for e in g["edges"] if e["target"] == n]) if x not in inside]
        e = g["entries"][n]
        out.append(
            f"### {n} ({s['addr']}){' ENTRY POINT' if n in entries else ''}\n"
            + (f"FOLD: {n} ends by popping its caller's frame (`pop {{pc}}` with no push of its own), so it returns to "
               f"its caller's caller. Write its code inside the C function of its caller "
               f"({', '.join(sorted(set(c['caller'] for c in g['sites'] if c.get('target') == n)))}): where it returns, "
               f"the caller returns. It has no C function or wrapper of its own.\n" if n in a.fold else "")
            + 
            f"signature ({s['confidence']}, {s.get('decided_by', '-')}): args {sig}; result "
            f"{ret.get('reg', 'none')} {ret.get('type', '')}\n"
            f"contract: inputs {' '.join(e.get('inputs_final', []))}; outputs {' '.join(e.get('outputs_final', [])) or '-'}; "
            f"preserved {' '.join(e.get('preserved', [])) or '-'}\n"
            + (f"called from outside: {', '.join(sorted(set(callers_out)))}\n" if callers_out else "")
            + "assembly:\n```\n" + "\n".join(asmsrc.asm_of(e.get("src"))) + "\n```\n")
        part = part_of(n)
        if part:
            out.append("today's readable C (register form; same behaviour, use it for names and meaning):\n```c\n" + part + "```\n")
    print("\n".join(out))


PROMPT_HEAD = """You convert one whole subsystem of the Game Boy Advance game Mega Man Battle Network 6 (Falzar),
"{subsystem}" ({n} functions), from its assembly into ONE file of clear, idiomatic C for a source port:
cport/src/game/{file}. Your file is compiled and tested automatically against the original on real
game states (captured calls, random inputs, the CPU oracle, and whole-game replays that compare
video memory every frame), so it must behave exactly the same: the same memory writes in the same
order where it matters, the same results, the same calls to the rest of the game with the same arguments.

## How the file works
- The file starts with these lines, as they are:
  `#include <stddef.h>` / `#include <stdint.h>` / `#include "game.h"` (game.h brings cpu.h, the A_LABEL
  address macros, struct field offsets oStruct_Field and assembler constants) /
  `#define G(T, addr) ((T *)(uintptr_t)(u32)(addr))` (a guest address as a typed pointer: guest memory sits
  at its GBA addresses on the GBA and in the host test build).
- Every function of the subsystem becomes an ordinary `static` C function with real parameters and a real
  return value: the signature given with it (its arguments arrive in those registers in the original).
  Give it a descriptive snake_case name (`start_screen_init`), and put the game's name in its comment
  (`/* startScreen_init_802F668: ... */`): the game's names are already declared elsewhere as
  `Next NAME(void)` (the original's register-level versions), so a static function cannot reuse them.
  Use the struct types given below for the struct pointers (`StartScreen *self`, `self->CurrCursorPos`).
  Get a struct pointer from a guest address with `G(Type, addr)` (e.g. `G(StartScreen, A_eStartScreen)`);
  struct fields that hold guest pointers are u32. Never cast an integer to a pointer any other way
  (the host test build is 64-bit), and pass a struct pointer to a wrapper parameter of type u32 as
  `(u32)(uintptr_t)ptr`.
- Inside the subsystem, functions call each other directly as C functions. A jump table of the
  subsystem's own functions (a `.word fn+1` table indexed by a state byte) becomes a C `switch` or a
  `static` array of function pointers, indexed the same way (state byte values are byte offsets into the
  table: index = offset / 4).
- Only the ENTRY POINTS (listed below; they are called from the rest of the game) also get a thin
  adapter `Next NAME(void)`: read the arguments from the registers (R0..R12), call the typed function,
  write its result (R0 = ...), restore the registers the original preserves (its `push {{r4-r7,lr}}` ...
  `pop {{r4-r7,pc}}`) when the subsystem's code calls other game code in between, and
  `RETURN_TO_CALLER();`. No control flow, no memory access, at most 8 statements. Save registers in
  locals first: `u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;` ... `R4 = r4; R5 = r5; R6 = r6; R7 = r7;`.
  Non-entry functions get NO `Next` adapter.
- Calls out of the subsystem go through the generated wrappers given below, `call_NAME(...)`. Copy them
  into the file unchanged (before the functions) and call them; never use R0..R12 yourself outside the
  wrappers and the adapters. A typed wrapper takes the callee's real arguments. A wrapper marked
  "register form" (the callee's signature is not settled) takes every register the callee's contract may
  read: pass the value the original has in that register at the call (often a value it loaded for its
  own use, or the struct pointer in r5); where the original's register holds something that is no
  longer meaningful at that point, pass what it holds in the original anyway.
- A call through a code address that is not one of the subsystem's own functions: a small static helper
  that sets R0.. as the original, calls `GUEST_CALL_ADDR(address)`, and returns R0, marked with
  `/* register form: call through a code address */`.
- Guest memory that is not one of the struct types: LD8/LD16/LD32(addr), signed loads (ldrsb/ldrsh)
  LDS8/LDS16(addr), stores ST8/ST16/ST32(addr, value) (statements). Addresses are u32. The toolkit (r10
  in the original) is at A_eToolkit: `LD32(A_eToolkit + oToolkit_GameStatePtr)` etc.
- Names: every label of the disassembly is a macro A_LABEL holding its address (A_eStartScreen,
  A_off_802F350; the bare label is not defined), struct field offsets are oStruct_Field, assembler
  constants keep their names. File-scope initializers must be constants (numbers and these macros).
- bn6f macros in the assembly: `movflag FLAG` sets r0 = FLAG >> 8 and r1 = FLAG & 0xFF (an event flag
  split in two bytes), so `movflag EVENT_1704; bl SetEventFlagFromImmediate` is
  `call_SetEventFlagFromImmediate(EVENT_1704 >> 8, EVENT_1704 & 0xFF)`.
- An entry point's contract "outputs" are the registers its callers read after it returns. The adapter
  must leave there what the original leaves: a value the original's own code computes last (e.g. a loop
  pointer left in r3) is returned by the typed function and written by the adapter; one a callee leaves is
  already there when that callee is the last thing the original does before returning.
- Condition flags only matter at the entry points if their contract lists nz/c/v as outputs (none here
  unless stated); inside the subsystem results are ordinary C values.
- Write it the way a careful human porter would: meaningful names (keep the game's function names for the
  static functions, they are in the symbol database), structured control flow, short comments in game
  terms, a one-paragraph comment at the top of the file describing the subsystem. No goto, no copies of
  machine-translated style.

Entry points: {entries}.{extra}

Answer with the complete file in one ```c block, nothing after it.
"""

if __name__ == "__main__":
    main()
