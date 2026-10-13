# Random decisions in BN6 Falzar (US): where they are made, and how to force them

This file is for vidsync, which replays a run from video and forces random outcomes with end-of-frame RAM writes.

Sources:
- the decomp at `recomp/bn6f`. File:line references are to `bn6f/asm/*.s` unless a path says otherwise.
- `tools/vidsync/rngfix.py`, `pt_table.py`, `pk2_search.py` in this repo, which already use the primary seed, the secondary seed and `iCurrFrame&3`.

**Status:** all of this comes from reading the code. None of it has been checked in the emulator yet. Statements marked *(guess)* are inferences.

## Summary: what a run hits, in order of frequency

| Event (how often) | RNG | Decided when | Cheapest forcing write |
|---|---|---|---|
| Battle seed (every battle) | primary, **snapshot then restore** | snapshot at battle phase 4; restored about 62 frames later at phase 8 | u32 **0x0203CBE4** during the intro, or u32 **0x020013F0** at the end of the restore frame. Without this write, setting 0x020013F0 during the intro has no effect. §1 |
| Folder order (every battle) | secondary | frame after the restore (`sub_800A570`) | 30 × u16 at **0x0203CDB0** (`eBattleFolder`) at the end of that frame. §1 |
| Hands on turns 2+ | **none** | custom-screen open | follows from the folder plus picks; nothing to force. §2 |
| Enemy held chip (every battle) | primary, 1 call | enemy spawn, after the restore (`sub_80AA88C`) | BattleObject+0x2A/+0x1A. §3 |
| Virus/navi AI (every battle) | primary only | each decision | covered by the battle seed if the inputs and the hand match; else AIState / CurAction / FuturePanel. §4-5 |
| Drop (every won battle) | primary, 2 calls | first frame of the result link-sync state | u16 **0x02035268** at one frame boundary, or u16 at **0x020364D4 + 0x0200A00C** during the busting-level display. §3 |
| Run from battle (L) | secondary, 1 call | L confirmed | **0x020364C4** = 2 (escaped) or 1 (failed). §2 |
| Encounter yes/no | primary | every 0x40 walk units | known (`sub_80AA4C0`) |
| Encounter formation | **`iCurrFrame`** | at the encounter | known: 0x02001B9C, or `iCurrFrame`. §7 |
| Green/gold mystery data | primary + `iCurrFrame&3` | every jack-in | known: 0x02004348..; gold at 0x02000210+2/3/4/6. §6 |
| Cybercrow lesson (TenguMan, probable any%) | primary, 4 calls per crow | each crow spawn | crow records at 0x02000D20. §6 |
| Random text branches | primary + `iCurrFrame&3` | text opcode `F0` | seed plus frame phase; mostly off-route. §6 |

## 0. The RNGs and how they advance

All three generators use the same step, `x' = (rotl(x,1) + 1) ^ 0x873CA9E5` (`asm/asm00_0.s:2751-2765`).

| Name | Address | Readers |
|---|---|---|
| primary `ePrimaryRngSeed` | 0x020013F0 | `GetRNG` (full 32 bits), `GetPositiveSignedRNG` (bit 31 cleared), `ShuffleByteList_PrimaryRNG`, `ShuffleHwordList_PrimaryRNG` (2 calls per swap) |
| secondary `eSecondaryRngSeed` | 0x02001120 | `GetRNGSecondary`, `GetPositiveSignedRNGSecondary`, `Shuffle*_SecondaryRNG` |
| third `dword_20018E8` | 0x020018E8 | only `dead_rng_800157C` (`asm00_0.s:2833`), which nothing calls. Ignore it. |

Every state lies on one orbit of this step, so "seed" and "position in the stream" mean the same thing. `tools/vidsync/pt_table.py` already uses this (POS1/POS2).

### Advances that happen every frame, not tied to a decision

- **Secondary, every frame in every mode:** `GetRNGSecondary` after the subsystem callback (`asm/main.s:43`).
- **Primary +1 and secondary +1 more, in the game-state subsystem** (overworld *and* battle, which runs inside it): `cbGameState_80050EC` (`asm00_1.s:4314-4315`).
- **Primary +1 per frame on the title screen:** `startscreen_render_802F544` (`asm03_0.s:19512`).
- **Primary +1 per frame in the PET submenu:** `SubMenuControl` (`asm32.s:34252`), skipped only in submenu states 0x18/0x1C.
- **Chip trader:** `ChipTraderControl` (`asm03_2.s:7061`) also calls `GetRNG` (see the overworld section).
- **No per-frame advance** in jack-in transitions (`ho_jackIn_80341B6`), `cb_8038AD0`, the shop, or email.
- `iCurrFrame` (0x0200A210) is incremented once per main-loop pass (`main.s:26-30`). A lag frame (two VBlanks for one step) still adds only 1.

Consequence: in the overworld and in battle, both seeds behave mostly like frame counters plus an offset. Our stream drifts from the runner's only through:

- (a) a different starting seed;
- (b) extra calls that depend on state we don't reproduce, such as NPC wandering, camera shakes, enemy AI, or text randomness;
- (c) frames spent in a different subsystem.

### Reseeds and restores (the "third RNG" question)

1. **Boot.** `main_` calls `SeedRNG` (`main.s:5`): primary = 0xA338244F (`asm00_0.s:2738`). The secondary is 0 after the boot RAM clear.
2. **Save/load.** Both seeds are inside the saved block 0x02000000-0x0200670F (`sub_803F838`, `asm03_1_1.s:13539`, size 0x6710).
   - Loading a save restores both seeds as they were at save time.
   - `SeedRNG` runs on load only if the checksum or timestamp check fails (`loc_803F870`, `asm03_1_1.s:13567`).
   - Load also calls `RandomizeExtraToolkitPointers` (1 `GetRNG`).
   - Saving calls `rollRngSeedIntoExtraToolkitPtrs_8006e26` (`asm00_1.s:7986`, ≥1 `GetRNG`) and `save_BackupToGamePakWithRetry_803FAB4` (`asm03_1_1.s:13949`, `GetRNG`).
   - `iCurrFrame` is **not** saved (0x0200A210 is outside the block). A soft reset does not reseed and does not clear `iCurrFrame`.
3. **Battle start: the primary seed is snapshotted and later restored.** This is the most important one; see section 1.

---

## 1. Battle start: the battle's primary seed

Single-player battles run through the link-cable packet code with a loopback.

**Phase 4: snapshot.** `battle_state_endPhaseWithEffect_80079A8` (`asm00_1.s:9868`) calls `sub_800B144` (`asm00_1.s:18021`).

- It copies `ePrimaryRngSeed` into the outgoing packet at **0x0203CBE4** (`dword_203CBE0+4`, line 18024-18026). It also writes the BattleSettings pointer to +8, the navi stats, and so on.
- It writes the magic 0x12345678 to 0x0203CBE0 and starts a 0x3E-word transfer with `sub_80200A4(0x3E)` (line ~18205).
- Right after the snapshot it calls `sub_81209DC(1)` (`asm32.s:36902`) **twice** (`asm00_1.s:18156`, `18162`). Each call makes 1 `GetPositiveSignedRNGSecondary` plus 1 `GetPositiveSignedRNG`, and the results go into packet bytes 0xC8-0xCB. Their purpose is unknown. The primary calls are undone by the restore below; the 2 secondary calls are not.

**The loopback.** With no link (`eStruct203F7D8[0]==0`), `sub_801FE6C` (`asm01.s`, called first thing in `battle_main_8007800`) copies the local record into both player slots. `sub_801FFD6` then stores `record.word` into `dword_203F4A0[idx]` and builds the next record from `dword_203CBE0[--count]`.

- One word moves per frame, highest index first.
- The seed (index 1) arrives at 0x0203F4A4 about 61-62 frames after phase 4. The magic (index 0) arrives one frame later.

**Phase 8: restore.** `battle_state_finishBattlePhase_80079d0` (`asm00_1.s:9899`) waits until `battle_doBothTrackedIdsMatch_800B46C(0x12345678)` is true. Then `battle_copyStructsIncludingBattleStats_800b2d8` sets `ePrimaryRngSeed = *(u32*)0x0203F4A4` (`asm00_1.s:18252-18255`), and the phase byte BattleState+0x02 becomes 0x0C.

**This applies in single player.** `dword_203F4A0` is written only by the transfer stores in `asm01.s` (`sub_801FF18`/`sub_801FFD6`). `battle_state_finishBattlePhase_80079d0` cannot advance until 0x0203F4A0 holds 0x12345678. Single-player battles do advance, so the loopback delivered the packet and the restore ran. `sub_801FE6C` (`asm01.s`, the `loc_801FED2` branch) is the no-link loopback: it copies `eStruct2036780` into both `unk_20399F0` and `unk_2039A00`.

So everything the primary RNG did during the roughly 62-frame battle intro is thrown away. The battle continues from the value captured at phase 4. Writing 0x020013F0 during the intro has no effect.

**Cheapest correct write to set the battle seed.** Any one of these works:

- Write `0x0203CBE4` (u32) at the end of any frame from the phase-4 frame until about 60 frames later.
- Write `0x020013F0` at the end of the frame in which BattleState(0x02034880)+0x02 first becomes 0x0C. Doing it at the restore frame also absorbs the one `GetRNG` that `cbGameState` makes after the battle callback in that same frame.

**Phase 0x0C: folder shuffle.** `battle_RevealFieldColumnsStep_80079F0` calls `sub_800A3E4` → `sub_800A570` (`asm00_1.s:16037`) on the frame after the restore. This is the known secondary-RNG folder shuffle:

- `ShuffleHwordList_SecondaryRNG` with n swaps over the non-Giga chips;
- one `GetPositiveSignedRNGSecondary % (n-12)` per Giga chip, for its insertion index;
- one `% 0x13` for the Tag pair position, stored in BattleState+0x45.

Its result is `eBattleFolder` = **0x0203CDB0**, 30 × u16 (chip id | code<<9). It is copied from the work buffer at 0x02033000 at the end (`asm00_1.s:16165-16170`).

**Cheapest forcing of the folder:** write the 60 bytes at 0x0203CDB0 at the end of the shuffle frame. This is cheaper and more robust than hitting the secondary seed, because camera shakes and other secondary users before battle make the secondary position hard to predict.

**One write per battle.** Battle AI, rewards and every other in-battle decision use the primary RNG; see sections 3 and 4. So if the battle seed and the folder both match the runner's, and inputs are frame-identical, the whole battle replays. That includes the drop.

---

## 2. Custom screen: hands, Run and the shuffle button

### The hand: no RNG

- The hand is the first N chips of `eBattleFolder` (0x0203CDB0), where N is the hand size in S20364C0+6 (0x020364C6).
- Custom-screen init is `sub_8026840` (`asm03_0.s:526`). It calls:
  - `battle_compactFolderChipList_802945A`, which removes the chips used last turn (0xFFFF) and keeps the order;
  - `sub_8027E2C` (`asm03_0.s:3461`) → `battle_BuildFolderEntryRecords_8027EE8`, which points the active-chip slots (0x020365C0, 12 bytes each, `getLocOfActiveChips_8027E1C`) at the first entries of the folder.
- Unchosen chips stay at the top in the same order.
- No RNG is called anywhere in the custom-screen open/deal path. The only RNG users in `asm03_0.s` 0x8026xxx-0x8029xxx are the two below.
- So after the battle-start shuffle, every later hand follows from the folder order plus the player's picks.
- BN6 has no "Add" button.

### "Run" (L button on the custom screen)

- **Path.** `custMenu` input 0x200 sets state 0x1C (`asm03_0.s:5481-5492`) → `battle_updateObjectByKind_8026E98` → `sub_8026F1A` (`asm03_0.s:1467`).
- **Roll.** With `t = (NaviStats+0x3E (hword) + 4) / 5` and `lvl = BattleSettings.Unk_01` (`GetBattleSettingsUnk01`):
  - if `t > lvl`, the run always succeeds and no RNG is used;
  - otherwise it picks a row of `off_8027014` (`asm03_0.s:1600`): {4,8,15}, {8,12,15} or {12,14,15}, depending on lvl vs 1.5·t and 2·t;
  - indexes that row with the attempt counter `dword_20349A8` (0x020349A8, post-incremented; zeroed at the first custom screen of the battle by `sub_8026840`);
  - calls `GetRNGSecondary` once (`asm03_0.s:1515`). Success if `(rng & 0xF) < threshold`.
- **Outcome.** Stored in **S20364C0+4 = 0x020364C4**: 2 = escaped, 1 = failed. `sub_8026FC0` reads it a few frames later to choose the "got away" (1) or "couldn't" (2) text (`asm03_0.s:1556-1570`).
- **Cheapest write.** Set 0x020364C4 = 2 or 1 at the end of the frame the L-press roll ran, before `sub_8026FC0`.

### "CustmScr Shuffle chp once" button

This is active-chip slot 8 with type 4/5. It is enabled when battle NaviStats byte 0x60 == 1 (`battle_checkActiveSlot8NaviStatReady_802A5DA`, `asm03_0.s:9150-9165`; slot set up by `battle_spawnMirrorFacingHelper_80280E0`, `asm03_0.s:3887`). The help text is `CompText873ECC8_unk14`, "CustmScr / Shuffle / chp once".

- Pressing it → `sub_8028DD6` → state 0x28 → `sub_80271F8` (`asm03_0.s:1855`).
- **Frame 1:** `sub_8029788` (`asm03_0.s:7138`) runs 1-2 `ShuffleHwordList_SecondaryRNG` over the undrawn and unchosen chips.
- **Then:** `sub_8029688` (`asm03_0.s:6992`, one more secondary shuffle of n chips) every 4 frames, 8 times.
- **Finally:** `sub_802983C` writes the result back into `eBattleFolder`.
- **Force:** write `eBattleFolder` after the animation ends.
- Probably not used in any%.

---

## 3. End-of-battle rewards (drop, zenny, bug frags)

### Enemy held chip: 1 primary call at enemy spawn

`battle_SpawnEnemyObjects_8007358` (`asm00_1.s:8839`) calls `sub_80AA88C` (`asm29.s:10746`). This happens in the battle-mode state `sub_80091F0` (`asm00_1.s:13112`, plus copies at 13780/14267/14676), after the seed restore. So the battle seed covers it.

- It makes **one** `GetPositiveSignedRNG`.
- For each of the 4 slots in BattleState+0x90, it takes 5 bits `(r>>4)&0x1E`, `(r>>9)&0x1E` and so on, and uses them to index that enemy's 16-hword table (`sub_80AED50(obj+0x28 NameID)`).
- The last slot whose entry is not 0xFFFF wins. That object gets **BattleObject+0x2A = chip** (field `Chip`) and **+0x1A = 1** (`ChipsHeld`).
- *(guess)* This is the "virus holding a chip" mechanic, used when the virus is deleted.
- **To force it:** write those two fields on the spawned object (battle objects at 0x0203A9B0 + n*0xD8, per `ewram.s:2974`) at the end of the spawn frame.

### Busting level: no RNG

At battle end, `sub_80097CC`/`sub_800980E`-style states (`asm00_1.s:13397-13453`, with three more copies at 14015, 14438, 14892 for other battle modes) do the following:

- call `sub_800AF84` → `sub_800AC20`;
- store the busting level in **BattleState+0x1E (0x0203489E)** and in `eStruct200A008+0` (0x0200A008).

### Roll: 2 primary calls in the normal case

The roll happens on the **first frame of the result link-sync state**. For example, `sub_80098D8` (`asm00_1.s:14102`) with BattleState+0x03==0 calls `sub_800B428` (`asm00_1.s:18381`) → `sub_802C8FA(0x0203CBE4)` (`asm03_0.s:13382`). `sub_802C8FA` fills a 12-byte result record:

```
+0 u8  mode (0 single player)       +1 u8 busting level (BattleState+0x1E)
+3 u8  number of non-zero rewards   +4 u32 battle time (BattleState+0x40)
+8 u16 reward 1                     +A u16 reward 2
```

**Reward 1** = `sub_80AA8E0(enemyList=BattleState+0x4C+8*side, count=BattleState[8+side], bust, HP, MaxHP, alliance)` (`asm29.s:10817`) → `sub_80AA910` (`asm29.s:10845`).

Default path (battle NaviStats byte 0x26 has bits 0 and 1 clear):

1. **Which enemy's table:** `sub_80AAA3C(mode 0)` (`asm29.s:11004`) does 1 `GetPositiveSignedRNG % count`. Its result is `enemyId = list[i]`; the table is `byte_80AAEA8 + enemyId*0x28` (`sub_80AAE98`, 20 × u16).
2. **Which entry:** `sub_80AAA98(mode 0)` (`asm29.s:11055`) does 1 `GetPositiveSignedRNG = r`:
   - `col = (r>>16) & 0xF`;
   - `pairOff = byte_8020B9C[bust*16 + col]` (`data/dat01.s:200`);
   - `+2` if `(r>>8) & 1`;
   - `+0x14` (the second 10 entries) if `HP <= 3*(MaxHP>>3)`;
   - reward = `table[byteOffset/2]`.
   - Decoded `byte_8020B9C` (byte offsets 0/4/8/12/16 = entry pairs {0,1} to {8,9}), one row per busting level:

     ```
     bust 0-3 : all 0
     bust 4   : cols 0-7 → 0, cols 8-15 → 4
     bust 5   : 0-7 → 4, 8-15 → 8
     bust 6   : all 8
     bust 7   : 0-7 → 8, 8-15 → 12
     bust 8   : all 12
     bust 9   : 12 except col 15 → 16
     bust 10  : 0-7 → 12, 8-15 → 16
     bust 11  : all 16
     ```

Other paths (`asm29.s:10930-11000`): bit 0 of NaviStats+0x26 picks only zenny-type entries (`sub_80AAB04`). Bit 1 picks only chip-type entries with a search over neighbouring busting rows. Each uses 2-3 calls. They need a special navi stat, so they are unlikely in any%.

**Reward 2** = `BattleState+0x36` (u16, 0x020348B6), not random. A battle object's death phase sets it from its Param1 (`battle_objectLingerThenDestroyPhase_80DA974`, `asm31.s:73307`). It is 0 in ordinary virus battles.

**Packed reward u16** (`decodePackedIdFields_802C54C`, `asm03_0.s:12889`):

| Value | Meaning |
|---|---|
| 0xFFFF | nothing |
| `v>>14 == 0` | chip: id = v & 0x1FF, code = (v>>9) & 0x1F |
| `== 1` | zenny, amount = v & 0x3FFF |
| `== 2` | HP recovery (0 = full HP; `sub_802C170`) |
| `== 3` | bug frags, amount = v & 0x3FFF |

### Where the result travels

| Frame | What happens | Address |
|---|---|---|
| R | roll | 0x0203CBEC (r1), 0x0203CBEE (r2), count 0x0203CBE7 |
| R+1 | word index 3 (the rewards) is sent | |
| R+2 | it arrives | 0x0203F4AC |
| R+4..5 | magic 0x1F2F3F4F arrives; `sub_800B444` copies 12 bytes 0x0203F4A4 → **0x02035260** (`asm00_1.s:18400`) | r1 at **0x02035268** |
| next frame | `sub_802C34E(0x02035260)` (`asm03_0.s:12617`) copies it into the result screen | S20364C0+0x14/+0x16 = **0x020364D4/0x020364D6** (display queue, count in 0x020364CD) and **0x0200A00C/0x0200A00E** (`eStruct200A008` Unk04/06: the reward actually *given*) |

- **Shown:** `queue_popHalfword_802C8D4` puts the displayed reward into S20364C0+0x10 (0x020364D0).
- **Reveal animation:** `sub_802BF6C` / `initShuffledOrderTable42_802C1F0` (`asm03_0.s:12061`, `12414`) shuffle a 42-entry tile-reveal order with `ShuffleByteList_PrimaryRNG(…, 42, 42)` = **84 primary calls**. This is cosmetic but shifts the stream.
- **Given:** on leaving the battle, `eStruct200A008_redrawOnDirtyFlags_802CA82` (`asm00_1.s:10460`) → `sub_802CAA6` → `GiveChips` / `GiveZenny` / `GiveBugfrags`.

**Cheapest correct write:**

- Either the u16 at **0x02035268**, at the end of the frame `sub_800B444` ran (BattleState+0x02 has just become 4), before `sub_802C34E`.
- Or, any time after `sub_802C34E` and before the reveal pops the queue (the busting-level and time display, more than a second): write the same u16 to **both 0x020364D4 and 0x0200A00C**. The first sets what is shown, the second what is given.
- Keep the count bytes (0x020364CD, record +3) consistent. They only change if you turn a reward into 0x0000.
- The alternative is to set the primary seed before frame R so that the two calls produce the runner's pair. That is pointless if the battle seed already matched.

## 4. Virus AI

### How AI code is dispatched

- Battle object type 0 (`t1_0x0_80B81EC`, `asm31.s:4-21`) branches on `AIData.ActorType`:
  - 0 = virus → `battleObject_dispatch_8108F50` (`asm31.s:169529`)
  - 1 = navi → `sub_80F2330`
  - 2 = player → `playerObject_main_80EA460`
- `GetVerActorTyAndAIIdx_80182B4` (`asm00_2.s:20411`) maps an enemy ID to (version, actor type, AI index).
  - Viruses come in families of 6 IDs (versions 0-5), starting at ID 1. Navis start at 0x100.
- `battle_8108F74` (`asm31.s:169552`) indexes four tables by AIIndex*4:
  - action table `off_8109050` (`asm31.s:169658`)
  - data `off_81090D0`
  - per-frame hooks `off_81091D0` and `off_8109250`

### RNG use

**All enemy AI uses the primary RNG only.** No AI code calls the secondary RNG or reads `iCurrFrame`. Two call patterns cover nearly every site:

- **Chance roll:** `GPSR & 0xF` against a threshold, or `& 1`. Always exactly 1 call.
- **Random panel:** collect the valid panels, then 1 call `GPSR % count`. **0 calls when no panel is valid**, so the call count depends on the board, including where MegaMan stands.
  - Shared pickers: `sub_8015C48`, `sub_8015C94` (`asm00_2.s:15641/15679`), `panelFlagCheck_8015D80`, `_GetRandomRelativePanelFiltered` (`asm00_2.s:15942`), `battle_pickRandomRowObjectFields_810971A` (`asm31.s:170313`), `object_getRandomPanelFromCurrentColumn` (`object.s:3158`).

Many of the viruses on the original checklist do not exist in BN6: Canodumb, Spikey, Bunny, Billy, Fishy, Mushy, Lark, HardHead, Volgear, Ratty, Cactikil, Powie, Elebee, Fanfan and Gaia are absent from `data/textscript/TextScriptVirusChipNames.s`. Per family:

| AI idx | Family | RNG |
|---|---|---|
| 0x01 | Mettaur | The normal loop (`sub_810A004`, `asm31.s:171421`: walk to the player's row, then attack) has **no RNG**. The order in which Mettaurs take turns comes from a list. Only in "flag state 8" (collision flags & 0xA000, meaning unknown): `sub_810A254` (`asm31.s:171772`) does 1×`&1` (try up or down first), and `sub_810A0EE` (`asm31.s:171566`) does 1×`&0xF`, where <2 means attack now and otherwise it waits 50 frames. |
| 0x07 / 0x08 | Catack / Champy | no RNG in the AI block (their attack objects were not audited) |
| 0x0E | Puffy | chance plus random panel (`sub_810EE64`/`EEDE`/`EF40`, `asm32.s:2478-2582`) |
| 0x17 | Gunner | `battle_initAttackFromPreset_8113162` (`asm32.s:10259`): 1 random panel in its row at each attack start |
| 0x02 | Piranha | `sub_810A648` (`asm31.s:172188`) `%3` attack or wait; `sub_810A724` random panel in its column |
| 0x04 | Swordy | `sub_810B30C` (`asm31.s:173696`) `&1` direction; `sub_810B49C`/`B6F0` `&1`; `sub_810B65E` random panel |
| 0x05 | KillerEye | `sub_810BDF4`/`BE2A` (`asm31.s:175033/175063`) `GetRNG&3` |
| 0x06 | Quaker | `sub_810C1F4` (`asm31.s:175447`) `(r&7)*10` delay *(guess)*; `sub_810C2B2` random panel |
| others with RNG | HeadyA 3, OldStove B, HauntedCandle C, Kettle D, EarthDragon 10, PulseBulb 12, BigHat 13, BombCorn 14, Shrubby 15, HoneyBomber 16, DarkMech 19, SnakeArm 1A, Armadill 1B, Cragger 1C, Nightmare 1D | |
| no RNG | WindBox 9, Trumpy A, StarFish F, ScareCrow 11, FighterPlane 18 | |

### Player actions also draw from the primary RNG, so they shift every later enemy decision

- **Every hit spark:** `object_spawnHiteffect` (`object.s:6142`) and `object_spawnCollisionEffect` (`asm00_2.s:22005`) call `AddRandomVarianceToTwoCoords`, which is 1 `GetRNG` (`asm00_2.s:26005`, 24 call sites).
- **Every MegaMan charge shot:** `megamanChargeShotBPwrAtk_init_8011A26` → `sub_8013D5E` makes 1 `GetRNG` (`asm00_2.s:11461`), plus 1 GPSR if navi stat 0x4D != 0. Also `busterBugChargeShotDamageCalcHappensHere_8011A7E` (`asm00_2.s:6208`).
- **NaviCust bug effects (stat-gated, normally off in any%):**
  - `sub_8013DA0`: emotion bug, 1 call every 60 frames
  - `sub_8013E58`: custom bug, `GetRNG`
  - `sub_8013CC4`: panel bug
  - `battle_RollNaviStatChance_8013FAE`
- **Every frame:** `cbGameState` adds 1 `GetRNG`, paused or not.

### Determinism

Given the restored battle seed, the same folder order (the hands follow from it) and frame-identical inputs, the whole battle replays: AI, drops, everything.

- Without the folder match it does not, because different chips mean different hit sparks and different panels.
- Camera shakes (`camera_doShakeEffect_80301e8`, `asm03_0.s:21282`) are cosmetic but consume the secondary RNG. They affect only the *next* battle's folder shuffle, which is one more reason to write the folder directly.

### Where AI decisions live (for forcing a single action without the seed)

**AIData pool** `eAIData`: 0x02034080, 8 × 0x100 bytes (`ewram.s:2564`). Each object's slot pointer is at BattleObject+0x58.

| Offset | Field |
|---|---|
| +0x00 | ActorType |
| +0x01 | AIIndex |
| +0x16 | Version |
| +0x80 | AIState: +0 decision state (for example Mettaur `off_8109FF0`, `asm31.s:171412`), +2 sub-state, +4/5/6 move and attack counters, +8 wait timer |
| +0xA0 | AttackVars: +0 attack step, +0x0C/0D chosen parameters/panel, +0x10 timer, +0x18 count/delay, +0x1C attack slot, +0x28 panel-picker callback, +0x2C target |

**Battle objects:** 0x0203A9B0 + n*0xD8 (`eT1BattleObjects`, `ewram.s:2974`).

| Offset | Field |
|---|---|
| +0x09 | CurAction (set by `object_setAttack0`, `asm00_2.s:5787`) |
| +0x12/0x13 | PanelX/Y |
| +0x14/0x15 | FuturePanelX/Y |
| +0x24 | HP |
| +0x28 | NameID |

**Example: the generic virus move** `sub_8109D08` (`asm31.s:170966`) writes the chosen panel to FuturePanel on the same frame as the roll and reserves that panel. The teleport happens about 3 frames later.

- To force it, overwrite FuturePanelX/Y at the end of the roll frame.
- *(guess)* You probably also need to move the panel reservation.
- The seed write is simpler and safer.

## 5. Navi / boss battles (scripted battles)

Boss battles use the same machinery: primary RNG only; the battle seed snapshot and restore; the folder shuffle; and the normal reward roll at the end.

- **Dispatch:** `sub_80F2354` (`asm31.s:123502`) uses the action table `off_80F23AC` (`asm31.s:123546`).
- **Naming:** navi AI index → name is inferred as sprite category − 0x2E (`constants/enums/sprite_categories.inc:57-79`) *(guess, mostly consistent)*.
- **Typical decision:** counters in AIState+4/5/6, then a chance roll, then `object_setAttack0(n)`, which also stores a panel-picker callback in AttackVars+0x28.
  - Example: BlastMan `sub_80FD158` (`asm31.s:144934`). After 3 moves, every second attack decision rolls `GPSR&0xF < 8` to choose between two attacks.

| idx | Navi *(guess)* | RNG sites (`asm31.s` unless noted) |
|---|---|---|
| 0xC | BlastMan | `sub_80FD158` 144949, `sub_80FD376` 145218; panel pickers `sub_80FDA62`/`DAC4`/`DB84`/`DBCC` 146123-146345 |
| 0xD | DiveMan | `sub_80FDE04` 146521 (`&0xF<6`), `sub_80FE4D0` 147523/147533, `sub_80FE5EE` 147692 |
| 0xE | CircusMan | `sub_80FE8E4` 147893/147914, `sub_80FF528` 149615, `sub_80FF614` 149669 |
| 0xF | JudgeMan | `sub_80FF904` 149941/149951, `sub_80FFFBC` 150913, `sub_8100144` 151090 |
| 0x10 | ElementMan | `sub_8100F5C`/`F8A`/`810102A`/`810112C` 152916-153177 |
| 0x12 | Colonel | `sub_81013AA` 153436 |
| 0x14 | Gregar | `sub_8103CD0` 158775, `sub_8104E98`..`sub_8105174` |
| 0x15 | Falzar | `sub_81054D8` 161852, `sub_8106418`, `sub_81064F4`, `sub_81065A8` (byte shuffle) 164131, `sub_8106718` 164274 |
| 0xB | ProtoMan | `sub_80FBA24`/`BA9A`/`BB34`/`BBCA` 141968-142242. The misnamed `judgemanChoosePanel_80FCCBC` (144402/144464) is in this block. |
| 6 / 7 / 8 | SpoutMan / TomahawkMan / TenguMan | `sub_80F7690` 133500/133509, `sub_80F8094`..`sub_80F8256`; `sub_80F8CF8`..`sub_80F8DFE`; `sub_80F9000` 136732/136754, `sub_80F90BA`, `sub_80F91A2`, `sub_80F993A`/`9992` |

- `chooseCornfiestaPanelXY_80DABC4` (`asm31.s:73618`) is a chip attack object (`GetRNG&0xF`), not navi AI.
- Battle-start mystery data in a battle's enemy setup is `spawnMysteryData_8007424` (`asm00_1.s:8976`): the chance is `GPSR % 15 < setup byte 2`, and the contents come from `rng_pickRandomHalfwordFromTableEntry_80DA9FE`. It runs after the restore, so the battle seed covers it.

## 6. Overworld and other non-battle randomness

### NPCs do not use the RNG

`asm/npc.s` has no RNG calls, and there is no random-walk NPC command. Movement is scripted (`NPCCommand_move_in_direction`, `init_movement`, `change_movement_direction`). NPCs on screen therefore do not shift the encounter stream.

What does consume the RNG in the overworld is **map objects** (`asm29.s`) and a few native callbacks.

### Hidden consumers that shift the primary stream (no decision, but they move the position)

| Consumer | Calls |
|---|---|
| Every map load: `RandomizeExtraToolkitPointers` (`asm00_1.s:7664-7674`, from `EnterMap` `asm00_1.s:4386`, battle start `sub_800531C` 4581, and game states 0x14/0x1C/0x20/0x24/0x28/0x30, `asm00_1.s:4703-4959`) | 1 `GetRNG` per load |
| New Game: `encryption_initAll_8006d00` (`asm00_1.s:7802-7893`, loop with retries) + `anticheat_modCardsInit_8007036` (8368), from `initNewGameData_8004DF0` | about 0x300 or more, once |
| Each save: `save_BackupToGamePakWithRetry_803FAB4` (`asm03_1_1.s:13907-13949`) + `rollRngSeedIntoExtraToolkitPtrs_8006e26` | about 97 |
| Shop item objects: `storeObfuscatedRandomToken_804733C` (`asm03_2.s:2305`, called 5 times in `ShopControl`) | ≥1 per item; only if the route uses a shop |
| Story explosion emitters, map object types 1 / 0x1E / 0x1F (`sub_80A4ACC` `asm29.s:181`, `sub_80A9858` 8651, `sub_80A9908` 8752; spawned by about 47 cutscenes in `data/dat21-27`) | 1-2 per puff, every few frames while alive; cosmetic |
| Falling object, type 0x2C (`sub_80AA282`, `asm29.s:9876`; `dat26.s:450-458, 604-614`) | 1 per landing; random wait of 8+(r&0x1F) frames at obj+0x24, which affects timing |
| `reqBBS_81409D0` (misnamed; a generic `(GetRNG&0xFF)%n` helper) used by map object types 5 (`asm29.s:1193`), 0x11 (`asm29.s:6130`, JudgeMan "trial" cutscene `sub_808CBD8`, `asm24.s:1093`) and 0x1A (`asm29.s:7537/7678`, re-roll until different) | 1 or more |

The explosions and falling objects are cosmetic, but they are still on the route and consume the stream during cutscenes. They need no forcing as long as the stream matches. If you resync the seed, do it **after** the cutscene.

### Mystery data

- **Green data is the known item.** It is rolled at every jack-in by `sub_809F9DC` → `sub_809FA68` (`asm28_1.s:116,148`) through `sub_8000E3A` (primary RNG plus `iCurrFrame&3`). The result is 0x02004348 + 2*(id−0x1400): byte0 = placement, byte1 = contents.
  - `sub_809F9DC` also runs at New Game.
  - The result is inside the save area, so it persists.
- **Gold data** is part of the same roll.
  - `sub_809FD70` (`asm28_1.s:589`) sets 0x02000210: +3 entry, +4 map, +2 slot.
  - `sub_809FDE0` (`asm28_1.s:653`) sets which green slot becomes gold, at 0x02000210+6.
  - Force all of these together after the jack-in frame.
- **Blue (type 1) and purple (type 3) data are fixed.** `sub_809F9DC` rolls only type-5 (green) entries (`asm28_1.s:31-32`). Each blue or purple entry has one placement and one contents entry (`data/dat28.s:28-60`). Real-world mystery data has no roll (`RealWorldMysteryDataMapGroupEntries` is empty, `dat28.s:2287-2290`).
- **Not random:** the text commands `ts_start_give_random_zenny` and `_chip_code` always take the last entry (`include/bytecode/text_script_commands.inc:1197-1206`).

### Cybercrow lesson (TenguMan scenario, Green Area; probably on the Falzar any% route *(route claim from memory)*)

The game state is 8 crow records of 0x14 bytes at **0x02000D20**:

| Offset | Field |
|---|---|
| +0 | state |
| +2 | timer, 0x1E+(r&0x3F) |
| +4 | heading |
| +6 | timer 2 |
| +8, +0xC, +0x10 | position |

- **Spawning:** `sub_8143314` (`asm37_1.s:2899`) fills the first free slot (up to 6). It makes **4 primary calls**: heading, the two timers, and the spawn angle `r&0xF0` at a fixed radius from the player.
- **Spawn cooldown:** 0x0200101C. It is re-rolled through `sub_814343A`/`sub_8000E3A`, but the result is always 5. The roll still burns 2-8 calls.
- **Heading wobble:** map object type 0x18, `sub_80A8CCC` (`asm29.s:7151`). When the crow's timer runs out (state 2), it makes 2 calls plus 1 via `sub_8143220`, and sets heading += ±(r%0x60)<<8.
- **Other fields:** `sub_8143406` (`asm37_1.s:3048`) stores `r&3` at 0x0200101A at the start. Its meaning is unknown.
- **Caught:** the `sub_808C378` cutscene (`asm24.s:421`, "pecked") makes ≥1 call per frame for 60 frames (rejection loop), then scatters the crows. This only happens if the runner is caught.
- **Cheapest force:** keep the seed in sync on entering the area. If the crows diverge, write the crow records (position and heading) directly.

### Traders, lottery, misc

| Thing | RNG | Notes |
|---|---|---|
| Chip trader | `ChipTraderControl` 1 `GetRNG` per frame (`asm03_2.s:7061`); `chipTraderWeightedRandomMaybe_804BD7C` (9181, weighted on `r&0x3FF`), `sub_804BE88` (9333), `chipTraderGetRewardFromRedraw_804BF94` (9490) on lists at 0x02034800 / 0x02035800 | Not on the any% route. Output stays in registers; its storage was not traced. |
| Bug Frag trader | NPC callback `sub_809A520` (`asm28_0.s:1635`) → `sub_813D48C` (`asm37_0.s:3604`); item rolls `sub_813D510`/`D568`/`D598` (`r&0xF` vs thresholds, code `r&3`) | Tier table `byte_813D500` is all zero, so the tier is fixed. Not any%. |
| Number Trader / lottery | none (`TextScriptLottery86C67E4`, `asm28_0.s:2473`) | deterministic |
| `sub_8049D70` / `sub_803C40C` | 1 `GetRNG` stored at 0x02001800 / 0x02006770 | link session token *(guess)*; not any% |
| `sub_804C818` (`asm03_2.s:10624`) | random 300-555 frame SFX timer | cosmetic |
| asm36 `sub_8131C2C`..`sub_8131E70` | secondary only (8-pass table shuffle to 0x0202522C) | UI not identified |
| `sub_81209DC` | 1 secondary + 1 primary per call | called twice per battle from `sub_800B144` (§1); also from comm code (`asm33.s:14690`, `asm36.s:4156`) |

### Random branches in text (`chatbox_F0_jump`, `chatbox.s:4288`)

- The opcode `F0 nn w… t…` is run by `chatbox_onUpdate` in any mode.
- It calls `GetRNG` `(iCurrFrame&3)+2` times, then picks `(r&0x3FF) % Σw` cumulatively.
- If bit 7 of nn is set and `EVENT_1709` is set, the first target is skipped.
- The outcome is the next script and nothing is stored, so to force it, write the seed and keep `iCurrFrame&3`.
- **Uses found (30):**
  - MegaMan's Humor-program jokes (`CompText8738B24`/`873A528`; only with Humor equipped, `asm28_0.s:450-472`)
  - rumor NPCs when talked to again (`CompText8784908`, `879073C`, `8792478`)
  - the 50/50 Mr. Prog 2000-zenny gift (`CompText87A3180`, `87A1144`, `87A15A0`, `879DA74`)
  - None of these should be on an any% route.
- `sub_8042554` (`chatbox.s:5778`, random item) is dead code: its `FB` command-table slot is 0.

### Encounter-formation filter, a detail of the known roll

`sub_80AA78C` (`asm29.s:10587`): while building the candidate list, a formation with condition byte 5 stays eligible with probability 193/256 if `EVENT_214` is set. That costs 1 primary call per such candidate. It changes the **count** that `iCurrFrame % count` uses, so force the formation pointer (0x02001B9C), not `iCurrFrame`, whenever such formations exist.

## 7. Decisions that read `iCurrFrame` (0x0200A210) instead of, or as well as, an RNG

How the counter is reached:

- The `iCurrFrame` symbol appears only in the toolkit pointer table (`asm00_1.s:7640`) and at `off_80AA884` (`asm29.s:10740`).
- Every other access is `ldr rX,[r10,#oToolkit_CurFramePtr]; ldrh rY,[rX]`.
- No raw 0x0200A210 literal exists.

How it behaves:

- It is written only by the main-loop increment (`main.s:26-30`).
- It is zeroed only by the power-on RAM clear (`start.s:39-42`).
- It is not saved, and it survives a soft reset.

Gameplay uses (everything else is blinking, animation or SFX timing):

| # | Decision | Function | Use | Force by |
|---|---|---|---|---|
| F1 | random-encounter formation | `sub_80AA5F4` (`asm29.s:10424`), from `sub_80AA4C0`, `chooseRandomEncounterMaybe_80aa5e4` (cutscene `start_random_battle`, `map_script_cutscene.s:5803`), `chatbox_804244C` (`chatbox.s:5637`) | `(u16)iCurrFrame % eligibleCount` | `GameState.CurBattleDataPtr` (0x02001B9C) after the roll, or `iCurrFrame` before it |
| F2 | mystery-data contents and position | `sub_8000E3A` (`asm00_0.s:1623`), from `sub_809F9DC`/`sub_809FA68` (`asm28_1.s:116,148`), and gold MD `sub_809FD70`/`sub_809FDE0` (`asm28_1.s:589,653`) | number of `GetRNG` calls = `2 + (rng>>30) + (iCurrFrame&3)`, then a weighted pick from `(last&0xFFFF) % sum` | 0x02004348+2*(id-0x1400), and `byte_2000210+2/+3/+6` for gold |
| F3 | random text-script branch (opcode `F0 XX`, XX>2) | `chatbox_F0_jump` (`chatbox.s:4288-4330`) | calls `GetRNG` `(iCurrFrame&3)+2` times, then `(rng&0x3FF) % sum(weights)`; with bit 7 of XX and `EVENT_1709` set, the first weight is skipped | the primary seed before the opcode runs (see overworld section) |
| - | dead/no-op | unlabeled block at 0x8143416 (`asm37_1.s:3070`, no references); `sub_814343A` (`asm37_1.s:3088`, picks from {5,5,5}) | - | - |

In battle, `iCurrFrame` is used only cosmetically. For example, `battle_pickObjectAnimState_802BF0C` (`asm03_0.s:12029`, the `&8` toggle on the reward screen).

Because F1-F3 mix in `iCurrFrame`, they replay correctly only if our frame count matches the runner's. Our count goes wrong after any inserted or skipped frame, and also because the runner's power-on-to-start time is unknown.

**Rule:** before each F-decision, either write `iCurrFrame` to the runner's value modulo the period (mod `count` for F1, mod 4 for F2/F3), or write the outcome directly.

Writing `iCurrFrame` is harmless: it is used elsewhere only for blinking.

## 8. Open questions and what to check in the emulator first

1. **Battle seed restore: check that 0x0203F4A4 == the value of 0x020013F0 at phase 4.**
   - Phase 4 is the frame BattleState(0x02034880)+0x02 becomes 8. Phase 8 is the frame +0x02 becomes 0x0C. On the phase-8 frame, 0x020013F0 should equal that value (+1 for the `cbGameState` call after it).
   - The code reading says this applies in single player (§1). The subagents read `sub_800B144` as a "battle end" and link-only path, which I believe is wrong: it is phase 4 of the *first* battle state (`sub_8007850` → `off_8007940` → `battle_dispatchPhase_8007978`). The proof is that `dword_203F4A0` is written only by the packet stores, and single-player battles get past phase 8.
2. **Measure the result-sync latency** (frames from roll R to the `sub_800B444` copy). The table in §3 assumes one word per frame.
3. **Identify the battle-mode handler** (`asm00_1.s` 13397 / 14015 / 14438 / 14892) that ordinary random battles use.
4. **NaviStats+0x26 bits 0/1** (zenny-only / chip-only drop paths) and **NaviStats+0x60** (the shuffle button): which NaviCust program or cross sets them?
5. **`sub_81209DC`'s purpose** (2 secondary calls per battle start).
6. **Is the cybercrow lesson on the route?** If so, verify the crow record layout.
