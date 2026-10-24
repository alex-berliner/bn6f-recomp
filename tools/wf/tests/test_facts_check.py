"""The mechanical name/facts contradiction check, over fake runtime records."""
from wf.tools import facts_check
from wf.tools.facts import FieldWrite, FunctionFacts, SceneAtCall
from wf.types import FunctionName, GuestAddress


def record(*, scenes: tuple[str, ...] = (), fields: tuple[tuple[str, ...], ...] = (),
           attributed: bool = True) -> FunctionFacts:
    """A fake function record with only the observations the check may use."""
    return FunctionFacts(
        address=GuestAddress(0x08001234),
        address_hex="0x08001234",
        name=FunctionName("sub_08001234"),
        end=GuestAddress(0x08001244),
        scenes_at_call=tuple(
            SceneAtCall(state=index, snapshot=f"snap_{index}.bin", scene=scene)
            for index, scene in enumerate(scenes)),
        field_writes=tuple(
            FieldWrite(base="r5", base_address=GuestAddress(0x02036000), symbol="eObject",
                       offset=8, struct_offset=8, size=4, count=1, field_names=names,
                       struct_guess=names[0].split("_", 1)[0] if names else "",
                       candidates=(names[0].split("_", 1)[0],) if names else (),
                       attributed=attributed)
            for names in fields),
    )


def test_route_level_facts_never_contradict_a_name() -> None:
    """There is no per-call scene here, so even a scene prefix cannot be refuted."""
    facts = record(fields=(("oBattleObject_Timer",),))
    assert facts_check.contradictions("battle_update", facts) == ()


def test_a_scene_prefix_must_fit_the_per_call_majority() -> None:
    facts = record(scenes=("overworld", "overworld", "overworld", "battle"))
    found = facts_check.contradictions("battle_update", facts)
    assert len(found) == 1 and found[0].kind == "scene"
    assert found[0].claim == "battle" and found[0].observed == ("overworld",)
    assert "3 of 4 calls" in found[0].reason

    # A minority is not a contradiction, and a tie has no majority to contradict.
    assert facts_check.contradictions("battle_update", record(scenes=("battle", "overworld"))) == ()
    assert facts_check.contradictions("battle_update", record(scenes=("overworld", "battle"))) == ()


def test_an_unidentified_mode_does_not_become_a_scene() -> None:
    facts = record(scenes=("other", "other", "other"))
    assert facts_check.contradictions("battle_update", facts) == ()


def test_an_attributed_struct_family_must_fit_the_name() -> None:
    facts = record(fields=(("oBattleObject_Timer",),))
    found = facts_check.contradictions("owPlayer_update", facts)
    assert len(found) == 1 and found[0].kind == "struct"
    assert found[0].claim == "oOWPlayerObject" and found[0].observed == ("oBattleObject",)
    assert facts_check.contradictions("battleObject_update", facts) == ()


def test_an_unattributed_field_write_does_not_contradict_a_struct_name() -> None:
    facts = record(fields=(("oBattleObject_Timer",),), attributed=False)
    assert facts_check.contradictions("owPlayer_update", facts) == ()


def test_the_cutscene_mode_byte_caveat_is_kept_when_the_field_supports_it() -> None:
    """Cutscene commands run under the map handler's overworld mode byte.  The attributed
    oCutsceneState field is the stronger evidence, so the name is not contradicted."""
    facts = record(scenes=("overworld", "overworld"),
                   fields=(("oCutsceneState_CutsceneScript3LongPauseTimer",),))
    assert facts_check.contradictions("cutscene_updateFacing", facts) == ()


def test_a_recognized_struct_prefix_is_not_also_a_scene_claim() -> None:
    """`owPlayer_` and `battleObject_` are struct families, not claims about the scene."""
    facts = record(scenes=("menu", "menu"))
    assert facts_check.contradictions("owPlayer_update", facts) == ()
    assert facts_check.contradictions("battleObject_update", facts) == ()
