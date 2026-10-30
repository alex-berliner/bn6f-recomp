# Shift-into-carry sweep

Generated candidates with `tools/check_shift_carry.py` (Thumb `lsl/lsr/asr/ror` followed within two
non-flag-setting instructions by `bcc/bcs/blo/bhs`, inside a function with a readable `Next NAME(void)`
body). Judgement below is manual. Carry after `lsr #N` = bit N-1; after `lsl #N` = bit 32-N.
The script finds 188 shift+carry-branch sites in total; 19 are in 19 converted functions (the rest
are in unconverted/lifted functions). Result: 19 checked, 1 WRONG (fixed), 18 OK.
Not covered: carry set by `movs/adds/subs/cmp` or register-amount shifts (the script finds no
register-amount shift feeding a carry branch in the converted functions).

asm lines are in bn6f/asm/*.s. "branch" = asm skips/exits when the condition holds.

| function | asm | C | asm tests | C tests | verdict |
|---|---|---|---|---|---|
| QueueGFXTransfersInList | asm00_0.s:1211 (lsl #1, bcs = packed) | asm00_0.c:714 | bit 31 set | `source & 0x80000000` | OK |
| battle_objectIntroEntryAndActedSetup_8016460 | asm00_2.s:17602 | asm00_2_8012BA2.c:3232 | bit 1 clear -> exit | `(state>>1)&1` | OK |
| battleObject_stepSprite_80B86A4 | asm31.s:681 | asm31.c:355 | bit 1 clear -> exit | `state & 2` | OK |
| sub_80C088C | asm31.s:19772 | asm31.c:1714 | bit 0 of Timer (clear -> skip) | `tick & 1` | OK |
| battleObject_updateSpriteMosaicAndExitIfNotScaled_80C461C | asm31.s:28660 | asm31.c:3425 | bit 1 clear -> exit | was `(state>>2)&1` (bit 2) | **WRONG, fixed** -> `(state>>1)&1` |
| battleObject_throwAndLandAction_80C58E0 | asm31.s:31219 | asm31.c:4976 | bit 2 of Timer clear -> skip | `(timer>>2)&1` | OK |
| battle_phasePanelHighlight_80CC260 | asm31.s:46021 | asm31_80C6CE4.c:4395 | bit 2 of Timer2 set -> skip | `(timer2 & 4)==0` | OK |
| battleObject_updateMysteryData_80E336E | asm31.s:97984 | asm31_80E0F02.c:1720 | bit 1 of Timer set -> exit | `Timer & 2` -> return | OK |
| aiAttack_selectPanelTick_80F7ED8 | asm31.s:143741 | asm31_80EBE00.c:3144 | bit 2 of cycles set -> skip | `(cycles & 4)==0` | OK |
| battle_aiChargeAttackTick_80F8A66 | asm31.s:145293 | asm31_80EBE00.c:4359 | bit 2 set -> skip | `(cycles & 4)==0` | OK |
| sub_80FD7F2 | asm31.s:155624 | asm31_80EBE00.c:5613 | bit 2 of StepTimer set -> skip | `!(timer & 4)` | OK |
| sub_80FD822 | asm31.s:155658 | asm31_80EBE00.c:5648 | bit 2 set -> skip | `!(timer & 4)` | OK |
| ai_chargeShakeUpdate | asm31.s:157227 | asm31_80EBE00.c:6675 | bit 2 of new count set -> skip | `((step>>2)&1)==0` | OK |
| battle_aiAttackStep_80FEF60 | asm31.s:158798 | asm31_80EBE00.c:7314 | bit 2 set -> skip | `(charge & 4)==0` | OK |
| ai_chargeAttack_step_80FFD6A | asm31.s:160750 | asm31_80EBE00.c:9080 | bit 2 set -> exit | `(newCycles>>2)&1` -> return | OK |
| battle_aiAttackStep_810AF90 | asm31.s:184929 | asm31_80EBE00.c:10683 | bit 2 clear -> skip | `timer & 4` -> do | OK |
| ai_clearFlagAndSetCurAction | asm32.s:17553 | asm32.c:6409 | bit 1 clear -> skip | `(state>>1)&1` | OK |
| object_drawChipName | object.s:251 | object.c:311 | bit 1 of ChipData+9 clear -> skip | `(data[9]>>1)&1` | OK |
| battle_getHitModifierOffsets_800E548 | object.s:5406 (loop, lsr #1) | object.c:2121 | bit 0 of running value | `value & 1` | OK |
