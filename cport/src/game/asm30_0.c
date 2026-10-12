/*
 * asm30_0.c: readable functions of bn6f asm30_0.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_80AED50 ── */
/* sub_80AED50(r0 = enemy index): resolve an enemy's AI data record.
 *
 * GetVerActorTyAndAIIdx_80182B4 maps the enemy index to its
 * { version, actor_type, ai_index } triple. The triple indexes the two-level
 * table at off_80AED74 — actor type picks a sub-table, AI index picks an
 * entry — and version * 0x20 steps to the version's record within it.
 * Returns r0 = record pointer, r2 = ai_index * 4, r3 = version * 0x20.
 */
Next sub_80AED50(void) {
    GUEST_CALL(GetVerActorTyAndAIIdx_80182B4);

    /* *const (version: u8, actor_type: ActorType, ai_index: u8) */
    u32 version    = LD8(R0 + 0);
    u32 actor_type = LD8(R0 + 1);
    u32 ai_index   = LD8(R0 + 2);

    u32 sub_table = LD32(A_off_80AED74 + actor_type * 4);
    u32 record    = LD32(sub_table + ai_index * 4) + version * 0x20;

    R0 = record;
    R2 = ai_index * 4;
    R3 = version * 0x20;
    RETURN_TO_CALLER();
}
