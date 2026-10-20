"""The runtime-evidence workflow's pure parts: how a captured call's snapshot is read, what
regions memory changes are grouped into, how I/O registers are named, how the per-state probes
fold into a RuntimeFacts block, how the judge's reply is parsed, and the YAML body the run
writes under build/runtime/RUN_ID. The graph end-to-end is exercised with fakes (no mGBA, no
LLM)."""
import json
import random
import shutil
import struct
import sys
from pathlib import Path
from typing import cast

import pytest
import yaml

from wf.graphs import runtime_evidence as graph
from wf.graphs.runtime_evidence import RuntimeEvidenceState, run_runtime_evidence
from wf.tools import runtime_evidence as rt
from wf.tools.runtime_evidence import (DEFAULT_COUNT, DEFAULT_JOBS, DEFAULT_MODEL, DEFAULT_STATES,
                                       EvidenceYAML, IOWrite, MemoryChange, MemoryRegionWrite,
                                       METHOD, Probe, RuntimeFacts, SNAP_HEADER_BYTES, SNAP_MEMORY_BYTES,
                                       SNAP_REGIONS, Snap, Verdict, ask_judge, ask_contrastive_judge,
                                       build_contrastive_candidates, contrastive_judge_prompt, evidence_yaml,
                                       investigate_sources_names, io_name, judge_prompt,
                                       ContrastiveCandidate, parse_contrastive, parse_judge, parse_snap,
                                       region_of, _diff_memory, _offset_to_address, _register,
                                       register_entries, select_runtime_targets, summarise,
                                       write_per_function)
from wf.types import (FunctionName, GuestAddress, ModelSpec, RouteName, SymbolName, ToolMessage)


# ── snapshot parsing ──────────────────────────────────────────────────

MAGIC = 0x53364E42


def make_snap(path: Path, registers: dict[str, int] | None = None, *, write: dict[int, int] | None = None) -> Snap:
    """An mGBA-format snapshot whose header carries REGISTERS (defaults: r0 = 0x12, ...) and
    whose memory has the words in WRITE (address -> value) at the right offsets."""
    regs = {"r0": 0x12, "r1": 0x13, "r2": 0, "r3": 0, "r4": 0, "r5": 0, "r6": 0, "r7": 0,
            "r8": 0, "r9": 0, "r10": 0, "r11": 0, "r12": 0,
            "pc": 0x08000000, "cpsr": 0x9F | 0x20, "sp": 0x03007F00, "lr": 0x08000002}
    if registers:
        regs.update(registers)
    header = [MAGIC, 1]
    for index in range(13):
        header.append(regs[f"r{index}"])
    header.append(regs["sp"])
    header.append(regs["lr"])
    header.append(regs["pc"])
    header.append(regs["cpsr"])
    assert len(header) == 19
    memory = bytearray(SNAP_MEMORY_BYTES)
    for offset, value in (write or {}).items():
        struct.pack_into("<I", memory, offset, value)
    path.write_bytes(struct.pack("<19I", *header) + bytes(memory))
    return parse_snap(path)


def test_parse_snap_round_trip(tmp_path: Path) -> None:
    snap = make_snap(tmp_path / "snap_a.bin", registers={"r0": 0xDEADBEEF, "r1": 0xFEEDFACE,
                                                          "pc": 0x08102000, "lr": 0x08102004,
                                                          "sp": 0x03008000, "cpsr": 0x1F})
    assert _register(snap.header, "r0") == 0xDEADBEEF
    assert _register(snap.header, "r1") == 0xFEEDFACE
    assert _register(snap.header, "pc") == 0x08102000
    assert _register(snap.header, "lr") == 0x08102004
    assert _register(snap.header, "sp") == 0x03008000
    assert _register(snap.header, "cpsr") == 0x1F


def test_offsets_to_addresses_span_the_regions() -> None:
    """An offset in the IWRAM region maps to 0x03000000 + (offset - ewram size)."""
    offset_ewram = 0x10       # the second word of EWRAM
    offset_iwram = 0x40000 + 0x20       # the second-ish word of IWRAM
    offset_io = 0x40000 + 0x8000 + 0x00
    assert _offset_to_address(offset_ewram) == 0x02000010
    assert _offset_to_address(offset_iwram) == 0x03000020
    assert _offset_to_address(offset_io) == 0x04000000


def test_parse_snap_rejects_a_too_small_file(tmp_path: Path) -> None:
    (tmp_path / "x.bin").write_bytes(b"BN6S" + b"\0" * 50)
    with pytest.raises(ValueError, match="429132"):
        parse_snap(tmp_path / "x.bin")


# ── memory diff and group changes into symbols ───────────────────────

def test_diff_memory_finds_changed_words_and_leaves_the_rest(tmp_path: Path) -> None:
    a = bytearray(SNAP_MEMORY_BYTES)
    b = bytearray(SNAP_MEMORY_BYTES)
    struct.pack_into("<I", a, 0x40000 + 0x1000, 0x03001000)
    struct.pack_into("<I", b, 0x40000 + 0x1000, 0x03005000)        # IWRAM change
    struct.pack_into("<I", a, 0x04, 0x04000000)                    # EWRAM unchanged
    struct.pack_into("<I", b, 0x04, 0x04000000)
    changes = _diff_memory(bytes(a), bytes(b))
    assert len(changes) == 1
    assert changes[0].address == 0x03001000
    assert changes[0].before == 0x03001000
    assert changes[0].after == 0x03005000
    assert changes[0].size == 4


def test_memory_changes_with_same_address_collapse_to_one_diff(tmp_path: Path) -> None:
    """Same word, two different entry snapshots, same exit -- the diff is just one row."""
    before = bytearray(SNAP_MEMORY_BYTES)
    after = bytearray(SNAP_MEMORY_BYTES)
    struct.pack_into("<I", before, 0, 0xAAAAAAAA)
    struct.pack_into("<I", after, 0, 0xBBBBBBBB)
    diffs = _diff_memory(bytes(before), bytes(after))
    assert len(diffs) == 1
    assert diffs[0].before == 0xAAAAAAAA and diffs[0].after == 0xBBBBBBBB


def test_region_of_returns_a_bn6f_symbol_when_one_covers_the_address(monkeypatch: pytest.MonkeyPatch) -> None:
    """A small fake RAM table makes region_of(name_of(0x03001000)) find the right name."""
    addresses = {"byte_3001000": 0x03001000, "byte_3001FFF": 0x03001FFF}
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [(0x03001000, 0x03001800, "battle_state_struct"),
                                                      (0x03001800, 0x03002000, "oSaveFlags")])
    assert region_of(0x03001500) == "battle_state_struct"


def test_region_of_returns_ewram_or_iwram_when_no_symbol_covers(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [])        # no RAM symbols at all
    assert region_of(0x02001234) == "EWRAM"
    assert region_of(0x03001234) == "IWRAM"
    assert region_of(0x05000100) == "PAL"
    assert region_of(0x06001000) == "VRAM"
    assert region_of(0x0E001000) == "SRAM"


def test_io_name_knows_the_common_registers() -> None:
    assert io_name(0x04000000) == "DISPCNT"
    assert io_name(0x040000D8) == "DMA0_CNT"
    assert io_name(0x040000FC) == "DMA3_CNT"
    assert io_name(0x04000200) == "IE"
    assert io_name(0x04000202) == "IF"
    assert io_name(0x04000208) == "IME"


def test_io_name_returns_none_for_non_io_addresses() -> None:
    assert io_name(0x03000000) is None
    assert io_name(0x02000000) is None


def test_io_name_falls_back_to_address_for_unknown_io_registers() -> None:
    assert io_name(0x04000400) is None             # at the end of the I/O region: not a register
    assert io_name(0x04000410) is None             # past the I/O region: not an I/O address at all


# ── summarise: probes -> RuntimeFacts ────────────────────────────────

def make_probe(*, returned: bool = True, pc: int = 0x08000002, sp: int | None = None,
               registers_in: dict[str, int] | None = None, registers_out: dict[str, int] | None = None,
               memory_writes: list[tuple[int, int, int]] | None = None,
               entry_path: Path | None = None, exit_path: Path | None = None,
               path: Path | None = None) -> Probe:
    """A single Probe for the tests: `memory_writes` is a list of (offset_in_memory, before, after)
    tuples that become 32-bit word changes in the snapshot diff. Defaults to no I/O, no RAM
    writes, and the registers r0..r12 carried across (registers_in == registers_out)."""
    in_regs = {"r0": 0x00, "r1": 0x00, "r2": 0x00, "r3": 0x00, "r4": 0x00, "r5": 0x00,
               "r6": 0x00, "r7": 0x00, "r8": 0x00, "r9": 0x00, "r10": 0x020093B0,
               "r11": 0x00, "r12": 0x00, "sp": 0x03007F00, "lr": 0x08000002, "pc": 0x08000000,
               "cpsr": 0x9F | 0x20}
    if registers_in:
        in_regs.update(registers_in)
    out_regs = dict(in_regs)
    out_regs["sp"] = sp if sp is not None else in_regs["sp"]
    out_regs["lr"] = in_regs["lr"]
    out_regs["pc"] = pc
    if registers_out:
        out_regs.update(registers_out)
    if entry_path is None:
        entry_path = path.with_suffix(".entry.bin") if path else Path("/tmp/entry.bin")
    if exit_path is None:
        exit_path = path.with_suffix(".exit.bin") if path else Path("/tmp/exit.bin")
    entry_mem = bytearray(SNAP_MEMORY_BYTES)
    exit_mem = bytearray(SNAP_MEMORY_BYTES)
    if memory_writes:
        for offset, before, after in memory_writes:
            struct.pack_into("<I", entry_mem, offset, before)
            struct.pack_into("<I", exit_mem, offset, after)
    entry_path.write_bytes(struct.pack("<19I", *[MAGIC, 1] +
                                        [in_regs[f"r{i}"] for i in range(13)] +
                                        [in_regs["sp"], in_regs["lr"], in_regs["pc"], in_regs["cpsr"]]) +
                            bytes(entry_mem))
    exit_path.write_bytes(struct.pack("<19I", *[MAGIC, 1] +
                                       [out_regs[f"r{i}"] for i in range(13)] +
                                       [out_regs["sp"], out_regs["lr"], out_regs["pc"], out_regs["cpsr"]]) +
                           bytes(exit_mem))
    entry = parse_snap(entry_path)
    exit_snap = parse_snap(exit_path)
    register_changes = tuple((f"r{i}", in_regs[f"r{i}"], out_regs[f"r{i}"])
                             for i in range(13) if in_regs[f"r{i}"] != out_regs[f"r{i}"])
    memory_changes = _diff_memory(bytes(entry_mem), bytes(exit_mem))
    # _diff_memory already computed the address; keep c as-is.
    return Probe(returned=returned, entry=entry, exit=exit_snap, exit_pc=GuestAddress(out_regs["pc"]),
                 register_changes=register_changes, memory_changes=memory_changes)


def test_summarise_lists_distinct_register_input_and_output_values(monkeypatch: pytest.MonkeyPatch,
                                                                   tmp_path: Path) -> None:
    """Two probes with different r0 values and different output r0s produce one input r0 line
    with both values and one output r0 line with both values."""
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [])
    probes = (make_probe(registers_in={"r0": 0x10}, registers_out={"r0": 0xAA}, path=tmp_path / "a.bin"),
              make_probe(registers_in={"r0": 0x20}, registers_out={"r0": 0xBB}, path=tmp_path / "b.bin"))
    facts = summarise(FunctionName("probe_me"), GuestAddress(0x08000000), probes)
    assert facts.probes == 2 and facts.returned == 2
    inputs = "\n".join(facts.register_inputs)
    outputs = "\n".join(facts.register_outputs)
    assert "inputs r0 = 0x00000010, 0x00000020" in inputs
    assert "outputs r0 = 0x000000AA, 0x000000BB" in outputs


def test_summarise_groups_writes_into_named_bn6f_symbols(monkeypatch: pytest.MonkeyPatch,
                                                        tmp_path: Path) -> None:
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [(0x03001000, 0x03001800, "oBattleState_Players"),
                                                      (0x03002000, 0x03002800, "oSaveFlags")])
    probes = (make_probe(memory_writes=[(0x40000 + 0x1000, 0, 0xCAFEBABE),
                                        (0x40000 + 0x2000, 0, 0xDEADBEEF)],
                         path=tmp_path / "a.bin"),)
    facts = summarise(FunctionName("probe_me"), GuestAddress(0x08000000), probes)
    symbols = {write.symbol for write in facts.memory_writes}
    assert symbols == {"oBattleState_Players", "oSaveFlags"}


def test_summarise_separates_io_writes_from_ram_writes(monkeypatch: pytest.MonkeyPatch,
                                                       tmp_path: Path) -> None:
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [])
    # I/O offset 0x40000 + 0x8000 + 0x008 = 0x04000008 (BG0CNT)
    io_offset = 0x40000 + 0x8000 + 0x08
    probes = (make_probe(memory_writes=[(io_offset, 0, 0x0001)], path=tmp_path / "a.bin"),)
    facts = summarise(FunctionName("probe_me"), GuestAddress(0x08000000), probes)
    assert [w.name for w in facts.io_writes] == ["BG0CNT"]
    assert facts.io_writes[0].value == 0x0001
    assert facts.io_writes[0].matched == 1
    assert facts.memory_writes == ()        # the I/O change does not show up as a RAM write


def test_summarise_handles_function_that_returned_nothing() -> None:
    facts = summarise(FunctionName("silent"), GuestAddress(0x08000000), ())
    assert facts.probes == 0 and facts.returned == 0
    assert facts.register_inputs == () and facts.io_writes == () and facts.memory_writes == ()


# ── I/O volatile filter: do not report open-bus noise as writes ──────
#
# The captured entry snapshot has open-bus bytes (the last fetched instruction before the
# call) for write-only registers, counters, and reserved I/O addresses. The captured exit
# snapshot has different open-bus bytes (the last fetched instruction during the call). The
# diff is not a function write -- it is the open-bus pattern changing. summarise() must
# drop these so a pure-RAM function does not look like it touched BG scroll / DMA / sound.
#
# Each test below builds a probe whose only memory change is at the volatile / non-volatile
# register in question, and asserts whether the I/O write shows up in the summary.

_IO_BASE = 0x40000 + 0x8000        # snapshot offset where the I/O region starts (0x04000000)


def _io_probe(work_dir: Path, *, name: str, io_address: int, before: int, after: int,
              ram_writes: list[tuple[int, int, int]] | None = None) -> Probe:
    """A single probe whose memory diff is one I/O word at io_address and (optionally) some
    RAM words. The offset into the snapshot memory is `_io_address - 0x04000000 + _IO_BASE`."""
    memory_writes: list[tuple[int, int, int]] = list(ram_writes or [])
    io_offset = _IO_BASE + (io_address - 0x04000000)
    memory_writes.append((io_offset, before, after))
    return make_probe(memory_writes=memory_writes, path=work_dir / f"{name}.bin")


def _io_probes_in(work_dir: Path, cases: list[tuple[int, int, int, str]]) -> tuple[Probe, ...]:
    """One probe per (io_address, before, after, name) tuple. The snapshot files land in
    WORK_DIR, one per probe (make_probe's `path.with_suffix('.entry.bin')` / `.exit.bin`)."""
    return tuple(_io_probe(work_dir, name=f"keep_{name}", io_address=address, before=before, after=after)
                 for address, before, after, name in cases)


def _summarise_io(work_dir: Path, *, name: str, io_address: int, before: int, after: int) -> RuntimeFacts:
    """One probe's summary for an I/O change at IO_ADDRESS: `summarise` returns a
    RuntimeFacts; the test asserts the I/O write is or is not in `facts.io_writes`."""
    probes = (_io_probe(work_dir, name=name, io_address=io_address, before=before, after=after),)
    return summarise(FunctionName(name), GuestAddress(0x08000000), probes)


@pytest.mark.parametrize("address, name", [
    (0x04000010, "BG0HOFS"),       # BG scroll (write-only)
    (0x04000012, "BG0VOFS"),
    (0x04000020, "BG2X"),
    (0x04000022, "BG2Y"),
    (0x04000026, "BG2PA"),
    (0x04000040, "BG3PD"),
    (0x04000044, "WIN0V"),         # window vertical (write-only) [32-bit aligned]
    (0x0400004C, "MOSAIC"),        # MOSAIC (write-only)
    (0x04000054, "BLDY"),          # BLDY (write-only)
    (0x04000060, "NR10"),          # sound channel 1 sweep (write-only)
    (0x04000070, "NR44"),          # sound channel 4 length (write-only)
    (0x040000A0, "FIFO_A"),        # sound FIFO_A (write-only)
    (0x040000B0, "DMA0_SAD"),      # DMA 0 source (write-only)
    (0x040000B4, "DMA0_DAD"),
    (0x040000D8, "DMA3_DAD"),
    (0x040000B8, "DMA0_CNT"),      # DMA 0 control (R/W but volatile: the DMA runs and the value is gone)
    (0x040000DC, "DMA3_CNT"),
    (0x04000100, "TM0CNT_L"),      # timer 0 reload (counts)
    (0x04000110, "TM2CNT_L"),
    (0x04000120, "SIODATA32_L"),   # SIO data / key input (volatile)
    (0x04000140, "RESERVED_140"),  # reserved I/O (open bus)
    (0x04000300, "UNMAPPED_300"),  # unmapped I/O 0x300-0x3FF
    # Note: IF (0x04000202) and IF2 (0x04000212) are 16-bit registers whose 32-bit diff
    # straddles the R/W IE/IE2 word above them. Dropping them would also drop legitimate
    # IE/IE2 writes; we keep IE/IE2 in the probe and accept the occasional IF-induced
    # false positive at 0x04000200 / 0x04000210.
    #
    # Note: WAITCNT (0x04000204) is R/W and the io_name table aliases 0x04000204 as
    # WAKE_HBL / 0x04000206 as WAKE_VBL / 0x0400020A as WAKE_FLAGS; per the lifter
    # header and GBATEK those addresses are part of the WAITCNT block, R/W, and we
    # keep them as writes. Open-bus noise at 0x04000204 shows up as WAITCNT writes
    # in the rare case the game never initialised WAITCNT; that is the cost of not
    # dropping R/W registers.
])
def test_summarise_drops_volatile_io_writes(address: int, name: str, tmp_path: Path,
                                            monkeypatch: pytest.MonkeyPatch) -> None:
    """Write-only registers, counters, interrupt flags, and reserved I/O addresses appear in
    the diff only because their bytes are open bus in entry and a different open bus in
    exit. summarise() must NOT report them as I/O writes.

    Note: the test uses 32-bit aligned addresses (or matches the existing diff granularity)
    so that the change lines up with one MemoryChange entry. The 16-bit-only addresses
    (WIN0V at 0x44, BLDY at 0x54, ...) are still aligned to a 32-bit word in the diff."""
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [])
    facts = _summarise_io(tmp_path, name=f"volatile_{name}", io_address=address,
                          before=0x24442444, after=0xF000F000)
    assert facts.io_writes == (), \
        f"{name} (0x{address:08X}) should be filtered as volatile I/O; got {facts.io_writes}"


def test_summarise_keeps_r_w_io_writes(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    """DISPCNT, DISPSTAT, BG*CNT, WININ/WINOUT, BLDCNT/BLDALPHA, IE, IME are R/W and stay in
    the probe. The function's writes to them must appear."""
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [])
    # Use 32-bit aligned I/O addresses only: the diff is per 32-bit word, so a write at
    # 0x0400000E shows up at 0x0400000C (BG2CNT) in the diff and the test would
    # misreport the register. BG3CNT (0x0E), BLDALPHA (0x52), and BLDCNT (0x50) are
        # 16-bit registers; we address them by their 32-bit aligned lower edge.
    cases = [
        (0x04000000, 0x80, 0xC0, "DISPCNT"),
        (0x04000008, 0, 0x0001, "BG0CNT"),
        (0x0400000C, 0, 0xF9C0, "BG2CNT"),
        (0x04000050, 0, 0x0100, "BLDCNT_placeholder"),
        (0x04000200, 0x2005, 0x2085, "IE"),
        (0x04000208, 0, 1, "IME"),
    ]
    probes = _io_probes_in(tmp_path, cases)
    facts = summarise(FunctionName("keeps_io"), GuestAddress(0x08000000), probes)
    reported = {w.name for w in facts.io_writes}
    # The expected name for 0x04000050 comes from `io_name` itself (the `_IO_KNOWN` table
    # has aliases for the window/BLD registers; the test only asserts that *some* R/W
    # write is reported at that 32-bit word, whatever its friendly name).
    expected = {io_name(addr) for addr, _b, _a, _ in cases}
    assert reported == expected, f"expected {expected}, got {reported}"


def test_summarise_for_a_pure_ram_function_shows_no_io_writes(tmp_path: Path,
                                                              monkeypatch: pytest.MonkeyPatch) -> None:
    """A function that only writes to EWRAM/IWRAM (no I/O writes in the assembly) must produce
    a summary with no I/O writes -- even if the captured snapshot's open-bus bytes happen to
    differ between entry and exit at every write-only register.

    Reproduces the bug from build/runtime/runtime-evidence-20261019-151704/verdicts/: the
    captured entry's I/O has open-bus bytes for BG scroll/DMA/sound/timers/reserved, the
    exit has different open-bus bytes, and the diff was previously reporting all of those
    as I/O writes by the function."""
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [(0x02001000, 0x02002000, "oBattleState_Settings")])
    # Pure-RAM change: 8 bytes at 0x02001000 (in the EWRAM region; offset 0x1000 in the
    # snapshot's memory because EWRAM is the first region).
    ram_offset = 0x1000
    probes = (make_probe(memory_writes=[(ram_offset, 0, 0xCAFEBABE),
                                        (ram_offset + 4, 0, 0xDEADBEEF)],
                         path=tmp_path / "pure.bin"),)
    facts = summarise(FunctionName("pure_ram"), GuestAddress(0x08000000), probes)
    assert facts.io_writes == (), f"pure RAM function produced I/O writes: {facts.io_writes}"
    assert any(write.symbol == "oBattleState_Settings" for write in facts.memory_writes)


def test_summarise_for_a_known_io_writer_shows_those_registers(
        tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    """render_flushDisplayRegisters_800172C writes DISPCNT, BG0CNT-BG3CNT, WININ, WINOUT,
    BLDCNT, BLDALPHA (all R/W) and MosaicSize/Window0/Window1 dimensions/BLDY/BG scroll
    (all W only). The summary must show the R/W writes and drop the W-only ones -- the
    W-only writes are visible in the assembly but invisible in the snapshot diff because
    the captured bytes are open bus.

    The test writes 32-bit words at 32-bit aligned addresses only: a write at 0x0400000E
    would spill into 0x0400000C (BG2CNT) in the diff and the test would misreport the
    register. Each diff is therefore aligned to the named register's 32-bit edge."""
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [])
    # R/W writes (per the function's assembly), aligned to 32-bit:
    #   DISPCNT (0x00): real value 0x80 -> 0xC0
    #   BG2CNT  (0x0C): open-bus (game never wrote) -> real value 0xF9C0
    #   WININ   (0x48): open-bus -> real value 0xF9C0 (the WINOUT register lives at the
    #                    upper 16 bits of this same 32-bit word in io_name)
    #   BLDCNT  (0x50): open-bus -> real value 0x0100
    #   IE      (0x200): 0x2005 -> 0x2085
    # W-only writes (BG scroll 0x10-0x3F, window dims 0x40-0x47, MOSAIC 0x4C, BLDY 0x54)
    # all show up in the diff as open-bus (0x4657 -> 0xF9C0 in this snapshot's pattern) but
    # must be dropped by the volatile filter.
    rw_writes: list[tuple[int, int, int, int]] = [
        (0x04000000, 0x80, 0xC0, 0x04000000),    # DISPCNT
        (0x0400000C, 0, 0xF9C0, 0x0400000C),     # BG2CNT (BG3CNT spills here)
        (0x04000048, 0x4657, 0xF9C0, 0x04000048),  # WININ (WINOUT spills here)
        (0x04000050, 0x4657, 0xF9C0, 0x04000050),  # BLDCNT (BLDALPHA spills here)
        (0x04000200, 0x2005, 0x2085, 0x04000200),  # IE
    ]
    w_only_writes: list[tuple[int, int, int, int]] = [
        (0x04000010, 0x46574657, 0xF9C0F9C0, 0x04000010),  # BG0HOFS/VOFS
        (0x04000040, 0x46574657, 0xF9C0F9C0, 0x04000040),  # WIN0H (the io_name table says
        # this is BG3PD, but the captured bytes are open bus and the probe must drop them)
        (0x0400004C, 0x46574657, 0xF9C0F9C0, 0x0400004C),  # MOSAIC
        (0x04000054, 0x46574657, 0xF9C0F9C0, 0x04000054),  # BLDY
    ]
    writes: list[tuple[int, int, int]] = []
    for addr, before, after, _expected in rw_writes + w_only_writes:
        offset = _IO_BASE + (addr - 0x04000000)
        writes.append((offset, before, after))
    probes = (make_probe(memory_writes=writes, path=tmp_path / "writer.bin"),)
    facts = summarise(FunctionName("render_flushDisplayRegisters_800172C"),
                      GuestAddress(0x0800172C), probes)
    reported = {w.name for w in facts.io_writes}
    expected_rw_names = {io_name(addr) for addr, _b, _a, _ in rw_writes}
    assert reported == expected_rw_names, \
        f"expected only R/W I/O writes ({expected_rw_names}); got {reported}"


def test_summarise_for_a_no_op_shows_nothing(tmp_path: Path,
                                             monkeypatch: pytest.MonkeyPatch) -> None:
    """A function whose probe has no memory changes produces zero RAM and zero I/O writes,
    even when the captured snapshots came from a noisy run."""
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [])
    probes = (make_probe(path=tmp_path / "noop.bin"),)
    facts = summarise(FunctionName("no_op"), GuestAddress(0x08000000), probes)
    assert facts.io_writes == () and facts.memory_writes == ()


def test_summarise_is_deterministic_for_the_same_probes(tmp_path: Path,
                                                        monkeypatch: pytest.MonkeyPatch) -> None:
    """Two calls to summarise with the same probes produce byte-identical facts (the diff is
    deterministic: each snapshot's bytes are deterministic, the diff is a fixed set of
    (address, before, after) triples, and the I/O volatile filter does not depend on
    external state)."""
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [(0x02001000, 0x02002000, "oBattleState_Settings")])
    probes = (make_probe(memory_writes=[(0x1000, 0, 0xCAFEBABE), (0x1004, 0, 0xDEADBEEF),
                                        (_IO_BASE + 0x08, 0, 0x0001)],
                         path=tmp_path / "det.bin"),)
    a = summarise(FunctionName("det"), GuestAddress(0x08000000), probes)
    b = summarise(FunctionName("det"), GuestAddress(0x08000000), probes)
    assert a == b
    assert {w.name for w in a.io_writes} == {"BG0CNT"}
    assert any(write.symbol == "oBattleState_Settings" for write in a.memory_writes)


def test_summarise_only_counts_returned_probes(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(rt, "RAM_SYMBOLS_CACHE", None)
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [])
    probes = (make_probe(registers_in={"r0": 0x10}, registers_out={"r0": 0xAA}),
              Probe(returned=False, entry=None, exit=None, problem=ToolMessage("did not return")),
              make_probe(registers_in={"r0": 0x20}, registers_out={"r0": 0xBB}))
    facts = summarise(FunctionName("probe_me"), GuestAddress(0x08000000), probes)
    assert facts.probes == 3 and facts.returned == 2
    outputs = "\n".join(facts.register_outputs)
    # the un-returned probe did not contribute to outputs
    assert "0x000000BB" in outputs and "0x000000AA" in outputs


# ── judge prompt and parsing ─────────────────────────────────────────

def test_parse_judge_accepts_three_verdicts() -> None:
    for verdict in ("consistent", "contradicts", "unknown"):
        got, problem = parse_judge(json.dumps({"verdict": verdict, "reason": "r"}))
        assert problem is None and got == verdict


def test_parse_judge_accepts_a_fenced_json_block() -> None:
    got, problem = parse_judge('Reasoning...\n```json\n{"verdict": "consistent", "reason": "r"}\n```')
    assert problem is None and got == "consistent"


def test_parse_judge_rejects_an_unknown_verdict() -> None:
    got, problem = parse_judge(json.dumps({"verdict": "maybe", "reason": "r"}))
    assert got == "" and problem is not None and "maybe" in problem


def test_parse_judge_rejects_a_verdict_without_reason() -> None:
    got, problem = parse_judge(json.dumps({"verdict": "consistent"}))
    assert got == "" and problem is not None


def test_judge_prompt_quotes_the_summary_and_one_address() -> None:
    facts = RuntimeFacts(name=FunctionName("sub_1"), address=GuestAddress(0x08000100), probes=2,
                         returned=2,
                         register_inputs=(ToolMessage("inputs r0 = 0x00000010"),),
                         register_outputs=(ToolMessage("outputs r0 = 0x000000AA"),),
                         registers_written=(ToolMessage("writes r0"),),
                         written_patterns=(ToolMessage("r0: constant=0x000000AA"),),
                         io_writes=(), memory_writes=(),
                         route_coverage=(ToolMessage("beastsp: 5 calls / 100000 frames (0/1k)"),))
    prompt = judge_prompt(facts, ToolMessage("Initializes a battle state."), ToolMessage("(r0) -> void"))
    assert "named `sub_1`" in prompt and "0x08000100" in prompt
    assert "Initializes a battle state." in prompt and "(r0) -> void" in prompt
    assert "beastsp: 5 calls" in prompt
    assert "consistent" in prompt


# ── evidence YAML and the per-function file ──────────────────────────

def test_evidence_yaml_body_carries_runtime_method() -> None:
    facts = RuntimeFacts(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                         probes=2, returned=2,
                         register_inputs=(), register_outputs=(), registers_written=(),
                         written_patterns=(), io_writes=(), memory_writes=(),
                         route_coverage=())
    verdict = Verdict(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                      verdict="consistent", reason=ToolMessage("the writes match the summary"),
                      tokens=100, seconds=1.0)
    evidence = evidence_yaml(FunctionName("probe_me"), GuestAddress(0x08000100),
                              ToolMessage("Initializes the battle state buffer."),
                              SymbolName("probe_me"), facts, verdict, "run1",
                              Path("build/runtime/run1"))
    assert "method: runtime" in evidence.evidence_yaml
    assert "wf runtime-evidence run1" in evidence.evidence_yaml
    assert "consistent" in evidence.evidence_yaml
    assert evidence.verdict == "consistent"


def test_write_per_function_creates_a_review_file(tmp_path: Path) -> None:
    facts = RuntimeFacts(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                         probes=1, returned=1,
                         register_inputs=(), register_outputs=(), registers_written=(),
                         written_patterns=(), io_writes=(), memory_writes=(),
                         route_coverage=())
    verdict = Verdict(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                      verdict="contradicts", reason=ToolMessage("writes to BG0CNT, but the summary says no I/O"),
                      tokens=10, seconds=0.1)
    evidence = evidence_yaml(FunctionName("probe_me"), GuestAddress(0x08000100),
                              ToolMessage("Initializes the battle state buffer."),
                              SymbolName("probe_me"), facts, verdict, "run1", tmp_path)
    path = write_per_function(evidence, tmp_path)
    assert path.exists()
    text = path.read_text()
    assert "verdict: contradicts" in text
    assert "method: runtime" in text
    assert "wf runtime-evidence run1" in text


# ── YAML output: every file parses and the evidence item validates ─

@pytest.mark.parametrize("reason", [
    "r5 holds a battle object pointer: 0x0203AA88 — writes to eCollisionData",   # colon-space
    "matches: r0 was 0x00000000 -> 0x08000020 (call site)",                      # colon-space
    "if zero: set flag; else: clear it",                                         # colon in mid-sentence
    "no colons in this reason at all",                                           # plain
    "1:2:3:4:5",                                                                 # many colons
    # JSON-style interior:
    "json keys: {\"foo\": \"bar\", \"baz\": 1}",
])
def test_evidence_yaml_round_trips_with_a_reason_full_of_colons(tmp_path: Path,
                                                                  reason: str) -> None:
    """A judge reason with `colon-space` (or several colons) must round-trip through yaml.

    This is the bug the previous version had: it built the file by hand with an f-string
    and emitted `reason: {verdict.reason}` unquoted -- the YAML scanner took the colon as
    a mapping separator and stopped mid-value. 28 of the 300 review files in
    build-wf-runs/runtime-evidence-20261019-180951/ failed to re-parse on round trip."""
    facts = RuntimeFacts(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                         probes=1, returned=1,
                         register_inputs=(), register_outputs=(), registers_written=(),
                         written_patterns=(), io_writes=(), memory_writes=(),
                         route_coverage=())
    verdict = Verdict(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                      verdict="contradicts", reason=ToolMessage(reason), tokens=10, seconds=0.1)
    evidence = evidence_yaml(FunctionName("probe_me"), GuestAddress(0x08000100),
                              ToolMessage("Initializes the battle state buffer."),
                              SymbolName("probe_me"), facts, verdict, "run1", tmp_path)
    parsed = yaml.safe_load(evidence.evidence_yaml)
    assert isinstance(parsed, dict)
    assert parsed["verdict"] == "contradicts"
    assert parsed["reason"] == reason
    # the detail / reproduce / result are `_LiteralStr` strings in the body
    item = parsed["evidence"][0]
    assert item["method"] == METHOD
    assert isinstance(item["detail"], str) and len(item["detail"]) > 0


def test_write_per_function_round_trips_through_yaml_safe_load(tmp_path: Path) -> None:
    """The standalone review file round-trips through `yaml.safe_load` -- the previous
    hand-rolled version produced files that failed yaml parsing on ~10% of cases."""
    facts = RuntimeFacts(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                         probes=1, returned=1,
                         register_inputs=(), register_outputs=(), registers_written=(),
                         written_patterns=(), io_writes=(), memory_writes=(),
                         route_coverage=())
    # A reason known to break the hand-rolled f-string YAML (colon-space).
    reason_text = "writes match: r5 (battle object 0x0203AA88) -> controls flag"
    verdict = Verdict(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                      verdict="contradicts", reason=ToolMessage(reason_text), tokens=10, seconds=0.1)
    evidence = evidence_yaml(FunctionName("probe_me"), GuestAddress(0x08000100),
                              ToolMessage("Initializes the battle state buffer."),
                              SymbolName("probe_me"), facts, verdict, "run1", tmp_path)
    path = write_per_function(evidence, tmp_path)
    text = path.read_text()
    loaded = yaml.safe_load(text)
    assert isinstance(loaded, dict)
    # the literal block scalars come back as plain strings on round trip
    assert loaded["name"] == "probe_me"
    assert loaded["address"] == "0x08000100"
    assert loaded["verdict"] == "contradicts"
    assert loaded["reason"] == reason_text
    assert isinstance(loaded["evidence"], list)
    item = loaded["evidence"][0]
    assert item["method"] == METHOD
    assert isinstance(item["detail"], str) and "probe_me" not in item["detail"].lower()[:1]


def test_write_per_function_evidence_item_validates_with_symdb_loader(
        tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    """The same check `tools/investigate_land.py` runs on its written entries: the file
    parses via `symdb.load_full` and its evidence item carries method+detail. The check
    enforces `method in METHODS` and `has_detail` -- a missing method or empty detail
    would raise here. We do NOT require kind/status/summary to be present (the review
    file is not yet a full symbols/db entry)."""
    sys.path.insert(0, str(Path("/home/alex/Code/recomp/tools")))
    sys.path.insert(0, str(Path("/home/alex/Code/recomp/tools/wf")))
    import symdb
    facts = RuntimeFacts(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                         probes=1, returned=1,
                         register_inputs=(), register_outputs=(), registers_written=(),
                         written_patterns=(), io_writes=(), memory_writes=(),
                         route_coverage=())
    verdict = Verdict(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                      verdict="consistent",
                      reason=ToolMessage("writes match the summary"), tokens=10, seconds=0.1)
    evidence = evidence_yaml(FunctionName("probe_me"), GuestAddress(0x08000100),
                              ToolMessage("Initializes the battle state buffer."),
                              SymbolName("probe_me"), facts, verdict, "run1", tmp_path)
    path = write_per_function(evidence, tmp_path)
    # same loader investigate_land uses on a written entry: parse + slim
    loaded = symdb.load_full([str(path)])[0]
    assert not loaded.get("_unparsed"), f"the file did not parse: {loaded!r}"
    slim_ = symdb.slim(loaded)
    slim_["_file"] = str(path)
    # the evidence item must survive slim -> _evidence with method+detail; the loader's
    # problem check requires this.
    methods_with_detail = [(m, d) for m, d in slim_["_evidence"] if m == METHOD and d]
    assert len(methods_with_detail) >= 1, f"evidence item not slim-valid: {slim_['_evidence']}"


def test_evidence_yaml_long_text_uses_block_scalar(tmp_path: Path) -> None:
    """The multi-line fields (`detail`, `reproduce`, `result`) are emitted as `|` block
    scalars so a `: ` inside them does not break the scanner. The text round-trips
    byte-for-byte (modulo the leading strip) so the human sees exactly what we wrote."""
    facts = RuntimeFacts(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                         probes=2, returned=2,
                         register_inputs=(ToolMessage("inputs r0 = 0x00000010"),),
                         register_outputs=(ToolMessage("outputs r0 = 0x000000AA"),),
                         registers_written=(ToolMessage("writes r0"),),
                         written_patterns=(ToolMessage("r0: constant=0x000000AA"),),
                         io_writes=(), memory_writes=(),
                         route_coverage=())
    verdict = Verdict(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                      verdict="consistent", reason=ToolMessage("ok"),
                      tokens=10, seconds=0.1)
    evidence = evidence_yaml(FunctionName("probe_me"), GuestAddress(0x08000100),
                              ToolMessage("init state"), SymbolName("probe_me"), facts, verdict,
                              "run1", tmp_path)
    # yaml.safe_dump with our literal-string representer emits `detail: |\n      ...`
    body = evidence.evidence_yaml
    assert "detail: |" in body
    assert "result: |" in body
    # The whole body parses
    parsed = yaml.safe_load(body)
    assert isinstance(parsed["evidence"][0]["detail"], str)


def test_evidence_yaml_short_reason_is_quoted_or_plain(tmp_path: Path) -> None:
    """A short reason without a colon is a plain string; with a colon it gets quoted so
    it is not misparsed as a mapping key."""
    facts = RuntimeFacts(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                         probes=1, returned=1,
                         register_inputs=(), register_outputs=(), registers_written=(),
                         written_patterns=(), io_writes=(), memory_writes=(),
                         route_coverage=())
    # Safe reason (no colon)
    verdict_ok = Verdict(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                          verdict="consistent", reason=ToolMessage("ok"),
                          tokens=10, seconds=0.1)
    e_ok = evidence_yaml(FunctionName("probe_me"), GuestAddress(0x08000100),
                          ToolMessage("init"), SymbolName("probe_me"), facts, verdict_ok,
                          "run1", tmp_path)
    p1 = yaml.safe_load(e_ok.evidence_yaml)
    assert p1["reason"] == "ok"

    # Unsafe reason (colon)
    verdict_bad = Verdict(name=FunctionName("probe_me"), address=GuestAddress(0x08000100),
                           verdict="contradicts", reason=ToolMessage("a: b"),
                           tokens=10, seconds=0.1)
    e_bad = evidence_yaml(FunctionName("probe_me"), GuestAddress(0x08000100),
                          ToolMessage("init"), SymbolName("probe_me"), facts, verdict_bad,
                          "run1", tmp_path)
    p2 = yaml.safe_load(e_bad.evidence_yaml)
    assert p2["reason"] == "a: b"


# ── selection: which `wf investigate` entries get probed ─────────────

def test_select_runtime_targets_only_picks_investigate_entries_with_captured_calls(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    """Two entries are in `wf investigate`'s outputs; one has captured calls and is newer than
    the other. The selector should return that one first."""
    snap_dir = tmp_path / "snaps"
    (snap_dir / "probe_a").mkdir(parents=True)
    (snap_dir / "probe_a" / "snap_beastsp_1_0.bin").write_bytes(b"\0" * 100)
    (snap_dir / "probe_b").mkdir(parents=True)
    (snap_dir / "probe_b" / "snap_beastsp_2_0.bin").write_bytes(b"\0" * 100)
    monkeypatch.setattr(rt, "SNAPSHOTS", snap_dir)
    monkeypatch.setattr(rt, "investigate_sources_names", lambda: frozenset({"probe_a", "probe_b"}))
    monkeypatch.setattr(rt, "_already_done", lambda name: False)
    targets = select_runtime_targets(DEFAULT_COUNT, (), DEFAULT_STATES, tmp_path)
    assert set(targets) == {"probe_a", "probe_b"}


def test_select_runtime_targets_skips_a_function_with_no_captured_calls(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    monkeypatch.setattr(rt, "SNAPSHOTS", tmp_path / "snaps")
    (tmp_path / "snaps").mkdir(parents=True)
    monkeypatch.setattr(rt, "investigate_sources_names", lambda: frozenset({"probe_a"}))
    monkeypatch.setattr(rt, "_already_done", lambda name: False)
    targets = select_runtime_targets(DEFAULT_COUNT, (), DEFAULT_STATES, tmp_path)
    assert targets == ()


def test_select_runtime_targets_honours_only(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    snap_dir = tmp_path / "snaps"
    (snap_dir / "x").mkdir(parents=True)
    (snap_dir / "x" / "snap_a_0.bin").write_bytes(b"\0" * 100)
    monkeypatch.setattr(rt, "SNAPSHOTS", snap_dir)
    monkeypatch.setattr(rt, "investigate_sources_names", lambda: frozenset({"x"}))
    monkeypatch.setattr(rt, "_already_done", lambda name: False)
    targets = select_runtime_targets(DEFAULT_COUNT, (FunctionName("x"),), DEFAULT_STATES, tmp_path)
    assert targets == (FunctionName("x"),)


def test_select_runtime_targets_skips_already_done(monkeypatch: pytest.MonkeyPatch,
                                                  tmp_path: Path) -> None:
    (tmp_path / "build/runtime/run1").mkdir(parents=True)
    snap_dir = tmp_path / "snaps"
    (snap_dir / "x").mkdir(parents=True)
    (snap_dir / "x" / "snap_a_0.bin").write_bytes(b"\0" * 100)
    (tmp_path / "build/runtime/run1/x.yaml").write_text("# verdict:\nverdict: consistent\n")
    monkeypatch.setattr(rt, "SNAPSHOTS", snap_dir)
    monkeypatch.setattr(rt, "RUNTIME", tmp_path / "build/runtime")
    monkeypatch.setattr(rt, "investigate_sources_names", lambda: frozenset({"x"}))
    monkeypatch.setattr(rt, "_already_done", lambda name: True)
    targets = select_runtime_targets(DEFAULT_COUNT, (), DEFAULT_STATES, tmp_path)
    assert targets == ()


# ── the graph end to end with fakes ──────────────────────────────────

@pytest.fixture
def fake_state(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> RuntimeEvidenceState:
    """A 2-target run with no real probes, no real judge calls."""
    monkeypatch.setattr(graph, "_addresses_of",
                        lambda targets: {name: GuestAddress(0x08000000 + 2 * index)
                                         for index, name in enumerate(targets)})
    monkeypatch.setattr(graph, "_summaries_and_interfaces",
                        lambda targets: ({name: ToolMessage(f"summary {name}") for name in targets},
                                         {name: ToolMessage(f"interface {name}") for name in targets}))
    return RuntimeEvidenceState(run_dir=tmp_path / "runtime-evidence" / "run1", count=4,
                                states=DEFAULT_STATES, jobs=DEFAULT_JOBS, model=DEFAULT_MODEL)


def test_the_graph_writes_a_yaml_per_function(monkeypatch: pytest.MonkeyPatch, tmp_path: Path,
                                              fake_state: RuntimeEvidenceState) -> None:
    targets = (FunctionName("sub_a"), FunctionName("sub_b"))
    facts = {name: RuntimeFacts(name=name, address=GuestAddress(0x08000000), probes=2, returned=2,
                                register_inputs=(), register_outputs=(), registers_written=(),
                                written_patterns=(), io_writes=(), memory_writes=(),
                                route_coverage=(ToolMessage(f"{name}: 3 calls / 1000 frames (3/1k)"),))
             for name in targets}
    verdicts = {
        FunctionName("sub_a"): Verdict(name=FunctionName("sub_a"), address=GuestAddress(0x08000000),
                                       verdict="consistent",
                                       reason=ToolMessage("writes match the summary"),
                                       tokens=100, seconds=1.0),
        FunctionName("sub_b"): Verdict(name=FunctionName("sub_b"), address=GuestAddress(0x08000000),
                                       verdict="contradicts",
                                       reason=ToolMessage("also writes BG0CNT, no I/O in summary"),
                                       tokens=80, seconds=0.9),
    }
    monkeypatch.setattr(graph, "select_runtime_targets", lambda *args, **kwargs: targets)
    monkeypatch.setattr(graph, "run_original",
                        lambda name, states, run_id, work_dir: (make_probe(path=tmp_path / "x.bin"),))
    monkeypatch.setattr(graph, "ask_judge",
                        lambda model, f, summary, interface, run_id: verdicts[f.name])
    monkeypatch.setattr(rt, "run_original",
                        lambda name, states, run_id, work_dir: (make_probe(path=tmp_path / "x.bin"),))
    monkeypatch.setattr(rt, "summarise",
                        lambda name, address, probes: facts[name])
    monkeypatch.setattr(rt, "ask_judge",
                        lambda model, f, summary, interface, run_id: verdicts[f.name])
    monkeypatch.setattr(rt, "_ram_symbols", lambda: [])
    start = fake_state.model_copy(update={"targets": targets, "run_dir": tmp_path / "run1"})
    report = run_runtime_evidence(start, run_id="runtime-evidence-1", tracked=False)
    assert report.selected == 2 and report.gathered == 2
    assert report.consistent == 1 and report.contradicts == 1
    assert report.tokens == 180
    assert sorted(p.name for p in (start.run_dir).glob("*.yaml")) == \
        sorted(["sub_a.yaml", "sub_b.yaml"])
    assert (start.run_dir / "report.tsv").exists()
    assert (start.run_dir / "contradictions.tsv").exists()
    contradictions_text = (start.run_dir / "contradictions.tsv").read_text()
    assert "sub_b" in contradictions_text and "BG0CNT" in contradictions_text


def test_the_graph_finishes_with_nothing_selected(monkeypatch: pytest.MonkeyPatch, tmp_path: Path,
                                                  fake_state: RuntimeEvidenceState) -> None:
    monkeypatch.setattr(graph, "select_runtime_targets", lambda *args, **kwargs: ())
    monkeypatch.setattr(graph, "run_original", lambda *a, **kw: ())
    monkeypatch.setattr(rt, "run_original", lambda *a, **kw: ())
    start = fake_state.model_copy(update={"targets": (), "run_dir": tmp_path / "run2"})
    report = run_runtime_evidence(start, run_id="runtime-evidence-2", tracked=False)
    assert report.selected == 0 and not list((start.run_dir).glob("*.yaml"))


def test_the_graph_handles_a_missing_mgba_run(monkeypatch: pytest.MonkeyPatch, tmp_path: Path,
                                              fake_state: RuntimeEvidenceState) -> None:
    def fail(name: FunctionName, states: int, run_id: str, work_dir: Path) -> tuple[Probe, ...]:
        raise FileNotFoundError("mgba_run not found; build it with tools/build.sh first")
    monkeypatch.setattr(graph, "select_runtime_targets", lambda *args, **kwargs: (FunctionName("sub_a"),))
    monkeypatch.setattr(graph, "run_original", fail)
    monkeypatch.setattr(rt, "run_original", fail)
    targets = (FunctionName("sub_a"),)
    start = fake_state.model_copy(update={"targets": targets, "run_dir": tmp_path / "run3"})
    report = run_runtime_evidence(start, run_id="runtime-evidence-3", tracked=False)
    assert report.selected == 1 and report.not_run == 1
    assert any("mgba_run" in note for note in report.problems)


def test_ask_judge_returns_unknown_when_no_json(monkeypatch: pytest.MonkeyPatch) -> None:
    """A model's reply that holds no JSON object: parse_judge returns ("", reason) and the
    verdict wraps it as unknown, the problem recorded."""
    class _FakeModel:
        def complete(self, *args: object, **kwargs: object) -> str:
            return "no json here"

    monkeypatch.setattr(rt.llm, "complete",
                        lambda *args, **kwargs: "no json here at all")
    facts = RuntimeFacts(name=FunctionName("sub_1"), address=GuestAddress(0x08000000), probes=2,
                         returned=2,
                         register_inputs=(), register_outputs=(), registers_written=(),
                         written_patterns=(), io_writes=(), memory_writes=(),
                         route_coverage=())
    verdict = ask_judge(ModelSpec("minimax/MiniMax-M3"), facts, ToolMessage("a summary"),
                        ToolMessage("(r0) -> void"), run_id="run-x")
    assert verdict.verdict == "unknown"
    assert "JSON object" in (verdict.problem or "") or "JSON object" in verdict.reason


def test_ask_judge_short_circuits_to_unknown_when_402(monkeypatch: pytest.MonkeyPatch) -> None:
    """HTTP 402 (out of credits) returns ("", "HTTP 402") and we want this surfaced as a
    clean stop -- the workflow pauses once on a 402 and saves what is done."""
    def error_complete(*args: object, **kwargs: object) -> str:
        raise RuntimeError("minimax/MiniMax-M3: HTTP 402: out of credits")

    monkeypatch.setattr(rt.llm, "complete", error_complete)
    facts = RuntimeFacts(name=FunctionName("sub_1"), address=GuestAddress(0x08000000), probes=2,
                         returned=2,
                         register_inputs=(), register_outputs=(), registers_written=(),
                         written_patterns=(), io_writes=(), memory_writes=(),
                         route_coverage=())
    verdict = ask_judge(ModelSpec("minimax/MiniMax-M3"), facts, ToolMessage("a summary"),
                        ToolMessage("(r0) -> void"), run_id="run-402")
    assert verdict.verdict == "unknown"
    assert verdict.problem is not None and "HTTP 402" in verdict.problem


# ── contrastive judge: candidates, prompt, parse, verdict mapping ─────


def test_build_contrastive_candidates_picks_two_hard_two_random() -> None:
    """The 5 options: the truth plus 4 decoys (2 hard, 2 random), shuffled with a recorded
    seed so a re-run is reproducible. The `is_truth` flag is set on exactly one option."""
    facts = RuntimeFacts(name=FunctionName("ai_attackStepCommitAndHit"),
                          address=GuestAddress(0x08000000), probes=2, returned=2,
                          register_inputs=(), register_outputs=(), registers_written=(),
                          written_patterns=(), io_writes=(),
                          memory_writes=(MemoryRegionWrite(symbol="eAIData1_AttackVars",
                                                              address=0, bytes=84,
                                                              distinct_values=9),),
                          route_coverage=())
    pool = frozenset({"ai_attackStepCommitAndHit", "ai_attackStepLand", "ai_attackCommit",
                       "ai_attackPanelHandler", "ai_attackSetupVars",
                       "battle_clearAttackVars", "battle_runPwrAtkStep",
                       "ow_applySpawnEntry"})
    rng = random.Random(7)
    candidates = build_contrastive_candidates(FunctionName("ai_attackStepCommitAndHit"),
                                                "Sets up an attacker's attack state.",
                                                pool, facts,
                                                lambda n: f"summary of {n}", rng)
    assert len(candidates) == 5
    labels = [c.label for c in candidates]
    assert sorted(labels) == [1, 2, 3, 4, 5]
    truth = [c for c in candidates if c.is_truth]
    assert len(truth) == 1
    assert truth[0].name == "ai_attackStepCommitAndHit"
    decoys = [c for c in candidates if not c.is_truth]
    assert len(decoys) == 4
    # all decoys are pool members (no overlap with truth)
    assert {c.name for c in decoys} <= pool - {"ai_attackStepCommitAndHit"}


def test_build_contrastive_candidates_uses_recognised_prefix_for_hard_decoys() -> None:
    """A function whose name starts with `ai_attack` should pick other `ai_attack*` functions as
    hard decoys (the prefix is the only signal we have without loading each decoy's facts)."""
    facts = RuntimeFacts(name=FunctionName("ai_attackStepCommitAndHit"),
                          address=GuestAddress(0x08000000), probes=2, returned=2,
                          register_inputs=(), register_outputs=(), registers_written=(),
                          written_patterns=(), io_writes=(), memory_writes=(),
                          route_coverage=())
    pool = frozenset({"ai_attackStepCommitAndHit", "ai_attackStepLand",
                       "ai_attackCommitMoveAttack", "ai_attackStepDispatchSubStep",
                       "battle_clearAttackVars", "battle_runPwrAtkStep"})
    rng = random.Random(3)
    candidates = build_contrastive_candidates(FunctionName("ai_attackStepCommitAndHit"),
                                                "summary", pool, facts,
                                                lambda n: f"summary of {n}", rng)
    hard = [c for c in candidates if not c.is_truth]
    hard_names = [c.name for c in hard[:2]] if len(hard) >= 2 else [c.name for c in hard]
    # the first two decoys should prefer the ai_attack* pool
    assert sum(1 for name in hard_names if name.startswith("ai_attack")) >= 1


def test_contrastive_judge_prompt_does_not_leak_the_truth_label() -> None:
    """The prompt shows the runtime facts and five (label, name, summary) candidates without
    any marker that says which is the truth. If the truth were leaked, the judge could pin the
    answer by string-matching, defeating the contrastive design."""
    facts = RuntimeFacts(name=FunctionName("foo"), address=GuestAddress(0x08000000),
                          probes=1, returned=1,
                          register_inputs=(), register_outputs=(), registers_written=(),
                          written_patterns=(), io_writes=(), memory_writes=(),
                          route_coverage=())
    candidates = [ContrastiveCandidate(label=idx, name=FunctionName(f"name_{idx}"),
                                          summary=f"summary_{idx}", is_truth=(idx == 3))
                    for idx in range(1, 6)]
    prompt = contrastive_judge_prompt(facts, candidates)
    assert "name_3" in prompt
    assert "is_truth" not in prompt and "truth_label" not in prompt
    # the 5 candidates are present, each as `[N] name: ...`
    for idx in range(1, 6):
        assert f"[{idx}] name: `name_{idx}`" in prompt


def test_parse_contrastive_extracts_choice_confidence_reason() -> None:
    choice, conf, reason, problem = parse_contrastive(json.dumps({"choice": 3, "confidence": 0.9,
                                                                    "reason": "RAM symbol matches"}))
    assert problem is None
    assert choice == 3 and abs(conf - 0.9) < 1e-6
    assert "RAM symbol" in reason


def test_parse_contrastive_picks_choice_zero_when_nothing_fits() -> None:
    choice, conf, _reason, problem = parse_contrastive(json.dumps({"choice": 0, "confidence": 1.0,
                                                                     "reason": "no fit"}))
    assert problem is None and choice == 0


def test_parse_contrastive_rejects_an_unparseable_reply() -> None:
    choice, conf, reason, problem = parse_contrastive("no json here at all")
    assert problem is not None
    assert choice == 0 and conf == 0.0 and reason == ""


def test_parse_contrastive_rejects_boolean_choice_and_confidence() -> None:
    """`bool` is a subclass of `int` in Python; a reply with `"choice": true` must NOT be
    accepted as choice 1 (which would silently make the model look like it picked the first
    candidate with full confidence). The current parser rejects both choices and confidences
    that are booleans, falling back to `unparsed`."""
    choice, conf, reason, problem = parse_contrastive(json.dumps({"choice": True,
                                                                    "confidence": 0.9,
                                                                    "reason": "x"}))
    assert problem is not None
    assert choice == 0 and conf == 0.0 and reason == ""
    choice, conf, reason, problem = parse_contrastive(json.dumps({"choice": 1,
                                                                    "confidence": True,
                                                                    "reason": "x"}))
    assert problem is not None
    assert choice == 0 and conf == 0.0 and reason == ""


def test_ask_contrastive_judge_returns_supported_when_truth_picked(monkeypatch: pytest.MonkeyPatch) -> None:
    """The model picks the truth in the 5 options with confidence above the threshold."""
    fake_reply = json.dumps({"choice": 3, "confidence": 0.9,
                              "reason": "matches the writes"})
    monkeypatch.setattr(rt.llm, "complete", lambda *args, **kwargs: fake_reply)
    candidates = [ContrastiveCandidate(label=idx, name=FunctionName(f"name_{idx}"),
                                          summary=f"summary_{idx}", is_truth=(idx == 3))
                    for idx in range(1, 6)]
    facts = RuntimeFacts(name=FunctionName("name_3"), address=GuestAddress(0x08000000),
                          probes=2, returned=2,
                          register_inputs=(), register_outputs=(), registers_written=(),
                          written_patterns=(), io_writes=(), memory_writes=(),
                          route_coverage=())
    verdict = ask_contrastive_judge(ModelSpec("minimax/MiniMax-M3"), facts, candidates, "run-x")
    assert verdict.verdict == "supported"
    assert "name_3" in verdict.reason


def test_ask_contrastive_judge_returns_contradicted_when_decoy_picked_at_high_confidence(
        monkeypatch: pytest.MonkeyPatch) -> None:
    fake_reply = json.dumps({"choice": 2, "confidence": 0.8,
                              "reason": "looks like a panel handler"})
    monkeypatch.setattr(rt.llm, "complete", lambda *args, **kwargs: fake_reply)
    candidates = [ContrastiveCandidate(label=idx, name=FunctionName(f"name_{idx}"),
                                          summary=f"summary_{idx}", is_truth=(idx == 3))
                    for idx in range(1, 6)]
    facts = RuntimeFacts(name=FunctionName("name_3"), address=GuestAddress(0x08000000),
                          probes=2, returned=2,
                          register_inputs=(), register_outputs=(), registers_written=(),
                          written_patterns=(), io_writes=(), memory_writes=(),
                          route_coverage=())
    verdict = ask_contrastive_judge(ModelSpec("minimax/MiniMax-M3"), facts, candidates, "run-x")
    assert verdict.verdict == "contradicted"
    assert "name_2" in verdict.reason
    assert "0.8" in verdict.reason


def test_ask_contrastive_judge_returns_inconclusive_for_low_confidence_decoy(
        monkeypatch: pytest.MonkeyPatch) -> None:
    """A pick at confidence below the threshold (0.7) becomes `inconclusive`, not
    `contradicted` -- the test would otherwise overstate the contradiction rate."""
    fake_reply = json.dumps({"choice": 2, "confidence": 0.5,
                              "reason": "matches some"})
    monkeypatch.setattr(rt.llm, "complete", lambda *args, **kwargs: fake_reply)
    candidates = [ContrastiveCandidate(label=idx, name=FunctionName(f"name_{idx}"),
                                          summary=f"summary_{idx}", is_truth=(idx == 3))
                    for idx in range(1, 6)]
    facts = RuntimeFacts(name=FunctionName("name_3"), address=GuestAddress(0x08000000),
                          probes=2, returned=2,
                          register_inputs=(), register_outputs=(), registers_written=(),
                          written_patterns=(), io_writes=(), memory_writes=(),
                          route_coverage=())
    verdict = ask_contrastive_judge(ModelSpec("minimax/MiniMax-M3"), facts, candidates, "run-x")
    assert verdict.verdict == "inconclusive"


def test_ask_contrastive_judge_returns_inconclusive_when_choice_zero(
        monkeypatch: pytest.MonkeyPatch) -> None:
    """The model honestly says it cannot pick any candidate (choice 0 = none fit)."""
    fake_reply = json.dumps({"choice": 0, "confidence": 0.95,
                              "reason": "no candidate matches"})
    monkeypatch.setattr(rt.llm, "complete", lambda *args, **kwargs: fake_reply)
    candidates = [ContrastiveCandidate(label=idx, name=FunctionName(f"name_{idx}"),
                                          summary=f"summary_{idx}", is_truth=(idx == 3))
                    for idx in range(1, 6)]
    facts = RuntimeFacts(name=FunctionName("name_3"), address=GuestAddress(0x08000000),
                          probes=2, returned=2,
                          register_inputs=(), register_outputs=(), registers_written=(),
                          written_patterns=(), io_writes=(), memory_writes=(),
                          route_coverage=())
    verdict = ask_contrastive_judge(ModelSpec("minimax/MiniMax-M3"), facts, candidates, "run-x")
    assert verdict.verdict == "inconclusive"


def test_ask_contrastive_judge_returns_inconclusive_when_402(monkeypatch: pytest.MonkeyPatch) -> None:
    """HTTP 402: same surface as the legacy judge's `unknown`: the verdict is `inconclusive`
    and the problem carries the error."""
    def error_complete(*args: object, **kwargs: object) -> str:
        raise RuntimeError("minimax/MiniMax-M3: HTTP 402: out of credits")

    monkeypatch.setattr(rt.llm, "complete", error_complete)
    candidates = [ContrastiveCandidate(label=idx, name=FunctionName(f"name_{idx}"),
                                          summary=f"summary_{idx}", is_truth=(idx == 3))
                    for idx in range(1, 6)]
    facts = RuntimeFacts(name=FunctionName("name_3"), address=GuestAddress(0x08000000),
                          probes=2, returned=2,
                          register_inputs=(), register_outputs=(), registers_written=(),
                          written_patterns=(), io_writes=(), memory_writes=(),
                          route_coverage=())
    verdict = ask_contrastive_judge(ModelSpec("minimax/MiniMax-M3"), facts, candidates, "run-402")
    assert verdict.verdict == "inconclusive"
    assert verdict.problem is not None and "HTTP 402" in verdict.problem


# ── regression: reproduce every hostile-reason shape the previous batch tripped on ─


@pytest.mark.parametrize("nasty_reason", [
    # from the actual previous 28 broken files (each had a "colon-space" mid-sentence)
    "R3 cleared (-1 -> 0). Interface matches: dx<0 yields direction 3.",
    "matches: r0 was 0x00000000 -> 0x08000020 (call site)",
    "AI object pointer: r5 holds 0x0203AA88. reset path: release panel reservations.",
    "phase counter updates: r0: 0x50->0x80020064, panel: cleared",
    "if value is non-zero: branch taken; else: branch not taken",
    "check: r0 = 0; flags: cleared; panel: released",
    "1:2:3:4:5 (a ratio, not a mapping)",
    "two-step: A) read B) write",
    "{\\\"foo\\\": \\\"bar\\\"}",
])
def test_write_per_function_survives_a_hostile_reason(tmp_path: Path,
                                                       nasty_reason: str) -> None:
    """For every shape of `colon-space`-bearing reason text the previous batch produced
    (the batch had 28 files that failed to round-trip), the new code round-trips the
    file through yaml.safe_load and keeps the reason byte-for-byte."""
    facts = RuntimeFacts(name=FunctionName("ai_attackFrontPanel"),
                          address=GuestAddress(0x080F7E40), probes=8, returned=8,
                          register_inputs=(), register_outputs=(), registers_written=(),
                          written_patterns=(), io_writes=(),
                          memory_writes=(MemoryRegionWrite(symbol="eAIData3_AttackVars",
                                                              address=0, bytes=84,
                                                              distinct_values=8),),
                          route_coverage=())
    verdict = Verdict(name=FunctionName("ai_attackFrontPanel"),
                       address=GuestAddress(0x080F7E40),
                       verdict="consistent",
                       reason=ToolMessage(nasty_reason), tokens=100, seconds=1.0)
    evidence = evidence_yaml(FunctionName("ai_attackFrontPanel"), GuestAddress(0x080F7E40),
                              ToolMessage("a summary"), SymbolName("ai_attackFrontPanel"), facts,
                              verdict, "run-1", tmp_path)
    path = write_per_function(evidence, tmp_path)
    loaded = yaml.safe_load(path.read_text())
    assert loaded["reason"] == nasty_reason, f"mangled: {loaded['reason']!r}"
    assert loaded["evidence"][0]["method"] == "runtime"
    assert "eAIData3_AttackVars" in loaded["evidence"][0]["detail"]
