# BN6 Falzar (US): everything vidsync deciphered, per symbol

The complete reference: it merges `SYMBOL_DOSSIER.md`, `RANDOM_EVENTS.md` (itemised here), the driver's code comments and findings recovered from the session transcript. Organised by subsystem. Written 2026-10-13. For moving it into recomp's `symbols/db`, see `docs/vidsync/README.md`.

**Levels:**

| Level | Meaning |
|---|---|
| **R** (runtime) | The sync driver writes or reads this and the game then follows the video, over many fixes |
| **P** (probe) | An emulator probe showed it (scripts in `tools/vidsync/probes/`) |
| **A** (assembly) | Read from the disassembly only |
| *(guess)* | An inference |

File:line references are to `bn6f/asm/*.s` (this repo) unless a path is given.

---

## 1. Random number generators

| Symbol | Address | Finding | Level |
|---|---|---|---|
| step function | `asm00_0.s:2751-2765` | All generators: `x' = (rotl(x,1) + 1) ^ 0x873CA9E5`. Every state lies on one orbit, so "seed" and "position in the stream" are the same thing. | R (`rngfix.rng_step`, every RNG fix) |
| `ePrimaryRngSeed` | 0x020013F0 | Read by `GetRNG` (32 bits), `GetPositiveSignedRNG` (bit 31 clear), `ShuffleByteList_PrimaryRNG`, `ShuffleHwordList_PrimaryRNG` (2 calls per swap) | R |
| `eSecondaryRngSeed` | 0x02001120 | Read by `GetRNGSecondary`, `GetPositiveSignedRNGSecondary`, `Shuffle*_SecondaryRNG`. 0 after the boot RAM clear. | R |
| `dword_20018E8` | 0x020018E8 | Third generator; only `dead_rng_800157C` (`asm00_0.s:2833`) uses it, and nothing calls that | A |
| `SeedRNG` | `main.s:5`, `asm00_0.s:2738` | Boot: primary = 0xA338244F. On load only if the save checksum or timestamp check fails (`loc_803F870`, `asm03_1_1.s:13567`). | A |
| `reqBBS_81409D0` | 0x081409D0 | **Misnamed:** `random(n) = (GetRNG() & 0xFF) % n` (svc 6 remainder). Used by map object types 5, 0x11 (JudgeMan trial `sub_808CBD8`) and 0x1A (tornado direction). The current DB entry reads the division backwards. | A |
| `sub_8000E3A` | `asm00_0.s:1623` | Weighted pick: `2 + (rng>>30) + (iCurrFrame&3)` `GetRNG` calls, then `(last & 0xFFFF) % sum`. Used for mystery data. | A |

### Per-frame advances (no decision, but they move the stream)

| Where | Advance | Ref |
|---|---|---|
| Main loop, every frame, every mode | secondary +1 (`GetRNGSecondary` after the subsystem callback) | `main.s:43` |
| Game-state subsystem (overworld and battle) | primary +1, secondary +1 (`cbGameState_80050EC`), paused or not | `asm00_1.s:4314-4315` |
| Title screen | primary +1 (`startscreen_render_802F544`) | `asm03_0.s:19512` |
| PET submenu | primary +1 (`SubMenuControl`), except submenu states 0x18/0x1C | `asm32.s:34252` |
| Chip trader | primary +1 (`ChipTraderControl`) | `asm03_2.s:7061` |
| Every map load | 1 `GetRNG` (`RandomizeExtraToolkitPointers`), from `EnterMap`, battle start `sub_800531C`, game states 0x14/0x1C/0x20/0x24/0x28/0x30 | `asm00_1.s:7664-7674`, 4386, 4581, 4703-4959 |
| New Game | about 0x300+ (`encryption_initAll_8006d00` retries, `anticheat_modCardsInit_8007036`, from `initNewGameData_8004DF0`) | `asm00_1.s:7802-7893`, 8368 |
| Each save | about 97 (`save_BackupToGamePakWithRetry_803FAB4`, `rollRngSeedIntoExtraToolkitPtrs_8006e26`) | `asm03_1_1.s:13907-13949`, `asm00_1.s:7986` |
| Each load | 1 (`RandomizeExtraToolkitPointers`) | |
| Shop items | ≥1 each (`storeObfuscatedRandomToken_804733C`, 5 calls in `ShopControl`) | `asm03_2.s:2305` |
| Every hit spark | 1 (`AddRandomVarianceToTwoCoords`, from `object_spawnHiteffect`, `object_spawnCollisionEffect`; 24 sites) | `object.s:6142`, `asm00_2.s:22005, 26005` |
| Every MegaMan charge shot | 1, +1 if navi stat 0x4D ≠ 0 (`megamanChargeShotBPwrAtk_init_8011A26` → `sub_8013D5E`; `busterBugChargeShotDamageCalcHappensHere_8011A7E`) | `asm00_2.s:11461, 6208` |
| NaviCust bugs (stat-gated) | `sub_8013DA0` emotion bug (1 per 60 frames), `sub_8013E58` custom bug, `sub_8013CC4` panel bug, `battle_RollNaviStatChance_8013FAE` | `asm00_2.s` |
| Camera shakes | secondary (`camera_doShakeEffect_80301e8`); cosmetic, shifts the next folder shuffle | `asm03_0.s:21282` |
| Story explosions, map object types 1 / 0x1E / 0x1F | 1-2 per puff (`sub_80A4ACC`, `sub_80A9858`, `sub_80A9908`; ~47 cutscenes in `data/dat21-27`) | `asm29.s:181, 8651, 8752` |
| Falling object, map object type 0x2C | 1 per landing; random wait 8+(r&0x1F) frames at obj+0x24 (`sub_80AA282`) | `asm29.s:9876` |
| `sub_81209DC` | 1 secondary + 1 primary per call; called twice per battle by `sub_800B144` (results to 0x0203CCA8..AB), also `asm33.s:14690`, `asm36.s:4156`; purpose unknown | `asm32.s:36902` |
| `sub_804C818` | random 300-555 frame SFX timer | `asm03_2.s:10624` |
| asm36 `sub_8131C2C`..`sub_8131E70` | secondary 8-pass table shuffle to 0x0202522C; UI not identified | |
| `sub_8049D70` / `sub_803C40C` | 1 `GetRNG` stored at 0x02001800 / 0x02006770; link session token *(guess)* | |

**NPCs do not use the RNG** (`asm/npc.s` has no RNG calls; movement is scripted). A

---

## 2. Frame counters and the main loop

| Symbol | Address | Finding | Level |
|---|---|---|---|
| `iCurrFrame` | 0x0200A210 | +1 per main-loop pass (`main.s:26-30`); zeroed only by the power-on RAM clear (`start.s:39-42`); **not saved** (outside the save block), survives a soft reset. Reached only through `oToolkit_CurFramePtr` (no literal). A lag frame still adds 1. | R (segments write it at every junction) |
| game-frame counter | u16 at `*(u32*)0x020093D4` | The counter the route harness unwraps (`--game-frame-addr 0x20093D4`) | R |
| `main_awaitFrame` | 0x080003A0 | Entry = end of the frame's game logic: where pokes go in (harness `--poke-at`, `emu_server`) | R |
| Readers of `iCurrFrame` (gameplay) | | F1: encounter formation `sub_80AA5F4` (`(u16)iCurrFrame % eligible`). F2: mystery data `sub_8000E3A`. F3: text random branches `chatbox_F0_jump`. Everything else is blinking/animation (e.g. `battle_pickObjectAnimState_802BF0C`). | A, F1/F2 R |
| unlabeled block 0x08143416, `sub_814343A` | | Dead / no-op (picks from {5,5,5}) | A |

---

## 3. Save data

| Symbol | Address | Finding | Level |
|---|---|---|---|
| saved block | 0x02000000-0x0200670F | `sub_803F838` saves 0x6710 bytes: both seeds, mystery data, event flags, pack… | A |
| save/load | `asm03_1_1.s:13539` | Load restores both seeds as saved; `iCurrFrame` is not in the block | A; R (segment junctions must rewrite seeds, iCurrFrame, mystery data, event flags) |
| Save from the PET menu | | START, UP to Save, A, A on Yes, A: the game's own save routine writes a valid 32 KB save that CONTINUE loads | R (`segments.py` TEARDOWN) |
| Title → CONTINUE | | Power-on, idle 400 frames, START, idle 60, A, idle 120: 584 game frames to control | R (`segments.py` BRINGUP) |

---

## 4. Game state, map, player

### `eGameState` (0x02001B80)

| Field | Address | Finding | Level |
|---|---|---|---|
| +0x00 | 0x02001B80 | Subsystem: 0x04 overworld, 0x0C battle, 0x18 PET menu | R / P |
| +0x04/+0x05 MapGroup/MapNumber | 0x02001B84 | Internet maps have group ≥ 0x80 | R |
| +0x0C/+0x0D LastMapGroup/Number | 0x02001B8C | A reload sets them its own way; segments rewrite them | R |
| +0x0E CoordInteractionValue | 0x02001B8E | Trigger code of MegaMan's tile | P |
| +0x18 OverworldPlayerObjectPtr | 0x02001B98 | → `eOWPlayerObject` 0x02009F40 | R |
| +0x1C CurBattleDataPtr | 0x02001B9C | Encounter's battle settings, set on the trigger frame; writing another entry picks the formation | R |
| +0x24/+0x28/+0x2C PlayerX/Y/Z | 0x02001BA4/A8/AC | Where a map (re)load puts MegaMan, after a battle too; written to correct his post-battle position | R |

### `eOWPlayerObject` (0x02009F40, via +0x18 above)

| Field | Finding | Level |
|---|---|---|
| +0x10 | Facing | R |
| +0x1C/+0x20/+0x24 | X/Y/Z, s16.16 | R |
| +0x28/+0x2C/+0x30 | NextX/Y/Z (walk target; the game pulls him back towards it, so relocations write both) | R |

### `eStruct2001c04` (0x02001C04, 0x84 bytes)

| Field | Address | Finding | Level |
|---|---|---|---|
| +0x12 | 0x02001C16 | Walk counter; `>> 6`, max 16, picks the encounter odds. Writing 0x400 makes the next roll able to start a battle. | R |
| +0x24 | 0x02001C28 | SneakRun steps left (u32, counts down while MegaMan moves) | R |
| +0x28 | 0x02001C2C | LocEnemy steps left | R |

Subchip flags: SneakRun = event 0x170A (0x02001F69 & 0x20), LocEnemy = 0x170B (& 0x10). Flag set and counter 0 → `npc_80350BC` runs "<item> just expired!", pausing the map. R. Related code: `submenu_subchip_8124230`.

### Event flags (`eEventFlags`, 0x02001C88)

Flag f = byte `0x02001C88 + (f >> 3)`, mask `0x80 >> (f & 7)` (`TestEventFlag`). R

| Flag | Finding | Level |
|---|---|---|
| 0x1400 + n | Mystery data n taken (e.g. 0x1402/0x1403 at 0x02001F08 & 0x20/0x10); must be clear for it to appear | A / R (`try_gmd` "gone" variant) |
| 0x170A / 0x170B | SneakRun / LocEnemy on | R |
| 0x1709 | With bit 7 of a text `F0` opcode, skips the first random branch | A |
| 0x214 | Makes some formations eligible only 193/256 of the time (`sub_80AA78C`) | A |
| 0xBE0..0xBFF | Mr. Weather comp (§9) | A |

### Camera

| Symbol | Address | Finding | Level |
|---|---|---|---|
| `eCamera` +0x30/+0x34 | 0x020099B0/B4 | Camera X/Y following MegaMan (s16.16), copied at +0x3C/+0x40 | P *(guess)* |

---

## 5. Encounters

| Symbol | Address | Finding | Level |
|---|---|---|---|
| `sub_80AA4C0` | | Encounter yes/no, primary RNG, every 0x40 walk units | A |
| `pickBattleSettingsEntry_80AA5F4` | | Chooses one of the four root tables by flags, then the formation as `(u16)iCurrFrame % eligibleCount` | R (formations handler uses the game's own root lookup) |
| encounter root tables | 0x08020170, 0x08020178, 0x08020180, 0x08020188 | The four roots above | R |
| a map's random-encounter list | e.g. 0x080B20A0 | Per-map list; entries are battle settings (e.g. 0x080B1BEC, 0x080B1C4C, 0x080B4CAC matched video layouts) | R |
| scripted/other battles | 0x080AF2xx | Battles not from the map's list come from another table (game frame 126,043 used 0x080AF2A0) | P |
| `sub_80AA78C` | `asm29.s:10587` | Candidate filter: condition byte 5 + `EVENT_214` → eligible with p = 193/256 (1 primary call each), which changes the count. So force the pointer, not `iCurrFrame`. | A |
| `chooseRandomEncounterMaybe_80aa5e4`, `chatbox_804244C` | | Other callers of the formation pick (cutscene `start_random_battle`, a chatbox command) | A |

---

## 6. Battle

### Start: seed snapshot and restore (single player too)

| Symbol | Address | Finding | Level |
|---|---|---|---|
| `battle_initBattleStateBuffer_800B144` | `asm00_1.s:18021` | Phase 4: copies `ePrimaryRngSeed` into the packet at **0x0203CBE4** (`dword_203CBE0`+4), BattleSettings ptr at +8, navi stats…; magic 0x12345678 at 0x0203CBE0; 0x3E-word transfer (`sub_80200A4`). Calls `sub_81209DC(1)` twice (results at +0xC8..+0xCB = 0x0203CCA8..AB). | R |
| `sub_801FE6C`, `sub_801FFD6`, `sub_801FF18` | `asm01.s` | No-link loopback: one word per frame, highest index first; the seed arrives at **0x0203F4A4** about 61-62 frames after phase 4 | R |
| `battle_state_finishBattlePhase_80079d0` | `asm00_1.s:9899` | Phase 8: waits for 0x12345678 at 0x0203F4A0, then `battle_copyStructsIncludingBattleStats_800b2d8` sets the primary seed from 0x0203F4A4 (`asm00_1.s:18252-18255`); BattleState+2 becomes 0x0C | **R**: an RNG write during an intro must also write 0x0203CBE4 and 0x0203F4A4 |
| `battle_state_endPhaseWithEffect_80079A8` | `asm00_1.s:9868` | Phase 4 entry | A |

### Folder and hand

| Symbol | Address | Finding | Level |
|---|---|---|---|
| `shuffleFolder_800A570` | `asm00_1.s:16037` | Frame after the restore (`battle_RevealFieldColumnsStep_80079F0` → `sub_800A3E4`): secondary-RNG shuffle of the non-Giga chips, one `% (n-12)` per Giga chip, one `% 0x13` for the Tag pair (BattleState+0x45); result copied from 0x02033000 | P (the block changes exactly on that frame) |
| `eBattleFolder` | 0x0203CDB0 | 30 × u16 (id \| code<<9); 0xFFFF where a chip was used | R (`try_hand`) |
| saved folder | 0x02002178 | The folder the battle copies from | R |
| hand | | First N of `eBattleFolder`, N = 0x020364C6; no RNG when the custom screen opens (`sub_8026840`, `battle_compactFolderChipList_802945A`, `sub_8027E2C` → `battle_BuildFolderEntryRecords_8027EE8`; active chip slots 0x020365C0, 12 bytes each, `getLocOfActiveChips_8027E1C`) | R |
| Run (L) | | `sub_8026F1A` (`asm03_0.s:1467`): `t = (NaviStats+0x3E + 4)/5` vs `lvl = BattleSettings.Unk_01`; t > lvl always succeeds, else a row of `off_8027014` ({4,8,15},{8,12,15},{12,14,15}) indexed by attempt counter `dword_20349A8` (0x020349A8), 1 `GetRNGSecondary`, success if `(r&0xF) < threshold`; outcome at **0x020364C4** (2 escaped, 1 failed), read by `sub_8026FC0` | A |
| Shuffle-once button | | Active slot 8, type 4/5, enabled when NaviStats byte 0x60 == 1 (`battle_checkActiveSlot8NaviStatReady_802A5DA`, `battle_spawnMirrorFacingHelper_80280E0`); `sub_8028DD6` → `sub_80271F8`; `sub_8029788` then `sub_8029688` ×8 (secondary shuffles), `sub_802983C` writes back | A |

### Battle objects and state

| Symbol | Address | Finding | Level |
|---|---|---|---|
| BattleState | 0x02034880 | +0x02 phase; +0x08+side enemy counts; +0x1E busting level (0x0203489E); +0x36 reward 2 (0x020348B6); +0x40 battle time; +0x45 Tag position; +0x4C + 8·side enemy lists; +0x90 held-chip slots | R (lists/counts/phase), A (rest) |
| `eT1BattleObjects` | 0x0203A9B0 + n·0xD8 | +0x09 CurAction; +0x12/13 PanelX/Y; +0x14/15 FuturePanelX/Y; +0x16 Alliance (0 MegaMan's side, 1 enemies); +0x1A ChipsHeld; **+0x24 HP**; +0x26 MaxHP; +0x28 NameID; +0x2A held Chip; +0x58 AIData slot ptr. Object 0 = MegaMan (HP 0x0203A9D4). | R (HP, MaxHP, Alliance: kill/die fixes), A (rest) |
| `eActiveT1BattleObjectsBitfield` | 0x02034000 | One bit per live object, object 0 = bit 31; object memory keeps stale data after a battle, so check this bit | R |
| `eAIData` | 0x02034080, 8 × 0x100 | +0x00 ActorType, +0x01 AIIndex, +0x16 Version, +0x80 AIState (+0 decision state, +2 sub-state, +4/5/6 counters, +8 wait timer), +0xA0 AttackVars (+0 step, +0x0C/0D params/panel, +0x10 timer, +0x18 count/delay, +0x1C slot, +0x28 panel-picker callback, +0x2C target) | A |
| Battle end without a result screen | | Story battles end with "YOU WIN" / "ENEMY DELETED" and a fade to black; a series of battles ("BATTLE 6") stays in subsystem 0x0C with no live enemy between battles | R |

### Enemy spawn and held chips

| Symbol | Address | Finding | Level |
|---|---|---|---|
| `sub_80AA88C` (DB: `battle_rollRandomItemDrops_80AA88C`) | `asm29.s:10746` | Called at enemy spawn (`battle_SpawnEnemyObjects_8007358`, in `sub_80091F0` and copies): 1 `GetPositiveSignedRNG`; per slot of BattleState+0x90, 5 bits index the enemy's 16-hword table (`sub_80AED50(NameID)`); the last non-0xFFFF wins → object +0x2A chip, +0x1A = 1 | A; conflicts with the DB summary |
| held tables | 0x080AED74 | [actor type][AI index] → 0x20 bytes per version: chips a virus can hold (`sub_80AED50`) | R |
| enemy kinds | 0x080182C4 | 3 bytes per enemy id: version, actor type, AI index (`GetVerActorTyAndAIIdx_80182B4`); viruses in families of 6 ids from 1, navis from 0x100 | R |
| `spawnMysteryData_8007424` | `asm00_1.s:8976` | Battle-start mystery data: chance `GPSR % 15 < setup byte 2`; contents from `rng_pickRandomHalfwordFromTableEntry_80DA9FE`; rows at **0x080211A0** (8 u16 per row, row = setup byte 2 >> 4, index RNG & 0xE) | R (the table), A (the code) |

### Rewards

| Symbol | Address | Finding | Level |
|---|---|---|---|
| busting level | `sub_800AF84` → `sub_800AC20` | No RNG; stored at BattleState+0x1E and `eStruct200A008`+0 (0x0200A008) | A |
| `sub_800B428` → `sub_802C8FA(0x0203CBE4)` | `asm00_1.s:18381`, `asm03_0.s:13382` | First frame of the result link-sync state: fills a 12-byte record (+0 mode, +1 bust, +3 count, +4 time, +8 reward 1, +0xA reward 2) | A |
| reward 1 | `sub_80AA8E0` → `sub_80AA910` | `sub_80AAA3C` (1 GPSR % count → enemy id; table `byte_80AAEA8` + id·0x28, 20 u16, via `sub_80AAE98`), then `sub_80AAA98` (1 GPSR r: col = (r>>16)&0xF, pair = `byte_8020B9C[bust·16+col]`, +2 if (r>>8)&1, +0x14 if HP ≤ 3·(MaxHP>>3)) | A; the table **0x080AAEA8** R |
| `byte_8020B9C` | `data/dat01.s:200` | Pair offsets per busting level: 0-3 all 0; 4: 0/4; 5: 4/8; 6: 8; 7: 8/12; 8: 12; 9: 12 (col 15 → 16); 10: 12/16; 11: 16 | A |
| NaviStats+0x26 bits 0/1 | | Zenny-only (`sub_80AAB04`) / chip-only drop paths | A |
| reward 2 | BattleState+0x36 | Set by a battle object's death phase from its Param1 (`battle_objectLingerThenDestroyPhase_80DA974`). **In practice it carries a virus's held chip** (the run's reward 2 was a held chip, not a table reward). | A + P |
| packed reward u16 | `decodePackedIdFields_802C54C` | 0xFFFF nothing; `v>>14`: 0 chip (id v&0x1FF, code (v>>9)&0x1F), 1 zenny (v&0x3FFF), 2 HP (0 = full), 3 bug frags | R |
| result path | | Roll at 0x0203CBEC/EE (count 0x0203CBE7) → arrives 0x0203F4AC (a shared link buffer: noisy) → magic 0x1F2F3F4F, `sub_800B444` copies 12 bytes to 0x02035260 (**reward 1 at 0x02035268, reward 2 at 0x0203526A**) → `sub_802C34E` copies into the result screen queue 0x020364D4/D6 (count 0x020364CD) and **0x0200A00C/0E** (`eStruct200A008`+4/+6: the reward actually given) | R (0x02035268/A written a few frames before the screen copy, one per reward; 0x0200A00C read to check what was given) |
| shown / given | | `queue_popHalfword_802C8D4` → 0x020364D0 (shown); `eStruct200A008_redrawOnDirtyFlags_802CA82` → `sub_802CAA6` → `GiveChips`/`GiveZenny`/`GiveBugfrags` on leaving | A |
| reveal animation | `sub_802BF6C`, `initShuffledOrderTable42_802C1F0` | 84 primary calls (42-entry shuffle), cosmetic | A |

---

## 7. Enemy AI

| Symbol | Finding | Level |
|---|---|---|
| `t1_0x0_80B81EC` | Battle object type 0 branches on ActorType: 0 virus → `battleObject_dispatch_8108F50`, 1 navi → `sub_80F2330`, 2 player → `playerObject_main_80EA460` | A |
| `battle_8108F74` | Indexes by AIIndex·4: actions `off_8109050`, data `off_81090D0`, hooks `off_81091D0`, `off_8109250` | A |
| AI index → virus family (`off_81090D0`) | 1 Mettaur, 2 Piranha, 3 HeadyA, 4 Swordy, 5 KillrEye, 6 Quaker, 7 Catack, 8 Champy, 9 WindBox, A Trumpy, B OldStov, C HntdCndl, D Kettle, E Puffy, F StarFish, 10 ErthDrgn, 11 ScarCrow, 12 PulsBulb, 13 BigHat, 14 BombCorn, 15 Shrubby, 16 HonyBmbr, 17 Gunner, 18 FgtrPlne, 19 DarkMech, 1A SnakeArm, 1B Armadill, 1C Cragger, 1D Nghtmare, 1E Mettaur variant?, 1F Tuby (the virus code ends near `off_81166B0`) | A (`fam.py`, from `TextScriptVirusChipNames.s`) |
| AI index → navi (`off_80F23AC`, via `sub_80F2354`) | 1 HeatMan, 2 ElecMan, 3 SlashMan, 4 EraseMan, 5 ChargeMan, 6 SpoutMan, 7 TomahawkMan, 8 TenguMan, 9 GroundMan, A DustMan, B ProtoMan, C BlastMan, D DiveMan, E CircusMan, F JudgeMan, 10 ElementMan, 12 Colonel, 13 Bass, 14 Gregar, 15 Falzar (11/16/17/18 dummies) | A *(guess: sprite category − 0x2E)* |
| RNG use | Primary only; chance rolls (`GPSR & 0xF` or `& 1`, 1 call) and random panels (1 call, **0 when no panel is valid**). Pickers: `sub_8015C48`, `sub_8015C94`, `panelFlagCheck_8015D80`, `_GetRandomRelativePanelFiltered`, `battle_pickRandomRowObjectFields_810971A`, `object_getRandomPanelFromCurrentColumn` | A |
| per virus | Mettaur: normal loop `sub_810A004` has no RNG (flag state 8: `sub_810A254` &1, `sub_810A0EE` &0xF<2 → attack, else wait 50); Catack/Champy: none in the AI block; Piranha `sub_810A648` %3, `sub_810A724` panel; Swordy `sub_810B30C` &1, `sub_810B49C`/`B6F0` &1, `sub_810B65E` panel; KillerEye `sub_810BDF4`/`BE2A` &3; Quaker `sub_810C1F4` (r&7)·10 delay, `sub_810C2B2` panel; Puffy `sub_810EE64`/`EEDE`/`EF40`; Gunner `battle_initAttackFromPreset_8113162` row panel; no RNG: WindBox, Trumpy, StarFish, ScareCrow, FighterPlane | A |
| `sub_8109D08` | Generic virus move: writes FuturePanel on the roll frame and reserves it; teleports ~3 frames later | A |
| per navi | BlastMan `sub_80FD158` (after 3 moves, every 2nd attack `GPSR&0xF<8`), `sub_80FD376`, pickers `sub_80FDA62`/`DAC4`/`DB84`/`DBCC`; DiveMan `sub_80FDE04` (&0xF<6), `sub_80FE4D0`, `sub_80FE5EE`; CircusMan `sub_80FE8E4`, `sub_80FF528`, `sub_80FF614`; JudgeMan `sub_80FF904`, `sub_80FFFBC`, `sub_8100144`; ElementMan `sub_8100F5C`/`F8A`/`810102A`/`810112C`; Colonel `sub_81013AA`; Gregar `sub_8103CD0`, `sub_8104E98`..`sub_8105174`; Falzar `sub_81054D8`, `sub_8106418`, `sub_81064F4`, `sub_81065A8` (byte shuffle), `sub_8106718`; ProtoMan `sub_80FBA24`/`BA9A`/`BB34`/`BBCA` (and the misnamed `judgemanChoosePanel_80FCCBC`); SpoutMan `sub_80F7690`, `sub_80F8094`..`sub_80F8256`; TomahawkMan `sub_80F8CF8`..`sub_80F8DFE`; TenguMan `sub_80F9000`, `sub_80F90BA`, `sub_80F91A2`, `sub_80F993A`/`9992` | A |
| `chooseCornfiestaPanelXY_80DABC4` | A chip attack object (`GetRNG&0xF`), not navi AI | A |
| determinism | Same restored seed + same folder + same inputs = the whole battle replays (AI and drops) | R |

---

## 8. Mystery data, chips, inventory

| Symbol | Address | Finding | Level |
|---|---|---|---|
| green mystery data | 0x02004348 + 2·(id−0x1400) | Byte 0 placement, byte 1 contents; rolled at every jack-in (`mysteryData_processMapGroups_809F9DC` → `sub_809FA68` → `sub_8000E3A`, primary + `iCurrFrame&3`) and at New Game; saved. Examples: Central Area 1's two at 0x0200434C/0x0200434E; the run's 0x140B (600z) = 0x0106 at 0x0200435E. | R (`try_gmd`) |
| gold mystery data | 0x02000210 +2 slot, +3 entry, +4 map, +6 which green becomes gold | `sub_809FD70`, `sub_809FDE0` | A |
| blue / purple | | Fixed (one placement and contents each, `data/dat28.s:28-60`); real-world MD has no roll | A |
| `InternetMysteryDataMapGroupEntries` | 0x080A336C | {map group, maps}, ended by 1 | R |
| `ts_start_give_random_zenny` / `_chip_code` | | Not random: always the last entry | A |
| chip pack | 0x02002230 | One count byte per (chip id × 12 + code index), the index being the code's position among the chip's 4 ROM codes; stored plain | R |
| `ChipData` | 0x08021DA8 | 44 bytes per chip id, its 4 codes first | R |
| zenny | 0x020047CC (u32) | *(guess, watched by a probe)* | P |
| bug frags | 0x02006740 | Bug frag count | A |

---

## 9. Overworld objects and the Mr. Weather comp

### Object systems

| Symbol | Address | Finding | Level |
|---|---|---|---|
| `eOverworldNPCObjects` | 0x020057B0, 16 × 0xD8 | Active = header bit 0; X +0x24, Y +0x28 | P |
| `eOverworldMapObjects` | 0x02011EE0, 56 × 0x78 | Header byte 1 = type; +4 params (low byte = instance); X +0xC, Y +0x10 (s16.16) | P |
| `TryUpdateEachOverworldMapObject_80048D2` | | Skipped while NON_NPC_ANIMATION_LOCKED; calls `off_8004724[type]` | A |
| `off_8004724` | | Map object update table by type: 0x18 crow (`sub_80A8AB4`; its wobble state `sub_80A8CCC`), 0x1A `sub_80A8E74`, 0x1B `sub_80A92B8`, 0x1C `sub_80A9430`, 0x1D `sub_80A9658` | A |
| map object spawns | `InternetSpawnMapObjectJumptable` (`asm03_1_0.s:1765`) | Per internet area, e.g. `MrWeatherComp_SpawnMapObjectsForMap` (`maps/MrWeatherComp/loader.s`) → `SpawnObjectsFromList` | A |

### Mr. Weather comp

| Symbol | Finding | Level |
|---|---|---|
| `unk_2000310` (0x02000310, 0x60 bytes) → proposed `eWeatherCompState` | +0x02 reset-routine step; +0x06 rises every 6 frames as the rainbow-orb trail grows (0x08 → 0x10; the run's loss at 432736 is where it reaches 0x10); +0x0E trigger index; +0x0F/+0x10 MegaMan's animation/facing for the reset; +0x12/+0x14 reset point (u16); +0x19+i, +0x31+i per-tornado bytes (+0x31+i last direction); +0x38 (0x02000348, s16.16) a countdown, −1 per frame, copied at tornado +0x25, not what drives it; +0x40/+0x46 copies of tornado 0's home X/Y | P (+6, +0x38, +0x40/46), A (rest) |
| `sub_808FDC0` | Comp entry once (flag 0xBFB): zero the state; +4 = 0xFF, +0x20 = 0xFF, +8 = 1, +0x16 = 3; clear flags 0xBF4..+7, 0xBE9..+6, 0xBF2; set 0xBE0 | A |
| `getWeatherCompStatus_8081308` (existing DB) | Weather comp puzzle status byte | DB |
| `sub_80A8E74` (type 0x1A, tornado) | States via `off_80A8E88`: 0 `sub_80A8E9C` init (sprite 0x80/0x1C/0x6C, anim 0x18, home from `sub_80A90B0`; flag 0xBFB: caught → free); 4 `sub_80A8F08` active (0xBEF → state 8; else `sub_80A8F48` + `sub_80A91A4`); 8 `sub_80A8FA0` caught (if `sub_80A91D4`: anim 0x19, sound 0x160, state 0xC); 0xC `sub_80A8FE4` free when the animation ends | P (fields moved it as predicted), A |
| `sub_80A8F48` | +9 mode (0 moving, ≠0 paused); moving: +0x24 counts down, step `sub_80A9038`, blocked → `sub_80A90E4`; at 0: reload from +0x26, `sub_80A9004`, maybe pause (`sub_80A9150`) for +0x27 frames (+0x25 countdown) | P |
| `sub_80A9004` | Direction +5 = `random(8)`, changed if equal to the last (state +0x31+i); +0x28 += 1 | A |
| `sub_80A9038` | Step by +0x2C (orthogonal on screen) or +0x30 (diagonal) in direction 0..7 | A |
| `sub_80A908C` / `sub_80A916C` | Parameters from a table: speed +0x2C, +0x30 = speed/2, move period +0x26 (60), pause +0x27 (200), +7 | A |
| `sub_80A90B0` | Home X/Y from the table, Z = 0x40 | A |
| `sub_80A90CC` | Coordinate trigger 0xC0 = blocked | A |
| `sub_80A90E4` | New direction ≠ current; undo and retry while blocked | A |
| `sub_80A91A4` | Flag 0xBE4 set and MegaMan within 0x14 px → set flag 0xBE6 | A |
| `sub_80A91D4` | Caught flag `byte_80A91EC[i]` = 0xBE9/0xBEA/0xBEB | A |
| `sub_80A91F4` | Priority 1 while 0xBE6 | A |
| `sub_80A92B8` (type 0x1B, rainbow orbs *(probably)*) | State 4 `sub_80A931C`: 0xBE6 → white fade (state 8: `sub_80A9360`/`sub_80A937C`, table `byte_80A93C0`), then flag 0xBE7, free; 0xBE4 → free; bobbing `sub_80A93E8` | A; white fade seen at 432736 in run and video |
| `sub_80A9430` (type 0x1C) | `sub_80A95A8`: MegaMan within 12 px at the same Z → flag 0xBFC, sound 0x1A4, state 8; `sub_80A9614`/`sub_80A9628` set/test flag `byte_80A9648[i]` | A |
| `sub_8090004` / `sub_8090020` | Tile trigger 0x70–0x79 / 0x60–0x69 → index to +0xE | A |
| `sub_8090104` | Starts cutscene `byte_808F668` (0x7x) or `byte_808F67C` (0x6x); other branches `sub_80902DC`, `sub_808FF8C` unread | A |
| `byte_808F668` (`data/dat24.s:3089`) | Lock player, flag 0x1731, call `sub_808F810`, game-state byte 9 = 3 | A |
| `sub_808F810` | Put MegaMan at state +0x12/+0x14 (Z 0x40), anim +0xF, facing +0x10, flag 0xBE4 | A |
| `sub_80900C8` | Spawn map object 0x1D near MegaMan; flags 0xBE5, PET_DISABLED, 0x1739 | A |
| `cutscenescript_808F594` | Fade, free all map objects, flag 0x11F, warp (`unk_808F5C4`) | A |

### Cybercrow lesson (TenguMan scenario, Green Area)

| Symbol | Finding | Level |
|---|---|---|
| crow records | 0x02000D20, 8 × 0x14: +0 state, +2 timer 0x1E+(r&0x3F), +4 heading, +6 timer 2, +8/+0xC/+0x10 position | A |
| `sub_8143314` | Spawn into the first free slot (≤6): 4 primary calls (heading, two timers, angle `r&0xF0` at a fixed radius) | A |
| spawn cooldown | 0x0200101C, re-rolled via `sub_814343A`/`sub_8000E3A`, always 5 (2-8 calls burnt) | A |
| `sub_80A8CCC` (type 0x18) | Heading wobble on timer expiry: 2 calls + 1 via `sub_8143220`, heading ±(r%0x60)<<8 | A |
| `sub_8143406` | `r&3` at 0x0200101A, meaning unknown | A |
| `sub_808C378` | "Pecked" cutscene: ≥1 call per frame for 60 frames | A |

---

## 10. Menus, input, text

| Symbol | Address | Finding | Level |
|---|---|---|---|
| `eJoypad` +0x08..+0x0F | 0x0200A278 | Held-time counter per key, saturating at about 0x10 | P |
| `eJoypad` +0x13 | 0x0200A283 | Key-repeat phase, cycles 0..4 while a direction is held; repeats fire on the wrap. A reload resets it, so after a reload the same held key repeats a frame off. | **P, confirmed** (fixed segment 12) |
| `ePETMenuData` +4 | 0x0200DF24 | Selected main-menu entry while a submenu opens (NaviCust 3, E-Mail 4 in the run) | P |
| `ePETMenuData` +5 | 0x0200DF25 | Set while the PET menu or a submenu is open (`map_script.inc:406`) | A |
| `pet_initMenu_811EC10` | | Zeroes 0x18 bytes of `ePETMenuData` | A (DB) |
| `sub_8120CC8` | | Table lookup: `t[i]`, or `t[0]` if `t[i]` == 0xFF | A |
| `chatbox_F0_jump` | `chatbox.s:4288` | Text opcode `F0 nn w… t…`: `(iCurrFrame&3)+2` `GetRNG` calls, then `(r&0x3FF) % Σw`; nothing stored. 30 uses: Humor jokes (`CompText8738B24`/`873A528`), rumor NPCs (`CompText8784908`, `879073C`, `8792478`), Mr. Prog 2000z (`CompText87A3180`, `87A1144`, `87A15A0`, `879DA74`) | A |
| `sub_8042554` | `chatbox.s:5778` | Random item: dead code (its `FB` slot is 0) | A |

---

## 11. Traders and misc

| Symbol | Finding | Level |
|---|---|---|
| Chip trader | `chipTraderWeightedRandomMaybe_804BD7C` (weighted on `r&0x3FF`), `sub_804BE88`, `chipTraderGetRewardFromRedraw_804BF94`; lists at 0x02034800/0x02035800 | A |
| Bug Frag trader | `sub_809A520` → `sub_813D48C`; costs 10/20/30/50 (`dword_813D4FC`); tier table `byte_813D500` all zero; item rolls `sub_813D510`/`D568`/`D598` (`r&0xF`, code `r&3`) | A |
| Number Trader / lottery | `TextScriptLottery86C67E4`: deterministic | A |

---

## 12. Conflicts with recomp's symbol DB

| DB entry | DB says | Here | Action |
|---|---|---|---|
| `reqBBS_81409D0` | input % random byte | `(GetRNG() & 0xFF) % n` | fix the DB (assembly is unambiguous) |
| `battle_rollRandomItemDrops_80AA88C` | post-battle drop roll | held-chip roll at enemy spawn | re-read the caller |
| `damage_jitter_80AAA98` | per-hit damage jitter | reward table entry pick | re-read |
| `setupNaviDisplay_802C34E` | navi info screen | reward record → result screen queue | re-read (the RAM addresses are runtime-confirmed either way) |
| `sub_800B444` | restores two snapshots | copies the 12-byte result record 0x0203F4A4 → 0x02035260 | consistent; add the meaning |

## 13. Open

- Mr. Weather comp outcome at 432736: what empties the rainbow meter and moves the view when the orb trail reaches 0x10 (`sub_80902DC`, `sub_808FF8C`, the meter's code unread).
- `sub_81209DC`'s purpose; NaviStats+0x26 bits 0/1 and +0x60: which program sets them.
- Which battle-mode handler (`asm00_1.s` 13397/14015/14438/14892) ordinary random battles use; the result-sync latency in frames.
- `unk_2000310` +0x04, +0x08, +0x16, +0x20, +0x31+i.
