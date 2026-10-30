# Live-register sweep (asm keeps a value in a register for a callee; the C keeps it in a local)

Tool: `tools/check_live_regs.py` (run over the 3349 readable functions). It walks each function's asm with a
forward dataflow (registers the function wrote itself and still holds at every `bl`), computes the callee's
inputs (read before written, transitively through its own `bl`/tail `b`; `push` is not a read), and checks the C:

* **FLAG**: a live register the callee reads that is neither a parameter of any `call_*` wrapper for that callee
  in the function's C region nor assigned (`Rn = ...`) outside a wrapper.
* **ZERO**: a wrapper argument that is the literal 0 for register rN, while the asm has a non-zero value in rN
  at some `bl` of that callee (the b8b4fb44 shape).
* **STALE**: `x = call_W(..); ... x.rN` where W's callee pushes/pops rN (so the wrapper's `R_N` result is the
  value from before the call), the function itself writes rN, and the C never assigns `Rn` outside a wrapper
  (the c1b67e74 shape: `dataPtr = init.r5` was the caller's stale r5). On the pre-c1b67e74 `asm24.c` it
  reports 7 STALE sites in `pet_tryStartRandomEncounterStep`.

3716 call sites have a live register that the callee reads. FLAG 2, STALE 23 (17 distinct sites after the fixes
below; the tool still lists the fixed ones it cannot tell are fixed), ZERO 32 (grouped by callee below). Every
one was read against both asm functions. **BUG: 5** (1 ZERO, 4 STALE) plus one faithfulness fix from a FLAG (r8 side
effect, callees unaffected). Everything else is OK.

## BUGs (fixed in the second commit)

| function | call | reg | asm | C | verdict |
|---|---|---|---|---|---|
| navicust_handleBoardInput | navicust_getMoveDir | r1 (=r4) | asm36.s:11778 `mov r4,#0`, :11850 `mov r4,r1` (after menu_adjustCursorOrValue), :11858 `mov r1,r4` | asm36.c:4418 passed literal 0 | **BUG** (ZERO). After a cursor move the callee tests r1 != 0 to skip the delta computation; the C always passed 0. Fixed: track `moveFlag = cur.r1`. |
| battleObject_stepMoveToFuturePanel_8016730 | battle_objectCheckPanelByAlliance_800E5AC | r6, r7 | asm00_2.s:18099-18100 `mov r6,r0; mov r7,r1`; :18107-18109 stores r6/r7 and `object_reservePanel` after the call (callee push/pops r6,r7) | asm00_2_8012BA2.c:3584-3586 used `chk.r6/chk.r7` = stale R6/R7 | **BUG** (STALE). FuturePanelX/Y and the reserved panel were the caller's r6/r7. Fixed: use nextX/nextY. |
| battle_findForwardUsableAttackPanel | object_checkPanelParameters (2nd call) | r0 (=r4) | asm31.s:159933 `mov r4,r0`, :159951 `mov r0,r4` | asm31_80EBE00.c:8093-8095 passed/returned `chk_up.r4` = stale R4 (callee restores r4) | **BUG** (STALE). Fixed: panel_x. |
| folder_moveChipEntry | sub_8135244 -> battle_copyStateSlotsAndZeroNext_813527C | r1, r2 | asm36.s:10340 (sub_8135244 is `push {r0-r2,lr}`..`pop {r0-r2,pc}`; r0-r2 are the function's locals, battle_copyStateSlots... reads r1, r2) | asm36.c:3485-3487 reassigned the locals from the wrapper's result = global R0-R2 | **BUG** (STALE). When the scroll counter hits 0 the next call got stale r1/r2. Fixed: keep the locals. |
| weatherComp_layOrbTrail_809005C | sub_8090308 | r4 (result) | asm25.s:1243 `mov r4,#0`; :1273-1281 returns r4 (0 unless `mov r4,#1`); sub_8090308 pushes/pops r4 | asm25.c:1028 `result = sub_res.r4` (stale R4) | **BUG** (STALE). Returned the caller's r4 instead of 0 when the trail has frames left. Fixed: 0. |
| reqBBS_8140358 | reqBBS_81403C0, menu_adjustCursorOrValue_811F7F8 | r8 | asm reqBBS.s:3830 `mov r8,r0` | reqBBS.c:990, 997 | FLAG, **OK for the callees** (they only save/restore r8: `mov r1,r8` ... `mov r8,r1`; menu_adjust overwrites it before use), but the original leaves r8 = r0 (not restored). Fixed (faithfulness): `R8 = R0` at entry. |

## Reviewed OK

ZERO (all are a literal 0 where the asm also has 0 at that call; the "non-zero value" came from another `bl` of the same callee, whose C call passes the matching value):
ai_sortEnemyPanelCoords/object_getEnemyByNameRange r1 (0, then 0x100); battleObject_runSubmenuInputHandler_81337E8 and battleObject_submenuFolderEditTick_81338D4 / battle_aiRunTurnHandler_81353F0 r1 (0 then 1); battle_customScreenExitRotateChips_8027796 / sprite_CopyObjTilesToVramSlot_80281D4 r1 (var then 0); battle_hitSelfAndFrontPanel / sub_80C8806 r3 (0x100 then 0); battle_loadNaviCustomizerGfx, sub_81252C0, sub_8127060, sub_8133EC0 / CopyBackgroundTiles r0,r1; battle_performBeastOutBattleAiTick_81419A0 / battleObject_initAIChipQueueSlot_80468C6 r1; battle_spawnRowPanelEffect / sub_80E3A92 r3 (1 then 0); beastOverworldAi_decideTransformOrIdle_8089C16 / ai_setAIDataUnk18_8089CB0 r0 (0x1e,0x1e,0,0); folder_moveChipEntry / folder_swapCursorIndex r0 (0, 4); navicust_confirmPartPlacement, submenu_updateScroll / navicust_drawBoardTilemap, sub_813660C r0; object_scriptedEffectTick / sub_80CE6E6 r3 (`mov r3,#0; lsl r3,r3,#8` = 0); reqBBS_anim_814004C / CopyBackgroundTiles r1 (2i+3, 0); sound_80006A2 / sound_8000808 r2 (asm leaves r1/r2 untouched across the five calls; C passes the same values; sound_8000808 reads only r0-r3); story_checkEntryAlliance_8034E88, sub_81299B0 / TestEventFlagFromImmediate r0,r1 (movflag halves, 0xE/0 and 0/0x84, 0/0xF2); sub_80355EC / SetNaviStatsByte r2 (1, 0); sub_81254FC / renderTextGfx_8045F8C r1 (0,1,2,5,3).

STALE, wrapper takes the register as a parameter so R_N is the value the C passed (asm: the register is untouched across the call):
ai_falzarAttackSpawnStep snd1.r2 (asm32.c:3178); battle_scrollHitTestLineVsWedge_8030F7C res1.r2 (asm03_0_802F2C8.c:1431-1432; asm03_0.s:24612-24624, callee is `push {r2,r3,lr}`, r2 = cross throughout); object_runCollisionState_80D1D14 rem.r7 (asm31_80C6CE4.c:7910); overworld_leaveArea_808CBD8 spawn.r2 (asm24.c:566; SpawnOverworldMapObject pushes r0-r4, r2 was 0); sub_8129870 and sub_81299B0 cur.r4 (asm33_81263D0.c:4467-4470, 4809-4812; r4 = old_index passed as parameter); sub_81299B0 snd.r4 (:4822-4824, R4 still the value `cur` set as parameter, asm never changes r4).

STALE, the register is the caller's own value in the asm too, or the callee does not read it:
ai_prepareTargetedAttack pick.r4 (asm32.s:9261 reads the caller's r4, `push {r4}` only brackets sub_800E994); battle_initNaviCustomizerScene_81356F4 tile.r7 (asm36.s:11642 `mov r7,r10`, but navicust_giveKeyItem14C and everything after read only r10/r5, not r7); folder_moveChipEntry chk.r4 (folder_chipMoveError does not read r4).

## Not covered

Values that are wrong but passed through a wrapper parameter (the wrapper sets R_N, so no FLAG) are only caught by the ZERO and STALE heuristics; callees whose asm reads an input through an indirect call (`bx`, jump tables) are not seen; ARM-mode functions are parsed with the Thumb rules; `NOCALL` (`--all`) lists sites with no C call found (inlined/reimplemented).
