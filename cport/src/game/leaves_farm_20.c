/*
 * leaves_farm_20.c: small functions from the model farm, set 20.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── petMenu_updateObject_81169B0 ── */
/* petMenu_updateObject_81169B0: per-frame update of one PET menu object (object pointer in r5).
 *
 * The object's mode byte at (r5 + 9) selects an entry of the three-entry
 * handler table off_81169CC by byte offset (0, 4 or 8). The chosen handler
 * runs with r7 = ePETMenuData and r1 = the mode byte, and returns to
 * 0x81169BE (the "mov lr, pc; bx r0" idiom), i.e. straight into the
 * sprite update below. Afterwards the object's sprite is refreshed with
 * sprite_update, then r7 is restored and the function returns to its own
 * caller via pop {r7,pc} -- which resumes at the lr saved on entry, NOT at
 * the link register left behind by the internal calls. */
Next petMenu_updateObject_81169B0(void) {
    u32 returnAddr = LR;    /* push {r7,lr}: pop {pc} returns here */
    u32 savedR7 = R7;

    R7 = A_ePETMenuData;

    /* ldrb r1,[r5,#9]; ldr r0,[table,r1]; mov lr,pc; bx r0 */
    u32 mode = LD8(R5 + 9);
    R1 = mode;
    LR = 0x81169BE;                      /* handlers return into the bl below */
    R0 = LD32(A_off_81169CC + mode);     /* table entry: handler address | 1 */

    switch (mode) {
    case 0: GUEST_CALL(npc_tickDisappearTimer_81169D8); break;
    case 4: GUEST_CALL(battle_reloadObjectSprite_8116A04); break;
    case 8: GUEST_CALL(sub_8116A28); break;
    default: break;  /* unreachable: the mode byte is always 0, 4 or 8 */
    }

    /* bl sprite_update */
    LR = 0x81169C2 | 1u;
    GUEST_CALL(sprite_update);

    /* pop {r7,pc}: restore the caller's r7 and return to the address that
     * was saved on entry, not to sprite_update's return link. */
    R7 = savedR7;
    LR = returnAddr;
    RETURN_TO_CALLER();
}
