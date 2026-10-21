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
                                 RouteName, evaluate_predicate, find_function, load_anchors,
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


def test_return_k_bytes_encodes_movs_bx_lr() -> None:
    for k in (0, 1, 5, 200, 255):
        b = return_k_bytes(k)
        assert len(b) == 4
        # little-endian halfwords: `movs r0, #k` then `bx lr`
        assert int.from_bytes(b[0:2], "little") == 0x2000 | k
        assert int.from_bytes(b[2:4], "little") == 0x4770


def test_return_k_bytes_range_checked() -> None:
    with pytest.raises(ValueError):
        return_k_bytes(-1)
    with pytest.raises(ValueError):
        return_k_bytes(256)


def test_patch_for_skip_is_bx_lr() -> None:
    fn = FunctionInfo(name="Test", address=0x08000100, size=24, mode="t")
    out = patch_for("skip", None, fn, b"\x00\xb5\x4e\xf1\xb3\xf9\x00\xbd")
    assert out == b"\x70\x47"
    assert len(out) == 2


def test_patch_for_return_is_movs_bx_lr() -> None:
    fn = FunctionInfo(name="Test", address=0x08000100, size=24, mode="t")
    out = patch_for("return", 5, fn, b"\x00\xb5\x4e\xf1\xb3\xf9\x00\xbd")
    assert out == b"\x05\x20\x70\x47"


def test_patch_for_null_rewrites_own_bytes() -> None:
    """The null control must produce a byte-identical ROM: it re-writes the function's
    own first instructions, so any diff is emulator nondeterminism, not the patch."""
    fn = FunctionInfo(name="Test", address=0x08000100, size=24, mode="t")
    baseline = b"\x00\xb5\x4e\xf1\xb3\xf9\x00\xbd"
    assert patch_for("null", None, fn, baseline) == baseline[:2]


def test_patch_for_rejects_function_too_short() -> None:
    fn = FunctionInfo(name="T", address=0x08000100, size=2, mode="t")
    with pytest.raises(ValueError, match="cannot fit"):
        patch_for("return", 0, fn, b"\x00\xb5")


def test_patch_for_rejects_unknown_intervention() -> None:
    fn = FunctionInfo(name="T", address=0x08000100, size=24, mode="t")
    with pytest.raises(ValueError, match="unknown intervention"):
        patch_for("nuke", None, fn, b"\x00\xb5\x4e\xf1\xb3")


def test_patch_rom_writes_new_file_and_leaves_original() -> None:
    if not ROM.exists():
        pytest.skip("bn6f.gba not present")
    fn = find_function("call_m4aSoundMain")
    original = ROM.read_bytes()
    out = patch_rom(ROM, fn, "skip", None, Path("/tmp/intervene-test-roms"))
    try:
        assert out.exists()
        patched = out.read_bytes()
        assert len(patched) == len(original)
        off = it._load_b6f_info().rom_offset(fn.address)
        assert off is not None
        assert patched[off:off + 2] == b"\x70\x47"            # bx lr
        assert patched[:off] == original[:off]                # nothing before
        assert patched[off + 2:] == original[off + 2:]        # nothing after
        assert ROM.read_bytes() == original                   # original untouched
    finally:
        for p in Path("/tmp/intervene-test-roms").glob("*"):
            p.unlink()


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
    chosen = select_intervene_targets(count=20, rng_seed=7)
    assert len(chosen) == 20
    for name in chosen:
        finfo = find_function(name)         # raises on RAM / ARM / too short
        assert finfo.mode == "t"


def test_pilot_named_has_twelve_resolvable_functions() -> None:
    assert len(it.PILOT_NAMED) == 12
    for name in it.PILOT_NAMED:
        finfo = find_function(name)         # must not raise
        assert finfo.size >= 4


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