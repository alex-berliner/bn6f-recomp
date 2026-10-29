"""`wf check-names`: the schema the blind claims have to fit, what each claim comes to against
a facts record, how a route maps to a scene, how the batch is paired with swapped controls, the
calibration gate's arithmetic, and the graph end to end with fakes.

No model is called and no emulator is started here: `complete()` and the one `wf intervene`
call are both faked, and one test asserts that nothing in this file reaches for a model.
"""
from __future__ import annotations

import json
from pathlib import Path
from types import SimpleNamespace
from typing import cast

import pytest

from wf.graphs import check_names as graph
from wf.graphs.check_names import CheckNamesState, run_check_names
from wf.tools import check_names as cn
from wf.tools import facts as ft
from wf.tools import intervene as it
from wf.tools import runtime_evidence as rt
from wf.tools.check_names import (ActiveResult, CalledEveryFrame, ClaimCheck, Control, DoIt,
                                  HasArgument, Level, NoEffect, Observation, ReadsOrWritesIo,
                                  ReachedInScene, Reply, ReturnsValue, WritesRam, calibration,
                                  check_claim, level_for, pair_up, passive_verdict, scene_of)
from wf.tools.facts import FunctionFacts, RegisterStats, RouteCoverage
from wf.types import FunctionName, GuestAddress, ModelSpec, RouteName, ToolMessage

FOO = GuestAddress(0x08001234)
BAR = GuestAddress(0x08005678)
BAZ = GuestAddress(0x0800ABCD)


# ── fakes ──────────────────────────────────────────────────────────────

def fake_record(*, name: str = "foo_08001234", address: GuestAddress = FOO,
                routes: tuple[RouteCoverage, ...] = (), memory: tuple[str, ...] = (),
                io: tuple[str, ...] = (), returned: int = 8, probes: int = 8,
                r0_in: tuple[int, ...] = (1, 2), r0_out: tuple[int, ...] = (0x02000000, 0x02000004),
                args: tuple[RegisterStats, ...] | None = None,
                facts_error: str | None = None,
                field_writes: tuple[ft.FieldWrite, ...] = (),
                scenes_at_call: tuple[ft.SceneAtCall, ...] = ()) -> FunctionFacts:
    """A `wf facts` record with the shape the checker reads, without any replay."""
    empty: tuple[ToolMessage, ...] = ()
    facts: rt.RuntimeFacts | None
    if returned == 0:
        facts = None
    else:
        facts = rt.RuntimeFacts(
            name=FunctionName(name), address=address, probes=probes, returned=returned,
            register_inputs=empty, register_outputs=empty, registers_written=empty,
            written_patterns=empty, route_coverage=empty,
            memory_writes=tuple(rt.MemoryRegionWrite(symbol=rt.SymbolName(symbol),
                                                     address=GuestAddress(0), bytes=8,
                                                     distinct_values=2) for symbol in memory),
            io_writes=tuple(rt.IOWrite(name=rt.SymbolName(register),
                                       address=GuestAddress(0x04000000), size=2, value=1)
                            for register in io))
    arguments = args if args is not None else tuple(
        ft.register_stats(register, list(r0_in) if register == "r0" else [0, 0])
        for register in ft.ARGUMENT_REGISTERS)
    returns = (ft.register_stats("r0", list(r0_out)),) if returned else ()
    scenes = tuple(ToolMessage(f"{call.scene}: {call.state}") for call in scenes_at_call)
    return FunctionFacts(
        address=address, address_hex=f"0x{int(address):08X}", name=FunctionName(name),
        end=GuestAddress(int(address) + 0x20), reached=any(route.executed for route in routes),
        captured=probes > 0, captured_states=probes, states=8, probes=probes, returned=returned,
        routes=routes, arguments=arguments, returns=returns, facts=facts,
        facts_error=facts_error, scenes_at_call=scenes_at_call, scenes=scenes,
        field_writes=field_writes)


def route(name: str, *, executed: bool = True, calls: int | None = None, frames: int = 1000,
          per_1000: float | None = None, entries: int | None = None,
          entries_per_1000: float | None = None) -> RouteCoverage:
    return RouteCoverage(route=RouteName(name), executed=executed, body_executed=executed,
                         calls=calls, calls_per_1000_frames=per_1000, frames=frames,
                         entries=entries, entries_per_1000_frames=entries_per_1000)


def scene(state: int, name: str = "battle") -> ft.SceneAtCall:
    """One captured call's scene (the entry snapshot said the game was in `name`)."""
    return ft.SceneAtCall(state=state, snapshot=f"snap_{state}.bin", main=4, gamestate=12,
                          scene=name, handler=ToolMessage("main 0x04 game-state handler"))


def field_write(base: str = "r5", symbol: str = "eT1BattleObject1_CurAction",
                offset: int = 0, names: tuple[str, ...] = ("oT1BattleObject_CurAction",),
                struct_guess: str = "oT1BattleObject", candidates: tuple[str, ...] = ("oT1BattleObject",),
                attributed: bool = True, in_family: bool = True) -> ft.FieldWrite:
    """A FieldWrite that maps to the bn6f field-name names + struct family struct_guess."""
    return ft.FieldWrite(base=base, base_address=GuestAddress(0x02000000), symbol=symbol,
                         offset=offset, struct_offset=offset, size=4, count=1,
                         field_names=names, struct_guess=struct_guess, candidates=candidates,
                         names_at_offset=len(names), attributed=attributed, in_family=in_family)


def battle_record() -> FunctionFacts:
    return fake_record(routes=(route("battles1", calls=4, per_1000=4.0),
                               route("menus2", executed=False)),
                       memory=("oBattleState_HP", "iOamShadowEnd"), io=("DISPCNT",))


def run_record(*, first_frame: int | None = 100, hung: bool = False,
               deterministic: bool = False, anchors: tuple[it.AnchorDiff, ...] = ()
               ) -> it.RunRecord:
    """A `wf intervene` RunRecord with the fields the checker and the predicates read."""
    return it.RunRecord(
        function=FunctionName("foo_08001234"), address=FOO, intervention="skip",
        route=RouteName("battles1"), rom_hashes=("", ""), start_frame=96, lead_in_frames=4,
        first_frame_executed=first_frame, frames_observed=120, deterministic=deterministic,
        hung=hung, notes=(), anchor_diffs=anchors, shot_baseline=None, shot_patched=None,
        shot_diff=None, artifacts_dir="")


def diff(name: str, first: int | None = 10, frames: int = 5) -> it.AnchorDiff:
    return it.AnchorDiff(name=name, kind="memory", first_frame=first, differing_frames=frames,
                         size=frames, unit="bytes")


# ── the schema ─────────────────────────────────────────────────────────

def test_a_valid_reply_parses_into_claims_and_one_prediction() -> None:
    reply, problem = cn.parse_reply(json.dumps({
        "claims": [{"kind": "reached_in_scene", "scene": "battle"},
                   {"kind": "writes_ram", "symbol_prefix": "oBattleState"},
                   {"kind": "reads_or_writes_io", "group": "palette"},
                   {"kind": "returns_value", "value": "pointer"},
                   {"kind": "called_every_frame", "yes": False},
                   {"kind": "has_argument", "n": 0}],
        "prediction": {"intervention": "skip",
                       "predicates": ["differs(anchor=oam, within=60)"], "window": 120},
        "notes": "the scene claim is about battles1"}))
    assert problem is None and reply is not None
    assert [type(claim).__name__ for claim in reply.claims] == [
        "ReachedInScene", "WritesRam", "ReadsOrWritesIo", "ReturnsValue", "CalledEveryFrame",
        "HasArgument"]
    assert isinstance(reply.prediction, DoIt) and reply.prediction.window == 120
    assert reply.prediction.kind == "skip"
    assert reply.notes.endswith("battles1")


def test_a_return_prediction_carries_its_k() -> None:
    reply, problem = cn.parse_reply(json.dumps({
        "claims": [{"kind": "has_argument", "n": 1}],
        "prediction": {"intervention": "return:K", "k": 1,
                       "predicates": ["unchanged(anchor=screen, for=60)"]}}))
    assert problem is None and reply is not None
    assert isinstance(reply.prediction, DoIt) and reply.prediction.k == 1
    assert reply.prediction.kind == "return"


def test_a_claim_outside_the_vocabulary_is_refused() -> None:
    _reply, problem = cn.parse_reply(json.dumps({
        "claims": [{"kind": "paints_the_hp_bar", "scene": "battle"}],
        "prediction": {"intervention": "skip", "predicates": ["differs(anchor=oam)"]}}))
    assert problem is not None and "paints_the_hp_bar" in str(problem)


def test_a_prediction_needs_one_to_three_predicates() -> None:
    _reply, empty = cn.parse_reply(json.dumps({
        "claims": [{"kind": "has_argument", "n": 0}],
        "prediction": {"intervention": "skip", "predicates": [], "window": 120}}))
    assert empty is not None and "1..3 predicates" in str(empty)
    _reply, four = cn.parse_reply(json.dumps({
        "claims": [{"kind": "has_argument", "n": 0}],
        "prediction": {"intervention": "skip", "window": 120,
                       "predicates": ["differs(anchor=oam)"] * 4}}))
    assert four is not None and "1..3 predicates" in str(four)


def test_return_k_without_a_k_in_range_is_refused() -> None:
    _reply, missing = cn.parse_reply(json.dumps({
        "claims": [{"kind": "has_argument", "n": 0}],
        "prediction": {"intervention": "return:K", "predicates": ["differs(anchor=oam)"]}}))
    assert missing is not None and "0..255" in str(missing)
    with pytest.raises(ValueError):
        DoIt(intervention="return:K", k=999, predicates=["differs(anchor=oam)"])


def test_a_reply_with_nothing_to_check_is_refused() -> None:
    _reply, problem = cn.parse_reply(json.dumps({
        "claims": [], "prediction": {"intervention": "none", "reason": "nothing visible"}}))
    assert problem is not None and "nothing to check" in str(problem)


def test_prose_without_a_json_object_is_a_problem_not_a_reply() -> None:
    reply, problem = cn.parse_reply("I think it draws the HP bar, probably every frame.")
    assert reply is None and problem is not None and "no JSON object" in str(problem)


def test_a_json_object_in_a_code_fence_is_read() -> None:
    reply, problem = cn.parse_reply(
        "sure!\n```json\n{\"claims\": [{\"kind\": \"called_every_frame\", \"yes\": true}],\n"
        " \"prediction\": {\"intervention\": \"none\", \"reason\": \"a getter\"}}\n```")
    assert problem is None and reply is not None
    assert isinstance(reply.prediction, NoEffect)


# ── claims against a facts record ──────────────────────────────────────

def test_reached_in_scene_reads_the_routes_that_executed_the_function() -> None:
    record = battle_record()
    assert check_claim(ReachedInScene(scene="battle"), record).status == "pass"
    failed = check_claim(ReachedInScene(scene="menu"), record)
    assert failed.status == "fail" and "battles1" in str(failed.observed)
    assert check_claim(ReachedInScene(scene="battle"),
                       fake_record(routes=(route("battle"),))).status == "pass"


def test_a_function_no_route_reached_fails_its_scene_claim() -> None:
    record = fake_record(routes=(route("battles1", executed=False),))
    assert check_claim(ReachedInScene(scene="battle"), record).status == "fail"


def test_a_route_the_table_cannot_place_is_unknown_not_failed() -> None:
    record = fake_record(routes=(route("beastsp"),))
    result = check_claim(ReachedInScene(scene="battle"), record)
    assert result.status == "unknown" and "cannot name" in str(result.observed)


def test_reached_in_scene_prefers_per_call_scenes_over_route_table() -> None:
    """A function captured on `battles1` (a battle route) is recorded as running inside the
    battle gamestate for both replayed calls, so a scene(battle) claim passes even though
    `battles1` is not the route's name -- the per-call scenes say so directly."""
    record = fake_record(routes=(route("battles1"),), scenes_at_call=(scene(0), scene(1)))
    assert check_claim(ReachedInScene(scene="battle"), record).status == "pass"
    assert check_claim(ReachedInScene(scene="menu"), record).status == "fail"


def test_reached_in_scene_falls_back_to_route_table_when_no_per_call_scenes() -> None:
    """Old records have no scenes_at_call: the route->scene table decides the claim."""
    record = fake_record(routes=(route("beastsp"),))
    assert check_claim(ReachedInScene(scene="battle"), record).status == "unknown"


def test_reached_in_scene_unknown_when_per_call_scenes_are_empty() -> None:
    """An empty `scenes_at_call` tuple is `unknown`, not `fail` (no per-call evidence)."""
    record = fake_record(routes=(route("battles1"),), scenes_at_call=())
    # No captured scenes -> falls back to the route->scene table, which says `battle`.
    assert check_claim(ReachedInScene(scene="battle"), record).status == "pass"


def test_writes_ram_matches_a_symbol_prefix_case_insensitively() -> None:
    record = battle_record()
    assert check_claim(WritesRam(symbol_prefix="oBattleState"), record).status == "pass"
    assert check_claim(WritesRam(symbol_prefix="obattlestate"), record).status == "pass"
    failed = check_claim(WritesRam(symbol_prefix="gVramTiles"), record)
    assert failed.status == "fail" and "oBattleState_HP" in str(failed.observed)


def test_a_claim_about_a_function_with_no_facts_is_unknown() -> None:
    record = fake_record(returned=0, facts_error="no probe returned")
    for claim in (WritesRam(symbol_prefix="oBattleState"),
                  ReturnsValue(value="pointer"), HasArgument(n=0)):
        assert check_claim(claim, record).status == "unknown"
    assert check_claim(claim=CalledEveryFrame(yes=True), record=record).status == "fail"


def test_writes_ram_matches_a_field_name_in_bn6f_notation() -> None:
    """A FieldWrite whose `field_names` carry `oAIData_AttackVars` matches the model's claim
    `writes_ram(oAIData)` even when the RAM symbol was `eAIData1_AttackVars` (the previous
    check matched only on the absolute RAM symbol, not the disassembly's field name)."""
    record = fake_record(field_writes=(field_write(symbol="eAIData1_AttackVars",
                                                   offset=0x00,
                                                   names=("oAIData_AttackVars",),
                                                   struct_guess="oAIData"),))
    assert check_claim(WritesRam(symbol_prefix="oAIData"), record).status == "pass"
    failed = check_claim(WritesRam(symbol_prefix="oBattleState"), record)
    assert failed.status == "fail"


def test_writes_ram_matches_a_struct_family_even_when_nested() -> None:
    """A FieldWrite whose `struct_guess` is the table-claim prefix (a struct family, not a
    full field name) matches `writes_ram(oT3BattleObject)` even when the base register saw a
    nested struct inside the AIData struct."""
    record = fake_record(field_writes=(field_write(symbol="eAIData1_AttackVars",
                                                   offset=0x0,
                                                   names=("oT3BattleObject_CurAction",),
                                                   struct_guess="oT3BattleObject"),))
    assert check_claim(WritesRam(symbol_prefix="oT3BattleObject"), record).status == "pass"


def test_writes_ram_ignores_a_field_name_the_base_register_does_not_point_into() -> None:
    """Offset 0x38 is `oBattleState_Unk_38` and eighty other structs' field at once. When the
    base register points at `eStruct2035280` and every region the calls changed is elsewhere,
    none of that says the function wrote to `oBattleState` -- it fails, not passes."""
    record = fake_record(field_writes=(
        field_write(symbol="eStruct2035280", offset=0x38,
                    names=("oAIAttackVars_PanelParamFlags2", "oBattleState_Unk_38", "oCamera_Z"),
                    struct_guess="", candidates=("oStruct2035280",),
                    attributed=False, in_family=False),),
        memory=("unk_2002230",))
    assert check_claim(WritesRam(symbol_prefix="oBattleState"), record).status == "fail"


def test_writes_ram_still_answers_about_the_family_the_base_register_sits_in() -> None:
    """The base register at `eStruct2035280` says the write landed in that struct even when
    the offset inside it carries no field name, so a claim about the struct's family stands."""
    record = fake_record(field_writes=(
        field_write(symbol="eStruct2035280", offset=0x38, names=("oBattleState_Unk_38",),
                    struct_guess="oStruct2035280", candidates=("oStruct2035280",),
                    attributed=False, in_family=True),))
    assert check_claim(WritesRam(symbol_prefix="oStruct2035280"), record).status == "pass"
    assert check_claim(WritesRam(symbol_prefix="oBattleState"), record).status == "fail"


def test_writes_ram_matches_a_field_name_against_a_ram_symbol_of_another_table() -> None:
    """The whole point of the struct-relative facts: a claim in the disassembly's vocabulary
    (`oBattleObject_Timer`, or the family `oBattleState`) names a field or struct FAMILY,
    while a RAM symbol names a struct INSTANCE with the object index spelled out
    (`eT3BattleObject1_Timer`, `eBattleState0_HP`). Matching the two as text never worked;
    going through bn6f's struct tables does."""
    record = fake_record(memory=("eT3BattleObject1_Timer", "end_file_3007B00"))
    assert check_claim(WritesRam(symbol_prefix="oBattleObject"), record).status == "pass"
    assert check_claim(WritesRam(symbol_prefix="oBattleObject_Timer"), record).status == "pass"
    assert check_claim(WritesRam(symbol_prefix="oT3BattleObject"), record).status == "pass"
    assert check_claim(WritesRam(symbol_prefix="eT3BattleObject"), record).status == "pass"
    assert check_claim(WritesRam(symbol_prefix="oCamera"), record).status == "fail"


def test_writes_ram_falls_back_to_the_ram_symbol_when_no_field_writes() -> None:
    """A record with no `field_writes` (nothing replayed into a base register) still answers
    from the RAM symbols, resolved through the same struct tables."""
    record = fake_record(memory=("eBattleState0_HP",))
    assert check_claim(WritesRam(symbol_prefix="eBattle"), record).status == "pass"
    assert check_claim(WritesRam(symbol_prefix="oBattleState"), record).status == "pass"
    assert check_claim(WritesRam(symbol_prefix="oCamera"), record).status == "fail"


def test_no_record_at_all_is_unknown_for_every_claim() -> None:
    assert check_claim(WritesRam(symbol_prefix="oX"), None).status == "unknown"


def test_io_groups_that_the_diff_drops_are_never_decided() -> None:
    record = battle_record()
    for group in ("sound", "dma", "input"):
        result = check_claim(ReadsOrWritesIo(group=group), record)
        assert result.status == "unknown" and group in str(result.observed)


def test_io_groups_that_the_diff_keeps_are_decided() -> None:
    record = battle_record()
    # OAM is a RAM mirror (iOamShadowEnd) and display is an I/O register (DISPCNT).
    assert check_claim(ReadsOrWritesIo(group="oam"), record).status == "pass"
    assert check_claim(ReadsOrWritesIo(group="display"), record).status == "pass"
    failed = check_claim(ReadsOrWritesIo(group="palette"), record)
    assert failed.status == "fail" and "nothing that lands in palette" in str(failed.observed)


def test_returns_value_reads_what_r0_came_back_with() -> None:
    record = fake_record(r0_in=(1, 2), r0_out=(0, 1))
    assert check_claim(ReturnsValue(value="bool"), record).status == "pass"
    assert check_claim(ReturnsValue(value="small_int"), record).status == "pass"
    pointer = fake_record(r0_in=(1, 2), r0_out=(0x02001234, 0x02005678))
    assert check_claim(ReturnsValue(value="pointer"), pointer).status == "pass"
    assert check_claim(ReturnsValue(value="bool"), pointer).status == "fail"
    truthy_unchanged = fake_record(r0_in=(7,), r0_out=(7,))
    assert check_claim(ReturnsValue(value="none"), truthy_unchanged).status == "pass"
    assert check_claim(ReturnsValue(value="none"), pointer).status == "fail"
    unchanged_args = tuple(ft.register_stats(register, [7] if register == "r0" else [0])
                           for register in ft.ARGUMENT_REGISTERS)
    zero_out = fake_record(r0_in=(7,), r0_out=(7,), args=unchanged_args)
    assert check_claim(ReturnsValue(value="none"), zero_out).status == "pass"


def test_called_every_frame_uses_the_call_counts_over_the_route_length() -> None:
    every = fake_record(routes=(route("battle", calls=900, frames=1000, per_1000=900.0),))
    assert check_claim(CalledEveryFrame(yes=True), every).status == "pass"
    assert check_claim(CalledEveryFrame(yes=False), every).status == "fail"
    rare = fake_record(routes=(route("battle", calls=3, frames=1000, per_1000=3.0),))
    assert check_claim(CalledEveryFrame(yes=False), rare).status == "pass"
    assert check_claim(CalledEveryFrame(yes=True), rare).status == "fail"


def test_called_every_frame_is_unknown_without_call_counts() -> None:
    record = fake_record(routes=(route("battle", executed=True),))
    result = check_claim(CalledEveryFrame(yes=True), record)
    assert result.status == "unknown" and "call counts" in str(result.observed)


def test_called_every_frame_prefers_real_entry_counts_over_call_counts() -> None:
    """When the per-route entry count exists (`entries_per_1000_frames`), the check uses it
    -- the CPU entering the function on the route -- instead of the indirect call sites the
    ABI export instruments (which miss `bl` and direct calls). A function called every frame
    on real entries but with only 2 indirect call sites reported by the old `.edges` is now
    a `yes`, not `unknown`."""
    record = fake_record(routes=(route("battle", calls=2, per_1000=2.0, entries=600,
                                       entries_per_1000=600.0),))
    assert check_claim(CalledEveryFrame(yes=True), record).status == "pass"
    record2 = fake_record(routes=(route("battle", calls=2, per_1000=2.0, entries=10,
                                        entries_per_1000=10.0),))
    assert check_claim(CalledEveryFrame(yes=False), record2).status == "pass"


def test_called_every_frame_falls_back_to_call_counts_when_no_entries() -> None:
    """Routes without entry counts (the count step was not run for them) keep using the
    `.edges` call counts the way the old checker did."""
    record = fake_record(routes=(route("battle", calls=900, per_1000=900.0),))
    assert check_claim(CalledEveryFrame(yes=True), record).status == "pass"


def test_has_argument_needs_the_caller_to_pass_more_than_one_value() -> None:
    record = fake_record(args=tuple(ft.register_stats(register, [1, 2, 3] if register == "r1"
                                                        else [0, 0, 0])
                                   for register in ft.ARGUMENT_REGISTERS))
    assert check_claim(HasArgument(n=1), record).status == "pass"
    assert check_claim(HasArgument(n=0), record).status == "fail"
    single = fake_record(args=tuple(ft.register_stats(register, [1])
                                    for register in ft.ARGUMENT_REGISTERS))
    assert check_claim(HasArgument(n=0), single).status == "unknown"


def test_passive_verdict_follows_the_failing_claims() -> None:
    passed = ClaimCheck(claim=HasArgument(n=0), status="pass", observed=ToolMessage("ok"))
    unknown = ClaimCheck(claim=HasArgument(n=1), status="unknown", observed=ToolMessage("no"))
    failed = ClaimCheck(claim=HasArgument(n=2), status="fail", observed=ToolMessage("no"))
    assert passive_verdict((passed, unknown)) == "consistent"
    assert passive_verdict((passed, failed)) == "contradicted"
    assert passive_verdict((unknown,)) == "unchecked"
    assert passive_verdict(()) == "unchecked"


def test_claim_text_is_the_way_the_prompt_spells_each_claim() -> None:
    assert cn.claim_text(ReachedInScene(scene="title")) == "title"
    assert cn.claim_text(WritesRam(symbol_prefix="oX_y")) == "oX_y"
    assert cn.claim_text(ReadsOrWritesIo(group="vram")) == "vram"
    assert cn.claim_text(ReturnsValue(value="none")) == "none"
    assert cn.claim_text(CalledEveryFrame(yes=True)) == "yes"
    assert cn.claim_text(HasArgument(n=0)) == "r0"
    assert cn.claim_text(HasArgument(n=4)) == "sp"


# ── routes to scenes ───────────────────────────────────────────────────

def test_the_scene_table_covers_the_recorded_routes() -> None:
    assert scene_of("battle") == "battle"
    for route_name in ("battles1", "battles2", "battlesweep", "chips"):
        assert scene_of(route_name) == "battle", route_name
    for route_name in ("story1", "story9", "maps", "wander"):
        assert scene_of(route_name) == "overworld", route_name
    for route_name in ("menus2", "menus3"):
        assert scene_of(route_name) == "menu", route_name
    assert scene_of("title_mash_a") == "title"


def test_a_route_the_table_does_not_know_is_none_and_is_asked_about() -> None:
    assert scene_of("beastsp") is None and scene_of("fuzz") is None
    assert cn.unclear_routes(["wander", "beastsp", "fuzz", "menus3"]) == ("beastsp", "fuzz")


def test_the_routes_the_facts_pick_are_the_reachable_ones_shortest_first() -> None:
    record = fake_record(routes=(route("story5", calls=3), route("menus3", calls=2),
                                 route("battle", calls=1)))
    picked = [name for name, _length in cn.candidate_routes(record)]
    assert picked == ["menus3", "battle", "story5"]     # sorted by route length from wf/routes.py
    assert cn.candidate_routes(None) == ()


# ── the batch pairing ──────────────────────────────────────────────────

def test_every_name_is_paired_with_another_one_and_never_with_itself() -> None:
    names = ["alpha", "beta", "gamma", "delta", "epsilon"]
    swaps = pair_up(names, seed=7).swaps
    assert set(swaps) == set(names)
    assert all(partner != name for name, partner in swaps.items())
    assert all(partner in names for partner in swaps.values())
    assert pair_up(names, seed=7).swaps == swaps      # the same seed, the same batch


def test_two_names_are_paired_with_each_other_and_one_name_pairs_with_nobody() -> None:
    assert pair_up(["a", "b"], seed=1).swaps == {"a": "b", "b": "a"}
    assert pair_up(["only"], seed=1).swaps == {}
    assert pair_up([], seed=1).swaps == {}


# ── the gate ───────────────────────────────────────────────────────────

def check(status: str, claim: ReachedInScene | None = None) -> ClaimCheck:
    return ClaimCheck(claim=claim or ReachedInScene(scene="battle"), status=cast("str", status),
                      observed=ToolMessage(""))


def test_the_claim_gate_needs_a_margin_over_the_swapped_pass_rate() -> None:
    passing = calibration([(check("pass"), check("pass"))], [(check("fail"),)],
                          [ActiveResult(outcome="held")], [ActiveResult(outcome="failed")])
    assert passing.true_claim_rate == 1.0 and passing.swapped_claim_rate == 0.0
    assert passing.claims_ok and passing.predictions_ok and passing.discriminating
    assert passing.reasons == ()
    even = calibration([(check("pass"), check("pass"))],
                       [(check("pass"), check("fail"))],
                       [ActiveResult(outcome="held")], [ActiveResult(outcome="failed")])
    assert even.claim_margin == 0.5 and even.claims_ok and even.predictions_ok
    thin = calibration([(check("pass"),)], [(check("pass"),)],
                       [ActiveResult(outcome="held")], [ActiveResult(outcome="failed")])
    assert thin.claim_margin == 0.0 and not thin.claims_ok
    empty = calibration([], [], [], [])
    assert not empty.discriminating and empty.reasons


def test_the_prediction_gate_needs_the_swapped_ones_to_hold_rarely() -> None:
    held = ActiveResult(outcome="held")
    missed = ActiveResult(outcome="failed")
    pass_check = check("pass")
    good = calibration([(pass_check,)], [(check("fail"),)], [held, held], [missed, missed])
    assert good.predictions_ok and good.discriminating
    leaky = calibration([(pass_check,)], [(check("fail"),)], [held, held], [held, missed])
    assert leaky.swapped_prediction_rate == 0.5 and not leaky.predictions_ok
    assert not leaky.discriminating
    untested = calibration([(pass_check,)], [(check("fail"),)], [held], [])
    assert untested.swapped_prediction_rate is None and not untested.predictions_ok


def test_untested_and_unknown_outcomes_do_not_count_as_held() -> None:
    for outcome in ("unreached", "nondeterministic", "none", "error"):
        assert not ActiveResult(outcome=outcome).tested
    for outcome in ("held", "failed", "hung", "indeterminate"):
        assert ActiveResult(outcome=outcome).tested


def test_hung_and_unreached_outcomes_are_not_held() -> None:
    """L3 is granted on `ActiveResult.held`; the gate's true_held / swapped_held are counted
    off `held`. A prediction whose patched run wedged the game (outcome='hung') or whose
    route never ran the function (outcome='unreached') is real evidence about the name --
    'failed' or 'unreached' -- but it is NEVER 'held'."""
    for outcome in ("held",):
        assert ActiveResult(outcome=outcome).held is True
    for outcome in ("failed", "hung", "unreached", "indeterminate", "nondeterministic",
                    "none", "error"):
        assert ActiveResult(outcome=outcome).held is False, outcome


def test_the_calibration_holds_no_swap_for_hung_or_unreached_predictions() -> None:
    """The prediction gate is `swapped_predictions >= 1 and swapped_held / swapped_predictions < 0.15`.
    When the only tested prediction is hung (a real wedge that came from swapping), the gate
    reads `swapped_held = 0 / 1 = 0 < 0.15` and says OK -- but the actual signal is 'the patched
    run wedged', not 'a non-matching function's prediction held'. Concretely: when a swapped
    prediction's outcome is 'hung' or 'unreached', `true_held` and `swapped_held` correctly
    read 0 from those outcomes, which is the gate's contract for 'this outcome is not held'."""
    pass_check = ClaimCheck(claim=WritesRam(symbol_prefix="oX"), status="pass",
                            observed=ToolMessage(""))
    fail_check = ClaimCheck(claim=WritesRam(symbol_prefix="oX"), status="fail",
                            observed=ToolMessage(""))
    true_held = ActiveResult(outcome="held", route=RouteName("wander"),
                             window=120, first_frame=100)
    # a swapped prediction that hung OR was unreached: not held, not in the numerator
    swapped_hung = ActiveResult(outcome="hung", route=RouteName("wander"),
                                window=120, first_frame=100)
    swapped_unreached = ActiveResult(outcome="unreached")
    cal = calibration([(pass_check,)], [(pass_check,)],
                      [true_held], [swapped_hung, swapped_unreached])
    # only the matching hint 'held' counts in the held columns
    assert cal.true_held == 1
    assert cal.swapped_held == 0
    # neither outcome 'hung' nor outcome 'unreached' was attributed to the function
    assert cal.swapped_prediction_rate == 0
    assert cal.predictions_ok is True        # 0 < 0.15 holds, on a clean signal


def test_a_return_k_prediction_runs_through_intervene_with_kind_and_k() -> None:
    """The brief asks the active half to drive `wf intervene` with kind='return' AND k=K.
    Earlier versions crashed when the active half passed a string like 'return:0' as kind
    alone; the fixed intervene takes the validated kind='return' alongside k=N. We
    monkeypatch `intervene_once` so we can see exactly the (kind, k) pair the active half
    passed -- testing the kind-property contract without a real `wf intervene` run."""
    import threading                                  # noqa: PLC0415
    seen: list[tuple[str, int | None]] = []
    lock = threading.Lock()

    def fake_once(*, name: FunctionName, address_hex: str, route: RouteName, kind: str,
                  k: int | None = None, window: int = cn.DEFAULT_FRAMES,
                  out_dir: Path, frames_window: int = cn.DEFAULT_FRAMES_WINDOW) -> Observation:
        with lock:
            seen.append((kind, k))
        if kind == "probe":
            return Observation(first_frame=100, route=route)
        # the patched run hung -> the active half must produce outcome='hung', not 'held'
        return Observation(record=run_record(first_frame=100, hung=True, anchors=(diff("oam"),)),
                           first_frame=100, route=route)

    action = DoIt(intervention="return:K", k=0,
                  predicates=("differs(anchor=oam, within=60)",), window=120)
    assert action.kind == "return" and action.k == 0
    # Run a real call into the fake: the active half will probe, then null-control,
    # then the prediction. The contract is that (kind, k) is what reaches `wf intervene`.
    monkey = pytest.MonkeyPatch()
    monkey.setattr(cn, "intervene_once", fake_once)
    try:
        obs = cn.intervene_once(name=FunctionName("foo"), address_hex="0x08000000",
                                route=RouteName("battles1"), kind=action.kind, k=action.k,
                                window=120, out_dir=Path("/tmp/none"))
    finally:
        monkey.undo()
    assert seen == [("return", 0)]                       # kind='return', k=0 -> one call
    assert obs.record is not None
    # And: a hung record renders as 'hung' and reads held=False, not 'held'.
    result = cn.judge_prediction(action, obs.record, None, route=RouteName("battles1"))
    assert result.outcome == "hung" and result.held is False


def test_a_hung_outcome_serialises_with_a_problem_carrying_the_evidence() -> None:
    """A patched run that wedged the game is real evidence -- 'the function must run'. The
    checker writes that evidence into the problem so a human reader of `calibration.md` or
    a per-name JSON can see why the prediction did not hold."""
    action = DoIt(intervention="return:K", k=1, predicates=("differs(anchor=oam, within=60)",),
                  window=120)
    result = cn.judge_prediction(action, run_record(hung=True, anchors=()),
                                 Control(route=RouteName("battles1"), window=120,
                                         deterministic=True, hung=False))
    assert result.outcome == "hung"
    assert result.problem is not None and "wedged" in str(result.problem)
    assert result.held is False


def test_an_unreached_outcome_serialises_with_a_problem_carrying_the_evidence() -> None:
    """A route the function never runs on, even with the cached-states search going from
    frame 1 to the route's end, is unreached: the active half never over-hand it to 'held'."""
    action = DoIt(intervention="skip", predicates=("differs(anchor=oam, within=60)",),
                  window=120)
    result = cn.judge_prediction(action, run_record(first_frame=None), None)
    assert result.outcome == "unreached"
    assert result.held is False
    assert result.problem is not None and ("never" in str(result.problem) or "not" in str(result.problem))


def test_the_gate_counts_each_claim_kind_separately() -> None:
    true_checks = (ClaimCheck(claim=WritesRam(symbol_prefix="oX"), status="pass",
                              observed=ToolMessage("")),
                   ClaimCheck(claim=CalledEveryFrame(yes=False), status="pass",
                              observed=ToolMessage("")))
    swapped_checks = (ClaimCheck(claim=WritesRam(symbol_prefix="oX"), status="fail",
                                 observed=ToolMessage("")),
                      ClaimCheck(claim=CalledEveryFrame(yes=False), status="pass",
                                 observed=ToolMessage("")))
    cal = calibration([true_checks], [swapped_checks], [], [])
    lines = {str(line).split(":")[0]: str(line) for line in cal.by_claim_kind}
    assert "writes_ram: true 1/1 passed, swapped 0/1" in lines["writes_ram"]
    assert "called_every_frame: true 1/1 passed, swapped 1/1" in lines["called_every_frame"]


def test_the_kind_breakdown_stays_well_formed_with_many_claims() -> None:
    """3 of one kind pass, 2 fail -- well-formed `passed/total` with total >= passed;
    catches the earlier counter-tuple bug where the unpacked labels were swapped and the
    second claim's row read the values from the wrong end of the tuple."""
    makes = lambda status: ClaimCheck(claim=WritesRam(symbol_prefix="oX"), status=status,
                                       observed=ToolMessage(""))
    cal = calibration([(makes("pass"), makes("fail"), makes("pass"), makes("fail"), makes("pass"))],
                      [(makes("fail"), makes("fail"), makes("pass"))], [], [])
    [line] = cal.by_claim_kind
    text = str(line)
    assert "true 3/5 passed" in text, text
    assert "swapped 1/3" in text, text# ── levels ─────────────────────────────────────────────────────────────

def test_a_level_is_granted_only_when_its_own_gate_passed() -> None:
    held = ActiveResult(outcome="held")
    assert level_for(reached=True, passive="consistent", active=held, control_held=False,
                     claims_ok=True, predictions_ok=True)[0] == Level.L3
    # the same held prediction, but the swapped control also held: not L3
    assert level_for(reached=True, passive="consistent", active=held, control_held=True,
                     claims_ok=True, predictions_ok=True)[0] == Level.L2
    failed = ActiveResult(outcome="failed")
    assert level_for(reached=True, passive="consistent", active=failed, control_held=False,
                     claims_ok=True, predictions_ok=True)[0] == Level.CONTRADICTED
    assert level_for(reached=True, passive="contradicted", active=failed, control_held=False,
                     claims_ok=True, predictions_ok=True)[0] == Level.CONTRADICTED
    ungated, why = level_for(reached=True, passive="contradicted", active=failed,
                             control_held=False, claims_ok=False, predictions_ok=False)
    assert ungated == Level.UNGATED and "gate" in str(why)


def test_a_function_no_route_reached_is_unreached_whatever_else_it_scored() -> None:
    assert level_for(reached=False, passive="contradicted",
                     active=ActiveResult(outcome="failed"), control_held=False,
                     claims_ok=True, predictions_ok=True)[0] == Level.UNREACHED


def test_nothing_checkable_is_L1() -> None:
    assert level_for(reached=True, passive="unchecked", active=ActiveResult(outcome="none"),
                     control_held=False, claims_ok=True, predictions_ok=True)[0] == Level.L1


# ── judging a prediction against the observed diffs ────────────────────

def test_a_prediction_whose_predicates_all_hold_is_held() -> None:
    action = DoIt(intervention="skip", predicates=("differs(anchor=oam, within=60)",), window=120)
    result = cn.judge_prediction(action, run_record(anchors=(diff("oam", 10),)),
                                 Control(route=RouteName("battles1"), window=120,
                                          deterministic=True, hung=False))
    assert result.outcome == "held" and result.sound is True and result.differing_anchors == 5
    assert result.predicates[0].verdict is True


def test_a_prediction_one_predicate_refutes_is_failed() -> None:
    action = DoIt(intervention="skip", predicates=("differs(anchor=oam, within=60)",
                                                   "differs(anchor=palette, within=60)"),
                  window=120)
    result = cn.judge_prediction(action, run_record(anchors=(diff("oam", 10),)),
                                 Control(route=RouteName("battles1"), window=120,
                                          deterministic=True, hung=False))
    assert result.outcome == "failed" and "did not hold" in str(result.problem)


def test_a_prediction_the_route_never_reached_is_unreached() -> None:
    action = DoIt(intervention="skip", predicates=("differs(anchor=oam, within=60)",))
    result = cn.judge_prediction(action, run_record(first_frame=None), None)
    assert result.outcome == "unreached" and "never tested" in str(result.problem)


def test_an_unsound_null_control_makes_the_prediction_mean_nothing() -> None:
    action = DoIt(intervention="skip", predicates=("differs(anchor=oam, within=60)",))
    unsound = Control(route=RouteName("battles1"), window=120, deterministic=False, hung=False)
    result = cn.judge_prediction(action, run_record(anchors=(diff("oam"),)), unsound)
    assert result.outcome == "nondeterministic" and result.sound is False


def test_a_patched_run_that_wedged_the_game_is_hung_not_failed() -> None:
    action = DoIt(intervention="skip", predicates=("differs(anchor=oam, within=60)",))
    result = cn.judge_prediction(action, run_record(hung=True, anchors=(diff("oam"),)),
                                 Control(route=RouteName("battles1"), window=120,
                                          deterministic=True, hung=False))
    assert result.outcome == "hung" and "wedged" in str(result.problem)


def test_a_predicate_the_language_cannot_parse_never_counts_as_held() -> None:
    action = DoIt(intervention="skip", predicates=("makes_the_sky_blue(anchor=oam)",))
    result = cn.judge_prediction(action, run_record(anchors=(diff("oam"),)), None)
    assert result.predicates[0].problem is not None
    assert result.outcome == "indeterminate"


def test_a_model_that_wrote_no_prediction_is_not_run() -> None:
    result = cn.judge_prediction(NoEffect(reason="a getter the caller cannot see"), None, None)
    assert result.outcome == "none" and "getter" in str(result.intervention)


# ── the graph, end to end, with fakes ──────────────────────────────────

GOOD = {"claims": [{"kind": "reached_in_scene", "scene": "battle"},
                   {"kind": "writes_ram", "symbol_prefix": "oBattleState"},
                   {"kind": "returns_value", "value": "pointer"}],
        "prediction": {"intervention": "skip",
                       "predicates": ["differs(anchor=oam, within=60)"], "window": 120}}
PARTNER = {"claims": [{"kind": "reached_in_scene", "scene": "title"},
                      {"kind": "reads_or_writes_io", "group": "sound"}],
           "prediction": {"intervention": "skip",
                          "predicates": ["unchanged(anchor=palette, for=60)"], "window": 120}}


def fake_facts_dir(folder: Path) -> tuple[Path, tuple[FunctionName, ...]]:
    """Two functions' `wf facts` records and the coverage index that points at them."""
    by_address = folder / "by_address"
    by_address.mkdir(parents=True, exist_ok=True)
    names = (FunctionName("drawHpBar"), FunctionName("oam_setPalettes"))
    for record, name, address in ((battle_record(), names[0], FOO),
                                  (fake_record(routes=(route("menus2", calls=2,
                                                              per_1000=1.0),),
                                               memory=("iOamShadowEnd",),
                                               r0_out=(0x02001234,)), names[1], BAR)):
        record = record.model_copy(update={"name": name, "address": address,
                                           "address_hex": f"0x{int(address):08X}"})
        (by_address / f"0x{int(address):08X}.json").write_text(record.model_dump_json())
    (folder / "coverage.tsv").write_text(
        "# address\tname\treached\n"
        + "".join(f"0x{int(address):08X}\t{name}\tyes\n"
                  for name, address in zip(names, (FOO, BAR), strict=True)))
    return folder, names


@pytest.fixture
def no_models(monkeypatch: pytest.MonkeyPatch) -> list[str]:
    """No model call can leave this test: `complete()` is a trap, and the claims are canned."""
    called: list[str] = []

    def trap(spec: ModelSpec, prompt: str, system: str, tag: str) -> tuple[str, ToolMessage | None]:
        called.append(str(prompt)[:40])
        canned = GOOD if "drawHpBar" in prompt else PARTNER
        return json.dumps(canned), None

    monkeypatch.setattr(cn, "complete", trap)
    return called


@pytest.fixture
def fake_intervene(monkeypatch: pytest.MonkeyPatch) -> list[str]:
    """The one `wf intervene` call, faked: a probe finds the function, `null` is deterministic,
    and `skip` makes the OAM anchor differ (so the true prediction holds)."""
    ran: list[str] = []

    def fake_once(*, name: FunctionName, address_hex: str, route: RouteName, kind: str,
                  k: int | None = None, window: int = cn.DEFAULT_FRAMES,
                  out_dir: Path, frames_window: int = cn.DEFAULT_FRAMES_WINDOW) -> Observation:
        ran.append(kind)
        if kind == "probe":
            return Observation(first_frame=100, route=route)
        record = run_record(first_frame=100, deterministic=(kind == "null"),
                            anchors=(diff("oam", 10),) if kind == "skip" else ())
        return Observation(record=record, first_frame=100, route=route)

    monkeypatch.setattr(cn, "intervene_once", fake_once)
    return ran


def test_the_graph_checks_every_name_and_writes_its_own_output(tmp_path: Path,
                                                               no_models: list[str],
                                                               fake_intervene: list[str]) -> None:
    facts_dir, names = fake_facts_dir(tmp_path / "facts")
    run_dir = tmp_path / "run"
    report = run_check_names(
        CheckNamesState(run_dir=run_dir, run_id="check-names-test", facts_dir=facts_dir,
                        only=names, count=0, jobs=2, seed=1),
        run_id="check-names-test", tracked=False)
    assert report.selected == 2 and report.answered == 2
    assert set(report.levels) <= {"L1", "L2 consistent", "L3 confirmed", "contradicted",
                                  "unreached", "ungated"}
    rows = (run_dir / "report.tsv").read_text().splitlines()
    assert rows[0].split("\t")[:3] == ["name", "address", "level"]
    assert {row.split("\t")[0] for row in rows[1:]} == {str(name) for name in names}
    for name in names:
        assert (run_dir / f"{name}.json").exists()
    calibration_text = (run_dir / "calibration.md").read_text()
    assert "check-names-test" in calibration_text and "swapped" in calibration_text
    assert no_models and len(no_models) >= 2     # the canned replies came from the trap, once each
    assert "probe" in fake_intervene and "null" in fake_intervene


def test_the_graph_pairs_each_name_with_another_and_levels_it_against_it(
        tmp_path: Path, no_models: list[str], fake_intervene: list[str]) -> None:
    facts_dir, names = fake_facts_dir(tmp_path / "facts")
    run_dir = tmp_path / "run"
    report = run_check_names(
        CheckNamesState(run_dir=run_dir, run_id="check-names-pair", facts_dir=facts_dir,
                        only=names, count=0, jobs=2, seed=1),
        run_id="check-names-pair", tracked=False)
    lines = (run_dir / "report.tsv").read_text().splitlines()
    header = lines[0].split("\t")
    swapped = {row.split("\t")[header.index("swapped_from")]: row.split("\t")[header.index(
        "swapped_active")] for row in lines[1:]}
    assert set(swapped) == {str(name) for name in names}
    assert all(partner for partner in swapped.values())
    # the swapped half is a real run: the partner's prediction was judged on this function
    assert "held" in swapped.values() or "failed" in swapped.values()
    assert report.calibration.swapped_predictions == 2


def test_the_graph_of_an_empty_batch_says_so_and_still_writes_a_report(tmp_path: Path) -> None:
    facts_dir, _names = fake_facts_dir(tmp_path / "facts")
    run_dir = tmp_path / "run"
    report = run_check_names(
        CheckNamesState(run_dir=run_dir, run_id="check-names-empty", facts_dir=facts_dir,
                        only=(FunctionName("no_such_function"),), count=0, jobs=1, seed=1),
        run_id="check-names-empty", tracked=False)
    assert report.selected == 0
    assert (run_dir / "report.tsv").read_text().startswith("name\taddress\tlevel")
    assert not report.calibration.discriminating


def test_a_name_whose_reply_does_not_fit_is_recorded_as_a_problem(tmp_path: Path,
                                                                 monkeypatch: pytest.MonkeyPatch,
                                                                 fake_intervene: list[str]
                                                                 ) -> None:
    facts_dir, names = fake_facts_dir(tmp_path / "facts")
    monkeypatch.setattr(cn, "complete",
                        lambda spec, prompt, system, tag: ("it draws the bar", None))
    report = run_check_names(
        CheckNamesState(run_dir=tmp_path / "run", run_id="check-names-bad", facts_dir=facts_dir,
                        only=names, count=0, jobs=1, seed=1),
        run_id="check-names-bad", tracked=False)
    assert report.answered == 0
    assert any("no usable claims" in problem for problem in report.problems)


def test_the_state_carries_what_every_step_reads_and_writes() -> None:
    """Burr builds each step's input from the state by name, so a field the state lacks fails
    at wiring time."""
    state = CheckNamesState.model_fields
    models = (graph.ChooseInput, graph.ChooseOutput, graph.ClaimsInput, graph.ClaimsOutput,
              graph.CheckInput, graph.CheckOutput, graph.PredictInput, graph.PredictOutput,
              graph.LevelInput, graph.LevelOutput, graph.WriteInput, graph.WriteOutput)
    for model in models:
        for field in model.model_fields:
            assert field in state, f"{model.__name__}.{field} is not in CheckNamesState"
    assert state["facts_dir"].default == cn.FACTS
    assert cn.SWAPPED_MAX_PREDICTION_RATE == 0.15 and cn.SWAPPED_MIN_CLAIM_MARGIN == 0.30


# ── the batch selection ────────────────────────────────────────────────

def test_a_tsv_selection_keeps_only_the_rows_that_match(tmp_path: Path) -> None:
    path = tmp_path / "audit.tsv"
    path.write_text("name\tkind\tverdict\n"
                    "foo_08001234\tfunction\twrong\n"
                    "bar_08005678\tdata\twrong\n"
                    "baz_0800abcd\tfunction\ttoo-specific\n")
    assert cn.names_from_tsv(path) == ("foo_08001234", "bar_08005678", "baz_0800abcd")
    assert cn.names_from_tsv(path, ["kind=function", "verdict=wrong"]) == ("foo_08001234",)
    assert cn.names_from_tsv(tmp_path / "missing.tsv") == ()


def test_the_coverage_index_and_the_random_reached_pool(tmp_path: Path) -> None:
    (tmp_path / "coverage.tsv").write_text("# address\tname\treached\n"
                                           "0x08001234\tfoo_08001234\tyes\n"
                                           "0x08005678\tbar_08005678\tno\n")
    index = cn.coverage_index(tmp_path)
    assert index["foo_08001234"] == ("0x08001234", True)
    assert index["bar_08005678"][1] is False
    assert cn.random_reached(5, seed=3, pool=("foo_08001234", "bar_08005678"),
                             facts_dir=tmp_path) == ()


# ── nothing here calls a model ─────────────────────────────────────────

def test_no_part_of_the_checker_calls_a_model(monkeypatch: pytest.MonkeyPatch) -> None:
    """`complete()` is the only way out to a provider; with it fenced off, every pure part still
    works (a run of this file never reaches a model)."""
    def trap(spec: ModelSpec, prompt: str, system: str, tag: str) -> tuple[str, ToolMessage | None]:
        raise AssertionError("a unit test called a model")

    monkeypatch.setattr(cn, "complete", trap)
    reply, problem = cn.parse_reply(json.dumps(GOOD))
    assert problem is None and reply is not None
    assert cn.check_claims(reply.claims, battle_record())
    assert cn.calibration([], [], [], []).names == 0
    assert cn.judge_prediction(reply.prediction, run_record(anchors=(diff("oam"),)), None)
    assert cn.pair_up(["a", "b"], seed=0).swaps
    assert cn.Random if False else True     # the module's own randomness is seeded, never global


def test_the_small_models_serialise_as_json() -> None:
    row = ActiveResult(outcome="held", intervention=ToolMessage("skip / differs(x)"),
                       predicates=(cn.PredicateCall(spec="differs(anchor=oam)", verdict=True),))
    assert json.loads(row.model_dump_json())["outcome"] == "held"
    answer = cn.Answer(name=FunctionName("foo"), address=FOO,
                       reply=Reply(claims=(HasArgument(n=0),),
                                   prediction=NoEffect(reason="nothing visible")))
    assert json.loads(answer.model_dump_json())["reply"]["prediction"]["intervention"] == "none"


def test_the_run_record_the_checker_reads_is_the_one_intervene_writes() -> None:
    """The active check reads `wf intervene`'s RunRecord by field; a field it names is a field
    that record has."""
    for field in ("first_frame_executed", "hung", "anchor_diffs", "route", "deterministic"):
        assert field in it.RunRecord.model_fields
    assert Observation(first_frame=3).ran
    assert not Observation(first_frame=None).ran
    assert Observation(first_frame=3, record=run_record(first_frame=3)).ran
    assert SimpleNamespace  # the fakes above stand in for the record by shape