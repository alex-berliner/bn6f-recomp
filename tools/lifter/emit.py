"""C emission for lifted units."""
from __future__ import annotations

import re
from dataclasses import dataclass

from .analysis import ALLF, C, NZ, V, COND_READS, Program, Unit, flag_effects, writes_pc
from .isa import Insn

REGN = ["r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
        "r8", "r9", "r10", "r11", "r12", "SP", "lr", "PC"]

COND_EXPR = {
    "eq": "COND_EQ", "ne": "COND_NE", "cs": "COND_CS", "cc": "COND_CC",
    "mi": "COND_MI", "pl": "COND_PL", "vs": "COND_VS", "vc": "COND_VC",
    "hi": "COND_HI", "ls": "COND_LS", "ge": "COND_GE", "lt": "COND_LT",
    "gt": "COND_GT", "le": "COND_LE",
}

C_RESERVED = {
    "main", "memcpy", "memset", "abs", "div", "exit", "free", "malloc", "rand",
    "printf", "puts", "strlen", "strcpy", "strcmp", "time", "sqrt", "atan2",
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if", "int",
    "long", "register", "return", "short", "signed", "sizeof", "static",
    "struct", "switch", "typedef", "union", "unsigned", "void", "volatile",
    "while", "inline", "cpu", "dispatch", "swi", "start", "_start", "boot",
}


IWRAM_TEXT_VMA = 0x03005B00
IWRAM_TEXT_SIZE = 0x1ED4


def h(v: int) -> str:
    v &= 0xFFFFFFFF
    return f"0x{v:X}" if v > 9 else str(v)


def cname(name: str) -> str:
    n = re.sub(r"[^A-Za-z0-9_]", "_", name)
    if n[0].isdigit() or n in C_RESERVED:
        n = "bn_" + n
    return n


@dataclass
class Ctx:
    p: Program
    names: dict[int, str]          # entry addr -> C function name
    addr_names: dict[int, str]     # address constant -> A_ symbol
    used_addr_syms: set[str]
    used_equates: dict[str, int]
    flags_in: dict[int, int]       # entry addr -> flags read on entry
    callers_live: dict[int, int]   # unit start -> flags live after return
    indirect_in: int


class UnitEmitter:
    def __init__(self, ctx: Ctx, u: Unit, live_out: dict[int, int]):
        self.ctx = ctx
        self.u = u
        self.live_out = live_out
        self.out: list[str] = []
        self.p = ctx.p if ctx else None
        self.info = ctx.p.info if ctx else None

    # ── helpers ─────────────────────────────────────────────────────────
    def reg(self, n: int, ins: Insn, regshift: bool = False) -> str:
        if n == 15:
            if ins.thumb:
                return h(ins.addr + 4)
            return h(ins.addr + (12 if regshift else 8))
        return REGN[n]

    def addr_const(self, v: int, mode_bit: bool = False) -> str:
        """Render an address constant using a symbol name when one exists."""
        # Data inside the IWRAM routine block (jump/callback/lookup tables)
        # is read from its identical ROM image: the lifted build never copies
        # the routines into IWRAM, and reuses that space for host stacks.
        if IWRAM_TEXT_VMA <= v < IWRAM_TEXT_VMA + IWRAM_TEXT_SIZE and not self.is_code(v & ~1):
            nm = self.ctx.addr_names.get(v)
            if nm is not None:
                self.ctx.used_addr_syms.add(nm)
                return f"IWRAM_ROM({nm})"
            return f"IWRAM_ROM({h(v)})"
        base = v & ~1 if mode_bit else v
        nm = self.ctx.addr_names.get(base)
        if nm is not None and v >= 0x02000000:
            self.ctx.used_addr_syms.add(nm)
            if mode_bit and v & 1:
                return f"({nm} | 1)"
            return nm
        return h(v)

    def is_code(self, a: int) -> bool:
        return (a, True) in self.p.insns or (a, False) in self.p.insns or a in self.ctx.names

    def imm(self, v: int, ins: Insn) -> str:
        """Immediate, rendered as a named equate when the source line used one."""
        src = self.info.source_line(ins.addr) or ""
        for m in re.finditer(r"#\s*([A-Za-z_][A-Za-z0-9_]*)", src):
            nm = m.group(1)
            val = self.info.equates.get(nm)
            if val is not None and (val & 0xFFFFFFFF) == (v & 0xFFFFFFFF) and v > 9:
                self.ctx.used_equates[nm] = val
                return nm
        return h(v)

    def op2(self, ins: Insn, want_carry: bool) -> tuple[str, str | None]:
        """(value_expr, carry_expr or None if C unchanged). Carry expressions
        are evaluated before the value is assigned anywhere."""
        o = ins.op2
        if o.is_imm:
            carry = None
            if want_carry and o.imm_carry is not None:
                carry = str(o.imm_carry)
            return self.imm(o.imm, ins), carry
        x = self.reg(o.rm, ins, regshift=o.rs is not None)
        if o.rs is not None:
            s = f"({self.reg(o.rs, ins)} & 0xFF)"
            fn = {"lsl": "LSL_REG", "lsr": "LSR_REG", "asr": "ASR_REG", "ror": "ROR_REG"}[o.shift]
            if want_carry:
                return f"{fn}_C({x}, {s})", "__inline__"
            return f"{fn}({x}, {s})", None
        n = o.amount
        if o.shift == "lsl":
            if n == 0:
                return x, None
            return f"({x} << {n})", (f"(({x} >> {32 - n}) & 1)" if want_carry else None)
        if o.shift == "lsr":
            if n == 32:
                return "0", (f"({x} >> 31)" if want_carry else None)
            return f"({x} >> {n})", (f"(({x} >> {n - 1}) & 1)" if want_carry else None)
        if o.shift == "asr":
            if n == 32:
                return f"(u32)((s32){x} >> 31)", (f"({x} >> 31)" if want_carry else None)
            return f"(u32)((s32){x} >> {n})", (f"(({x} >> {n - 1}) & 1)" if want_carry else None)
        if o.shift == "ror":
            return f"ROR({x}, {n})", (f"(({x} >> {n - 1}) & 1)" if want_carry else None)
        if o.shift == "rrx":
            return f"((c << 31) | ({x} >> 1))", (f"({x} & 1)" if want_carry else None)
        raise ValueError(o.shift)

    def emit(self, line: str, indent: int = 1):
        self.out.append("    " * indent + line)

    def fn_for(self, target: int) -> str:
        nm = self.ctx.names.get(target)
        if nm is None:
            raise KeyError(f"no entry function for {target:#x} (from unit {self.u.name})")
        return nm

    def goto_or_tail(self, target: int) -> str:
        if target in self.u.insns:
            return f"goto L_{target:08X};"
        return f"TAIL({self.fn_for(target)});"

    # ── per-instruction ─────────────────────────────────────────────────
    def comment(self, ins: Insn) -> str:
        src = self.info.source_line(ins.addr)
        txt = src if src else ins.text
        txt = txt.replace("*/", "* /")
        return f"/* {ins.addr:08X}: {txt} */"

    def body(self) -> list[str]:
        u = self.u
        addrs = sorted(u.insns)
        skip_next = False
        for k, a in enumerate(addrs):
            ins = u.insns[a]
            if a in u.labels:
                self.out.append(f"L_{a:08X}:;")
            if skip_next:
                skip_next = False
                self.emit(self.comment(ins))
                continue
            self.emit(self.comment(ins))
            nxt = u.insns.get(a + ins.size)
            fused = self.try_fuse(ins, nxt)
            if fused:
                for ln in fused:
                    self.emit(ln)
                skip_next = True
                # the fused branch is emitted; continue with fallthrough of nxt
                if nxt.addr + nxt.size not in u.insns and nxt.cond != "al":
                    self.emit(f"TAIL({self.fn_for(nxt.addr + nxt.size)});")
                continue
            lines = self.insn(ins)
            if ins.cond not in ("al",) and not ins.thumb and ins.kind != "b":
                self.emit(f"if ({COND_EXPR[ins.cond]}) {{")
                for ln in lines:
                    self.emit(ln, 2)
                self.emit("}")
            else:
                for ln in lines:
                    self.emit(ln)
            # fall off the end of the unit's decoded code
            if self.falls_through(ins) and a + ins.size not in u.insns:
                self.emit(f"TAIL({self.fn_for(a + ins.size)});")
        return self.out

    def falls_through(self, ins: Insn) -> bool:
        if ins.kind == "undef":
            return False
        if ins.kind == "b" and not ins.link:
            return ins.cond != "al"
        if writes_pc(ins) and not (ins.kind == "b" and ins.link):
            ret = self.call_ret(ins) if ins.kind in ("bx", "dp") else None
            if ret is not None:
                return (ret & ~1) == ins.addr + ins.size or ins.cond != "al"
            return ins.cond != "al"
        return True

    # Compare-and-branch fusion ------------------------------------------
    def try_fuse(self, ins: Insn, nxt: Insn | None) -> list[str] | None:
        if nxt is None or nxt.kind != "b" or nxt.link or nxt.cond == "al" or not ins.thumb:
            return None
        if ins.kind != "dp" or not ins.s or ins.rd == 15:
            return None
        if nxt.addr in self.u.labels:
            return None
        r, w = flag_effects(ins)
        if r:
            return None
        if self.live_out.get(nxt.addr, ALLF) & w:
            return None
        cond = nxt.cond
        op = ins.op
        o = ins.op2
        if op in ("add", "sub", "cmp", "cmn", "adc", "sbc", "rsb") and op not in ("add", "sub", "cmp", "cmn"):
            return None
        if o.rs is not None:
            return None
        if not o.is_imm and o.shift != "lsl":
            return None
        if not o.is_imm and o.amount != 0 and op not in ("mov",):
            return None
        b, _c = self.op2(ins, False)
        a = self.reg(ins.rn, ins)
        lines: list[str] = []
        res = None
        if op in ("cmp", "sub"):
            sig = {"eq": "a == b", "ne": "a != b", "cs": "a >= b", "cc": "a < b",
                   "hi": "a > b", "ls": "a <= b", "ge": "(s32)a >= (s32)b",
                   "lt": "(s32)a < (s32)b", "gt": "(s32)a > (s32)b", "le": "(s32)a <= (s32)b",
                   "mi": "(s32)(a - b) < 0", "pl": "(s32)(a - b) >= 0"}.get(cond)
            if sig is None:
                return None
            if op == "cmp":
                expr = sig.replace("a", "\x00").replace("b", b).replace("\x00", a)
                lines.append(f"if ({expr}) {self.goto_or_tail(nxt.target)}")
                return lines
            # subs rd, rn, op2
            lines.append(f"{{ u32 a = {a}, b = {b}; {REGN[ins.rd]} = a - b;")
            lines.append(f"  if ({sig}) {self.goto_or_tail(nxt.target)} }}")
            return lines
        if op in ("cmn", "add"):
            sig = {"eq": "(u32)(a + b) == 0", "ne": "(u32)(a + b) != 0",
                   "mi": "(s32)(a + b) < 0", "pl": "(s32)(a + b) >= 0",
                   "cs": "(u32)(a + b) < a", "cc": "(u32)(a + b) >= a"}.get(cond)
            if sig is None:
                return None
            if op == "cmn":
                lines.append(f"{{ u32 a = {a}, b = {b}; if ({sig}) {self.goto_or_tail(nxt.target)} }}")
                return lines
            lines.append(f"{{ u32 a = {a}, b = {b}; {REGN[ins.rd]} = a + b;")
            lines.append(f"  if ({sig}) {self.goto_or_tail(nxt.target)} }}")
            return lines
        if op in ("and", "tst", "orr", "eor", "bic", "mov", "mvn"):
            if cond not in ("eq", "ne", "mi", "pl"):
                return None
            if not o.is_imm and o.amount != 0:
                # shifted mov: value is expressible but shifter C may matter
                pass
            val = {"and": f"{a} & {b}", "tst": f"{a} & {b}", "orr": f"{a} | {b}",
                   "eor": f"{a} ^ {b}", "bic": f"{a} & ~{b}", "mov": b, "mvn": f"~{b}"}[op]
            if op != "tst":
                lines.append(f"{REGN[ins.rd]} = {val};")
                res = REGN[ins.rd]
            else:
                res = f"({val})"
            test = {"eq": f"{res} == 0", "ne": f"{res} != 0",
                    "mi": f"(s32){res} < 0", "pl": f"(s32){res} >= 0"}[cond]
            lines.append(f"if ({test}) {self.goto_or_tail(nxt.target)}")
            return lines
        return None

    # Calls ---------------------------------------------------------------
    def call_ret(self, ins: Insn) -> int | None:
        """For `bx rN`/`mov pc, rN` preceded by an LR setup: return address."""
        if ins.kind == "bx" and ins.rm == 14:
            return None
        if ins.kind == "dp" and not (ins.op == "mov" and ins.rd == 15):
            return None
        return self.reg_const_before(ins, 14)

    def reg_const_before(self, ins: Insn, reg: int, depth: int = 0) -> int | None:
        """Constant value of `reg` just before `ins`, from a short straight-line
        backward scan (no intervening labels or control flow)."""
        u = self.u
        a = ins.addr
        step = 2 if ins.thumb else 4
        for _ in range(8):
            if a in self.p.targets or a in self.p.entries:
                return None     # another path may reach here with other values
            a -= step
            prev = u.insns.get(a)
            if prev is None or prev.size != step:
                return None
            if prev.kind in ("b", "bx", "swi") or writes_pc(prev):
                return None
            if prev.kind == "block" and prev.load and reg in prev.regs:
                return None
            if prev.kind == "block" and prev.wb and prev.rn == reg:
                return None
            if prev.kind == "dp" and prev.rd == reg and prev.op not in ("tst", "teq", "cmp", "cmn"):
                o = prev.op2
                if prev.op == "mov" and o and not o.is_imm and o.rs is None and o.amount == 0 \
                        and o.shift == "lsl":
                    if o.rm == 15:
                        return prev.addr + (4 if prev.thumb else 8)
                    if depth < 2:
                        return self.reg_const_before(prev, o.rm, depth + 1)
                    return None
                if prev.op == "add" and prev.rn == 15 and o.is_imm and not prev.thumb:
                    return prev.addr + 8 + o.imm
                if prev.op == "sub" and prev.rn == 15 and o.is_imm and not prev.thumb:
                    return prev.addr + 8 - o.imm
                if prev.op == "adr":
                    return ((prev.addr + 4) & ~3) + o.imm
                if prev.op == "mov" and o.is_imm:
                    return o.imm
                return None
            if prev.kind == "mem" and prev.load and prev.rd == reg:
                lit = self.p.literal(prev)
                return lit
            if prev.kind == "mem" and (prev.wb or not prev.pre) and prev.rn == reg:
                return None
            if prev.kind in ("mul", "mull", "mrs", "swp") and (prev.rd == reg or
                                                             (prev.kind == "mull" and prev.rdhi == reg)):
                return None
        return None

    def is_pop_return(self, ins: Insn) -> bool:
        """`pop {rX}; bx rX` / `ldr rX, [sp], #4; bx rX` interworking return."""
        prev = self.u.insns.get(ins.addr - (2 if ins.thumb else 4))
        if prev is None:
            return False
        # A `bx rN` that is also reached directly (e.g. `bl` to a lone
        # `bx r3` used as a call-through-register veneer) is not a return.
        if ins.addr in self.p.targets or ins.addr in self.p.entries or ins.addr in self.p.data_refs:
            return False
        if prev.kind == "block" and prev.load and prev.rn == 13 and ins.rm in prev.regs:
            return True
        if prev.kind == "mem" and prev.load and prev.rn == 13 and prev.rd == ins.rm:
            return True
        # pop {r4-r7}; pop {r0}; bx r0 (two pops) is covered by the first check
        return False

    def emit_call(self, target_expr: str, direct: str | None, ret: int, thumb_lr: bool) -> list[str]:
        lines = []
        if direct:
            lines.append(f"CALL({direct}, {h(ret)});" if thumb_lr else f"CALL_A({direct}, {h(ret)});")
        else:
            lines.append(f"CALLX({target_expr}, {h(ret)});")
        return lines

    def jump_indirect(self, ins: Insn, expr: str, lines: list[str]):
        """Unknown-target jump: local switch over data-referenced labels."""
        cands = sorted(t for t in self.u.labels if t in self.p.data_refs and t in self.u.insns)
        if cands:
            lines.append(f"switch ({expr} & ~1u) {{")
            for t in cands:
                lines.append(f"    case {h(t)}: goto L_{t:08X};")
            lines.append(f"    default: JUMP({expr});")
            lines.append("}")
        else:
            lines.append(f"JUMP({expr});")

    def insn(self, ins: Insn) -> list[str]:
        k = ins.kind
        L: list[str] = []
        live = self.live_out.get(ins.addr, ALLF)
        if k == "undef":
            return [f"UNDEFINED({h(ins.addr)});"]
        if k == "b":
            if ins.link:
                ret = ins.addr + ins.size
                L += self.emit_call("", self.fn_for(ins.target), ret, ins.thumb)
                return L
            if ins.cond != "al":
                return [f"if ({COND_EXPR[ins.cond]}) {self.goto_or_tail(ins.target)}"]
            return [self.goto_or_tail(ins.target)]
        if k == "swi":
            return [f"SWI({h(ins.imm)});"]
        if k == "bx":
            return self.control_to(ins, self.reg(ins.rm, ins), ins.rm)
        if k == "dp":
            return self.dp(ins, live)
        if k == "mem":
            return self.mem(ins)
        if k == "block":
            return self.block(ins)
        if k == "mul":
            e = f"{self.reg(ins.rm, ins)} * {self.reg(ins.rs, ins)}"
            if ins.acc:
                e += f" + {self.reg(ins.rn, ins)}"
            L.append(f"{REGN[ins.rd]} = {e};")
            if ins.s and live & NZ:
                L.append(f"nz = {REGN[ins.rd]};")
            return L
        if k == "mull":
            if ins.signed:
                e = f"(u64)((s64)(s32){self.reg(ins.rm, ins)} * (s32){self.reg(ins.rs, ins)})"
            else:
                e = f"(u64){self.reg(ins.rm, ins)} * {self.reg(ins.rs, ins)}"
            lo, hi = REGN[ins.rd], REGN[ins.rdhi]
            acc = f" + (((u64){hi} << 32) | {lo})" if ins.acc else ""
            L.append(f"{{ u64 r = {e}{acc}; {lo} = (u32)r; {hi} = (u32)(r >> 32);")
            if ins.s and live & NZ:
                L.append(f"  nz = {hi} | ({lo} != 0); }}")
            else:
                L[-1] += " }"
            return L
        if k == "swp":
            ld = "LD8" if ins.width == 1 else "LD32"
            st = "ST8" if ins.width == 1 else "ST32"
            n = REGN[ins.rn]
            return [f"{{ u32 t = {ld}({n}); {st}({n}, {REGN[ins.rm]}); {REGN[ins.rd]} = t; }}"]
        if k == "mrs":
            if ins.spsr:
                return [f"{REGN[ins.rd]} = cpu_get_spsr();"]
            return [f"SAVE; {REGN[ins.rd]} = cpu_get_cpsr();"]
        if k == "msr":
            v, _ = self.op2(ins, False)
            if ins.spsr:
                return [f"cpu_set_spsr({v}, {h(ins.mask)});"]
            return [f"{{ u32 x_ = {v}; SAVE; cpu_set_cpsr(x_, {h(ins.mask)}); LOAD; }}"]
        raise ValueError(k)

    def control_to(self, ins: Insn, expr: str, srcreg: int) -> list[str]:
        """A write of `expr` to PC (bx / mov pc / ldr pc / pop pc)."""
        L: list[str] = []
        if srcreg == 14:
            return ["RETURN(lr);"]
        ct = self.p.const_targets.get(ins.addr)
        ret = self.call_ret(ins)
        if ret is not None:
            ret &= ~1
            if ct is not None:
                L += self.emit_call(expr, self.fn_for(ct & ~1), ret, False)
            else:
                L += self.emit_call(expr, None, ret, False)
            nxt = ins.addr + ins.size
            if ret != nxt:
                L.append(self.goto_or_tail(ret))
            return L
        if ct is not None:
            return [self.goto_or_tail(ct & ~1)]
        if ins.kind == "bx" and self.is_pop_return(ins):
            return [f"RETURN({expr});"]
        self.jump_indirect(ins, expr, L)
        return L

    def dp(self, ins: Insn, live: int) -> list[str]:
        L: list[str] = []
        op = ins.op
        rd = REGN[ins.rd]
        if op == "adr":
            v = ((ins.addr + 4) & ~3) + ins.op2.imm
            return [f"{rd} = {self.addr_const(v)};"]
        s = ins.s and ins.rd != 15
        nzl = s and bool(live & NZ)
        cl = s and bool(live & C)
        vl = s and bool(live & V)
        logical = op in ("and", "eor", "orr", "bic", "mov", "mvn", "tst", "teq")
        b, carry = self.op2(ins, want_carry=logical and cl)
        a = self.reg(ins.rn, ins, regshift=ins.op2.rs is not None) if op not in ("mov", "mvn") else ""
        if ins.rn == 15 and op in ("add", "sub") and ins.op2.is_imm and op not in ("mov", "mvn"):
            # PC-relative address arithmetic (ARM adr)
            if ins.rd != 15:
                v = (ins.addr + 8 + (ins.op2.imm if op == "add" else -ins.op2.imm)) if not ins.thumb else None
                if v is not None:
                    return [f"{rd} = {self.addr_const(v)};"]
        pre = []
        if carry == "__inline__":
            pass  # *_REG_C helpers update c themselves
        elif carry is not None:
            pre.append(f"c = {carry};")
        if logical:
            val = {"and": f"{a} & {b}", "eor": f"{a} ^ {b}", "orr": f"{a} | {b}",
                   "bic": f"{a} & ~{b}", "mov": b, "mvn": f"~{b}",
                   "tst": f"{a} & {b}", "teq": f"{a} ^ {b}"}[op]
            if op in ("tst", "teq"):
                if carry and carry != "__inline__":
                    L += pre
                if nzl:
                    L.append(f"nz = {val};")
                elif carry == "__inline__":
                    L.append(f"(void)({val});")
                return L
            if ins.rd == 15:
                return self.pc_write(ins, val)
            if carry and carry != "__inline__":
                # carry must be computed from the source before rd is written
                L.append(f"{{ u32 t = {val}; {' '.join(pre)} {rd} = t; }}")
            else:
                L.append(f"{rd} = {val};")
            if nzl:
                L.append(f"nz = {rd};")
            return L
        # arithmetic
        if op in ("cmp", "cmn"):
            fn = "SUBS" if op == "cmp" else "ADDS"
            if cl or vl:
                L.append(f"nz = {fn}({a}, {b});")
            elif nzl:
                L.append(f"nz = {a} {'-' if op == 'cmp' else '+'} {b};")
            return L
        if ins.rd == 15:
            val = {"add": f"{a} + {b}", "sub": f"{a} - {b}", "rsb": f"{b} - {a}",
                   "adc": f"{a} + {b} + c", "sbc": f"{a} - {b} - (c ^ 1)",
                   "rsc": f"{b} - {a} - (c ^ 1)"}[op]
            return self.pc_write(ins, val)
        if cl or vl:
            fn = {"add": f"ADDS({a}, {b})", "sub": f"SUBS({a}, {b})", "rsb": f"SUBS({b}, {a})",
                  "adc": f"ADCS({a}, {b})", "sbc": f"SBCS({a}, {b})", "rsc": f"SBCS({b}, {a})"}[op]
            L.append(f"{rd} = {fn};")
        else:
            val = {"add": f"{a} + {b}", "sub": f"{a} - {b}", "rsb": f"{b} - {a}",
                   "adc": f"{a} + {b} + c", "sbc": f"{a} - {b} - (c ^ 1)",
                   "rsc": f"{b} - {a} - (c ^ 1)"}[op]
            if op == "add" and b == "0":
                val = a
            L.append(f"{rd} = {val};")
        if nzl:
            L.append(f"nz = {rd};")
        return L

    def pc_write(self, ins: Insn, val: str) -> list[str]:
        if ins.s:
            return [f"EXCEPTION_RETURN({val});"]
        o = ins.op2
        if ins.op == "mov" and o and not o.is_imm and o.rs is None and o.amount == 0 and o.shift == "lsl":
            return self.control_to(ins, self.reg(o.rm, ins), o.rm)
        L = [f"{{ u32 t = {val};"]
        inner: list[str] = []
        self.jump_indirect(ins, "t", inner)
        L += ["  " + x for x in inner]
        L.append("}")
        return L

    def mem(self, ins: Insn) -> list[str]:
        L: list[str] = []
        rd = REGN[ins.rd]
        lit = self.p.literal(ins)
        if lit is not None:
            if ins.rd == 15:
                return [self.goto_or_tail(lit & ~1)] if (lit & ~1) in self.ctx.names else [f"JUMP({h(lit)});"]
            return [f"{rd} = {self.addr_const(lit, mode_bit=True)};"]
        base = self.reg(ins.rn, ins)
        if ins.rn == 15 and ins.thumb:
            base = h((ins.addr + 4) & ~3)
        o = ins.op2
        if o.is_imm:
            off = self.imm(o.imm, ins)
            offv = o.imm
        else:
            off, _ = self.op2(ins, False)
            offv = None
        sign = "+" if ins.up else "-"
        if offv == 0:
            ea_pre = base
        else:
            ea_pre = f"{base} {sign} {off}"
        ea = ea_pre if ins.pre else base
        if ins.load:
            fn = {(4, False): "LD32", (2, False): "LD16", (1, False): "LD8",
                  (2, True): "LDS16", (1, True): "LDS8"}[(ins.width, ins.signed)]
            load = f"{fn}({ea})"
            if ins.rd == 15:
                L.append(f"{{ u32 t = {load};")
                if ins.wb:
                    L.append(f"  {REGN[ins.rn]} = {ea_pre};")
                inner = self.control_to(ins, "t", -1) if not (ins.rn == 13 and not ins.pre) \
                    else ["RETURN(t);"]
                L += ["  " + x for x in inner]
                L.append("}")
                return L
            if ins.wb and ins.rn != ins.rd:
                if ins.pre:
                    L.append(f"{{ u32 a = {ea_pre}; {rd} = {fn}(a); {REGN[ins.rn]} = a; }}")
                else:
                    L.append(f"{rd} = {load}; {REGN[ins.rn]} = {ea_pre};")
            else:
                L.append(f"{rd} = {load};")
            return L
        fn = {4: "ST32", 2: "ST16", 1: "ST8"}[ins.width]
        val = self.reg(ins.rd, ins) if ins.rd != 15 else h(ins.addr + 12)
        if ins.wb:
            if ins.pre:
                # update the base first so an IRQ never sees data below SP
                L.append(f"{{ u32 a = {ea_pre}; {REGN[ins.rn]} = a; {fn}(a, {val}); }}")
            else:
                L.append(f"{fn}({base}, {val}); {REGN[ins.rn]} = {ea_pre};")
        else:
            L.append(f"{fn}({ea}, {val});")
        return L

    def block(self, ins: Insn) -> list[str]:
        L: list[str] = []
        regs = ins.regs
        n = len(regs)
        rn = REGN[ins.rn]
        if n == 0:
            return ["/* empty register list */"]
        if ins.mode == "ia":
            offs = [4 * i for i in range(n)]
            wbv = 4 * n
        elif ins.mode == "ib":
            offs = [4 * (i + 1) for i in range(n)]
            wbv = 4 * n
        elif ins.mode == "da":
            offs = [-4 * n + 4 + 4 * i for i in range(n)]
            wbv = -4 * n
        else:  # db
            offs = [-4 * n + 4 * i for i in range(n)]
            wbv = -4 * n
        if ins.user:
            L.append("/* ^ user-bank transfer treated as normal */")
        if not ins.load:
            if ins.wb and wbv < 0:
                # push: move SP down first (IRQ safety), then store relative to new base
                L.append(f"{{ u32 a = {rn} - {-wbv}; {rn} = a;")
                for r, o in zip(regs, offs):
                    oo = o - wbv
                    val = self.reg(r, ins) if r != 15 else h(ins.addr + (4 if ins.thumb else 12))
                    L.append(f"  ST32(a{' + ' + str(oo) if oo else ''}, {val});")
                L.append("}")
                return L
            L.append(f"{{ u32 a = {rn};")
            for r, o in zip(regs, offs):
                val = self.reg(r, ins) if r != 15 else h(ins.addr + 12)
                L.append(f"  ST32(a{' + ' + str(o) if o > 0 else (' - ' + str(-o) if o < 0 else '')}, {val});")
            if ins.wb:
                L.append(f"  {rn} = a {'+' if wbv > 0 else '-'} {abs(wbv)};")
            L.append("}")
            return L
        # load
        L.append(f"{{ u32 a = {rn};")
        pcload = 15 in regs
        for r, o in zip(regs, offs):
            src = f"LD32(a{' + ' + str(o) if o > 0 else (' - ' + str(-o) if o < 0 else '')})"
            if r == 15:
                L.append(f"  u32 t = {src};")
            else:
                L.append(f"  {REGN[r]} = {src};")
        if ins.wb and ins.rn not in regs:
            L.append(f"  {rn} = a {'+' if wbv > 0 else '-'} {abs(wbv)};")
        if pcload:
            if ins.user:
                L.append("  EXCEPTION_RETURN(t);")
            else:
                L.append("  RETURN(t);")
        L.append("}")
        return L
