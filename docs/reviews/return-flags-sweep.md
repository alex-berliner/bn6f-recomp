# Return-flags sweep: callers branch on flags the callee returns, the C does not set them

Tool: `tools/check_return_flags.py` (static; asm in `bn6f/asm`, C in `cport/src/game`). Bug class: cdf7c1f4
(`init_beastSpScenario` ended `mov r0, #0` = movs, its indirect caller `CutsceneCmd_call_native_with_return_value`
does `bx r4; bne .halt`, the C returned with the stale flags of its last callee).

## What the tool found

- Conditional branches whose flags come, on every CFG path back, from a `bl X` or an indirect call
  (`mov lr, pc; bx rN`) with no flag-setting instruction between: 245 direct callees.
- 16 indirect call sites (`bx rN` followed by a flag branch). Targets resolved from the `// =sym+1` literal
  comments or the jump table the register is loaded from:
  `CutsceneCommandJumptable` (117 entries, `RunCutscene` `bne .cutsceneCommandLoop`), `MapScriptCommandJumptable`
  (68, three loops `bne`), `JumpTable_80A6ABC` (19), `off_80A7950` (7), `off_81174A4` (5), `off_811CAC4` (7),
  the function pointers `battle_isTimeStop`, `battle_isPaused`, `object_isValidPanel`, `ow_updateMarkerProj`,
  and the script macro `cs_call_native_with_return_value ptr1=X+1` (92 uses, 80 distinct targets: the beastsp
  site; its consumer is `CutsceneCmd_call_native_with_return_value` map_script_cutscene.s:5683).
- 28 callees whose result `.z/.n/.c/.v` a C caller reads (`call_X(...).z`, `v = call_X(); v.z`).
- Closed over pass-through (a return path whose last flag source is `bl Y` or a tail `b Y` makes Y flags-returning).
- Total 514 flags-returning functions, 208 of them with readable C (the rest are lifted, which keep the flags
  as faithfully as the asm). Only 6 direct consumers use C/V conditions (bge/bgt/bcc) and all six callees
  are lifted; every readable site is consumed through beq/bne, so Z (and N for none) is what matters.
- Per readable function: every return path walked back to the last flag-setting instruction, the C region
  searched for flag-setting constructs, plus four heuristics on the C: `NOFLAGS` (none at all), `FEWER` (fewer
  flag sets than distinct asm setters), `RETNF` (a return with no flag set since the previous return),
  `CALLAFTER` (a call between the last flag set and the return), `R0AFTER` (R0 assigned after the last flag set),
  `PASS` (asm flags only pass through a callee).

## Verdicts

### BUG (fixed)

| function | return path | asm setter | consumer | C | verdict |
|---|---|---|---|---|---|
| CutsceneCmd_do_camera_shake | the only return | map_script_cutscene.s:6057 `mov r0, #1` (movs) after `add r7, #4` (6056) | map_script_cutscene.s:6653 `bx r0` / 6656 `bne .cutsceneCommandLoop` (RunCutscene, via CutsceneCommandJumptable) | map_script_cutscene.c:4108 (was: ended with `R0 = 1; RETURN_TO_CALLER()` after GUEST_CALL(camera_initShakeEffect_80302a8), no flag set) | BUG: RunCutscene saw the flags of camera_initShakeEffect_80302a8; if Z was set the command loop ended the frame instead of advancing to the next command. Fixed: `SET_FLAGS_ADD(scriptPos, 4); R0 = 1; SET_FLAGS_NZ(1)`. |

### Latent (fixed, no change for the consumer)

| function | asm setter | consumer | C | verdict |
|---|---|---|---|---|
| MapScriptCutsceneCmd_play_sound | map_script_cutscene.s:5779.. `add r7, #3; mov r0, #1` | s:1471/1495/1527 `bne` (MapScriptCommandJumptable loops) | map_script_cutscene.c:3736 | N/Z came from `cursor + 3` (nonzero for any pointer, so `bne` agreed), comment claimed "mov sets none". Added `SET_FLAGS_NZ(1)`. |
| MapScriptCutsceneCmd_set_event_flag_range | map_script_cutscene.s:1016-1017 `add r7, #4; mov r0, #1` | same loops | map_script_cutscene.c:759 | same as above, same fix. |

### Flagged by a heuristic, reviewed OK

| function | mark | asm flag source | consumer | C | why OK |
|---|---|---|---|---|---|
| SWI_CpuSet | NOFLAGS | libs.s:20553 `svc 0xb` (SWI return restores CPSR) | ZeroFillByWord pass-through (asm00_0.s:623) | libs.c:863 | flags are not touched by the BIOS call; swi_call does not write cpu.nz. |
| ZeroFillByWord | PASS/CALLAFTER | asm00_0.s:620 `mov r0, #0` before `bl SWI_CpuSet` (625 pop) | StartCutscene pass-through, map_script_cutscene.s:2595 | asm00_0.c:368 | C sets ADD(dest,0) then NZ(0) before the call, and the call keeps them. Same as asm (N=0, Z=1, C=V=0). |
| TestEventFlagRange | NOFLAGS | asm03_0.s:20308 `tst r0, r0` | asm24.s:1606 beq, chatbox.s:3721 bne, chatbox.s:4172 bne | event_flags.c:105-115 `test_range()` (the tool only sees the one-line wrapper) | `SET_FLAGS_CMP(set, count); SET_FLAGS_NZ(R0)` = cmp r6,r7 then tst. |
| TestEventFlagRangeFromImmediate, TestEventFlagFromImmediate, TestEventFlag | PASS/HAS | asm03_0.s:20005.. `tst r0, r1`, range as above | many beq/bne | event_flags.c:56-67, 105-118 | shared helpers `test_flag()`/`test_range()` set flags before the return. |
| playerObjectMovingToPanelValidityRelated_800E618 | NOFLAGS | object.s:5548 `mov r0, #0` on the invalid-panel path, else bl object_checkPanelParameters | asm00_2.s:1915 beq, 1952 bne | object.c:2287 early `return` | invalid path: object_isValidPanel (battle_helpers.c:92) already leaves R0 = 0 and Z set, C/V untouched = movs r0,#0 result; valid path passes the callee's flags. |
| battle_objectCheckPanelByAlliance_800E5AC | RETNF/CALLAFTER/R0AFTER | object.s:5464 `mov r0, #0`; else bl object_checkPanelParameters | beq/bne callers | object.c:2153 | invalid path: `R0 = 0; SET_FLAGS_NZ(R0)`; valid path returns the callee's flags as the asm does. |
| aiTrySelectChipFromHand_801336C | RETNF/CALLAFTER | asm00_2.s:10625 `mov r0, #0`, tail `bl ai_tryUseChipFromHand_8013236` | asm00_2.s:10617 beq, 10641 | asm00_2_8012BA2.c:328 | each `R0 = 0` return has SET_FLAGS_NZ(R0) after the preceding SET_FLAGS_CMP; last return passes the callee's flags. |
| object_isCurrentPanelSolid, object_isCurrentPanelValid, GetRandomRelativePanelFiltered, npc_checkStateEventFlag_80871C0, PlaySoundEffect, m4a_800061E, StartCutscene | PASS/RETNF | `bl callee; pop {pc}` (pop never sets flags, add sp/str/ldr/ldr=literal neither) | beq/bne callers | object.c:1156,1219; asm00_2_8012BA2.c:2908; asm22.c:214; asm00_0.c:11,88; map_script_cutscene.c:1330 | GUEST_CALL of the callee is the last flag source in the C too. |
| sound_8000808 | HAS | asm00_0.s:440 `cmp r7, #0x20` (full) / 448 `add r7, r7, r5` (stored) | via m4a_800061E/PlaySoundEffect | asm00_0.c:310 | both paths set CMP / ADD as the asm; `lsl`/`add #0xc` before the final `adds` do not matter. |
| initWorkArea5Entries_80467D8, sub_803C248, sub_8049D42, TestCutsceneFlag, MapScriptCutsceneCmd_give_or_take_item | R0AFTER | `cmp r7,#5` / `tst r0,r0` / `mov r0,#1` | callers read Z only | asm03_2.c:564, asm03_1_1.c:1154, asm03_2.c:3331, map_script_cutscene.c:4645, :3881 | flags are set from the same value that R0 receives (SET_FLAGS_NZ(x); R0 = x), or the value is the constant 1. give_or_take_item leaves C/V from the callee instead of `add r7,#4`: only Z is read. |
| the 23 FEWER functions (CutsceneCmd_call_native_with_return_value, move_player_in_facing_direction, pause, warp_cmd_8038040, MapScriptCmd_jump_if_player_z_*, ..., IsNotSlipRunningAndNotInCutscene, navicust_check*, verifyEwramWorkAreaChecksum_803F8C4, ...) | FEWER | `mov r0, #1` / `mov r0, #0` on two return paths | beq/bne | listed in the appendix | the C computes the result in R0 / a struct field once and calls SET_FLAGS_NZ(R0) on the merged path after the last `R0 =`; same Z as the asm's two movs. |

### Everything else (HAS, 150 functions)

All have a flag-setting construct whose argument is the same value as the R0 / tested register of the asm
(checked by listing, for every SET_FLAGS_NZ, the last `R0 =` before it, and the argument expression for the
rest). Handlers of the cutscene and map-script tables return 1 or 0 (`mov r0, #1/#0`) and the C sets
`SET_FLAGS_NZ(R0 | 1 | 0 | status)` right after assigning it; `SET_FLAGS_ADD` before it reproduces the `adds r7, #n`
c/v. No further BUG.

## Known blind spots

- C/V are only compared by presence of SET_FLAGS_CMP/ADD, not by value; no readable function has a C/V consumer
  (the six bge/bgt/bcc consumers call lifted functions).
- Return-path matching is by text order, not a control-flow graph of the C; a flag set in an unrelated branch
  just before a return would mask a missing one (the multi-return functions were read by hand).
- Flags consumed by a callee's own caller further up (a function that returns a flag it got from its caller as
  input, "entry flags") are listed as `| entry flags` only; the reverse direction (flags input to a function
  via `bl`-less fallthrough) is not covered.
- Indirect calls through a pointer that is not loaded by `ldr ... // =sym` within the same function (e.g. a
  pointer stored in RAM) are listed (`--sites`) but unresolved; the only unresolved site is the script macro
  one, handled by the `cs_call_native_with_return_value` extraction.
- Unreadable (lifted) functions are not checked; the lifter keeps flags per contract.

## Appendix: all 208 flags-returning functions with readable C

| function | marks | return setter (asm) | first consumer (asm) | C |
|---|---|---|---|---|
| CutsceneCmd_call_native_with_return_value | FEWER | map_script_cutscene.s:5686, map_script_cutscene.s:5689 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:3579 |
| CutsceneCmd_chatbox_cmd_8037a70 | HAS | map_script_cutscene.s:4730 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2785 |
| CutsceneCmd_cutscene_end | HAS | map_script_cutscene.s:4376 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2432 |
| CutsceneCmd_decomp_text_archive | HAS | map_script_cutscene.s:4808 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2898 |
| CutsceneCmd_do_camera_shake | HAS | map_script_cutscene.s:6057 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:4108 |
| CutsceneCmd_end_for_map_reload_maybe_8037c64 | HAS | map_script_cutscene.s:3976 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2045 |
| CutsceneCmd_jump_if_var_equal | HAS | map_script_cutscene.s:4485, map_script_cutscene.s:4489 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2568 |
| CutsceneCmd_kill_cutscene_process | HAS | map_script_cutscene.s:4438 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2489 |
| CutsceneCmd_move_player_in_facing_direction | FEWER | map_script_cutscene.s:5078, map_script_cutscene.s:5081 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:3106 |
| CutsceneCmd_navi_cmd_80340f6 | HAS | map_script_cutscene.s:6238 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:4391 |
| CutsceneCmd_nop_80377d0 | HAS | map_script_cutscene.s:4167, map_script_cutscene.s:4170 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2246 |
| CutsceneCmd_ow_player_sprite_special_with_arg | HAS | map_script_cutscene.s:4927 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2966 |
| CutsceneCmd_pause | FEWER | map_script_cutscene.s:4044, map_script_cutscene.s:4047 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2080 |
| CutsceneCmd_run_or_stop_cutscene_camera_script | HAS | map_script_cutscene.s:5937, map_script_cutscene.s:5946 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:3927 |
| CutsceneCmd_run_text_script | HAS | map_script_cutscene.s:4706, map_script_cutscene.s:4715 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2738 |
| CutsceneCmd_set_cutscene_skip_script | HAS | map_script_cutscene.s:4458 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2539 |
| CutsceneCmd_set_or_clear_chatbox_flags | HAS | map_script_cutscene.s:4761 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2801 |
| CutsceneCmd_set_var | HAS | map_script_cutscene.s:4682 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2709 |
| CutsceneCmd_spawn_cutscene_process | HAS | map_script_cutscene.s:4418 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2442 |
| CutsceneCmd_start_fixed_battle | HAS | map_script_cutscene.s:5982 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:3972 |
| CutsceneCmd_store_or_load_game_progress_buffer_maybe_803843c | HAS | map_script_cutscene.s:6479 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:4518 |
| CutsceneCmd_switch_case_from_chatbox_flags_bit0_to_2 | HAS | map_script_cutscene.s:4784 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2865 |
| CutsceneCmd_wait_camera_script | HAS | map_script_cutscene.s:4207, map_script_cutscene.s:4210 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2324 |
| CutsceneCmd_wait_chatbox | HAS | map_script_cutscene.s:4121, map_script_cutscene.s:4124 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2179 |
| CutsceneCmd_wait_cutscene_process | HAS | map_script_cutscene.s:4261, map_script_cutscene.s:4264 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2398 |
| CutsceneCmd_wait_if_player_sprite_cur_frame_not_equal_maybe | HAS | map_script_cutscene.s:4145, map_script_cutscene.s:4148 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2216 |
| CutsceneCmd_wait_screen_fade | HAS | map_script_cutscene.s:4186, map_script_cutscene.s:4189 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2276 |
| CutsceneCmd_wait_var_equal | HAS | map_script_cutscene.s:4236, map_script_cutscene.s:4239 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:2350 |
| CutsceneCmd_warp_cmd_8038040 | FEWER | map_script_cutscene.s:5743, map_script_cutscene.s:5753, map_script_cutscene.s:5759 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:3605 |
| CutsceneCmd_write_or_offset_ow_player_fixed_anim_select_8037dac | HAS | map_script_cutscene.s:5276 | map_script_cutscene.s:6656 | cport/src/game/map_script_cutscene.c:3243 |
| GetRandomRelativePanelFiltered | PASS | - | asm32.s:7511 | cport/src/game/asm00_2_8012BA2.c:2908 |
| IsButtonPressed | HAS | asm32.s:36864 | asm00_1.s:5741 | cport/src/game/asm32_811CAEC.c:2043 |
| IsCutsceneScriptNonNull | HAS | map_script_cutscene.s:2695 | asm00_1.s:5353 | cport/src/game/map_script_cutscene.c:1469 |
| IsGFXAnimActive | HAS | asm00_0.s:3988 | asm32.s:35570 | cport/src/game/asm00_0.c:2338 |
| IsNotSlipRunningAndNotInCutscene | FEWER | asm03_1_0.s:2185, asm03_1_0.s:2188 | asm03_1_0.s:2200 | cport/src/game/asm03_1_0.c:1584 |
| MapScriptCmd_end | HAS | map_script_cutscene.s:83 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:11 |
| MapScriptCmd_jump_if_coord_interaction_value_not_equal | HAS | map_script_cutscene.s:572, map_script_cutscene.s:576 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:417 |
| MapScriptCmd_jump_if_map_group_compare_last_map_group | HAS | map_script_cutscene.s:729, map_script_cutscene.s:733 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:584 |
| MapScriptCmd_jump_if_player_z_equals | FEWER | map_script_cutscene.s:604, map_script_cutscene.s:608 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:455 |
| MapScriptCmd_jump_if_player_z_not_equal | FEWER | map_script_cutscene.s:636, map_script_cutscene.s:640 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:515 |
| MapScriptCutsceneCmd_add_bbs_message_range | HAS | map_script_cutscene.s:6207 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:4296 |
| MapScriptCutsceneCmd_add_mail_range | HAS | map_script_cutscene.s:6288 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:4435 |
| MapScriptCutsceneCmd_clear_event_flag_range | HAS | map_script_cutscene.s:1043 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:798 |
| MapScriptCutsceneCmd_coordinate_trigger_not_equal_cmd_8035b44 | HAS | map_script_cutscene.s:515, map_script_cutscene.s:519 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:303 |
| MapScriptCutsceneCmd_do_pet_effect | HAS | map_script_cutscene.s:1369 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:1107 |
| MapScriptCutsceneCmd_end_scenario_effect | HAS | map_script_cutscene.s:6148 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:4205 |
| MapScriptCutsceneCmd_give_or_take_item | R0AFTER/HAS | map_script_cutscene.s:5906 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:3883 |
| MapScriptCutsceneCmd_init_minigame_effect | HAS | map_script_cutscene.s:6168 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:4254 |
| MapScriptCutsceneCmd_init_scenario_effect | HAS | map_script_cutscene.s:6132 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:4155 |
| MapScriptCutsceneCmd_jump | HAS | map_script_cutscene.s:103 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:20 |
| MapScriptCutsceneCmd_jump_if_battle_result_equals | HAS | map_script_cutscene.s:4513, map_script_cutscene.s:4517 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:2604 |
| MapScriptCutsceneCmd_jump_if_current_navi_not_equal | FEWER | map_script_cutscene.s:4586, map_script_cutscene.s:4590 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:2645 |
| MapScriptCutsceneCmd_jump_if_flag_range_clear | HAS | map_script_cutscene.s:274, map_script_cutscene.s:278 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:261 |
| MapScriptCutsceneCmd_jump_if_flag_range_set | HAS | map_script_cutscene.s:209, map_script_cutscene.s:213 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:169 |
| MapScriptCutsceneCmd_jump_if_flag_set | FEWER | map_script_cutscene.s:176, map_script_cutscene.s:180 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:86 |
| MapScriptCutsceneCmd_jump_if_progress_in_range | HAS | map_script_cutscene.s:135, map_script_cutscene.s:139 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:42 |
| MapScriptCutsceneCmd_load_gfx_anim | HAS | map_script_cutscene.s:1265 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:1008 |
| MapScriptCutsceneCmd_load_gfx_anims | HAS | map_script_cutscene.s:1287 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:1038 |
| MapScriptCutsceneCmd_play_music | HAS | map_script_cutscene.s:5812 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:3755 |
| MapScriptCutsceneCmd_play_sound | HAS | map_script_cutscene.s:5786 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:3736 |
| MapScriptCutsceneCmd_run_eStruct200a6a0_callback | HAS | map_script_cutscene.s:6033 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:4077 |
| MapScriptCutsceneCmd_set_enter_map_screen_fade | HAS | map_script_cutscene.s:938 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:691 |
| MapScriptCutsceneCmd_set_event_flag | HAS | map_script_cutscene.s:967 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:724 |
| MapScriptCutsceneCmd_set_event_flag_range | HAS | map_script_cutscene.s:1017 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:759 |
| MapScriptCutsceneCmd_set_screen_fade | HAS | map_script_cutscene.s:914 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:636 |
| MapScriptCutsceneCmd_sound_cmd_80380ea | HAS | map_script_cutscene.s:5837 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:3786 |
| MapScriptCutsceneCmd_sound_cmd_803810e | HAS | map_script_cutscene.s:5862 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:3847 |
| MapScriptCutsceneCmd_terminate_one_or_all_gfx_anims | HAS | map_script_cutscene.s:1335, map_script_cutscene.s:1340 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:1071 |
| MapScriptCutsceneCmd_write_byte | HAS | map_script_cutscene.s:1151 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:878 |
| MapScriptCutsceneCmd_write_eStruct2001c04_byte | HAS | map_script_cutscene.s:1243 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:978 |
| MapScriptCutsceneCmd_write_gamestate_byte | HAS | map_script_cutscene.s:1218 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:944 |
| MapScriptCutsceneCmd_write_word | HAS | map_script_cutscene.s:1193 | map_script_cutscene.s:1471 | cport/src/game/map_script_cutscene.c:911 |
| PlaySoundEffect | PASS | - | asm36.s:7927 | cport/src/game/asm00_0.c:11 |
| RunLMessageTextScript | HAS | asm28_0.s:516 | asm28_0.s:436 | cport/src/game/asm28_0.c:349 |
| SWI_CpuSet | NOFLAGS | libs.s:20553 | asm00_0.s:625 | cport/src/game/libs.c:855 |
| StartCutscene | PASS | - | asm37_1.s:285 | cport/src/game/map_script_cutscene.c:1331 |
| TestBattleFlag_0x40 | HAS | asm03_0.s:17605 | asm00_1.s:11713 | cport/src/game/asm03_0_80299B6.c:3363 |
| TestCutsceneFlag | R0AFTER/HAS | map_script_cutscene.s:6777 | map_script_cutscene.s:4009 | cport/src/game/map_script_cutscene.c:4652 |
| TestEventFlag | HAS | asm03_0.s:20023 | asm00_0.s:1961 | cport/src/game/event_flags.c:66 |
| TestEventFlagFromImmediate | PASS |  | asm00_0.s:1789 | cport/src/game/event_flags.c:67 |
| TestEventFlagRange | NOFLAGS | asm03_0.s:20308 | asm24.s:1606 | cport/src/game/event_flags.c:117 |
| TestEventFlagRangeFromImmediate | PASS |  | asm00_0.s:1873 | cport/src/game/event_flags.c:118 |
| TestPETMenuDataFlag | HAS | asm32.s:35304 | asm00_1.s:5347 | cport/src/game/asm32_811CAEC.c:677 |
| TryCutsceneSkip | HAS | map_script_cutscene.s:2777 | map_script_cutscene.s:6692 | cport/src/game/map_script_cutscene.c:1557 |
| ZeroFillByWord | CALLAFTER/R0AFTER/PASS | - | map_script_cutscene.s:2608 | cport/src/game/asm00_0.c:368 |
| aiAttackingBuster_8013396 | FEWER | asm00_2.s:10642, asm00_2.s:10645 | asm00_2.s:10230 | cport/src/game/asm00_2_8012BA2.c:379 |
| aiTrySelectChipFromHand_801336C | RETNF/CALLAFTER/HAS | -, asm00_2.s:10625 | asm00_2.s:10221 | cport/src/game/asm00_2_8012BA2.c:328 |
| battleFindEntryByFieldByte_8047456 | HAS | asm03_2.s:2643 | asm03_2.s:2516 | cport/src/game/asm03_2.c:1603 |
| battleObject_checkPwrAtkTargetSwitch_8119880 | FEWER | asm32.s:24625, asm32.s:24629 | asm32.s:24329 | cport/src/game/asm32_8119438.c:501 |
| battleObject_scenarioEffectInputHandler_8089704 | HAS | asm23.s:1048 | asm23.s:951 | cport/src/game/asm23.c:11 |
| battleObject_selectAIDataHandler_81347A4 | HAS | asm36.s:9638 | asm36.s:7551 | cport/src/game/asm36.c:3002 |
| battleObject_submenuHandleBPress_8133BB4 | HAS | asm36.s:7871 | asm36.s:7403 | cport/src/game/asm36.c:2100 |
| battleObject_submenuHandleExitStartOrB_8133C0C | HAS | asm36.s:7925 | asm36.s:7405 | cport/src/game/asm36.c:2207 |
| battle_chatbox_setCursorTextFromEventFlag_808CF8C | HAS | asm24.s:1819 | asm24.s:1378 | cport/src/game/asm24.c:1180 |
| battle_checkAlliancePanelAtObjectPos | HAS | asm31.s:185909 | asm31.s:185882 | cport/src/game/asm31_80EBE00.c:11041 |
| battle_checkLockedEventTable_808CDC4 | HAS | asm24.s:1627 | asm24.s:1385 | cport/src/game/asm24.c:868 |
| battle_checkNaviFreeze_800A1D0 | FEWER | asm00_1.s:16285, asm00_1.s:16288 | asm00_1.s:11282 | cport/src/game/asm00_1_800801C.c:1331 |
| battle_checkPanelPlacement_813BB00 | HAS | asm37_0.s:708 | asm37_0.s:640 | cport/src/game/asm37_0.c:90 |
| battle_checkPlayerActorStoryFlag_808CC10 | HAS | asm24.s:1351 | asm24.s:1373 | cport/src/game/asm24.c:586 |
| battle_getLinkStateCode_803F560 | HAS | asm03_1_1.s:14089 | asm03_1_1.s:12121 | cport/src/game/asm03_1_1.c:2793 |
| battle_initObjectSpriteAndStartAnimation_811CC0C | HAS | asm32.s:31285 | asm32.s:31069 | cport/src/game/asm32_811CAEC.c:106 |
| battle_isBattleOver | HAS | asm00_1.s:16209 | asm00_1.s:17014 | cport/src/game/asm00_1_800801C.c:1294 |
| battle_isPaused | HAS | asm00_1.s:15937 | asm00_1.s:17012 | cport/src/game/asm00_1_800801C.c:1096 |
| battle_isTimeStop | HAS | asm00_1.s:16010 | asm00_1.s:121 | cport/src/game/battle_helpers.c:21 |
| battle_main_8007800 | HAS | asm00_1.s:10005 | asm00_1.s:4786 | cport/src/game/asm00_1_8005C04.c:2239 |
| battle_matchTrackedSpriteAndShrink_811CBD0 | HAS | asm32.s:31256 | asm32.s:31069 | cport/src/game/asm32_811CAEC.c:52 |
| battle_networkInvert | HAS | asm00_1.s:17676 | asm00_2.s:12872 | cport/src/game/asm00_1_800801C.c:2195 |
| battle_objectCheckPanelByAlliance_800E5AC | RETNF/CALLAFTER/R0AFTER/HAS | -, object.s:5464 | asm00_2.s:18102 | cport/src/game/object.c:2153 |
| battle_pickRandomRowObjectFields_810971A | HAS | asm31.s:181783 | asm32.s:7489 | cport/src/game/asm31_80EBE00.c:9630 |
| battle_scanPanelsInFront_800D58C | HAS | object.s:4302 | asm31.s:91610 | cport/src/game/object.c:1535 |
| battle_testFlag0x40_800A8F8 | HAS | asm00_1.s:17476 | asm00_2.s:4553 | cport/src/game/asm00_1_800801C.c:2026 |
| camera_followOverworldPlayer | HAS | asm28_0.s:854 | asm28_0.s:770 | cport/src/game/asm28_0.c:542 |
| camera_restartWithScriptHead | HAS | asm28_0.s:4606 | asm28_0.s:4349 | cport/src/game/asm28_0.c:747 |
| camera_sineEasePanStep | HAS | asm29.s:5323, asm29.s:5332 | asm29.s:5145 | cport/src/game/asm29.c:1890 |
| chatbox_GetState_8045F4C | HAS | chatbox.s:10884 | asm36.s:12088 | cport/src/game/chatbox.c:3761 |
| chatbox_maskFlags_3e | HAS | chatbox.s:1809 | chatbox.s:343 | cport/src/game/chatbox_flags.c:30 |
| chatbox_mask_eFlags2009F38 | HAS | chatbox.s:10866 | asm00_1.s:5681 | cport/src/game/chatbox.c:3750 |
| chatbox_uncompMapTextArchives_803FD54 | HAS | chatbox.s:65 | asm28_0.s:438 | cport/src/game/chatbox.c:53 |
| checkBattleOrderMatch_802CE30 | FEWER | asm03_0.s:14928, asm03_0.s:14931 | asm31.s:39047 | cport/src/game/asm03_0_80299B6.c:2679 |
| checkCurrentMapEventFlag_80902DC | PASS | - | asm25.s:1338 | cport/src/game/asm25.c:1195 |
| checkLeftRightInputForFolderEdit_8133E3C | HAS | asm36.s:8225 | asm36.s:7424 | cport/src/game/asm36.c:2287 |
| checkObjectField20Zero_808FF8C | HAS | asm25.s:1100 | asm25.s:1248 | cport/src/game/asm25.c:750 |
| checkScrambledCounter_8047366 | HAS | asm03_2.s:2483 | asm03_2.s:1794 | cport/src/game/asm03_2.c:1581 |
| checkSubEntryHiddenFlag_8002E04 | HAS | sprite.s:1341 | asm28_0.s:3597 | cport/src/game/sprite.c:605 |
| check_playerCanAdvanceForward_8034C36 | FEWER | asm03_1_0.s:2142, asm03_1_0.s:2145 | asm28_0.s:4810 | cport/src/game/asm03_1_0.c:1464 |
| claimNextPendingRequest_8035756 | HAS | asm03_1_0.s:3660 | asm03_1_0.s:2741 | cport/src/game/asm03_1_0.c:2637 |
| cross_loadEntryFields_8098CCC | HAS | asm28_0.s:117 | asm28_0.s:54 | cport/src/game/asm28_0.c:82 |
| cutscene_checkOriginalCutsceneScriptPos_8036F40 | HAS | map_script_cutscene.s:2715 | asm03_2.s:10278 | cport/src/game/map_script_cutscene.c:1481 |
| cutscene_checkOriginalCutsceneScriptPos_8036F58 | HAS | map_script_cutscene.s:2737 | asm03_2.s:10280 | cport/src/game/map_script_cutscene.c:1497 |
| cutscene_checkOriginalCutsceneScriptPos_8036F70 | HAS | map_script_cutscene.s:2760 | asm29.s:6651 | cport/src/game/map_script_cutscene.c:1518 |
| e_setEventFlagFromStruct | HAS | asm28_0.s:4797 | asm28_0.s:4425 | cport/src/game/asm28_0.c:1012 |
| encryption_bugfrags_8006fd0 | HAS | asm00_1.s:8651 | asm03_1_1.s:9417 | cport/src/game/asm00_1_8005C04.c:1500 |
| encryption_navicustMaybe_8006e50 | HAS | asm00_1.s:8371 | asm03_1_1.s:9052 | cport/src/game/asm00_1_8005C04.c:1360 |
| encryption_testPack_8006e84 | HAS | asm00_1.s:8415 | asm00_1.s:8434 | cport/src/game/asm00_1_8005C04.c:1396 |
| encryption_zenny_8006f78 | HAS | asm00_1.s:8588 | asm03_1_1.s:9320 | cport/src/game/asm00_1_8005C04.c:1443 |
| eventFlag1720IsSet_813BF1C | HAS | asm37_0.s:1315 | asm00_2.s:33405 | cport/src/game/asm37_0.c:667 |
| event_TestPlayerAdvanceForward_809DB50 | HAS | ow_player.s:1392 | ow_player.s:406 | cport/src/game/ow_player.c:197 |
| getStructFrom2008450 | HAS | asm03_2.s:990 | asm03_2.s:9274 | cport/src/game/asm03_2.c:454 |
| infoText_IsTimestampUnchanged_803F8F4 | HAS | asm03_1_1.s:14603, asm03_1_1.s:14606 | asm03_1_1.s:14479 | cport/src/game/asm03_1_1.c:3084 |
| initWorkArea5Entries_80467D8 | R0AFTER/HAS | asm03_2.s:1151 | C: reads result flag at asm03_2.c:621 | cport/src/game/asm03_2.c:564 |
| init_beastSpScenario | HAS | asm24.s:1154 | asm24.s:627 | cport/src/game/asm24.c:11 |
| isSameSubsystem_800A732 | HAS | asm00_1.s:17128 | main.s:46 | cport/src/game/asm00_1_800801C.c:1767 |
| library_isCommsBusy_8146BF4 | HAS | libs.s:6219 | asm03_1_1.s:7682 | cport/src/game/libs.c:250 |
| link_getModeSelector_803DD30 | HAS | asm03_1_1.s:10162 | asm37_0.s:4938 | cport/src/game/asm03_1_1.c:2251 |
| loadWorkByteFromStepTable_809C830 | HAS | asm28_0.s:4672 | asm28_0.s:4352 | cport/src/game/asm28_0.c:890 |
| m4a_800061E | PASS | - | asm00_0.s:39 | cport/src/game/asm00_0.c:88 |
| navicust_checkPieceOverlap | FEWER | asm37_0.s:759 | asm36.s:15303 | cport/src/game/asm37_0.c:172 |
| navicust_checkRowOccupied | FEWER | asm37_0.s:1373, asm37_0.s:1388 | asm36.s:15082 | cport/src/game/asm37_0.c:699 |
| notZero_eByte200AD04 | HAS | asm03_1_1.s:14007 | asm00_2.s:3235 | cport/src/game/asm03_1_1.c:2730 |
| npc_checkStateEventFlag_80871C0 | PASS | - | asm22.s:178 | cport/src/game/asm22.c:214 |
| obj_decrementFixedCounterBy4_80A856C | FEWER | asm29.s:6793 | asm29.s:6629 | cport/src/game/asm29.c:2971 |
| object_canMove | HAS | object.s:5480, object.s:5483 | asm31.s:115765 | cport/src/game/object.c:2202 |
| object_checkPanelParameters | HAS | object.s:2881, object.s:2884 | asm31.s:12228 | cport/src/game/object.c:1169 |
| object_checkStatusFlag_811BE90 | HAS | asm32.s:29932 | asm32.s:29813 | cport/src/game/asm32_8119438.c:2878 |
| object_getClosestPanelMatchingRowFiltered | HAS | object.s:3534 | asm32.s:9501 | cport/src/game/object.c:1408 |
| object_isCurrentPanelSolid | PASS | - | asm31.s:30852 | cport/src/game/object.c:1219 |
| object_isCurrentPanelValid | PASS | - | asm31.s:91897 | cport/src/game/object.c:1156 |
| object_isPanelSolid | HAS | object.s:2909 | asm31.s:29824 | cport/src/game/object.c:1232 |
| object_isValidPanel | HAS | object.s:2849, object.s:2852 | asm00_2.s:1133 | cport/src/game/battle_helpers.c:92 |
| object_setStateHandlerAndReturnOne_811CC02 | HAS | asm32.s:31268 | asm32.s:31069 | cport/src/game/asm32_811CAEC.c:93 |
| object_startFacingStep | HAS | asm29.s:5272 | asm29.s:5145 | cport/src/game/asm29.c:1804 |
| overworld_leaveArea_808CBD8 | HAS | asm24.s:1325 | asm24.s:1107 | cport/src/game/asm24.c:517 |
| owMapObject_isHoveringElevator_0809E3A2 | HAS | ow_player.s:2806 | asm29.s:5632 | cport/src/game/ow_player.c:1244 |
| owMapObject_weatherTornado_isBlocked_80A90CC | HAS | asm29.s:8401 | asm29.s:8156 | cport/src/game/asm29.c:3767 |
| owMapObject_weatherTornado_isPauseDue_80A9150 | HAS | asm29.s:8486 | asm29.s:8170 | cport/src/game/asm29.c:3880 |
| owPlayer_isBusyOrCutsceneActive_809E462 | HAS | ow_player.s:2968 | asm00_1.s:5340 | cport/src/game/ow_player.c:1393 |
| owPlayer_setFixedAnimationFromLookup | HAS | asm28_0.s:4645 | asm28_0.s:4351 | cport/src/game/asm28_0.c:837 |
| ow_applySpawnEntry | HAS | asm28_0.s:834 | asm28_0.s:755 | cport/src/game/asm28_0.c:445 |
| ow_cutsceneLoadEntry | HAS | asm28_0.s:410 | asm28_0.s:297 | cport/src/game/asm28_0.c:227 |
| ow_periodicObjectSpawner | FEWER | asm28_0.s:3163, asm28_0.s:3186 | asm28_0.s:3009 | cport/src/game/asm28_0.c:662 |
| ow_player_updateCoords_809C7FC | HAS | asm28_0.s:4628 | asm28_0.s:4350 | cport/src/game/asm28_0.c:800 |
| pet_exitToPetMenuReturnZero_809ADA8 | HAS | asm28_0.s:2820 | asm28_0.s:2808 | cport/src/game/asm28_0.c:570 |
| playerAdvanceForwardCheck_8034C6E | FEWER | asm03_1_0.s:2167, asm03_1_0.s:2170 | asm03_1_0.s:2079 | cport/src/game/asm03_1_0.c:1526 |
| playerObjectMovingToPanelValidityRelated_800E618 | NOFLAGS | -, object.s:5548 | asm00_2.s:1915 | cport/src/game/object.c:2239 |
| reqBBS_pollTouchTrigger_808CD04 | HAS | asm24.s:1513 | asm24.s:1237 | cport/src/game/asm24.c:730 |
| returnBoolIfAIDataFlagsAreSet_8012F3E | FEWER | asm00_2.s:10040, asm00_2.s:10043 | asm00_2.s:9955 | cport/src/game/asm00_2_8012BA2.c:200 |
| save_BackupToGamePakWithRetry_803FAB4 | HAS | asm03_1_1.s:14867, asm03_1_1.s:14870 | asm03_1_1.s:14387 | cport/src/game/asm03_1_1.c:3111 |
| sound_8000808 | HAS | asm00_0.s:440 | asm00_0.s:117 | cport/src/game/asm00_0.c:310 |
| sprite_testFrameParamsMaskC0_81190F8 | HAS | asm32.s:23658 | asm33.s:730 | cport/src/game/asm32.c:10180 |
| startScreen_IsTitleAnimationFinished_803E928 | HAS | asm03_1_1.s:12005 | asm03_0.s:20864 | cport/src/game/asm03_1_1.c:2383 |
| startScreen_TstZero | HAS | asm03_1_1.s:12015 | asm03_0.s:20633 | cport/src/game/asm03_1_1.c:2395 |
| story_checkEntryAlliance_8034E88 | FEWER | asm03_1_0.s:2461 | asm03_1_0.s:2353 | cport/src/game/asm03_1_0.c:1651 |
| stub_doNothingReturnZero_8047CA0 | HAS | asm03_2.s:3812 | asm03_2.s:3558 | cport/src/game/asm03_2.c:2041 |
| sub_8034E3C | HAS | asm03_1_0.s:2421 | asm03_1_0.s:2350 | cport/src/game/asm03_1_0.c:1617 |
| sub_803BB80 | HAS | asm03_1_1.s:6222, asm03_1_1.s:6225 | asm03_1_1.s:1824 | cport/src/game/asm03_1_1.c:838 |
| sub_803C248 | R0AFTER/HAS | asm03_1_1.s:7067 | asm03_1_1.s:4262 | cport/src/game/asm03_1_1.c:1154 |
| sub_803C3E0 | HAS | asm03_1_1.s:7351 | asm00_1.s:4092 | cport/src/game/asm03_1_1.c:1230 |
| sub_8047CB0 | HAS | asm03_2.s:3830 | asm03_2.s:3700 | cport/src/game/asm03_2.c:2051 |
| sub_8049D42 | R0AFTER/HAS | asm03_2.s:6660 | asm03_2.s:4947 | cport/src/game/asm03_2.c:3331 |
| sub_808D25C | HAS | asm24.s:2183 | asm24.s:2142 | cport/src/game/asm24.c:1403 |
| sub_8098E24 | HAS | asm28_0.s:205 | asm28_0.s:149 | cport/src/game/asm28_0.c:113 |
| sub_8098E6C | HAS | asm28_0.s:240 | asm28_0.s:155 | cport/src/game/asm28_0.c:193 |
| sub_811CC26 | HAS | asm32.s:31301 | asm32.s:31069 | cport/src/game/asm32_811CAEC.c:128 |
| sub_811CC3A | HAS | asm32.s:31318 | asm32.s:31069 | cport/src/game/asm32_811CAEC.c:144 |
| sub_81279A8 | HAS | asm33.s:9458 | asm29.s:10954 | cport/src/game/asm33_81263D0.c:1729 |
| submenu_compareCursorBytes_811A24C | HAS | asm32.s:25921 | asm32.s:25805 | cport/src/game/asm32_8119438.c:1470 |
| submenu_isOpen_81355D8 | HAS | asm36.s:11537 | asm32.s:20708 | cport/src/game/asm36.c:3921 |
| submenu_isStateChanged_81198B0 | FEWER | asm32.s:24667, asm32.s:24671 | asm32.s:24467 | cport/src/game/asm32_8119438.c:552 |
| test0x200bc50_0x5_813D60C | HAS | asm37_0.s:4074 | asm01.s:528 | cport/src/game/asm37_0.c:1330 |
| tickCountdownGate_803F530 | HAS | asm03_1_1.s:14037 | asm00_1.s:4723 | cport/src/game/asm03_1_1.c:2741 |
| timerCountdown_step_811CBA0 | HAS | asm32.s:31225, asm32.s:31228 | asm32.s:31069 | cport/src/game/asm32_811CAEC.c:11 |
| verifyEwramWorkAreaChecksum_803F8C4 | FEWER | asm03_1_1.s:14572, asm03_1_1.s:14575 | asm03_1_1.s:14477 | cport/src/game/asm03_1_1.c:3063 |
| warp_LoadCurrentAndReadUnk10_8005F28 | HAS | asm00_1.s:6414 | asm00_1.s:5343 | cport/src/game/asm00_1_8005C04.c:436 |
| weatherComp_checkRingCapture_809019C | HAS | asm25.s:1441 | asm25.s:909 | cport/src/game/asm25.c:1050 |
| weatherComp_checkTrigger60_8090020 | HAS | asm25.s:1210 | asm25.s:1333 | cport/src/game/asm25.c:843 |
| weatherComp_checkTrigger70_8090004 | HAS | asm25.s:1189 | asm25.s:1328 | cport/src/game/asm25.c:810 |
| weatherComp_findRingCapture_80901EC | HAS | asm25.s:1473 | asm25.s:1370 | cport/src/game/asm25.c:1132 |
| weatherComp_layOrbTrail_809005C | HAS | asm25.s:1286 | asm25.s:911 | cport/src/game/asm25.c:936 |
