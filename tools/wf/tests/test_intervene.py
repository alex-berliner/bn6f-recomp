"""Tests for the `wf intervene` workflow: ROM patching, the per-anchor diff of the
observed frames, the small typed predicate language, and the function lookup. The
end-to-end mGBA run is exercised by `test_intervene_null_is_deterministic`, which is
skipped when mgba_run / the BIOS are missing."""
from __future__ import annotations

from pathlib import Path

import pytest

from wf.tools import intervene as it
from wf.graphs.intervene import WriteInput
from wf.tools.intervene import (AnchorDiff, FunctionInfo, InterventionKind, OamEntriesMissing,
                                 Route, RouteName, evaluate_predicate, find_function, load_anchors,
                                 make_predicate, patch_bytes, patch_for, patch_rom, return_k_bytes,
                                 select_intervene_targets)

ROM = Path("/home/alex/Code/recomp/bn6f/bn6f.gba")
BIOS = Path("/home/alex/Code/recomp/roms/gba_bios.bin")


def _mgba_run() -> Path:
    """Resolve mgba_run the way the tool does (paths.env), not by hardcoding a path:
    the binary lives in build-tools/, the source lives in tools/."""
    import gamepaths
    return Path(gamepaths.MGBA_RUN)


# ── patching ────────────────────────────────────────────────────────────────

def test_patch_bytes_replaces_slice() -> None:
    orig = bytes(range(20))
    out = patch_bytes(orig, 4, b"\xaa\xbb")
    assert out[:4] == orig[:4]
    assert out[4:6] == b"\xaa\xbb"
    assert out[6:] == orig[6:]
    assert len(out) == len(orig)


def test_patch_bytes_rejects_out_of_range() -> None:
    with pytest.raises(ValueError, match="outside ROM"):
        patch_bytes(b"abc", 3, b"abcd")


def test_patch_bytes_rejects_odd_offset() -> None:
    with pytest.raises(ValueError, match="odd byte address"):
        patch_bytes(b"abcd", 1, b"ab")


def test_return_k_bytes_encodes_movs_then_a_mode_preserving_return() -> None:
    """`return:K` = `movs r0,#K; <chosen return>`. With no `function`, the default
    return is `mov pc, lr` (the safe mode-preserving 2-byte form); the call_m4aSoundMain
    regression and the per-return-kind tests exercise the other kinds.

    The crash this rewrites: the callee's own `pop {pc}` leaves the instruction set
    alone whatever LR's bit 0 says; `bx lr` does not, and the IWRAM trampolines that
    call into ROM functions leave LR with bit 0 clear (`mov lr, pc`). See SKIP_BYTES
    for the helper that decides which encoding to use."""
    for k in (0, 1, 5, 200, 255):
        b = return_k_bytes(k)
        assert len(b) == 4                          # movs (2 bytes) + return (2 bytes)
        assert int.from_bytes(b[0:2], "little") == 0x2000 | k   # movs r0, #k
        assert int.from_bytes(b[2:4], "little") == 0x46F7       # mov pc, lr


def test_skip_bytes_are_mode_preserving_default() -> None:
    """`SKIP_BYTES` = `mov pc, lr` (0x46F7, bytes F7 46): the safe default.

    Regression for the call_m4aSoundMain crash: on battles1 that function is entered
    from an IWRAM trampoline whose `mov lr, pc; bx r0; pop {r0}; bx r0` leaves an
    even LR (no Thumb bit) when it returns into the callee. The original v1 patch
    used `bx lr` here, which switches the CPU to ARM state when LR's bit 0 is clear,
    decodes the THUMB `pop {r0}; bx r0` as garbage, and walks into EWRAM. `mov
    pc, lr` does NOT take the instruction set on ARMv4T (`bx` does), and `pop
    {pc}` is also mode-agnostic; `bx lr` (0x4770) is the only encoding that breaks.
    See skip_bytes_for for the per-function kind."""
    assert it.SKIP_BYTES == b"\xf7\x46"
    assert len(it.SKIP_BYTES) == 2
    assert int.from_bytes(it.SKIP_BYTES, "little") == 0x46F7   # mov pc, lr
    assert int.from_bytes(b"\x70\x47", "little") == 0x4770     # bx lr (the encoding that crashed)


def test_skip_bytes_for_chooses_push_pop_pc_for_pop_pc_return() -> None:
    """`skip_bytes` is a pure dispatcher on the detected return kind. Per the
    brief: 'pop_pc' / 'mov_pc' / 'mov_pc_lr' / 'unknown' → `mov pc, lr`
    (mode-preserving, safe when LR's bit 0 is clear -- the IWRAM-trampoline
    case that crashed `bx lr`); 'bx_lr' → `bx lr` (matches what a function
    did originally when the caller's LR has bit 0 set)."""
    assert it.skip_bytes("pop_pc") == b"\xf7\x46"
    assert it.skip_bytes("mov_pc") == b"\xf7\x46"
    assert it.skip_bytes("mov_pc_lr") == b"\xf7\x46"
    assert it.skip_bytes("bx_lr") == b"\x70\x47"
    assert it.skip_bytes("unknown") == b"\xf7\x46"


def test_skip_bytes_for_integration_detects_pop_pc_from_real_function() -> None:
    """`skip_bytes_for(call_m4aSoundMain)` reads the function's last bytes via the
    real bn6f.elf ROM and finds a `pop {pc}`; the patch bytes are `mov pc, lr`.
    `call_m4aSoundMain`'s last instruction is `pop {pc}` at 0x080005b2."""
    fn = find_function("call_m4aSoundMain")
    payload, kind = it.skip_bytes_for(fn)
    assert kind == "pop_pc"
    assert payload == b"\xf7\x46"


def test_skip_bytes_for_integration_detects_bx_lr_from_real_function() -> None:
    """A bn6f function whose epilogue is `bx lr` gets the matching `bx lr` patch
    bytes. find_function refuses a 2-byte bx-lr nullsub (PATCH_HEAD_MIN = 4), so we
    build a FunctionInfo directly with the address+size we've measured. `nm`
    attributes bytes after the last `bx lr` instruction to the function, so the
    last-2-bytes test sometimes picks up padding from a sibling symbol; we instead
    ask `return_kind_for` (which decodes the function's instructions) and trust its
    classification."""
    info = it._load_b6f_info()
    candidates: list[tuple[int, int, str]] = []
    for addr, (mode, size, nm) in info.funcs.items():
        if mode != "t" or size < 4:
            continue
        offset = info.rom_offset(addr)
        if offset is None:
            continue
        body = info.rom[offset: offset + size]
        if b"\x70\x47" not in body:
            continue
        candidates.append((addr, size, nm))
    if not candidates:
        pytest.skip("no bx-lr-containing function in this bn6f.elf snapshot")
    chosen = next((c for c in candidates
                   if it.return_kind_for(FunctionInfo(name=c[2], address=c[0], size=c[1],
                                                      mode="t")) == "bx_lr"),
                  None)
    if chosen is None:
        pytest.skip("none of the bx-lr-containing functions classify as bx_lr"
                    " (all have a later pop_pc / mov_pc epilogue)")
    addr, size, name = chosen
    payload, kind = it.skip_bytes_for(FunctionInfo(name=name, address=addr, size=size, mode="t"))
    assert kind == "bx_lr"
    assert payload == b"\x70\x47"


def test_return_kind_reads_instructions_not_the_literal_pool() -> None:
    """The return kind has to come from the function's *instructions*.

    `cgbSoundInit_814EB84` (0x0814eb84) ends with the data word 0x4770df2a, whose
    low half is exactly `bx lr` (0x4770). A byte-pattern scan of the function's tail
    reads that as the function's epilogue and would hand the function the `bx lr`
    patch -- the encoding that crashes when the caller's LR has bit 0 clear. The
    ELF's `$d` mapping symbol marks the literal pool as data, so the instruction-level
    detection skips it and sees the function's real exit (`bx <reg>`)."""
    fn = find_function("cgbSoundInit_814EB84")
    kind = it.return_kind_for(fn)
    assert kind != "bx_lr", f"the literal pool's 0x4770 was read as an epilogue: {kind}"
    payload, kind = it.skip_bytes_for(fn)
    assert payload == b"\xf7\x46"          # the mode-preserving return
    assert kind != "bx_lr"


def test_return_kind_reads_the_real_epilogue_of_known_functions() -> None:
    """Three functions whose epilogue the disassembly shows, checked against the
    detection: `call_m4aSoundMain` ends in `pop {pc}` (0x080005b2),
    `IsScreenFadeActive` ends in `mov pc, lr` (0x08006308) and `getStruct0BHalfptr`
    ends in `bx lr` (0x08146726)."""
    expected = {
        "call_m4aSoundMain": "pop_pc",
        "IsScreenFadeActive": "mov_pc_lr",
        "getStruct0BHalfptr": "bx_lr",
    }
    for name, kind in expected.items():
        fn = find_function(name)
        assert it.return_kind_for(fn) == kind, f"{name}: {it.return_kind_for(fn)} != {kind}"
        payload, _kind = it.skip_bytes_for(fn)
        assert payload == (b"\x70\x47" if kind == "bx_lr" else b"\xf7\x46")


def test_a_function_with_several_exits_gets_the_mode_preserving_return() -> None:
    """A function with more than one exit is only allowed `bx lr` when EVERY exit is
    `bx lr`: `mov pc, lr` is correct whether the caller left LR's bit 0 set or
    clear, `bx lr` is not. So one `pop {…,pc}` anywhere in the function decides the
    whole patch."""
    fn = find_function("call_m4aSoundMain")
    assert it._function_returns(it._load_b6f_info(), int(fn.address), int(fn.size))
    kinds = it._function_returns(it._load_b6f_info(), int(fn.address), int(fn.size))
    assert "pop_pc" in kinds
    assert it.skip_bytes_for(fn)[0] == b"\xf7\x46"


def test_return_k_bytes_for_mirrors_skip_kind() -> None:
    """For each kind, the trailing 2-byte return matches `skip_bytes` for that
    kind. The first halfword is always `movs r0, #(K<<0)` (LSB set = 8-bit
    immediate form)."""
    for k in (0, 1, 7, 200, 255):
        assert it.return_k_bytes_for(k, kind="pop_pc")[0] == bytes([0x00 | k, 0x20, 0xf7, 0x46])
        assert it.return_k_bytes_for(k, kind="bx_lr")[0] == bytes([0x00 | k, 0x20, 0x70, 0x47])
        assert it.return_k_bytes_for(k, kind="unknown")[0] == bytes([0x00 | k, 0x20, 0xf7, 0x46])


def test_return_k_bytes_range_checked() -> None:
    with pytest.raises(ValueError):
        return_k_bytes(-1)
    with pytest.raises(ValueError):
        return_k_bytes(256)


def test_patch_for_skip_is_mov_pc_lr_for_pop_pc_function() -> None:
    """skip's encoding tracks the function's own kind: a function whose epilogue
    is `pop {pc}` gets the 2-byte `mov pc, lr` patch."""
    fn = FunctionInfo(name="Test", address=0x08000100, size=24, mode="t")
    baseline = b"\x00\xb5" + bytes(8) + b"\x00\xbd"             # push {lr}; ...; pop {pc}
    out = patch_for("skip", None, fn, baseline)
    assert out == b"\xf7\x46"


def test_patch_for_return_is_movs_then_mov_pc_lr() -> None:
    """return:K = `movs r0,#K; <chosen return>`. With this baseline (pop_pc
    epilogue) the chosen return is `mov pc, lr` -- 4 total bytes, where v1 was 6
    (movs; push {lr}; pop {pc}). The new patch is one instruction shorter and
    reproduces the function's own epilogue encoding."""
    fn = FunctionInfo(name="Test", address=0x08000100, size=24, mode="t")
    baseline = b"\x00\xb5" + bytes(8) + b"\x00\xbd"             # push {lr}; ...; pop {pc}
    out = patch_for("return", 5, fn, baseline)
    assert out == b"\x05\x20\xf7\x46"


def test_patch_for_null_rewrites_own_bytes() -> None:
    """The null control must produce a byte-identical ROM: it re-writes the
    function's own first instructions, so any diff is emulator nondeterminism,
    not the patch."""
    fn = FunctionInfo(name="Test", address=0x08000100, size=24, mode="t")
    baseline = b"\x00\xb5" + bytes(8) + b"\x00\xbd"             # push {lr}; ...; pop {pc}
    assert patch_for("null", None, fn, baseline) == baseline[: len(it.SKIP_BYTES)]


def test_patch_for_rejects_function_too_short() -> None:
    fn = FunctionInfo(name="T", address=0x08000100, size=2, mode="t")
    with pytest.raises(ValueError, match="cannot fit"):
        patch_for("return", 0, fn, b"\x00\xb5")


def test_patch_for_rejects_unknown_intervention() -> None:
    fn = FunctionInfo(name="T", address=0x08000100, size=24, mode="t")
    with pytest.raises(ValueError, match="unknown intervention"):
        patch_for("nuke", None, fn, b"\x00\xb5\x4e\xf1\xb3")


def test_patch_head_min_fits_the_longest_patch() -> None:
    """find_function must refuse anything shorter than the longest patch (return:K
    on a `pop_pc` function: 4 bytes -- `movs r0,#K; <return>`), so a 2-byte
    function gets refused rather than half-patched."""
    assert it.PATCH_HEAD_MIN == 4
    assert it.PATCH_HEAD_MIN >= max(len(it.SKIP_BYTES), len(return_k_bytes(255)))


def test_patch_rom_writes_new_file_and_leaves_original(tmp_path: Path) -> None:
    if not ROM.exists():
        pytest.skip("bn6f.gba not present")
    fn = find_function("call_m4aSoundMain")
    original = ROM.read_bytes()
    skip_pay, _kind = it.skip_bytes_for(fn)             # 2-byte mov pc, lr (call_m4aSoundMain returns with pop {pc})
    out = patch_rom(ROM, fn, "skip", None, tmp_path)
    assert out.exists()
    patched = out.read_bytes()
    assert len(patched) == len(original)
    off = it._load_b6f_info().rom_offset(fn.address)
    assert off is not None
    assert patched[off:off + len(skip_pay)] == skip_pay
    assert patched[:off] == original[:off]                # nothing before
    assert patched[off + len(skip_pay):] == original[off + len(skip_pay):]
    assert ROM.read_bytes() == original                   # original untouched


def test_patch_rom_null_is_byte_identical(tmp_path: Path) -> None:
    if not ROM.exists():
        pytest.skip("bn6f.gba not present")
    fn = find_function("call_m4aSoundMain")
    out = patch_rom(ROM, fn, "null", None, tmp_path)
    assert out.read_bytes() == ROM.read_bytes()


# ── function lookup ────────────────────────────────────────────────────────

def test_find_function_by_name() -> None:
    finfo = find_function("call_m4aSoundMain")
    assert finfo.mode == "t"
    assert 0x08000000 <= int(finfo.address) < 0x0A000000
    assert finfo.size >= 4


def test_find_function_by_address() -> None:
    by_name = find_function("call_m4aSoundMain")
    by_addr = find_function(f"{by_name.address:08x}")
    assert by_addr.address == by_name.address
    assert by_addr.name == by_name.name


def test_find_function_refuses_arm() -> None:
    with pytest.raises(ValueError, match="ARM"):
        find_function("GameEntryPoint")


def test_find_function_refuses_ram() -> None:
    with pytest.raises(ValueError, match="RAM"):
        find_function("palette_addOrSubtractColor")          # 0x03005ef0


def test_find_function_refuses_unknown() -> None:
    with pytest.raises(ValueError):
        find_function("not_a_real_symbol_zzz")


# ── intervention kind parsing ──────────────────────────────────────────────

def test_intervention_kind_parse_skip() -> None:
    assert InterventionKind.parse("skip") == ("skip", None)


def test_intervention_kind_parse_return() -> None:
    assert InterventionKind.parse("return:5") == ("return:K", 5)
    assert InterventionKind.parse("return:0") == ("return:K", 0)
    assert InterventionKind.parse("return:255") == ("return:K", 255)


def test_intervention_kind_parse_return_out_of_range() -> None:
    with pytest.raises(ValueError):
        InterventionKind.parse("return:256")


def test_intervention_kind_parse_null() -> None:
    assert InterventionKind.parse("null") == ("null", None)


def test_intervention_kind_parse_rejects_unknown() -> None:
    with pytest.raises(ValueError):
        InterventionKind.parse("garbage")


def test_intervention_kind_k_defaults_to_zero() -> None:
    assert InterventionKind.parse("return") == ("return:K", 0)


def test_canonical_strips_the_k() -> None:
    """The graph branches on the *kind* and carries K separately, so 'return:K' as a
    kind matches neither branch and the graph dies with '0 of these held'."""
    assert InterventionKind.canonical("return:K") == "return"
    assert InterventionKind.canonical("return") == "return"
    assert InterventionKind.canonical("skip") == "skip"
    assert InterventionKind.canonical("null") == "null"


def test_state_canonicalises_a_return_k_kind() -> None:
    """Guard the exact crash: --intervention return:1 used to reach the graph as the
    kind 'return:K', which no transition matched."""
    from wf.graphs.intervene import InterveneState
    state = InterveneState(run_dir=Path("/tmp"), function_name="IsScreenFadeActive",
                           intervention="return:K", intervention_k=1)
    assert state.intervention == "return"
    assert state.intervention_k == 1
    from wf.graphs.intervene import is_a_real_intervention, is_the_null_run
    assert is_a_real_intervention(state) is True
    assert is_the_null_run(state) is False


def test_skip_and_null_states_route_to_the_right_branch() -> None:
    from wf.graphs.intervene import (InterveneState, is_a_real_intervention, is_the_null_run)
    for kind, real, null in (("skip", True, False), ("return", True, False), ("null", False, True)):
        state = InterveneState(run_dir=Path("/tmp"), function_name="f", intervention=kind)
        assert is_a_real_intervention(state) is real
        assert is_the_null_run(state) is null


# ── predicates ─────────────────────────────────────────────────────────────

def _make_runrecord() -> "it.RunRecord":
    diffs = (
        AnchorDiff(name="oam", kind="memory", first_frame=10, differing_frames=4,
                    size=42, unit="bytes"),
        AnchorDiff(name="palette", kind="memory", first_frame=None, differing_frames=0,
                    size=0, unit="bytes"),
        AnchorDiff(name="screen", kind="screen", first_frame=120, differing_frames=1,
                    size=2048, unit="pixels"),
        AnchorDiff(name="sound_io", kind="register", first_frame=15, differing_frames=10,
                    size=20, unit="registers"),
    )
    from wf.types import FunctionName, GuestAddress
    return it.RunRecord(function=FunctionName("T"), address=GuestAddress(0x08000100),
                        intervention="skip", route=RouteName("wander"),
                        rom_hashes=("a" * 40, "b" * 40),
                        start_frame=10, lead_in_frames=4, first_frame_executed=10,
                        frames_observed=120, deterministic=True, hung=False, notes=(),
                        anchor_diffs=diffs, shot_baseline=None, shot_patched=None,
                        shot_diff=None, artifacts_dir="/tmp")


def _without(rec: "it.RunRecord", name: str) -> "it.RunRecord":
    return rec.model_copy(update={"anchor_diffs": tuple(d for d in rec.anchor_diffs if d.name != name)})


def test_differs_passes_and_within_window() -> None:
    rec = _make_runrecord()
    assert evaluate_predicate(make_predicate("differs(anchor=oam)"), rec) is True
    assert evaluate_predicate(make_predicate("differs(anchor=oam, within=60)"), rec) is True


def test_differs_within_too_short_fails() -> None:
    rec = _make_runrecord()
    assert evaluate_predicate(make_predicate("differs(anchor=oam, within=5)"), rec) is False


def test_differs_false_on_unchanged_anchor() -> None:
    rec = _make_runrecord()
    assert evaluate_predicate(make_predicate("differs(anchor=palette)"), rec) is False


def test_unchanged_passes_on_clean_anchor() -> None:
    rec = _make_runrecord()
    assert evaluate_predicate(make_predicate("unchanged(anchor=palette, for=120)"), rec) is True


def test_unchanged_fails_when_diff_lands_inside_window() -> None:
    rec = _make_runrecord()                     # oam first differs at frame 10
    assert evaluate_predicate(make_predicate("unchanged(anchor=oam, for=120)"), rec) is False


def test_unchanged_passes_when_diff_lands_after_window() -> None:
    rec = _make_runrecord()                     # screen first differs at frame 120
    assert evaluate_predicate(make_predicate("unchanged(anchor=screen, for=60)"), rec) is True


def test_oam_entries_missing_true_false_and_indeterminate() -> None:
    rec = _make_runrecord()                     # oam: 42 bytes differ, first at frame 10
    assert evaluate_predicate(make_predicate("oam_entries_missing(min=2, first_after=5)"), rec) is True
    assert evaluate_predicate(make_predicate("oam_entries_missing(min=80, first_after=5)"), rec) is False
    assert evaluate_predicate(make_predicate("oam_entries_missing(min=2, first_after=50)"), rec) is False
    assert evaluate_predicate(
        make_predicate("oam_entries_missing(min=2, first_after=5)"), _without(rec, "oam")) is None


def test_oam_entries_missing_returns_typed_predicate() -> None:
    pred = make_predicate("oam_entries_missing(min=4, first_after=10)")
    assert isinstance(pred, OamEntriesMissing)
    assert (pred.min_k, pred.first_after) == (4, 10)


def test_screen_differs_passes_when_screen_changed() -> None:
    assert evaluate_predicate(
        make_predicate("screen_differs(region=0,0,240,160)"), _make_runrecord()) is True


def test_screen_differs_false_when_screen_static() -> None:
    rec = _make_runrecord()
    rec = rec.model_copy(update={"anchor_diffs": tuple(
        d.model_copy(update={"differing_frames": 0}) if d.name == "screen" else d
        for d in rec.anchor_diffs)})
    assert evaluate_predicate(make_predicate("screen_differs(region=0,0,240,160)"), rec) is False


def test_sound_write_missing_true_and_false() -> None:
    rec = _make_runrecord()
    # sound_io differs across the window -> the writes are missing -> False.
    assert evaluate_predicate(make_predicate("sound_write_missing(reg=sound_io)"), rec) is False
    # An anchor that did not differ -> the writes are all still there -> True.
    assert evaluate_predicate(make_predicate("sound_write_missing(reg=NR10)"), rec) is True


def test_ram_anchor_threshold_both_ways() -> None:
    rec = _make_runrecord()                     # screen diff magnitude = 2048
    assert evaluate_predicate(
        make_predicate("ram_anchor(name=screen) baseline_minus_patched >= 1024"), rec) is True
    assert evaluate_predicate(
        make_predicate("ram_anchor(name=screen) baseline_minus_patched >= 4096"), rec) is False
    # An anchor that never differed is zero-magnitude -> below any positive threshold.
    assert evaluate_predicate(
        make_predicate("ram_anchor(name=palette) baseline_minus_patched >= 16"), rec) is False


def test_predicate_parse_rejects_unknown() -> None:
    with pytest.raises(ValueError, match="unknown predicate"):
        make_predicate("garbage_thing(a=1)")


# ── report rows carry their own record's notes ─────────────────────────────

def test_report_rows_do_not_share_notes(tmp_path: Path) -> None:
    """A `null` row must never inherit the `skip` row's verdict in its notes column:
    that is exactly the confusion that makes a reader distrust the table."""
    from wf.graphs.intervene import write_artifacts
    skip = _make_runrecord().model_copy(update={
        "deterministic": False, "hung": True,
        "notes": ("call_m4aSoundMain: deterministic=False, hung=True, 43 anchor frames differ",)})
    null = skip.model_copy(update={"deterministic": True, "hung": False,
                                  "anchor_diffs": tuple(d.model_copy(update={"differing_frames": 0})
                                                        for d in skip.anchor_diffs),
                                  "notes": ("no anchor difference over the window",)})
    write_artifacts(WriteInput(run_dir=tmp_path, function_name="call_m4aSoundMain",
                               address=0x080005AC, intervention="skip",
                               intervention_k=None, route="battles1", frames=60,
                               lead_in_frames=4, deterministic=False, record=skip,
                               null_record=null, wall_seconds=6.8, notes=("shared",)))
    report = (tmp_path / "report.tsv").read_text().splitlines()
    header, skip_row, null_row = report[0].split("\t"), report[1].split("\t"), report[2].split("\t")
    cols = {name: i for i, name in enumerate(header)}
    assert skip_row[cols["deterministic"]] == "False"
    assert null_row[cols["deterministic"]] == "True"
    assert "hung=True" in skip_row[cols["notes"]]
    assert "hung=True" not in null_row[cols["notes"]]
    assert "no anchor difference" in null_row[cols["notes"]]
    assert "shared" not in skip_row[cols["notes"]]


# ── hang detection ──────────────────────────────────────────────────────────

def test_stalled_before_true_when_target_frame_never_reached() -> None:
    # What a wedged run looks like: the hardware budget runs out, so mgba_run never
    # prints the "reached game frame" line.
    assert it._stalled_before("host: r0=080005ad pc=080005ae cpsr=2000003f\nran 500 frames\n", 61) is True


def test_stalled_before_false_when_target_frame_reached() -> None:
    log = "watch game=60 hw=59 40 1f\nreached game frame 61 at hardware frame 60\nran 60 frames\n"
    assert it._stalled_before(log, 61) is False


def test_stalled_before_true_when_short_of_target() -> None:
    log = "reached game frame 30 at hardware frame 29\nran 30 frames\n"
    assert it._stalled_before(log, 61) is True


def test_hung_patched_run_is_not_a_determinism_failure() -> None:
    """A hang is the intervention's own effect; only a hung *baseline* invalidates
    the route/window."""
    rec = _make_runrecord()
    # summarise_run / the observe() contract: hung comes from either side, but
    # `deterministic` is gated on the baseline alone.
    assert rec.hung is False
    hung_rec = rec.model_copy(update={"hung": True})
    assert hung_rec.deterministic is True     # carried through, verdict still valid


# ── anchors.yaml ───────────────────────────────────────────────────────────

def test_load_anchors_reads_hardware_and_ram(tmp_path: Path) -> None:
    cfg = tmp_path / "anchors.yaml"
    cfg.write_text(
        "hardware:\n"
        "  - {name: oam, base: 0x07000000, size: 0x400}\n"
        "  - name: display_io\n"
        "    registers:\n"
        "      - {name: DISPCNT, address: 0x04000000}\n"
        "      - {name: BG0CNT, address: 0x04000008}\n"
        "  - {name: screen, every: 2}\n"
        "ram:\n"
        "  - {name: gUnknown_03004EA0, base: 0x03004EA0, size: 0x20}\n")
    anchors = load_anchors(cfg)
    by_name = {a.name: a for a in anchors}
    assert set(by_name) == {"oam", "display_io", "screen", "gUnknown_03004EA0"}
    assert by_name["oam"].base == 0x07000000 and by_name["oam"].size == 0x400
    assert by_name["display_io"].is_register_set
    assert [r[0] for r in by_name["display_io"].registers] == ["DISPCNT", "BG0CNT"]
    assert [r[1] for r in by_name["display_io"].registers] == [0x04000000, 0x04000008]
    assert by_name["screen"].is_screen and by_name["screen"].every == 2
    assert by_name["gUnknown_03004EA0"].base == 0x03004EA0


def test_repo_anchors_yaml_parses() -> None:
    """The committed symbols/anchors.yaml must stay loadable and non-empty."""
    anchors = load_anchors()
    assert anchors
    assert {"oam", "palette", "vram_bg", "vram_obj", "display_io", "screen"} <= {a.name for a in anchors}


# ── pilot target selection ─────────────────────────────────────────────────

def test_select_intervene_targets_takes_named_prefix() -> None:
    out = select_intervene_targets(count=3, named=("A", "B", "C", "D", "E"))
    assert out[:3] == ("A", "B", "C")


def test_select_intervene_targets_only_filter() -> None:
    assert select_intervene_targets(count=10, named=("A", "B", "C"), only=("B",)) == ("B",)


def test_random_tail_is_patchable() -> None:
    """Every name the pilot selects must resolve to a ROM-resident THUMB function:
    a name that cannot be patched can only ever produce an `unsupported` row."""
    reached = it.reached_function_names()
    if not reached:
        pytest.skip("no facts layer (build/facts/by_address) to draw the random half from")
    chosen = select_intervene_targets(count=20, rng_seed=7, reached=reached)
    assert len(chosen) == 20
    for name in chosen:
        finfo = find_function(name)         # raises on RAM / ARM / too short
        assert finfo.mode == "t"


def test_random_tail_comes_from_the_reached_set() -> None:
    """The random half of the pilot is drawn from the facts layer's reached set: an
    earlier pilot drew from the lifted unit names and threw away 17 of 24 rows because
    no recording ever executed them."""
    reached = it.reached_function_names()
    if not reached:
        pytest.skip("no facts layer (build/facts/by_address) to draw the random half from")
    chosen = select_intervene_targets(count=30, rng_seed=7, reached=reached)
    assert set(chosen) <= set(reached)


def test_pilot_named_has_twentyfour_reachable_functions() -> None:
    assert len(it.PILOT_NAMED) == 24
    for name in it.PILOT_NAMED:
        finfo = find_function(name)         # must not raise
        assert finfo.size >= it.PATCH_HEAD_MIN


def test_pilot_named_covers_the_categories_it_claims() -> None:
    """The pilot is a regression suite, so the six functions an earlier pilot reported
    `hung` on -- and which turned out to be the `bx lr` crash, not six bugs -- have to
    stay in it."""
    for name in ("call_m4aSoundMain", "SetScreenFade", "IsScreenFadeActive",
                 "LoadGFXAnims", "LoadGFXAnim", "startScreen_AnimatePressStart_803E938"):
        assert name in it.PILOT_NAMED


# ── the facts layer: which route a function runs on ────────────────────────

def test_routes_executing_shortest_first() -> None:
    """The route comes from the facts layer, shortest first -- not from a 1200-frame
    window of whichever route happened to be tried first."""
    if not it.FACTS_DIR.is_dir():
        pytest.skip("no facts layer (build/facts/by_address)")
    finfo = find_function("call_m4aSoundMain")
    routes = it.routes_executing(finfo)
    assert routes, "the facts layer should record routes for a function the pilot watches"
    lengths = [int(r.length) for r in routes]
    assert lengths == sorted(lengths)


def test_routes_executing_drops_routes_with_no_recording() -> None:
    """The facts list routes the coverage profiles know about (battlesweep, chips,
    fuzz, maps); those have no .trace to replay, so they cannot be picked."""
    from wf.routes import ALL_ROUTES, LONG_ROUTES
    known = {str(r.name) for r in ALL_ROUTES + LONG_ROUTES}
    if not it.FACTS_DIR.is_dir():
        pytest.skip("no facts layer (build/facts/by_address)")
    for name in ("call_m4aSoundMain", "SetScreenFade", "LoadGFXAnims"):
        for route in it.routes_executing(find_function(name)):
            assert str(route.name) in known


def test_pick_route_uses_the_facts_layer() -> None:
    if not it.FACTS_DIR.is_dir():
        pytest.skip("no facts layer (build/facts/by_address)")
    finfo = find_function("call_m4aSoundMain")
    executing = it.routes_executing(finfo)
    if not executing:
        pytest.skip("no route in the facts layer executes this function")
    assert str(it.pick_route(finfo).name) == str(executing[0].name)


def test_state_stride_keeps_the_state_count_bounded() -> None:
    """A route's cached states must stay bounded whatever its length: `beastsp` is
    432,312 frames and one state every 32 frames would be 13,500 x 380 KB."""
    from wf.routes import route_named
    for name in ("battles1", "beastsp"):
        route = route_named(RouteName(name))
        stride = it.state_stride(route)
        assert stride >= it.STATE_STRIDE_MIN
        assert int(route.length) // stride <= it.STATE_STRIDE_TARGET


def test_state_paths_are_per_route() -> None:
    from wf.routes import route_named
    a = it.state_path_at(route_named(RouteName("battles1")), 32)
    b = it.state_path_at(route_named(RouteName("wander")), 32)
    assert a.parent.name == "battles1" and b.parent.name == "wander"
    assert a != b


# ── hang evidence ──────────────────────────────────────────────────────────

def test_hung_is_not_inferred_from_the_stalled_line_alone() -> None:
    """mgba_run prints `stalled:` for an ordinary slow-down as well as for a real
    wedge, so a hang verdict needs the frame counter. An earlier pilot called a run
    hung from that line alone."""
    stopped = it.SideRun(reached_frame=100, want_frame=120, timed_out=False,
                         illegal_opcodes=0, stalled=True)
    ran = it.SideRun(reached_frame=120, want_frame=120, timed_out=False,
                     illegal_opcodes=0, stalled=True)
    assert stopped.hung is True          # never got to the window's end
    assert ran.hung is False              # reached it, whatever mgba_run said


def test_hung_records_which_side_stopped() -> None:
    from wf.types import ToolMessage
    stopped = it.SideRun(reached_frame=3, want_frame=120, timed_out=False,
                         illegal_opcodes=2_400_000, stalled=False)
    ok = it.SideRun(reached_frame=120, want_frame=120, timed_out=False,
                    illegal_opcodes=0, stalled=False)
    obs = it.Observation(anchor_diffs=(), baseline=ok, patched=stopped,
                        shots=(Path("b"), Path("p")), logs=(Path("b"), Path("p")),
                        start_frame=0, hung_side="patched")
    assert obs.hung is True
    assert obs.hung_side == "patched"
    text = " ".join(str(n) for n in obs.notes())
    assert "patched run hung" in text and "2400000 illegal opcodes" in text


def test_hung_baseline_marks_the_window_unusable() -> None:
    stopped = it.SideRun(reached_frame=0, want_frame=120, timed_out=False,
                         illegal_opcodes=0, stalled=False)
    ok = it.SideRun(reached_frame=120, want_frame=120, timed_out=False,
                    illegal_opcodes=0, stalled=False)
    obs = it.Observation(anchor_diffs=(), baseline=stopped, patched=ok,
                        shots=(Path("b"), Path("p")), logs=(Path("b"), Path("p")),
                        start_frame=0, hung_side="baseline")
    assert any("baseline hung too" in str(n) for n in obs.notes())


def test_illegal_opcodes_come_from_mgba_run_stderr() -> None:
    assert it._illegal_opcodes("17 illegal opcodes executed in all\n") == 17
    assert it._illegal_opcodes("nothing here") == 0
    assert it._illegal_opcodes("") == 0


def test_last_reached_reads_the_frame_counter() -> None:
    log = "digest game=1 hw=1 pal=a vram=b oam=c\nreached game frame 117 at hardware frame 468\n"
    assert it._last_reached(log) == 117
    assert it._last_reached("no line here") is None


# ── temporary patched ROMs ─────────────────────────────────────────────────

def test_patched_roms_do_not_survive_a_run(tmp_path: Path) -> None:
    """build/intervene/patched-roms reached 945 MB because every run kept its own 8 MB
    copy forever. A run's copy lives in a temp directory inside it and is deleted with
    it, patched or not."""
    if not (_mgba_run().exists() and BIOS.exists() and ROM.exists()):
        pytest.skip("mgba_run / BIOS / bn6f.gba not present")
    from wf.routes import route_named
    import wf.repo as wfrepo
    route = route_named(RouteName("battles1"))
    before = set(wfrepo.REPO.glob("build/intervene/patched-roms/*"))
    rec = it.run_intervene_one(function=find_function("call_m4aSoundMain"),
                               intervention="skip", k=None, route=route, frames=4,
                               mgba_run=_mgba_run(), bios=BIOS, out_dir=tmp_path,
                               anchors=())
    after = set(wfrepo.REPO.glob("build/intervene/patched-roms/*"))
    assert after == before
    assert rec.rom_hashes[1]                     # the hash was taken before deleting


# ── the CLI option ─────────────────────────────────────────────────────────

def test_intervention_argument_is_a_validated_string() -> None:
    """--intervention used to be 258 argparse choices for what is three forms of
    argument; it is one string now, validated by InterventionKind.parse."""
    from wf.__main__ import _intervention_arg
    assert _intervention_arg("skip") == "skip"
    assert _intervention_arg("null") == "null"
    assert _intervention_arg("return:0") == "return:0"
    assert _intervention_arg("return:255") == "return:255"
    for bad in ("nuke", "return:", "return:256", "return:x"):
        with pytest.raises(Exception):
            _intervention_arg(bad)


def test_intervention_subparser_does_not_enumerate_k() -> None:
    from wf.__main__ import build_intervene_parser
    help_text = _intervene_help()
    assert "return:K" in help_text
    assert "return:255" not in help_text               # no enumerated choices
    assert help_text.count("return:") == 1


def _intervene_help() -> str:
    """`wf intervene --help`'s text."""
    import argparse
    from wf.__main__ import build_intervene_parser
    parser = argparse.ArgumentParser(prog="wf")
    commands = parser.add_subparsers(dest="command", required=True)
    build_intervene_parser(commands)
    return commands.choices["intervene"].format_help()


# ── end-to-end (needs mgba_run + BIOS + ROM) ───────────────────────────────

@pytest.mark.skipif(not (_mgba_run().exists() and BIOS.exists() and ROM.exists()),
                    reason="mgba_run / BIOS / bn6f.gba not present")
def test_intervene_null_is_deterministic() -> None:
    """A null intervention must produce zero anchor diffs: it re-writes a function's
    own bytes, so the patched ROM is byte-identical to the baseline. Anything else
    means the route/window is nondeterministic and every verdict would be noise."""
    from wf.routes import route_named
    route = route_named(RouteName("battles1"))
    rec = it.run_intervene_one(function=find_function("call_m4aSoundMain"),
                               intervention="null", k=None, route=route,
                               frames=10, mgba_run=_mgba_run(), bios=BIOS,
                               out_dir=Path("/tmp/intervene-test-run"),
                               anchors=load_anchors())
    if rec.first_frame_executed is None:
        pytest.skip("call_m4aSoundMain is not on battles1 within the bisect window")
    assert rec.deterministic is True
    assert all(d.differing_frames == 0 for d in rec.anchor_diffs)
    assert rec.rom_hashes[0] == rec.rom_hashes[1]      # null => identical ROM hash


@pytest.mark.skipif(not (_mgba_run().exists() and BIOS.exists() and ROM.exists()),
                    reason="mgba_run / BIOS / bn6f.gba not present")
@pytest.mark.parametrize("intervention,k", [("skip", None), ("return", 0), ("return", 1)])
def test_skip_does_not_stop_the_game(intervention: str, k: int | None) -> None:
    """The regression for the call_m4aSoundMain crash.

    On battles1 `call_m4aSoundMain` (0x080005ac) is entered from an IWRAM trampoline
    at 0x03005d78 whose `mov lr, pc` leaves an even LR (0x03005d8c). The old skip patch
    `bx lr` therefore switched the CPU to ARM state at the return, decoded the
    trampoline's THUMB `pop {r0}; bx r0` as garbage and walked into EWRAM: the game
    frame counter stopped after 2.4M illegal opcodes and the pilot reported `hung` for
    six unrelated functions.

    Every one of the three real interventions must now keep the counter running."""
    from wf.routes import route_named
    route = route_named(RouteName("battles1"))
    finfo = find_function("call_m4aSoundMain")
    route_known = str(it.routes_executing(finfo) and it.routes_executing(finfo)[0].name or "battles1")
    rec = it.run_intervene_one(function=finfo, intervention=intervention, k=k,
                               route=route_named(RouteName(route_known)),
                               frames=30, mgba_run=_mgba_run(), bios=BIOS,
                               out_dir=Path(f"/tmp/intervene-test-{intervention}{k}"),
                               anchors=())
    if rec.first_frame_executed is None:
        pytest.skip(f"call_m4aSoundMain does not execute on {route_known}")
    assert rec.hung is False, f"{rec.intervention} stopped the game: " + "; ".join(str(n) for n in rec.notes)
    assert rec.illegal_opcodes[1] == 0
    assert rec.hung_side == ""
    assert rec.patch_bytes


@pytest.mark.skipif(not (_mgba_run().exists() and BIOS.exists() and ROM.exists()),
                    reason="mgba_run / BIOS / bn6f.gba not present")
def test_bx_lr_skip_does_stop_the_game() -> None:
    """The counter-example: the old `bx lr` encoding, written straight to the ROM, does
    still wedge battles1 at frame 1. If this ever stops crashing, the encoding above is
    no longer the reason the earlier pilots reported `hung`, and the explanation in
    docs/workflows/intervene-notes.md needs revisiting."""
    from wf.routes import route_named
    route = route_named(RouteName("battles1"))
    fn = find_function("call_m4aSoundMain")
    rom = it.patch_rom(it._origin_rom(), fn, "null", None, Path("/tmp/intervene-bxlr"))
    data = bytearray(rom.read_bytes())
    off = it._load_b6f_info().rom_offset(fn.address)
    data[off:off + 2] = b"\x70\x47"                       # bx lr
    rom.write_bytes(bytes(data))
    state = it.state_path_at(route, 1)
    if not state.exists() or state.stat().st_size == 0:
        it.save_state_at(route, at_frame=1, mgba_run=_mgba_run(), bios=BIOS, out=state)
    run = it._observe_one(rom=rom, route=route, state_path=state, start_frame=1, frames=30,
                          anchors=(), log=Path("/tmp/intervene-bxlr/log.txt"),
                          shots_dir=Path("/tmp/intervene-bxlr"), mgba_run=_mgba_run(),
                          bios=BIOS)
    assert run.hung is True
    assert run.illegal_opcodes > 0


@pytest.mark.skipif(not (_mgba_run().exists() and BIOS.exists() and ROM.exists()),
                    reason="mgba_run / BIOS / bn6f.gba not present")
@pytest.mark.parametrize("name", [
    "call_m4aSoundMain", "SetScreenFade", "IsScreenFadeActive", "LoadGFXAnims",
    "LoadGFXAnim", "startScreen_AnimatePressStart_803E938",
])
def test_the_six_previously_hung_functions_run_to_completion(name: str) -> None:
    """Problem 2: every function the v1 pilot reported `hung` on battles1, re-checked.

    All six shared one cached state (battles1, game frame 1, the sound VBlank
    interrupt) and all six were reported hung for the same reason: the skip patch's
    `bx lr` left the CPU in ARM state at the trampoline's return, mGBA counted 2.4M
    illegal opcodes and the frame counter stopped. With the mode-preserving return
    (`mov pc, lr`) each of them has to play the whole window on both sides, with no
    illegal instructions anywhere.

    `IsScreenFadeActive` is the one that could legitimately wedge a caller -- it
    returns a status -- and `skip` (which leaves r0 as the caller left it) is what the
    old pilot ran. It does not hang: the fade wait loop reads the *palette* the fade
    wrote, not the flag. The `return:0` rows of the same pilot say the same thing."""
    from wf.routes import route_named
    route = route_named(RouteName("battles1"))
    rec = it.run_intervene_one(function=find_function(name), intervention="skip", k=None,
                               route=route, frames=60, mgba_run=_mgba_run(), bios=BIOS,
                               out_dir=Path(f"/tmp/intervene-six-{name}"), anchors=())
    if rec.first_frame_executed is None:
        pytest.skip(f"{name} does not execute on battles1")
    assert rec.hung is False, "; ".join(str(n) for n in rec.notes)
    assert rec.illegal_opcodes == (0, 0)
    assert rec.patch_bytes == "f746"          # the mode-preserving return, not `bx lr`


def test_state_paths_line_up_with_the_bulk_pass() -> None:
    """`state_path_at` has to name the states `mgba_run --save-state-every` writes
    (`PREFIX_%07u.state` at every multiple of the stride, tools/mgba_run.c) and the grid
    has to be frame 1 plus those multiples. When it did not, nothing lined up:
    `ensure_route_states` reported one cached state, every probe fell back to a
    full-route run and a one-function pilot took minutes instead of seconds."""
    from wf.routes import route_named
    for route_name, frames in (("wander", 8000), ("battles1", 20111), ("beastsp", 432312)):
        route = route_named(RouteName(route_name))
        stride = it.state_stride(route)
        assert it.state_path_at(route, stride).name == f"{route_name}_{stride:07d}.state"
        grid = [f for f in range(stride, frames + 1, stride)]
        assert grid and all(f % stride == 0 for f in grid)     # exactly the bulk pass's grid
        assert 1 not in grid                                   # frame 1 is saved on its own


def test_first_frame_search_is_not_off_by_a_state() -> None:
    """The frame-level bisect inside a chunk is anchored at the chunk's own cached
    state. Anchoring it at the moving bound instead asks `_count_entry` to resume from
    a state that does not exist (only grid frames have one), it answers 0, and the
    search walks to the end of the chunk: `LoadGFXAnims` on wander reported frame 239
    where the entry is really in [232, 236)."""
    frames = [1, 33, 65, 97]
    hit = 90
    starts = []

    def fake_count(_address: int, _route: Route, start: int, until: int, **_kw: object) -> int:
        starts.append(start)
        return 1 if start <= hit < until else 0

    monkey = pytest.MonkeyPatch()
    monkey.setattr(it, "ensure_route_states", lambda *a, **k: tuple(frames))
    monkey.setattr(it, "first_execution_segment", lambda *a, **k: (False, None))
    monkey.setattr(it, "_count_entry", fake_count)
    try:
        # the offsets do not matter here, only that every probe starts on a frame that
        # has a cached state
        found = it.first_game_frame(
            _fake_function(), _fake_route(129), mgba_run=Path("/bin/true"),
            bios=Path("/bin/true"))
    finally:
        monkey.undo()
    assert found == 90
    assert set(starts) <= set(frames)              # never off the grid


def test_the_gallop_finds_a_lone_late_chunk() -> None:
    """A chunk-level binary search is wrong here: "hit in chunk i" is NOT monotone (a
    function can run in one chunk and never again), so the scan gallops over ranges
    [0,1), [1,3), [3,7), ... instead. With one hit, in one chunk near the start of a
    long route, the search has to find it in a handful of probes -- a linear walk of the
    grid would cost one probe per chunk."""
    frames = [1] + [32 * i for i in range(1, 129)]           # 128 chunks
    hit = 40                                                # only chunk 1 hits
    probes = 0

    def fake_count(_address: int, _route: Route, start: int, until: int, **_kw: object) -> int:
        nonlocal probes
        probes += 1
        return 1 if start <= hit < until else 0

    monkey = pytest.MonkeyPatch()
    monkey.setattr(it, "ensure_route_states", lambda *a, **k: tuple(frames))
    monkey.setattr(it, "first_execution_segment", lambda *a, **k: (False, None))
    monkey.setattr(it, "_count_entry", fake_count)
    try:
        found = it.first_game_frame(_fake_function(), _fake_route(4096),
                                    mgba_run=Path("/bin/true"), bios=Path("/bin/true"))
    finally:
        monkey.undo()
    assert found == hit
    assert probes <= 16, f"the gallop degenerated into a scan ({probes} probes)"


def _fake_function() -> FunctionInfo:
    return FunctionInfo(name="fake", address=0x08000000, size=16, mode="t")


def _fake_route(length: int) -> Route:
    """A route object `first_game_frame` can read `.length` off, without touching the
    recordings."""
    return Route(name="fake", trace=Path("/dev/null"), length=length, sav=None)


@pytest.mark.skipif(not (_mgba_run().exists() and BIOS.exists() and ROM.exists()),
                    reason="mgba_run / BIOS / bn6f.gba not present")
def test_first_frame_search_finds_a_function_past_the_old_window() -> None:
    """The old search only looked at the first 1200 frames of one route and reported
    17 of 24 functions as "not reached". It now scans the whole route in chunks from
    the cached states, so a function whose first call is late is still found."""
    from wf.routes import route_named
    if not it.FACTS_DIR.is_dir():
        pytest.skip("no facts layer (build/facts/by_address)")
    finfo = find_function("call_m4aSoundMain")
    routes = it.routes_executing(finfo)
    if not routes:
        pytest.skip("no route in the facts layer executes this function")
    route = routes[0]
    frame = it.first_game_frame(finfo, route, mgba_run=_mgba_run(), bios=BIOS,
                                frames_window=min(400, int(route.length)))
    assert frame is None or 1 <= frame <= min(400, int(route.length))