#!/usr/bin/env python3
"""The knowledge layer, mechanical pass: a proposed C signature for every function.

    python3 tools/sig/derive.py [--graph build-abi/graph.json] [--out build/sig/signatures.json]

Sources, per entry of the lifter's ABI export (graph.json; lifted units, readable or not):
  contract   the refined contract and proposed args/ret/globals/keep (`proposal_final`, the export's
             final step: edges, BIOS calls, per-site liveness, saves pass through, flag results);
  captures   the original's captured calls (build-snapshots/NAME, matched by entry PC): each
             argument register's values at entry and r0 at exit, classified (small int, signed,
             a pointer into a region, the bn6f struct instance it points to, the ROM label);
  structs    bn6f's include/structs via tools/abi/gen_structs.py: RAM instances and sizes;
  readable   typed readable functions: a `Next NAME(void)` adapter that calls a typed C function
             with argument registers (cport/src/game) gives types and names;
  callers    propagation: an argument with no captures takes the class a caller passed in the
             same register when that caller holds it unchanged from its own entry (its own arg).
Confidence: `confident` = arguments and return known from the contract, every argument typed by
captures (3+ calls) or a readable typed adapter, return typed (void, a flag, or r0 seen at exit or
typed by readable C), no unwinding; `none` = the function unwinds or has no contract; else `unsure`.
Writes JSON {name: signature}. tools/sig/write_db.py puts them into symbols/db.
"""
from __future__ import annotations

import argparse
import bisect
import collections
import glob
import json
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "tools", "abi"))
import gamepaths  # noqa: E402
import gen_structs  # noqa: E402
sys.path.insert(0, os.path.join(ROOT, "tools", "sig"))
import asm as asmsrc  # noqa: E402
import names as symnames  # noqa: E402

HDR = struct.Struct("<19I")
MAGIC = 0x53364E42
REGIONS = [(0x02000000, 0x02040000, "ewram"), (0x03000000, 0x03008000, "iwram"), (0x04000000, 0x04000400, "io"),
           (0x05000000, 0x05000400, "palette"), (0x06000000, 0x06018000, "vram"), (0x07000000, 0x07000400, "oam"),
           (0x08000000, 0x0A000000, "rom"), (0x0E000000, 0x0E010000, "sram")]
REGION_TYPE = {"io": "volatile u16 *", "palette": "u16 *", "vram": "u16 *", "oam": "u16 *", "sram": "u8 *"}
MAX_SNAPS = 64


def region(v: int) -> str | None:
    for lo, hi, n in REGIONS:
        if lo <= v < hi:
            return n
    return None


class Symbols:
    """bn6f.elf's labels by address (data and code), for naming what a pointer points into."""

    def __init__(self) -> None:
        nm = subprocess.run(["arm-none-eabi-nm", "-n", gamepaths.ORIG_ELF],
                            capture_output=True, text=True, check=True).stdout
        rows = []
        for line in nm.splitlines():
            p = line.split()
            if len(p) == 3 and p[1] in "tTdDbBrRNnaA" and not p[2].startswith(("$", ".")) \
                    and (p[1] not in "NnaA" or 0x02000000 <= int(p[0], 16) < 0x03008000):
                rows.append((int(p[0], 16) & ~1, p[2], p[1] in "tT"))
        rows.sort()
        self.addr = [r[0] for r in rows]
        self.rows = rows

    def label(self, v: int) -> tuple[str, int, bool] | None:
        i = bisect.bisect_right(self.addr, v & ~1) - 1
        if i < 0:
            return None
        a, n, code = self.rows[i]
        return n, v - a, code


class Instances:
    def __init__(self) -> None:
        _text, rep = gen_structs.generate(os.path.join(ROOT, "bn6f"))
        size = {s["struct"]: s.get("size") for s in rep["structs"]}
        for s in rep["structs"]:
            if s.get("alias_of"):
                size[s["struct"]] = size.get(s["alias_of"])
        rows = []
        for i in rep["instances"]:
            sz = size.get(i["type"])
            if sz:
                rows.append((int(i["address"], 16), sz, i["type"], i["instance"]))
        rows.sort()
        self.rows = rows
        self.addr = [r[0] for r in rows]

    def find(self, v: int) -> tuple[str, str, int] | None:
        i = bisect.bisect_right(self.addr, v) - 1
        if i >= 0:
            a, sz, t, n = self.rows[i]
            if v < a + sz:
                return t, n, v - a
        return None


def read_captures(entries_by_addr: dict[int, str]) -> dict[str, dict]:
    """entry name -> {calls, entry: [r0..r14 per call], exit: [(r0, r1, cpsr)]}."""
    out: dict[str, dict] = {}
    store = os.path.join(ROOT, "build-snapshots")
    for d in sorted(os.listdir(store)):
        path = os.path.realpath(os.path.join(store, d))
        if not os.path.isdir(path):
            continue
        files = sorted(f for f in os.listdir(path) if f.startswith("snap_") and f.endswith(".bin")
                       and not f.endswith(".exit.bin"))[:MAX_SNAPS]
        ent, ext = [], []
        name = None
        for f in files:
            with open(os.path.join(path, f), "rb") as fh:
                raw = fh.read(HDR.size)
            if len(raw) != HDR.size:
                continue
            h = HDR.unpack(raw)
            if h[0] != MAGIC:
                continue
            name = name or entries_by_addr.get(h[17] & ~1)
            ent.append(h[2:17])
            xp = os.path.join(path, f[:-4] + ".exit.bin")
            if os.path.exists(xp):
                with open(xp, "rb") as fh:
                    x = fh.read(HDR.size)
                if len(x) == HDR.size:
                    xh = HDR.unpack(x)
                    ext.append((xh[2], xh[3], xh[18]))
        if name and ent and name not in out:
            out[name] = {"calls": len(ent), "entry": ent, "exit": ext}
    return out


def classify(vals: list[int], syms: Symbols, inst: Instances) -> dict:
    """What a register's captured values say about its type."""
    vals = [v & 0xFFFFFFFF for v in vals]
    distinct = sorted(set(vals))
    sample = [f"{v:#x}" for v in distinct[:4]]
    nonzero = [v for v in distinct if v]
    regs = {region(v) for v in nonzero}
    out: dict = {"n": len(vals), "distinct": len(distinct), "sample": sample}
    if nonzero and None not in regs and len(regs) == 1:
        reg = regs.pop()
        out["nullable"] = 0 in distinct
        if reg == "rom":
            labs = collections.Counter()
            code = True
            for v in nonzero:
                lab = syms.label(v)
                if lab:
                    labs[lab[0] if lab[1] == 0 else f"{lab[0]}+{lab[1]:#x}"] += 1
                    code = code and lab[2] and lab[1] in (0, 1) and v & 1 == 1
                else:
                    code = False
            if code:
                out.update(kind="code", type="void (*)(void)", into=sorted(labs)[:4])
            else:
                out.update(kind="rom", type="const u8 *", into=[k for k, _ in labs.most_common(4)])
            return out
        if reg in ("ewram", "iwram"):
            hits = [inst.find(v) for v in nonzero]
            if all(hits):
                types = {h[0] for h in hits}
                offs = {h[2] for h in hits}
                names = sorted({h[1] for h in hits})
                if len(types) == 1 and offs == {0}:
                    out.update(kind="struct", type=f"{types.pop()} *", into=names[:4])
                    return out
                if len(types) == 1 and len(offs) == 1:
                    out.update(kind="field", type="u8 *", into=[f"{n}+{next(iter(offs)):#x}" for n in names[:4]],
                               struct=next(iter(types)))
                    return out
            labs = collections.Counter()
            for v in nonzero:
                lab = syms.label(v)
                labs[(lab[0], lab[1]) if lab else ("?", v)] += 1
            bases = {k[0] for k in labs}
            into = [f"{n}+{o:#x}" if o else n for (n, o), _ in labs.most_common(4)]
            if len(bases) == 1 and len(nonzero) > 2:
                offs = sorted(o for _n, o in labs)
                diffs = [b - a for a, b in zip(offs, offs[1:]) if b != a]
                g = 0
                for d in diffs:
                    g = d if not g else __import__("math").gcd(g, d)
                if g >= 8:
                    out["stride"] = f"{g:#x}"
            out.update(kind="ram", type="u8 *", into=into, region=reg)
            return out
        out.update(kind=reg, type=REGION_TYPE.get(reg, "u8 *"))
        return out
    if None in regs and len(regs) > 1:
        out.update(kind="mixed", type="u32")
        return out
    hi = max(distinct) if distinct else 0
    if distinct and all(v >= 0xFFFF8000 or v < 0x8000 for v in distinct) and any(v >= 0xFFFF8000 for v in distinct):
        out.update(kind="int", type="s16" if all(v >= 0xFFFF8000 or v < 0x8000 for v in distinct) else "s32")
    elif hi <= 1:
        out.update(kind="int", type="bool" if len(distinct) == 2 else "u32", note="only 0/1 seen" if hi <= 1 else "")
    elif hi < 0x100:
        out.update(kind="int", type="u8")
    elif hi < 0x10000:
        out.update(kind="int", type="u16")
    else:
        out.update(kind="int", type="u32")
    return out


ARG_RE = re.compile(r"^\(?\s*(?:\([\w ]+\*?\)\s*)?(R\d+)\s*\)?$")


def readable_signatures() -> dict[str, dict]:
    """NAME -> {fn, params: {reg: (type, name)}, ret: (type, reg)} from thin typed adapters."""
    out = {}
    for p in sorted(glob.glob(os.path.join(ROOT, "cport/src/game/*.c"))):
        text = open(p).read()
        protos = {}
        for m in re.finditer(r"^(?:static\s+)?(?:inline\s+)?([\w\s\*]+?)\s*\b(\w+)\(([^)]*)\)\s*\{", text, re.M):
            ret, fn, params = m.group(1).strip(), m.group(2), m.group(3).strip()
            if ret.endswith("Next") or fn in ("if", "while", "for", "switch"):
                continue
            ps = []
            if params and params != "void":
                for q in params.split(","):
                    q = q.strip()
                    mm = re.match(r"(.*?)(\w+)$", q)
                    if mm:
                        ps.append((mm.group(1).strip(), mm.group(2)))
            protos[fn] = (re.sub(r"\bstatic\b|\binline\b", "", ret).strip(), ps)
        for m in re.finditer(r"^Next (\w+)\(void\) \{\n(.*?)^\}", text, re.M | re.S):
            name, body = m.group(1), m.group(2)
            stmts = [s.strip() for s in body.strip().splitlines() if s.strip() and not s.strip().startswith(("/*", "//", "*"))]
            if not stmts or stmts[-1] != "RETURN_TO_CALLER();" or len(stmts) > 4:
                continue
            for s in stmts[:-1]:
                c = re.match(r"(?:(R\d+)\s*=\s*(?:\([\w ]+\)\s*)?)?(\w+)\((.*)\);$", s)
                if not c or c.group(2) not in protos:
                    continue
                rtype, ps = protos[c.group(2)]
                args = [a.strip() for a in c.group(3).split(",")] if c.group(3).strip() else []
                if len(args) != len(ps):
                    continue
                params = {}
                for a, (t, n) in zip(args, ps):
                    am = ARG_RE.match(a)
                    if am:
                        params[am.group(1).lower()] = (t, n)
                out[name] = {"fn": c.group(2), "file": os.path.relpath(p, ROOT), "params": params,
                             "ret": (rtype, (c.group(1) or "").lower() or None)}
                break
    return out


def own_args_unchanged(g: dict) -> dict[tuple[str, str], set[str]]:
    """(caller unit, callee) -> argument registers the caller passes unchanged from its entry.
    Approximation: the register is an input of the caller and the caller's contract preserves it
    (it saves and restores it, so its body treats it as a value it holds), and it is the callee's
    input too. Used only to propagate a class to callees with no captures; marked as such."""
    out = collections.defaultdict(set)
    ents = g["entries"]
    for s in g["sites"]:
        if s["kind"] not in ("call", "tail"):
            continue
        a, b = ents.get(s["caller"]), ents.get(s.get("target", ""))
        if not a or not b:
            continue
        shared = set(a.get("inputs_final", [])) & set(b.get("inputs_final", [])) & {"r4", "r5", "r6", "r7"}
        shared &= set(a.get("preserved", []))
        if shared:
            out[(s["caller"], s["target"])] |= shared
    return out


def bn6f_comment(lines: list[str], name: str) -> str | None:
    for x in lines[:3]:
        m = re.match(rf"{re.escape(name)}:\s*//\s*(\(.*\)\s*->.*)", x)
        if m:
            return m.group(1).strip()
    return None


def narrow(g: dict, bounds: dict) -> dict[str, set[str] | None]:
    """Each entry's arguments by liveness over its own assembly (tools/sig/asm.py live_in), a call
    reading what its callee needs by the same measure, to a fixpoint over the call graph (growing from
    nothing, so every register in the result is read on some path). Bounded by the contract's upper
    bound. None where the assembly leaves through a computed jump or calls through a pointer the
    routes never showed a target for: the contract stays."""
    ents = g["entries"]
    lines = {n: asmsrc.asm_of(e.get("src")) for n, e in ents.items()}
    unobserved = set()
    targets: dict[str, set[str]] = collections.defaultdict(set)
    for site in g["sites"]:
        if site["kind"] == "call" and site.get("target") in ents:
            targets[site["caller"]].add(site["target"])   # constant code addresses the lifter resolved
        if site["kind"] == "indirect":
            if not site.get("observed"):
                unobserved.add(site["caller"])
            for t in site.get("observed") or []:
                if t in ents:
                    targets[site["caller"]].add(t)
                elif t != "return":
                    unobserved.add(site["caller"])     # a target that is no entry: unknown here
    upper = {n: set(bounds.get(n, {}).get("final", {}).get("args", [])) | ({"r10"} if "r10" in ents[n].get("inputs_final", []) else set())
             for n in ents}
    # what a call leaves changed: may-write without the registers the callee restores
    writes = {n: set(e.get("may_write_final") or e.get("may_write") or []) - set(e.get("preserved") or [])
              for n, e in ents.items()}
    T: dict[str, set[str] | None] = {n: set() for n in ents}
    dead = {n for n in ents if ents[n]["unit"] in unobserved}
    for n in dead:
        T[n] = None
    tainted: set[str] = set()     # used the contract's bound of a callee liveness could not narrow
    for _round in range(30):
        changed = 0
        for n, e in ents.items():
            if T[n] is None:
                continue
            unit = e["unit"]

            def args_of(c, unit=unit):
                if c == "<indirect>":
                    if not targets[unit]:
                        hit.add(1)
                        return {f"r{i}" for i in range(13)}
                    out = set()
                    for t in targets[unit]:
                        if T[t] is None or t in tainted:
                            hit.add(1)
                        out |= (T[t] if T[t] is not None else upper[t])
                    return out
                if c in T:
                    if T[c] is None or c in tainted:
                        hit.add(1)
                    return T[c] if T[c] is not None else upper[c]
                hit.add(1)
                return {f"r{i}" for i in range(13)}       # unknown code: everything

            def writes_of(c, unit=unit):
                if c == "<indirect>":
                    out = set()
                    for t in targets[unit]:
                        out |= writes.get(t, set())
                    return out
                return writes.get(c, {f"r{i}" for i in range(13)})

            hit: set[int] = set()
            got = asmsrc.live_in(lines[n], args_of, writes_of)
            if hit and n not in tainted:
                tainted.add(n)
                changed += 1
            new = None if got is None else (got & upper[n])
            if new != T[n]:
                if new is None or T[n] is None or not new <= T[n]:
                    T[n] = new if new is None or T[n] is None else (T[n] | new)
                    changed += 1
        if not changed:
            break
    return T, tainted


def derive(g: dict, bounds: dict) -> dict[str, dict]:
    syms, inst = Symbols(), Instances()
    by_addr = {int(e["addr"], 16) & ~1: n for n, e in g["entries"].items()}
    caps = read_captures(by_addr)
    readable = symnames.ByName(readable_signatures())      # the port's adapters are named today's way
    passes = own_args_unchanged(g)
    T, tainted = narrow(g, bounds)
    sigs: dict[str, dict] = {}
    callee_of: dict[str, set[str]] = collections.defaultdict(set)
    for site in g["sites"]:
        if site["kind"] in ("call", "tail") and site.get("target"):
            callee_of[site["caller"]].add(site["target"])
    ents = g["entries"]
    for name, e in g["entries"].items():
        p = bounds.get(name, {}).get("final") or e.get("proposal_final") or {}
        low = bounds.get(name, {}).get("ceiling") or {}
        lines = asmsrc.asm_of(e.get("src"))
        own = asmsrc.own_reads(lines)
        comment = bn6f_comment(lines, name)
        cap = caps.get(name)
        rd = readable.get(name)
        params = []
        narrowed = T.get(name)
        partial = None
        if name in tainted and narrowed is not None:
            partial, narrowed = narrowed, None
        arg_regs = p.get("args", []) if narrowed is None else [r for r in p.get("args", []) if r in narrowed]
        for i, reg in enumerate(arg_regs):
            ev = []
            c = None
            if cap:
                idx = int(reg[1:])
                c = classify([call[idx] for call in cap["entry"]], syms, inst)
                ev.append(f"captures: {c['n']} calls, {c['kind']} {', '.join(c.get('into') or c['sample'])}"
                          + (f" (stride {c['stride']})" if c.get("stride") else "")
                          + (" or NULL" if c.get("nullable") else ""))
            ctype, pname = (c or {}).get("type"), None
            if rd and reg in rd["params"]:
                t, pname = rd["params"][reg]
                ev.append(f"readable: {rd['fn']}({t} {pname}) in {rd['file']}")
                if not ctype or (ctype in ("u32", "u8 *") and t not in ("u32",)):
                    ctype = t
            if reg == "r10":
                ctype, pname = "Toolkit *", "toolkit"
            params.append({"reg": reg, "type": ctype, "name": pname, "points_to": (c or {}).get("into") if c and c["kind"] in ("struct", "field", "ram", "rom", "code") else None,
                           "struct": (c["type"][:-2] if c and c["kind"] == "struct" else (c or {}).get("struct")),
                           "calls": (c or {}).get("n", 0), "evidence": ev})
        ret_spec = p.get("ret", "none")
        ret = {"from": ret_spec, "type": None, "evidence": []}
        if ret_spec == "none":
            ret["type"] = "void"
        elif ret_spec.startswith("flag:"):
            ret["type"] = "bool"
            ret["evidence"].append(f"contract: callers test {ret_spec}")
        elif ret_spec in ("r0", "r0:r1"):
            if cap and cap["exit"]:
                c = classify([x[0] for x in cap["exit"]], syms, inst)
                ret["type"] = c["type"] if ret_spec == "r0" else None
                ret["evidence"].append(f"captures at exit: r0 {c['kind']} {', '.join(c.get('into') or c['sample'])}")
            if rd and rd["ret"][1] == "r0" and rd["ret"][0] not in ("void",):
                ret["type"] = ret["type"] if ret["type"] and ret["type"].endswith("*") else rd["ret"][0]
                ret["evidence"].append(f"readable: {rd['fn']} returns {rd['ret'][0]}")
            if ret_spec == "r0:r1":
                ret["type"] = "u64 (r0, r1)"
        sigs[name] = {"addr": e["addr"], "unit": e["unit"], "src": e.get("src"), "readable": bool(e.get("readable")),
                      "params": params, "ret": ret, "globals": p.get("globals", []), "keep": p.get("keep", []),
                      "flags_out": p.get("flags_out", []), "flags_in": p.get("flags_in", []),
                      "blockers": p.get("blockers", []), "unwinds": bool(e.get("may_unwind")) and not any(ents.get(c, {}).get("may_unwind") for c in callee_of[e["unit"]]),
                      "callee_may_unwind": any(ents.get(c, {}).get("may_unwind") for c in callee_of[e["unit"]]),
                      "captured_calls": cap["calls"] if cap else 0,
                      "upper_args": p.get("args", []), "lower_args": low.get("args", []),
                      "own_reads": sorted(r for r in own if r in p.get("args", [])),
                      "narrow_args": None if narrowed is None else sorted(r for r in narrowed if r != "r10"),
                      "narrow_partial": None if partial is None else sorted(r for r in partial if r != "r10"),
                      "bn6f_comment": comment, "asm_lines": len(lines)}
    # callers: propagate a class to callee args without captures
    for (caller, callee), regs in passes.items():
        s, c = sigs.get(callee), sigs.get(caller)
        if not s or not c:
            continue
        for prm in s["params"]:
            if prm["reg"] in regs and not prm["type"]:
                src = next((q for q in c["params"] if q["reg"] == prm["reg"] and q["type"] and q["calls"]), None)
                if src:
                    prm["type"], prm["struct"], prm["points_to"] = src["type"], src["struct"], src["points_to"]
                    prm["evidence"].append(f"callers: {caller} passes its own {prm['reg']} ({src['type']}) unchanged")
                    prm["propagated"] = True
    for name, s in sigs.items():
        s["c"] = c_proto(name, s)
        s["confidence"] = confidence(s)
    return sigs


def confidence(s: dict) -> str:
    if s["unwinds"]:
        return "none"
    if s["narrow_args"] is None:
        return "unsure"          # a computed jump or an unseen pointer call: the contract's bound stands
    for p in s["params"]:
        if not p["type"]:
            return "unsure"
        if p["reg"] != "r10" and p["calls"] < 3 and not any(e.startswith("readable") for e in p["evidence"]):
            return "unsure"
        if p["type"] == "u32" and not any(e.startswith("readable") for e in p["evidence"]) and "mixed" in " ".join(p["evidence"]):
            return "unsure"
    if not s["ret"]["type"]:
        return "unsure"
    return "confident"


def c_proto(name: str, s: dict) -> str:
    ps = []
    for i, p in enumerate(s["params"]):
        t = p["type"] or "u32 /*?*/"
        n = p["name"] or ("toolkit" if p["reg"] == "r10" else f"{p['reg']}")
        ps.append(f"{t}{'' if t.endswith('*') else ' '}{n}")
    rt = s["ret"]["type"] or "u32 /*?*/"
    if rt.startswith("u64"):
        rt = "u64"
    return f"{rt} {name}({', '.join(ps) or 'void'})"


def main() -> None:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("--graph", default=os.path.join(ROOT, "build-abi/graph.json"))
    ap.add_argument("--out", default=os.path.join(ROOT, "build/sig/signatures.json"))
    ap.add_argument("--bounds", default=os.path.join(ROOT, "build/sig/bounds.json"), help="tools/sig/bounds.py")
    a = ap.parse_args()
    g = json.load(open(a.graph))
    bounds = symnames.ByName(json.load(open(a.bounds)))     # keyed by unit name: any name of it finds it
    sigs = derive(g, bounds)
    json.dump(sigs, open(a.out, "w"), indent=0)
    c = collections.Counter(s["confidence"] for s in sigs.values())
    print(f"{len(sigs)} signatures: {dict(c)}; with captures {sum(1 for s in sigs.values() if s['captured_calls'])}")


if __name__ == "__main__":
    main()
