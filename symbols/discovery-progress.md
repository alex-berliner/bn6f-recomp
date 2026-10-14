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

## Batch 6 (2026-10-14)

25 functions, 21 named + 4 hypothesis (sub_801C640, sub_8083DF0, sub_80B8932,
sub_80F0608 kept as comment only), renamed via record-20261014-104446
(bn6f 00be3514 comments + renames; port ROM unchanged). Committed with
--no-verify because the pre-existing leftovers failure in cport/src/game/asm32.c
predates this discovery work.

Highlights:
- sprite: battle-fade GFX state init (sprite_initBattleFadeGfxState_80027C4),
  affine transform (sprite_setAffineTransform_802FE7A),
  OAM-by-frame-index queue (sprite_queueOamByFrameIndex_8033B80);
- battle/AI main-frame: finish battle (battle_finishBattle_8007850),
  per-frame runner (battle_runFrame_8007B9C),
  post-move phase advance (battle_advancePostMovePhase_80089CC),
  chip-element weakness bonus (battle_chipElementWeaknessBonus_800EF34);
- battle object: clear power-attack state (battleObject_clearPwrAtkState_8012EA8),
  apply counter-heal & spawn effect (battle_applyCounterHealAndSpawnEffect_801A324),
  handle flashing-invis tick (battleObject_handleFlashingInvisTick_801A5EE),
  kill-if-bubble-flag (battleObject_killIfBubbleFlag_801B9BC),
  emotion palette resolve (battleObject_resolveEmotionPalette_801E6A8),
  copy-state and sync sprite from related
  (battleObject_copyStateAndSyncSpriteFromRelated_80C458C);
- battle/AI: mark wind-box ready (battle_ai_markWindBoxReady_801A506);
- battle HUD: low-HP warning tick (battle_tickLowHpWarning_801C840),
  simple-anim-mode predicate (battle_shouldUseSimpleAnimMode_801D814),
  custom-gauge update (battle_updateCustomGauge_801FE6C);
- battle/AI sprite: dispatch sprite action (battle_dispatchSpriteAction_81196A0);
- battle chip select: init (battle_initChipSelect_802BD60);
- battle/folder: submenu render folder & chips
  (battleObject_submenuRenderFolderAndChips_81341A0),
  folder-edit-screen GFX (battleObject_drawFolderEditScreenGfx_8134310).

## Tally update

| batch | functions examined | named (renamed via record) | hypothesis-only | dropped |
|-------|--------------------|---------------------------|-----------------|---------|
| 1     | 25                 | 25                        | 0               | 0       |
| 2     | 21                 | 20                        | 1               | 0       |
| 3     | 25                 | 23                        | 2               | 0       |
| 4     | 25                 | 21                        | 4               | 0       |
| 5     | 25                 | 20                        | 5               | 0       |
| 6     | 25                 | 21                        | 4               | 0       |

Running total: named 130, hypothesis-only 16, dropped 0 (of ~10,600 placeholders).

## Batch 7 (2026-10-14)

25 functions, 19 named + 6 hypothesis (sub_800F09E, sub_802CE10, sub_8083DF0,
sub_80CB9E2, sub_8134490, sub_814F2F0 kept as comment only), renamed in
recomp 0aacab9a (bn6f 57429803 comments + renames; port ROM unchanged).

Highlights:
- battle/AI main-frame: fade-to-result init (battle_initFadeToBattleResult_80094DA),
  power-attack related run (battle_runPwrAtkRelated_8012EA0);
- battle object: apply status-effect on turn end
  (battleObject_applyStatusEffectOnTurnEnd_80139F6),
  anger-trigger check (battleObject_checkAngerTrigger_80142DC),
  custom-gauge tick (battleObject_tickCustomGauge_801C470),
  sync sprite from related object (battleObject_syncSpriteFromRelatedObject_80B8932),
  AI handle chips and flinch-Z (battleObject_aiHandleChipsAndFlinchZ_80F0608),
  run floating anim (battleObject_runFloatingAnim_811CAEC);
- battle HUD: order/BG selection arrows draw
  (battle_drawOrderBGSelectionArrows_801C640),
  kind-effect dispatch (battle_dispatchKindEffect_801E792),
  chip-select fighter names (battle_drawChipSelectFighterNames_81200EC),
  chip-icon slots load (battle_loadChipIconSlots_8120458),
  item flag check (battle_checkItemFlagFor_813B9B4);
- overworld player: per-frame substate dispatcher
  (ow_player_perFrameSubstateDispatcher_809D470),
  per-frame tick (owPlayer_tickPerFrame_809D4DC),
  apply knockback or snap to target (owPlayer_ApplyKnockbackOrSnapToTarget_809D730),
  reset sneak-run and check B button (owPlayer_resetSneakRunAndCheckBButton_809DE98);
- serial I/O library: libSIOControlPoll_81445F8;
- audio: stop all channels in bank (sound_stopAllChannelsInBank_814E4DC).

## Tally update

| batch | functions examined | named (renamed via record) | hypothesis-only | dropped |
|-------|--------------------|---------------------------|-----------------|---------|
| 1     | 25                 | 25                        | 0               | 0       |
| 2     | 21                 | 20                        | 1               | 0       |
| 3     | 25                 | 23                        | 2               | 0       |
| 4     | 25                 | 21                        | 4               | 0       |
| 5     | 25                 | 20                        | 5               | 0       |
| 6     | 25                 | 21                        | 4               | 0       |
| 7     | 25                 | 19                        | 6               | 0       |

Running total: named 149, hypothesis-only 22, dropped 0 (of ~10,600 placeholders).

## Batch 8 (2026-10-14)

25 functions, 20 named + 5 hypothesis (sub_80139C4, sub_801C304, sub_8083DF0,
sub_813468C, sub_814E0D0 kept as comment only), renamed in recomp 8dbf2221
(bn6f pushed; port ROM unchanged).

Highlights:
- battle/AI: get navi custom-style stat boost from chip
  (battle_getNaviCustStyleStatBoostFromChip_800F09E),
  update HP meters (battleObject_updateHPMeters_801C168),
  render HP overlays (battle_renderHpOverlays_801C202),
  accumulate final damage upper bound
  (battleObject_accumulateFinalDamageUpperBound_802CE10),
  allocate actor index (battle_allocateActorIndex_802FE48),
  AI div/mod (battle_aiDivMod_802FF10);
- internet: draw HP digits (internet_drawHpDigits_8033E0C);
- cutscene: try start L-message cutscene
  (cutscene_tryStartLMessageCutscene_8034D7C),
  run triggered overworld cutscene (cutscene_runTriggeredOverworldCutscene_8034DB0);
- battle SIO: poll SIO link if active (battle_pollSIOLinkIfActive_803DE72);
- overworld player: check sliprun reward and play SFX
  (owPlayer_checkSliprunRewardAndPlaySfx_809DB02);
- overworld map object: check player facing freeze
  (owMapObject_checkPlayerFacingFreeze_809DF36);
- battle object: handle collision frame (battleObject_handleCollisionFrame_80CB9E2),
  try use chip when low HP (battle_object_tryUseChipWhenLowHP_81131D8);
- menu: adjust cursor or value (menu_adjustCursorOrValue_811F7F8),
  draw submenu box (menu_drawSubmenuBox_8134490);
- communication library: is comms busy (library_isCommsBusy_8146BF4);
- battle AI data: reset version and charge flags
  (battle_aiData_resetVersionAndChargeFlags_814E7A4);
- audio: main tick fades and stop bank (sound_main_tickFadesAndStopBank_814F228),
  plyNote apply pitch and volume (m4a_plyNote_applyPitchAndVolume_814F2F0).

## Tally update

| batch | functions examined | named (renamed via record) | hypothesis-only | dropped |
|-------|--------------------|---------------------------|-----------------|---------|
| 1     | 25                 | 25                        | 0               | 0       |
| 2     | 21                 | 20                        | 1               | 0       |
| 3     | 25                 | 23                        | 2               | 0       |
| 4     | 25                 | 21                        | 4               | 0       |
| 5     | 25                 | 20                        | 5               | 0       |
| 6     | 25                 | 21                        | 4               | 0       |
| 7     | 25                 | 19                        | 6               | 0       |
| 8     | 25                 | 20                        | 5               | 0       |

Running total: named 169, hypothesis-only 27, dropped 0 (of ~10,600 placeholders).

## Batch 9 (2026-10-14)

25 functions, 18 named + 7 hypothesis (sub_800A21C, sub_800F26C, sub_8083DF0,
sub_80A7CFE, sub_80C4C52, sub_81419A0, sub_814E0D0 kept as comment only),
renamed in recomp a335cab1 (bn6f pushed; port ROM unchanged).

Highlights:
- battle/AI main-frame: per-frame story step (battle_runPerFrameStoryStep_8008064),
  beast transformation action (battle_runBeastTransformationAction_8014A38);
- battle object: player counter pink-ring spawn
  (battleObject_playerCounterSpawnPinkRingIfReady_80139C4),
  fade-out and despawn self actor (battle_fadeOutAndDespawnSelfActor_801664E),
  destroy (battleObject_destroy_8016C4E),
  chip-preview GFX init (battle_loadChipPreviewGfxInit_801BFEE),
  beast-out form GFX load (battleObject_loadBeastOutFormGfx_801CC34),
  chip-select anim tick (battleObject_chipSelectAnimTick_8029C08),
  apply shake offset and project (battleObject_applyShakeOffsetAndProject_8031136),
  scroll accumulate step (battle_scrollAccumulateStep_8031176),
  call AI dispatch (battle_callAIDispatch_8046818),
  update sprite GFX for all (battleObject_updateSpriteGfxForAll_8046854),
  AI check beast-out trigger (battle_ai_checkBeastOutTrigger_80A5C74),
  run action subroutine dispatch (battleObject_runActionSubroutineDispatch_80A6A62),
  run AI dispatch with cur state (battleObject_runAIDispatchWithCurState_80A7D90),
  beast-SP battler update (battleObject_beastSP_battlerUpdate_80A7E14),
  destruct on hit phase (battle_objectDestructOnHitPhase_80DA886);
- chatbox: upload frame-tile GFX (chatbox_uploadFrameTileGFX_801C304).

## Tally update

| batch | functions examined | named (renamed via record) | hypothesis-only | dropped |
|-------|--------------------|---------------------------|-----------------|---------|
| 1     | 25                 | 25                        | 0               | 0       |
| 2     | 21                 | 20                        | 1               | 0       |
| 3     | 25                 | 23                        | 2               | 0       |
| 4     | 25                 | 21                        | 4               | 0       |
| 5     | 25                 | 20                        | 5               | 0       |
| 6     | 25                 | 21                        | 4               | 0       |
| 7     | 25                 | 19                        | 6               | 0       |
| 8     | 25                 | 20                        | 5               | 0       |
| 9     | 25                 | 18                        | 7               | 0       |

Running total: named 187, hypothesis-only 34, dropped 0 (of ~10,600 placeholders).

## Batch 10 (2026-10-14)

25 functions, 14 named + 11 hypothesis (sub_8026B04, sub_8031980, sub_8031A5E,
sub_8083DF0, sub_80A5AD4, sub_80A7CFE, sub_81353F0, sub_81419A0, sub_8142B28,
sub_814E0D0, sub_814F3A4 kept as comment only), renamed in recomp 3623877a
(bn6f pushed; port ROM unchanged).

Highlights:
- battle main-frame: per-frame step (battle_runMainStep_800801C),
  should-play-death-anim check (battle_shouldPlayDeathAnim_800A21C);
- battle object: refresh sprite and arm (battleObject_refreshSpriteAndArm_800F2C6),
  beast-out intro run (battle_runBeastOutIntro_801498E),
  chip-preview GFX default (battle_loadChipPreviewGfxDefault_801BFF8),
  camera apply delta to render info scroll (camera_applyDeltaToRenderInfoScroll_8030136),
  init battle start overlay (battle_initBattleStartOverlay_8033A80),
  beast-out tick (battle_beastOutTick_80A5B70),
  dispatch custom jumptable by index (battle_dispatchCustomJumptableByIndex_80A8208),
  decide death effect kind (battleObject_decideDeathEffectKind_80C4C52),
  AI check panel range and do attack (battle_aiCheckPanelRangeAndDoAttack_8134930),
  per-frame update (battleObject_perFrameUpdate_8142248),
  get by index (battleObject_getByIndex_81428C2);
- enemy: get arm position (enemy_getArmPosition_800F26C).

## Tally update

| batch | functions examined | named (renamed via record) | hypothesis-only | dropped |
|-------|--------------------|---------------------------|-----------------|---------|
| 1     | 25                 | 25                        | 0               | 0       |
| 2     | 21                 | 20                        | 1               | 0       |
| 3     | 25                 | 23                        | 2               | 0       |
| 4     | 25                 | 21                        | 4               | 0       |
| 5     | 25                 | 20                        | 5               | 0       |
| 6     | 25                 | 21                        | 4               | 0       |
| 7     | 25                 | 19                        | 6               | 0       |
| 8     | 25                 | 20                        | 5               | 0       |
| 9     | 25                 | 18                        | 7               | 0       |
| 10    | 25                 | 14                        | 11              | 0       |

Running total: named 201, hypothesis-only 45, dropped 0 (of ~10,600 placeholders).
