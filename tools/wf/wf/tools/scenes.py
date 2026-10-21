"""Which scene the ORIGINAL game was in at one captured call -- the mode bytes `main_`
dispatches on, read out of that call's own entry snapshot.

`wf facts` groups a route's calls under the route's name (`story2`, `battle`), but a route is
an *input tape*, not a scene: `story2` runs a battle, opens a menu and returns to the map, and
`menus3` spends most of its frames in the PET menu. What a name claims is "this runs in a
battle" or "this runs in a menu", so the fact has to be per CALL.

Two bytes say what the game is doing, and `main_` (bn6f/asm/main.s) dispatches on both:

  * `oToolkit_MainJumptableIndexPtr` -- the byte a pointer at `eToolkit + 0x00` points at
    (`eToolkit` is 0x020093B0). `main_` reads it and jumps through `main_subsystemJumpTable`
    (entries 4 bytes apart, so the index IS the table's byte offset): the start screen, the
    game state, the logo screen, the sub-menu, the shop, the chip trader, ...
  * `GameState.SubsystemIndex` -- the first byte of the `GameState` struct, reached through the
    pointer at `eToolkit + 0x3c` (`eGameState` is 0x02001B80). `cbGameState_80050EC` reads it
    and jumps through `GameStateJumptable` (16 entries): map entry, on-map update, battle init,
    battle main, the overworld tick, the scene tick, the PET menu, ...

The game-state byte only means anything while the main byte is `0x04` (the gamestate handler
itself): outside it the game leaves the last one there, so a pair like (0x2C, 0x18) is the shop
with a stale PET-menu byte, and must not be read as a menu. `scene_of_modes` follows that rule.

Each value's scene comes from the handler the jump table names there (plus symbols/db's
one-line summaries). `docs/workflows/facts-notes.md` carries the whole table with its evidence;
the two dicts below are what `wf facts` records per call.
"""
from __future__ import annotations

import struct
from pathlib import Path
from typing import NamedTuple

from wf.types import ToolMessage

# ── where the two mode bytes live (bn6f/include/structs/Toolkit.inc, GameState.inc) ──

MAIN_PTR = 0x020093B0          # eToolkit + oToolkit_MainJumptableIndexPtr: a pointer to the byte
GAMESTATE_PTR = 0x020093EC    # eToolkit + oToolkit_GameStatePtr: a pointer to the GameState
GAMESTATE = 0x02001B80        # eGameState; oGameState_SubsystemIndex is its byte 0

#: the scene vocabulary: the five a claim can name (check_names' `Scene`), plus `other` for a
#: mode this table cannot place (game over, an unnamed full-screen subsystem, an index outside
#: the table). A claim is never made about `other`, and a function only ever seen in `other` is
#: `unknown` to a scene claim rather than contradicted by it.
SCENES: tuple[str, ...] = ("battle", "overworld", "menu", "title", "cutscene", "other")

# ── the two jump tables, and what each index means ────────────────────
#
# main_subsystemJumpTable (bn6f/asm/main.s, `main_`): index -> (handler, scene). `"*"` is the
# one handler whose scene is the second byte's business (`cbGameState_80050EC`).

MAIN_MODES: dict[int, tuple[str, str]] = {
    0x00: ("startscreen_render_802F544", "title"),
    0x04: ("cbGameState_80050EC", "*"),
    0x08: ("ho_jackIn_80341B6", "cutscene"),
    0x0C: ("cb_8038AD0", "other"),                       # unnamed; no capture has run in it
    0x10: ("logoScreen_dispatch_803D1CA", "title"),
    0x14: ("cb_dispatchS200F348State_803FB3C", "other"),  # the game-over state machine
    0x18: ("cb_80395A4", "other"),                        # unnamed; dispatches on eS200A290
    0x1C: ("cb_803CBA6", "other"),                        # unnamed; dispatches on byte_2008978
    0x20: ("cb_803CCD6", "other"),                        # unnamed; dispatches on byte_2009790
    0x24: ("reqBBS_cb_draw_813E0A4", "menu"),
    0x28: ("SubMenuControl", "menu"),
    0x2C: ("ShopControl", "menu"),
    0x30: ("cb_8048FD4", "other"),                        # the eS200A290 subsystem tick
    0x34: ("ChipTraderControl", "menu"),
    0x38: ("cb_81382AC", "other"),                        # unnamed
    0x3C: ("(NULL)", "other"),
    0x40: ("(NULL)", "other"),
    0x44: ("reqBBS_subsystemCotnrol", "menu"),
    0x48: ("HandleEmailMenu81279F8", "menu"),
    0x4C: ("cb_8049E04", "other"),                        # the E-Reader chatbox handler
    0x50: ("(NULL)", "other"),
}

# GameStateJumptable (bn6f/asm/asm00_1.s, `cbGameState_80050EC`): index -> (handler, scene).
GAMESTATE_MODES: dict[int, tuple[str, str]] = {
    0x00: ("EnterMap", "overworld"),
    0x04: ("gamestate_OnMapUpdate_8005268", "overworld"),
    0x08: ("battle_init_80052D8", "battle"),
    0x0C: ("HandlesBattleMainUntilEndOfBattleThenTriggersEnterMap", "battle"),
    0x10: ("overworld_owMap_renderAndTickSubsystems_800536E", "overworld"),
    0x14: ("scene_nextOrUpdate_80053E4", "cutscene"),
    0x18: ("pet_menuUpdateAndHandleExit_8005462", "menu"),
    0x1C: ("sub_800555A", "other"),
    0x20: ("scenario_finalizeFrameOrRestart_8005642", "overworld"),
    0x24: ("gameState_tickFrameAndHandleFadeTransition", "other"),
    0x28: ("start_reloadDuringFadeTick", "overworld"),
    0x2C: ("pet_tickSceneAndReloadOnFade", "menu"),
    0x30: ("gamestate_runOverworldSceneStep_80055CE", "overworld"),
    0x34: ("mainState_runFrame_8005814", "overworld"),
    0x38: ("pet_perFrameTickWithFadeReset", "overworld"),
}

#: the main index whose handler is `cbGameState_80050EC`: only here the game-state byte counts.
GAMESTATE_MAIN = 0x04


class Mode(NamedTuple):
    """The two mode bytes at one captured call, and the scene they add up to."""

    main: int | None
    gamestate: int | None
    scene: str


def scene_of_modes(main: int | None, gamestate: int | None) -> str:
    """The scene one call happened in, from the two mode bytes: `other` when the bytes do not
    name one (an index outside the jump table, or a handler this table cannot place)."""
    if main is None or main not in MAIN_MODES:
        return "other"
    _handler, scene = MAIN_MODES[main]
    if scene != "*":
        return scene
    if gamestate is None:
        return "other"
    row = GAMESTATE_MODES.get(gamestate)
    return row[1] if row is not None else "other"


def handler_of(main: int | None, gamestate: int | None = None) -> ToolMessage:
    """Which handler the game's own jump tables name for these bytes -- the evidence behind
    one row of the table in docs/workflows/facts-notes.md."""
    if main is None:
        return ToolMessage("no main index")
    row = MAIN_MODES.get(main)
    if row is None:
        return ToolMessage(f"main index {main} (0x{main:02X}) is outside main_subsystemJumpTable")
    handler = row[0]
    if main == GAMESTATE_MAIN and gamestate is not None:
        inner = GAMESTATE_MODES.get(gamestate)
        if inner is not None:
            return ToolMessage(f"main 0x{main:02X} {handler} -> gamestate 0x{gamestate:02X} "
                               f"{inner[0]}")
    return ToolMessage(f"main 0x{main:02X} {handler}")


def guest_to_snapshot(address: int) -> int | None:
    """Where GUEST address sits in a captured snapshot's memory -- the regions are concatenated
    in the order tools/mgba_run.c saves them (`runtime_evidence.SNAP_REGIONS`) -- or None when
    the address is not in guest memory."""
    for base, size in ((0x02000000, 0x40000), (0x03000000, 0x8000), (0x04000000, 0x400),
                       (0x05000000, 0x400), (0x06000000, 0x18000), (0x07000000, 0x400),
                       (0x0E000000, 0x8000)):
        if base <= address < base + size:
            return address - base
    return None


def modes_in_memory(memory: bytes) -> Mode:
    """The mode bytes of one snapshot's memory (a `runtime_evidence.Snap.memory`: the regions
    concatenated, the header already stripped)."""
    def read_word(address: int) -> int | None:
        offset = guest_to_snapshot(address)
        if offset is None or offset + 4 > len(memory):
            return None
        return struct.unpack_from("<I", memory, offset)[0]

    def read_byte(address: int | None) -> int | None:
        if address is None:
            return None
        offset = guest_to_snapshot(address)
        if offset is None or offset >= len(memory):
            return None
        return memory[offset]

    main = read_byte(read_word(MAIN_PTR))
    gamestate = read_byte(read_word(GAMESTATE_PTR))
    return Mode(main=main, gamestate=gamestate, scene=scene_of_modes(main, gamestate))


def modes_in_file(path: Path, header_bytes: int) -> Mode:
    """The mode bytes straight out of a `snap_*.bin` file, without reading its 420 KB: two
    pointers and two bytes, each a seek away. HEADER_BYTES is the snapshot's header size
    (`runtime_evidence.SNAP_HEADER_BYTES`)."""
    with Path(path).open("rb") as fh:
        def read_word(address: int) -> int | None:
            offset = guest_to_snapshot(address)
            if offset is None:
                return None
            fh.seek(header_bytes + offset)
            raw = fh.read(4)
            return struct.unpack("<I", raw)[0] if len(raw) == 4 else None

        def read_byte(address: int | None) -> int | None:
            if address is None:
                return None
            offset = guest_to_snapshot(address)
            if offset is None:
                return None
            fh.seek(header_bytes + offset)
            raw = fh.read(1)
            return raw[0] if raw else None

        main = read_byte(read_word(MAIN_PTR))
        gamestate = read_byte(read_word(GAMESTATE_PTR))
    return Mode(main=main, gamestate=gamestate, scene=scene_of_modes(main, gamestate))