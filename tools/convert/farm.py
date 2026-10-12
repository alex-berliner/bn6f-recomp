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

# The typed prompt. Its version goes into every logged round with its hash (tools/wf/wf/tools/models.py);
# the one before it is kept verbatim for the benchmark in tools/convert/bench/prompt_typed1.py.
#   typed-2 (2026-10-10, docs/reviews/2026-10-10-benchmark.md): the callees' typed wrappers are generated
#     (the prompt lists them, the test adds them to the code); helpers are only for calls through an address
#     and BIOS calls, and the tool notes them; every check runs and the report lists every problem; a repair
#     (and the next model) starts from the best attempt so far, with a one-line history; the rules say how
#     names, signed loads, ST* and file-scope initializers work.
#     2026-10-12, same version: a wrapper returns only what its callee returns to this function (the
#     callee's contract outputs that the original reads after the call), as the commit gate's leftover lint
#     (tools/abi/leftovers.py --deep) allows; the test runs that lint too (69 typed candidates had been taken
#     out at install for wrappers that copied back everything the callee can leave).
PROMPT_VERSION = "typed-2"

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
  appear in only three places, and an automatic check rejects them anywhere else:
  1. the entry point `Next NAME(void)`, which must be a THIN ADAPTER: read the arguments from
     registers, make ONE call of a typed function defined in your code, write its results to the
     output registers (and flags, if "outputs" lists them), `RETURN_TO_CALLER();`. No if/for/while/
     switch/?:, no memory access, no GUEST_CALL in it, at most 8 statements.
  2. the generated wrappers of the game functions it calls, listed with the function below:
     `call_NAME(...)` sets NAME's argument registers from its parameters, runs NAME and returns
     what it returns to this function: nothing, R0 as a u32, or a struct of those registers and
     flags (.r0, .r1, ..., .z and .n for the N/Z flags, .c, .v). The tool adds them to your code:
     call them, do not define them, and do not write your own helpers for these functions.
  3. a small helper for a call no wrapper covers: through a code address (`mov lr, pc; bx rN`, a
     jump table) with `GUEST_CALL_ADDR(address);`, or a BIOS call (`svc N`) with `swi_call(N);`.
     It sets R0.. from its parameters, makes that one call and returns R0 or a flag (ZERO_FLAG(),
     CARRY_FLAG(), ...); the tool marks such a helper as register form. GUEST_CALL takes only a
     function name; nothing else exists (no GUEST_CALL_INDIRECT, _PTR or _DISPATCH, no tail-call macro).
  Any other register use needs a `/* register form: <why> */` note that really rules C out (e.g.
  a stack buffer whose guest address is passed to a callee: `SP -= N; ... SP += N;`).
- The typed function takes the arguments as parameters with real types and names and returns the
  result (a struct or pointer out-parameter if there are several). An output register the original
  leaves unchanged on some path is passed in and returned unchanged on that path.
- Arguments arrive in the registers listed under "inputs"; results go in those under "outputs".
  Registers listed under "clobbers" are free; every other register must end exactly as the
  original leaves it (a typed function never touches them, so this is automatic). The same goes
  for an output the original only leaves behind from a callee (e.g. r2/r3 after a fill): the
  wrapper's call already left it there, so do not pass registers through structs or return
  values; the adapter writes only results the typed function computes.
- Guest memory (GBA addresses as u32): LD8/LD16/LD32(addr); signed loads (the original's ldrsb/
  ldrsh) LDS8/LDS16(addr); stores ST8/ST16/ST32(addr, value), which are statements, not
  expressions. Types: u8 u16 u32 s8 s16 s32 bool.
- Names: every label of the disassembly is a macro A_LABEL holding its address (A_eScreenFade,
  A_off_8006040; the bare label is not defined), struct field offsets are oStruct_Field
  (oBattleObject_Alliance, oToolkit_BattleStatePtr), and assembler constants keep their names. Use
  them where the assembly does. They are plain integers: there are no struct types or game
  variables to declare. File-scope initializers must be constants (numbers and these macros); read
  memory inside a function.
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
/* Queue one OAM entry unless the pool is full or the gate flag is set.
 * Returns 0xFF if it was queued, else attr01 unchanged (the original leaves r0 alone). */
static u32 queue_oam_entry(u32 attr01, u32 attr2, u32 layer, s32 position) {
    if ((s32)LD32(SPRITE_COUNT) >= POOL_SLOTS)
        return attr01;
    if (!call_TestEventFlag(0x1234).z)        /* TestEventFlag: Z clear when the flag is set */
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

{callees}Original assembly (bn6f disassembly):
```
{asm}
```

Machine-translated C currently used (exact, but unreadable):
```c
{lifted}
```

Reply with one ```c code block containing the typed function(s), any helpers for calls through an
address or BIOS calls, and the thin adapter `Next {name}(void)` (plus enums or #defines it needs).
No #include lines, nothing outside the block.
"""

# {callees}: the section CALLEES fills (empty when the function calls no other); {whose}: whose code it is;
# {history}: HISTORY or nothing
CALLEES = """Functions it calls, as generated typed wrappers (the tool adds them; r10 stays as on entry):
```c
{declarations}
```

"""

REPAIR = """The best attempt so far, {whose}, failed the automatic checks. Every problem they found:
```
{failure}
```
That code was:
```c
{code}
```
{history}Fix every problem listed (compile errors, differences from the original's behaviour, register use
the conventions do not allow) and reply with the corrected complete code in one ```c block. Remember: same
memory writes, same output registers, flags as listed in "outputs", untouched registers unchanged."""

HISTORY = "Earlier attempts, best first: {attempts}.\n"


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


def register_problems(name, code, workdir, legacy=False):
    """tools/abi/lint.py on the candidate: guest registers only in thin adapters and noted helpers.
    LEGACY: typed-1's advice (hand-written call helpers) at the end of the report."""
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
    if legacy:
        lines.append("Move the logic into a typed function; keep `Next` a thin adapter (no branches, memory access or "
                     "GUEST_CALL); put each outside call in a helper with the `register form: calls NAME ...` note.")
    else:
        lines.append("Move the logic into a typed function; keep `Next` a thin adapter (no branches, memory access or "
                     "GUEST_CALL); call game functions through their generated call_NAME wrappers (a call through an "
                     "address or a BIOS call: a one-call helper, which the tool marks); any other register use needs "
                     "a `/* register form: <why> */` note that rules C out.")
    return "\n".join(lines)


# ── the callees' generated wrappers (review 3 Area 4.2: tools write the boilerplate) ─────────────────

_units_cache = {}


def _units():
    """Every lifted function's header (tools/funcindex.py): its range and register contract, for the
    lifted output under HOST (pool.py points HOST at another tree)."""
    if HOST not in _units_cache:
        import funcindex
        _units_cache[HOST] = funcindex.lifted_units(os.path.join(HOST, "src", "lifted"))
    return _units_cache[HOST]


_callees_cache = {}


def callees(name, lifted=None):
    """The game functions NAME's lifted body calls or branches to directly (`bl X`, a tail `b X`), in
    order: every other lifted function its code names, not labels inside it or other entries of it."""
    import funcindex
    if (HOST, name) in _callees_cache:
        return _callees_cache[(HOST, name)]
    if lifted is None:
        lifted, _ = find_lifted(name)
    units = _units()
    own = units.get(name)
    found = []
    for target in funcindex.BRANCH.findall(lifted or ""):
        unit = units.get(target)
        if unit is None or target == name or target in found:
            continue
        if own is not None and own.lo <= unit.lo < own.hi:     # another entry of the same code
            continue
        found.append(target)
    _callees_cache[(HOST, name)] = found
    return found


def callee_wrappers(name, lifted=None):
    """{callee: gen.ContractWrapper}: a typed wrapper of each function NAME calls, from the callee's
    lifted register contract (tools/abi/gen.py contract_wrapper, the generator of the typed calls). It
    returns only what the callee returns to NAME (returned_to): reading anything else after the call is
    a leftover read, which the commit gate refuses."""
    sys.path.insert(0, os.path.join(ROOT, "tools", "abi"))
    import gen
    units = _units()
    wrappers = {}
    for callee in callees(name, lifted):
        outputs = list(units[callee].contract.outputs)
        wrappers[callee] = gen.contract_wrapper(callee, list(units[callee].contract.inputs),
                                                returned_to(name, callee, outputs),
                                                f"generated wrapper: {callee} has no typed C interface yet",
                                                as_struct=[r for r in outputs if re.fullmatch(r"r\d+|nz|c|v", r)]
                                                not in ([], ["r0"]))
    return wrappers


_contracts_cache = {}


def leftover_contracts():
    """tools/abi/leftovers.py's contracts: the lifted headers, and per call site what the original reads
    after each call (build-abi/graph.json); read once per process (the graph is large)."""
    if HOST not in _contracts_cache:
        sys.path.insert(0, os.path.join(ROOT, "tools", "abi"))
        import leftovers
        _contracts_cache[HOST] = leftovers.load_contracts(
            lifted_dir=os.path.join(HOST, "src", "lifted"), lifted_h=os.path.join(HOST, "include", "lifted.h"))
    return _contracts_cache[HOST]


def returned_to(name, callee, outputs):
    """What CALLEE returns to NAME, of its contract's OUTPUTS: those the original NAME reads after its calls
    to CALLEE (the leftover lint's rule: tools/abi/leftovers.py classify), in OUTPUTS' order; all of OUTPUTS
    where the call sites' liveness is not known."""
    contracts = leftover_contracts()
    caller, unit = contracts.of(name), contracts.of(callee)
    if caller is None or unit is None or not contracts.have_graph:
        return outputs
    live = contracts.site_live.get((caller.start, unit.start))
    return outputs if live is None else [reg for reg in outputs if reg in live and reg in unit.outputs]


def callees_section(name, lifted=None):
    """The prompt's list of the wrappers (CALLEES), or nothing when NAME calls no other function."""
    wrappers = callee_wrappers(name, lifted)
    if not wrappers:
        return ""
    return CALLEES.format(declarations="\n".join(w.declaration for w in wrappers.values()))


def _defines(code, function):
    return re.search(rf"^[^\n;(]*\b{re.escape(function)}\s*\([^;{{]*\)\s*\{{", blank_comments(code), re.M) is not None


def wrappers_used(name, code):
    """The definitions of the generated wrappers CODE calls and does not define itself, in one text."""
    bare = blank_comments(code)
    return "".join(w.definition + "\n" for callee, w in callee_wrappers(name).items()
                   if re.search(rf"\bcall_{re.escape(callee)}\s*\(", bare) and not _defines(code, f"call_{callee}"))


# ── register-form notes the tool writes or accepts ──────────────────────────────────────────────────

# a note in other words: "Register-form helper:", "register form -", "REGISTER FORM:" (the lint wants
# `register form:`); a separator is required, so a comment that only mentions register form stays as it is
_NEAR_NOTE = re.compile(r"\bregister[ \t_-]*form(?:[ \t_-]+helper)?[ \t]*(?::|-+|–|—)[ \t]*", re.I)
_ONE_CALL = re.compile(r"\b(GUEST_CALL_ADDR|GUEST_CALL|swi_call)\s*\(\s*([^;]*?)\s*\)\s*;")
_STRUCTURE = re.compile(r"\b(?:if|for|while|switch|goto|do)\b")
_MEMORY = re.compile(r"\b(?:LD|LDS|ST)(?:8|16|32)\s*\(")


def _abi_lint():
    sys.path.insert(0, os.path.join(ROOT, "tools", "abi"))
    import lint
    return lint


_next_cache = {}


def _next_names(lint):
    """The game's `Next` functions (lifted.h), whose direct C call is the lint's next_call."""
    if lint.LIFTED_H not in _next_cache:
        text = open(lint.LIFTED_H).read() if os.path.exists(lint.LIFTED_H) else ""
        _next_cache[lint.LIFTED_H] = set(lint.NEXT_PROTO_RE.findall(text))
    return _next_cache[lint.LIFTED_H]


def note_register_forms(code):
    """CODE with the register-form notes the tool can write itself, on the lines they belong to (line
    numbers stay): a note in other words is put in the lint's words, and a call helper the conventions
    allow (a typed function that only sets R0.. from its values, makes one GUEST_CALL, GUEST_CALL_ADDR
    or swi_call, and returns R0 or a flag: no branches, loops or memory access) gets the note after its
    `{`. Any other register use stays unexplained, for the register check to report."""
    def normal(match):
        text = match.group(0)
        return _NEAR_NOTE.sub(lambda m: "register form: " if m.group(0).lower().strip() != "register form:" else m.group(0), text)
    code = re.sub(r"/\*.*?\*/|//[^\n]*", normal, code, flags=re.S)
    lint = _abi_lint()
    functions = lint.scan_file("candidate.c", code)
    lint.mark_calls(functions, _next_names(lint) | {f.name for f in functions if f.kind == "next"})
    notes = []
    for f in functions:
        if not f.violates() or f.kind != "typed" or set(f.hits) - {"regs", "guest_call", "flags"}:
            continue
        calls = _ONE_CALL.findall(f.text)
        if (len(calls) != 1 or _STRUCTURE.search(f.text) or _MEMORY.search(f.text) or "SET_FLAGS" in f.text
                or f.text.count(";") > 14):
            continue
        kind, target = calls[0]
        why = {"GUEST_CALL": f"calls {target}, which has no typed C interface yet",
               "GUEST_CALL_ADDR": "calls through a guest code address",
               "swi_call": f"a BIOS call (svc {target})"}[kind]
        notes.append((f.body[0] + 1, f" /* register form: {why} (noted by the tool) */"))
    for at, note in sorted(notes, reverse=True):
        code = code[:at] + note + code[at:]
    return code


def without_generated_wrappers(name, code):
    """CODE as the model wrote it: without the generated wrappers finished() put before it (a kept candidate
    shown to a model again, e.g. after a review: the tool adds today's wrappers itself)."""
    code = code.replace("/* generated wrappers of the game functions it calls (tools/abi/gen.py) */\n", "")
    for callee in callees(name):
        c = re.escape(callee)
        code = re.sub(rf"^typedef struct \{{[^}}]*\}} call_{c}_result;\n", "", code, flags=re.M)
        code = re.sub(rf"^/\* register form: generated wrapper: {c} has no typed C interface yet \*/\n"
                      rf"static inline [^\n]*\bcall_{c}\([^)]*\) \{{\n(?:    [^\n]*\n)*?\}}\n\n?", "", code, flags=re.M)
    return code


def finished(name, code):
    """The candidate as it is kept and installed: self-contained, with the generated wrappers it calls
    and the tool's register-form notes."""
    code = note_register_forms(code)
    used = wrappers_used(name, code)
    return (f"/* generated wrappers of the game functions it calls (tools/abi/gen.py) */\n{used}" if used else "") + code


# ── the acceptance test ─────────────────────────────────────────────────────────────────────────────

# the report's sections, as test() writes them (worst first) and section() finds them again
SECTIONS = (
    ("entry", re.compile(r"^the code must define", re.M)),
    ("compile", re.compile(r"^compile errors:", re.M)),
    ("behaviour", re.compile(r"^(?:captured calls|random inputs):|^difftest timed out", re.M)),
    ("registers", re.compile(r"^guest registers used where", re.M)),
    ("leftovers", re.compile(r"^leftover reads:", re.M)),
    ("style", re.compile(r"^the code still uses machine-translated style", re.M)),
    ("review", re.compile(r"^line \d+: (?:struct offsets and constants|use LDS16)", re.M)),
)
# the order typed-1's test stopped at (the first failing check was its whole report)
FIRST_FAILURE_ORDER = ("entry", "style", "review", "registers", "compile", "behaviour", "leftovers")


def report_sections(report):
    """{kind: text} of a test report: one section per check that has something to say."""
    starts = sorted((m.start(), kind) for kind, pattern in SECTIONS for m in [pattern.search(report)] if m)
    out = {}
    for i, (at, kind) in enumerate(starts):
        end = starts[i + 1][0] if i + 1 < len(starts) else len(report)
        out[kind] = report[at:end].strip()
    return out


def behaviour_ok(text):
    """Whether a behaviour section says both comparisons passed."""
    lines = text.splitlines()
    captured = next((l for l in lines if l.startswith("captured calls:")), "")
    rand = next((l for l in lines if l.startswith("random inputs:")), "")
    return " ok " in f" {captured} " and " ok " in f" {rand} " and "out of domain: nothing was compared" not in text


def first_failure(report):
    """The check typed-1's test would have stopped at (its order: FIRST_FAILURE_ORDER), and its text."""
    sections = report_sections(report)
    for kind in FIRST_FAILURE_ORDER:
        if kind in sections and not (kind == "behaviour" and behaviour_ok(sections[kind])):
            return kind, sections[kind]
    return None, report


_CALLS = re.compile(r"\((\d+) snapshots, (\d+) runs, (\d+) failures, (\d+) out-of-domain\)")
_TRIALS = re.compile(r"\((\d+) trials, (\d+) failures, (\d+) out-of-domain\)")


def rank(passed, report):
    """How close a tested candidate came, smaller is better (review 3 Area 2, step 4, with every check run):
    passed > behaves the same but breaks a form rule > differs only on random inputs > differs on fewer
    captured calls > compares nothing > does not compile > no entry point; then fewer form problems."""
    if passed:
        return (0, 0.0, 0)
    sections = report_sections(report)
    form = sum(1 for kind in ("registers", "leftovers") for line in sections.get(kind, "").splitlines()
               if line.startswith("- ")) \
        + ("style" in sections) + len(re.findall(r"^line \d+: ", sections.get("review", ""), re.M))
    if "entry" in sections:
        return (6, 1.0, form)
    if "compile" in sections:
        return (5, 1.0, form + sections["compile"].count("\nline "))
    text = sections.get("behaviour", "")
    if behaviour_ok(text):
        return (1, 0.0, form)
    calls, trials = _CALLS.search(text), _TRIALS.search(text)
    if calls and int(calls[3]) and int(calls[2]) > int(calls[4]):
        return (3, int(calls[3]) / max(1, int(calls[2]) - int(calls[4])), form)
    if calls and trials and int(trials[2]):
        return (2, int(trials[2]) / max(1, int(trials[1])), form)
    return (4, 1.0, form)


def verdict(passed, report):
    """The test's outcome in a few words, for a history line."""
    if passed:
        return "passes"
    sections = report_sections(report)
    form = [what for kind, what in (("registers", "the register check"), ("leftovers", "the leftover check"),
                                    ("style", "lifted style"), ("review", "a review rule")) if kind in sections]
    place, share, _ = rank(passed, report)
    text = sections.get("behaviour", "")
    calls, trials = _CALLS.search(text), _TRIALS.search(text)
    words = {6: "no entry point", 5: f"does not compile ({sections.get('compile', '').count(chr(10) + 'line ') or 1} errors)",
             1: "behaves the same", 4: "compares nothing" if "out of domain" in text else "the difftest failed",
             3: f"differs on {calls[3]} of {int(calls[2]) - int(calls[4])} captured calls" if calls else "",
             2: f"differs on {trials[2]} of {trials[1]} random inputs" if trials else ""}[place]
    return words + (f"; fails {' and '.join(form)}" if form else "")


def _compile(name, code, workdir, wrappers):
    """(compiled, report): the candidate as a shared library for the difftest, its entry point renamed;
    WRAPPERS: with the generated wrappers it calls (their own lines are named apart in errors)."""
    src = os.path.join(workdir, "cand.c")
    so = os.path.join(workdir, "cand.so")
    renamed = re.sub(rf"\bNext\s+{re.escape(name)}\s*\(\s*void\s*\)", f"Next cand_{name}(void)", code)
    used = wrappers_used(name, code) if wrappers else ""
    with open(src, "w") as f:   # errors give the candidate's own lines
        f.write('#include "game.h"\n#include "runtime.h"\n'
                + (f'#line 1 "generated wrappers"\n{used}' if used else "")
                + f'#line 1 "{src}"\n' + renamed + "\n")
    cc = subprocess.run(["gcc", *CFLAGS, "-o", so, src], capture_output=True, text=True)
    if not cc.returncode:
        return True, ""
    lines = code.splitlines()
    errs = []
    stderr = cc.stderr.splitlines()
    for i, l in enumerate(stderr):
        where = None          # (the candidate's line, the error)
        m = re.match(re.escape(src) + r":(\d+):\d+: (?:fatal )?error: (.*)", l)
        if m:
            where = int(m.group(1)), m.group(2)
        elif wrappers and re.match(r"\S+\.h:\d+:\d+: (?:fatal )?error: ", l):
            # typed-2: an error inside a header's macro; gcc names the candidate's line in a note after it
            for later in stderr[i + 1:i + 8]:
                note = re.match(re.escape(src) + r":(\d+):\d+: note: (in expansion of macro .*)", later)
                if note:
                    where = int(note.group(1)), f"{l.split(' error: ', 1)[1]} ({note.group(2)})"
                    break
        if where and len(errs) < 8:
            n, message = where
            error = f"line {n}: error: {message}" + (f"\n    {lines[n - 1].strip()}" if 0 < n <= len(lines) else "")
            if not (wrappers and error in errs):     # typed-2: the same error on the same line once
                errs.append(error)
        elif "error" in l and src not in l and len(errs) < 8 and not (wrappers and l in errs):
            errs.append(l)
    return False, "compile errors:\n" + "\n".join(errs)


def _difftest(name, snaps, contract, workdir):
    """(ok, report): the compiled candidate against the lifted original, on the captured calls, then
    (when they pass) on random inputs."""
    so = os.path.join(workdir, "cand.so")
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
        m = _CALLS.search(line)
        if m and int(m.group(2)) == int(m.group(4)):
            ok = False
            reports.append("every captured call was out of domain: nothing was compared")
            break
    return ok, "\n".join(reports)


def leftover_problems(name, code):
    """tools/abi/leftovers.py --deep, the commit gate's lint, on the candidate as it would be installed (the
    tool's notes and the generated wrappers it calls, in NAME's part): the reads of a register or flag that
    a callee leaves but does not return to NAME. The report's line numbers are the candidate's."""
    sys.path.insert(0, os.path.join(ROOT, "tools", "abi"))
    import leftovers
    import tempfile
    noted = note_register_forms(code)
    used = wrappers_used(name, noted)
    head = f"/* ── {name} ── */\n" + used
    with tempfile.NamedTemporaryFile("w", suffix=".c", dir=None, delete=False) as f:
        f.write(head + noted)
    try:
        sites = [s for s in leftovers.scan([f.name], leftover_contracts(), deep=True)
                 if not s.noted and s.kind != "zflag"]
    finally:
        os.unlink(f.name)
    if not sites:
        return ""
    offset = head.count("\n")
    lines = noted.splitlines()
    why = {"contract": "never returns it", "site": "does not return it to this function: the original never "
           "reads it after this call", "passed": "does not return it, and it is passed on into the next call"}
    out = ["leftover reads: after a call the code reads a register or flag the callee leaves but does not return "
           "(tools/abi/leftovers.py --deep, the commit gate's check):"]
    for s in sites:
        line = s.line - offset
        where = (f"line {line}: {lines[line - 1].strip()}" if 0 < line <= len(lines)
                 else f"in the generated wrapper call_{s.callee}")
        out.append(f"- {where}\n    reads {s.reg} after {s.callee}, which {why.get(s.kind, s.kind)}")
    out.append("Read only what each call returns (its wrapper's result); where the original really relies on a "
               "callee leaving a register, say so: /* register form: relies on F leaving rN */.")
    return "\n".join(out)


def test(name, code, snaps, contract, workdir, check_registers=True, legacy=False, leftovers=False):
    """(ok, report): the acceptance test of a candidate. Every check runs and the report lists every
    problem, worst first (review 3 Area 3 step 3): the entry point, compile errors, behaviour (the
    difftest on the captured calls, then on random inputs: whenever the code compiles), the register
    check (CHECK_REGISTERS=False skips it: register-form prompts and candidates written before it), lifted
    style, the review rules. The code is checked with the tool's register-form notes (note_register_forms)
    and compiled with the generated wrappers it calls (callee_wrappers).
    LEFTOVERS: also the commit gate's leftover lint (leftover_problems; the farm asks for it, so what it
    accepts can be installed; the benchmark's frozen arms do not).
    LEGACY: typed-1's test, for the benchmark's frozen arms: neither of those, and only the first failing
    check in its order (FIRST_FAILURE_ORDER) is run and reported (with LEFTOVERS: the leftover lint last,
    once the rest passes)."""
    sections = {}
    has_entry = re.search(rf"\bNext\s+{re.escape(name)}\s*\(\s*void\s*\)", code) is not None
    if not has_entry:
        sections["entry"] = f"the code must define `Next {name}(void)`"
        if legacy:
            return False, sections["entry"]
    if not legacy:
        code = note_register_forms(code)   # on the same lines: the reports' line numbers stay the candidate's
    # comments may mention the flags ("sets nz"); only code counts
    bare = blank_comments(code)
    lines = code.splitlines()
    style = {m.group(0) for m in LIFTED_STYLE.finditer(bare)}
    if style:
        sections["style"] = "the code still uses machine-translated style: " + ", ".join(sorted(style))[:200]
    rules = review_rule_problems(bare)
    if rules:
        sections["review"] = "\n".join(f"line {line}: {rule}\n    {lines[line - 1].strip() if line <= len(lines) else ''}"
                                       for line, rule in rules)
    registers = register_problems(name, code, workdir, legacy) if check_registers else ""
    if registers:
        sections["registers"] = registers
    if leftovers and not legacy and has_entry:
        reads = leftover_problems(name, code)
        if reads:
            sections["leftovers"] = reads
    if legacy:
        for kind in ("style", "review", "registers"):
            if kind in sections:
                return False, sections[kind]
    compiled, errors = _compile(name, code, workdir, wrappers=not legacy)
    if not compiled:
        sections["compile"] = errors
        if legacy:
            return False, errors
    behaves = False
    if compiled and has_entry:
        behaves, sections["behaviour"] = _difftest(name, snaps, contract, workdir)
        if legacy:
            reads = leftover_problems(name, code) if leftovers and behaves else ""
            return (False, reads) if reads else (behaves, sections["behaviour"])
    ok = behaves and not (set(sections) - {"behaviour"})
    order = [kind for kind, _ in SECTIONS]
    return ok, "\n\n".join(sections[kind] for kind in order if kind in sections)
