#!/usr/bin/env python3
"""Convert lifted functions to readable C with outside models, keeping only
versions that behave exactly like the original game.

  tools/convert/farm.py --snaps DIR [--models P/M,...] [--rounds N] [--jobs N] FUNC...

For each FUNC a model gets the original assembly, the lifted C, the
function's register contract and the project's conventions, and writes a
readable C version. The candidate is compiled into a shared library and
checked by cport/build-host/difftest (candidate mode) against the lifted
function, which the lockstep tests tie to the original:
  - on the original's own calls captured by tools/convert/capture.sh (DIR/FUNC),
  - on random inputs.
Failures go back to the model for up to --rounds repairs. With several
models, each gets its own attempt. Passing versions are written to
tools/convert/accepted/FUNC.MODEL.c; every attempt is logged to
tools/convert/log/FUNC.MODEL.jsonl.
"""
import argparse, glob, json, os, re, subprocess, sys, tempfile, threading, time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import llm  # noqa: E402

HOST = os.path.join(ROOT, "cport")
DIFFTEST = os.path.join(HOST, "build-host", "difftest")
CFLAGS = ["-O1", "-fPIC", "-shared", "-DTARGET_FLAT", "-DDIFFTEST", f"-I{HOST}/include", f"-I{HOST}/host",
          "-fno-strict-aliasing", "-Werror=implicit-function-declaration", "-Werror=return-type",  # no -w: it silenced these too
          "-Werror=int-conversion", "-Werror=incompatible-pointer-types"]

SYSTEM = (
    "You convert functions of the Game Boy Advance game Mega Man Battle Network 6 from machine-"
    "translated C into clear, idiomatic, readable C for a source port. Your version is compiled and "
    "tested automatically against the original on real game states, so it must behave exactly the same: "
    "same memory writes, same results in the same registers. Write it the way a careful human porter "
    "would: meaningful names, structured control flow, short comments that say what the code does in "
    "game terms.")

CONVENTIONS = r"""
## Conventions of the readable port (cport/include/game.h, cpu.h)
- A function is `Next NAME(void)`. Guest registers are the u32 lvalues R0..R12, SP and LR.
  Arguments arrive in registers (see "inputs"); results are left in the registers listed under
  "outputs". Return with `RETURN_TO_CALLER();` and do not change LR.
- Guest memory (GBA addresses as u32): LD8/LD16/LD32(addr) and ST8/ST16/ST32(addr, value);
  signed loads as (s8)LD8(a), (s16)LD16(a). Types: u8 u16 u32 s8 s16 s32.
- The game's struct field offsets and RAM symbols are macros named as in the disassembly
  (e.g. oBattleObject_Alliance, oToolkit_BattleStatePtr); use them instead of numbers where the
  assembly uses them.
- Calling another function of the game: set R0..R3 (and any other register it reads), then
  `GUEST_CALL(function_name);` and read its results from R0... Every game function is available
  by its disassembly name. BIOS calls (`svc N`): set R0.. and call `swi_call(N);`.
- Condition flags: if "outputs" lists nz, c or v, callers branch on them right after the call.
  Leave them as the original's last flag-setting instruction does: `SET_FLAGS_NZ(x)` (after
  MOV/AND/TST/... with result x), `SET_FLAGS_CMP(a, b)` (after CMP a, b or SUB/SUBS a - b),
  `SET_FLAGS_ADD(a, b)` (after ADDS/CMN). If the last flag-setting instruction differs per
  path, set them on each path.
- Registers listed under "clobbers" are free scratch for you. Every other register must end
  exactly as the original leaves it; registers the original never writes must stay unchanged.
- The original's push/pop of saved registers need not be reproduced (the stack below SP is not
  compared). If it reserves stack space whose address it passes to a callee (e.g. `sub sp, #N;
  add r7, sp, #M`), reserve it on the guest stack too: `u32 buf = SP -= N; ... SP += N;`.
- Write readable C: named local variables, if/else and loops instead of goto where possible,
  no copies of the machine-translated style (no nz/c/v locals, no SUBS()/COND_ macros, no
  L_xxxxxxxx labels, no SAVE/LOAD/CALL_DIRECT).

## Example (another function of the game, already converted)
Original assembly of sub_30068E8, which queues one OAM entry:
```
sub_30068E8:  ldr r7, =byte_3001950; ldr r6, [r7]; cmp r6, #0x80; bge locret ...
```
Readable version:
```c
/* sub_30068E8(r0 = attr01, r1 = attr2, r2 = layer, r3 = list position):
 * queue a single OAM entry; r0 = 0xFF if it was queued. */
Next sub_30068E8(void) {
    if ((s32)LD32(SPRITE_COUNT) < POOL_SLOTS && link_entry(R0, R1, R2, (s32)R3))
        R0 = 0xFF;
    RETURN_TO_CALLER();
}
```
"""

TEMPLATE = """{conventions}
## Function to convert: {name} (at {addr:#010x})
Register contract (from the lifter's analysis of every caller):
{contract}

Original assembly (bn6f disassembly):
```
{asm}
```

Machine-translated C currently used (exact, but unreadable):
```c
{lifted}
```

Reply with one ```c code block containing the complete readable `Next {name}(void)` (plus any
static helper functions, enums or #defines it needs). No #include lines, nothing outside the block.
"""

REPAIR = """Your version failed the automatic comparison against the original:
```
{failure}
```
Your code was:
```c
{code}
```
Find the difference from the original's behaviour and reply with the corrected complete code in
one ```c block. Remember: same memory writes, same output registers, flags as listed in "outputs",
untouched registers unchanged."""


def find_asm(name):
    for path in glob.glob(os.path.join(ROOT, "bn6f", "asm", "*.s")):
        text = open(path).read()
        m = re.search(rf"^{re.escape(name)}::?\s*(?://.*)?$", text, re.M)   # labels may carry `// () -> u32?`
        if not m:
            continue
        end = re.search(rf"^\s*(?:thumb|arm)_func_end {re.escape(name)}\b.*$", text[m.start():], re.M)
        body = text[m.start(): m.start() + (end.end() if end else 3000)]
        return body.strip()
    return None


def find_lifted(name):
    for path in glob.glob(os.path.join(HOST, "src", "lifted", "*.c")):
        text = open(path).read()
        m = re.search(rf"^/\* {re.escape(name)}: ([0-9A-F]{{8}})-", text, re.M)
        if not m:
            continue
        end = text.find("#undef LOAD", m.start())
        return text[m.start(): end + len("#undef LOAD")], int(m.group(1), 16)
    return None, None


def contract_of(lifted):
    fields = {}
    for key in ("inputs", "outputs", "clobbers", "preserved"):
        m = re.search(rf"\*\s+{key}[^:]*:\s*([^\n*]*)", lifted)
        fields[key] = (m.group(1).split() if m else [])
        fields[key] = [x for x in fields[key] if x != "-"]
    return fields


def compare_args(contract):
    regs = [f"r{i}" for i in range(13) if f"r{i}" not in contract["clobbers"]]
    flags = [f for f in ("nz", "c", "v") if f in contract["outputs"]]
    return regs, flags


def extract_code(reply):
    blocks = re.findall(r"```(?:c|C)?\s*\n(.*?)```", reply, re.S)
    if not blocks:
        return None
    code = max(blocks, key=len)
    return "\n".join(l for l in code.splitlines() if not l.strip().startswith("#include"))


LIFTED_STYLE = re.compile(r"\b(nz|SUBS|ADDS|COND_\w+|CALL_DIRECT|CALL_LEAF|SAVE|LOAD)\b|L_[0-9A-F]{8}")


def test(name, code, snaps, contract, workdir):
    """(ok, report): compile the candidate and run the comparisons."""
    if not re.search(rf"\bNext\s+{re.escape(name)}\s*\(\s*void\s*\)", code):
        return False, f"the code must define `Next {name}(void)`"
    # comments may mention the flags ("sets nz"); only code counts
    bare = re.sub(r"/\*.*?\*/|//[^\n]*", " ", code, flags=re.S)
    style = {m.group(0) for m in LIFTED_STYLE.finditer(bare)}
    if style:
        return False, "the code still uses machine-translated style: " + ", ".join(sorted(style))[:200]
    src = os.path.join(workdir, "cand.c")
    so = os.path.join(workdir, "cand.so")
    renamed = re.sub(rf"\bNext\s+{re.escape(name)}\s*\(\s*void\s*\)", f"Next cand_{name}(void)", code)
    with open(src, "w") as f:
        f.write('#include "game.h"\n#include "runtime.h"\n' + renamed + "\n")
    cc = subprocess.run(["gcc", *CFLAGS, "-o", so, src], capture_output=True, text=True)
    if cc.returncode:
        errs = [l for l in cc.stderr.splitlines() if "error" in l][:8]
        return False, "compile errors:\n" + "\n".join(e.replace(src + ":", "line ") for e in errs)
    regs, flags = compare_args(contract)
    base = [DIFFTEST, "-L", so, "-R", ",".join(regs)] + (["-F", ",".join(flags)] if flags else [])
    env = dict(os.environ, BN6F_ROM=os.path.join(ROOT, "bn6f", "bn6f.gba"))
    reports = []
    ok = True
    for extra in (["-S", os.path.join(snaps, name)], ["-n", "300"]):
        r = subprocess.run(base + extra + [name], capture_output=True, text=True, cwd=HOST, env=env, timeout=900)
        line = (r.stdout.strip().splitlines() or [""])[-1]
        diag = "\n".join(r.stderr.strip().splitlines()[:12])
        reports.append(f"{'captured calls' if extra[0] == '-S' else 'random inputs'}: {line}\n{diag}".strip())
        if r.returncode != 0 or " ok " not in f" {line} ":
            ok = False
            break
        m = re.search(r"\((\d+) snapshots, (\d+) runs, \d+ failures, (\d+) out-of-domain\)", line)
        if m and int(m.group(2)) == int(m.group(3)):
            ok = False
            reports.append("every captured call was out of domain: nothing was compared")
            break
    return ok, "\n".join(reports)


lock = threading.Lock()


def convert(name, model, args):
    lifted, addr = find_lifted(name)
    asm = find_asm(name)
    if not lifted or not asm:
        return name, model, False, "no lifted code or assembly found"
    if not os.path.isdir(os.path.join(args.snaps, name)):
        return name, model, False, "no captured calls"
    contract = contract_of(lifted)
    ctext = "\n".join(f"  {k}: {' '.join(v) or '-'}" for k, v in contract.items())
    prompt = TEMPLATE.format(conventions=CONVENTIONS, name=name, addr=addr, contract=ctext, asm=asm, lifted=lifted)
    tag = model.replace("/", "_")
    os.makedirs(os.path.join(ROOT, "tools/convert/log"), exist_ok=True)
    os.makedirs(os.path.join(ROOT, "tools/convert/accepted"), exist_ok=True)
    logp = os.path.join(ROOT, "tools/convert/log", f"{name}.{tag}.jsonl")
    msgs = prompt
    report = ""
    with tempfile.TemporaryDirectory() as wd:
        for rnd in range(args.rounds + 1):
            t0 = time.time()
            try:
                reply = llm.complete(model, msgs, system=SYSTEM, max_tokens=24000, tag=f"{name} round {rnd}",
                                     usage_log=os.path.join(ROOT, "tools/convert/usage.jsonl"))
            except Exception as e:
                report = f"model error: {e}"
                break
            code = extract_code(reply)
            if code is None:
                ok, report = False, "the reply had no ```c code block"
            else:
                ok, report = test(name, code, args.snaps, contract, wd)
            with open(logp, "a") as f:
                f.write(json.dumps({"round": rnd, "ok": ok, "seconds": round(time.time() - t0),
                                    "report": report, "code": code}) + "\n")
            with lock:
                print(f"{name:48s} {model:28s} round {rnd}: {'PASS' if ok else 'fail'}"
                      f"{'' if ok else ' - ' + report.splitlines()[0][:80]}", flush=True)
            if ok:
                with open(os.path.join(ROOT, "tools/convert/accepted", f"{name}.{tag}.c"), "w") as f:
                    f.write(code.strip() + "\n")
                return name, model, True, report
            if code is None:
                code = reply[:4000]
            msgs = prompt + "\n\n" + REPAIR.format(failure=report[:3000], code=code)
    return name, model, False, report


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--snaps", required=True)
    ap.add_argument("--models", default="opencode/kimi-k3")
    ap.add_argument("--rounds", type=int, default=3)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--redo", action="store_true", help="also rerun pairs that already passed or used up their rounds")
    ap.add_argument("funcs", nargs="+")
    args = ap.parse_args()
    args.snaps = os.path.abspath(args.snaps)
    # test with a difftest built from the current sources (a stale one once
    # accepted candidates that return to the wrong place); farms for several
    # providers run at once, so they take turns building
    import fcntl
    with open(os.path.join(HOST, ".farm-build.lock"), "w") as lk:
        fcntl.flock(lk, fcntl.LOCK_EX)
        r = subprocess.run(["make", "-C", os.path.join(HOST, "host"), f"-j{os.cpu_count()}"], capture_output=True, text=True)
    if r.returncode:
        sys.exit("cannot build cport/build-host/difftest:\n" + r.stdout[-2000:] + r.stderr[-2000:])
    models = args.models.split(",")
    work = [(f, m) for f in args.funcs for m in models]
    if not args.redo:   # resume: skip pairs that already passed or used up their rounds
        def finished(f, m):
            log = os.path.join(ROOT, "tools", "convert", "log", f"{f}.{m.replace('/', '_')}.jsonl")
            if not os.path.exists(log):
                return False
            rows = [json.loads(l) for l in open(log) if l.strip()]
            return any(r.get("ok") for r in rows) or any(r.get("round", 0) >= args.rounds for r in rows)
        skipped = [w for w in work if finished(*w)]
        work = [w for w in work if not finished(*w)]
        if skipped:
            print(f"resuming: {len(skipped)} function/model pairs already finished", flush=True)
    with ThreadPoolExecutor(args.jobs) as ex:
        results = list(ex.map(lambda fm: convert(fm[0], fm[1], args), work))
    print("\nsummary:")
    for m in models:
        ok = [r for r in results if r[1] == m and r[2]]
        print(f"  {m:28s} {len(ok)}/{len(args.funcs)} passed")
    for f in args.funcs:
        passed = [r[1] for r in results if r[0] == f and r[2]]
        print(f"  {f:48s} {', '.join(passed) or '-'}")


if __name__ == "__main__":
    main()
