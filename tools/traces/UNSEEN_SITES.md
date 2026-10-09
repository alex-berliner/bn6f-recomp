# Unobserved indirect call sites: what to record next

Snapshot of 1862 indirect call/jump sites in `build/abi/graph.json` against the routes' `tools/traces/coverage/*.edges` (13 routes). 355 sites fired at least once and **1507 never fired**. This file groups the unfired sites into game areas and says what to play to reach them. Method and caveats are at the end.

Categories (each site counted once):

| category | sites | meaning |
|---|---|---|
| A: new content | 923 | no route ran the code around the site: needs a navi, virus, area, screen or story event the routes have not reached |
| B: visited area, different action | 513 | the area is visited but the branch never ran. Strictly, only 12 sites sit in a function a route ran whose call never fired. 108 sit in a menu or battle handler whose dispatcher a route ran, so a different option, attack or status effect reaches them. 393 are chip handlers and chip-spawned battle objects: battles are visited, so they need a different chip |
| C: probably unreachable by play | 71 | link-session code that needs a second GBA or Wireless Adapter (61), and functions nothing references (10) |

432 of the unfired sites have their PC in a sweep profile (`battlesweep.prof` 390, `chips.prof` 66, `maps.prof` 28): the poked battle and map sweeps reach them, so the code is live. Those sweeps are not routes and recorded no edges.

## Recording themes, in priority order

The order weighs sites per hour of play from the latest save (`tools/traces/story8.end.sav`: Central Area 2, HP 140, 3450z). Counts overlap a little where one battle object serves several themes.

| # | theme | start from | sites | what to do |
|---|---|---|---|---|
| 1 | PET menu sweep | story8.end.sav | 75 (+ some of the 55 UI-sprite sites) | Open every PET screen the routes skipped. Key Items has never been opened. Then: NaviCust, if available; using a SubChip from the menu; every folder-edit operation (sort, pack to folder, swapping folders); Library tabs; Mail. Then PET → Network: pick every entry, go as far as the cable/Wireless Adapter prompt or the connection failure, and back out. |
| 2 | Chip variety battles | story8.end.sav | 130 | Buy or collect chips and use at least one of each family in Central Area battles. The families with unfired handlers are bombs/seeds, Sandworm/FireHit/Boomer/Lance/GolemHit, YoYo, AirSpin, CornShot, RiskyHoney, H-Burst, MachGun, AirHockey, IronShield, WideShot, TankCannon, BubbleStar, WindRack, RollingLog, AuraHead, the Dragon chips, Spreader, FireBurner, WaveArm, TornadoArrow, Tornado/Static, DrillArm, DollThunder, MoonBlade, ElecPulse, MagCoil, AquaNeedle, GunDelSol, BatCan and the Variable/Neo/Z Saver swords. |
| 3 | Story chain: next navi bosses | story8.end.sav, then each new end save | 337 (all navis; one chapter's boss is roughly 10–35) | Continue the story chain (story9…) through each chapter's navi boss. Use several different chips during each boss fight, and later fight the V2/V3/SP ghosts. |
| 4 | Virus hunting in each newly opened net area | after each story step | 134 (+ part of the 50 enemy-only attack actions and the 149 battle objects with no known spawner) | Once the story opens a new net area, wander it until every virus family there has been fought. Use a few different chips so the viruses' counter-attacks also run. |
| 5 | Overworld and event content in new towns | after each story step | 95 | Talk to everyone and use every interactive object in new towns. Walk into each newly opened net area at least once: just entering a net area runs its background-animation setup (AquariumComp, JudgeTreeComp, MrWeatherComp, PavilionComp, Comps2, SeasideArea, GreenArea, Underground, SkyACDCArea, Undernet, Graveyard). |
| 6 | Cross / Beast Out | first save after MegaMan gets a Cross | 77 (forms 26, custom screen 13, engine helpers 38; some engine helpers are general chip-use paths) | Open the custom screen's Cross/Beast window, transform, use each form's charge shot and special, and Beast Out (and Beast Over, if possible). |
| 7 | Navi chips, Program Advances, giga and time-freeze support chips | late or postgame save | 267 (navi-chip/PA table 148, time-freeze table 119) + 4 navi-chip action | Use navi chips (Roll, ProtoMan, the crossed navis), Program Advances, giga chips and the support chips that freeze time (Anubis/Guardian/VDoll/Fanfare-type obstacles) in battle. chips.prof shows that even the poked every-chip sweep barely reached these, so they need deliberate, successful use. |
| 8 | Special battles and battle endings | story8.end.sav | 24 | Run away from a battle; lose one (game over and continue); play any special-rule battle (Request BBS jobs, timed or conditional battles); let MegaMan get frozen, bubbled, paralysed and pushed. |
| 9 | Shops, traders and boards | story8.end.sav or later | 10 | Use a Chip Trader machine, the Request BBS, and any number/password entry (Number Trader). Note that maps.prof already reached the Request BBS control code. |
| 10 | Ending | final save | 6 | Beat the game and let the credits run. |

A cheaper alternative for battle content is a poked route rather than play. `battle` is already a route of `battle.trace` plus `battle.poke`, which points StartBattle at a battle-settings record. More routes like it, with records that pick a given navi or virus, would reach most of the navi and virus sites: `battlesweep.prof` shows that pokes reach them. A `chipsweep.py`-style folder poke does the same for chips.

## Battle (1184 sites)

The battle code in asm31/asm32 is laid out per content, so each site is assigned by address to its enclosing content region. The regions come from:

- `T1/T3/T4BattleObjectJumptable` (asm00_1): T1 is field objects and obstacles, T3 attack objects, T4 effects and summons;
- `AIAttackJumptable`: chip and attack actions 0x10–0x5e, where the chip family's `attack_family` is the action number;
- the navi AI region table `off_80F24D8` and the virus AI region table `off_81090D0`.

Each object is then attributed to the code that spawns it (bn6f references to its spawner): a chip action, a virus, a navi, the useChip special table `off_802CCB4` (navi chips and PAs), or the time-freeze table `off_802CD5C`.

### Navi bosses: 337 sites (319 A, 18 B)

Navi AI index n maps to the navi name table in order (1 HeatMan … 16 ElementMan). Two anchors support this: bn6f's comment `0x2C protoman` at index 11, and story7's boss ran index 12, BlastMan. Names for index 17 and up are guesses. 'Own' counts sites in the navi's AI code; the rest are its attack objects.

| navi | sites | own | sweep-reached | examples |
|---|---|---|---|---|
| BassXX? (name uncertain) | 34 | 22 | 26 | `t1_0x58_80C46FC`, `t3_0x53_80CEB00`, `sub_80CEB66` |
| Falzar(beast) (name uncertain) | 24 | 12 | 10 | `t1_0x26_80BFDFC`, `sub_80BFE6E`, `t3_0x81_80D5580` |
| Gregar(beast) (name uncertain) | 23 | 10 | 0 | `t1_0x1a_80BC4FC`, `sub_80BC56E`, `t3_0x61_80D0610` |
| ElementMan | 23 | 11 | 14 | `t3_0x1c_80C7C18`, `sub_80C7CC0`, `t3_0x8d_80D6BD4` |
| TomahawkMan | 19 | 9 | 9 | `t3_0x3a_80CB284`, `sub_80CB368`, `t3_0x40_80CC0E8` |
| SpoutMan | 18 | 8 | 18 | `t3_0x17_80C6DCC`, `sub_80C6E50`, `t3_0x22_80C853C` |
| ChargeMan | 18 | 10 | 7 | `t3_0x7c_80D4B68`, `t3_0x86_80D5D54`, `sub_80D5DC8` |
| ElecMan | 17 | 9 | 16 | `t3_0x1a_80C740C`, `sub_80C7450`, `t3_0x1b_80C793C` |
| ?24 (name uncertain) | 17 | 15 | 12 | `t3_0x50_80CE530`, `sub_80CE5F0`, `sub_8107C4C` |
| CircusMan | 16 | 8 | 14 | `t3_0x5d_80D00A0`, `sub_80D0132`, `t3_0x5e_80D0268` |
| GroundMan | 16 | 8 | 4 | `t3_0x7e_80D5028`, `t3_0x7f_80D5138`, `sub_80D51E4` |
| ProtoMan | 15 | 12 | 12 | `t3_0x6b_80D1B48`, `t4_0x63_80E795C`, `sub_80E79CE` |
| HeatMan | 14 | 8 | 14 | `t3_0x25_80C8AA4`, `sub_80C8B3E`, `t3_0x26_80C8C74` |
| EraseMan | 14 | 8 | 8 | `t3_0x65_80D0F8C`, `sub_80D102E`, `t3_0x67_80D1514` |
| DustMan | 14 | 8 | 10 | `t3_0x83_80D58B4`, `sub_80D592C`, `t3_0x84_80D5ABC` |
| JudgeMan | 13 | 7 | 9 | `t3_0x6d_80D2034`, `sub_80D20E8`, `t3_0x70_80D25D4` |
| ?23 (name uncertain) | 12 | 12 | 7 | `sub_8106924`, `sub_8106DEC`, `sub_8106E0C` |
| SlashMan | 11 | 9 | 7 | `t3_0x43_80CC76C`, `sub_80CC800`, `sub_80F48C8` |
| TenguMan | 10 | 8 | 9 | `t3_0x47_80CCD70`, `sub_80CCDFC`, `sub_80F8FE0` |
| BlastMan | 5 | 2 | 5 | `t3_0x21_80C8388`, `sub_80C8498`, `t4_0x1a_80E21D8` |
| DiveMan | 3 | 1 | 3 | `t3_0x39_80CB0DC`, `sub_80CB154`, `sub_80FE330` |
| Bass (name uncertain) | 1 | 1 | 1 | `sub_8101360` |

Notes:

- AI 20 ('Gregar'?) is the only navi region whose code `battlesweep` never ran. It may be the other version's Cybeast, which would make it unreachable in Falzar. Not confirmed.
- AI 24's region also holds shared navi code that every battle runs, so its name and its count are unreliable.
- BlastMan and DiveMan were fought (story7 and the poked `battle` route). Their leftovers are attack variants: let the fight run longer, and use different chips against them.

### Viruses: 134 sites (131 A, 3 B)

Virus AI index n maps to virus family n, in the order of `TextScriptVirusChipNames` (bn6f comments confirm Mettaur = 1, Gunner = 23, SnakeArm = 26). The routes fought only Mettaur, Gunner, Champy and OldStove.

| virus family | sites | own AI | sweep-reached | examples |
|---|---|---|---|---|
| Heady | 9 | 3 | 9 | `t1_0x19_80BC17C`, `sub_80BC1DA`, `sub_80BC29C` |
| DarkMech/ElecMech | 8 | 4 | 8 | `t1_0x27_80BFF68`, `sub_80BFFC0`, `t3_0xc0_80DD0AC` |
| Swordy | 8 | 6 | 8 | `t1_0x57_80C4530`, `sub_80C458C`, `sub_810AF30` |
| SnakeArm | 7 | 3 | 7 | `snakearmRelatedObject_80BC87C`, `snakearmRelatedObject_update_80BC8EC`, `snakearmRelatedObject_t1_0x22_80BE4D8` |
| HoneyBomber | 7 | 4 | 7 | `t3_0x6c_80D1C20`, `sub_80D1D14`, `sub_80D1DEE` |
| ElementDragon | 6 | 2 | 6 | `t1_0x2e_80C0F6C`, `loc_80C1014`, `t1_0x2f_80C11AC` |
| BombCorn | 6 | 4 | 6 | `t3_0x5_80C55B0`, `sub_80C5654`, `sub_8111A00` |
| Piranha | 6 | 2 | 6 | `t3_0x51_80CE70C`, `sub_80CE782`, `t4_0x42_80E5470` |
| KillerEye | 6 | 2 | 6 | `t3_0x9f_80D9154`, `sub_80D91C8`, `t3_0xa0_80D9350` |
| PulseBulb | 6 | 5 | 6 | `t4_0x6b_80E807C`, `sub_8110714`, `sub_81107A0` |
| Cragger | 5 | 2 | 5 | `t1_0x2_80B85E0`, `sub_80B863A`, `sub_80B86D4` |
| BigHat | 5 | 3 | 3 | `t1_0x1d_80BCA04`, `sub_80BCA72`, `sub_811121C` |
| Tuby1-3EX (minigame?) | 5 | 2 | 4 | `t3_0x79_80D4440`, `sub_80D44D2`, `sub_80D4570` |
| FighterPlane | 4 | 3 | 4 | `t1_0x3_80B88D0`, `sub_8113330`, `sub_811348E` |
| Puffy | 4 | 2 | 4 | `t3_0x6_80C57C0`, `sub_80C5886`, `sub_810ED70` |
| StarFish | 4 | 2 | 4 | `t3_0x3d_80CB900`, `sub_80CB9E2`, `sub_810F2EC` |
| HauntedCandle | 4 | 2 | 4 | `t3_0x97_80D825C`, `sub_80D8310`, `sub_810DE00` |
| Mettaur1-3EX (minigame?) | 4 | 1 | 4 | `t3_0xd1_80DF90C`, `sub_80DFA58`, `t4_0x24_80E2F56` |
| Catack | 4 | 4 | 4 | `sub_810C448`, `sub_810C524`, `sub_810C544` |
| Kettle | 4 | 4 | 2 | `sub_810E4D4`, `sub_810E57C`, `sub_810E8DA` |
| WindBox/VacuumFan | 3 | 1 | 3 | `t4_0x41_80E5244`, `sub_80E5296`, `sub_810D130` |
| Quaker | 3 | 3 | 3 | `sub_810BFB8`, `sub_810BFE0`, `sub_810C1D8` |
| Trumpy/Tuby/Tromby/MuteAnt/Xylos | 3 | 3 | 3 | `sub_810D258`, `sub_810D280`, `sub_810D378` |
| Nightmare | 3 | 3 | 3 | `sub_8115CD8`, `sub_8115E4C`, `sub_8115E6C` |
| Mettaur | 2 | 2 | 1 | `sub_8109E7A`, `sub_810A0BA` |
| ScareCrow | 2 | 2 | 2 | `sub_811037C`, `sub_811055C` |
| Shrubby | 2 | 2 | 2 | `sub_81120E4`, `sub_8112278` |
| Armadill | 2 | 2 | 2 | `sub_81151A0`, `sub_811557C` |
| TestVirus | 1 | 1 | 0 | `sub_8109B9C` |
| Gunner/Shooter/Sniper | 1 | 1 | 1 | `sub_8112D9C` |

'Mettaur1-3EX' and 'Tuby1-3EX' (AI 30 and 31) are special variants; what they are for is unknown.

### Standard chips: 130 sites (B: battles are visited, these chips were never used)

One site per family is the action handler's dispatch, and the rest are the family's attack objects. The chip names come from `ChipDataArr` `attack_family`.

| action | chips | sites | examples |
|---|---|---|---|
| 0x1c | MegaBstr/SandWrm1/SandWrm2/SandWrm3/FireHit1… | 27 | `t3_0xe_80C6280`, `sub_80C62B0`, `t3_0x30_80C9D00` |
| 0x12 | MiniBomb/EnergBom/MegEnBom/FlshBom1/FlshBom2… | 14 | `t3_0x4a_80CD4EC`, `t3_0x4f_80CE24C`, `sub_80CE2B4` |
| 0x18 | YoYo/GreatYo | 7 | `t3_0x52_80CE81C`, `sub_80CE87C`, `sub_80CE8D8` |
| 0x38 | AirSpin1/AirSpin2/AirSpin3 | 6 | `airspinObject_80D88E0`, `airspinObject_update_80D8988`, `t3_0xd4_80DFFB8` |
| 0x2a | CornSht1/CornSht2/CornSht3 | 4 | `cornfiestaRelatedObject_80C6580`, `sub_80C65EE`, `sub_80C667C` |
| 0x39 | RskyHny1/RskyHny2/RskyHny3 | 4 | `riskyHoneyObject_80D30D0`, `riskyHoneyObject_update_80D317C`, `riskyHoneyObject_update_0x4_80D3240` |
| 0x34 | H-Burst | 4 | `t3_0x9e_80D8FC4`, `sub_80D9024`, `sub_80D90AE` |
| 0x1b | Roll/Roll2/Roll3/ProtoMan/ProtoMnEX… | 4 | `t4_0x10_80E17E8`, `sub_80E1808`, `sub_80E1830` |
| 0x29 | MachGun1/MachGun2/MachGun3 | 3 | `t3_0x9_80C5DDC`, `sub_80C5E44`, `sub_80ECF2E` |
| 0x26 | AirHocky/PitHocky | 3 | `t3_0x2e_80C9824`, `sub_80C98DC`, `sub_80ECCB0` |
| 0x2c | IronShl1/IronShl2/IronShl3/ParaShl | 3 | `t3_0x34_80CA6B8`, `sub_80CA7AC`, `sub_80ED25C` |
| 0x30 | WideSht/SuprSpr | 3 | `t3_0x3b_80CB49C`, `sub_80CB5B4`, `sub_80ED55C` |
| 0x24 | TankCan1/TankCan2/TankCan3 | 3 | `t3_0x3c_80CB6F8`, `sub_80CB762`, `sub_80ECACA` |
| 0x2d | BblStar1/BblStar2/BblStar3 | 3 | `t3_0x42_80CC5A8`, `sub_80CC668`, `sub_80ED2F8` |
| 0x3f | WindRack | 3 | `t3_0x49_80CD2EC`, `sub_80CD3B0`, `sub_80EE192` |
| 0x36 | RlngLog1/RlngLog2/RlngLog3 | 3 | `t3_0x66_80D1218`, `sub_80D12CA`, `sub_80ED9AE` |
| 0x43 | AuraHed1/AuraHed2/AuraHed3/StreamHd | 3 | `t3_0x92_80D7400`, `sub_80D74DC`, `sub_80EE634` |
| 0x51 | HeatDrgn/ElecDrgn/AquaDrgn/WoodDrgn | 3 | `t3_0xc9_80DE404`, `sub_80DE4EC`, `sub_80EF4B4` |
| 0x19 | BatCan1/BatCan2/BatCan3/BatCan4 | 2 | `t3_0x1_80C50B8`, `sub_80EC0E6` |
| 0x28 | TrnArrw1/TrnArrw2/TrnArrw3 | 2 | `t3_0xb_80C60A8`, `sub_80ECDFC` |
| 0x2f | Tornado/Static | 2 | `t3_0x31_80C9F78`, `tornadoAiAttack_80ED454` |
| 0x55 | SonicBom/Curse/Punisher/SprSonic | 2 | `t3_0x58_80CF7F0`, `sub_80EF970` |
| 0x2e | DrilArm | 2 | `t3_0x71_80D2A94`, `sub_80ED374` |
| 0x3e | DolThdr1/DolThdr2/DolThdr3 | 2 | `dollThunderObject_80D5740`, `dollThunderAttack_80EE0BC` |
| 0x40 | MoonBld | 2 | `t3_0x85_80D5C48`, `sub_80EE2A0` |
| 0x42 | ElcPuls1/ElcPuls2/ElcPuls3/DestPuls | 2 | `t3_0x8c_80D6A20`, `sub_80EE55E` |
| 0x44 | MagCoil | 2 | `t3_0x95_80D7DE4`, `sub_80EE6FC` |
| 0x32 | AquaNdl1/AquaNdl2/AquaNdl3 | 2 | `t4_0x40_80E50C4`, `sub_80ED6E6` |
| 0x37 | GunDelS1/GunDelS2/GunDelS3/GunDelEX | 2 | `t4_0x48_80E5C2C`, `sub_80EDAE0` |
| 0x14 | Cannon/HiCannon/M-Cannon/GigaCan1/GigaCan2… | 1 | `t4_0x28_80E32B8` |
| 0x25 | Spreadr1/Spreadr2/Spreadr3 | 1 | `bubbleShotAttack_80ECBB0` |
| 0x27 | FireBrn1/FireBrn2/FireBrn3/WideBrn1/WideBrn2… | 1 | `sub_80ECD28` |
| 0x31 | WaveArm1/WaveArm2/WaveArm3/PwrWave1/PwrWave2… | 1 | `sub_80ED64C` |
| 0x49 | MchnSwrd/ElemSwrd/AssnSwrd | 1 | `sub_80EEB4C` |
| 0x53 | VarSwrd | 1 | `sub_80EF62E` |
| 0x54 | NeoVari | 1 | `sub_80EF7E2` |
| 0x5b | Z Saver | 1 | `sub_80EFEE0` |

### Navi chips, Program Advances, giga and time-freeze chips: 267 sites

- **148**: objects spawned through `off_802CCB4`, the useChip special handler table, used when `useChipRelated_80127C0` returns 0x15. Its entries include `spawnDblHeroSetupObject`, `spawnSunMoonObject`, `cornfiestaObject` and `spawnBattleTrapMaybe` (navi chips, PAs and giga chips). Examples: `t1_0x4_80B8A18`, `sub_80B8AD4`, `t1_0x13_80BAF50`, `sub_80BB00C`.
- **119**: objects spawned through `off_802CD5C`, which `timefreezeObjectRunFunction_80E1880` indexes, so these are chips that stop time. Examples: `t1_0x1_80B8210`, `sub_80B829C`, `sub_80B82C4`, `sub_80B8376`.
- Which chip spawns which object is not resolved. chips.prof (a poked folder of every chip with random input) reached only 9 of these 267 sites; battlesweep reached 1.

### Other battle content

| area | sites | A/B | sweep-reached | examples | what to do |
|---|---|---|---|---|---|
| Battle objects with no known spawner | 149 | 148/1 | 12 | `t1_0x1b_80BC650`, `t1_0x29_80C0334`, `sub_80C03AE`, `t1_0x2a_80C04AC` | T1/T3/T4 objects whose spawner is not traced (the spawn index is an immediate, or the spawner is indirect). Expect them to come with the navi, virus and chip themes. |
| Enemy-only attack actions | 50 | 48/2 | 0 | `t3_0x5c_80CFEC4`, `sub_80CFF62`, `t3_0x76_80D385C`, `t3_0x96_80D807C` | Attack actions no chip uses (0x1a, 0x1d, 0x3a, 0x41, 0x45–0x50, 0x57…). Viruses and navis use them, so fight new enemies. |
| MegaMan forms/actions | 26 | 26/0 | 4 | `sub_80EAD9C`, `sub_80EAF36`, `sub_80F020E`, `sub_80F070E` | Handlers in `PlayerObjectAIAttackJumptables` form tables 1–11 (probably the Cross and Beast forms' special attacks), plus the shared code after action 0x5e. Which table is which form is not known; some tables may be the other version's crosses. |
| engine helpers (chip use, MegaMan actions, Beast/Cross) | 38 | 5/33 | 4 | `sub_800B79A`, `sub_800BDB2`, `sub_80109A4`, `dispatchHandlerById_8010D58` | Chip-use and MegaMan action paths: `dispatchHandlerById_8010D58` (ids 0x11E–0x122), the time-freeze end path, player-AI states, and status-effect reactions. Use more chip kinds, Beast Out, and get hit by status effects. |
| battle-flow variants (special battle types) | 24 | 16/8 | 9 | `sub_80039AA`, `sub_8003AFA`, `sub_8007E62`, `sub_8007F4E` | `battle_update` states for battle modes 6, 7 and 9 (`GetBattleMode`) and the early-termination paths. Some may be link-battle paths. battlesweep reached 9 of them, so some battle-settings records use these modes. |
| custom screen & battle HUD states | 13 | 6/7 | 2 | `battle_beastJumpLockOn_8026554`, `sub_8027044`, `sub_80271F8`, `sub_8027406` | Unused states 0x24–0x5C of `custMenuMainMaybe_8026A88`, plus `battle_beastJumpLockOn`. Likely the Cross/Beast window and other custom-screen options (a guess). |

## Menus and screens

| area | sites | A/B | examples | what to do |
|---|---|---|---|---|
| PET Network menu (screens before a connection is needed) | 55 | 41/14 | `sub_8129598`, `sub_8129658`, `sub_81297DC`, `sub_8129918` | `HandleCommMenu81291E8` ran in story2, but only entries 0x00 and 0x04 and one sub-state. Entries 0x08–0x38 (asm33, dat33–35, asm36) never ran. Go into each one up to the 'cable or Wireless Adapter?' prompt. Without a partner the deeper screens are likely unreachable. |
| UI sprite entities (table off_804646C) | 55 | 55/0 | `sub_8117110`, `sub_8117218`, `sub_81172B0`, `sub_8117404` | Types of the 15-slot UI sprite array (`entity_runAllUpdateHandlers_80465BC`) used by PET screens, BBS, traders and result screens. Which screen spawns each type is not mapped. |
| PET: Key Items menu | 2 | 1/1 | `HandleKeyItemMenu8128730`, `sub_81287B4` | Submenu 0x14 was never opened in any route. Open Key Items and look at an item. |
| PET: submenu 0x24 (NaviCust-related?) | 3 | 2/1 | `sub_81356D4`, `sub_81357C4`, `sub_81368D4` | Uses the NaviCust program description text. Probably the NaviCust screen. |
| PET: NaviCust (submenu 0x28) | 1 | 0/1 | `sub_8136DE0` | Contains `RunTextScriptNaviCustDialog`. |
| NaviCust program effects | 5 | 0/5 | `sub_813B7A0`, `applyNavicustPrograms_813C684`, `sub_813CBCC` | `applyNavicustPrograms_813C684` runs, but three program-effect calls never fired. Install different programs and run them. |
| PET: folder edit | 4 | 2/2 | `sub_8133484`, `sub_8133540`, `sub_81335EC`, `sub_8133A44` | Folder edit (submenu 0x20) states not used yet. |
| PET: submenu 0x2C (navi records?) | 2 | 1/1 | `sub_81378B4`, `sub_8137964` | Uses navi chip names and the text 'You got the top record, Lan!'. A guess: navi time records. |
| PET: submenu 0x30 (unknown) | 2 | 1/1 | `sub_8128CBC`, `sub_8128DCC` | Uses chip description text. Trigger unknown. |
| PET: SubChip menu | 1 | 0/1 | `submenu_subchip_dispatch_8124144` | Use a SubChip from the menu. |
| Chip Trader machine | 2 | 2/0 | `ChipTraderControl`, `sub_804A54C` | Main subsystem 0x34: find and use a Chip Trader. |
| Request BBS | 2 | 2/0 | `reqBBS_cb_draw_813E0A4`, `reqBBS_subsystemCotnrol` | Main subsystem 0x44 (maps.prof reached the control code). Open the Request BBS. |
| Text-script commands (dialog features) | 3 | 0/3 | `chatbox_80426A8`, `chatbox_FE_numberinput`, `chatbox_FF_copytext` | `chatbox_FE_numberinput` (number entry, e.g. a Number Trader or password), `chatbox_FF_copytext`, and an FC interface sub-command. |
| Main subsystem 0x4C (folder-name screen?) | 3 | 3/0 | `cb_8049E04`, `sub_8049E84`, `sub_8049FA4` | Entered from the folder menu code and uses `TextScriptFolderNames`. maps.prof reached it. |
| Scripted special screen (main subsystem 0x0C) | 17 | 17/0 | `cb_8038AD0`, `sub_8038B64`, `sub_8038B9C`, `sub_8046818` | Launched by native callbacks in 4 cutscene scripts (asm23, asm24, asm26, dat21). Its text bank has '…just expired!' and 'Let's practice, Lan!'. Which event triggers it is unknown. |
| Ending credits | 6 | 6/0 | `cb_81382AC`, `sub_8138348`, `sub_81383F4`, `sub_813842C` | Main subsystem 0x38 (`playCreditsScene_81382D0`): beat the game. |
| Text rendering variants | 4 | 1/3 | `sub_3006DA4`, `sub_3006E7C`, `sub_8046154`, `sub_8046450` | Glyph or text renderers for screens not visited (IWRAM `sub_3006DA4` and `sub_3006E7C`). |
| Sound engine | 6 | 2/4 | `sound_8000822`, `sub_802CB78`, `sub_802CBAC`, `sub_814E15C` | Rare sound-request paths and m4a commands; may come with new music and effects. |

## Overworld, maps and events

| area | sites | A/B | sweep-reached | examples | what to do |
|---|---|---|---|---|---|
| Overworld map objects (event-specific) | 42 | 42/0 | 5 | `dispatch_8089E90`, `dispatch_808A028`, `sub_80A57AC`, `sub_80A5AD4` | Overworld map object types 4–46 (table `off_8004724`, asm29) and two dat23 dispatchers. Map-specific code spawns them during events; the 108-map sweep reached only 5. Advance the story in later towns and use every object. |
| Map / cutscene script callbacks | 19 | 16/3 | 2 | `endScenarioEffectMaybe_8003940`, `endMinigameEffectMaybe_8003a90`, `RunSecondaryContinuousMapScript`, `sub_804C30C` | Native callbacks of specific map and cutscene scripts: Central Town scripts reached from `MapScriptOnInitCentralTown_804EA28` and `cutscenescript_808DB30`, plus scenario-effect and minigame-effect end handlers. These are later story events. |
| Internet area BG animations (map loaders) | 11 | 11/0 | 11 | `AquariumComp_UnkFunction_8067d1c`, `JudgeTreeComp_UnkFunction_80691d4`, `MrWeatherComp_UnkFunction_806a1b4`, `PavilionComp_UnkFunction_806acc0` | The `*_UnkFunction` BG-animation setup of 11 net areas. Entering the area is enough (maps.prof shows this). |
| Overworld map loading via cutscene command | 2 | 2/0 | 0 | `mapObject_spawnMapObjectsForMap`, `LoadBGAnimForMapGroup` | `cs_spawn_ow_map_objects_from_map` and BG-animation loads through the per-group jump tables. These are cutscenes that re-spawn map objects. |
| Overworld player movement variants | 2 | 0/2 | 1 | `sub_809D800`, `sub_809D8F4` | A special movement path (`calledWhenUsingSlipRun_809D6BC`, slip-run or sliding). |
| NPC script features | 1 | 0/1 | 0 | `npc_runNativeCallback_809ea74` | `npc_runNativeCallback_809ea74`: an NPC script with a native callback. |
| Cutscene navi swap | 1 | 0/1 | 0 | `sub_8121154` | `CutsceneCmd_change_navi` → `reloadCurNaviBaseStats`: a scene that switches the active navi. |

## Probably unreachable by play: don't chase these

- **Link sessions, 61 sites.** These are the code behind a working connection:
  - main subsystem 0x18 (`cb_80395A4`), reached from the title screen's third option;
  - subsystem 0x30 (`cb_8048FD4`), reached from the Network menu;
  - the comm-error screens (`cb_803CBA6`, `cb_803CCD6`);
  - the link-state machine (asm03_1_1 0x803DE80–0x803EE00);
  - the RFU/STWI wireless library (libs.s 0x8144000–0x814E000, `rfu_iwram_*`).

  mgba_run plays one GBA, so these need a partner. Examples: `rfu_iwram_03000E28`, `rfu_iwram_03000E2C`, `cb_80395A4`, `sub_8039658`, `sub_8039846`, `sub_80399CE`.
- **No external references, 10 sites.** Nothing in bn6f references these functions except their own jump tables: `sub_80049FC`, `sub_8013C4E`, `sub_802DAA8`, `sub_802E95C`, `sub_802EC00`, `sub_8038606`, `sub_80E4B68`, `sub_80FB5B8`, `sub_8112C80`, `sub_8131F20`. They are probably dead, or reached through a pointer the reference scan missed.
- **Possibly the other version's content.** Navi AI 20 (23 sites; 10 in its own AI) and possibly some MegaMan form tables. Unconfirmed, so they are counted above as A or B.
- **Unclassified, 1 site:** `sub_813D9AC`.

## Method and caveats

- **Inputs.** Copies of `build/abi/graph.json` (indirect and jump sites) and `tools/traces/coverage/*.edges` and `*.prof`, taken 2026-10-09. A site counts as observed if any `.edges` file lists it. This gives 355 sites, against 340 in the export's `observed` field, which drops sites whose target is not a lifted entry.
- **Executed by a route.** A route's `.prof` contains a PC of the site's enclosing graph entry (the function). The sweep profiles (`battlesweep`, `chips`, `maps`, `fuzz`) are poked or random runs, not routes; they are used only as evidence that the code is reachable.
- **Area names.** They come from function and table names in bn6f, symbols/db summaries, a reference graph over bn6f's labels (`bl`, `.word`, `ldr =`), bn6f table comments, and the name text scripts. Names marked '?' or 'guess' are inferences; the navi names for AI 17 and up, the PET submenus 0x24/0x2C/0x30, and main subsystems 0x0C and 0x4C are the weakest.
- **Weighting.** Weighting is by site count. The export has no static target set for unobserved sites, so the number of distinct targets behind each site is unknown.

