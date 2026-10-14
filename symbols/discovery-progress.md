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
