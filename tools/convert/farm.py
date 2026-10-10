#!/usr/bin/env python3
"""The model farm's pieces: the prompt texts, finding a function's assembly and
lifted C, its register contract, and the acceptance test of a candidate. The
workflows (tools/wf: `./wf farm`, `./wf convert`) and tools/convert/pool.py use
them; this file has no command line of its own any more (review 2 Area 5.1:
the wf graphs are the only driver; its old one is in git before that commit).

A candidate is compiled into a shared library and checked by
cport/build-host/difftest (candidate mode) against the lifted function, which
the lockstep tests tie to the original:
  - on the original's own calls captured by `./wf capture` (DIR/FUNC),
  - on random inputs.
"""
import glob, json, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import gamepaths  # noqa: E402  # tools/gamepaths.py: the game profile (tools/paths.env)
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
## Conventions of the readable port (cport/include/game.h, cpu.h, docs/C_CALLING_CONVENTION.md)
- The game code is ordinary typed C. Guest registers (the u32 lvalues R0..R12, SP, LR) and flags
  appear in only two places, and an automatic check rejects them anywhere else:
  1. the entry point `Next NAME(void)`, which must be a THIN ADAPTER: read the arguments from
     registers, make ONE call of a typed function defined in your code, write its results to the
     output registers (and flags, if "outputs" lists them), `RETURN_TO_CALLER();`. No if/for/while/
     switch/?:, no memory access, no GUEST_CALL in it, at most 8 statements.
  2. a small helper that calls a game function outside your code, marked with the comment
     `/* register form: calls NAME outside this file, which has no typed C interface yet */`
     right above it. It sets R0.. from its parameters, does `GUEST_CALL(NAME);` and returns the
     result (R0, or a flag read with ZERO_FLAG()/CARRY_FLAG()/... for callees that answer in flags).
  Any other register use needs a `/* register form: <why> */` note that really rules C out (e.g.
  a stack buffer whose guest address is passed to a callee: `SP -= N; ... SP += N;`).
- The typed function takes the arguments as parameters with real types and names and returns the
  result (a struct or pointer out-parameter if there are several). An output register the original
  leaves unchanged on some path is passed in and returned unchanged on that path.
- Arguments arrive in the registers listed under "inputs"; results go in those under "outputs".
  Registers listed under "clobbers" are free; every other register must end exactly as the
  original leaves it (a typed function never touches them, so this is automatic). The same goes
  for an output the original only leaves behind from a callee (e.g. r2/r3 after a fill): the
  helper's GUEST_CALL already left it there, so do not pass registers through structs or return
  values; the adapter writes only results the typed function computes.
- Guest memory (GBA addresses as u32): LD8/LD16/LD32(addr) and ST8/ST16/ST32(addr, value);
  signed loads as (s8)LD8(a), (s16)LD16(a). Types: u8 u16 u32 s8 s16 s32 bool.
- The game's struct field offsets and RAM symbols are macros named as in the disassembly
  (e.g. oBattleObject_Alliance, oToolkit_BattleStatePtr); use them instead of numbers where the
  assembly uses them.
- Every game function is available to GUEST_CALL by its disassembly name. BIOS calls (`svc N`):
  in a register-form helper, set R0.. and call `swi_call(N);`.
- Condition flags: if "outputs" lists nz, c or v, callers branch on them right after the call.
  The adapter sets them from the typed function's result, as the original's last flag-setting
  instruction does: `SET_FLAGS_NZ(x)` (MOV/AND/TST/... with result x), `SET_FLAGS_CMP(a, b)` (CMP
  a, b or SUBS a - b), `SET_FLAGS_ADD(a, b)` (ADDS/CMN). If that differs per path, return what the
  adapter needs (e.g. the compared value) so it can set them without branching.
- The original's push/pop of saved registers need not be reproduced.
- Write readable C: named local variables, if/else and loops instead of goto, no copies of the
  machine-translated style (no nz/c/v locals, no SUBS()/COND_ macros, no L_xxxxxxxx labels, no
  SAVE/LOAD/CALL_DIRECT), never cpu.nz/cpu.c/cpu.v.

## Example (another function of the game, already converted)
```c
/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();          /* TestEventFlag: Z set if the flag is clear */
}

/* Queue one OAM entry unless the pool is full or the gate flag is set.
 * Returns 0xFF if it was queued, else attr01 unchanged (the original leaves r0 alone). */
static u32 queue_oam_entry(u32 attr01, u32 attr2, u32 layer, s32 position) {
    if ((s32)LD32(SPRITE_COUNT) >= POOL_SLOTS || event_flag_set(0x1234))
        return attr01;
    link_entry(attr01, attr2, layer, position);
    return 0xFF;
}

/* sub_30068E8(r0 = attr01, r1 = attr2, r2 = layer, r3 = list position) -> r0 */
Next sub_30068E8(void) {
    R0 = queue_oam_entry(R0, R1, R2, (s32)R3);
    SET_FLAGS_NZ(R0);
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

Reply with one ```c code block containing the typed function(s), any register-form call helpers,
and the thin adapter `Next {name}(void)` (plus enums or #defines it needs). No #include lines, nothing outside the block.
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
    """The reply's C: every ```c / ```cpp / ``` block, in order (models sometimes split helpers
    and the entry point), each once; #include lines dropped."""
    blocks = re.findall(r"```(?:c|C|cpp|c\+\+|h)?[ \t]*\n(.*?)```", reply, re.S)
    blocks = [b for i, b in enumerate(blocks) if b.strip() and not any(b in other for j, other in enumerate(blocks) if j != i and len(other) > len(b))]
    unique = list(dict.fromkeys(blocks))
    if not unique:
        return None
    code = "\n".join(unique)
    return "\n".join(l for l in code.splitlines() if not l.strip().startswith("#include"))


LIFTED_STYLE = re.compile(r"\b(nz|SUBS|ADDS|COND_\w+|CALL_DIRECT|CALL_LEAF|SAVE|LOAD)\b|L_[0-9A-F]{8}")

# review rules a machine can check (docs/PROCESS.md §2): rejected here so the model fixes them itself
REVIEW_RULES = (
    (re.compile(r"^[ \t]*(#)\s*ifndef\s+\w+\s*\n\s*#\s*define", re.M),
     "struct offsets and constants come from enums taken from bn6f include/structs, not #ifndef fallbacks"),
    (re.compile(r"(\()\s*s16\s*\)\s*LD16\b"),
     "use LDS16 for the original's ldrsh ((s16)LD16 differs at a misaligned address)"),
)


def blank_comments(code):
    """Comments replaced by spaces, newlines kept, so line numbers still match the candidate."""
    return re.sub(r"/\*.*?\*/|//[^\n]*", lambda m: re.sub(r"[^\n]", " ", m.group(0)), code, flags=re.S)


def input_key(name):
    """What the function was made from: a hash of its original bytes in bn6f.gba (the
    lifted header's address range), or of its assembly when they are not in ROM.
    Renames and lifter changes leave it alone; a change to the original code does not."""
    import hashlib
    lifted, start = find_lifted(name)
    if lifted is not None:
        m = re.match(r"/\* \w+: ([0-9A-F]{8})-([0-9A-F]{8})", lifted)
        lo, hi = (int(m.group(1), 16), int(m.group(2), 16)) if m else (0, 0)
        if 0x08000000 <= lo < hi <= 0x0A000000 and hi - lo < 0x10000:
            with open(gamepaths.ORIG_ROM, "rb") as rom:
                rom.seek(lo - 0x08000000)
                return "rom:" + hashlib.sha1(rom.read(hi - lo)).hexdigest()[:16]
    asm = find_asm(name)
    return "asm:" + hashlib.sha1(asm.encode()).hexdigest()[:16] if asm else None


def review_rule_problems(code):
    """[(line, rule)] for each place the code breaks a review rule."""
    # the line of the rule's token itself (group 1), not of whitespace a match may start in
    return [(code.count("\n", 0, m.start(1)) + 1, rule) for pattern, rule in REVIEW_RULES for m in pattern.finditer(code)]


def register_problems(name, code, workdir):
    """tools/abi/lint.py on the candidate: guest registers only in thin adapters and noted helpers."""
    path = os.path.join(workdir, f"{name}.c")
    with open(path, "w") as f:
        f.write(code)
    done = subprocess.run([sys.executable, os.path.join(ROOT, "tools/abi/lint.py"), "--quiet", path],
                          capture_output=True, text=True)
    functions = json.loads(done.stdout)["files"].get(f"{name}.c", [])
    bad = [f for f in functions if f["violation"]]
    if not bad:
        return ""
    lines = ["guest registers used where the conventions do not allow them:"]
    for f in bad:
        kind = "a register body, not a thin adapter" if f["kind"] == "next" else f"a {f['kind']} function"
        where = "; ".join(f"{cat} on lines {', '.join(map(str, sorted(set(ns))))}" for cat, ns in sorted(f["lines"].items()))
        lines.append(f"- {f['name']} (line {f['line']}, {kind}): {where}")
    lines.append("Move the logic into a typed function; keep `Next` a thin adapter (no branches, memory access or "
                 "GUEST_CALL); put each outside call in a helper with the `register form: calls NAME ...` note.")
    return "\n".join(lines)


def test(name, code, snaps, contract, workdir, check_registers=True):
    """(ok, report): compile the candidate and run the comparisons. check_registers=False
    skips the register check (re-testing candidates written before it existed)."""
    if not re.search(rf"\bNext\s+{re.escape(name)}\s*\(\s*void\s*\)", code):
        return False, f"the code must define `Next {name}(void)`"
    # comments may mention the flags ("sets nz"); only code counts
    bare = blank_comments(code)
    lines = code.splitlines()
    style = {m.group(0) for m in LIFTED_STYLE.finditer(bare)}
    if style:
        return False, "the code still uses machine-translated style: " + ", ".join(sorted(style))[:200]
    rules = review_rule_problems(bare)
    if rules:
        return False, "\n".join(f"line {line}: {rule}\n    {lines[line - 1].strip() if line <= len(lines) else ''}"
                                 for line, rule in rules)
    registers = register_problems(name, code, workdir) if check_registers else ""
    if registers:
        return False, registers
    src = os.path.join(workdir, "cand.c")
    so = os.path.join(workdir, "cand.so")
    renamed = re.sub(rf"\bNext\s+{re.escape(name)}\s*\(\s*void\s*\)", f"Next cand_{name}(void)", code)
    with open(src, "w") as f:
        f.write('#include "game.h"\n#include "runtime.h"\n#line 1\n' + renamed + "\n")   # errors give the candidate's lines
    cc = subprocess.run(["gcc", *CFLAGS, "-o", so, src], capture_output=True, text=True)
    if cc.returncode:
        errs = []
        for l in cc.stderr.splitlines():
            m = re.match(re.escape(src) + r":(\d+):\d+: (?:fatal )?error: (.*)", l)
            if m and len(errs) < 8:
                n = int(m.group(1))
                errs.append(f"line {n}: error: {m.group(2)}" + (f"\n    {lines[n - 1].strip()}" if 0 < n <= len(lines) else ""))
            elif "error" in l and src not in l and len(errs) < 8:
                errs.append(l)
        return False, "compile errors:\n" + "\n".join(errs)
    regs, flags = compare_args(contract)
    base = [DIFFTEST, "-L", so, "-R", ",".join(regs)] + (["-F", ",".join(flags)] if flags else [])
    env = dict(os.environ, BN6F_ROM=gamepaths.ORIG_ROM)
    reports = []
    ok = True
    if not os.path.isdir(snaps):
        raise FileNotFoundError(f"no snapshot folder {snaps}")
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

