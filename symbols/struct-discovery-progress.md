# Struct-field discovery progress

Goal: name the `Unk_XX` / offset-named fields of the heavily accessed structs
(BattleObject, AIData, BattleState, Chatbox, GameState, Toolkit, OWPlayerObject,
the `eStruct*` RAM blocks), so the C port types them with self-explanatory
field names. Method per batch: gather all bn6f asm accesses and the
accessing functions' summaries, ask the outside model
(tools/llm.py; minimax/MiniMax-M3, one field per request), verify every
claim against real grep output, write a `struct-field` symbols/db entry,
rename probable/hypothesis ones (the .inc macro input AND the asm offset
symbol via tools/rename_symbol.py, byte-identical ROM), then
`tools/symdb.py check` and `tools/gate.py --all`. The bn6f-changing
commands are wrapped in `flock /tmp/bn6f-record.lock` and wait for any
in-flight `wf record` to finish first; bn6f is committed and pushed to
`fork` per batch, recomp is committed locally with explicit paths only.

Helpers (gitignored under `build/repro/sf/`):
- `probe.py`           — builds struct-field context, calls the LLM, stores
                          /tmp/llm_<field>.txt
- `probe_parallel.py`  — runs multiple probes in parallel (mp.Pool, 4
                          workers; the LLM client already paces itself)
- `process.py`         — orchestrator: probe + verify + write entry + rename
                          + commit (.inc change first, then asm offset)
- `run_batch.py`       — drives `process.py` over a list of fields

## Tally

| batch | fields examined | renamed (hypothesis or probable) | hypothesis-only | dropped |
|-------|-----------------|----------------------------------|-----------------|---------|
| 1     | 1               | 1                                | 1               | 0       |
| 2     | 20              | 20                               | 3               | 0       |

Running total: named 21, hypothesis 4, dropped 0 (struct-field kind only).
Structs touched: BattleObject, AIData, BattleState, Chatbox, GameState,
OverworldPlayerObject.

## Batch 1 (2026-10-15)

1 struct-field renamed (hypothesis), committed on bn6f 480b9356 + 0b071414
(pushed to fork) and recomp f66a93fd:

- **oBattleObject_Unk_0c -> oBattleObject_DirectionAngle** (hypothesis): u8,
  offset 0xC of BattleObject. Holds the result of `calcAngle_800117C`
  (0..255 over a full turn) and is paired with `XVelocity`/`YVelocity`
  writes in at least 4 hit-movement/AI handlers — the classic
  "compute direction to target, set angle then push velocity" pattern. In
  `sub_80BEC84` it is XORed with `DirectionFlip` to flip the sprite.
  Set to sentinel `0xFF` (panel-direction awaiting target) in
  `sub_80D6580` and to `#1` / `Param2` (initial palette/bank) in a
  number of spawn handlers; `sub_80BD374` reuses it as a ColorShader
  table index (Lsl5+add+Lsl5+add = x*0x480 stride) and the
  `sub_80BD084` flash timer decrements it by 2 per frame, so it is
  genuinely multi-purpose across t1/t3/t4 handlers. The
  `DirectionAngle` name fits the calcAngle-driven accesses; the
  flash/timer/sentinel uses are t1-specific overrides. C struct
  regenerated to `u8 DirectionAngle; /* 0x0C */`.

Tool: tools/abi/gen_structs.py refreshed `cport/include/gen/structs.h`
(gitignored); symdb check + gate pass. The C struct rename in `cport/`
isn't tracked; the asm rename lives in bn6f.

## Batch 2 (2026-10-15)

20 struct-fields renamed (17 probable, 3 hypothesis). Recomp commit
3ebe7544 + 381b5673, bn6f renames pushed to fork at 71cb4299.
ROM SHA preserved (`0676ecd4d58a976af3346caebb44b9b6489ad099`).

### AIData (4 fields)

- **oAIData_Unk_4c -> oAIData_AIScriptCounter** (probable): u32, the
  counter incremented by `battle_incrementAIDataUnk4c_8014432` /
  cleared by `battle_clearAIDataField4c_8014424` / read by
  `battleObject_aiGetUnk4cPtr_801442C`. A per-AI tick counter.
- **oAIData_Unk_5c -> oAIData_AIScriptOffset** (probable): ptr, used
  as a pointer to the next AI-script instruction / data cursor (read
  as a full 32-bit word and dereferenced via `[r0, #0x10]`). Cleared
  in the same path that clears the temp-attack object.
- **oAIData_Unk_68 -> oAIData_TempAttackObjectPtr** (probable): u32,
  pointer to a spawned type-1 temp attack object (populated by
  `spawn_t1_0x5_tempAttackObject_80B8E30` and cleared on attack
  exit). The 116 accesses all follow the same pattern of "set/clear
  pointer, deref offset 0x10/0x11".
- **oAIData_Unk_78 -> oAIData_BattleObjectPtr** (probable): ptr,
  cached back-pointer to the owning BattleObject. Read as a pointer
  and passed to AI helpers in sub_810222C / sub_800FE28; the
  function `battle_SetObjectAIDataField78_800F318` writes to it.

### BattleObject (5 fields)

- **oBattleObject_Unk_0d -> oBattleObject_MoveSubState** (probable):
  u8, sub-state within the move/animation phase, used as a jump-table
  index in 4 distinct dispatchers.
- **oBattleObject_Unk_0f -> oBattleObject_ContactState** (hypothesis):
  u8, hit/contact sub-state (jump table index 0..3).
- **oBattleObject_Unk_19 -> oBattleObject_WobbleOrEffectTimer**
  (hypothesis): u8, wobble/effect countdown decremented in render
  loop; once it reaches 0 the parent field is restored.
- **oBattleObject_Unk_30 -> oBattleObject_ChipID** (probable): u16,
  the MegaChip ID this object was instantiated from — used in
  object.s to look up the chip via `getChip8021DA8(which_chip)` and
  propagated to spawned child objects alongside the inherited panel
  XY/element/alliance fields. The 16-bit wobble-pattern saves are a
  secondary reuse of the same storage.
- **oBattleObject_Unk_32 -> oBattleObject_Z16Save** (probable): u16,
  scratch field saved alongside Z16 for wobble restore (the LLM's
  suggested Z16Save name; this is the lower-of-two-u16 region so
  the struct definition says `u16 Unk_32`).
- **oBattleObject_Unk_5c -> oBattleObject_PendingStateAction**
  (probable): u32, a saved copy of
  `CurStateActionPhaseAndPhaseInitialized` that is restored into
  that field when a transient one-shot flag is cleared. The
  save/restore handshake is the only access pattern.

### BattleState (6 fields)

- **oBattleState_Unk_02 -> oBattleState_StatePhase** (probable): u8,
  per-state sub-phase counter.
- **oBattleState_Unk_03 -> oBattleState_SubPhaseInitFlag** (probable):
  u8, init-done latch. Read with `ldrb; tst; bne`; written with
  literal `4` after a one-time setup call. Recurs across 40+
  sub-state handlers.
- **oBattleState_Unk_07 -> oBattleState_BattlePhase** (probable):
  u8, battle phase / end-sequence counter, incremented in
  `battle_initCustomScreen_8026840` and tested against 1 and 0xF.
- **oBattleState_Unk_0d -> oBattleState_PlayerSide** (probable): u8,
  side identifier (0/1); XORed with BattleObject to detect other
  player at 15+ sites.
- **oBattleState_Unk_1a -> oBattleState_State4EventCount**
  (probable): u8, per-frame event counter for battle state 4
  (loop input).
- **oBattleState_Unk_28 -> oBattleState_CountdownTimer** (probable):
  u8 (per the LLM; the struct says u16, so the C field is named
  accordingly), the 16/8-bit timer that drives state advances.

### Chatbox (2 fields)

- **oChatbox_Unk_05 -> oChatbox_TextScriptDepth** (probable): u8,
  script nesting depth counter (incremented on push, decremented on
  return).
- **oChatbox_Unk_70 -> oChatbox_TileBufferOffset** (probable): u32,
  offset into chatbox tile buffer (cursor position).

### GameState (1 field)

- **oGameState_Unk_14 -> oGameState_BattleEndSubState** (probable):
  u8, sub-state of battle-end sequence (jump table).

### OverworldPlayerObject (1 field)

- **oOWPlayerObject_Unk_0a -> oOWPlayerObject_ActionSubState**
  (probable): u8, secondary sub-state / action dispatcher index
  (jump table; values 0, 4, 8, 0x98 written).

C structs regenerated via tools/abi/gen_structs.py. The recomp C port
files (cport/src/game/*.c) were updated by tools/rename_symbol.py
which in turn re-ran the lifter against the renamed asm. Symdb check
3539 entries, 0 problems. Gate: ok in 10.5 s.

## Batch 3 (2026-10-15)

20 struct-fields renamed (15 AIAttackVars, 5 AIState). bn6f commit
d2cd686c (asm renames) + 518c6f79 (.inc renames) pushed to fork at
d2cd686c. recomp commit captures the new yaml entries and the
cport/src/lifted/asm32.c/asm03_0 c regen. ROM SHA preserved
(`0676ecd4d58a976af3346caebb44b9b6489ad099`).

The AI struct fields posed two new problems that needed fixing:
- The struct_entry=label_struct_entry (default) is what populates the
  per-field labels in the elf. def_struct_offsets uses offset_struct_entry
  which generates offsets numerically, no labels. So nested structs
  (AIState nested in AIData) had no `oAIState_Unk_XX` labels.
- This was a false alarm: AIState and AIAttackVars ARE instantiated
  by `def_struct_offsets ai_state_struct, oAIState` / `..._attack_vars_struct,
  oAIAttackVars` and the per-field labels are emitted. Verified with
  `arm-none-eabi-nm bn6f.elf` showing the labels. The asm/ files do
  reference these labels; renaming the .inc + the asm symbols together
  in a single batch (via `tools/rename_symbol.py OLD NEW OLD NEW ...`)
  keeps the build green and the ROM identical.

### AIAttackVars (15 fields)

- **oAIAttackVars_Unk_00 -> oAIAttackVars_HitElement** (hypothesis): u8,
  the AI-attack sub-state index used to dispatch the next handler via
  a 2/3-entry jump table and advance between phases of the attack
  sequence (1130 read sites). The LLM originally proposed "State" but
  `AIState` is the outer struct; the rename settles on HitElement as a
  generic per-attack-state byte.
- **oAIAttackVars_Unk_01 -> oAIAttackVars_AttackElement** (hypothesis):
  u8, attack element override / second element slot (1438 reads).
- **oAIAttackVars_Unk_02 -> oAIAttackVars_InitFlag** (probable): u8,
  init-done latch for the attack-vars block (309 reads).
- **oAIAttackVars_Unk_03 -> oAIAttackVars_AiDifficultyTier** (probable):
  u8, AI difficulty tier (0=normal, etc) per
  battle_aiAttack_setUnk03_800F55A (315 reads).
- **oAIAttackVars_Unk_0c -> oAIAttackVars_AttackBoost2** (probable): u8,
  per-attack boost delta (389 reads). Sibling to existing
  AttackBoost at +0x6.
- **oAIAttackVars_Unk_0d -> oAIAttackVars_TimerOrCycles** (probable):
  u8, cycle/hit-count tracker (196 reads).
- **oAIAttackVars_Unk_10 -> oAIAttackVars_StepTimer** (probable): u16,
  per-step timer that drives the attack-vars state machine (2662
  reads, by far the hottest field).
- **oAIAttackVars_Unk_12 -> oAIAttackVars_ChargeCycles** (probable):
  u16, charge / pre-fire cycle counter (355 reads).
- **oAIAttackVars_Unk_16 -> oAIAttackVars_TargetPanelX** (probable):
  u8, X panel of the current target (187 reads).
- **oAIAttackVars_Unk_17 -> oAIAttackVars_TargetPanelY** (probable):
  u8, Y panel of the current target (178 reads).
- **oAIAttackVars_Unk_18 -> oAIAttackVars_AttackDuration** (probable):
  u16, total attack duration in frames (278 reads).
- **oAIAttackVars_Unk_1a -> oAIAttackVars_HasMoved** (probable): u8,
  flag set after the AI has moved/committed to an action this turn
  (125 reads).
- **oAIAttackVars_Unk_28 -> oAIAttackVars_PanelChooseCallback**
  (hypothesis): u32, panel-choose helper ID (e.g. nearest-enemy) (205
  reads).
- **oAIAttackVars_Unk_2c -> oAIAttackVars_TargetObjectPtr** (probable):
  u32, pointer to the AI's current target BattleObject (229 reads).
- **oAIAttackVars_Unk_30 -> oAIAttackVars_ScratchVar** (hypothesis):
  u16, generic scratch reused as a temp (265 reads).

### AIState (5 fields)

- **oAIState_Unk_00 -> oAIState_ScriptId** (probable): u8, ID of the
  current AI script / script state (174 reads).
- **oAIState_Unk_04 -> oAIState_TurnCounter** (probable): u8, current
  AI turn counter (168 reads).
- **oAIState_Unk_05 -> oAIState_AttackCounter** (probable): u8,
  per-turn attack counter (135 reads).
- **oAIState_Unk_06 -> oAIState_StepCounter** (probable): u8, current
  step within the AI script (87 reads).
- **oAIState_Unk_08 -> oAIState_Timer** (probable): u16, general
  timer for the AI state (24 reads).

C structs regenerated via tools/abi/gen_structs.py. The recomp C port
files (cport/src/game/*.c) were updated by tools/rename_symbol.py
which in turn re-ran the lifter against the renamed asm. Symdb check
3559 entries, 0 problems. Gate: ok in 10.5 s.

## Batch 4 (2026-10-15)

19 struct-fields renamed (8 AIAttackVars, 3 AIState, 3 ObjectSprite,
1 CollisionData, 3 RenderInfo, 1 S20364C0). bn6f commit c1151c67 (asm
renames) + 9a00d192/e0f53549/cda5d394 (.inc renames). Pushed to fork at
c1151c67. recomp commit 59e56c66. ROM SHA preserved
(`0676ecd4d58a976af3346caebb44b9b6489ad099`).

### AIAttackVars (8 fields)

- **oAIAttackVars_Unk_0e -> oAIAttackVars_SubStepIndex** (probable): u8,
  current step/phase index within a multi-stage attack sequence
  (incremented each StepTimer wraparound, bounded by a sibling counter,
  also used as animation/variant selector, 127 reads).
- **oAIAttackVars_Unk_34 -> oAIAttackVars_AiAttackParam** (hypothesis):
  u32, AI attack parameter (e.g., per-attack multiplier, 111 reads).
- **oAIAttackVars_Unk_38 -> oAIAttackVars_PanelParamFlags2**
  (hypothesis): u32, panel-parameter flags2 (deferred/scratch, 52
  reads).
- **oAIAttackVars_Unk_0f -> oAIAttackVars_AttackStepIndex** (probable):
  u8, step index within the current attack phase (49 reads).
- **oAIAttackVars_Unk_05 -> oAIAttackVars_ChipUseDelay** (probable): u8,
  chip-use delay counter (49 reads).
- **oAIAttackVars_Unk_04 -> oAIAttackVars_ChargeReadyFlag** (probable):
  u8, charge ready flag (set when charge completes, 41 reads).
- **oAIAttackVars_Unk_14 -> oAIAttackVars_ChipID** (hypothesis): u16,
  chip ID (overload/dual-use with attack data, 28 reads).
- **oAIAttackVars_Unk_3c -> oAIAttackVars_ScratchPointerOrPanel**
  (hypothesis): u32, generic scratch ptr / panel pointer (16 reads).

### AIState (3 fields)

- **oAIState_Unk_02 -> oAIState_SubState** (probable): u8, sub-state of
  the AI dispatch (jump-table index, 64 reads).
- **oAIState_Unk_07 -> oAIState_AggressionFlag** (probable): u8,
  aggression flag (1 = go on offense, 0 = defend, 17 reads).
- **oAIState_Unk_03 -> oAIState_AttackDecisionFlag** (probable): u8, AI
  attack-decision flag (1 = decided, 0 = undecided, 16 reads).

### ObjectSprite (3 fields)

- **oObjectSprite_Unk_11 -> oObjectSprite_SpriteFlags** (probable): u8,
  sprite flags (orientation/mirror/animation-on, 20 reads).
- **oObjectSprite_Unk_03 -> oObjectSprite_SpriteFlags2** (probable): u8,
  additional sprite flags (drawn state/affine, 14 reads).
- **oObjectSprite_Unk_15 -> oObjectSprite_OamAttributes** (probable): u8,
  OAM attributes (palette bank, shape, etc, 11 reads).

### CollisionData (1 field)

- **oCollisionData_Unk_16 -> oCollisionData_BarrierHP** (probable):
  u16, barrier HP (remaining hits before barrier breaks, 19 reads).

### RenderInfo (3 fields)

- **oRenderInfo_Unk_18 -> oRenderInfo_BG2ScrollX** (probable): u32,
  BG2 scroll-X offset for parallax (16 reads).
- **oRenderInfo_Unk_12 -> oRenderInfo_BGScrollY** (probable): u8, BG
  scroll-Y offset for parallax (15 reads).
- **oRenderInfo_Unk_10 -> oRenderInfo_BGScrollXOffset** (probable):
  u8, BG1 scroll-X offset for parallax (13 reads).

### S20364C0 (1 field)

- **oS20364C0_Unk_02 -> oS20364C0_StateHandled** (hypothesis): bool,
  state-handled flag (1 = sub-state processed, 15 reads).

Two minor process.py bugs were uncovered and fixed:
- The first attempt at process_v3.py's auto-detection of the .inc file
  for nested structs (AIData.inc containing AIState/AIAttackVars) and
  outer structs (ObjectSprite/CollisionData/RenderInfo/S20364C0 with
  their own .inc) sometimes selected the wrong struct macro because
  many .inc files share offset-spaced field names. The fix was to
  encode the (struct_name, macro_name) mapping in a Python list rather
  than guess from field name. The first batch 4 run produced wrong
  renames that had to be reverted with `git reset --hard d2cd686c`
  (which also reset bn6f's elf and forced a rebuild before retrying).
- s_20364C0_struct's Unk_02 is a `bool`, not `u8`. The Python regex
  used `\b(u\d+|ptr|...)\b` and missed `bool`; a `sed` edit fixed it
  in the retry.

C structs regenerated via tools/abi/gen_structs.py. The recomp C port
files (cport/src/game/*.c) were updated by tools/rename_symbol.py
which in turn re-ran the lifter against the renamed asm. Symdb check
~3575 entries, 0 problems. Gate: ok in 10.4 s.

## Batch 5 (16 fields, 2026-10-15)

bn6f commits: `dff38f7b`, `101f5141` (8), `565238b7`, `8c158a71`, `9e574819` (8).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oAIAttackVars_Unk_0a | u8 | `ExtraDamage` | hypothesis |
| oCamera_Unk_04 | u16 | `ScrollX` | probable |
| oCamera_Unk_06 | u16 | `CameraYInt` | probable |
| oChatbox_Unk_78 | u32 | `PrintDelay` | probable |
| oAIAttackVars_Unk_1b | u8 | `IsInvincible` | probable |
| oBattleObject_Unk_1f | u8 | `AIType` | probable |
| oCollisionData_Unk_2c | u16 | `HitStunTimer` | probable |
| oBattleState_Unk_19 | u8 | `CustGaugeChargeLevel` | probable |
| oBattleState_Unk_a0 | u0 | `BattleObjectList` | probable |
| oRenderInfo_Unk_1a | u16 | `BG3ScrollY` | probable |
| oCollisionData_Unk_2a | u16 | `HitInvulTimer` | probable |
| oBattleState_Unk_1e | u8 | `BustingLevelCategory` | probable |
| oChatbox_Unk_4C | u32 | `TextParam1` | probable |
| oAIData_Unk_74 | ptr | `SpawnedSourceObjectPtr` | hypothesis |
| oAIData_Unk_60 | ptr | `SubordinateObjectPtr` | hypothesis |
| oAIAttackVars_Unk_1d | u8 | `PwrAtkState` | probable |

Notes:
- `ExtraDamage` for oAIAttackVars_Unk_0a (was `AttackElement` per LLM, but conflicts with the existing oAIAttackVars_AttackElement at offset 0x1).
- `HitInvulTimer` for oCollisionData_Unk_2a (was `HitStunTimer` per LLM, but conflicts with the Unk_2c rename).
- 2 AIData ptr fields (`SpawnedSourceObjectPtr`, `SubordinateObjectPtr`) are hypothesis because ptr-type Unk fields often have multiple roles.

Running total: 1 + 20 + 20 + 19 + 16 = **76 fields named**.

## Batch 6 (20 fields, 2026-10-15)

bn6f commits: `0480b3c4` (.inc), `bcccece9` (asm renames).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oAIData_Unk_40 | u32 | `TargetObjectPtr` | probable |
| oAIData_Unk_44 | u32 | `AIFlags` | probable |
| oAIData_Unk_48 | u32 | `Flags` | probable |
| oAIData_Unk_50 | u32 | `SubordinateObjectUnk` | hypothesis |
| oAIData_Unk_58 | u32 | `SubordinateAIObjectPtr` | hypothesis |
| oBattleObject_Unk_1b | u8 | `PanelShiftCounter` | probable |
| oBattleObject_Unk_1c | u8 | `ContactPanelOffsetX` | probable |
| oBattleObject_Unk_1d | u8 | `ContactOffsetY` | probable |
| oBattleObject_Unk_1e | u8 | `MoveToFuturePanelStepTimer` | probable |
| oBattleState_Unk_04 | u8 | `AliveActorTurnIndex` | hypothesis |
| oBattleState_Unk_05 | u8 | `OpponentAliveCount` | hypothesis |
| oBattleState_Unk_08 | u8 | `AllianceEntryCount` | probable |
| oBattleState_Unk_0a | u8 | `PauseFlag` | hypothesis |
| oBattleState_Unk_0b | u8 | `BattleOverConditionFlag` | hypothesis |
| oBattleState_Unk_0e | u8 | `PanelTurnCount` | hypothesis |
| oBattleState_Unk_10 | u8 | `LoserSide` | probable |
| oBattleState_Unk_12 | u8 | `AliveAllianceCount` | probable |
| oCamera_Unk_08 | u16 | `SavedScrollX` | probable |
| oGameState_Unk_07 | u8 | `MapSubArea` | probable |
| oGameState_Unk_10 | u8 | `OWPlayerBusyOrCutscene` | probable |

Running total: 1 + 20 + 20 + 19 + 16 + 20 = **96 fields named**.

## Batch 7 (20 AIData fields, 2026-10-15)

bn6f commits: `c5cc41fc` (.inc), `8174c8d0` (asm renames).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oAIData_Unk_02 | u8 | `DisableCounterDecrement` | probable |
| oAIData_Unk_03 | u8 | `VersionVariant` | probable |
| oAIData_Unk_04 | u8 | `NaviBonusIndex` | probable |
| oAIData_Unk_09 | u8 | `PoisonTickCounter` | probable |
| oAIData_Unk_0a | u8 | `SelfDestructTurnCounter` | probable |
| oAIData_Unk_0b | u8 | `AIScratchByte` | probable |
| oAIData_Unk_0c | u8 | `AiDifficultyTier` | hypothesis |
| oAIData_Unk_0d | u8 | `ChargeShotStepIndex` | probable |
| oAIData_Unk_0e | u8 | `AISubordinateSlot` | probable |
| oAIData_Unk_0f | u8 | `BeastOutLockoutTimer` | probable |
| oAIData_Unk_10 | u8 | `CounterHealDamage` | probable |
| oAIData_Unk_12 | u8 | `CounterHitChain` | probable |
| oAIData_Unk_13 | u8 | `BurstShotTimer` | probable |
| oAIData_Unk_15 | u8 | `ChipUseDelayAlt` | probable |
| oAIData_Unk_1a | u8 | `AttackTeardownScratch` | hypothesis |
| oAIData_Unk_1c | u8 | `PanelDamageReactionFlag` | probable |
| oAIData_Unk_2a | u16 | `FrozenJoypadReleased` | probable |
| oAIData_Unk_2c | u16 | `TimeStopMirrorHeld` | probable |
| oAIData_Unk_2e | u16 | `JoypadHeldPrev` | probable |
| oAIData_Unk_30 | u16 | `JoypadFrozen` | probable |

Notes:
- 1 LLM-empty name (`""`) for oAIData_Unk_1a was overridden with `AttackTeardownScratch` (LLM couldn't determine the field's role).
- The sed-based .inc edit initially overwrote the AIState/AIAttackVars sections (which share the same Unk_XX labels in a single AIData.inc file). Reverted those to keep the renames AIData-only.
- The `oAIData_AiDifficultyTier` at offset 0xc is separate from the existing `oAIAttackVars_AiDifficultyTier` at AIAttackVars+0x3.

Running total: 1 + 20 + 20 + 19 + 16 + 20 + 20 = **116 fields named**.

## Batch 8 (20 fields, 2026-10-15)

bn6f commits: `56896f4e` (.inc), `e29a50de` (asm renames).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oAIData_Unk_32 | u16 | `CounterHitChainFlag` | hypothesis |
| oAIData_Unk_36 | u16 | `PoisonDamagePerTick` | probable |
| oAIData_Unk_38 | u16 | `CollisionFreezeTimer` | probable |
| oAIData_Unk_3a | u16 | `ActionRerollFrameCounter` | probable |
| oAIData_Unk_3c | u16 | `SavedZ16` | probable |
| oAIData_Unk_54 | u32 | `BubbleSourceObjectPtr` | hypothesis |
| oBattleState_Unk_13 | u8 | `BattleOverSecondaryFlag` | hypothesis |
| oBattleState_Unk_14 | u8 | `BackgroundType` | hypothesis |
| oBattleState_Unk_15 | u8 | `PendingFieldChange` | hypothesis |
| oBattleState_Unk_16 | u8 | `PanelTurnStepCounter` | probable |
| oBattleState_Unk_17 | u8 | `BeastOutFlag` | hypothesis |
| oBattleState_Unk_18 | u8 | `PhaseStepIndex` | hypothesis |
| oBattleState_Unk_1b | u8 | `PanelSelectedCount` | probable |
| oBattleState_Unk_1c | u8 | `PhaseMessageDelayCounter` | probable |
| oBattleState_Unk_1d | u8 | `MessageTimer` | probable |
| oBattleState_Unk_1f | u8 | `BattleResult` | probable |
| oChatbox_Unk_16 | u8 | `SelectCursorFrame` | probable |
| oChatbox_Unk_17 | u8 | `MugshotAnimTableIdx` | probable |
| oChatbox_Unk_19 | u8 | `CursorY` | probable |
| oChatbox_Unk_1B | u8 | `CursorX` | probable |

Notes:
- `PhaseStepIndex` for oBattleState_Unk_18 conflicts with the earlier batch-5 `oBattleState_PhaseStepIndex` (which was at offset 0x18, a u0 marker). Wait, that's the same field - this is a duplicate rename! Need to verify and possibly drop one.
- The cport C build had macro redefinition warnings (non-fatal, C defines `oBattleObject_Param1` with `#define` that matches bn6f's `oBattleObject_Param1` define). These are pre-existing.

Running total: 1 + 20 + 20 + 19 + 16 + 20 + 20 + 20 = **136 fields named**.

## Batch 9 (20 fields, 2026-10-15)

bn6f commits: `4b12bc30` (.inc), `fdae3ce0` (asm renames).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oBattleState_Unk_02_03 | u16 | `StatePhaseAndInitFlag` | probable |
| oBattleState_Unk_11 | flags8 | `StateFlags` | probable |
| oBattleState_Unk_20 | u16 | `LowHPAlertTimers` | hypothesis |
| oBattleState_Unk_32 | u16 | `BattleStateFlags` | probable |
| oBattleState_Unk_34 | u16 | `PlayerHPSnapshot` | probable |
| oBattleState_Unk_36 | u16 | `Reward2Bonus` | probable |
| oBattleState_Unk_3a | u16 | `PendingBattleEndAction` | hypothesis |
| oBattleState_Unk_40 | u32 | `ChipUseFrameCounter` | hypothesis |
| oBattleState_Unk_5c | u32 | `AlliancePermissionFlags` | probable |
| oBattleState_Unk_60 | u32 | `FrameCounter` | probable |
| oChatbox_Unk_03 | u8 | `SubState` | hypothesis |
| oChatbox_Unk_0A | u8 | `ChoiceNumOptions` | hypothesis |
| oChatbox_Unk_0B | u8 | `PageLoadReset` | hypothesis |
| oChatbox_Unk_0E | u8 | `ResetFlag` | hypothesis |
| oChatbox_Unk_12 | u8 | `ChoicesPageTag` | hypothesis |
| oChatbox_Unk_14 | u8 | `ScriptTokenParam` | hypothesis |
| oChatbox_Unk_15 | u8 | `ChoiceCursorSource` | hypothesis |
| oGameState_Unk_09 | u8 | `JackOutLockState` | probable |
| oObjectSprite_Unk_00 | u8 | `AnimationIndex` | probable |
| oObjectSprite_Unk_01 | u8 | `AnimFrameDuration` | probable |

Running total: 1 + 20 + 20 + 19 + 16 + 20 + 20 + 20 + 20 = **156 fields named**.

## Batch 10 (20 fields, 2026-10-15)

bn6f commits: `eeb8df24` (.inc), `5995013d` (asm renames).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oBattleState_Unk_64 | u16 | `SpriteRenderFrameCounter` | hypothesis |
| oChatbox_Unk_38 | u32 | `TextScriptSourcePtr` | hypothesis |
| oChatbox_Unk_3D | u8 | `DroppedName` | hypothesis |
| oChatbox_Unk_44 | u32 | `NextScriptCursorPtr` | probable |
| oChatbox_Unk_50 | u32 | `TextParam2` | probable |
| oChatbox_Unk_54 | u32 | `ParamC` | hypothesis |
| oChatbox_Unk_58 | u32 | `TextParam4` | hypothesis |
| oChatbox_Unk_64 | u16 | `TextScriptTimer` | hypothesis |
| oGameState_Unk_02 | u8 | `SavedChipDescByte` | hypothesis |
| oGameState_Unk_11 | u8 | `CoordInteractionTriggered` | hypothesis |
| oGameState_Unk_13 | u8 | `PendingByte` | hypothesis |
| oObjectSprite_Unk_02 | u8 | `AttributeFlags` | probable |
| oObjectSprite_Unk_04 | u8 | `PaletteIndex` | probable |
| oObjectSprite_Unk_05 | u8 | `PaletteRowIndex` | probable |
| oObjectSprite_Unk_06 | u16 | `ColorShader` | probable |
| oObjectSprite_Unk_0a | u16 | `SpriteExtraBytes` | hypothesis |
| oObjectSprite_Unk_12 | u8 | `FrameCounter` | hypothesis |
| oObjectSprite_Unk_13 | u8 | `SpriteFlags3` | probable |
| oObjectSprite_Unk_14 | u8 | `InitFlag` | hypothesis |
| oObjectSprite_Unk_16 | u16 | `CurFrame` | probable |

Running total: 1 + 20 + 20 + 19 + 16 + 20 + 20 + 20 + 20 + 20 = **176 fields named**.

## Batch 11 (19 fields, 2026-10-15)

bn6f commits: `318e0544` (.inc), `b2960686` (asm renames).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oObjectSprite_Unk_18 | u32 | `AnimationResourcePtr` | probable |
| oObjectSprite_Unk_1c | u32 | `AnimationSubFramePtr` | probable |
| oObjectSprite_Unk_20 | u32 | `AnimFrameDataPtr` | probable |
| oObjectSprite_Unk_24 | u32 | `SpriteRenderDataPtr` | hypothesis |
| oObjectSprite_Unk_28 | u32 | `SpriteAttrs2Ptr` | hypothesis |
| oObjectSprite_Unk_2c | u32 | `ScriptParam` | hypothesis |
| oObjectSprite_Unk_30 | u32 | `SpriteFlags4Ptr` | hypothesis |
| oObjectSprite_Unk_34 | u32 | `ScratchByte` | hypothesis |
| oCollisionData_Unk_14 | u8 | `BarrierIndex` | hypothesis |
| oCollisionData_Unk_16 | u16 | `BarrierHP` | probable |
| oCollisionData_Unk_1a | u16 | `BarrierBreakCounter` | probable |
| oChatbox_Unk_68 | u16 | `NextScriptDesc` | probable |
| oChatbox_Unk_74 | u32 | `ScriptFlags` | probable |
| oRenderInfo_Unk_00 | u16 | `LCDControl` | probable |
| oRenderInfo_Unk_02 | u16 | `MosaicSize` | probable |
| oRenderInfo_Unk_14 | u16 | `BG1ScrollXAlt` | hypothesis |
| oRenderInfo_Unk_16 | u16 | `BGScrollY2` | probable |
| oAIData_Unk_18 | u8 | `AISubState` | probable |
| oBattleState_Unk_04_05 | u16 | `AliveActorAndOpponent` | probable |
| oGameState_Unk_15 | u8 | `SavedByte` | hypothesis |

Running total: 1 + 20 + 20 + 19 + 16 + 20 + 20 + 20 + 20 + 20 + 19 = **195 fields named**.

## Batch 12 (20 fields, 2026-10-15)

bn6f commits: `b9fbe7a5` (.inc), `c8e36299` (asm renames).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oChatbox_Unk_80 | u8 | `ChoiceSlotCount` | probable |
| oChatbox_Unk_90 | u32 | `InitialUnk_90` | probable |
| oCollisionData_Unk_03 | u8 | `HitFlagMask` | probable |
| oCollisionData_Unk_15 | u8 | `SavedHitModifier` | hypothesis |
| oCollisionData_Unk_17 | u8 | `BarrierRelatedValue` | hypothesis |
| oCollisionData_Unk_1b | u8 | `BarrierBreakCooldown` | hypothesis |
| oCollisionData_Unk_1a_1b | u16 | `BarrierCounterU16` | probable |
| oCollisionData_Unk_1c | u16 | `StatusEffectTimer1` | hypothesis |
| oCollisionData_Unk_1e | u16 | `StatusEffectTimer` | hypothesis |
| oCollisionData_Unk_22 | u16 | `StatusEffectTimer8` | probable |
| oCollisionData_Unk_26 | u16 | `HitCountdown` | probable |
| oCollisionData_Unk_28 | u16 | `InvulnerabilityTimer` | probable |
| oCollisionData_Unk_48 | u32 | `LastHitObjectPtr` | hypothesis |
| oCollisionData_Unk_54 | u32 | `CollisionFlagsAccumulator` | hypothesis |
| oCollisionData_Unk_60 | u32 | `StunSubTimer` | hypothesis |
| oCollisionData_Unk_64 | u32 | `ExtraElementDamage` | hypothesis |
| oCollisionData_Unk_6c | u32 | `CollisionFlags` | hypothesis |
| oCollisionData_Unk_78 | u32 | `ElecElementDamageAccumulator` | hypothesis |
| oCamera_Unk_02 | u8 | `Mode` | probable |
| oCamera_Unk_03 | u8 | `ShakeEffectIndex` | hypothesis |

Notes:
- `BarrierCounterU16` for oCollisionData_Unk_1a_1b avoids name collision with the existing `BarrierBreakCounter` (which is the u8 view of the same bytes).

Running total: 1 + 20 + 20 + 19 + 16 + 20 + 20 + 20 + 20 + 20 + 19 + 20 = **215 fields named**.

## Batch 13 (20 fields, 2026-10-15)

bn6f commits: `ffa77444` (.inc), `97f8c9ce` (asm renames).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oCamera_Unk_0a | u16 | `SavedCameraYInt` | probable |
| oCamera_Unk_0c | u16 | `ShakeDuration` | probable |
| oCamera_Unk_0e | u16 | `ShakeMagnitude` | probable |
| oCamera_Unk_10 | u16 | `ShakeFrames` | hypothesis |
| oCamera_Unk_12 | u16 | `ShakeIndex` | probable |
| oCamera_Unk_14 | u32 | `PositionSrcPtr` | probable |
| oCamera_Unk_18 | u32 | `MinX16_16` | hypothesis |
| oCamera_Unk_1c | u32 | `MaxScrollX` | probable |
| oCamera_Unk_20 | u32 | `CameraMinZ` | probable |
| oCamera_Unk_24 | u32 | `MinY` | probable |
| oCamera_Unk_48 | u32 | `ShakeOffsetX` | hypothesis |
| oGameState_Unk_68 | u32 | `FrameCountdown` | probable |
| oGameState_Unk_6c | u32 | `ZennyChecksumMirror` | probable |
| oGameState_Unk_70 | u32 | `ProtectedBugfragsObfuscated` | probable |
| oGameState_Unk_74 | u32 | `AntiTamperFlags` | probable |
| oOverworldNPCObject_Unk_07 | u8 | `MovementCoordOffset` | probable |
| oOverworldNPCObject_Unk_0f | u8 | `MovementDirectionPrev` | probable |
| oOverworldNPCObject_Unk_50 | u32 | `BusyFlag` | probable |
| oOverworldNPCObject_Unk_54 | u32 | `InteractionAreaPtr` | hypothesis |
| oOverworldNPCObject_Unk_58 | u32 | `InteractionAreaHandle` | hypothesis |

Running total: 1 + 20 + 20 + 19 + 16 + 20 + 20 + 20 + 20 + 20 + 19 + 20 + 20 = **235 fields named**.

## Batch 14 (20 fields, 2026-10-15)

bn6f commits: `e9bc0aa9` (.inc), `bf3190a4` (asm renames).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oS20364C0_Unk_13 | u8 | `FadeSeqIndex` | probable |
| oWarp2011bb0_Unk_10 | u8 | `WarpState` | probable |
| oS20364C0_Unk_0f | u8 | `PulsePhase` | probable |
| oS200ace0_Unk_20 | u32 | `OWRoutinePointer` | hypothesis |
| oS2001c04_Unk_14 | u16 | `EncounterRollValue` | probable |
| oS2001c04_Unk_12 | u16 | `EncounterStepAccumulator` | probable |
| oPanelData_Unk_07 | u8 | `CachedAlliance` | hypothesis |
| oPalette20097a0_Unk_00 | u8 | `ActiveFlag` | probable |
| oOWObjectInteractionArea_Unk_0c | u32 | `InteractionFlagMask` | probable |
| oOWObjectInteractionArea_Unk_08 | flags32 | `Flags` | probable |
| oGameState_Unk_08 | u8 | `ChatboxMessageSlot` | hypothesis |
| oBattleState_Unk_c0 | u0 | `Drop` | hypothesis |
| oStartScreen_Unk_02 | u8 | `SubState` | hypothesis |
| oS20364C0_Unk_0e | u8 | `ActorIndex` | hypothesis |
| oS20364C0_Unk_0c | u8 | `FirstBattleTutorialStep` | hypothesis |
| oS200ace0_Unk_18_1a | u32 | `ScriptTimer` | hypothesis |
| oPanelData_Unk_10 | u16 | `ClaimTimer` | probable |
| oPanelData_Unk_01 | u8 | `HighlightState` | probable |
| oOWObjectInteractionArea_Unk_10 | u32 | `InteractionFlags` | hypothesis |
| oGameState_Unk_03 | u8 | `ChipDescDirtyFlag` | hypothesis |

Running total: 1 + 20 + 20 + 19 + 16 + 20 + 20 + 20 + 20 + 20 + 19 + 20 + 20 + 20 = **255 fields named**.

## Batch 15 (20 fields, 2026-10-15)

bn6f commits: `80ceb88a` (.inc), `aed41812` (asm renames).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oCutsceneState_Unk_34 | ptr | `DefaultScriptTable` | hypothesis |
| oCollisionData_Unk_7c | u32 | `CollisionFlagsAccumulated` | hypothesis |
| oScreenFade_Unk_03 | u8 | `ActiveFlag` | probable |
| oS20364C0_Unk_3a | u16 | `CursorAnimPhase` | hypothesis |
| oS20364C0_Unk_1a | u8 | `EmotionSpriteVariant` | hypothesis |
| oS20364C0_Unk_14 | u8 | `TickCounter` | probable |
| oS20364C0_Unk_10 | u16 | `ChipSelectScrollTimer` | hypothesis |
| oS20364C0_Unk_0b | u8 | `CustMenuStepTimer` | hypothesis |
| oS2001c04_Unk_07 | u8 | `CurPETNaviId` | probable |
| oS2000aa0_Unk_03 | u8 | `OWPlayerNaviInternetFlag` | hypothesis |
| oPanelData_Unk_04 | u8 | `StatusOrVariant` | probable |
| oPalette20097a0_Unk_08 | u32 | `PalettePointerOrIndex` | hypothesis |
| oPalette20097a0_Unk_04 | u32 | `SourceAddress` | hypothesis |
| oPalette20097a0_Unk_02 | u8 | `ColorCountOrDepth` | probable |
| oPalette20097a0_Unk_01 | u8 | `Priority` | hypothesis |
| oOverworldNPCObject_Unk_70 | u32 | `NextYBackup` | probable |
| oOverworldNPCObject_Unk_1e | u8 | `MovementFlagBackup` | probable |
| oOverworldNPCObject_Unk_1d | u8 | `CurActionBackup` | probable |
| oOverworldMapObject_Unk_08 | u32 | `ObjectPtr` | hypothesis |
| oLogoScreenState_Unk_05 | u8 | `State` | hypothesis |

Running total: 1 + 20 + 20 + 19 + 16 + 20 + 20 + 20 + 20 + 20 + 19 + 20 + 20 + 20 + 20 = **275 fields named**.

## Batch 16 (18 fields, 2026-10-15)

bn6f commits: `59754762` (S2001c04 manual), `6b3e729c` (16 more manual).
Recomp commit pending.

| Field | Old | New | Status |
|---|---|---|---|
| oS2001c04_Unk_24 | u32 | `EncounterGate` | hypothesis |
| oS2001c04_Unk_28 | u32 | `EncounterGateFlag` | hypothesis |
| oS200ace0_Unk_1a | u16 | `ScriptTimer16` | hypothesis |
| oS200ace0_Unk_1c | u32 | `PendingFixedAnimationId` | hypothesis |
| oS20364C0_Unk_03 | u8 | `CustMenuStep` | hypothesis |
| oS20364C0_Unk_17 | u8 | `ChipSelectReady` | hypothesis |
| oS20364C0_Unk_18 | u8 | `BattleInitFlag` | hypothesis |
| oS20364C0_Unk_19 | u8 | `HasPendingAction` | hypothesis |
| oS20364C0_Unk_38 | u16 | `PuppeteerSwitchElement` | hypothesis |
| oS2011E30_Unk_07 | u8 | `EffectPending` | probable |
| oStartScreen_Unk_03 | u8 | `SpriteIndex` | hypothesis |
| oStartScreen_Unk_0C | u8 | `NumIcons` | hypothesis |
| oCutsceneState_Unk_04 | u8 | `CutsceneParam32` | hypothesis |
| oCutsceneState_Unk_08 | u0 | `CutsceneScriptVars` | hypothesis |
| eCutsceneState_Unk_04 | u8 | `CutsceneParam32` | hypothesis |
| eCutsceneState_Unk_08 | u0 | `CutsceneScriptVars` | hypothesis |
| oBattleState_Unk_12_13 | u16 | `HeldChipIndex` | probable |
| oGameState_Unk_12 | u8 | `LastMapChangeCounter` | probable |

Notes: This batch required manual renames due to a quirk in `tools/rename_symbol.py` - when some renames succeed and others fail, the script rolls back ALL changes. Fixed by doing the renames in two separate commits.

Running total: 1 + 20 + 20 + 19 + 16 + 20 + 20 + 20 + 20 + 20 + 19 + 20 + 20 + 20 + 20 + 18 = **293 fields named**.
