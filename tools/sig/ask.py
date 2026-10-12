#!/usr/bin/env python3
"""The knowledge layer, judgment pass: outside models propose signatures the mechanical pass left
unsure, and the two are compared.

    python3 tools/sig/ask.py run [--models M,...] [--jobs N] [--names F...] [--subsystem S...] [--all]
    python3 tools/sig/ask.py merge            # agreement -> build/sig/final.json

`run` sends functions (in subsystem order, a few per request, with their evidence: the contract's
upper bound and the callers' lower bound, the registers the function's own code reads first, the
captured calls' values, bn6f's comment, today's readable C, the callees' bounds, the assembly) to the
models in turn and stores each answer in build/sig/model/BATCH.json (resumable: done batches are
skipped). Tokens go to build/sig/usage.jsonl.

`merge` keeps a model's signature as `confident` when it agrees with the mechanical pass:
  - its registers lie between the bounds (lower <= model <= upper; r10 is the toolkit global),
    and include every register the function's own code reads before writing;
  - its result lies between the bounds (a result the known callers read is kept; none beyond the
    contract's);
  - its types agree with the captured calls (a pointer where they saw pointers, an integer where
    they saw integers, the bn6f struct they pointed at).
Otherwise `unsure`, with both proposals and the reasons. Mechanically confident signatures stay.
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import hashlib
import json
import os
import re
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "tools", "sig"))
import asm as asmsrc  # noqa: E402
import llm  # noqa: E402

SIG = os.path.join(ROOT, "build/sig/signatures.json")
SUBS = os.path.join(ROOT, "tools/sig/subsystems.json")
GRAPH = os.path.join(ROOT, "build-abi/graph.json")
MODEL_DIR = os.path.join(ROOT, "build/sig/model")
USAGE = os.path.join(ROOT, "build/sig/usage.jsonl")
FINAL = os.path.join(ROOT, "build/sig/final.json")
MAX_FUNCS, MAX_LINES, ASM_CAP = 8, 420, 160

SYSTEM = """You read GBA Thumb assembly from the Mega Man Battle Network 6 disassembly (bn6f) and give each
function a C signature for a readable C port. The port's C calls functions directly, so each argument
is one register the function reads on entry (r0-r12), and the result is r0, a pair r0:r1, a condition
flag (flag:z = the caller tests Z after the call), or none. r10 always holds the global toolkit
(`Toolkit *`), not an argument: leave it out. r5 commonly holds the object the function works on.

You get, per function: the contract's UPPER bound (registers the lifter's whole-program analysis says
may be read on entry; it is conservative: a register a callee merely saves, or a callee reached
through a jump table reads, ends up there), the LOWER bound (what the function itself and the callers
and callees the game was seen to use need), the registers its own instructions read before writing
them (a linear scan), the values the original game passed in real calls (captured), and the callees'
bounds. Decide which registers really are arguments: one the code (or a callee that needs it as an
argument) reads before writing it. Choose C types: u8/u16/u32/s8/s16/s32/bool for integers, a bn6f
struct pointer when the value points at one (struct names below), `const u8 *` for ROM data
(scripts, tables), `u8 *`/`u16 *` for other memory, `void (*)(void)` for code. Name each parameter.

bn6f structs: AIData AIState AIAttackVars BGScrollCBCounters BGScrollCallbacks BattleState Camera
Chatbox CollisionData CutsceneCameraInfo CutsceneState GFXAnimState GameState Joypad LogoScreenState
MapScriptState MapTilesState200be70 NaviStats OWObjectInteractionArea ObjectHeader ObjectSprite
OverworldMapObject OverworldNPCObject OWPlayerObject Palette20097a0 PanelData RenderInfo ReqBBSGui
ScreenFade StartScreen SubMenu Toolkit Warp2011bb0 BattleObject T1BattleObject T4BattleObject
(S2000aa0 S2001c04 S200A290 S200BC50 S200F348 S200ace0 S2011E30 S20364C0 Struct2035280: unnamed RAM
structs at those addresses).

Answer with one ```json block: a list with one object per function, in order:
{"name": "...", "params": [{"reg": "r5", "type": "StartScreen *", "name": "self"}],
 "ret": {"reg": "none|r0|r0:r1|flag:z|flag:c|flag:n", "type": "void|u8|bool|...", "name": "..."},
 "why": "one short sentence: which registers you dropped from the upper bound and why"}"""


def load() -> tuple[dict, dict, list]:
    sigs = json.load(open(SIG))
    g = json.load(open(GRAPH))
    subs = json.load(open(SUBS))["subsystems"]
    return sigs, g, subs


def callees(g: dict) -> dict[str, list[str]]:
    out: dict[str, list[str]] = {}
    for s in g["sites"]:
        if s["kind"] in ("call", "tail") and s.get("target"):
            lst = out.setdefault(s["caller"], [])
            if s["target"] not in lst:
                lst.append(s["target"])
    return out


def block(name: str, s: dict, calls: list[str], sigs: dict, g: dict) -> tuple[str, int]:
    e = g["entries"][name]
    lines = asmsrc.asm_of(e.get("src"))
    shown = lines[:ASM_CAP] + ([f"// ... {len(lines) - ASM_CAP} more lines"] if len(lines) > ASM_CAP else [])
    caps = []
    for p in s["params"]:
        ev = [x for x in p["evidence"] if x.startswith("captures")]
        if ev:
            caps.append(f"{p['reg']}: {ev[0][10:]}")
    for x in s["ret"]["evidence"]:
        if x.startswith("captures at exit"):
            caps.append(x[9:])
    rd = [x for p in s["params"] for x in p["evidence"] if x.startswith("readable")]
    cl = []
    for c in calls[:12]:
        cs = sigs.get(c)
        if cs:
            cl.append(f"{c}: upper {' '.join(cs['upper_args']) or '-'}; lower {' '.join(cs['lower_args']) or '-'}"
                      f"; own {' '.join(cs['own_reads']) or '-'}"
                      + (f"; bn6f {cs['bn6f_comment']}" if cs.get("bn6f_comment") else ""))
    text = (f"### {name} ({s['addr']}, bn6f/asm/{(s['src'] or '?').split(':')[0]})\n"
            f"upper: {' '.join(s['upper_args']) or '-'}; result {s['ret']['from']}\n"
            f"lower: {' '.join(s['lower_args']) or '-'}; result {s.get('lower_ret', '?')}\n"
            f"own code reads first: {' '.join(sorted(asmsrc.own_reads(lines))) or '-'}\n"
            + (f"captured calls ({s['captured_calls']}): " + "; ".join(caps) + "\n" if caps else "captured calls: none\n")
            + (f"bn6f comment: {s['bn6f_comment']}\n" if s.get("bn6f_comment") else "")
            + (f"readable C today: {'; '.join(rd)}\n" if rd else "")
            + (f"callees:\n  " + "\n  ".join(cl) + "\n" if cl else "")
            + "asm:\n" + "\n".join(shown) + "\n")
    return text, len(shown)


def batches(names: list[str], sigs: dict, g: dict) -> list[list[str]]:
    calls = callees(g)
    out, cur, n = [], [], 0
    for name in names:
        _t, k = block(name, sigs[name], calls.get(sigs[name]["unit"], []), sigs, g)
        if cur and (len(cur) >= MAX_FUNCS or n + k > MAX_LINES):
            out.append(cur)
            cur, n = [], 0
        cur.append(name)
        n += k
    if cur:
        out.append(cur)
    return out


def batch_id(names: list[str]) -> str:
    return hashlib.sha1(" ".join(names).encode()).hexdigest()[:12]


def parse(reply: str) -> list[dict] | None:
    m = re.findall(r"```(?:json)?\s*(.*?)```", reply, re.S)
    for text in reversed(m or [reply]):
        try:
            v = json.loads(text)
            return v if isinstance(v, list) else None
        except json.JSONDecodeError:
            continue
    return None


def run(args) -> None:
    sigs, g, subs = load()
    bounds = json.load(open(os.path.join(ROOT, "build/sig/bounds.json")))
    for n, s in sigs.items():
        s["lower_ret"] = bounds.get(n, {}).get("ceiling", {}).get("ret", "?")
    order = [u["unit"] for sub in subs for u in sub["units"]
             if not args.subsystem or sub["name"] in args.subsystem]
    by_unit: dict[str, list[str]] = {}
    for n, s in sigs.items():
        by_unit.setdefault(s["unit"], []).append(n)
    names = [n for u in order for n in sorted(by_unit.get(u, []))]
    if args.names:
        want = set(args.names)
        names = [n for n in names if n in want] + [n for n in args.names if n not in set(names) and n in sigs]
    if not args.all and not args.names:
        names = [n for n in names if sigs[n]["confidence"] == "unsure"]
    if args.redo_mech:
        pass
    todo = batches(names, sigs, g)
    os.makedirs(MODEL_DIR, exist_ok=True)
    models = args.models.split(",")
    calls = callees(g)
    lock = threading.Lock()
    done = [0]

    def one(i: int, names: list[str]) -> None:
        bid = batch_id(names)
        path = os.path.join(MODEL_DIR, bid + ".json")
        if os.path.exists(path):
            return
        prompt = "\n".join(block(n, sigs[n], calls.get(sigs[n]["unit"], []), sigs, g)[0] for n in names)
        for attempt in range(len(models) * 2):
            model = models[(i + attempt) % len(models)]
            t0 = time.time()
            try:
                reply = llm.complete(model, prompt, system=SYSTEM, max_tokens=12000, timeout=600, retries=2,
                                     tag=f"sig {bid}", usage_log=USAGE, wait_out_limits=False,
                                     fields={"function": names[0], "batch": bid, "functions": len(names)})
            except Exception as ex:  # noqa: BLE001
                print(f"[{bid}] {model}: {str(ex)[:120]}", flush=True)
                continue
            got = parse(reply)
            if got is None:
                print(f"[{bid}] {model}: unparseable reply", flush=True)
                continue
            with open(path, "w") as fh:
                json.dump({"model": model, "names": names, "seconds": round(time.time() - t0),
                           "answers": got}, fh, indent=0)
            with lock:
                done[0] += 1
                print(f"[{done[0]}/{len(todo)}] {bid} {model} {len(names)} functions {round(time.time() - t0)}s",
                      flush=True)
            return

    with cf.ThreadPoolExecutor(args.jobs) as ex:
        list(ex.map(lambda p: one(*p), enumerate(todo)))


# ── merge ──

PTR = re.compile(r"\*|\(\*\)")


def agree(s: dict, m: dict) -> list[str]:
    """Reasons a model's signature disagrees with the mechanical pass (none: they agree)."""
    why = []
    regs = [p.get("reg") for p in m.get("params", []) if p.get("reg") != "r10"]
    upper, lower = set(s["upper_args"]), set(s["lower_args"]) - {"r10"}
    if not set(regs) <= upper:
        why.append(f"registers outside the contract: {sorted(set(regs) - upper)}")
    for r in sorted(upper - set(regs) - {"r10"}):
        for c in s.get("callees", []):
            need = CALLEE_ARGS.get(c)
            if need is None or r not in need:
                continue
            w = asmsrc.written_before_call(s.get("_lines", []), c)
            if w is not None and r not in w:
                why.append(f"drops {r}, which it passes unchanged to {c} (an argument there)")
                break
    own = set(s["own_reads"]) - {"r10"}
    if not own <= set(regs):
        why.append(f"drops registers its own code reads first: {sorted(own - set(regs))}")
    ret = (m.get("ret") or {}).get("reg", "none")
    up, lo = s["ret"]["from"], s.get("lower_ret", "none")
    width = {"none": 0, "r0": 1, "r0:r1": 2}
    if ret in width and up in width and lo in width:
        if not width[lo] <= width[ret] <= width[up]:
            why.append(f"result {ret} outside {lo}..{up}")
    elif ret.startswith("flag:") and not (up.startswith("flag:") or lo.startswith("flag:")):
        why.append(f"result {ret}, contract {up}")
    elif up.startswith("flag:") and ret != up:
        why.append(f"result {ret}, contract {up}")
    for p in m.get("params", []):
        mine = next((q for q in s["params"] if q["reg"] == p.get("reg")), None)
        if not mine or not mine["calls"]:
            continue
        t = str(p.get("type", ""))
        cap = next((x for x in mine["evidence"] if x.startswith("captures")), "")
        is_ptr = bool(PTR.search(mine["type"] or "")) and "propagated" not in mine
        if is_ptr and not PTR.search(t) and " or NULL" not in cap:
            why.append(f"{p['reg']}: {t}, captures saw pointers ({mine['type']})")
        if not is_ptr and PTR.search(t) and mine["type"] not in (None, "u32") and "mixed" not in cap:
            why.append(f"{p['reg']}: {t}, captures saw integers ({mine['type']})")
        if mine.get("struct") and mine["struct"] not in t and is_ptr and "field" not in cap:
            why.append(f"{p['reg']}: {t}, captures pointed at {mine['struct']}")
    return why


CALLEE_ARGS: dict[str, set[str]] = {}


def merge(_args) -> None:
    sigs = json.load(open(SIG))
    g = json.load(open(GRAPH))
    calls = callees(g)
    bounds = json.load(open(os.path.join(ROOT, "build/sig/bounds.json")))
    answers: dict[str, tuple[str, dict]] = {}
    for f in sorted(os.listdir(MODEL_DIR)) if os.path.isdir(MODEL_DIR) else []:
        d = json.load(open(os.path.join(MODEL_DIR, f)))
        for a in d["answers"]:
            if isinstance(a, dict) and a.get("name") in d["names"]:
                answers[a["name"]] = (d["model"], a)
    # what each callee takes: the agreed model signature, else the mechanical one when confident
    for n, s in sigs.items():
        if n in answers and s["confidence"] == "unsure":
            CALLEE_ARGS[n] = {p.get("reg") for p in answers[n][1].get("params", [])}
        elif s["confidence"] == "confident":
            CALLEE_ARGS[n] = set(s["upper_args"])
    out = {}
    stats: dict[str, int] = {}
    for n, s in sigs.items():
        s["callees"] = calls.get(s["unit"], [])
        s["_lines"] = asmsrc.asm_of(g["entries"][n].get("src")) if n in answers else []
        s["lower_ret"] = bounds.get(n, {}).get("ceiling", {}).get("ret", "none")
        rec = {k: v for k, v in s.items() if k not in ("_lines", "callees")}
        if n in answers and s["confidence"] == "unsure":
            model, a = answers[n]
            why = agree(s, a)
            rec["model"] = {"by": model, "params": a.get("params", []), "ret": a.get("ret"), "why": a.get("why", "")}
            rec["disagreements"] = why
            rec["confidence"] = "confident" if not why else "unsure"
            rec["decided_by"] = "model+mechanical" if not why else "-"
        elif s["confidence"] == "confident":
            rec["decided_by"] = "mechanical"
        stats[rec["confidence"]] = stats.get(rec["confidence"], 0) + 1
        out[n] = rec
    json.dump(out, open(FINAL, "w"), indent=0)
    print(f"{len(out)} signatures: {stats}; model answers {len(answers)}")


def main() -> None:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("--models", default="hyper/glm-5.3-flash,minimax/MiniMax-M3,hyper/qwen3.8-flash")
    r.add_argument("--jobs", type=int, default=6)
    r.add_argument("--names", nargs="*")
    r.add_argument("--subsystem", nargs="*")
    r.add_argument("--all", action="store_true", help="also functions the mechanical pass settled")
    r.add_argument("--redo-mech", action="store_true")
    sub.add_parser("merge")
    a = ap.parse_args()
    run(a) if a.cmd == "run" else merge(a)


if __name__ == "__main__":
    main()
