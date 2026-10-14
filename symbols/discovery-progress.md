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

## Batch 11 (2026-10-14)

25 functions, 17 named + 8 hypothesis (sub_8026B04, sub_802DE5C, sub_8030F04,
sub_8031980, sub_8031A5E, sub_8083DF0, sub_8123C20, sub_8142B28 kept as
comment only), renamed in recomp 8e6d70d4 (bn6f pushed; port ROM unchanged).

Highlights:
- battle main-frame: step run and decide result
  (battle_stepRunAndDecideResult_800938A);
- battle HUD: draw chip-counts OAM with retval zero
  (battle_drawChipCountsOAMWithRetvalZero_801C078);
- chatbox: get text script pointer by index
  (chatbox_getTextScriptPointerByIndex_804A24C) — first chatbox entry
  in the 0x0804xxxx range (chatbox main area);
- overworld player: apply countdown update (owPlayer_applyCountdownUpdate_809D5C4);
- battle object: load AI data entry (battleObject_loadAIDataEntry_80A6014),
  set flags 6 bit (battleObject_setFlags6Bit_81171F4),
  queue OAM from table row (battleObject_queueOamFromTableRow_811FAF4),
  update shield or status effect (battleObject_updateShieldOrStatusEffect_813468C),
  run AI dispatch table (battleObject_runAIDispatchTable_813B7A0),
  validate script pointer (battleObject_validateScriptPointer_814E0B6),
  run update routine and advance PC (battleObject_runUpdateRoutineAndAdvancePC_814E0D0);
- beast-sp battle: beast-out dispatch (battle_beastOutDispatch_80A5AD4),
  battle perform beast-out AI tick (battle_performBeastOutBattleAiTick_81419A0),
  get beast-out availability (battle_getBeastOutAvailability_81422BE);
- overworld map: check bounds and start beast-sp encounter
  (owMapObject_checkBoundsAndStartBeastSpEncounter_80A7CFE);
- battle AI: run turn handler (battle_aiRunTurnHandler_81353F0);
- battle helper: calc panel screen coord
  (battle_calcPanelScreenCoord_814F3A4).

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
| 11    | 25                 | 17                        | 8               | 0       |

Running total: named 218, hypothesis-only 53, dropped 0 (of ~10,600 placeholders).

## Batch 12 (2026-10-14)

25 functions, 15 named + 10 hypothesis (sub_802DE5C, sub_8031980, sub_8031A5E,
sub_8083DF0, sub_81196F0, sub_812053C, sub_81345D4, sub_8134610, sub_814E0B4,
sub_814E1E8 kept as comment only), renamed in recomp ba5410f4 (bn6f pushed;
port ROM unchanged).

Highlights:
- battle AI: get beast-out cost base (battle_aiActorKind_getBeastOutCostBase_800FD0A);
- custMenu: animate cursor to OK (battle_custMenuAnimateCursorToOK_8026B04);
- battle state: get story flag from battle state
  (battle_getStoryFlagFromBattleState_802CE60);
- camera: compute scroll delta (camera_computeScrollDelta_8030E8C);
- battle textbox: run textbox update and set state bit 1
  (battle_runTextboxUpdateAndSetStateBit1_8030F04);
- battle object: update hitbox panel overlap (battleObject_updateHitboxPanelOverlap_804692C),
  get from list pair (battleObject_getFromListPair_80A6E78),
  update cursor pulsate (battle_updateCursorPulsate_811F0C8),
  apply AI data to attack table (battleObject_applyAIDataToAttackTable_8123C20),
  update chip charge (battleObject_updateChipCharge_814E168),
  run update routine and advance PC next step
  (battleObject_runUpdateRoutineAndAdvancePCNextStep_814E17C);
- battle beast-sp: beast-sp evolve step (battle_beastSpEvolveStep_8141A10);
- battle AI/soul: get soul weapon base (battle_getSoulWeaponBase_81427CE);
- battle object/chip: get panel chip by index at player pos
  (battleObject_getPanelChipByIndexAtPlayerPos_8142B28);
- rfu: dispatch call from callback (rfu_dispatchCallFromCallback_814DA6C).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |

Running total: named 233, hypothesis-only 63, dropped 0 (of ~10,600 placeholders).

## Batch 13 (2026-10-14)

25 functions, 19 named + 6 hypothesis (sub_800AA06, sub_8083DF0, sub_8113124,
sub_812053C, sub_81345D4, sub_814E0B4 kept as comment only), renamed in
recomp 2c497203 (bn6f pushed; port ROM unchanged).

Highlights:
- overworld object: within-box check (signed)
  (overworldObject_withinBoxCheck_signed_80011D6);
- battle AI: clear PwrAtk state from battle object
  (ai_clearPwrAtkStateFromBattleObject_801011A);
- battle main-frame: run controller step (battle_runControllerStep_801483C),
  init object story flags (battle_initObjectStoryFlags_801C984);
- battle HUD/state: get total damage taken for actor
  (battle_getTotalDamageTakenForActor_801DFE4),
  set navi status GFX (battle_setNaviStatusGfx_801E71C);
- battle GFX: load GFX chunk by selector with rows
  (battle_loadGfxChunkBySelectorWithRows_802868C);
- custMenu: dispatch state (custMenu_dispatchState_802A2B0);
- battle object: run AI script (battleObject_runAIScript_802DE5C);
- camera: clamp to bounds (battle_clampCameraToBounds_80300AC);
- battle object/collision: collision project ray
  (battleObject_collisionProjectRay_8031048);
- battle AI: index to actor count (battle_aiIndexToActorCount_8031980);
- battle object/chip: get chip lock colour one
  (battleObject_getChipLockColourOne_8031A5E);
- map entry: on update (mapEntry_onUpdate_80A5268);
- battle OAM: queue OAM entries for battle-object state shadow
  (battle_queueOamEntriesForBattleObjectStateShadow_8116A44);
- submenu: update panel chip info if dirty
  (submenu_updatePanelChipInfoIfDirty_81196F0);
- object: resolve table entry by index (object_resolveTableEntryByIndex_8134538);
- battle object/OAM: queue OAM panel grid
  (battleObject_queueOamPanelGrid_8134610);
- battle object: remove element and flag from update routine
  (battleObject_removeElementAndFlagFromUpdateRoutine_814E1E8).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |

Running total: named 252, hypothesis-only 69, dropped 0 (of ~10,600 placeholders).

## Batch 14 (2026-10-14)

25 functions, 19 named + 6 hypothesis (sub_802C460, sub_8083DF0, sub_80A7E44,
sub_8113124, sub_8119880, sub_81345D4 kept as comment only), renamed in
recomp 80ba811b (bn6f pushed; port ROM unchanged).

Highlights:
- battle object: is current-turn battle object
  (battleObject_isCurrentTurnBattleObject_800AA06),
  AI get unk-4C ptr (battleObject_aiGetUnk4cPtr_801442C);
- battle: count alive next entries (battle_countAliveNextEntries_800AA92);
- battle state: set unk flag bit 0 eStruct2035280
  (battle_setUnkFlagBit0_eStruct2035280_801DB60),
  set flag 0x04 eStruct2035280
  (battle_setFlag0x04_eStruct2035280_801DC6E);
- camera: should apply BG scroll to both layer pairs
  (camera_shouldApplyBgScrollToBothLayerPairs_80269E2);
- battle object: load chip panel GFX (battleObject_loadChipPanelGfx_80284E2);
- overworld player: clear state unk-10 (owPlayerObject_clearStateUnk10_809D7D8);
- overworld map object: BBS object update
  (owMapObject_bbsObjectUpdate_80A5588);
- battle object: update sprite (battleObject_updateSprite_80C464C),
  destroy on collision (battleObject_destroyOnCollision_80DFEA8);
- object: end PwrAtk action (object_endPwrAtkAction_80EAF36);
- battle AI: attack step run decrement (ai_attackStepRunDecrement_80EB1C4);
- battle GFX: render lattice or spinner
  (battle_gfx_renderLatticeOrSpinner_811E0BA);
- battle object/OAM: draw indicator or status icon
  (battleObject_drawIndicatorOrStatusIcon_812053C);
- battle submenu: dispatch by beast-SP index
  (battleObject_submenuDispatchByBeastSpIndex_8133484),
  folder edit tick (battleObject_submenuFolderEditTick_81338D4);
- battle object: align row offset check SN3
  (battleObject_alignRowOffsetCheckSN3_81353DC);
- battle AI: get AI data ptr (battle_getAIDataPtr_8142068).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |

Running total: named 271, hypothesis-only 75, dropped 0 (of ~10,600 placeholders).

## Batch 15 (2026-10-14)

25 functions, 21 named + 4 hypothesis (sub_802C460, sub_8083DF0, sub_8119880,
sub_8120504 kept as comment only), renamed in recomp c9a0f86d (bn6f pushed;
port ROM unchanged).

Highlights:
- battle: dispatch post-move substate (battle_dispatchPostMoveSubstate_8008950);
- object: exit attack state (object_exitAttackState_801171C);
- battle object: compute AI step forward delta-X
  (battleObject_computeAIStepForwardDeltaX_8018856),
  update sprite anim if changed (battleObject_updateSpriteAnimIfChanged_801BCD0);
- battle sound: play counter hit sound (battle_playCounterHitSound_801C5F4);
- battle event: eStruct2035280 post-event (battle_eStruct2035280_postEvent_801E0A0);
- table: get current entry column count (table_GetCurrentEntryColumnCount_80317FA);
- battle: lottery step (battle_lotteryStep_809AB20);
- battle AI: dispatch by AI-data-B-left ability
  (battle_aiDispatchByAIDataBLeftAbility_80A78B8);
- battle object: clear action phase unk-09 (battleObject_clearActionPhaseUnk09_80A7E44);
- battle AI/attack: check mystery data unk (battle_aiAttack_checkMysteryDataUnk_80F02A2);
- battle AI: dispatch AI state (battle_object_dispatchAIState_8113124);
- battle gauge: update cust gauge (battle_updateCustGauge_8116720);
- battle pack: synthesize pack entry from chip search result
  (battle_synthesizePackEntryFromChipSearchResult_811FF54);
- sprite: queue two OAM entries (sprite_queueTwoOamEntries_81205C0);
- pet menu: draw character stat block (pet_menu_drawCharacterStatBlock_8123C98);
- battle object: tick OAM sub flag anim slot
  (battleObject_tickOamSubFlagAnimSlot_81282C0),
  run action shield or status effect (battleObject_runActionShieldOrStatusEffect_81345D4);
- battle state: get player cur state (battle_getPlayerCurState_8137884);
- battle object: empty stub (battleObject_emptyStub_8142C90);
- m4a: mplay track handler (mplay_trackHandler_814ECB4).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |
| 15    | 25                 | 21                        | 4               | 0       |

Running total: named 292, hypothesis-only 79, dropped 0 (of ~10,600 placeholders).

## Batch 16 (2026-10-14)

25 functions, 21 named + 4 hypothesis (sub_802C460, sub_8083DF0, sub_80A7912,
sub_8119880 kept as comment only), renamed in recomp 9f3e1c42 (bn6f pushed;
port ROM unchanged).

Highlights:
- object: attach to EUnk battle-object linked list
  (object_attachToEUnkBattleObjectLinkedList_8003400);
- battle: chip effect dispatcher (battle_chipEffectDispatcher_80109A4);
- battle AI: try use chip from hand (ai_tryUseChipFromHand_8013236);
- table: get anm-seq row index table entry (getAnmSeqRowIndexTableEntry_8018886);
- cutscene weather: update (cutsceneWeather_update_808FE74);
- battle object: dispatch run action by state
  (battleObject_dispatchRunActionByState_80A6A16);
- battle: check enemy in attack range and trigger
  (battle_checkEnemyInAttackRangeAndTrigger_80A7B32),
  check beast-SP trigger (battle_checkBeastSpTrigger_80A7BC0);
- object: present collision and handle hit (object_presentCollisionAndHandleHit_80CA3A0);
- battle object/AI: AI attack step move toward target
  (battleObject_aiAttackStepMoveTowardTarget_80EB088);
- battle AI attack: apply panel move (battle_aiAttack_applyPanelMove_80EB128);
- battle AI: attack dispatch (battle_aiAttackDispatch_810ED70);
- battle object: run triple sound dispatch
  (battleObject_runTripleSoundDispatch_81176C0),
  AI state jumptable dispatcher (battleObject_aiStateJumpTableDispatcher_8117EC8),
  beast-SP dispatch action (battleObject_beastSpDispatchAction_8117F78);
- battle beast-sp: dispatch to phase handler
  (battle_beastsp_dispatchToPhaseHandler_8117FC4);
- battle object: set beast-out phase and sprite
  (battleObject_setBeastOutPhaseAndSprite_81182A4);
- battle pack: pack chip into battle pack entry
  (battle_packChipIntoBattlePackEntry_8120504);
- object: resolve table entry wrapper (object_resolveTableEntryWrapper_8134548);
- battle object: run beast status effect (battleObject_runBeastStatusEffect_81345B0);
- battle: check scenario effect active (battle_checkScenarioEffectActive_8142190).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |
| 15    | 25                 | 21                        | 4               | 0       |
| 16    | 25                 | 21                        | 4               | 0       |

Running total: named 313, hypothesis-only 83, dropped 0 (of ~10,600 placeholders).

## Batch 17 (2026-10-14)

25 functions, 21 named + 4 hypothesis (sub_802C460, sub_8083DF0, sub_80A7354,
sub_80E1670 kept as comment only), renamed in recomp f6458265 (bn6f pushed;
port ROM unchanged).

Highlights:
- battle: tick pre-battle init (battle_tickPreBattleInit_8008AA0);
- battle object: tick sprite anim (battleObject_tickSpriteAnim_801BC24),
  register custom arm (battleObject_registerCustomArm_801DB84);
- custMenu: dispatch selection (battle_custMenuDispatchSelection_8028820);
- battle object: compute puppeteer switch element
  (battleObject_computePuppeteerSwitchElement_802A220);
- battle: calc dist panel between (battle_calcDistPanelBetween_8031852);
- overworld map: get hotspot boundary for map group
  (owMap_getHotspotBoundaryForMapGroup_80A5724);
- battle object: dispatch AI custom op (battleObject_dispatchAICustomOp_80A57AC),
  dispatch AI data (battleObject_dispatchAIData_80A72B4);
- battle beast-sp: stepper (battle_beastSpStepper_80A7912);
- beast-SP: init or update (beastSP_initOrUpdate_80A86B4);
- battle object: dispatch cur phase by jumptable
  (battleObject_dispatchCurPhaseByJumpTable_80D4570);
- battle AI attack: vars try reset and process action
  (battle_aiAttackVarsTryResetAndProcessAction_80EC67E);
- battle object: clear beast-out submenu flag
  (battleObject_clearBeastOutSubmenuFlag_8117FF8);
- submenu: update chip palette (submenu_updateChipPalette_8118234);
- battle object: get cur state row ptr
  (battleObject_getCurStateRowPtr_8118314);
- battle object: check PwrAtk target switch
  (battleObject_checkPwrAtkTargetSwitch_8119880);
- battle: dispatch scenario effect (battle_dispatchScenarioEffect_811E248);
- battle object: beast-SP update (battleObject_beastSp_update_811E2B4),
  update sprite unk-0x2C from scenario flags
  (battleObject_updateSpriteUnk0x2cFromScenarioFlags_811E314);
- battle: get enemy index from map group
  (battle_getEnemyIndexFromMapGroup_8142080).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |
| 15    | 25                 | 21                        | 4               | 0       |
| 16    | 25                 | 21                        | 4               | 0       |
| 17    | 25                 | 21                        | 4               | 0       |

Running total: named 334, hypothesis-only 87, dropped 0 (of ~10,600 placeholders).

## Batch 18 (2026-10-14)

25 functions, 20 named + 5 hypothesis (sub_8083DF0, sub_80A5C9C, sub_80A82DC,
sub_80E1670, sub_81421D0 kept as comment only), renamed in recomp 7911de90
(bn6f pushed; port ROM unchanged).

Highlights:
- battle object: try get next panel in facing direction
  (battleObject_tryGetNextPanelInFacingDirection_800F964);
- battle AI attack: init vars and start (aiAttack_initVarsAndStart_80116AE);
- object: AI attack vars exit state (object_aiAttackVarsExitState_8014F04);
- battle chip-select: handle up-down input
  (battle_chipSelect_handleUpDownInput_802C460);
- battle: calc panel distance (battle_calcPanelDistance_803182A);
- battle object: run update jumptable
  (battleObject_runUpdateJumpTable_80895A8);
- battle AI: dispatch beast-SP (battle_ai_dispatchBeastSp_80A6E98);
- battle object: update animation (battleObject_updateAnimation_80A6F2C),
  update from cur action (battleObject_updateFromCurAction_80A7354);
- overworld map object: event 5F1 trigger
  (owMapObject_event5F1Trigger_80A75DC);
- battle object: dispatch on subtype (battleObject_dispatchOnSubtype_80A781C);
- sprite: setup beast-out sprite (sprite_setupBeastOutSprite_80A786C);
- battle object: update sprite and maybe reload anim
  (battleObject_updateSpriteAndMaybeReloadAnim_80A827C);
- battle object: aura update or destroy
  (battleObject_auraUpdateOrDestroy_80C5886),
  run update actions and collisions
  (battleObject_runUpdateActionsAndCollisions_80D2320);
- object: destroy phase (object_destroyPhase_80D4A98);
- battle object: AI attack tick charge (battleObject_aiAttackTickCharge_80EBEB2);
- battle: get AI data for enemy index
  (battle_getAIDataForEnemyIndex_8142050);
- overworld map object: count by chip or something
  (owMapObject_countByChipOrSomething_8142B04);
- sound: music player set stop flag
  (sound_musicPlayerSetStopFlag_814F1E8).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |
| 15    | 25                 | 21                        | 4               | 0       |
| 16    | 25                 | 21                        | 4               | 0       |
| 17    | 25                 | 21                        | 4               | 0       |
| 18    | 25                 | 20                        | 5               | 0       |

Running total: named 354, hypothesis-only 92, dropped 0 (of ~10,600 placeholders).

## Batch 19 (2026-10-14)

25 functions, 17 named + 8 hypothesis (sub_80315B8, sub_8083DF0, sub_80A70FC,
sub_80A73A0, sub_80A82DC, sub_80E1670, sub_81196C4, sub_813C06C kept as
comment only), renamed in recomp e62a32c1 (bn6f pushed; port ROM unchanged).

Highlights:
- battle object: steal chip from enemy navi
  (battleObject_stealChipFromEnemyNavi_8010740),
  mark used action (battleObject_markUsedAction_80143A6);
- startScreen: run beast cutscene (startScreen_runBeastCutscene_8086FD8);
- battle: load beast-SP dmg (battle_loadBeastSpDmg_809BA14);
- battle object: step beast-out (battleObject_stepBeastOut_80A6F78),
  copy PwrAtk fields to temp buffer
  (battleObject_copyPwrAtkFieldsToTempBuffer_80A7238);
- battle beast-sp: set AI template stats
  (battle_beastsp_setAITemplateStats_80A775C);
- battle object: update sprite mosaic and exit if not scaled
  (battleObject_updateSpriteMosaicAndExitIfNotScaled_80C461C);
- battle AI attack: handle move direction timer and start new attack
  (aiAttack_handleMoveDirectionTimerAndStartNewAttack_80EB502),
  walk forward then attack (aiAttack_walkForwardThenAttack_80ED7A2);
- battle object: advance AI step move
  (battleObject_advanceAIStepMove_810986E);
- battle AI attack vars: run key-item trap step
  (aiAttackVars_runKeyItemTrapStep_8109DEC);
- submenu: update chip palette (submenu_updateChipPalette_81197B4);
- chatbox: init folder name display buffer
  (chatbox_initFolderNameDisplayBuffer_81202FC);
- battle object: run update handler by type
  (battleObject_runUpdateHandlerByType_8123514),
  dispatch update by state (battleObject_dispatchUpdateByState_8133200);
- battle: get scenario effect state field
  (battle_getScenarioEffectStateField_81421D0).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |
| 15    | 25                 | 21                        | 4               | 0       |
| 16    | 25                 | 21                        | 4               | 0       |
| 17    | 25                 | 21                        | 4               | 0       |
| 18    | 25                 | 20                        | 5               | 0       |
| 19    | 25                 | 17                        | 8               | 0       |

Running total: named 371, hypothesis-only 100, dropped 0 (of ~10,600 placeholders).

## Batch 20 (2026-10-14)

25 functions, 15 named + 10 hypothesis (sub_80315B8, sub_8031BE2, sub_803DEB4,
sub_8083DF0, sub_8086EA8, sub_80A6FDC, sub_80A70FC, sub_80A82DC, sub_80E1670,
sub_813BD90 kept as comment only), renamed in recomp abcd23cb (bn6f pushed;
port ROM unchanged).

Highlights:
- battle: start cross net (battle_startCrossNet_800898C);
- battle AI beast-SP: page step (battle_aiBeastSPageStep_80A5C9C);
- battle object: update beast-out sprite
  (battleObject_updateBeastOutSprite_80A704C),
  pick beast-SP face (battleObject_pickBeastSpFace_80A708C),
  beast step update (battleObject_beastStepUpdate_80A73A0),
  AI spawn snake arm attack (battleObject_aiSpawnSnakeArmAttack_80F1808),
  try start chip attack (battleObject_tryStartChipAttack_8112600);
- battle object/GFX: tick color shader from extra vars
  (battleObject_tickColorShaderFromExtraVars_81159DC);
- submenu: dispatch by cur action (submenu_dispatchByCurAction_81196C4);
- battle object: set coords from cutscene script
  (battleObject_setCoordsFromCutsceneScript_811E3CC);
- startScreen: queue four corner arrows
  (startScreen_queueFourCornerArrows_811F1B8);
- pet menu: draw status icons (pet_menu_drawStatusIcons_8123E58);
- beastsp: dispatch handler (beastsp_dispatchHandler_81357C4);
- battle: beast-out apply cross changes
  (battle_beastOutApplyCrossChanges_813C06C);
- battle object: scenario effect state address
  (battleObject_scenarioEffectStateAddr_81421D8).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |
| 15    | 25                 | 21                        | 4               | 0       |
| 16    | 25                 | 21                        | 4               | 0       |
| 17    | 25                 | 21                        | 4               | 0       |
| 18    | 25                 | 20                        | 5               | 0       |
| 19    | 25                 | 17                        | 8               | 0       |
| 20    | 25                 | 15                        | 10              | 0       |

Running total: named 386, hypothesis-only 110, dropped 0 (of ~10,600 placeholders).

## Batch 21 (2026-10-14)

25 functions, 18 named + 7 hypothesis (sub_8031388, sub_803DEB4, sub_8083DF0,
sub_8086EA8, sub_80897A6, sub_80E1670, sub_8112A78 kept as comment only),
renamed in recomp 6011da2c (bn6f pushed; port ROM unchanged).

Highlights:
- battle: calc angle octant (battle_calcAngleOctant_800118A);
- battle scroll: tick step or stop (battle_scrollTickStepOrStop_80313FC);
- textbox: compute cursor blink advance
  (textbox_computeCursorBlinkAdvance_80315B8);
- battle object: check in attack range
  (battleObject_checkInAttackRange_8031BE2);
- battle: set object charge params from table
  (battle_setObjectChargeParamsFromTable_80A6FDC);
- battle beast-out: check player in box trigger
  (battle_beastOut_checkPlayerInBoxTrigger_80A70FC);
- battle object: check panel match (battleObject_checkPanelMatch_80A743C);
- battle AI: select AI ptr from table (battle_ai_selectAIPtrFromTable_80A7590);
- battle object: check set priority by event flag
  (battleObject_checkSetPriorityByEventFlag_80A82DC),
  dispatch update by cur action
  (battleObject_dispatchUpdateByCurAction_80A8654);
- battle beast-SP: update or init (battle_beastSP_updateOrInit_80A86A0);
- battle object: step sprite if alive and unfrozen
  (battleObject_stepSpriteIfAliveAndUnfrozen_80B881E);
- object: dispatch collision phase (object_dispatchCollisionPhase_80CC1A0);
- battle object: clear AI data unknown
  (battleObject_clearAIDataUnknown_80E1662);
- beast-out: advance set transform (beastOutOrProgramAdvanceSetTransform_811842C);
- battle object: collect targets in region
  (battleObject_collectTargetsInRegion_813BD90);
- battle: get beast-out A-PwrAtk handler by item check
  (battle_getBeastOutAPwrAtkHandlerByItemCheck_813C4A8);
- submenu: get beast-out gate anim script table ptr
  (submenu_getBeastOutGateAnimScriptTablePtr_813C4C8).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |
| 15    | 25                 | 21                        | 4               | 0       |
| 16    | 25                 | 21                        | 4               | 0       |
| 17    | 25                 | 21                        | 4               | 0       |
| 18    | 25                 | 20                        | 5               | 0       |
| 19    | 25                 | 17                        | 8               | 0       |
| 20    | 25                 | 15                        | 10              | 0       |
| 21    | 25                 | 18                        | 7               | 0       |

Running total: named 404, hypothesis-only 117, dropped 0 (of ~10,600 placeholders).

## Batch 22 (2026-10-14)

25 functions, 17 named + 8 hypothesis (sub_8031C7C, sub_8083DF0, sub_8086EA8,
sub_80897A6, sub_80E1670, sub_81185E4, sub_8118604, sub_8123BE0 kept as
comment only), renamed in recomp f5bca610 (bn6f pushed; port ROM unchanged).

Highlights:
- battle object: apply fullscreen shake and camera bump
  (battleObject_applyFullscreenShakeAndCameraBump_8031388);
- battle: calc rel pos and advance textbox blink
  (battle_calcRelPosAndAdvanceTextboxBlink_8031C2A);
- game state: get protected bugfrags
  (gameState_getProtectedBugfrags_803D0F4);
- battle: update link state (battle_updateLinkState_803DEB4);
- battle object: dispatch beast-out action
  (battleObject_dispatchBeastOutAction_804CBA4);
- beast-SP: spawn index dispatcher
  (beastSp_spawnIndexDispatcher_804CC8E);
- battle object: panel snapshot buffer init
  (battleObject_panelSnapshotBufferInit_80A54F0);
- overworld player: set direction anim from offset
  (owPlayer_setDirectionAnimFromOffset_80A5974);
- battle object: get damage element or palette
  (battleObject_getDamageElementOrPalette_80C46CC);
- battle AI: try chip attack (battle_aiTryChipAttack_810F1B6);
- battle AI: snake arm tick step (battle_aiSnakeArmTickStep_8112A78);
- battle object: apply AI data to attack table
  (battleObject_applyAIDataToAttackTable_8123BC0);
- battle object: run AI turn phase
  (battleObject_runAITurnPhase_8123C00);
- battle: update target panels all rows
  (battle_updateTargetPanelsAllRows_813BD24);
- battle: get AI data version fields
  (battle_getAIDataVersionFields_81418E4);
- pet: test is map group allowed (pet_testIsMapGroupAllowed_8141AA8);
- cutscene: dispatch event flags (cutscene_dispatchEventFlags_8142618).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |
| 15    | 25                 | 21                        | 4               | 0       |
| 16    | 25                 | 21                        | 4               | 0       |
| 17    | 25                 | 21                        | 4               | 0       |
| 18    | 25                 | 20                        | 5               | 0       |
| 19    | 25                 | 17                        | 8               | 0       |
| 20    | 25                 | 15                        | 10              | 0       |
| 21    | 25                 | 18                        | 7               | 0       |
| 22    | 25                 | 17                        | 8               | 0       |

Running total: named 421, hypothesis-only 125, dropped 0 (of ~10,600 placeholders).

## Batch 23 (2026-10-14)

25 functions, 14 named + 11 hypothesis (sub_8083DF0, sub_8086EA8, sub_80897A6,
sub_80A5890, sub_80A7CEA, sub_80E1670, sub_81185E4, sub_8118604, sub_811E168,
sub_811E202, sub_8123BE0 kept as comment only), renamed in recomp 11f05b0d
(bn6f pushed; port ROM unchanged).

Highlights:
- battle: update chip select init (battle_updateChipSelectInit_80094B6),
  dispatch by index (battle_dispatchByIndex_80099A4),
  run battle step (battle_runBattleStep_8009B20);
- battle chip-select: chip select step (battle_chipSelectStep_802BDC0);
- chatbox: render battle cursor step
  (chatbox_renderBattleCursorStep_8031C7C);
- battle AI: story sub-3 (battle_aiStorySub3_8031C94);
- battle: run battle frame for cyberworld
  (battle_runBattleFrameForCyberWorld_808FEA4);
- reqBBS: update bobbing camera and play chime
  (reqBBS_updateBobbingCameraAndPlayChime_80A55D4);
- battle object: dispatch cur action
  (battleObject_dispatchCurAction_80A7C84);
- battle object: run action dispatcher
  (battleObject_runActionDispatcher_80E19F4);
- battle: dispatch object state (battle_dispatchObjectState_811E024);
- submenu: subchip tick OAM and advance anim table
  (submenu_subchip_tickOamAndAdvanceAnimTable_81244EC);
- battle: load navicust program table
  (battle_loadNavicustProgramTable_813BD10);
- battle object: increment unk-200431C counter for matching objects
  (battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |
| 15    | 25                 | 21                        | 4               | 0       |
| 16    | 25                 | 21                        | 4               | 0       |
| 17    | 25                 | 21                        | 4               | 0       |
| 18    | 25                 | 20                        | 5               | 0       |
| 19    | 25                 | 17                        | 8               | 0       |
| 20    | 25                 | 15                        | 10              | 0       |
| 21    | 25                 | 18                        | 7               | 0       |
| 22    | 25                 | 17                        | 8               | 0       |
| 23    | 25                 | 14                        | 11              | 0       |

Running total: named 435, hypothesis-only 136, dropped 0 (of ~10,600 placeholders).

## Batch 24 (2026-10-14)

25 functions, 15 named + 10 hypothesis (sub_8014E08, sub_804C404, sub_804C478,
sub_8083DF0, sub_8086EA8, sub_80E1670, sub_810D808, sub_81171D4, sub_8118604,
sub_8123BE0 kept as comment only), renamed in recomp ff20f6d0 (bn6f pushed;
port ROM unchanged).

Highlights:
- battle object: dispatch phase by paused
  (battleObject_dispatchPhaseByPaused_80170C4),
  run wind recovery state (battleObject_runWindRecoveryState_8017122);
- battle chip-select: draw meter blocks
  (battle_chipSelectDrawMeterBlocks_802C85C);
- battle object: beast-out check or init
  (battleObject_beastOut_checkOrInit_80897A6);
- battle: set player direction and anim from AI state
  (battle_setPlayerDirectionAndAnimFromAIState_80A5890);
- overworld map object: update beast-SP encounter
  (owMapObject_updateBeastSpEncounter_80A7CEA);
- object: destroy step (object_destroyStep_80CE782);
- battle object: update movement highlight
  (battleObject_updateMovementHighlight_80D4544),
  snake arm tick (battleObject_snakeArmTick_80E1566),
  run beast-SP action handler
  (battleObject_runBeastSpActionHandler_80E3A32),
  slash beast wave update (battleObject_slashBeastWaveUpdate_80EAE28);
- battle AI attack vars: tick or exit
  (battle_aiAttackVarsTickOrExit_80EDF0C);
- battle object: set palette from offset table
  (battleObject_setPaletteFromOffsetTable_81185E4);
- battle AI state: dispatch state machine
  (battle_aiState_dispatchStateMachine_811E168);
- battle object: set sprite unk-0x2C flags from AI version
  (battleObject_setSpriteUnk2cFlagsFromAIVersion_811E202).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |
| 15    | 25                 | 21                        | 4               | 0       |
| 16    | 25                 | 21                        | 4               | 0       |
| 17    | 25                 | 21                        | 4               | 0       |
| 18    | 25                 | 20                        | 5               | 0       |
| 19    | 25                 | 17                        | 8               | 0       |
| 20    | 25                 | 15                        | 10              | 0       |
| 21    | 25                 | 18                        | 7               | 0       |
| 22    | 25                 | 17                        | 8               | 0       |
| 23    | 25                 | 14                        | 11              | 0       |
| 24    | 25                 | 15                        | 10              | 0       |

Running total: named 450, hypothesis-only 146, dropped 0 (of ~10,600 placeholders).

## Batch 25 (2026-10-14)

25 functions, 19 named + 6 hypothesis (sub_8014E08, sub_8083DF0, sub_8086EA8,
sub_810D808, sub_8123BE0, sub_814E7E0 kept as comment only), renamed in
recomp 00c28c53 (bn6f pushed; port ROM unchanged).

Highlights:
- battle AI: get actor info (battle_ai_getActorInfo_801595E);
- battle object: check AI unk-0x48 flag 0x80
  (battleObject_checkAIUnk0x48Flag0x80_801597C);
- custom screen: redraw chip slot (customScreen_redrawChipSlot_8029CD4);
- battle scroll: advance by panel step
  (battle_scrollAdvanceByPanelStep_8031520);
- battle: dispatch jump by actor slot and action
  (battle_dispatchJumpByActorSlotAndAction_804C404);
- battle render: set blur and anim update
  (battle_render_setBlurAndAnimUpdate_804C478);
- battle object: update beast-out PwrAtk
  (battleObject_updateBeastOutPwrAtk_80A5828);
- object: spawn related child effect
  (object_spawnRelatedChildEffect_80E08C4);
- battle AI: find enemy targets (battle_ai_findEnemyTargets_80E1670);
- battle AI beast-out: tick (battle_aiBeastOutTick_810F0D8);
- battle object: dispatch beast-out action
  (battleObject_dispatchBeastOutAction_8117110);
- battle object: set sprite coords from EStruct entry if active
  (battleObject_setSpriteCoordsFromEStructEntryIfActive_81171D4);
- battle object: update beast-SP slot mask
  (battleObject_updateBeastSpSlotMask_8118604);
- battle object: set coords and update sprite
  (battleObject_setCoordsAndUpdateSprite_811E1D4);
- beast AI: get attack type affinity
  (beastAi_getAttackTypeAffinity_813C624);
- battle: can beast out (battle_canBeastOut_8141AD2);
- battle object: run charge tick and store charge time
  (battleObject_runChargeTickAndStoreChargeTime_814E210);
- battle object: mark AI data unk-18 and mark dirty
  (battleObject_markAIDataUnk18AndMarkDirty_814E21C);
- battle AI data: process stream byte
  (battle_aiData_processStreamByte_814E7CC).

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
| 11    | 25                 | 17                        | 8               | 0       |
| 12    | 25                 | 15                        | 10              | 0       |
| 13    | 25                 | 19                        | 6               | 0       |
| 14    | 25                 | 19                        | 6               | 0       |
| 15    | 25                 | 21                        | 4               | 0       |
| 16    | 25                 | 21                        | 4               | 0       |
| 17    | 25                 | 21                        | 4               | 0       |
| 18    | 25                 | 20                        | 5               | 0       |
| 19    | 25                 | 17                        | 8               | 0       |
| 20    | 25                 | 15                        | 10              | 0       |
| 21    | 25                 | 18                        | 7               | 0       |
| 22    | 25                 | 17                        | 8               | 0       |
| 23    | 25                 | 14                        | 11              | 0       |
| 24    | 25                 | 15                        | 10              | 0       |
| 25    | 25                 | 19                        | 6               | 0       |

Running total: named 469, hypothesis-only 152, dropped 0 (of ~10,600 placeholders).
