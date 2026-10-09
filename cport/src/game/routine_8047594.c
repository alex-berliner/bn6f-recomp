/*
 * routine_8047594.c — the chip list screen's five rows (bn6f asm03_2.s,
 * 0x08047594-0x08047800, with the helpers only it calls at
 * 0x08047CFA-0x08047D84).
 *
 * sub_8047594 draws five rows of a chip list: each row shows a chip's
 * name, its code letter and a small strip.  The rows come from an array
 * of 8-byte entries the caller scrolls by moving the index of the first
 * entry to show; row i shows entry first + i, and the entry's kind byte
 * picks the path:
 *   1  plain: name from the fixed name script text_873D9FC and a blank
 *      tile in the code column (no code letter)
 *   2  coded: name from the banked name tables TextScriptChipNames0/1
 *      (bank = chip >> 8, index = chip & 0xFF), then the code letter —
 *      the no-code letter in the library view for chips neither event
 *      flag (chip+0x1E20, chip+0x2420) marks
 *   3  secret: name row drawn by sub_8047708 (kept lifted)
 *   anything else: 16 blank tiles over the name area, and no strip
 * Kinds 2 and 3 are first dimmed (sub_8047CFA) when the player owns
 * none: kind 2 counts the pack (GetChipCountOfCode of chip and code),
 * kind 3 the chip+0x90 collection (sub_803D138).  Kinds 1-3 then get
 * their strip (sub_8047790): the entry's value halfword (+6) goes
 * through sub_8047CDC, which leaves it times 100 (unchanged in view 1),
 * and that result is stored at the toolkit's 200A220 struct (+8) — what
 * the number means is not shown by this code.  The strip itself takes
 * its variant from the screen's view word ([menu+0x1C] deref: 1 = the
 * dim strip, 2 = the library view, where a chip whose chip+0x2420 flag
 * is clear gets a blank strip) and from whether the chip has a code
 * (no code → the "no code" strip).
 *
 * The dimming repaints the strip tilemap entries byte_202F800 holds per
 * row (0x48 bytes: ten u16 at +0 and ten at +0x24): the low 12 bits of
 * each entry stay, 0x3000 is ORed in on even rows and 0x4000 on odd
 * rows, and palette << 13 is added on top.
 *
 * Every piece of text goes through renderTextGfx_8045F8C with the
 * decompression buffers at 0x2013A00 (names), 0x2013C00 (code column)
 * and 0x2013C40 (strips) — each screen row owns 0x400 bytes of buffer
 * and of the BG maps at 0x6008400 / 0x6008600 / 0x6008640 — and the
 * fixed colour table dword_86A5D60.
 *
 * The helpers are called only by sub_8047594, but their adapters still
 * leave every register the original leaves (its push/pop pattern
 * restores r0, r5 and r7 around the renders), so no promise is narrowed.
 */
#include "game.h"
#include "runtime.h"
#include <stdbool.h>

/* ── the entry array and the screen state ─────────────────────────── */

enum {  /* one 8-byte row of the list (asm03_2.s, not in the shared header) */
    ENTRY_KIND      = 0x00,  /* u8: which drawing path, see the header */
    ENTRY_HAS_CODE  = 0x01,  /* u8: set → the chip has a code letter */
    ENTRY_CHIP      = 0x02,  /* u16: chip id = bank << 8 | name index */
    ENTRY_CODE      = 0x04,  /* u8: the code letter, index into the code script */
    ENTRY_PALETTE   = 0x05,  /* u8: added to the strip's tiles as palette << 13 */
    ENTRY_VALUE     = 0x06,  /* u16: a number; sub_8047CDC leaves it times 100
                                (view 1: unchanged) and that result goes to Unk200a220+8 */
    ENTRY_SIZE      = 0x08,
};

enum { KIND_PLAIN = 1, KIND_CODED = 2, KIND_SECRET = 3 };

enum {  /* the screen state passed in r5 */
    MENU_VIEW_PTR      = 0x1C,  /* u32* → the view word below */
    MENU_SYMBOL_SCRIPT = 0x24,  /* the script with the blank / strip tiles */
};

enum {  /* the view word at [MENU_VIEW_PTR] */
    VIEW_DIMMED  = 1,  /* every strip takes the dim variant */
    VIEW_LIBRARY = 2,  /* unregistered chips lose their code and their strip */
};

enum {  /* event flags checked as chip id + base (which is which is a guess) */
    FLAG_CHIP_KNOWN = 0x1E20,
    FLAG_CHIP_OWNED = 0x2420,
};

/* ── the drawing surfaces ─────────────────────────────────────────── */

#define ROW_BYTES 0x400  /* each screen row owns this much of every buffer and map */

#define NAME_SCRIPT  A_text_873D9FC       /* the fixed name script (kind 1) */
#define CODE_SCRIPT  A_TextScript86EF71C  /* the code letters */
#define NAME_BANKS   A_off_80476E4        /* {TextScriptChipNames0, TextScriptChipNames1} */
#define DECOMP_NAMES A_eDecompBuffer2013A00
#define DECOMP_CODE  A_unk_2013C00
#define DECOMP_STRIP A_unk_2013C40
#define VRAM_NAMES   0x6008400
#define VRAM_CODE    0x6008600
#define VRAM_STRIP   0x6008640
#define COLOUR_LUT   A_dword_86A5D60

enum {  /* char indices into the scripts */
    STRIP_DIM    = 0x0D,  /* symbol script: the dim strip variant */
    STRIP_NORMAL = 0x0F,  /* symbol script: the strip */
    SYM_BLANK    = 0x10,  /* symbol script: the blank tile / blank strip */
    SYM_NO_CODE  = 0x11,  /* symbol script: the "no code" strip */
    CODE_NONE    = 0x1B,  /* code script: the no-code letter */
};

/* ── shared helpers ───────────────────────────────────────────────── */

/* renderTextGfx_8045F8C draws `count` tiles of char `code` of `script`,
 * decompressed through `decomp` into the BG map at `vram`; every call
 * here uses mode 1, no flip and the fixed colour table. */
static void render_tiles(u32 script, u32 code, u32 decomp, u32 vram, u32 count) {
    R0 = script; R1 = code; R2 = decomp; R3 = vram;
    R4 = count; R5 = 1; R6 = COLOUR_LUT; R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);
}

/* TestEventFlag (event_flags.c) leaves the flag's byte in r0 and its one
 * bit's mask in r1; the flag is set exactly when r0 & r1 is not zero. */
static bool event_flag_set(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return (R0 & R1) != 0;
}

static u32 view_mode(u32 menu) { return LD32(LD32(menu + MENU_VIEW_PTR)); }

/* ── the name rows ────────────────────────────────────────────────── */

/* sub_804760E: a plain entry — its name from the fixed script, then a
 * blank tile in the code column (plain rows show no code letter). */
static void draw_plain_chip_row(u32 slot, u32 chip, u32 menu) {
    u32 row = ROW_BYTES * slot;
    render_tiles(NAME_SCRIPT, chip, DECOMP_NAMES + row, VRAM_NAMES + row, 8);
    render_tiles(LD32(menu + MENU_SYMBOL_SCRIPT), SYM_BLANK, DECOMP_CODE + row, VRAM_CODE + row, 1);
}

/* sub_8047664: a coded chip — name from the banked tables, then the code
 * letter; in the library view a chip neither event flag marks gets the
 * no-code letter instead. */
static void draw_coded_chip_row(u32 slot, u32 chip, u32 code, u32 has_code, u32 menu) {
    u32 row = ROW_BYTES * slot;
    u32 names = LD32(NAME_BANKS + (chip >> 8) * 4);
    render_tiles(names, chip & 0xFF, DECOMP_NAMES + row, VRAM_NAMES + row, 8);

    u32 letter = CODE_NONE;
    if (has_code) {
        letter = code;
        if (view_mode(menu) == VIEW_LIBRARY
            && !event_flag_set(chip + FLAG_CHIP_KNOWN)
            && !event_flag_set(chip + FLAG_CHIP_OWNED))
            letter = CODE_NONE;
    }
    render_tiles(CODE_SCRIPT, letter, DECOMP_CODE + row, VRAM_CODE + row, 1);
}

/* sub_8047708 (kept lifted) draws the secret chip's name row; the
 * original reaches it with the kind-3 path's registers — r3 = the kind
 * byte, r6 = 1 — so they are set here. */
static void draw_secret_chip_row(u32 slot, u32 entry, u32 menu) {
    R0 = slot;
    R1 = LD16(entry + ENTRY_CHIP);
    R2 = LD8(entry + ENTRY_CODE);
    R3 = KIND_SECRET;
    R4 = LD8(entry + ENTRY_PALETTE);
    R5 = menu;
    R6 = 1;
    R7 = entry;
    GUEST_CALL(sub_8047708);
}

/* sub_8047760: a blank row — 16 blank tiles across the name area. */
static void draw_blank_chip_row(u32 slot, u32 menu) {
    u32 row = ROW_BYTES * slot;
    render_tiles(LD32(menu + MENU_SYMBOL_SCRIPT), SYM_BLANK, DECOMP_NAMES + row, VRAM_NAMES + row, 16);
}

/* ── the strip and the dimming ────────────────────────────────────── */

/* sub_8047790: the row's 7-tile strip.  The entry's value (+6) goes to
 * sub_8047CDC in r1 and comes back times 100 (unchanged when the view
 * word is 1); sub_8047CDC preserves r0 and r2-r7.  That result is
 * stored at the toolkit's 200A220 struct (+8) — what the number means
 * is not shown by this code.  The strip itself takes the dim variant in
 * view 1, a blank strip in the library view for chips whose chip+0x2420
 * flag is clear, and the "no code" strip for chips without a code. */
static void draw_chip_row_strip(u32 slot, u32 value, u32 has_code, u32 chip,
                                u32 palette, u32 menu, u32 toolkit) {
    R0 = slot; R1 = value; R2 = has_code; R3 = chip; R4 = palette; R5 = menu;
    R7 = palette << 3;                 /* as the original passes it on */
    GUEST_CALL(sub_8047CDC);
    u32 row = ROW_BYTES * slot;
    ST32(LD32(toolkit + oToolkit_Unk200a220_Ptr) + 8, R1);

    u32 view = view_mode(menu);
    u32 strip;
    if (view == VIEW_LIBRARY && palette != 0 && !event_flag_set(chip + FLAG_CHIP_OWNED)) {
        strip = SYM_BLANK;             /* unregistered: skips the no-code check */
    } else {
        strip = view == VIEW_DIMMED ? STRIP_DIM : STRIP_NORMAL;
        if (has_code == 0)
            strip = SYM_NO_CODE;
    }
    render_tiles(LD32(menu + MENU_SYMBOL_SCRIPT), strip, DECOMP_STRIP + row, VRAM_STRIP + row, 7);
}

/* sub_8047CFA: dim a chip the player has none of.  `secret` rows count
 * through the chip+0x90 collection (sub_803D138), the others through the
 * pack (GetChipCountOfCode); owning one leaves the strip alone.  The
 * dimming repaints byte_202F800's strip tilemap for the row (0x48 bytes:
 * ten u16 at +0 and ten at +0x24): the low 12 bits of each entry stay,
 * 0x3000 is ORed in on even rows and 0x4000 on odd rows, with
 * palette << 13 added on top.  `branch_byte` is only the r3 the
 * original's kind branch leaves on the counter call (the has-code byte,
 * or the kind for secret rows). */
static void dim_unowned_chip_strip(u32 slot, u32 entry, u32 chip, u32 code,
                                   u32 palette, u32 branch_byte, bool secret) {
    /* the counters take the chip and code in r0/r1; r2-r7 ride along as
     * the original leaves them */
    R2 = code; R3 = branch_byte; R4 = palette;
    R5 = palette << 13; R6 = slot; R7 = entry;
    if (secret) {
        R0 = chip + 0x90; R1 = code;
        GUEST_CALL(sub_803D138);
    } else {
        R0 = chip; R1 = code;
        GUEST_CALL(GetChipCountOfCode);
    }
    if (R0 != 0)
        return;                        /* the player owns one: nothing to dim */

    u32 mark = (slot & 1) ? 0x4000 : 0x3000;
    u32 shift = palette << 13;
    u32 strip = A_byte_202F800 + slot * 0x48;
    for (u32 i = 0; i < 10; i++)
        ST16(strip + i * 2, ((LD16(strip + i * 2) & 0xFFF) | mark) + shift);
    for (u32 i = 0; i < 10; i++)
        ST16(strip + 0x24 + i * 2, ((LD16(strip + 0x24 + i * 2) & 0xFFF) | mark) + shift);
}

/* ── the five rows (sub_8047594) ──────────────────────────────────── */

/* Draw entries[first .. first+4] at screen rows 0..4; returns the address
 * one past the last entry, where the original's entry pointer ends. */
static u32 draw_chip_list_rows(u32 first, u32 entries, u32 menu, u32 toolkit) {
    u32 entry = entries + first * ENTRY_SIZE;
    for (u32 slot = 0; slot < 5; slot++, entry += ENTRY_SIZE) {
        switch (LD8(entry + ENTRY_KIND)) {
        case KIND_PLAIN:
            draw_plain_chip_row(slot, LD16(entry + ENTRY_CHIP), menu);
            break;
        case KIND_CODED:
            dim_unowned_chip_strip(slot, entry, LD16(entry + ENTRY_CHIP), LD8(entry + ENTRY_CODE),
                                   LD8(entry + ENTRY_PALETTE), LD8(entry + ENTRY_HAS_CODE), false);
            draw_coded_chip_row(slot, LD16(entry + ENTRY_CHIP), LD8(entry + ENTRY_CODE),
                                LD8(entry + ENTRY_HAS_CODE), menu);
            break;
        case KIND_SECRET:
            dim_unowned_chip_strip(slot, entry, LD16(entry + ENTRY_CHIP), LD8(entry + ENTRY_CODE),
                                   LD8(entry + ENTRY_PALETTE), KIND_SECRET, true);
            draw_secret_chip_row(slot, entry, menu);
            break;
        default:
            draw_blank_chip_row(slot, menu);
            continue;                  /* a blank row draws no strip */
        }
        draw_chip_row_strip(slot, LD16(entry + ENTRY_VALUE), LD8(entry + ENTRY_HAS_CODE),
                            LD16(entry + ENTRY_CHIP), LD8(entry + ENTRY_PALETTE), menu, toolkit);
    }
    return entry;
}

/* ── the entry points the rest of the game calls ──────────────────── */

/* sub_8047594(r0 = first entry of the five to show, r1 = the entry array,
 * r5 = the screen state, r10 = the toolkit): draw five chip rows. */
Next sub_8047594(void) {
    const u32 menu = R5, toolkit = R10, saved_r6 = R6;

    R7 = draw_chip_list_rows(R0, R1, menu, toolkit);

    /* as the original leaves them: r5 and r6 popped back, the row counter
     * run to 5, the entry pointer one past the last row; r2 and r4 keep
     * whatever the final render left in them */
    R0 = 5;
    R5 = menu;
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* sub_804760E(r0 = row, r1 = chip, r5 = screen state): a plain row. */
Next sub_804760E(void) {
    const u32 slot = R0, menu = R5, entry = R7;
    draw_plain_chip_row(slot, R1, menu);
    R0 = slot; R5 = menu; R7 = entry;  /* the original's pops; r6 keeps the render's leftover */
    RETURN_TO_CALLER();
}

/* sub_8047664(r0 = row, r1 = chip, r2 = code letter, r3 = has code,
 * r5 = screen state): a coded chip row. */
Next sub_8047664(void) {
    const u32 slot = R0, menu = R5, entry = R7;
    draw_coded_chip_row(slot, R1, R2, R3, menu);
    R0 = slot; R5 = menu; R7 = entry;  /* the original's pops; r6 keeps the render's leftover */
    RETURN_TO_CALLER();
}

/* sub_8047760(r0 = row, r5 = screen state): a blank row. */
Next sub_8047760(void) {
    const u32 slot = R0, menu = R5, entry = R7;
    draw_blank_chip_row(slot, menu);
    R0 = slot; R5 = menu; R7 = entry;  /* the original's pops; r2/r4 keep the render's leftovers */
    RETURN_TO_CALLER();
}

/* sub_8047790(r0 = row, r1 = the entry's value, r2 = has code,
 * r3 = chip, r4 = palette, r5 = screen state, r10 = toolkit): the row's
 * strip. */
Next sub_8047790(void) {
    const u32 slot = R0, menu = R5, entry = R7;
    draw_chip_row_strip(slot, R1, R2, R3, R4, menu, R10);
    R0 = slot; R5 = menu; R7 = entry;  /* the original's pops; r2/r4 keep the render's leftovers */
    RETURN_TO_CALLER();
}

/* sub_8047CFA(r0 = row, r1 = chip, r2 = code, r4 = palette, r6 = count
 * the secret collection): dim an unowned chip's strip; r3 rides along. */
Next sub_8047CFA(void) {
    /* the original pushes r0-r7 at entry and pops them at exit: every one
     * of them leaves as it came, however the dimming went */
    const u32 saved_r0 = R0, saved_r1 = R1, saved_r2 = R2, saved_r3 = R3,
              saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    dim_unowned_chip_strip(R0, R7, R1, R2, R4, R3, R6 != 0);
    R0 = saved_r0; R1 = saved_r1; R2 = saved_r2; R3 = saved_r3;
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    RETURN_TO_CALLER();
}
