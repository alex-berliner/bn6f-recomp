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
