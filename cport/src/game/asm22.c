/*
 * asm22.c: readable functions of bn6f asm22.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── npc_checkStateEventFlag_80871C0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n; } call_TestEventFlag_result_sub_80871C0;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_sub_80871C0 call_TestEventFlag_sub_80871C0(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_sub_80871C0){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Check the event flag selected by the caller's state.
 * The byte at r5+0xC, doubled, indexes a table of u16 flag ids at
 * byte_80871D8; TestEventFlag is called with that id.  This function's
 * only observable result is the N/Z state TestEventFlag leaves behind
 * (Z clear when the flag is set), so the typed function just performs
 * the call and lets those flags stand. */
static void check_state_event_flag(u32 statePtr) {
    u32 offset = (u32)LD8(statePtr + 0xC) << 1;
    u32 flagId = LD16(A_byte_80871D8 + offset);
    call_TestEventFlag_sub_80871C0(flagId);
}

Next npc_checkStateEventFlag_80871C0(void) {
    check_state_event_flag(R5);
    RETURN_TO_CALLER();
}
