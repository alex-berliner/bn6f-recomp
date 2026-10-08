"""ARMv4T (ARM7TDMI) instruction decoding into a small shared IR.

Both THUMB and ARM decode into `Insn`. THUMB forms are expressed as their ARM
equivalents (e.g. `lsl r0, r1` -> `movs r0, r0, lsl r1`) so one emitter
covers both instruction sets; only PC-read semantics differ and those are
resolved here (`pc_val`).
"""
from __future__ import annotations

from dataclasses import dataclass, field

COND = ["eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
        "hi", "ls", "ge", "lt", "gt", "le", "al", "nv"]
DP_OPS = ["and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc",
          "tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn"]
SHIFTS = ["lsl", "lsr", "asr", "ror"]


@dataclass
class Op2:
    """Second operand: immediate, or register with shift."""
    imm: int | None = None          # immediate value (already rotated)
    imm_carry: int | None = None    # carry-out of a rotated immediate (None = unchanged)
    rm: int = 0
    shift: str = "lsl"              # lsl/lsr/asr/ror/rrx
    amount: int = 0                 # immediate shift amount (after ARM 0->32 fixups)
    rs: int | None = None           # register shift amount

    @property
    def is_imm(self) -> bool:
        return self.imm is not None


@dataclass
class Insn:
    addr: int
    size: int
    thumb: bool
    kind: str                     # dp mul mull mem block b bx swi mrs msr swp undef
    cond: str = "al"
    op: str = ""                  # dp opcode / mem 'ldr'|'str' / etc
    rd: int = 0
    rn: int = 0
    op2: Op2 | None = None
    s: bool = False               # sets flags
    # mul
    rm: int = 0
    rs: int = 0
    acc: bool = False
    signed: bool = False          # mull / ldrs*
    rdhi: int = 0
    # mem
    width: int = 4                # 1, 2, 4
    load: bool = False
    pre: bool = True
    up: bool = True
    wb: bool = False
    # block
    regs: list[int] = field(default_factory=list)
    mode: str = "ia"              # ia ib da db
    user: bool = False            # ^ (S bit) on ldm/stm
    # branch
    target: int = 0
    link: bool = False
    # swi / msr
    imm: int = 0
    spsr: bool = False
    mask: int = 0                 # msr field mask (f=8 s=4 x=2 c=1)
    raw: int = 0
    text: str = ""                # short disassembly for comments

    def pc_val(self) -> int:
        """Value read from r15 by this instruction (pipeline offset applied)."""
        return self.addr + (4 if self.thumb else 8)

    def reads_reg_shift_pc(self) -> int:
        # ARM data-processing with register-specified shift reads PC as +12.
        return self.addr + 12


def _rlist(mask: int) -> list[int]:
    return [i for i in range(16) if mask >> i & 1]


def _ror(v: int, n: int) -> int:
    n &= 31
    return ((v >> n) | (v << (32 - n))) & 0xFFFFFFFF if n else v


# ═══════════════════════════════════════════════════════════════════════
# THUMB
# ═══════════════════════════════════════════════════════════════════════

def decode_thumb(addr: int, hw: int, next_hw: int | None) -> Insn:
    I = lambda kind, **kw: Insn(addr=addr, size=2, thumb=True, kind=kind, raw=hw, **kw)
    top3 = hw >> 13
    if top3 == 0b000:
        op = (hw >> 11) & 3
        if op != 3:  # format 1: shift by immediate
            rd, rs, off = hw & 7, (hw >> 3) & 7, (hw >> 6) & 31
            sh = SHIFTS[op]
            if sh in ("lsr", "asr") and off == 0:
                off = 32
            return I("dp", op="mov", rd=rd, s=True, op2=Op2(rm=rs, shift=sh, amount=off),
                     text=f"{sh}s r{rd}, r{rs}, #{off}")
        # format 2: add/sub
        rd, rs = hw & 7, (hw >> 3) & 7
        imm_flag, sub = (hw >> 10) & 1, (hw >> 9) & 1
        val = (hw >> 6) & 7
        o2 = Op2(imm=val) if imm_flag else Op2(rm=val)
        opn = "sub" if sub else "add"
        return I("dp", op=opn, rd=rd, rn=rs, s=True, op2=o2,
                 text=f"{opn}s r{rd}, r{rs}, {'#%d' % val if imm_flag else 'r%d' % val}")
    if top3 == 0b001:  # format 3
        op, rd, imm = (hw >> 11) & 3, (hw >> 8) & 7, hw & 0xFF
        opn = ["mov", "cmp", "add", "sub"][op]
        return I("dp", op=opn, rd=rd, rn=rd, s=True, op2=Op2(imm=imm), text=f"{opn}s r{rd}, #{imm}")
    if (hw >> 10) == 0b010000:  # format 4: ALU
        op, rs, rd = (hw >> 6) & 15, (hw >> 3) & 7, hw & 7
        name = ["and", "eor", "lsl", "lsr", "asr", "adc", "sbc", "ror",
                "tst", "neg", "cmp", "cmn", "orr", "mul", "bic", "mvn"][op]
        t = f"{name}s r{rd}, r{rs}"
        if name in ("lsl", "lsr", "asr", "ror"):
            return I("dp", op="mov", rd=rd, s=True, op2=Op2(rm=rd, shift=name, rs=rs), text=t)
        if name == "neg":
            return I("dp", op="rsb", rd=rd, rn=rs, s=True, op2=Op2(imm=0), text=t)
        if name == "mul":
            return I("mul", rd=rd, rm=rd, rs=rs, s=True, text=t)
        return I("dp", op=name, rd=rd, rn=rd, s=True, op2=Op2(rm=rs), text=t)
    if (hw >> 10) == 0b010001:  # format 5: hi register ops / bx
        op = (hw >> 8) & 3
        h1, h2 = (hw >> 7) & 1, (hw >> 6) & 1
        rd = (hw & 7) | (h1 << 3)
        rs = ((hw >> 3) & 7) | (h2 << 3)
        if op == 3:
            return I("bx", rm=rs, text=f"bx r{rs}")
        opn = ["add", "cmp", "mov"][op]
        return I("dp", op=opn, rd=rd, rn=rd, s=(opn == "cmp"), op2=Op2(rm=rs), text=f"{opn} r{rd}, r{rs}")
    if (hw >> 11) == 0b01001:  # format 6: ldr rd, [pc, #imm]
        rd, imm = (hw >> 8) & 7, (hw & 0xFF) * 4
        return I("mem", op="ldr", load=True, width=4, rd=rd, rn=15, op2=Op2(imm=imm),
                 text=f"ldr r{rd}, [pc, #{imm}]")
    if (hw >> 12) == 0b0101:  # formats 7/8: register offset
        ro, rb, rd = (hw >> 6) & 7, (hw >> 3) & 7, hw & 7
        if not (hw >> 9) & 1:
            l, b = (hw >> 11) & 1, (hw >> 10) & 1
            name = ["str", "strb", "ldr", "ldrb"][l * 2 + b]
            return I("mem", op="ldr" if l else "str", load=bool(l), width=1 if b else 4,
                     rd=rd, rn=rb, op2=Op2(rm=ro), text=f"{name} r{rd}, [r{rb}, r{ro}]")
        hs = (hw >> 10) & 3
        name = ["strh", "ldsb", "ldrh", "ldsh"][hs]
        if hs == 0:
            return I("mem", op="str", width=2, rd=rd, rn=rb, op2=Op2(rm=ro), text=f"{name} r{rd}, [r{rb}, r{ro}]")
        return I("mem", op="ldr", load=True, width=1 if hs == 1 else 2, signed=hs != 2,
                 rd=rd, rn=rb, op2=Op2(rm=ro), text=f"{name} r{rd}, [r{rb}, r{ro}]")
    if (hw >> 13) == 0b011:  # format 9: imm offset word/byte
        b, l = (hw >> 12) & 1, (hw >> 11) & 1
        off, rb, rd = (hw >> 6) & 31, (hw >> 3) & 7, hw & 7
        off = off if b else off * 4
        name = ["str", "ldr", "strb", "ldrb"][b * 2 + l]
        return I("mem", op="ldr" if l else "str", load=bool(l), width=1 if b else 4,
                 rd=rd, rn=rb, op2=Op2(imm=off), text=f"{name} r{rd}, [r{rb}, #{off}]")
    if (hw >> 12) == 0b1000:  # format 10: halfword imm
        l = (hw >> 11) & 1
        off, rb, rd = ((hw >> 6) & 31) * 2, (hw >> 3) & 7, hw & 7
        return I("mem", op="ldr" if l else "str", load=bool(l), width=2, rd=rd, rn=rb,
                 op2=Op2(imm=off), text=f"{'ldrh' if l else 'strh'} r{rd}, [r{rb}, #{off}]")
    if (hw >> 12) == 0b1001:  # format 11: sp relative
        l, rd, imm = (hw >> 11) & 1, (hw >> 8) & 7, (hw & 0xFF) * 4
        return I("mem", op="ldr" if l else "str", load=bool(l), width=4, rd=rd, rn=13,
                 op2=Op2(imm=imm), text=f"{'ldr' if l else 'str'} r{rd}, [sp, #{imm}]")
    if (hw >> 12) == 0b1010:  # format 12: load address
        sp, rd, imm = (hw >> 11) & 1, (hw >> 8) & 7, (hw & 0xFF) * 4
        if sp:
            return I("dp", op="add", rd=rd, rn=13, op2=Op2(imm=imm), text=f"add r{rd}, sp, #{imm}")
        # add rd, pc, #imm: PC is word-aligned here.
        return I("dp", op="adr", rd=rd, rn=15, op2=Op2(imm=imm), text=f"adr r{rd}, #{imm}")
    if (hw >> 8) == 0b10110000:  # format 13: add sp, #imm
        imm = (hw & 0x7F) * 4
        neg = (hw >> 7) & 1
        return I("dp", op="sub" if neg else "add", rd=13, rn=13, op2=Op2(imm=imm),
                 text=f"{'sub' if neg else 'add'} sp, #{imm}")
    if (hw >> 12) == 0b1011 and ((hw >> 9) & 3) == 0b10:  # format 14: push/pop
        l, r = (hw >> 11) & 1, (hw >> 8) & 1
        regs = _rlist(hw & 0xFF)
        if l:
            if r:
                regs.append(15)
            return I("block", op="ldm", load=True, rn=13, regs=regs, mode="ia", wb=True,
                     text="pop {%s}" % ",".join(f"r{x}" for x in regs))
        if r:
            regs.append(14)
        return I("block", op="stm", rn=13, regs=regs, mode="db", wb=True,
                 text="push {%s}" % ",".join(f"r{x}" for x in regs))
    if (hw >> 12) == 0b1100:  # format 15: ldmia/stmia
        l, rb = (hw >> 11) & 1, (hw >> 8) & 7
        regs = _rlist(hw & 0xFF)
        return I("block", op="ldm" if l else "stm", load=bool(l), rn=rb, regs=regs, mode="ia",
                 wb=not (l and rb in regs),
                 text="%s r%d!, {%s}" % ("ldmia" if l else "stmia", rb, ",".join(f"r{x}" for x in regs)))
    if (hw >> 12) == 0b1101:  # format 16/17
        cond = (hw >> 8) & 15
        if cond == 15:
            return I("swi", imm=hw & 0xFF, text=f"swi {hw & 0xFF:#x}")
        if cond == 14:
            return I("undef", text="undef")
        off = hw & 0xFF
        if off & 0x80:
            off -= 0x100
        tgt = addr + 4 + off * 2
        return I("b", cond=COND[cond], target=tgt, text=f"b{COND[cond]} {tgt:#x}")
    if (hw >> 11) == 0b11100:  # format 18
        off = hw & 0x7FF
        if off & 0x400:
            off -= 0x800
        tgt = addr + 4 + off * 2
        return I("b", target=tgt, text=f"b {tgt:#x}")
    if (hw >> 11) == 0b11110:  # format 19 prefix
        if next_hw is not None and (next_hw >> 11) == 0b11111:
            hi = hw & 0x7FF
            if hi & 0x400:
                hi -= 0x800
            tgt = addr + 4 + (hi << 12) + ((next_hw & 0x7FF) << 1)
            ins = I("b", target=tgt & 0xFFFFFFFF, link=True, text=f"bl {tgt & 0xFFFFFFFF:#x}")
            ins.size = 4
            ins.raw = hw | (next_hw << 16)
            return ins
        return I("undef", text="bl-prefix-alone")
    if (hw >> 11) == 0b11111:
        # Lone BL suffix: pc = lr + off<<1, lr = next|1. Unsupported (unused by
        # compiler-free hand-written code in practice).
        return I("undef", text="bl-suffix-alone")
    return I("undef", text=f"undef {hw:#06x}")


# ═══════════════════════════════════════════════════════════════════════
# ARM
# ═══════════════════════════════════════════════════════════════════════

def _arm_op2_shifted(w: int) -> Op2:
    rm = w & 15
    sh = SHIFTS[(w >> 5) & 3]
    if (w >> 4) & 1:
        return Op2(rm=rm, shift=sh, rs=(w >> 8) & 15)
    amt = (w >> 7) & 31
    if amt == 0:
        if sh in ("lsr", "asr"):
            amt = 32
        elif sh == "ror":
            sh = "rrx"
            amt = 1
    return Op2(rm=rm, shift=sh, amount=amt)


def decode_arm(addr: int, w: int) -> Insn:
    cond = COND[w >> 28]
    I = lambda kind, **kw: Insn(addr=addr, size=4, thumb=False, kind=kind, cond=cond, raw=w, **kw)
    if cond == "nv":
        return I("undef", text="nv")
    if (w & 0x0FFFFFF0) == 0x012FFF10:
        return I("bx", rm=w & 15, text=f"bx{cond} r{w & 15}")
    if (w & 0x0E000000) == 0x0A000000:
        off = w & 0xFFFFFF
        if off & 0x800000:
            off -= 0x1000000
        tgt = (addr + 8 + off * 4) & 0xFFFFFFFF
        link = bool(w >> 24 & 1)
        return I("b", target=tgt, link=link, text=f"b{'l' if link else ''}{cond} {tgt:#x}")
    if (w & 0x0F000000) == 0x0F000000:
        return I("swi", imm=(w >> 16) & 0xFF, text=f"swi{cond} {(w >> 16) & 0xFF:#x}")
    if (w & 0x0FC000F0) == 0x00000090:  # mul/mla
        rd, rn, rs, rm = (w >> 16) & 15, (w >> 12) & 15, (w >> 8) & 15, w & 15
        acc, s = bool(w >> 21 & 1), bool(w >> 20 & 1)
        return I("mul", rd=rd, rn=rn, rs=rs, rm=rm, acc=acc, s=s, text=f"{'mla' if acc else 'mul'}{cond}")
    if (w & 0x0F8000F0) == 0x00800090:  # long multiply
        rdhi, rdlo, rs, rm = (w >> 16) & 15, (w >> 12) & 15, (w >> 8) & 15, w & 15
        signed, acc, s = bool(w >> 22 & 1), bool(w >> 21 & 1), bool(w >> 20 & 1)
        return I("mull", rd=rdlo, rdhi=rdhi, rs=rs, rm=rm, signed=signed, acc=acc, s=s,
                 text=f"{'s' if signed else 'u'}{'mlal' if acc else 'mull'}{cond}")
    if (w & 0x0FB00FF0) == 0x01000090:  # swp
        return I("swp", rd=(w >> 12) & 15, rn=(w >> 16) & 15, rm=w & 15,
                 width=1 if w >> 22 & 1 else 4, text=f"swp{cond}")
    if (w & 0x0E000090) == 0x00000090 and (w & 0x60):  # halfword / signed transfers
        p, u, i, wb, l = (w >> 24) & 1, (w >> 23) & 1, (w >> 22) & 1, (w >> 21) & 1, (w >> 20) & 1
        rn, rd = (w >> 16) & 15, (w >> 12) & 15
        sh = (w >> 5) & 3
        o2 = Op2(imm=((w >> 4) & 0xF0) | (w & 15)) if i else Op2(rm=w & 15)
        width, signed = {1: (2, False), 2: (1, True), 3: (2, True)}[sh]
        return I("mem", op="ldr" if l else "str", load=bool(l), width=width, signed=signed and bool(l),
                 rd=rd, rn=rn, op2=o2, pre=bool(p), up=bool(u), wb=bool(wb) or not p,
                 text=f"{'ldr' if l else 'str'}{'s' if signed else ''}{'h' if width == 2 else 'b'}{cond}")
    if (w & 0x0FBF0FFF) == 0x010F0000:  # mrs
        return I("mrs", rd=(w >> 12) & 15, spsr=bool(w >> 22 & 1), text=f"mrs{cond}")
    if (w & 0x0DB0F000) == 0x0120F000:  # msr
        mask = (w >> 16) & 15
        spsr = bool(w >> 22 & 1)
        if w >> 25 & 1:
            o2 = Op2(imm=_ror(w & 0xFF, ((w >> 8) & 15) * 2))
        else:
            o2 = Op2(rm=w & 15)
        return I("msr", spsr=spsr, mask=mask, op2=o2, text=f"msr{cond}")
    if (w & 0x0C000000) == 0x00000000:  # data processing
        op = DP_OPS[(w >> 21) & 15]
        s = bool(w >> 20 & 1)
        rn, rd = (w >> 16) & 15, (w >> 12) & 15
        if w >> 25 & 1:
            rot = ((w >> 8) & 15) * 2
            val = _ror(w & 0xFF, rot)
            o2 = Op2(imm=val, imm_carry=(val >> 31) if rot else None)
        else:
            o2 = _arm_op2_shifted(w)
        if op in ("tst", "teq", "cmp", "cmn") and not s:
            return I("undef", text=f"dp-no-s {w:#010x}")
        return I("dp", op=op, rd=rd, rn=rn, s=s, op2=o2, text=f"{op}{cond}{'s' if s else ''}")
    if (w & 0x0C000000) == 0x04000000:  # ldr/str
        if (w & 0x02000010) == 0x02000010:
            return I("undef", text="undef-media")
        i, p, u, b, wb, l = [(w >> k) & 1 for k in (25, 24, 23, 22, 21, 20)]
        rn, rd = (w >> 16) & 15, (w >> 12) & 15
        o2 = _arm_op2_shifted(w) if i else Op2(imm=w & 0xFFF)
        return I("mem", op="ldr" if l else "str", load=bool(l), width=1 if b else 4,
                 rd=rd, rn=rn, op2=o2, pre=bool(p), up=bool(u), wb=bool(wb) or not p,
                 text=f"{'ldr' if l else 'str'}{cond}{'b' if b else ''}")
    if (w & 0x0E000000) == 0x08000000:  # ldm/stm
        p, u, s, wb, l = [(w >> k) & 1 for k in (24, 23, 22, 21, 20)]
        mode = {(0, 1): "ia", (1, 1): "ib", (0, 0): "da", (1, 0): "db"}[(p, u)]
        return I("block", op="ldm" if l else "stm", load=bool(l), rn=(w >> 16) & 15,
                 regs=_rlist(w & 0xFFFF), mode=mode, wb=bool(wb), user=bool(s),
                 text=f"{'ldm' if l else 'stm'}{cond}{mode}")
    return I("undef", text=f"undef {w:#010x}")
