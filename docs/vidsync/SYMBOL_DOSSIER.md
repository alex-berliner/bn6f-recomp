# Symbol dossier: what vidsync learned about BN6 Falzar, for transfer to recomp

Written 2026-10-13. Target: `recomp/bn6f` (renames, comments) and `recomp/symbols/db` (one yaml per symbol, with evidence and a repro). Nothing here has been applied to recomp yet.

## What vidsync is

vidsync rebuilds a frame-exact input recording of Komari's BeastSP run of MMBN6 Falzar (US) from the YouTube video (`tAicOP_JS4A`), for recomp's route harness.

- **Sync.** A beam search over the input display, with `tools/vidsync/emu_server.c` (mGBA core) and `tools/vidsync/beam.py`, matches every emulated frame against the video thumbnail.
- **Fixes.** Where the game's luck differs from the video, `tools/vidsync/autosync.py` writes RAM at the end of a game frame. These writes are "pokes", applied at the entry of `main_awaitFrame` (0x080003A0), as the harness's `--poke-at` does. Each fix is kept only if the sync then follows the video for hundreds of frames.
  - Types of fix: RNG seeds, battle pointer, folder, mystery data, rewards, enemy/MegaMan HP, positions, overworld objects.
- **Workers.** A worker server on a second PC runs candidate fixes in parallel.
- **Output.** `build-vidsync/ready/` holds the recording (trace + pokes), verified with `mgba_run`, and save-file segments for the recomp validator.

Because a fix is kept only when hundreds of frames afterwards match the video, every address below marked **runtime** has been exercised many times. It was written or read during the sync, and the game then behaved as the video shows.

## Evidence levels

| Level | Meaning |
|---|---|
| **runtime** | Used by the sync driver or the segment cutter; writes or reads at this address had the effect described, over many fixes |
| **probe** | Shown by a one-off emulator probe (repro given) |
| **read** | Read from the assembly only; not run |

Repro scripts are in `tools/vidsync/probes/` (copied from the session). They run on a PC with the vidsync data, through a wrapper like `tools/vidsync/probes/fcmpj.sh`, against a result or loss json and a state from `data/auto/`.

`RANDOM_EVENTS.md` is an earlier code-reading catalogue of every random decision; its entries are **read** level unless the tables below upgrade them.

---

## 1. RAM: variables and struct fields

### Core

| Address | recomp name now | Finding | Level | Evidence |
|---|---|---|---|---|
| 0x020013F0 | `ePrimaryRngSeed` | Primary RNG. Step: `x' = (rotl(x,1)+1) ^ 0x873CA9E5`. Almost every fix writes it. | runtime | `rngfix.py` PRIMARY; hundreds of fixes |
| 0x02001120 | `eSecondaryRngSeed` | Secondary RNG, same step; folder shuffle, Run roll | runtime | `rngfix.py` SECONDARY |
| 0x0203CBE4 | (in `dword_203CBE0` packet) | The battle seed snapshot at battle phase 4 (`sub_800B144`). The primary is restored from the loopback copy at 0x0203F4A4 at phase 8, so an RNG fix during a battle intro must write all three addresses. | runtime | `segments.py` SEEDS; every RNG fix writes the three |
| 0x0203F4A4 | (`dword_203F4A0`+4) | Loopback copy of the battle seed; source of the phase-8 restore | runtime | as above |
| 0x0200A210 | `iCurrFrame` | +1 per main-loop pass; encounter formation and mystery-data rolls read it. Not saved: a reload starts it elsewhere, so segment junctions write it. | runtime | `segments.py` ICURR |
| *(u32*)0x020093D4 → u16 | eToolkit field | The game-frame counter the route harness unwraps (`--game-frame-addr 0x20093D4`). The pointer target is the u16 counter. | runtime | `export_trace.py` GAME_FRAME_ADDR |
| 0x080003A0 | `main_awaitFrame` | End-of-frame poke point; a write here lands after the frame's logic | runtime | `tools/vidsync/emu_server.c` AWAIT_FRAME_PC; harness `--poke-at` |
| 0x02001C88 | `eEventFlags` | Flag f = byte `f>>3`, mask `0x80 >> (f&7)` (`TestEventFlag`) | runtime + read | `autosync.py` EVENT_FLAGS |

### `eJoypad` (0x0200A270)

| Field | Finding | Level | Evidence |
|---|---|---|---|
| +0x08..+0x0F | Per-key held-time counters: they count up while a key is held and saturate around 0x10 | probe | `probes/seg12joy.py`: the run shows `10 00 00 10`, a fresh reload `01 00 00 01` |
| +0x13 | Key-repeat timer, cycles 0..4 while a direction is held. A held key repeats when it wraps (the "repeat" bits at +0x04 fire then). | **probe, confirmed** | Segment 12 of the recording opened E-Mail instead of NaviCust because this timer was one phase off after a reload. Writing +0x08..+0x17 at the junction fixed it (`probes/seg12fix.py`, `segments.py` ALWAYS) |

Suggested `ewram.s`/struct comment: `+0x08 held frames per key (8 x u8), +0x13 key repeat phase (0-4)`.

### `eGameState` (0x02001B80)

| Field (address) | Finding | Level |
|---|---|---|
| +0x00 (0x02001B80) | Subsystem byte: 0x04 overworld, 0x0C battle, 0x18 PET menu (seen) | runtime (battle/map), probe (0x18) |
| +0x04/+0x05 (0x02001B84/85) | MapGroup, MapNumber | runtime |
| +0x0C (0x02001B8C) | LastMapGroup/LastMapNumber: a reload sets them its own way, so segment junctions write them | runtime |
| +0x0E (0x02001B8E) | CoordInteractionValue: the trigger code of MegaMan's tile (already in `GameState.inc`) | probe (`probes/trig.py`) |
| +0x18 (0x02001B98) | OverworldPlayerObjectPtr → `eOWPlayerObject` (0x02009F40): X/Y/Z at +0x1C/+0x20/+0x24 (s16.16), NextX/Y/Z at +0x28.., facing +0x10 | runtime (`try_relocate`, segment junctions) |
| +0x1C (0x02001B9C) | CurBattleDataPtr: an encounter's battle settings, set on the trigger frame. Writing another table entry picks the formation. | runtime (`try_formations`) |
| +0x24 (0x02001BA4) | PlayerX/Y/Z where a map (re)load puts MegaMan, after a battle too | runtime |

### `eStruct2001c04` (0x02001C04)

| Field | Finding | Level |
|---|---|---|
| +0x12 (0x02001C16) | Walk counter: `>>6`, max 16, picks the encounter odds. Writing 0x400 makes the next roll able to start a battle. | runtime (`try_encounter`, fixed the stall at game 262844) |
| +0x24 (0x02001C28) | SneakRun steps left | runtime (`try_subchip`) |
| +0x28 (0x02001C2C) | LocEnemy steps left | runtime |

SneakRun on/off is flag 0x170A (byte 0x02001F69, mask 0x20); LocEnemy is flag 0x170B (mask 0x10). When the flag is set and the counter is 0, `npc_80350BC` runs the "<item> just expired!" cutscene.

### Mystery data, chips, rewards

| Address | Finding | Level |
|---|---|---|
| 0x02004348 + 2·(id−0x1400) | One u16 per mystery-data id: the two picked indexes, rolled at jack-in | runtime (`try_gmd`) |
| 0x02002230 | Chip pack: one count byte per (chip id × 12 + code index) | runtime (`try_hand` pack chips) |
| 0x02002178 | Saved folder | runtime |
| 0x0203CDB0 | `eBattleFolder`: 30 × u16 (id \| code<<9); hand = first N | runtime (`try_hand`) |
| 0x0203F4AC / 0x02035268 | The battle reward on its way to the result screen (arrival / copy) | runtime (`check_reward`) |
| 0x0200A00C | `eStruct200A008`+4: the reward actually given (packed u16) | runtime |
| 0x020364D4 | Result screen display queue entry for it | runtime |
| 0x02034880 | BattleState: enemy lists at +0x4C + 8·side, counts at +8+side; phase byte +0x02 | runtime |

### Battle objects

| Address | Finding | Level |
|---|---|---|
| 0x0203A9B0 + n·0xD8 | T1 battle objects: HP +0x24, MaxHP +0x26, Alliance +0x16 (0 = MegaMan's side, 1 = enemies). Object 0 is MegaMan (HP at 0x0203A9D4). | runtime (`try_kill`: enemy HP 0 ends a battle when the video's does; `try_kill(side=0)`: MegaMan deleted, the run's game over at 375361) |
| 0x02034000 | `eActiveT1BattleObjectsBitfield`: one bit per live object, object 0 = bit 31 | runtime |

### PET menu

| Address | Finding | Level |
|---|---|---|
| `ePETMenuData` (0x0200DF20) +4 | The selected main-menu entry while a submenu opens (3 = the entry that opened NaviCust in the run, 4 = E-Mail) | probe (`probes/seg12open.py`) |
| +0x0A..+0x0F | Cursor/animation bytes; `pet_initMenu_811EC10` zeroes 0x18 bytes, so nothing here survives a menu reopen | probe |

### Overworld objects

| Address | Finding | Level |
|---|---|---|
| `eOverworldNPCObjects` 0x020057B0, 16 × 0xD8 | Active = header bit 0; X +0x24, Y +0x28 (matches `OverworldNPCObject.inc`) | probe (`probes/npcs.py`) |
| `eOverworldMapObjects` 0x02011EE0, 56 × 0x78 | Header byte 1 = object type (index into `off_8004724`); +4 params (low byte = instance index); X +0xC, Y +0x10 (s16.16) | probe (`probes/mapobjs.py`, `probes/obj0.py`) |
| `eCamera` 0x02009980 +0x30/+0x34 | Camera X/Y, following MegaMan (s16.16), duplicated at +0x3C/+0x40 | probe (`probes/movers.py`), hypothesis |

### `unk_2000310` (0x02000310): the Mr. Weather comp puzzle state (0x60 bytes)

Proposed name: `eWeatherCompState`. `sub_808FDC0` zeroes it on comp entry and sets +4 = 0xFF, +0x20 = 0xFF, +8 = 1, +0x16 = 3.

| Field | Finding | Level |
|---|---|---|
| +0x02 | Step counter of the reset routine `sub_808F810` | read |
| +0x06 | Rises by 1 every 6 frames while MegaMan's rainbow-orb trail grows (0x08 → 0x10). The run's loss at 432736 is the frame it reaches 0x10. | probe (`probes/trig.py`) |
| +0x0E | Trigger index (code − 0x70 or − 0x60) from `sub_8090004` / `sub_8090020` | read |
| +0x0F / +0x10 | Animation and facing given to MegaMan by the reset | read |
| +0x12 / +0x14 (u16) | The point MegaMan is put back at (`sub_808F810`) | read |
| +0x19+i, +0x31+i | Per-tornado bytes written by `sub_80A9004` (+0x31+i: the last direction, so the next differs) | read |
| +0x38 (0x02000348, s16.16) | A countdown, −1 per frame; also copied at tornado object +0x25. Writing it does not change the tornado. | probe |
| +0x40 / +0x46 (s16) | Copies of tornado 0's X/Y home position | probe (`probes/dump300.py`) |

---

## 2. Functions and data

### Mr. Weather comp: the tornado (map object type 0x1A)

| Symbol | Proposed name | Finding | Level |
|---|---|---|---|
| `off_8004724` | `OverworldMapObjectUpdateJumptable` | Per-type update routines, indexed by header byte 1 (`TryUpdateEachOverworldMapObject_80048D2`). Entries 0x1A–0x1D are `sub_80A8E74`, `sub_80A92B8`, `sub_80A9430`, `sub_80A9658`. | read |
| `sub_80A8E74` | `owMapObject_weatherTornado_80A8E74` | Tornado update. r6 = `unk_2000310`. State at +8 via `off_80A8E88`: 0 init `sub_80A8E9C`, 4 active `sub_80A8F08`, 8 caught `sub_80A8FA0`, 0xC free when its animation ends (`sub_80A8FE4`). | probe (writing its fields moved the tornado as predicted, `probes/obj0.py`) |
| `sub_80A8E9C` | `…_init` | Sprite 0x80/0x1C/0x6C, animation 0x18; home from `sub_80A90B0`. With flag 0xBFB set: already caught → free, else `sub_80A9268`. Otherwise mode +9 = 0, +0x28 = 0, +0x24 = 1. Then `sub_80A908C`. | read |
| `sub_80A8F08` | `…_active` | Flag 0xBEF → state 8. Else, outside cutscenes/chatbox: `sub_80A8F48` (move) and `sub_80A91A4` (hit check). | read |
| `sub_80A8F48` | `…_moveOrPause` | Mode +9: 0 = moving, nonzero = paused. Moving: +0x24 counts down; while nonzero it steps (`sub_80A9038`) and on a blocked tile re-picks (`sub_80A90E4`). At 0 it reloads +0x24 from +0x26, draws a direction (`sub_80A9004`), and if `sub_80A9150` says so, pauses for +0x27 frames (+0x25 countdown). | probe (pause/move/direction fields; the 308-state search in `try_tornado` follows the video's path) |
| `sub_80A9004` | `…_pickDirection` | Direction +5 = random(8), changed if equal to the last (`unk_2000310`+0x31+i); +0x28 += 1 | read |
| `sub_80A9038` | `…_step` | Steps X/Y by +0x2C (orthogonal on screen) or +0x30 (half, diagonal) in direction 0..7 | read |
| `sub_80A908C` | `…_loadParams` | From the `sub_80A916C` table: speed +0x2C, +0x30 = speed/2, move period +0x26, pause length +0x27, +7 | read |
| `sub_80A90B0` | `…_loadHome` | Home X/Y (table +8/+0xA), Z = 0x40 | read |
| `sub_80A90CC` | `…_isBlocked` | Coordinate trigger 0xC0 at its position = blocked | read |
| `sub_80A90E4` | `…_repickDirection` | Random direction ≠ current; undoes the step and retries while blocked | read |
| `sub_80A91A4` | `…_checkHitPlayer` | If flag 0xBE4 is set and MegaMan is within 0x14 px: set flag 0xBE6 | read |
| `sub_80A91D4` | `…_isCaught` | Tests flag `byte_80A91EC[i]` = 0xBE9 / 0xBEA / 0xBEB for tornado instance i | read |
| `sub_80A91F4` | `…_setPriority` | Priority 1 while 0xBE6 is set, else from the layer | read |

### Mr. Weather comp: other objects and scripts

| Symbol | Proposed name | Finding | Level |
|---|---|---|---|
| `sub_80A92B8` | `owMapObject_rainbowOrb_80A92B8` (type 0x1B) | Probably MegaMan's rainbow-orb trail. State 4 (`sub_80A931C`): flag 0xBE6 → fade to white (state 8, `sub_80A9360`/`sub_80A937C` with table `byte_80A93C0`), then set flag 0xBE7 and free; flag 0xBE4 → free. Bobs via `sub_80A93E8`. | read; the white fade seen in both the run and the video at 432736 |
| `sub_80A95A8` | (type 0x1C) | MegaMan within 12 px at the same Z → set flag 0xBFC, sound 0x1A4, state 8 | read |
| `sub_80A9614` / `sub_80A9628` | | Set / test flag `byte_80A9648[index]` | read |
| `sub_808FDC0` | `weatherComp_init_808FDC0` | Comp entry, once (flag 0xBFB): zero `unk_2000310`; clear flags 0xBF4..+7, 0xBE9..+6, 0xBF2; set 0xBE0 | read |
| `sub_8090004` / `sub_8090020` | `weatherComp_checkTrigger70/60` | MegaMan's tile trigger 0x70–0x79 / 0x60–0x69 → index in +0xE | read |
| `sub_8090104` | `weatherComp_pickScript_8090104` | Starts cutscene `byte_808F668` (trigger 0x7x) or `byte_808F67C` (0x6x) | read |
| `byte_808F668` (`data/dat24.s:3089`) | | Script: lock player, set flag 0x1731, call `sub_808F810`, game-state byte 9 = 3 | read |
| `sub_808F810` | `weatherComp_putPlayerBack_808F810` | Moves MegaMan to `unk_2000310`+0x12/+0x14 (Z 0x40), animation +0xF, facing +0x10, sets flag 0xBE4 | read |
| `sub_80900C8` | | Spawns map object 0x1D near MegaMan, sets flags 0xBE5, PET_DISABLED, 0x1739 | read |

### General routines

| Symbol | Finding | Level |
|---|---|---|
| `reqBBS_81409D0` | Misnamed: a general random helper used by map objects. **Its DB entry has the division backwards** (see §3). | read |
| `TryUpdateEachOverworldMapObject_80048D2` | Map-object update loop: skipped while flag NON_NPC_ANIMATION_LOCKED is set; calls `off_8004724[type]` for active objects | read |
| `sub_8120CC8` | Table lookup with a fallback: `t[i]`, or `t[0]` if `t[i]` is 0xFF (used by `pet_initMenu_811EC10`) | read |

### Data tables (all runtime, from `autosync.py`)

| Address | Content |
|---|---|
| 0x08020170..0x08020188 | The four encounter root tables `pickBattleSettingsEntry_80AA5F4` chooses from by flags |
| 0x08021DA8 | ChipData, 44 bytes per chip id, its codes first |
| 0x080A336C | InternetMysteryDataMapGroupEntries: {map group, maps}, ended by 1 |
| 0x080AAEA8 | Reward tables: 20 u16 per enemy id (0x28 bytes) |
| 0x080182C4 | 3 bytes per enemy id: version, actor type, AI index (`GetVerActorTyAndAIIdx_80182B4`) |
| 0x080AED74 | [actor type][AI index] → 0x20 bytes per version: the chips a virus can hold (`sub_80AED50`) |
| 0x080211A0 | Battle-start mystery-data contents: 8 u16 per row (setup byte 2 >> 4), RNG & 0xE |

---

## 3. Conflicts with the current symbol DB (review before transfer)

| DB entry | DB says | vidsync says | Who is right |
|---|---|---|---|
| `reqBBS_81409D0.yaml` (hypothesis) | "input % random byte" | `svc 6` is Div(r0, r1) with r0 = `GetRNG() & 0xFF` and r1 = the argument; it returns r1 = the remainder, so the result is **(GetRNG() & 0xFF) % n**, in 0..n−1 | vidsync, from the assembly; the tornado's direction is `random(8)` in 0..7 |
| `battle_rollRandomItemDrops_80AA88C.yaml` (confirmed) | post-battle item drop roll | `RANDOM_EVENTS.md` §3: the enemy held-chip roll at enemy spawn | check: one of the two misreads its caller |
| `damage_jitter_80AAA98.yaml` (confirmed) | per-hit damage jitter | `RANDOM_EVENTS.md` §3: picks the reward table entry (`sub_80AAA98(mode 0)`) | check |
| `setupNaviDisplay_802C34E.yaml` (confirmed) | builds the navi info screen | `RANDOM_EVENTS.md` §3: copies the reward record into the result screen queue (0x020364D4) and `eStruct200A008` | check; the runtime addresses (0x020364D4 / 0x0200A00C) are right whichever routine writes them |
| `sub_800B444.yaml` (hypothesis) | copies two saved 12-byte snapshots back | `RANDOM_EVENTS.md`: copies 12 bytes 0x0203F4A4 → 0x02035260 (the reward record) | consistent; vidsync adds what the bytes are |

---

## 4. Still open

- **The Mr. Weather comp outcome at game 432736.** In the video, when the orb trail reaches 0x10 (`unk_2000310`+6), the rainbow meter empties and the view moves. In the run it doesn't. Tried without success:
  - the tornado's place, phase and direction;
  - flags 0xBE0–0xBFF;
  - putting the tornado on MegaMan.

  The meter/view change is likely `sub_808F810` or a sibling started from `sub_8090104`'s other branches (`sub_80902DC`, `sub_808FF8C`); not yet read.
- `unk_2000310` fields +0x04, +0x08, +0x16, +0x20 and +0x31.. are set or used but not understood.
- `RANDOM_EVENTS.md` §8 lists the open questions from code reading. Of those, the battle-seed restore (§8.1) is confirmed at runtime: every RNG fix in a battle intro needs 0x0203CBE4 and 0x0203F4A4 written along with 0x020013F0.

---

## 5. Found in the session transcript only (added after mining it)

The vidsync git history holds nothing beyond the files above. The Claude session transcript (`~/.claude/projects/-home-alex-Code-recomp/bb066827-df96-49e7-a696-c1663c454b73.jsonl`, 120 MB, 2026-10-10..13) holds the earlier research; these items from it are not elsewhere:

| Address | Finding | Level |
|---|---|---|
| 0x02001F08 masks 0x20 / 0x10 | Event flags 0x1402 / 0x1403: a mystery data's "taken" flags (flag 0x1400 + n); must be clear for it to appear; read by the NPC init on map load | read |
| 0x020047CC | Zenny (u32), as watched by a segment probe | probe, unconfirmed |
| 0x02006740 | Bug frag count (`sub_813D48C`, the Bug Frag trader) | read |
| 0x0203CCA8..AB | The two `sub_81209DC` results that `sub_800B144` stores in the battle packet (bytes 0xC8–0xCB of `dword_203CBE0`) | read (same as `RANDOM_EVENTS.md` §1) |
| 0x02008172 | A second folder-like list checked while finding the saved folder (the folder is 0x02002178) | probe, meaning unknown |
