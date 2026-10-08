"""Code discovery, function units, CFG and flag liveness for the lifter."""
from __future__ import annotations

import bisect
from collections import defaultdict
from dataclasses import dataclass, field

from .elfinfo import ElfInfo
from .isa import Insn, decode_arm, decode_thumb

# Flag groups tracked for liveness. N and Z always come from the same result
# on ARMv4T, so they share one storage word (cpu.nz) and one liveness bit.
NZ, C, V = 1, 2, 4
ALLF = NZ | C | V

COND_READS = {
    "eq": NZ, "ne": NZ, "mi": NZ, "pl": NZ,
    "cs": C, "cc": C, "vs": V, "vc": V,
    "hi": NZ | C, "ls": NZ | C,
    "ge": NZ | V, "lt": NZ | V, "gt": NZ | V, "le": NZ | V,
    "al": 0,
}


def writes_pc(i: Insn) -> bool:
    if i.kind == "dp":
        return i.rd == 15 and i.op not in ("tst", "teq", "cmp", "cmn")
    if i.kind == "mem":
        return i.load and i.rd == 15
    if i.kind == "block":
        return i.load and 15 in i.regs
    if i.kind in ("bx",):
        return True
    if i.kind == "b":
        return True
    return False


def flag_effects(i: Insn) -> tuple[int, int]:
    """(reads, writes) flag groups of an instruction, excluding its condition."""
    reads = 0
    writes = 0
    if i.kind == "dp":
        o2 = i.op2
        if i.op in ("adc", "sbc", "rsc"):
            reads |= C
        if o2 is not None and not o2.is_imm and o2.shift == "rrx":
            reads |= C
        if i.s and i.rd != 15:
            writes |= NZ
            if i.op in ("add", "sub", "rsb", "adc", "sbc", "rsc", "cmp", "cmn"):
                writes |= C | V
            else:
                # Logical ops: C comes from the shifter. Shifts by 0 and plain
                # immediates (THUMB mov/ARM unrotated) leave C unchanged, which
                # we model as read+write so the old value is carried through.
                if o2 is None:
                    pass
                elif o2.is_imm:
                    if o2.imm_carry is not None:
                        writes |= C
                elif o2.rs is not None:
                    writes |= C
                    reads |= C      # amount may be 0 at runtime -> C kept
                elif o2.shift == "lsl" and o2.amount == 0:
                    pass
                else:
                    writes |= C
        elif i.s and i.rd == 15:
            reads |= ALLF       # movs pc, lr: CPSR <- SPSR (exception return)
    elif i.kind in ("mul", "mull"):
        if i.s:
            writes |= NZ
    elif i.kind == "msr":
        if not i.spsr and i.mask & 8:
            writes |= ALLF
    elif i.kind == "mrs":
        if not i.spsr:
            reads |= ALLF
    # SWIs return through the BIOS with the caller's CPSR restored: no effect.
    reads |= COND_READS.get(i.cond, 0)
    return reads, writes


@dataclass
class Unit:
    start: int
    end: int
    thumb: bool
    name: str
    insns: dict[int, Insn] = field(default_factory=dict)
    entries: set[int] = field(default_factory=set)
    labels: set[int] = field(default_factory=set)      # intra-unit jump targets
    resume: set[int] = field(default_factory=set)      # possible return-to points
    # liveness results: flags live *after* each instruction executes
    live_out: dict[int, int] = field(default_factory=dict)
    flags_in: int = ALLF        # flags read on entry before being written
    jt_targets: dict[int, list[int]] = field(default_factory=dict)  # indirect site -> local targets
    callers_live: int = 0       # flags live after any return from this unit


class Program:
    def __init__(self, info: ElfInfo):
        self.info = info
        self.insns: dict[tuple[int, bool], Insn] = {}
        self.entries: dict[int, bool] = {}            # addr -> thumb
        self.units: list[Unit] = []
        self.unit_starts: list[int] = []
        self.data_refs: set[int] = set()              # addrs referenced as data code pointers
        self.const_targets: dict[int, int] = {}       # indirect branch site -> constant target (with mode bit)
        self.warnings: list[str] = []
        self.call_returns: set[int] = set()
        self._reached: set[tuple[int, bool]] = set()
        self.targets: set[int] = set()     # every direct branch/call target

    def unit_at(self, addr: int) -> "Unit | None":
        k = bisect.bisect_right(self.unit_starts, addr) - 1
        if k < 0:
            return None
        u = self.units[k]
        return u if u.start <= addr < u.end else None

    # ── decoding ────────────────────────────────────────────────────────
    def decode(self, addr: int, thumb: bool) -> Insn | None:
        key = (addr, thumb)
        ins = self.insns.get(key)
        if ins is not None:
            return ins
        if thumb:
            hw = self.info.u16(addr)
            if hw is None:
                return None
            ins = decode_thumb(addr, hw, self.info.u16(addr + 2))
        else:
            w = self.info.u32(addr)
            if w is None:
                return None
            ins = decode_arm(addr, w)
        self.insns[key] = ins
        return ins

    def literal(self, ins: Insn) -> int | None:
        """Constant loaded by a PC-relative LDR from read-only memory."""
        if ins.kind != "mem" or not ins.load or ins.rn != 15 or not ins.op2.is_imm:
            return None
        if ins.thumb:
            base = ins.pc_val() & ~3
        else:
            base = ins.pc_val()
        if not ins.pre or ins.wb:
            return None
        a = base + ins.op2.imm if ins.up or ins.thumb else base - ins.op2.imm
        if ins.width != 4 or a & 3:
            return None
        if not self.info.is_readonly(a):
            # iwram_text literal pools are immutable copies of ROM bytes too.
            if not (0x03005B00 <= a < 0x03007B00):
                return None
        return self.info.u32(a)

    def successors(self, ins: Insn) -> list[tuple[int, str]]:
        """Intra-procedural successors: (addr, kind) kind in fall/branch."""
        out = []
        nxt = ins.addr + ins.size
        if ins.kind == "undef":
            return out
        if ins.kind == "b":
            if ins.link:
                out.append((ins.target, "call"))
                out.append((nxt, "fall"))
            else:
                out.append((ins.target, "branch"))
                if ins.cond != "al":
                    out.append((nxt, "fall"))
            return out
        if writes_pc(ins):
            if ins.cond != "al":
                out.append((nxt, "fall"))
            return out
        out.append((nxt, "fall"))
        return out

    def discover(self, seeds: list[tuple[int, bool]]):
        work = list(seeds)
        seen: set[tuple[int, bool]] = set()
        for a, t in seeds:
            self.entries.setdefault(a, t)
        while work:
            addr, thumb = work.pop()
            # walk a linear run
            regs: dict[int, int] = {}   # tiny constant tracker for bx/adr call idioms
            while (addr, thumb) not in seen:
                seen.add((addr, thumb))
                ins = self.decode(addr, thumb)
                if ins is None or ins.kind == "undef":
                    if ins is not None:
                        self.warnings.append(f"undef at {addr:#x} ({'T' if thumb else 'A'}): {ins.text}")
                    break
                # constant tracking (reset at control flow)
                lit = self.literal(ins)
                if lit is not None:
                    regs[ins.rd] = lit
                elif ins.kind == "dp" and ins.op == "adr":
                    regs[ins.rd] = ((ins.pc_val() & ~3) + ins.op2.imm) | 0
                elif ins.kind == "dp" and ins.op in ("add", "sub") and ins.rn == 15 and ins.op2.is_imm and not ins.thumb:
                    v = ins.pc_val() + (ins.op2.imm if ins.op == "add" else -ins.op2.imm)
                    regs[ins.rd] = v & 0xFFFFFFFF
                elif ins.kind == "dp" and ins.op == "mov" and ins.op2 and not ins.op2.is_imm \
                        and ins.op2.rs is None and ins.op2.amount == 0 and ins.op2.shift == "lsl" \
                        and ins.op2.rm in regs and ins.rd != 15:
                    regs[ins.rd] = regs[ins.op2.rm]
                elif ins.kind == "dp" and ins.op == "mov" and ins.op2 and not ins.op2.is_imm \
                        and ins.op2.rm == 15 and ins.op2.rs is None and ins.rd != 15:
                    regs[ins.rd] = ins.pc_val()
                elif ins.kind == "dp" and ins.op == "mov" and ins.op2 and ins.op2.is_imm and ins.rd != 15:
                    regs[ins.rd] = ins.op2.imm
                elif ins.kind == "dp" and ins.op in ("bic",) and ins.op2 and ins.op2.is_imm and ins.rn in regs and ins.rd != 15:
                    regs[ins.rd] = regs[ins.rn] & ~ins.op2.imm
                else:
                    for r in self._written_regs(ins):
                        regs.pop(r, None)
                if ins.kind == "bx" and ins.rm != 14 and ins.rm in regs:
                    tgt = regs[ins.rm]
                    self.const_targets[ins.addr] = tgt
                    self._add_entry(tgt & ~1, bool(tgt & 1), work)
                if ins.kind == "dp" and ins.rd == 15 and ins.op == "mov" and ins.op2 and not ins.op2.is_imm \
                        and ins.op2.rm in regs and ins.op2.rs is None and ins.op2.amount == 0:
                    tgt = regs[ins.op2.rm]
                    self.const_targets[ins.addr] = tgt | (1 if thumb else 0)
                    self._add_entry(tgt & ~1, thumb, work)
                # Indirect call idiom (`mov lr, pc` / `adr lr, X` then bx/mov pc):
                # execution resumes at the LR value after the callee returns.
                if (ins.kind == "bx" and ins.rm != 14) or (ins.kind == "dp" and ins.rd == 15
                                                           and ins.op == "mov"):
                    if 14 in regs:
                        work.append((regs[14] & ~1, thumb))
                        self.call_returns.add(regs[14] & ~1)
                succ = self.successors(ins)
                fall = None
                for a, kind in succ:
                    if kind in ("call", "branch"):
                        self.targets.add(a)
                    if kind == "call":
                        self._add_entry(a, thumb, work)
                        self.call_returns.add(ins.addr + ins.size)
                    elif kind == "branch":
                        work.append((a, thumb))
                    elif kind == "fall":
                        fall = a
                if ins.kind in ("b",) or writes_pc(ins):
                    regs = {}
                if fall is None:
                    break
                addr = fall
        self._reached |= seen
        return seen

    def _add_entry(self, a: int, thumb: bool, work):
        if a not in self.entries:
            self.entries[a] = thumb
        work.append((a, thumb))

    @staticmethod
    def _written_regs(ins: Insn) -> list[int]:
        k = ins.kind
        if k == "dp":
            return [] if ins.op in ("tst", "teq", "cmp", "cmn") else [ins.rd]
        if k == "mul":
            return [ins.rd]
        if k == "mull":
            return [ins.rd, ins.rdhi]
        if k == "mem":
            r = [ins.rd] if ins.load else []
            if ins.wb or not ins.pre:
                r.append(ins.rn)
            return r
        if k == "block":
            r = list(ins.regs) if ins.load else []
            if ins.wb:
                r.append(ins.rn)
            return r
        if k in ("mrs", "swp"):
            return [ins.rd]
        if k == "swi":
            return [0, 1, 2, 3]
        if k == "b" and ins.link:
            return list(range(15))
        return []


# ═══════════════════════════════════════════════════════════════════════
# Program construction
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class CodeCopy:
    """Code executed from RAM after being copied there from ROM."""
    name: str
    vma: int
    src: int
    size: int


def build(info: ElfInfo, copies: list[CodeCopy], extra_seeds=()) -> Program:
    from .elfinfo import Section
    for c in copies:
        info.sections.insert(0, Section(c.name, c.vma, c.size, c.src - 0x08000000, True))

    p = Program(info)
    seeds: list[tuple[int, bool]] = []
    for a, (m, size, name) in info.funcs.items():
        if size == 0 and name.startswith("removed_"):
            continue
        seeds.append((a, m == "t"))
    seeds.extend(extra_seeds)

    # Data words that point at code labels (jump tables, callback tables,
    # script handler tables). A THUMB pointer has bit0 set and must name a
    # label inside a $t region; an ARM pointer is word aligned inside $a.
    regions = info.mapping
    rstarts = [a for a, _k in regions]

    def region_kind(addr: int) -> str | None:
        k = bisect.bisect_right(rstarts, addr) - 1
        return regions[k][1] if k >= 0 else None

    label_addrs = set(info.labels)
    rom = info.rom
    import struct as _st
    n = len(rom) // 4
    words = _st.unpack_from("<%dI" % n, rom, 0)
    datarefs = set()
    # Only labels on assembled instructions qualify: the DWARF line table has
    # rows for instructions and none for .byte/.word data.
    for w in words:
        if w & 1:
            a = w & ~1
            if a in label_addrs and a in info.lines and region_kind(a) == "t":
                datarefs.add((a, True))
        elif w in label_addrs and w in info.lines:
            k = region_kind(w)
            if k == "a" and w & 3 == 0:
                datarefs.add((w, False))
            elif k == "t":
                # THUMB switch tables consumed by `mov pc, rN` (no mode
                # change) store plain even addresses: `.word loc_814428C`.
                datarefs.add((w, True))
    for a, t in datarefs:
        seeds.append((a, t))
        p.data_refs.add(a)

    p.discover(seeds)
    _build_units(p, copies)
    return p


def _build_units(p: Program, copies: list[CodeCopy]):
    info = p.info
    # Unit starts: every FUNC symbol, plus discovered entries that are not
    # inside any FUNC range region (RAM copies without symbols).
    starts: dict[int, tuple[bool, str]] = {}
    for a, (m, size, name) in info.funcs.items():
        if size == 0 and name.startswith("removed_"):
            continue
        starts[a] = (m == "t", name)
    copy_ranges = [(c.vma, c.vma + c.size, c) for c in copies]

    def in_copy(a):
        for lo, hi, c in copy_ranges:
            if lo <= a < hi:
                return c
        return None

    for a, t in p.entries.items():
        c = in_copy(a)
        if c is not None and a not in starts:
            starts[a] = (t, f"{c.name}_{a:08X}")
    # Entries whose instruction-set mode differs from the enclosing symbol
    # range (THUMB `bx pc` stubs in front of ARM bodies) start their own unit.
    order = sorted(starts)
    for a, t in sorted(p.entries.items()):
        k = bisect.bisect_right(order, a) - 1
        if k < 0 or starts[order[k]][0] != t:
            nm = info.labels.get(a) or f"{'t' if t else 'a'}func_{a:08X}"
            starts[a] = (t, nm)
            order = sorted(starts)
    # Same for decoded instruction runs reached by plain branches into a
    # different-mode region (no entry recorded for them).
    while True:
        added = False
        for (addr, thumb) in sorted(p.insns):
            k = bisect.bisect_right(order, addr) - 1
            if k >= 0 and starts[order[k]][0] == thumb:
                continue
            prev = addr - (2 if thumb else 4)
            if (prev, thumb) in p.insns and k >= 0 and order[k] <= prev:
                pk = bisect.bisect_right(order, prev) - 1
                if pk == k and starts[order[k]][0] != thumb:
                    continue    # continuation of a run that will get its own start
            nm = info.labels.get(addr) or f"{'t' if thumb else 'a'}func_{addr:08X}"
            starts[addr] = (thumb, nm)
            order = sorted(starts)
            added = True
            break
        if not added:
            break
    p.unit_starts = order
    units = []
    for k, a in enumerate(order):
        t, name = starts[a]
        end = order[k + 1] if k + 1 < len(order) else a + 0x100000
        # never let a unit run across a code-copy boundary or section end
        c = in_copy(a)
        if c is not None:
            end = min(end, c.vma + c.size)
        u = Unit(start=a, end=end, thumb=t, name=name)
        units.append(u)
    p.units = units
    # Assign instructions to units.
    for (addr, thumb), ins in p.insns.items():
        u = p.unit_at(addr)
        if u is None:
            p.warnings.append(f"insn {addr:#x} outside any unit")
            continue
        if u.thumb != thumb:
            # A mode switch inside one symbol range (e.g. THUMB stub + ARM body
            # without its own FUNC symbol): split by creating a new unit.
            p.warnings.append(f"mode mismatch at {addr:#x} in unit {u.name}")
            continue
        if ins.kind == "undef" and (addr, thumb) not in p._reached:
            continue
        u.insns[addr] = ins
