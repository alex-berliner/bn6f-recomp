#!/usr/bin/env python3
"""The game addresses vidsync reads and writes, by name from bn6f: the ELF's symbols (RAM and ROM labels)
and the struct offsets bn6f/include/structs defines (the assembler keeps them in the ELF as absolute
symbols: oGameState_MapGroup, ...). A rename in bn6f flows in here; a name bn6f no longer has stops the
import with the name, so a stale address never reaches a poke.

    import syms                                     # scripts in tools/vidsync
    syms.PRIMARY_RNG_SEED, syms.GAME_FRAME_ADDR     # ints

    tools/vidsync/syms.py               print the bn6f symbols vidsync uses and every constant
    tools/vidsync/syms.py --cflags      emu_server's -D flags (tools/Makefile)
    tools/vidsync/syms.py --json OUT    the symbols vidsync uses, for a machine without bn6f (the compute
                                        PCs): there VIDSYNC_SYMS=OUT makes this module read them from OUT

Addresses that bn6f names only automatically (unk_, dword_, byte_, word_, off_: the disassembler's names,
about to change) or not at all stay numbers here, each with a `# TODO: name in bn6f` comment, until bn6f
gives them a name.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS)
import gamepaths  # noqa: E402  # tools/gamepaths.py: the game profile (tools/paths.env)

SYMS_FILE = os.environ.get("VIDSYNC_SYMS")   # set on machines without bn6f (--json writes it)


def elf_symbols(elf: str = gamepaths.ORIG_ELF) -> dict[str, int | None]:
    """bn6f.elf's symbols, name -> value (code labels without the Thumb bit). A name defined twice with
    different values maps to None: using it is an error."""
    try:
        nm = subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError) as e:
        sys.exit(f"syms.py: cannot read bn6f's symbols ({elf}): {e}. On a machine without bn6f, set "
                 f"VIDSYNC_SYMS to a file written by `tools/vidsync/syms.py --json`.")
    syms: dict[str, int | None] = {}
    for line in nm.splitlines():
        f = line.split()
        if len(f) != 3:
            continue
        value = int(f[0], 16) & (~1 if f[1] in "tT" else ~0)
        if syms.setdefault(f[2], value) != value:
            syms[f[2]] = None
    return syms


if SYMS_FILE:
    with open(SYMS_FILE) as fh:
        _SYMBOLS: dict[str, int | None] = json.load(fh)
else:
    _SYMBOLS = elf_symbols()
USED: dict[str, int] = {}   # the bn6f symbols this module used, name -> value (--json writes them)


def at(*names: str) -> int:
    """The sum of the named symbols' values: a label, or a label plus struct offsets."""
    total = 0
    for name in names:
        value = _SYMBOLS.get(name)
        if value is None:
            why = "is defined with two values" if name in _SYMBOLS else "is not a symbol"
            sys.exit(f"syms.py: {name} {why} in bn6f ({SYMS_FILE or gamepaths.ORIG_ELF}). Renamed? "
                     f"Update tools/vidsync/syms.py.")
        USED[name] = value
        total += value
    return total


# ── the frame loop ──
AWAIT_FRAME_PC = at("main_awaitFrame")                  # where pokes go in (as mgba_run --poke-at)
GAME_FRAME_ADDR = at("eToolkit", "oToolkit_CurFramePtr")   # u32 pointer to the u16 game-frame counter
ICURR_FRAME = at("iCurrFrame")                          # decides encounter formations and mystery data

# ── RNG ──
PRIMARY_RNG_SEED = at("ePrimaryRngSeed")
SECONDARY_RNG_SEED = at("eSecondaryRngSeed")
# A battle's intro snapshots the primary seed into the link packet and restores it ~62 frames later from
# its looped-back copy (RANDOM_EVENTS.md, docs/vidsync/SYMBOL_DOSSIER.md).
BATTLE_SEED_COPY_LINK = 0x0203CBE4   # TODO: name in bn6f (dword_203CBE4 now; dword_203CBE0+4)
BATTLE_SEED_COPY = 0x0203F4A4        # TODO: name in bn6f (byte_203F4A4 now; dword_203F4A0+4)

# ── GameState ──
GAME_STATE = at("eGameState")
GS_SUBSYSTEM = at("eGameState", "oGameState_SubsystemIndex")             # 0x04 on the map, 0x0C in battle
GS_MAP = at("eGameState", "oGameState_MapGroup")                         # MapGroup, MapNumber (u8 each)
GS_LAST_MAP = at("eGameState", "oGameState_LastMapGroup")                # LastMapGroup, LastMapNumber
GS_OW_PLAYER_PTR = at("eGameState", "oGameState_OverworldPlayerObjectPtr")
GS_BATTLE_PTR = at("eGameState", "oGameState_CurBattleDataPtr")
GS_PLAYER_XYZ = at("eGameState", "oGameState_PlayerX")                   # PlayerX/Y/Z (u32 each)
GS_COORD_INTERACTION = at("eGameState_CoordInteractionValue")         # the map's trigger code under MegaMan

# ── overworld counters, event flags, mystery data ──
S2001C04 = at("eStruct2001c04")
ENC_STEPS = at("eStruct2001c04", "oS2001c04_Unk_12")        # walk counter: (>> 6, max 16) picks the encounter odds
SNEAKRUN_STEPS = at("eStruct2001c04", "oS2001c04_Unk_24")   # SneakRun's movement counter
LOCENEMY_STEPS = at("eStruct2001c04", "oS2001c04_Unk_28")   # LocEnemy's movement counter
EVENT_FLAGS = at("eEventFlags")                             # flag n at byte n >> 3, mask 0x80 >> (n & 7)
SUBCHIP_FLAGS = EVENT_FLAGS + (0x170A >> 3)                 # flags 0x170A (SneakRun on) and 0x170B (LocEnemy on)
MYSTERY_DATA = at("eMysteryDataRolls")                    # u16 per mystery data id, rolled at jack-in

# ── chips ──
CHIP_PACK = 0x02002230      # TODO: name in bn6f (unk_2002230 now): a count byte per chip id and code
BATTLE_FOLDER = at("eBattleFolder")                          # 30 u16 chips (id | code << 9); the hand is the first 5

# ── battle ──
BATTLE_STATE = at("eBattleState")                            # enemy lists at +0x4C + 8*side, counts at +8+side
T1_OBJECTS = at("eT1BattleObject0")
T1_ACTIVE = at("eActiveT1BattleObjectsBitfield")
T1_OBJECT_SIZE = at("oT1BattleObject_Size")
REWARD_GIVEN = 0x0200A00C   # TODO: name in bn6f (word_200A00C now): the battle reward actually given
REWARD_SHOWN = at("eS20364C0", "oS20364C0_Unk_14")           # the result screen's display queue entry for it
REWARD_NAVI_STATS = at("eBattleNaviStats203F4AC")            # rewards written here (and below) before the screen
REWARD_RECORD = 0x02035268  # TODO: name in bn6f (dword_2035268 now): the reward record sub_800B444 copies back

# ── overworld objects, joypad, the PET menu ──
OW_MAP_OBJECTS = at("eOverworldMapObjects")
OW_MAP_OBJECT_SIZE = at("oOverworldMapObject_Size")
OW_NPC_OBJECTS = at("eOverworldNPCObjects")
OW_NPC_OBJECT_SIZE = at("oOverworldNPCObject_Size")
OW_NPC_ACTIVE = at("eActiveOverworldNPCObjectsBitfield")    # bit n: NPC object n is active
WEATHER_COMP_STATE = at("eWeatherCompState")                 # the Mr. Weather comp (symbols/db/eWeatherCompState.yaml):
                                                             # +0x28.. the tornadoes' saved state, +0x40/+0x46 tornado 0's X/Y
PET_MENU_DATA = at("ePETMenuData")
JOYPAD = at("eJoypad")
JOYPAD_HELD_TIMERS = at("eJoypad", "oJoypad_AHeldTimer")    # +8..+0x11 held times, +0x13 the repeat phase
JOYPAD_REPEAT_TIMER = at("eJoypad", "oJoypad_LowSensitivityTimer")

# ── ROM tables ──
ENC_ROOTS = 0x08020170      # TODO: name in bn6f (off_8020170 now): 4 encounter roots, 8 bytes apart
CHIP_DATA = at("ChipDataArr")                                # 44 bytes per chip id: its codes first
MYSTERY_DATA_GROUPS = at("InternetMysteryDataMapGroupEntries")   # {map group, maps}, ended by 1
REWARD_TABLES = 0x080AAEA8  # TODO: name in bn6f (byte_80AAEA8 now): 20 u16 rewards per enemy id
ENEMY_KINDS = at("EnemyVerActorTyAndAIIdxTable")            # version, actor type, AI index per enemy id
HELD_TABLES = 0x080AED74    # TODO: name in bn6f (off_80AED74 now): the chips a virus can hold
BATTLE_MYSTERY_DATA = 0x080211A0   # TODO: name in bn6f (byte_80211A0 now): battle-start mystery data rows

if int(gamepaths.GAME_FRAME_ADDR, 16) != GAME_FRAME_ADDR:
    sys.exit(f"syms.py: GAME_FRAME_ADDR {GAME_FRAME_ADDR:#x} (bn6f) is not tools/paths.env's {gamepaths.GAME_FRAME_ADDR}")


def constants() -> dict[str, int]:
    """Every constant above, name -> value."""
    return {k: v for k, v in globals().items() if k.isupper() and isinstance(v, int)}


def main() -> None:
    args = sys.argv[1:]
    if args == ["--cflags"]:
        print(" ".join(f"-D{k}={constants()[k]:#010x}u" for k in ("GAME_FRAME_ADDR", "AWAIT_FRAME_PC", "PRIMARY_RNG_SEED")))
    elif len(args) == 2 and args[0] == "--json":
        with open(args[1], "w") as fh:
            json.dump(USED, fh, indent=1, sort_keys=True)
    elif not args:
        for name, value in sorted(USED.items()):
            print(f"{value:#010x}  {name}")
        print()
        for name, value in constants().items():
            print(f"{value:#010x}  {name}")
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
