"""Reading a readable C file (cport/src/game) closely enough to mutate it: its functions, their
bodies, the parts tools/layout.py lays out, and which `Next` entries run a given function's code.

Not a C parser. Comments and string literals are blanked first (same length, so offsets stay
valid), then functions are found at brace depth 0 as `... NAME(PARAMS) {`, and tokens are read
with one regular expression. That is enough for the code the farm and the convert workflow write
(macros like LD8/ST16/SET_FLAGS_NZ/GUEST_CALL, typed static helpers, `Next NAME(void)` adapters).
Standard library only.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field

# what tools/funcindex.py READABLE accepts before the name: an entry the lifter replaces
ENTRY_HEADER = re.compile(r"^(?:IWRAM_\w+\s+|ARM_CODE\s+)*Next$")
PART = re.compile(r"^/\* ── (\w+) ── \*/", re.M)
TOKEN = re.compile(r"""
    (?P<num>0[xX][0-9a-fA-F]+[uUlL]*|\d+[uUlL]*)
  | (?P<id>[A-Za-z_]\w*)
  | (?P<op><<=|>>=|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^]=|[-+*/%&|^~!<>=?:;,.(){}\[\]])
""", re.X)


def blank(text: str) -> str:
    """TEXT with comments, string and character literals replaced by spaces (newlines kept)."""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            j = min(n, j + 1)
        else:
            i += 1
            continue
        for k in range(i, j):
            if out[k] != "\n":
                out[k] = " "
        i = j
    return "".join(out)


@dataclass
class Token:
    kind: str          # num | id | op
    text: str
    start: int
    end: int


def tokens(code: str, start: int, end: int) -> list[Token]:
    out: list[Token] = []
    for m in TOKEN.finditer(code, start, end):
        kind = m.lastgroup or "op"
        out.append(Token(kind, m.group(0), m.start(), m.end()))
    return out


def matching(code: str, open_at: int) -> int:
    """The offset of the bracket closing the one at OPEN_AT (blanked code)."""
    pairs = {"(": ")", "[": "]", "{": "}"}
    opener = code[open_at]
    closer = pairs[opener]
    depth = 0
    for i in range(open_at, len(code)):
        ch = code[i]
        if ch == opener:
            depth += 1
        elif ch == closer:
            depth -= 1
            if depth == 0:
                return i
    raise ValueError(f"unbalanced {opener} at offset {open_at}")


@dataclass
class Function:
    name: str
    header: str              # return type and qualifiers, e.g. "static u32", "IWRAM_CODE Next"
    params: str
    start: int               # offset of the first character of the declaration
    body_open: int           # offset of `{`
    body_close: int          # offset of the matching `}`
    part: str | None         # the `── NAME ──` part it sits in
    is_next: bool            # `Next NAME(void)`: an entry the lifter replaces
    calls: set[str] = field(default_factory=set)        # functions of this file it calls or names directly
    guest_calls: set[str] = field(default_factory=set)  # names given to GUEST_CALL(...)


@dataclass
class CFile:
    path: str
    text: str
    code: str                # blanked
    functions: dict[str, Function]

    def line_of(self, offset: int) -> int:
        return self.text.count("\n", 0, offset) + 1

    def touched_entries(self, name: str) -> set[str]:
        """The `Next` entries whose own behaviour a change to NAME changes: NAME itself if it is one,
        and every entry that reaches NAME through direct calls of non-entry functions. (An entry that
        reaches NAME only through another entry calls the changed code on both sides of its
        difftest, the lifted original included, so it is not touched.)"""
        fn = self.functions.get(name)
        if fn is None:
            return set()
        if fn.is_next:
            return {name}
        callers: dict[str, set[str]] = {}
        for f in self.functions.values():
            for callee in f.calls:
                callers.setdefault(callee, set()).add(f.name)
        out: set[str] = set()
        seen = {name}
        todo = [name]
        while todo:
            for caller in callers.get(todo.pop(), ()):
                if caller in seen:
                    continue
                seen.add(caller)
                if self.functions[caller].is_next:
                    out.add(caller)
                else:
                    todo.append(caller)
        return out

    def owned_by(self, entry: str) -> set[str]:
        """The functions whose code entry ENTRY runs as its own: itself and the non-entry functions it
        reaches through direct calls (the sites a mutation of ENTRY may change)."""
        out = {entry}
        todo = [entry]
        while todo:
            for callee in self.functions[todo.pop()].calls:
                fn = self.functions.get(callee)
                if fn is not None and not fn.is_next and callee not in out:
                    out.add(callee)
                    todo.append(callee)
        return out


def parse(path: str, text: str | None = None) -> CFile:
    if text is None:
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
    code = blank(text)
    parts = [(m.start(), m.group(1)) for m in PART.finditer(text)]
    functions: dict[str, Function] = {}
    depth = 0
    i = 0
    n = len(code)
    while i < n:
        ch = code[i]
        if ch == "{" and depth == 0:
            # a function body if a `)` (the parameter list) comes right before it
            j = i - 1
            while j >= 0 and code[j].isspace():
                j -= 1
            if j >= 0 and code[j] == ")":
                close_params = j
                depth_p = 0
                k = close_params
                while k >= 0:
                    if code[k] == ")":
                        depth_p += 1
                    elif code[k] == "(":
                        depth_p -= 1
                        if depth_p == 0:
                            break
                    k -= 1
                open_params = k
                e = open_params
                while e > 0 and code[e - 1].isspace():
                    e -= 1
                b0 = e
                while b0 > 0 and (code[b0 - 1].isalnum() or code[b0 - 1] == "_"):
                    b0 -= 1
                if b0 < e:
                    name = code[b0:e]
                    # the declaration starts after the previous `;`, `}` or preprocessor line
                    head_from = max(code.rfind(";", 0, b0), code.rfind("}", 0, b0)) + 1
                    line = code.rfind("\n", 0, b0)
                    while line > head_from:
                        prev_line = code.rfind("\n", 0, line)
                        if code[prev_line + 1:line].lstrip().startswith("#"):
                            head_from = line + 1
                            break
                        line = prev_line
                    header = " ".join(code[head_from:b0].split())
                    body_close = matching(code, i)
                    part = None
                    for at, pname in parts:
                        if at <= i:
                            part = pname
                    params = " ".join(code[open_params + 1:close_params].split())
                    is_next = bool(ENTRY_HEADER.match(header)) and params == "void"
                    if name not in ("if", "while", "for", "switch"):
                        functions[name] = Function(name, header, params, head_from, i, body_close, part, is_next)
                        i = body_close + 1
                        continue
            depth += 1
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
        i += 1
    names = set(functions)
    for f in functions.values():
        toks = tokens(code, f.body_open, f.body_close)
        for t_i, t in enumerate(toks):
            if t.kind != "id":
                continue
            if t.text in ("GUEST_CALL",) and t_i + 2 < len(toks):
                f.guest_calls.add(toks[t_i + 2].text)
                continue
            prev = toks[t_i - 1].text if t_i else ""
            if t.text in names and t.text != f.name and prev != "GUEST_CALL":
                # a call, or the function named as a value (a pointer handed on): both run its code
                f.calls.add(t.text)
            elif t.text == f.name and t_i and prev != "GUEST_CALL":
                f.calls.add(t.text)   # recursion
    for f in functions.values():
        f.calls.discard(f.name)
    return CFile(path, text, code, functions)
