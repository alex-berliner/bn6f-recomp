/*
 * asm21.c: readable functions of bn6f asm21.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── bgScroll_ResetScrollCBCounters_8080D90 ── */
/* bgScroll_ResetScrollCBCounters_8080D90: clear the BG scroll callback counters (8 bytes). */
Next bgScroll_ResetScrollCBCounters_8080D90(void) {
    R0 = A_eBGScrollCBCounters;
    R1 = 8;
    GUEST_CALL(ZeroFillByByte);
    RETURN_TO_CALLER();
}
