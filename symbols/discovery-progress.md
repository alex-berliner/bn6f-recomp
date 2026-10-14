# Discovery progress

Goal: name the sub_XXXXXXX placeholders that the recorded routes actually run
(beastsp route priority: battle, chips, encounters, map objects, text/chatbox,
menus), in batches of ~25. Method per batch: pick route-run placeholders,
build context (assembly, named callers/callees, db summaries, contracts,
captured calls, structs), ask the outside model (tools/llm.py chain:
hyper/glm-5.3-flash, then hyper/qwen3.8-flash, then minimax/MiniMax-M3),
verify every claim against the real assembly/coverage, write symbols/db
entries, rename probable ones via ./wf record, gate per batch, commit.

Scratch helpers (gitignored): build/repro/discover/{pick,ctx,driver,check,family,write_entries}.py

## Tally

| batch | functions examined | named (renamed via record) | hypothesis-only | dropped |
|-------|--------------------|---------------------------|-----------------|---------|
| 1     | 25                 | 25                        | 0               | 0       |

Running total: named 25, hypothesis-only 0, dropped 0 (of ~10,600 placeholders).

## Batch 1 (2026-10-14)

25 functions, all probable/confirmed, renamed in recomp 2481925c (bn6f
b29fc142 comments, 6ea2baf9 renames; port ROM unchanged):

- battle per-frame collision/damage pipeline family: battleObject_processCollisionResults_801A9B8,
  battleObject_processCollisionResultsByMode_801AAC0 (skips battle_tickAiSelfDrain
  in battle mode 0xA), battleObject_processCollisionResultsForPlayer_801AC6C
  (from playerObject_update_80EA484), battleObject_processCollisionResultsForEnemy_801ABB8
  (from battle_8108F74)
- battleObject_dispatchActionHandler_801B394 (applies pending FinalDamage:
  white flash, SFX 0x85, HP subtract, defeat/status cascade; then dispatches
  the caller's CurAction handler table)
- battleObject_actionFlinch_80174FE (CurAction 3 "on flinch": AI dispatch,
  commits the pushed-back FuturePanel, anim 1, 23 frames, ends with CurAction 8)
- battleObject_initNaviObject_8016F56 (init state of the navi battle object:
  sprite/palette/flip from enemy data, element + secondary weakness, collision
  from UnkFlags_03 bits, name popup, CUR_STATE_UPDATE)
- battle_runTransformationIntroStep_8014BEE (transformation intro, state 2 of
  off_8014AB4; per-form dispatch of 24 handlers, SFX 0x8D + log-in)
- attack-object inits: battleObject_initPanelBreakBounce_80CA568,
  battleObject_initFastLunge_80CE554, battleObject_initTrainArrowVariant_80C60CC,
  battleObject_initBriefCollisionObject_80C9BE8, battleObject_initRisingShot_80CAD4C
- t1_0x54_updateFromRelatedObject_80C3F52 / t1_0x55_updateFromRelatedObject_80C4146
- battle_updateEStructSprites_80465F8 (15x0x58 eStructArr2008450 sprite pass),
  battle_resetEStructRecords_8046664 (hypothesis -> probable), battle_initCustomScreen_8026840
- owPlayer_update_809D348, gamestate_openPetMenuOnStartPress_8005AF4,
  pet_menuUpdateAndHandleExit_8005462, shopBuyMenu_updateFrame_8047134,
  startScreen_updateFrame_802F8D8 (caller evidence added),
  sprite_queueOamEntryThunk_802FE28 (pending rename honoured),
  cutscene_TryStartOnMapEnterAfterJackIn_8034FB8

Corrections made to model claims during verification: inverted panel-bit logic
in sub_80CA568 (crack when bit 0x10 CLEAR or occupancy bits set), swapped table
indices in sub_802F8D8, dropped a wrong "returns 0x80" claim for sub_809D348
(that r0 is caller residue), reinterpreted sub_80174FE from "AI move action" to
the flinch action handler per the existing '// on flinch // 0x03' annotation.

pending_renames.txt: removed the 2 resolved lines (sub_802FE28, sub_801AC6C);
829 lines remain.

## Batch 2

Not started.

## Batch 2 (2026-10-14)

21 functions, 20 named + 1 hypothesis (sub_80C57F4 kept as comment only), renamed
in recomp eba7a4a7 (bn6f 4daef61b comments + renames; port ROM unchanged):

- the player-side anger-state maintenance (battle_updatePlayerAngerState_8014326);
- the collision-hit application (object_applyCollisionHit_801AD9E) and the
  selected-chip name+damage drawer (battle_drawSelectedChipNameAndDamage_801C6EE);
- the player's paralyze-action handler (battleObject_actionParalyze_80175B8);
- four table-driven battle-object init handlers
  (battleObject_initAttackFromDefTable_80C4E7C, battleObject_initProjectileFromTable_80CA2D4,
  battleObject_initSpriteAndCollision_80C8708, battleObject_syncSpriteWithRelatedObject_80C43C4);
- main-flow ticks (battle_updateMainFlow_80080D2) and victory phase (battle_runVictorySequence_80081A4);
- chip-selection turn transition (battle_endChipSelectionBeginTurn_8026DC4), eStruct
  record spawner (battle_spawnEStructRecordsFromTable_80465A0), shop init (shop_initScreen_8046D4C),
  battle-intro spawn step (battle_runBattleIntroSpawnStep_80091F0);
- overworld map-object draw step (owMapObject_queueSprites_8004934) and overworld scene
  tick (gamestate_runOverworldSceneStep_80055CE);
- PET navi-select screen update (pet_updateNaviSelectScreen_811EDB4) and PET navi-display
  scene update (battle_petNaviDisplayUpdate_8126CC0);
- AI attack-sequence step (battleAi_spawnTempAttackObjectStep_80EB862) and AI charged-attack
  step (battleAi_chargeThenFireAttackStep_80EBC28).

tool: tools/asmcat.py (recreated after rebuild) — evidence reproduce commands take the
address, resolve the current name from the lifter's header comments.

## Tally update

| batch | functions examined | named (renamed via record) | hypothesis-only | dropped |
|-------|--------------------|---------------------------|-----------------|---------|
| 1     | 25                 | 25                        | 0               | 0       |
| 2     | 21                 | 20                        | 1               | 0       |

Running total: named 45, hypothesis-only 1, dropped 0 (of ~10,600 placeholders).

## Batch 3 (2026-10-14)

25 functions, 23 named + 2 hypothesis (sub_800A01C, sub_801002C kept as comment
only), renamed in recomp 3dbe05db (bn6f 0fc2dd04 comments + renames; port ROM unchanged):

- sprite subsystem: renderAll and palette-count reset
  (sprite_resetPaletteCount_800286C, sprite_setRenderFlushState_80027B4,
  sprite_finalizeBattleSpriteGroupQueue_8004298, sprite_finishGroup_8004590);
- overworld: the per-frame overworld render over the OWPlayerObject list
  (owMapObject_renderAll_8003BF4) and the type-dispatch object router
  (object_dispatchByType_8003AFA);
- battle-object layer: barrier-state update
  (battleObject_updateBarrierStatus_801A802), emotion-shader trigger
  (battleObject_applyEmotionShaderIfCounterReady_8016CA4), collision-hit clear
  (battleObject_clearCollisionIfHitAction_801BADE), terminate flag dispatch
  (battleObject_checkTerminateFlagAndDispatch_8020140), counter-timer tick
  (battleObject_tickCounterTimer_801A420), custom-chip-cursor OAM setup
  (battleObject_setupCustomChipCursorState_803C59C);
- panel/status effects on objects: ice-freeze tick
  (object_tickIcePanelFreeze_801A36A), poison-panel status
  (object_updatePoisonPanelStatus_801A186), air-shoe clear
  (battleObject_clearChipSelectionOnAirShoe_801A2CC);
- battle/AI attack queue: queued-attacks dispatch
  (battle_ai_dispatchQueuedAttacks_8017BC0) and AI player use-chip action
  (battle_aiPlayerUseChipAction_8017AB4);
- battle HUD/draw: chip-counts OAM (battle_drawChipCountsOAM_801C082),
  custom-gauge sync (battle_updateCustGaugeFromAI_802E156),
  battle-start overlay (battle_drawBattleStartOverlay_8033F80),
  OAM queue-all (battle_queueAllOamEntries_8009FCC);
- flagged-update dispatch (battle_dispatchFlaggedUpdates_801BEE0);
- one OAM/queue helper (object_computeAndQueueOamEntry_811FA98).

Hypothesis entries: sub_800A01C (resets byte_3000EA8 - 1 instruction),
sub_801002C (transform-aware palette row picker for AI-controlled player
objects - too many unverified struct field names to commit to a specific
name).

tool: continued from batch 2 (tools/asmcat.py).

## Tally update

| batch | functions examined | named (renamed via record) | hypothesis-only | dropped |
|-------|--------------------|---------------------------|-----------------|---------|
| 1     | 25                 | 25                        | 0               | 0       |
| 2     | 21                 | 20                        | 1               | 0       |
| 3     | 25                 | 23                        | 2               | 0       |

Running total: named 68, hypothesis-only 3, dropped 0 (of ~10,600 placeholders).

## Batch 4 (2026-10-14)

25 functions, 21 named + 4 hypothesis (sub_80007BE, sub_801002C, sub_8033B80,
sub_8083DF0 kept as comment only), renamed in recomp ddca5267 (bn6f e008e389
comments + renames; port ROM unchanged).

Highlights:
- sprite/render layer: reset and finish helpers (battle_resetSpritePaletteCount_8003C70,
  battle_renderObjectSprites_8004218, battle_updateSprites_8004510,
  battleObject_finishQueuedSpriteGroup_80049B0);
- overworld render/tick: overworld_owMap_renderAndTickSubsystems_800536E;
- FIFO helper: fifo_resetWriteCounter_800A01C (companion to the
  fifo_pushRecord16_8009FF8 named in batch 3);
- battle/AI layer: object-status timers (object_tickStatusEffectTimers_800E730),
  player-by-alliance finder (battle_findPlayerByAlliance_800EB6C),
  try-execute attack-chip (playerAI_tryExecuteAttackChip_800FB54),
  battleObject player-AI update (battleObject_playerAIUpdate_8012E74),
  battleObject on-collision (battleObject_onCollision_801A200),
  chip-preview GFX load (battle_loadChipPreviewGfxFromHandTable_801C002),
  AI pwr-atk-presses tick (battle_ai_tickPwrAtkPresses_801CC94),
  panel-grid finalize (battle_panelGridFinalizeOrPickDimension_800C488);
- camera: in-front-of-camera predicate (camera_isObjectInFrontOfCamera_80269D0);
- overworld player: collision/pos update (ow_player_updateCollisionAndPos_809D800),
  jump-landing accumulate facing (owPlayer_checkJumpLandingAccumulateFacing_809DFC0),
  busy/cutscene-active gate (owPlayer_isBusyOrCutsceneActive_809E462);
- battle object lookup: by-index (battleObject_getByIndex_813B780);
- audio: m4a MPlay track-end trampoline (m4a_mplay_trackEndJumpStub_814E4CC);
- RFU wireless: send-data tail-call thunk (rfufunc_tailCallStub_814DA64).

Hypothesis entries:
- sub_80007BE: sound_dispatchAndPlayPendingSoundRequest_80007BE (not enough
  evidence - the function's interaction with the sound queue isn't yet visible
  from the assembly).
- sub_801002C: battleObject_computePaletteSlotForNavi_801002C (carried from
  batch 3 - too many unverified struct field names to commit).
- sub_8033B80: battle_queueBattleStartingOamEntry_8033B80 (the OAM animation
  pointer table is not yet documented).
- sub_8083DF0: reqBBS_init_gfxAndPostBBSFlags_8083DF0 (body in a region the
  lifter could not disassemble; callee set suggests ReqBBS init/update but
  the function's exact role cannot be verified).

Note: pre-existing leftover failure in cport/src/game/asm32.c:185 is not in
this commit's scope (the call there predates batch 1).

## Tally update

| batch | functions examined | named (renamed via record) | hypothesis-only | dropped |
|-------|--------------------|---------------------------|-----------------|---------|
| 1     | 25                 | 25                        | 0               | 0       |
| 2     | 21                 | 20                        | 1               | 0       |
| 3     | 25                 | 23                        | 2               | 0       |
| 4     | 25                 | 21                        | 4               | 0       |

Running total: named 89, hypothesis-only 7, dropped 0 (of ~10,600 placeholders).

## Batch 5 (2026-10-14)

25 functions, 20 named + 5 hypothesis (sub_80027C4, sub_801B9BC, sub_8033B80,
sub_8083DF0, sub_81196A0 kept as comment only), renamed in recomp 2eda8b0a
(bn6f 4ab63ce3 comments + renames; port ROM unchanged).

Highlights:
- main loop / system: pending-request drain and sound forward
  (main_drainPendingRequestQueueAndForwardSoundReq_80007BE);
- sprite: queue-frame-pieces sub (sprite_queueFramePiecesViaSub_8002694);
- battle object layer: AI category classify (battle_aiClassifyChipCategory_8012A38),
  power-attack charge update (battle_ai_updatePowerAttackCharge_8012EBC),
  object poison-tick (battleObject_processPoisonTick_801026A), anim/state update
  (battleObject_updateAnimAndState_801CADC), visual-state sync
  (battleObject_syncVisualStateFromRelatedObject_80B863A), invincibility destroy
  handler (battleObject_handleDestroyByInvincibility_80D44D2),
  charge-shot play-charge (battleObject_chargeShotChargeObject_playChargeSounds_80E0F5E);
- battle HUD: timer tile draw (battle_drawTimerTiles_801C4E4),
  battle-busy predicate (battle_isInputAllowedOrBattleBusy_80A7C58);
- battle/AI dispatch: advance Unk40 if flag set
  (battle_advanceUnk40IfFlagSet_800A6A6), object-update/effect dispatch
  (battle_dispatchObjectUpdateOrEffect_81333A0);
- cutscene: jack-in/jack-out try (cutscene_tryJackInOrJackOut_8034CB6);
- overworld player: run/slip/run engine (owPlayer_runSlipRunOrEngine_0809D61A);
- overworld map object: player interact (owMapObject_handlePlayerInteract_809D9E0),
  elevator hover (owMapObject_isHoveringElevator_0809E3A2);
- navi palette picker (battle_pickPaletteIndexForNavi_801002C - upgraded from
  hypothesis now that the called sub_800FD0A is identified);
- m4a audio: ply trampoline (m4a_ply_trampoline_814ECA0);
- RFU wireless: call-func-by-ptr stub (rfu_callFuncByPtr_814DA68).

## Tally update

| batch | functions examined | named (renamed via record) | hypothesis-only | dropped |
|-------|--------------------|---------------------------|-----------------|---------|
| 1     | 25                 | 25                        | 0               | 0       |
| 2     | 21                 | 20                        | 1               | 0       |
| 3     | 25                 | 23                        | 2               | 0       |
| 4     | 25                 | 21                        | 4               | 0       |
| 5     | 25                 | 20                        | 5               | 0       |

Running total: named 109, hypothesis-only 12, dropped 0 (of ~10,600 placeholders).
