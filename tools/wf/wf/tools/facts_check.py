"""The mechanical part of checking a proposed name against runtime facts.

A model may propose a name whose prefix says something the observations contradict: a
``battle_`` name for calls that were all in the overworld, or an ``owPlayer_`` name
for a function that was only seen writing ``oBattleObject`` fields.  The checks in
this module are deliberately narrow.  They use only observations that belong to one
particular call (``scenes_at_call``) or one attributed struct-field write; route
coverage is not a scene and never contradicts a name.

The result is data, not prose: callers can put the contradiction in a retry prompt,
record it as a rejection, or count it in an A/B run.
"""
from __future__ import annotations

import re
from collections import Counter
from typing import Literal

from wf.tools import facts
from wf.types import Result

ContradictionKind = Literal["scene", "struct"]


class Contradiction(Result):
    """One prefix that the runtime facts contradict.

    ``claim`` is what the name says (a scene or a struct family), ``observed`` is what
    the per-call observations actually showed, and ``reason`` is the sentence a retry
    prompt or a rejection record can quote.
    """
    kind: ContradictionKind
    claim: str
    observed: tuple[str, ...]
    reason: str


# ── what a name's leading words claim ────────────────────────────────────────────────

#: The scene vocabulary used by ``scenes.py``.  A leading word not listed here makes no
#: scene claim.
SCENE_CLAIMS: dict[str, str] = {
    "battle": "battle",
    "overworld": "overworld",
    "ow": "overworld",
    "menu": "menu",
    "submenu": "menu",
    "cutscene": "cutscene",
    "title": "title",
}

#: The struct families a recognized struct prefix may mean.  The battle-object copies
#: are one family for this purpose: their fields describe the same kind of object.
STRUCT_CLAIMS: dict[str, tuple[str, ...]] = {
    "battleobject": ("oBattleObject", "oT1BattleObject", "oT3BattleObject", "oT4BattleObject"),
    "owplayer": ("oOWPlayerObject",),
    "chatbox": ("oChatbox",),
    "renderinfo": ("oRenderInfo",),
    "joypad": ("oJoypad",),
    "overworldmapobject": ("oOverworldMapObject",),
    "owmapobject": ("oOverworldMapObject",),
    "mapobject": ("oOverworldMapObject",),
    "aiattack": ("oAIAttackVars",),
    "ai": ("oAIAttackVars", "oAIData", "oAIState"),
    "gamestate": ("oGameState",),
    "battlestate": ("oBattleState",),
    "cutscenestate": ("oCutsceneState",),
    "collisiondata": ("oCollisionData",),
    "camera": ("oCamera",),
    "paneldata": ("oPanelData",),
    "mapscript": ("oMapScriptState",),
}

#: ``battleObject_``, ``battleState_`` and the other battle prefixes name a struct, not
#: a scene.  A recognized struct prefix takes precedence over a scene prefix for the
#: same reason (an ``owPlayer_`` name is about a struct, not merely about the map).
STRUCT_SECOND_WORDS = frozenset({"object", "state", "data", "ui"})


def head_words(name: str) -> list[str]:
    """A name split into words: ``battleObjectHandleChargeAttack`` becomes
    ``battle, Object, Handle, Charge, Attack``; ``battle_drawBcdNumberGrid`` becomes
    ``battle, draw, Bcd, Number, Grid``."""
    return re.findall(r"[A-Z]+(?![a-z])|[A-Z][a-z0-9]*|[a-z0-9]+", name)


def scene_claim(name: str) -> str | None:
    """The scene the name claims, or ``None`` when it makes no scene claim.

    A recognized struct prefix is not a scene claim even when its first word also names
    one (``battleObject_`` and ``owPlayer_`` are the important cases).
    """
    words = head_words(name)
    if not words:
        return None
    first = words[0].lower()
    second = words[1].lower() if len(words) > 1 else ""
    if first == "battle" and second in STRUCT_SECOND_WORDS:
        return None
    if struct_claim(name) is not None and first in {"battle", "ow", "overworld", "cutscene"}:
        return None
    return SCENE_CLAIMS.get(first)


def struct_claim(name: str) -> tuple[str, ...] | None:
    """The struct families the name claims, or ``None`` when it makes no struct claim."""
    words = head_words(name)
    if len(words) < 2:
        return None
    compound = (words[0] + words[1]).lower()
    if compound in STRUCT_CLAIMS:
        return STRUCT_CLAIMS[compound]
    if words[0].lower() in STRUCT_CLAIMS:
        return STRUCT_CLAIMS[words[0].lower()]
    return None


def family_of(field_name: str) -> str:
    """The struct family an ``o*`` field name belongs to: ``oT3BattleObject_Timer``
    belongs to ``oT3BattleObject``."""
    return field_name.split("_", 1)[0]


# ── what the facts actually observed ─────────────────────────────────────────────────

def attributed_field_families(record: facts.FunctionFacts) -> tuple[str, ...]:
    """The families of the field writes that were attributed to a struct.

    Only an ``attributed`` write counts.  Such a write has at least one field name in
    the family the base register pointed into; aliases from unrelated families at the
    same offset are deliberately left out.
    """
    families: set[str] = set()
    for write in record.field_writes:
        if not write.attributed:
            continue
        supported = set(write.candidates)
        if write.struct_guess:
            supported.add(write.struct_guess)
        for field_name in write.field_names:
            family = family_of(field_name)
            if family in supported:
                families.add(family)
    return tuple(sorted(families))


def _majority_scene(record: facts.FunctionFacts) -> tuple[str, int, int] | None:
    """The scene most of the captured calls were in, when one has a strict majority.

    The return value is ``(scene, count, total)``.  ``other`` is an unidentified mode,
    not a scene, so it can never become the majority that contradicts a name.  A tie
    gives no majority and therefore no contradiction.
    """
    if not record.scenes_at_call:
        return None
    counts = Counter(call.scene for call in record.scenes_at_call if call.scene != "other")
    total = len(record.scenes_at_call)
    for scene, count in counts.most_common():
        if count * 2 > total:
            return scene, count, total
    return None


def _scene_contradiction(name: str, record: facts.FunctionFacts) -> Contradiction | None:
    claim = scene_claim(name)
    majority = _majority_scene(record)
    if claim is None or majority is None:
        return None
    scene, count, total = majority
    if scene == claim:
        return None

    # Cutscene commands are dispatched by the map handler, so their mode bytes say
    # ``overworld`` while the code is plainly cutscene code.  When the attributed field
    # family supports the cutscene prefix, that is the stronger evidence and the mode
    # byte is not a contradiction.
    if claim == "cutscene" and any(family.startswith("oCutscene")
                                   for family in attributed_field_families(record)):
        return None

    reason = (f"the proposed name claims scene '{claim}', but the captured calls were "
              f"mostly in '{scene}' ({count} of {total} calls)")
    return Contradiction(kind="scene", claim=claim, observed=(scene,), reason=reason)


def _struct_contradiction(name: str, record: facts.FunctionFacts) -> Contradiction | None:
    claimed = struct_claim(name)
    observed = attributed_field_families(record)
    if claimed is None or not observed:
        return None
    if set(claimed) & set(observed):
        return None
    claim = "/".join(claimed)
    reason = (f"the proposed name claims struct family '{claim}', but the attributed field "
              f"writes belong to '{'/'.join(observed)}'")
    return Contradiction(kind="struct", claim=claim, observed=observed, reason=reason)


def contradictions(name: str, record: facts.FunctionFacts | None) -> tuple[Contradiction, ...]:
    """The ways ``name`` contradicts ``record``.

    A scene is checked only against the per-call scenes, and only when those calls
    have a strict scene majority.  A struct is checked only against attributed field
    writes.  Route-level coverage is never used: it says where a recording went, not
    what scene each call of this function ran in.
    """
    if record is None:
        return ()
    out: list[Contradiction] = []
    scene = _scene_contradiction(name, record)
    if scene is not None:
        out.append(scene)
    struct = _struct_contradiction(name, record)
    if struct is not None:
        out.append(struct)
    return tuple(out)


def check_name(name: str, record: facts.FunctionFacts | None) -> tuple[Contradiction, ...]:
    """Alias for :func:`contradictions`, for callers that read better as a check."""
    return contradictions(name, record)
