/*
 * asm03_2.c: readable functions of bn6f asm03_2.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── renderTextGfx_8045F8C ── */
/* renderTextGfx_8045F8C(r0 = text graphics base, r1 = entry index):
 *
 * Hands a text's graphics data to the glyph renderer sub_3006B94. The
 * graphics base is first stashed in the toolkit's text-renderer state
 * (offset 0x40), then the callee is invoked with r0 = graphics +
 * offsetTable[index], where the leading table of u16 offsets picks which
 * chunk of the graphics to draw, and r1 = r9 as its second argument.
 *
 * The original saves r8/r9 around the call with push/pop pairs (Thumb
 * cannot push high registers directly) and, as a side effect of popping
 * into r1 both times, ends with r1 = the saved r8 value. */
Next renderTextGfx_8045F8C(void) {
    u32 gfx = R0;

    /* Publish the graphics base for the text renderer state. */
    u32 textState = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(textState + 0x40, gfx);

    /* gfx starts with a u16 offset table; entry r1 gives the offset (from
       gfx itself) of the glyph data to render. */
    R0 = gfx + LD16(gfx + (R1 << 1));

    u32 savedR8 = R8;
    u32 savedR9 = R9;
    R1 = R9;                    /* sub_3006B94 takes r9 as its second arg */
    GUEST_CALL(sub_3006B94);
    R1 = savedR8;               /* pop {r1}; mov r8, r1 leaves r1 = old r8 */
    R8 = savedR8;
    R9 = savedR9;

    RETURN_TO_CALLER();
}

/* ── sub_8046154 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; } call_FillByEightWords_result;
/* register form: generated wrapper: FillByEightWords has no typed C interface yet */
static inline call_FillByEightWords_result call_FillByEightWords(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(FillByEightWords);
    return (call_FillByEightWords_result){ R1 };
}

typedef struct { u32 r2; } call_ZeroFillByEightWords_result;
/* register form: generated wrapper: ZeroFillByEightWords has no typed C interface yet */
static inline call_ZeroFillByEightWords_result call_ZeroFillByEightWords(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ZeroFillByEightWords);
    return (call_ZeroFillByEightWords_result){ R2 };
}

typedef struct { u32 r0, r2, r3, r4, r5, r6, r8, r9, r10, r12; } call_sub_80461D0_result;
/* register form: generated wrapper: sub_80461D0 has no typed C interface yet */
static inline call_sub_80461D0_result call_sub_80461D0(u32 r1) {
    R1 = r1;
    GUEST_CALL(sub_80461D0);
    return (call_sub_80461D0_result){ R0, R2, R3, R4, R5, R6, R8, R9, R10, R12 };
}

typedef struct { u32 r0, r1, r2, r3, r8, r9, r12; bool z, n, c, v; } call_sub_8046290_result;
/* register form: generated wrapper: sub_8046290 has no typed C interface yet */
static inline call_sub_8046290_result call_sub_8046290(u32 r0) {
    R0 = r0;
    GUEST_CALL(sub_8046290);
    return (call_sub_8046290_result){ R0, R1, R2, R3, R8, R9, R12, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Animation opcode stream parser.
 * Initializes a Toolkit animation-state block, fills/zeroes scratch buffers,
 * then decodes a byte-coded opcode stream via a jump table or helper calls
 * until a handler clears the continue flag. A finalizer processes the result.
 */

/* register form: jump-table handlers and sub_80461D0 read/write R4-R7 through the
 * register file; there is no typed interface that carries these per-call. */
static void parse_stream(u32 stream_ptr, u32 toolkit, u32 src, u32 saved_r3,
                         u32 entry_r4, u32 entry_r5, u32 entry_r6, u32 entry_r7) {
    /* Fetch the Toolkit's animation-state structure. */
    u32 state = LD32(toolkit + oToolkit_Unk200a220_Ptr);

    /* Slot index = stream_id << 3, stored as a byte. */
    u32 slot = entry_r4 << 3;

    /* Initialize the state block for this animation packet. */
    ST8(state + 0x00, slot);
    ST8(state + 0x01, 0);
    ST8(state + 0x02, entry_r5);
    ST32(state + 0x18, entry_r6);
    ST32(state + 0x1C, 0);
    ST32(state + 0x20, src);
    ST32(state + 0x24, entry_r7);
    ST32(state + 0x30, 0);
    ST32(state + 0x34, 0);
    ST32(state + 0x3C, 0);

    /* Fill scratch: (slot * (bank<<4)) 8-word blocks from the block table. */
    u32 bank16 = entry_r5 << 4;
    u32 fill_count = slot * bank16;
    u32 fill_block = LD32(state + 0x38);
    call_FillByEightWords(src, fill_count, fill_block);
    call_ZeroFillByEightWords(entry_r7, R1);

    /* Opcode decode loop.  R3 is the continue flag; handlers modify R0-R7. */
    u32 r0 = stream_ptr;
    u32 r3 = 1;
    u32 r4 = fill_count;
    u32 r5 = bank16;
    u32 r6 = stream_ptr;
    u32 r7 = entry_r7;

    while (r3 != 0) {
        u32 op = LD8(r0);
        if ((s32)op >= 0xE5) {
            /* Long opcode (>= 0xE5): dispatch through jump table at off_80460F4. */
            u32 idx = (op - 0xE5) << 2;
            R7 = A_off_80460F4;
            u32 handler = LD32(R7 + idx);
            R0 = r0; R1 = op; R3 = r3; R4 = r4; R5 = r5; R6 = r6;
            GUEST_CALL_ADDR(handler);
            r0 = R0; r3 = R3; r4 = R4; r5 = R5; r6 = R6; r7 = R7;
        } else if (op == 0xE4) {
            /* Two-byte opcode: argument = next byte + 0xE4. */
            u32 arg = LD8(r0 + 1) + 0xE4;
            R0 = r0; R3 = r3; R4 = r4; R5 = r5; R6 = r6; R7 = r7;
            call_sub_80461D0(arg);
            r0 = R0 + 2; r3 = R3; r4 = R4; r5 = R5; r6 = R6;
        } else {
            /* Single-byte opcode (< 0xE4): argument is the opcode itself. */
            R0 = r0; R3 = r3; R4 = r4; R5 = r5; R6 = r6; R7 = r7;
            call_sub_80461D0(op);
            r0 = R0 + 1; r3 = R3; r4 = R4; r5 = R5; r6 = R6;
        }
    }

    /* Finalizer: processes the saved third argument (original's pop {r0}; bl sub_8046290). */
    call_sub_8046290(saved_r3);

    /* Restore callee-saved registers (original's pop {r4-r7}). */
    R4 = entry_r4; R5 = entry_r5; R6 = entry_r6; R7 = entry_r7;
}

/* sub_8046154(r0 = stream ptr, r2 = src data, r3 = finalizer arg,
 *             r4 = stream ID, r5 = bank, r6 = base ptr, r7 = dest buf, r10 = toolkit) */
Next sub_8046154(void) {
    parse_stream(R0, R10, R2, R3, R4, R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── sub_804634C ── */
Next sub_804634C(void) {
    /* Tail-call: do the work in sub_8046356, then clear r3 and return.
     * sub_8046356 preserves nothing we care about here, so just invoke it
     * directly and let its return path bring us back to the caller with
     * r3 already zeroed. */
    GUEST_CALL(sub_8046356);
    R3 = 0;
    RETURN_TO_CALLER();
}

/* ── sub_8046356 ── */
/* sub_8046356 - advance the animated bitmap reveal whose state lives at
 * byte_200A220.
 *
 * State block layout (offsets from the base A_byte_200A220):
 *   +0x00 byte   row stride in bytes
 *   +0x01 byte   step counter, bumped once per call
 *   +0x1C dword  packed bit position: stride * 112 * counter
 *   +0x20 dword  base address of the bitmap row words
 *   +0x38 dword  pixel mask for the column being drawn
 *
 * The packed position splits into a word index (>> 5, scaled to bytes by
 * << 2) and a bit offset within the word (& 0x1F).  The mask with its low
 * `bit` bits cut off is OR-ed into 12 row words spaced `stride` bytes
 * apart, starting at the indexed word.  Exactly as in the original, the
 * stores are skipped entirely when the bit offset is zero (TST r6,r6 /
 * BLE in the loop).  Afterwards the counter is bumped and the packed
 * position is rewritten as stride * 14 * 8 * counter.
 *
 * The original pushes the caller's r0 and pops it just before
 * `add r0, #1`, so r0 comes back as caller_r0 + 1; C/V are left by that
 * ADD and N/Z are then forced clear by the final `mov r3, #1`. */

struct sub_8046356_out {
    u32 r0, r1, r2, r3, r4, r6, r7;
};

static struct sub_8046356_out sub_8046356_step(u32 caller_r0)
{
    u32 state = A_byte_200A220;

    u32 packed  = LD32(state + (A_dword_200A23C - 0x200A220));
    u32 rows    = LD32(state + (A_dword_200A240 - 0x200A220));
    u32 mask    = LD32(state + (A_dword_200A258 - 0x200A220));
    u32 stride  = LD8(state);

    u32 bit     = packed & 0x1F;
    u32 row     = rows + ((packed >> 5) << 2);
    u32 pattern = (mask >> bit) << bit;   /* mask, low `bit` bits cleared */

    for (s32 i = 0; i < 12; i++) {
        u32 word = LD32(row) | pattern;
        if (bit != 0)
            ST32(row, word);              /* original skips stores at bit == 0 */
        row += stride;
    }

    u32 counter = LD8(state + (A_byte_200A221 - 0x200A220)) + 1;
    ST8(state + (A_byte_200A221 - 0x200A220), counter);

    u32 new_packed = ((stride * 14) << 3) * counter;
    ST32(state + (A_dword_200A23C - 0x200A220), new_packed);

    struct sub_8046356_out o;
    o.r0 = caller_r0 + 1;      /* popped caller r0, then add r0, #1 */
    o.r1 = new_packed;
    o.r2 = state;
    o.r3 = 1;
    o.r4 = counter;
    o.r6 = bit;
    o.r7 = 14;                 /* mov r7, #0xe is the last thing in r7 */
    return o;
}

Next sub_8046356(void) {
    struct sub_8046356_out o = sub_8046356_step(R0);
    R0 = o.r0, R1 = o.r1, R2 = o.r2, R3 = o.r3,
    R4 = o.r4, R6 = o.r6, R7 = o.r7;
    SET_FLAGS_ADD(o.r0 - 1, 1);   /* C/V from the final add r0, #1 */
    SET_FLAGS_NZ(o.r3);           /* N/Z from the final mov r3, #1 */
    RETURN_TO_CALLER();
}

/* ── sub_8046450 ── */
/* sub_8046450: run an entity's per-state handler.
 *
 * r0 points to the entity; byte 1 of the entity holds the state index. That
 * index selects one entry of the handler table at dword_8046464, and the
 * handler is entered with
 *   r0 = the entity pointer,
 *   r3 = index * 4 (the table offset the "lsl r3, r3, #2" left in r3),
 *   N/Z = flags of index * 4, C = clear (a byte shifted left by two never
 *         shifts a one out of bit 30),
 * via "mov lr, pc; bx r1", so it returns straight through the pop back to
 * sub_8046450's caller. The caller's r3/r7 are pushed around the dispatch and
 * popped before the return, so both end unchanged; r0 and whatever the handler
 * leaves in r8/r10/r11/r12 are the results. */

/* register form: the handler address is read from the dispatch table at run
 * time, so the call must go through the code address itself (mov lr, pc; bx r1),
 * and the caller's r3/r7 must survive the dispatched call the way the
 * original's push {r3,r7,lr} / pop {r3,r7,pc} does. */
static u32 run_entity_state_handler(u32 entity) {
    u32 state = LD8(entity + 1);
    u32 slot = state << 2;
    u32 handler = LD32(A_dword_8046464 + slot);
    u32 saved_r3 = R3;
    u32 saved_r7 = R7;

    R0 = entity;                /* handler receives the entity pointer ... */
    R3 = slot;                  /* ...and the shifted state index in r3 */
    SET_FLAGS_ADD(slot, 0);     /* N/Z of the shifted index, C clear: the lsl's flags */

    GUEST_CALL_ADDR(handler);   /* table entries are stored Thumb+1, as bx expects */

    R3 = saved_r3;              /* pop {r3,r7}: both preserved for the caller */
    R7 = saved_r7;
    return R0;
}

Next sub_8046450(void) {
    R0 = run_entity_state_handler(R0);
    RETURN_TO_CALLER();
}

/* ── battle_initEStructArr_804657C ── */
/* battle_initEStructArr_804657C: initialise eStructArr2008450.
 * Zero-fills the whole 0x528-byte array (15 entries of 0x58 bytes each),
 * then sets every entry's byte at +2 to 0x20 and its byte at +3 to the
 * 0-based entry index.
 * Preserves r2-r7; leaves r1 as ZeroFillByWord left it. */
Next battle_initEStructArr_804657C(void) {
    R0 = A_eStructArr2008450;
    R1 = 0x528;
    GUEST_CALL(ZeroFillByWord);

    u32 entry = A_eStructArr2008450;
    for (u32 i = 0; i < 15; i++) {
        ST8(entry + 2, 0x20);   /* element type */
        ST8(entry + 3, i);      /* entry index */
        entry += 0x58;
    }

    /* Flags come from the init loop's final `cmp r7, #0xF` (r7 == 0xF here). */
    SET_FLAGS_CMP(0xF, 0xF);
    RETURN_TO_CALLER();
}

/* ── entity_runAllUpdateHandlers_80465BC ── */
/* entity_runAllUpdateHandlers_80465BC: run the update handler of every active record in the
 * 15-entry array at eStructArr2008450 (0x58 bytes per record).
 * A record is active when bit 0 of its flag byte (+0) is set; the byte at
 * +1 selects the handler from the pointer table off_804646C (byte * 4).
 * Each handler is entered with:
 *   r0 = its own address (the table entry), r1 = selector byte * 4,
 *   r4 = incoming r8, r5 = pointer to the record, r6 = incoming r12,
 *   r7 = record index; r2/r3 and r8-r12 pass through unchanged.
 * Exits with c/v from the loop's final "cmp r7, #0xF" (r7 == 15, so carry
 * set, overflow clear) and r1 = 1 if the last record was inactive, else
 * whatever the last handler left in r1. */
Next entity_runAllUpdateHandlers_80465BC(void) {
    u32 inR4 = R4, inR5 = R5, inR6 = R6, inR7 = R7;
    u32 inR8 = R8, inR9 = R9, inR12 = R12;

    u32 record = A_eStructArr2008450;
    for (u32 i = 0; i < 15; i++, record += 0x58) {
        R1 = 1;                              /* mov r1, #1 (kept if inactive) */
        if (LD8(record) & 1) {
            u32 sel = (u32)LD8(record + 1) << 2;
            R1 = sel;
            u32 handler_addr = LD32(A_off_804646C + sel);
            R0 = handler_addr;               /* handler sees its own address */
            R4 = inR8;                       /* copies of r8/r12 kept live in */
            R5 = record;                     /* r4/r6 across the whole loop   */
            R6 = inR12;
            R7 = i;                          /* push {r7} before the call...  */
            GUEST_CALL(dispatch_fast(handler_addr));
            R7 = i;                          /* ...pop {r7} after it          */
        }
    }

    /* Epilogue: r4-r7 and the saved r8/r9/r12 all end up as they came in. */
    R4 = inR4;
    R5 = inR5;
    R6 = inR6;
    R7 = inR7;
    R8 = inR8;
    R9 = inR9;
    R12 = inR12;

    /* Loop exit: the last iteration's cmp r7, #0xF with r7 == 15. */
    SET_FLAGS_CMP(15, 15);

    RETURN_TO_CALLER();
}

/* ── battle_resetEStructRecords_8046664 ── */
/* battle_resetEStructRecords_8046664: run both halves of the (unspecified) update routine:
 * first sub_80466AA, then battle_initEStructArr_804657C. All registers other than the
 * flags are preserved; r4-r7 are untouched here and by both callees. */
Next battle_resetEStructRecords_8046664(void) {
    GUEST_CALL(sub_80466AA);
    GUEST_CALL(battle_initEStructArr_804657C);
    RETURN_TO_CALLER();
}

/* ── sub_8046670 ── */
/* sub_8046670(r0 = id byte, r1 = slot index, r2 = param word):
 * initialize one 0x58-byte entry of eStructArr2008450 for r1*0x58:
 * set the entry's id byte, type 9 marker, param word, and clear its counter.
 * Returns r0 = pointer to the entry, r1 = r1*0x58, flags from ADDS r0, entry, #0. */
Next sub_8046670(void) {
    u32 entry = A_eStructArr2008450 + R1 * 0x58;

    ST8(entry + 1, R0);   /* id byte            */
    ST8(entry,      9);   /* entry type = 9     */
    ST32(entry + 4, R2);  /* parameter          */
    ST32(entry + 8, 0);   /* counter/state = 0  */

    R1 = R1 * 0x58;       /* original leaves the scaled index in r1 */
    R0 = entry;
    SET_FLAGS_ADD(entry, 0);  /* adds r0, entry, #0: N/Z from entry, C set, V clear */
    RETURN_TO_CALLER();
}

/* ── sprite_clearScaleCounterAndMakeUnscalable_804668A ── */
/* sprite_clearScaleCounterAndMakeUnscalable_804668A(r5 = pointer to the sprite's scale/cycle counter):
 * Clears the byte at *r5 to 0, then rebuilds the sprite with
 * sprite_makeUnscalable (called with r0 = 0). r4-r7 are preserved. */
Next sprite_clearScaleCounterAndMakeUnscalable_804668A(void) {
    R0 = 0;             /* mov r0, #0 */
    ST8(R5, R0);        /* strb r0, [r5] */

    GUEST_CALL(sprite_makeUnscalable); /* bl sprite_makeUnscalable */

    RETURN_TO_CALLER();
}

/* ── sub_8046696 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3; bool c, v; } call_sprite_makeUnscalable_result;
/* register form: generated wrapper: sprite_makeUnscalable has no typed C interface yet */
static inline call_sprite_makeUnscalable_result call_sprite_makeUnscalable(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_makeUnscalable);
    return (call_sprite_makeUnscalable_result){ R1, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_8046696(r0 = index into the struct array at eStructArr2008450):
 * Clears the first byte of entry #index (its "scalable"/scale state byte) and
 * marks that entry's sprite as unscalable via sprite_makeUnscalable, which
 * takes the entry address in r5. r4-r7 are pushed/popped around the call, so
 * they (including the entry address in r5) end unchanged; r2/r3 are whatever
 * sprite_makeUnscalable leaves behind. */

typedef struct {
    u32 r2, r3, r4, r5, r6, r7;
} sprite_scale_clear_result;

static sprite_scale_clear_result clear_sprite_scale(u32 index, u32 r4, u32 r5, u32 r6, u32 r7) {
    sprite_scale_clear_result out;

    /* Address of this entry's struct: base + 0x58 * index */
    u32 entry = A_eStructArr2008450 + 0x58 * index;

    ST8(entry, 0);

    call_sprite_makeUnscalable_result call = call_sprite_makeUnscalable(entry);

    out.r2 = call.r2;
    out.r3 = call.r3;
    /* The original's pop {r4-r7} restores the entry values. */
    out.r4 = r4;
    out.r5 = r5;
    out.r6 = r6;
    out.r7 = r7;
    return out;
}

Next sub_8046696(void) {
    sprite_scale_clear_result out = clear_sprite_scale(R0, R4, R5, R6, R7);
    R2 = out.r2;
    R3 = out.r3;
    R4 = out.r4;
    R5 = out.r5;
    R6 = out.r6;
    R7 = out.r7;
    RETURN_TO_CALLER();
}

/* ── getStructFrom2008450 ── */
/* getStructFrom2008450(r0 = struct index): address one of the fixed 0x58-byte
 * entries of eStructArr2008450 and load the byte at its start (its "in use"
 * flag). Returns that byte in r0 with NZ flags set from it; r1 ends holding
 * the entry's address and r2 the byte offset 0x58*index, as callers expect.
 *
 * Note: the original's ADDS (base + 0x58*index) is the last instruction that
 * sets C and V — the trailing TST only updates NZ — so the add's carry and
 * overflow must be reproduced for callers that branch on them. */
Next getStructFrom2008450(void) {
    u32 entryOffset = 0x58 * R0;
    u32 entryAddr = A_eStructArr2008450 + entryOffset;

    SET_FLAGS_ADD(A_eStructArr2008450, entryOffset); /* C/V from the ADDS */

    R1 = entryAddr;
    R2 = entryOffset;
    R0 = LD8(entryAddr);

    SET_FLAGS_NZ(R0); /* TST r0, r0: NZ from the loaded byte */
    RETURN_TO_CALLER();
}

/* ── battleObject_scanHitboxesAndSetFlags_80466D8 ── */
/* battleObject_scanHitboxesAndSetFlags_80466D8: walks a battle-object's hitbox list and marks which entries
 * fall inside the screen bounds (with mirror adjustments), then updates
 * the object's on-screen / collision flags byte and bitmask. */

typedef struct {
    u32 flags;     /* r3: updated flags byte (written back to [obj+3]) */
    u32 r6;        /* r6: on-screen-subset mask, or original Y */
    u32 mirrorX;   /* r8: X-mirror active (0 or 1) */
    u32 mirrorY;   /* r9: Y-mirror active (0 or 1) */
    u32 r12;       /* r12: original Y coordinate */
} HitboxScanResult;

static HitboxScanResult scan_hitboxes(u32 obj) {
    s32 x = LDS16(obj + 0xC);
    s32 y = LDS16(obj + 0xE);

    u32 mirrorX = (LD8(obj + 0x13) & 0x10) ? 1 : 0;
    u32 mirrorY = (LD8(obj + 0x13) & 0x20) ? 1 : 0;

    /* Resolve the hitbox entry pointer for this object's type */
    u32 typeData  = LD32(obj + 0x20);
    u32 typeOff   = (u32)LD8(typeData) << 2;
    u32 tableBase = LD32(LD32(obj + 0x1C) + 0xC);
    u32 entryBase = LD32(obj + 0x18) + tableBase;
    u32 cur       = LD32(entryBase + typeOff) + entryBase;

    /* Walk the 5-byte-per-entry hitbox list; build a 32-bit mask of entries
     * whose offset lands inside the visible screen area. */
    u32 activeMask = 0;
    u32 bit        = 0x80000000;

    while (LD8(cur) != 0xFF) {
        s32 dx = (s8)LD8(cur + 1);
        if (mirrorX) dx = -dx;
        dx += x;

        s32 dy = (s8)LD8(cur + 2);
        if (mirrorY) dy = -dy;
        dy += y;

        /* Screen bounds: x in [-64, 305), y in [-32, 193) */
        if (dx >= -64 && dx < 305 && dy >= -32 && dy < 193) {
            activeMask |= bit;
        }

        bit >>= 1;
        cur  += 5;
    }

    /* Update the object's flags byte and produce the r6 output */
    u32 flags = LD8(obj + 3);
    u32 outR6;

    if (flags & 4) {
        /* Alternate mode: bits 1 and 3 together reflect the full mask */
        outR6 = (u32)y;
        flags &= ~0xA;
        if (activeMask) flags |= 0xA;
    } else {
        /* Normal mode: bit 3 tracks mask minus the MSB entry,
         * bit 1 tracks the full mask */
        outR6 = activeMask & ~0x80000000;
        flags &= ~8;
        if (outR6) flags |= 8;
        flags &= ~2;
        if (activeMask) flags |= 2;
    }

    ST8(obj + 3, flags);
    ST32(obj + 0x28, activeMask);

    HitboxScanResult result = { flags, outR6, mirrorX, mirrorY, (u32)y };
    return result;
}

/* battleObject_scanHitboxesAndSetFlags_80466D8(r5 = battle object pointer) -> r3, r6, r8, r9, r12 */
Next battleObject_scanHitboxesAndSetFlags_80466D8(void) {
    HitboxScanResult res = scan_hitboxes(R5);
    R3  = res.flags;
    R6  = res.r6;
    R8  = res.mirrorX;
    R9  = res.mirrorY;
    R12 = res.r12;
    RETURN_TO_CALLER();
}

/* ── initWorkArea5Entries_80467D8 ── */
/* initWorkArea5Entries_80467D8: clear the 0x1B8-byte work block at byte_2011860, then set up
 * its five 0x58-byte entries: byte 2 = 0x20 (entry "alive"/default value),
 * byte 3 = entry index. r0 = 0x20, r1 = 0x1B8 afterwards; r2-r7 preserved. */
Next initWorkArea5Entries_80467D8(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R0 = A_byte_2011860;
    R1 = 0x1B8; /* 5 entries * 0x58 bytes */
    GUEST_CALL(ZeroFillByWord);

    u32 entry = A_byte_2011860;
    for (u32 i = 0; i < 5; i++) {
        ST8(entry + 2, 0x20); /* entry state */
        ST8(entry + 3, i);    /* entry index */
        entry += 0x58;
    }

    /* Exit state of the original's last CMP r7, #5 (counter ended at 5). */
    SET_FLAGS_CMP(5, 5);
    R0 = 0x20;
    R1 = 0x1B8;
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── reqBBS_initRequestWorkArea_8046CC8 ── */
/* reqBBS_initRequestWorkArea_8046CC8(r0 = BBS request slot index, r10 = toolkit):
 * Prepares the BBS request work area for the selected request:
 * clears the 0x34-byte buffer, stores the slot index and the per-request
 * data pointer/text-offset table, and records the current map group as
 * the group to return to. Leaves r2 = request data table base. */
Next reqBBS_initRequestWorkArea_8046CC8(void) {
    u32 slot = R0;
    u32 toolkit = R10;

    /* Zero-fill the whole request work area (0x34 bytes). */
    R0 = A_byte_2011C10;
    R1 = 0x34;
    GUEST_CALL(ZeroFillByWord);

    ST8(A_byte_2011C10 + 4, slot);

    R2 = A_dword_8046B98;
    ST32(A_byte_2011C10 + 0x1C, R2 + slot * 0x10);
    ST32(A_byte_2011C10 + 0x24, A_reqBBS_requestInfo_textOffsets);

    /* Save the current map group as the "last visited" group.
     * (The map group lives at offset 4 of the game state struct.) */
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    ST16(gameState + oGameState_LastMapGroup, LD16(gameState + 4));

    RETURN_TO_CALLER();
}

/* ── ShopControl ── */
/* ShopControl: selects a shop-phase handler via byte_2011C10 (byte offset into
 * the jump table at off_8046D1C) and dispatches to it, preserving r4-r7, r8-r9,
 * r12 and lr across the call. The handler receives:
 *   r0 = its own function pointer, r1 = state offset, r2 = caller r9,
 *   r3 = caller r12, r5 = &byte_2011C10.
 * After the handler returns, r0 and flags pass through; r1/r2/r3 are set to the
 * caller's saved r8/r9/r12 (matching the original's push {r1-r3} / pop {r1-r3}). */

/* register form: dispatches to the selected shop handler, saving and restoring
 * r4-r7, r8-r9, r12, lr exactly as the original's push/pop pairs do */
/* The handlers' r0 is handed back to ShopControl's r0 unchanged, which no caller of ShopControl reads: */
/* leftover unused: shop_initScreen_8046D4C's r0, battle_objectAdvanceCounterToState8_8046DDC's r0, sub_8046DF8's r0 */
/* leftover unused: scramblePanel_tick_8046E06's r0, dialogueEnd_unlockPlayer_8046E48's r0, sub_8046E84's r0 */
/* leftover unused: chatbox_confirmShopPurchase_8046ED2's r0, chatbox_stepContextScript_8047028's r0 */
static u32 call_shop_handler(u32 handler_addr, u32 state_offset) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    u32 s8 = R8, s9 = R9, s12 = R12, slr = LR;

    R0 = handler_addr;
    R1 = state_offset;
    R2 = s9;
    R3 = s12;
    R5 = A_byte_2011C10;

    switch (state_offset) {
    case 0:  GUEST_CALL(shop_initScreen_8046D4C); break;
    case 4:  GUEST_CALL(battle_objectAdvanceCounterToState8_8046DDC); break;
    case 8:  GUEST_CALL(sub_8046DF8); break;
    case 12: GUEST_CALL(scramblePanel_tick_8046E06); break;
    case 16: GUEST_CALL(dialogueEnd_unlockPlayer_8046E48); break;
    case 20: GUEST_CALL(sub_8046E84); break;
    case 24: GUEST_CALL(chatbox_confirmShopPurchase_8046ED2); break;
    case 28: GUEST_CALL(chatbox_stepContextScript_8047028); break;
    case 32: GUEST_CALL(sub_8047050); break;
    case 36: GUEST_CALL(sub_8047066); break;
    case 40: GUEST_CALL(sub_804709C); break;
    default: break;
    }

    /* Restore as original's pop {r1-r3}; mov r8/r9/r12; pop {r4-r7,pc}:
     * r1 <- saved r8, r2 <- saved r9, r3 <- saved r12 */
    R1 = s8;
    R2 = s9;
    R3 = s12;
    R4 = s4;
    R5 = s5;
    R6 = s6;
    R7 = s7;
    R8 = s8;
    R9 = s9;
    R12 = s12;
    LR = slr;

    return R0;
}

/* Read the state byte, look up the handler address, and dispatch. */
static u32 shop_execute(void) {
    u32 offset = LD8(A_byte_2011C10);
    u32 handler_addr = LD32(A_off_8046D1C + offset);
    return call_shop_handler(handler_addr, offset);
}

/* ShopControl() -> r0 = handler result; r1/r2/r3 = caller's saved r8/r9/r12;
 * r4-r7, r8-r9, r12, lr preserved; flags from handler. */
Next ShopControl(void) {
    R0 = shop_execute();
    RETURN_TO_CALLER();
}

/* ── battle_objectAdvanceCounterToState8_8046DDC ── */
/* battle_objectAdvanceCounterToState8_8046DDC: advance the counter at (R5+8) by 2. Once it reaches 0x15,
 * clamp it there and move the object at R5 into state 8 (byte at R5+0).
 * Then runs shopBuyMenu_updateFrame_8047134 (which reads R5) and returns 0. */
Next battle_objectAdvanceCounterToState8_8046DDC(void) {
    u32 counter = (u32)LD8(R5 + 8) + 2;

    ST8(R5 + 8, counter);

    if ((s32)counter >= 0x15) {
        ST8(R5 + 8, 0x15);
        ST8(R5, 8);
    }

    GUEST_CALL(shopBuyMenu_updateFrame_8047134);

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8046DF8 ── */
/* sub_8046DF8: run two update passes (battleMenu_navigate_8047950 then shopBuyMenu_updateFrame_8047134) and
 * return 0. Always yields r0 = 0 with N/Z set accordingly. */
Next sub_8046DF8(void) {
    GUEST_CALL(battleMenu_navigate_8047950);
    GUEST_CALL(shopBuyMenu_updateFrame_8047134);

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── scramblePanel_tick_8046E06 ── */
/* register form: calls chatbox_mask_eFlags2009F38 outside this file, which has no typed C interface yet */
static bool chatbox_should_skip(u32 flag) {
    R0 = flag;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return ZERO_FLAG();            /* original: beq loc_8046E40 — skip when return == 0 */
}

/* register form: calls checkScrambledCounter_8047366 outside this file, which has no typed C interface yet */
static bool scrambled_counter_active(u32 counter) {
    R0 = counter;
    GUEST_CALL(checkScrambledCounter_8047366);
    return !ZERO_FLAG();           /* original: bne loc_8046E40 — skip when return != 0 */
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void set_screen_fade(u32 a, u32 b) {
    R0 = a;
    R1 = b;
    GUEST_CALL(SetScreenFade);
}

/* register form: calls shopBuyMenu_updateFrame_8047134 outside this file, which has no typed C interface yet */
static void call_sub_8047134(void) {
    GUEST_CALL(shopBuyMenu_updateFrame_8047134);
}

/* Drives the scramble-counter UI panel stored at `state_ptr`.
 *   ticker (byte at +8):   subtract 2 each call; once it reaches 0,
 *                          reset it and bump the frame counter.
 *   frames (byte at +0xA): increment; once it hits 0x10, reset both,
 *                          store 0x10 at +0 and trigger the screen fade.
 * Suppressed while the chatbox flag 8 is clear, or a scramble counter
 * is currently being resolved. */
static void animate_scramble_panel(u32 state_ptr) {
    if (chatbox_should_skip(8))
        goto end;
    if (scrambled_counter_active(LD32(state_ptr + 0x28)))
        goto end;

    {
        u32 ticker = LD8(state_ptr + 8) - 2;      /* ldrb / sub r0,#2 — 32-bit arithmetic */
        ST8(state_ptr + 8, ticker);               /* strb stores low byte */
        if ((s32)ticker > 0)                      /* original: cmp r0,#0 ; bgt (signed) */
            goto end;
        ST8(state_ptr + 8, 0);

        {
            u32 frames = LD8(state_ptr + 0xA) + 1;
            ST8(state_ptr + 0xA, frames);
            if ((s32)frames < 0x10)               /* original: cmp r0,#0x10 ; blt (signed) */
                goto end;

            ST8(state_ptr + 0xA, 0);
            ST8(state_ptr, 0x10);
            set_screen_fade(0xC, 0xC);
        }
    }

end:
    call_sub_8047134();
}

Next scramblePanel_tick_8046E06(void) {
    animate_scramble_panel(R5);
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── dialogueEnd_unlockPlayer_8046E48 ── */
/* register form: calls shopBuyMenu_updateFrame_8047134 outside this file, which has no typed C interface yet */
static void sub_8047134_call(void) {
    GUEST_CALL(shopBuyMenu_updateFrame_8047134);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool is_screen_fade_active_sub_8046E48(void) {
    GUEST_CALL(IsScreenFadeActive);
    return ZERO_FLAG();  /* beq taken (Z=1) when a fade transition is in progress */
}

/* register form: calls checkScrambledCounter_8047366 outside this file, which has no typed C interface yet */
static bool scrambled_counter_valid(u32 counter) {
    R0 = counter;
    R1 = counter;
    GUEST_CALL(checkScrambledCounter_8047366);
    return ZERO_FLAG();  /* bne taken (Z=0) when the scramble check fails */
}

/* register form: calls sub_803D0B4 outside this file, which has no typed C interface yet */
static void apply_counter_normal(u32 counter) {
    R0 = counter;
    GUEST_CALL(sub_803D0B4);
}

/* register form: calls protectZenny_803D02C outside this file, which has no typed C interface yet */
static void apply_counter_default(u32 counter) {
    R0 = counter;
    GUEST_CALL(protectZenny_803D02C);
}

/* register form: calls owPlayer_unlockPlayerAfterNonNPCDialogue_809E122 outside this file, which has no typed C interface yet */
static void unlock_player_after_dialogue(void) {
    GUEST_CALL(owPlayer_unlockPlayerAfterNonNPCDialogue_809E122);
}

/* register form: calls resetGameStateToOpening_8005F84 outside this file, which has no typed C interface yet */
static void sub_8005F84_call(void) {
    GUEST_CALL(resetGameStateToOpening_8005F84);
}

/* Attempt to unlock the player after a non-NPC overworld dialogue ends.
 * Returns 1 on success, 0 if skipped (fade active or scramble check failed). */
static u32 handle_dialogue_unlock(u32 dialogue_ptr) {
    sub_8047134_call();

    if (is_screen_fade_active_sub_8046E48())
        return 0;

    u32 counter = LD32(dialogue_ptr + 0x28);
    if (!scrambled_counter_valid(counter))
        return 0;

    u32 state = LD32(LD32(dialogue_ptr + 0x1C));
    if (state == 1)
        apply_counter_normal(counter);
    else
        apply_counter_default(counter);

    unlock_player_after_dialogue();
    sub_8005F84_call();
    return 1;
}

/* dialogueEnd_unlockPlayer_8046E48(r5 = dialogue struct ptr) -> r0 */
Next dialogueEnd_unlockPlayer_8046E48(void) {
    R0 = handle_dialogue_unlock(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8046E84 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3, r4, r5, r6, r8, r9, r10, r11, r12; bool c, v; } call_sub_8047134_result;
/* register form: generated wrapper: shopBuyMenu_updateFrame_8047134 has no typed C interface yet */
static inline call_sub_8047134_result call_sub_8047134_sub_8046E84(void) {
    GUEST_CALL(shopBuyMenu_updateFrame_8047134);
    return (call_sub_8047134_result){ R2, R3, R4, R5, R6, R8, R9, R10, R11, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_sub_8046E84;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_8046E84 call_PlaySoundEffect_sub_8046E84(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_8046E84){ R1, R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; bool z, n; } call_chatbox_mask_eFlags2009F38_result_sub_8046E84;
/* register form: generated wrapper: chatbox_mask_eFlags2009F38 has no typed C interface yet */
static inline call_chatbox_mask_eFlags2009F38_result_sub_8046E84 call_chatbox_mask_eFlags2009F38_sub_8046E84(u32 r0) {
    R0 = r0;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return (call_chatbox_mask_eFlags2009F38_result_sub_8046E84){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r1; } call_chatbox_GetState_8045F4C_result;
/* register form: generated wrapper: chatbox_GetState_8045F4C has no typed C interface yet */
static inline call_chatbox_GetState_8045F4C_result call_chatbox_GetState_8045F4C(void) {
    GUEST_CALL(chatbox_GetState_8045F4C);
    return (call_chatbox_GetState_8045F4C_result){ R0, R1 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7; bool c, v; } call_chatbox_runScript_result;
/* register form: generated wrapper: chatbox_runScript has no typed C interface yet */
static inline call_chatbox_runScript_result call_chatbox_runScript(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(chatbox_runScript);
    return (call_chatbox_runScript_result){ R1, R2, R3, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: chatbox_set_eFlags2009F38 has no typed C interface yet */
static inline void call_chatbox_set_eFlags2009F38(u32 r0) {
    R0 = r0;
    GUEST_CALL(chatbox_set_eFlags2009F38);
}

/* Chatbox tick logic for sub_8046E84.
 * Handles B-cancel, busy-prompt (script 1), and default exit (script 3). */

typedef struct {
    u32 r10;
    u32 r11;
} ChatboxTickResult;

/* R5 = chatbox state pointer; toolkit in R10 is read by shopBuyMenu_updateFrame_8047134 via its wrapper.
 * Dispatches based on B-button press, the 0x20 chatbox flag, and current state. */
static ChatboxTickResult chatbox_tick(u32 cb) {
    call_sub_8047134_result setup = call_sub_8047134_sub_8046E84();

    u32 joypad_ptr = LD32(setup.r10 + oToolkit_JoypadPtr);
    u16 pressed = LD16(joypad_ptr + oJoypad_Pressed);

    bool run_default = false;

    if (pressed & 2) {
        /* B pressed: play "unselect" SFX, then fall through to default exit path */
        call_PlaySoundEffect_sub_8046E84(SOUND_UNSELECT_68, setup.r2);
        run_default = true;
    } else {
        call_chatbox_mask_eFlags2009F38_result_sub_8046E84 mask =
            call_chatbox_mask_eFlags2009F38_sub_8046E84(0x20);
        if (!mask.z) {
            /* Flag 0x20 is set – chatbox is mid-prompt; check its state */
            call_chatbox_GetState_8045F4C_result st =
                call_chatbox_GetState_8045F4C();
            if (st.r0 == 0) {
                /* State idle → default exit */
                run_default = true;
            } else {
                /* Chatbox active → run confirmation script 1, state = 8 */
                call_chatbox_runScript(LD32(cb + 0x24), 1);
                ST8(cb, 8);
            }
        }
        /* else: flag 0x20 clear → original jumps to end (nothing to do) */
    }

    if (run_default) {
        /* Set 0x40 flag, run good-bye script 3, state = 0xC */
        call_chatbox_set_eFlags2009F38(0x40);
        call_chatbox_runScript(LD32(cb + 0x24), 3);
        ST8(cb, 0xC);
    }

    ChatboxTickResult out = { setup.r10, setup.r11 };
    return out;
}

/* sub_8046E84(r5 = chatbox state ptr, r10 = toolkit, lr) -> r10, r11 */
Next sub_8046E84(void) {
    ChatboxTickResult out = chatbox_tick(R5);
    R10 = out.r10;
    R11 = out.r11;
    RETURN_TO_CALLER();
}

/* ── chatbox_confirmShopPurchase_8046ED2 ── */
/* register form: calls shopBuyMenu_updateFrame_8047134 outside this file, which has no typed C interface yet */
static void call_refresh_chatbox(void) {
    GUEST_CALL(shopBuyMenu_updateFrame_8047134);
}

/* register form: calls chatbox_mask_eFlags2009F38 outside this file, which has no typed C interface yet */
/* register form: relies on chatbox_mask_eFlags2009F38 leaving r0: its Z is r0 == 0 (and r0, r1), so testing
 * r0 is the original's branch on Z */
static u32 chatbox_mask_flag(u32 flag) {
    R0 = flag;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return R0;
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound(u32 sound) {
    R0 = sound;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls PlaySoundEffect with R7=0xFF (success path: "item acquired" marker,
 * preserved across the call so the function returns with R7=0xFF) */
static void play_sound_success(u32 sound) {
    R7 = 0xFF;
    R0 = sound;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls chatbox_GetState_8045F4C outside this file, which has no typed C interface yet */
static u32 chatbox_get_state(void) {
    GUEST_CALL(chatbox_GetState_8045F4C);
    return R0;
}

/* register form: calls checkScrambledCounter_8047366 outside this file, which has no typed C interface yet.
 * Inputs R0=counter R1=mult R2=cost; result in flags (Z=1 → check passed). */
static bool scrambled_check_fails(u32 counter, u32 mult, u32 cost) {
    R0 = counter; R1 = mult; R2 = cost;
    GUEST_CALL(checkScrambledCounter_8047366);
    return !ZERO_FLAG();
}

/* register form: calls sub_8047384 outside this file, which has no typed C interface yet.
 * Inputs R0=code R1=param R7=item_ptr R8=remaining; result in flags (Z=1 → available). */
static bool availability_fails(u32 code, u32 param, u32 item_ptr, u32 remaining) {
    R0 = code; R1 = param; R7 = item_ptr; R8 = remaining;
    GUEST_CALL(sub_8047384);
    return !ZERO_FLAG();
}

/* register form: calls GiveChips outside this file, which has no typed C interface yet.
 * R0 (chip code) arrives from sub_8047384; R2=qty. Preserves R4,R5,R7,R8. */
static u32 give_chips(u32 chip_code, u32 qty) {
    R2 = qty;
    GUEST_CALL(GiveChips);
    return R0;
}

/* register form: calls GiveItem outside this file, which has no typed C interface yet */
static u32 give_item_sub_8046ED2(u32 id, u32 qty) {
    R0 = id; R1 = qty;
    GUEST_CALL(GiveItem);
    return R0;
}

/* register form: calls GiveNaviCustPrograms outside this file, which has no typed C interface yet */
static u32 give_navicust(u32 prog, u32 qty) {
    R0 = prog; R2 = qty;
    GUEST_CALL(GiveNaviCustPrograms);
    return R0;
}

/* register form: calls storeObfuscatedRandomToken_804733C outside this file, which has no typed C interface yet */
static void store_obfuscated_token(u32 value) {
    R0 = value;
    GUEST_CALL(storeObfuscatedRandomToken_804733C);
}

/* register form: calls chatbox_runScript outside this file, which has no typed C interface yet */
static void run_chatbox_script_sub_8046ED2(u32 archive, u32 script_idx) {
    R0 = archive; R1 = script_idx;
    GUEST_CALL(chatbox_runScript);
}

/* register form: calls sub_8048DAE outside this file, which has no typed C interface yet */
static void record_purchase(u32 selected_idx, u32 kind, u32 code, u32 param, u32 type_val, u32 item_ptr) {
    R0 = selected_idx; R1 = kind; R2 = code; R3 = param; R4 = type_val; R7 = item_ptr;
    GUEST_CALL(sub_8048DAE);
}

/* register form: calls sub_8047B88 outside this file, which has no typed C interface yet.
 * R7 = item_ptr is an input to sub_8047B88. */
static u32 get_updated_code(u32 code, u32 item_ptr) {
    R0 = code; R7 = item_ptr;
    GUEST_CALL(sub_8047B88);
    return R0;
}

/* register form: calls menu_clampListScrollAfterCountChange_8047CB6 outside this file, which has no typed C interface yet */
static void menu_clamp_list(void) {
    GUEST_CALL(menu_clampListScrollAfterCountChange_8047CB6);
}

/* Handle item/chip/navi-cust selection in the shop menu.
 * ctx (r5) is the dialog-box state pointer. Returns 0 (always). */
static u32 handle_item_selection(u32 ctx) {
    call_refresh_chatbox();

    /* If both confirm buttons are pressed, mark "pressed" and play unselect sound */
    if (chatbox_mask_flag(0x20) && chatbox_mask_flag(0x100)) {
        ST8(ctx + 7, 1);
        play_sound(SOUND_UNSELECT_68);
    }

    /* Gate: flag 8 must be set to proceed */
    if (!chatbox_mask_flag(8))
        return 0;

    /* If chatbox is busy displaying text */
    if (chatbox_get_state() != 0) {
        if (!LD8(ctx + 7))
            play_sound(SOUND_UNSELECT_68);
        run_chatbox_script_sub_8046ED2(LD32(ctx + 0x24), 1);
        ST8(ctx, 8);
        return 0;
    }

    /* Compute adjusted cost: type 1 uses raw cost, others multiply by 100 */
    u32 type_val = LD32(LD32(ctx + 0x1C));
    u32 item_ptr = LD32(ctx + 0x20);
    u32 raw_cost = LD16(item_ptr + 6);
    u32 mult = (type_val == 1) ? 1 : 0x64;
    u32 adjusted_cost = raw_cost * mult;

    /* Verify scrambled counter allows the purchase */
    u32 counter = LD32(ctx + 0x28);
    if (scrambled_check_fails(counter, mult, adjusted_cost)) {
        run_chatbox_script_sub_8046ED2(LD32(ctx + 0x24), 8);
        play_sound(SOUND_CANT_JACK_IN);
        ST8(ctx, 0x1C);
        return 0;
    }

    s32 remaining = (s32)(counter - adjusted_cost);
    if (remaining < 0) {
        run_chatbox_script_sub_8046ED2(LD32(ctx + 0x24), 8);
        play_sound(SOUND_CANT_JACK_IN);
        ST8(ctx, 0x1C);
        return 0;
    }

    /* Play select sound when browsing a "type 2" category */
    if (type_val == 2)
        play_sound(SOUND_SELECT_67);

    /* Dispatch by item kind byte */
    item_ptr = LD32(ctx + 0x20);
    u32 kind = LD8(item_ptr);
    u32 script_idx;

    if (kind == 2) {
        /* Chip */
        if (type_val == 2) {
            /* Special chip: store to 0x2C, set state 0x28, run script 6 */
            ST32(ctx + 0x2C, (u32)remaining);
            store_obfuscated_token((u32)remaining);
            ST8(ctx, 0x28);
            run_chatbox_script_sub_8046ED2(LD32(ctx + 0x24), 6);
            return 0;
        }
        u32 chip_code = LD16(item_ptr + 2);
        if (availability_fails(chip_code, LD8(item_ptr + 4), item_ptr, (u32)remaining)) {
            run_chatbox_script_sub_8046ED2(LD32(ctx + 0x24), 1);
            ST8(ctx, 8);
            return 0;
        }
        if (give_chips(chip_code, 1) != 0) {
            run_chatbox_script_sub_8046ED2(LD32(ctx + 0x24), 9);
            play_sound(SOUND_CANT_JACK_IN);
            ST8(ctx, 0x1C);
            return 0;
        }
        script_idx = 7;
    } else if (kind == 1) {
        /* Item */
        u32 item_id = LD16(item_ptr + 2);
        if (availability_fails(item_id, 0xFF, item_ptr, (u32)remaining)) {
            run_chatbox_script_sub_8046ED2(LD32(ctx + 0x24), 1);
            ST8(ctx, 8);
            return 0;
        }
        if (give_item_sub_8046ED2(item_id, 1) != 0) {
            run_chatbox_script_sub_8046ED2(LD32(ctx + 0x24), 9);
            play_sound(SOUND_CANT_JACK_IN);
            ST8(ctx, 0x1C);
            return 0;
        }
        script_idx = 6;
    } else {
        /* Navi Cust Program */
        u32 prog_code = LD16(item_ptr + 2);
        if (availability_fails(prog_code, LD8(item_ptr + 4), item_ptr, (u32)remaining)) {
            run_chatbox_script_sub_8046ED2(LD32(ctx + 0x24), 1);
            ST8(ctx, 8);
            return 0;
        }
        if (give_navicust(prog_code + 0x90, 1) != 0) {
            run_chatbox_script_sub_8046ED2(LD32(ctx + 0x24), 9);
            play_sound(SOUND_CANT_JACK_IN);
            ST8(ctx, 0x1C);
            return 0;
        }
        script_idx = 0xC;
    }

    /* Success: update counter, record purchase, refresh menu list */
    ST32(ctx + 0x28, (u32)remaining);
    store_obfuscated_token((u32)remaining);

    item_ptr = LD32(ctx + 0x20);
    type_val = LD32(LD32(ctx + 0x1C));
    record_purchase(LD8(ctx + 4), LD8(item_ptr), LD16(item_ptr + 2),
                    LD8(item_ptr + 4), type_val, item_ptr);

    /* Reload item_ptr and pass it as R7 input to sub_8047B88 */
    item_ptr = LD32(ctx + 0x20);
    ST16(ctx + 0x12, get_updated_code(LD16(item_ptr + 2), item_ptr));
    menu_clamp_list();

    ST8(ctx, 0x1C);
    run_chatbox_script_sub_8046ED2(LD32(ctx + 0x24), script_idx);
    ST8(ctx + 0xD, 0xFF);
    play_sound_success(SOUND_UNK_73);
    return 0;
}

/* chatbox_confirmShopPurchase_8046ED2(r5 = ctx) -> r0 = 0, nz/c/v from final R0 */
Next chatbox_confirmShopPurchase_8046ED2(void) {
    R0 = handle_item_selection(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_stepContextScript_8047028 ── */
/* chatbox_stepContextScript_8047028: run the chatbox script attached to the dialogue context in R5.
 *
 * First updates some inter-frame state (shopBuyMenu_updateFrame_8047134), then, while the
 * 0x8 event flag is set, stores the text scroll speed into the context's
 * state byte and starts the script indexed from the context:
 *   - offset 0x12 (halfword) nonzero -> script #1,  speed 8
 *   - offset 0x12 (halfword) zero    -> script #10, speed 32
 * The script itself is read from the archive pointer at offset 0x24.
 *
 * Always returns R0 = 0 (Z set), with R1/R2 left as the callees left them.
 * Preserves R4-R7 and everything from R8 up.
 */
Next chatbox_stepContextScript_8047028(void) {
    GUEST_CALL(shopBuyMenu_updateFrame_8047134);

    /* Proceed only while event flag 0x8 is set. */
    R0 = 8;
    GUEST_CALL(chatbox_mask_eFlags2009F38); /* answers in Z; the original branches on it */

    if (!ZERO_FLAG()) {
        /* Pick the script and scroll speed based on the repeat counter. */
        u32 scriptIdx = 1;
        u32 scrollSpeed = 8;

        if (LD16(R5 + 0x12) == 0) {
            scriptIdx = 0xA;
            scrollSpeed = 0x20;
        }

        ST8(R5, scrollSpeed);
        R0 = LD32(R5 + 0x24);
        R1 = scriptIdx;
        GUEST_CALL(chatbox_runScript);
    }

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── text_decompressMessageAssets_80471F8 ── */
/* text_decompressMessageAssets_80471F8: decompress the three LZ77-compressed blobs the message
 * system works from into their EWRAM buffers.
 *
 * The first blob's address is fetched through r5: r5->[0x1C]->[4]. The other
 * two are fixed data files. r4-r7 must come out unchanged. */
Next text_decompressMessageAssets_80471F8(void) {
    u32 sceneSrc = LD32(LD32(R5 + 0x1C) + 4);

    /* Blob reached through the r5 handle -> unk_2029A00. */
    R0 = sceneSrc;
    R1 = A_unk_2029A00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    /* CompText873DE4C -> eDecomp202BA00. */
    R0 = A_CompText873DE4C;
    R1 = A_eDecomp202BA00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    /* CompText873ECC8 -> eDecompressedTextArchive202DA00. */
    R0 = A_CompText873ECC8;
    R1 = A_eDecompressedTextArchive202DA00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    RETURN_TO_CALLER();
}

/* ── sprite_emitColumnEntry_80472E8 ── */
/* byte_8047318: halfword table indexed by column -- columns 0-5 hold
 * 0xE002, columns 6-11 hold 0xE006, columns 12-17 hold 0xE00A. */
#define COLUMN_COUNT 0x12

/* register form: calls sprite_queueOamEntryThunk_802FE28 outside this file, which has no typed C interface yet */
static void emit_column(u32 packed, u16 entry) {
    R0 = packed;
    R1 = entry;
    R2 = 0;
    R3 = 0;
    GUEST_CALL(sprite_queueOamEntryThunk_802FE28);
}

/* What one step leaves behind: the column's table halfword (r1, which
 * sprite_queueOamEntryThunk_802FE28 preserves), the advanced column counter (r2) and 0 (r3). */
typedef struct {
    u16 entry;
    u32 next_column;
    u32 zero;
} ColumnStep;

/* Feed the packed pair and the current column's table entry to sprite_queueOamEntryThunk_802FE28,
 * then advance the 18-column counter, wrapping back to 0 once it passes the
 * last column (the original compares signed, so negative counters are left
 * to keep incrementing). */
static ColumnStep advance_column(u32 upper, u32 lower, u32 column) {
    u32 packed = 0x40000000 | (upper << 16) | lower;
    u16 entry = LD16(A_byte_8047318 + column * 2);

    emit_column(packed, entry);

    u32 next = column + 1;
    if ((s32)next >= (s32)COLUMN_COUNT)
        next = 0;

    ColumnStep result = { entry, next, 0 };
    return result;
}

/* sprite_emitColumnEntry_80472E8(r0 = upper halfword, r1 = lower halfword, r2 = column counter)
 * Passes the packed halves plus the column's table entry on to sprite_queueOamEntryThunk_802FE28,
 * and leaves the incremented (wrapped) counter in r2. r4-r7 and lr are
 * untouched, r0 is scratch. */
Next sprite_emitColumnEntry_80472E8(void) {
    ColumnStep result = advance_column(R0, R1, R2);
    R1 = result.entry;
    R2 = result.next_column;
    R3 = result.zero;
    RETURN_TO_CALLER();
}

/* ── storeObfuscatedRandomToken_804733C ── */
/* storeObfuscatedRandomToken_804733C(r0 = base value, r5 = object):
 * Rolls a random byte (retrying until it is nonzero) and stores an obfuscated
 * code derived from it into object+0x30:
 *
 *     stored = ~(((base + byte) << 8) | (byte_hi << 28) | (byte_lo ^ byte_hi))
 *
 * where byte_hi = byte >> 4 and byte_lo = byte & 0xF.  The low byte of the
 * stored value is a XOR-fold of the random byte, the top nibble repeats
 * byte_hi, and the middle bits hold (base + byte) shifted up by 8.
 *
 * On return r0-r4, r5 and r7 are unchanged (the original pushes/pops r0-r4
 * and never writes r5/r7).  Flags left for the caller: C = 0 (carry out of
 * the `lsl r3,#28` is bit 8 of the masked byte, always 0) and V = signed
 * overflow of `base + byte` (from the `adds r1,r1,r2`); N/Z come from the
 * final `mvn r1,r1`.
 */
Next storeObfuscatedRandomToken_804733C(void) {
    u32 in0 = R0, in1 = R1, in2 = R2, in3 = R3, in4 = R4;
    u32 base = R0;
    u32 obj = R5;

    /* 08047340: draw a random value until its low byte is nonzero */
    u32 byte;
    do {
        GUEST_CALL(GetPositiveSignedRNG);
        byte = R0 & 0xFF;
    } while (byte == 0);

    u32 hi = byte >> 4;
    u32 lo = (byte & 0xF) ^ hi;
    u32 sum = base + byte;
    u32 stored = ~((sum << 8) | (hi << 28) | lo);

    /* 08047362: str r1, [r5,#0x30] */
    ST32(obj + 0x30, stored);

    /* restore what the original's `pop {r0-r4,pc}` restores */
    R0 = in0; R1 = in1; R2 = in2; R3 = in3; R4 = in4; R5 = obj;

    /* flags: V from the adds, C from the later `lsl r3,#28` (always 0),
     * N/Z from the final `mvn r1,r1` */
    SET_FLAGS_ADD(base, byte);
    cpu.c = 0;
    SET_FLAGS_NZ(stored);
    RETURN_TO_CALLER();
}

/* ── checkScrambledCounter_8047366 ── */
/* checkScrambledCounter_8047366(r0 = expected value, r5 = record):
 * Validates a scrambled 32-bit field kept at record+0x30. The field is stored
 * bitwise-inverted (so the game can detect it being cleared to zero) and packed as
 *     bits  8..27 : the actual 20-bit counter
 *     bits 28..31 : a "high" nibble h
 *     bits  0..3  : a "low"  nibble l
 * The value compared against r0 is counter - (h * 16 + (l ^ h)).
 * Only the flags are meaningful to callers; r0/r1/r2/r4 must come back unchanged. */
Next checkScrambledCounter_8047366(void) {
    u32 expected = R0;
    u32 field = ~LD32(R5 + 0x30);

    u32 counter = (field << 4) >> 12;   /* bits 8..27 of the field */
    u32 highNibble = field >> 28;
    u32 lowNibble = field & 0xF;
    u32 tweak = (highNibble << 4) | (lowNibble ^ highNibble);

    SET_FLAGS_CMP(counter - tweak, expected);
    RETURN_TO_CALLER();
}

/* ── battleFindEntryByFieldByte_8047456 ── */
/* battleFindEntryByFieldByte_8047456(r0 = value to look up, r4 = entry count, r7 = table base):
 * Scan a table of 8-byte entries, comparing r0 against the byte at offset 4
 * of each entry. Sets Z (found) when a match is encountered, clears it when
 * the whole table was searched without a match. r3-r7 are preserved. */
Next battleFindEntryByFieldByte_8047456(void) {
    u32 needle = R0;
    u32 entry = R7;
    u32 notFound = 1;

    for (s32 i = 0; i < (s32)R4; i++) {
        if (LD8(entry + 4) == needle) {
            notFound = 0;
            break;
        }
        entry += 8;
    }

    SET_FLAGS_NZ(notFound);
    RETURN_TO_CALLER();
}

/* ── copyData_804747C ── */
/* register form: calls ZeroFillGFX30025c0 outside this file, which has no typed C interface yet */
static void call_zero_fill_gfx(void) {
    GUEST_CALL(ZeroFillGFX30025c0);
}

/* register form: calls battle_resetEStructRecords_8046664 outside this file, which has no typed C interface yet */
static void call_init_sub_8046664(void) {
    GUEST_CALL(battle_resetEStructRecords_8046664);
}

/* register form: calls decompAndCopyData outside this file, which has no typed C interface yet */
static void call_decomp_and_copy_data(u32 table_addr) {
    R0 = table_addr;
    GUEST_CALL(decompAndCopyData);
}

/* Load game data into VRAM/work RAM: zero-fill GFX scratch buffers,
 * run the per-object init pass, then walk the ROM data table at
 * off_8047494 to decompress and copy all static graphics and data.
 * All output registers and flags are set by the final callee. */
static void load_game_data(void) {
    call_zero_fill_gfx();
    call_init_sub_8046664();
    call_decomp_and_copy_data(A_off_8047494);
}

/* copyData_804747C: no computed outputs; callees leave r2-r7 and nz/c/v in place. */
Next copyData_804747C(void) {
    load_game_data();
    RETURN_TO_CALLER();
}

/* ── chatbox_uploadWindowTileGfx_804754C ── */
/* chatbox_uploadWindowTileGfx_804754C: upload the current text window's tile graphics.
 *
 * Copies 0x800 bytes of tile data from the staging buffer at byte_2017204
 * into the second half of the BG tile block (offset 0x800 of the toolkit's
 * iBGTileIdBlocks allocation), then renders the text object passed in r5
 * with the standard text-gfx renderer into the text layer (VRAM 0x6002000).
 *
 * Inputs:  r3 (context passed through), r5 = text object, r10 = toolkit.
 * Outputs: r3 r4 r6 r7 r10 and v as left by renderTextGfx_8045F8C;
 *          r5 r8 r9 preserved.
 */
Next chatbox_uploadWindowTileGfx_804754C(void) {
    u32 textObject = R5;

    /* Copy tile gfx from the staging buffer into BG tile block #1. */
    R0 = A_byte_2017204;
    R1 = LD32(R10 + oToolkit_iBGTileIdBlocks_Ptr) + 0x800;
    R2 = 0x800;
    GUEST_CALL(CopyByEightWords); /* (src, dest, size); preserves r3-r7, r10 */

    /* Render the caller's text object. */
    R0 = LD32(textObject + 0x24);
    R1 = 0x10;
    R2 = A_unk_2015E00;
    R3 = 0x6002000;
    R4 = 1;
    R5 = 1;
    R6 = A_dword_86A5D60;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    R5 = textObject;
    RETURN_TO_CALLER();
}

/* ── sub_804758C ── */
/* sub_804758C: thin wrapper that tail-calls mail_drawSelectedEntry_80478D4 (all caller registers
 * are preserved, results come straight from the callee). */
Next sub_804758C(void) {
    GUEST_CALL(mail_drawSelectedEntry_80478D4);
    RETURN_TO_CALLER();
}

/* ── background_LoadAreaTiles_8047800 ── */
/* background_LoadAreaTiles_8047800(r5 = background state struct):
 * Redraw both background layers for the area whose id is stored at state+8.
 * The id is an index into the area table starting at 0x15; layer 1 copies its
 * tile graphics from the shared tile-id table eTileIds2018A04 into tile block
 * 2, layer 2 copies its tile map from byte_202F800 into tile block 0.
 * Preserves r4-r7 (the original pushes/pops them around the two calls). */
Next background_LoadAreaTiles_8047800(void) {
    const u32 state = R5;
    const u32 savedR4 = R4;
    const u32 savedR6 = R6;
    const u32 savedR7 = R7;
    const u32 areaId = LD8(state + 8);

    /* Layer 1: tile graphics, 0x15 x 0xC tiles into tile block 2. */
    R0 = areaId - 0x15;
    R1 = 0;
    R2 = 2;
    R3 = A_eTileIds2018A04;
    R4 = 0x15;
    R5 = 0xC;
    R7 = state;
    GUEST_CALL(CopyBackgroundTiles);

    /* Layer 2: tile map, 0x12 x 0xA tiles into tile block 0.
     * Its id is two past the graphics id (re-read from the struct as the
     * original does). */
    R0 = (LD8(state + 8) - 0x15) + 2;
    R1 = 1;
    R2 = 0;
    R3 = A_byte_202F800;
    R4 = 0x12;
    R5 = 0xA;
    R7 = state;
    GUEST_CALL(CopyBackgroundTiles);

    /* Restore the callee-saved registers the original pops. */
    R4 = savedR4;
    R5 = state;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_80478B8 ── */
/* sub_80478B8:
 *   inputs:  lr
 *   outputs: r2 r5 r7
 *   clobbers (unused by callers): nz c v r0 r1 r3 lr
 *   preserved: r4 r5 r6 r7
 *
 * Initializes chapter/area state by calling sub_8046696 four times with
 * indices 0..3, then returns to the address saved in LR. */
Next sub_80478B8(void) {
    for (u32 i = 0; i < 4; i++) {
        R0 = i;
        GUEST_CALL(sub_8046696);
    }
    RETURN_TO_CALLER();
}

/* ── mail_drawSelectedEntry_80478D4 ── */
/* Mail list entry object (arrives in r5) — field offsets as used by the disassembly. */
#define ENTRY_CURSOR_INDEX  0x08  /* u8: which list row is currently highlighted */
#define ENTRY_INFO_PTR      0x1C  /* -> entry info record */
#define ENTRY_TEXT_PTR      0x24  /* reader text shown for this entry */
#define ENTRY_ID            0x28  /* entry id recorded in the shared menu state */
#define MENU_STATE_ID_SLOT  8     /* id slot inside the unk_200a220 struct */

/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet */
static void copy_background_tiles_sub_80478D4(u32 j, u32 i, u32 which_tile_block_32x32,
                                  u32 tile_ids, u32 j_size, u32 i_size)
{
    R0 = j;
    R1 = i;
    R2 = which_tile_block_32x32;
    R3 = tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
}

/* register form: calls renderTextGfx_8045F8C outside this file, which has no typed C interface yet */
static void render_text_gfx(u32 text_ptr, u32 style, u32 source_gfx, u32 dest_vram,
                            u32 param4, u32 param5, u32 lookup_table, u32 param7)
{
    R0 = text_ptr;
    R1 = style;
    R2 = source_gfx;
    R3 = dest_vram;
    R4 = param4;
    R5 = param5;
    R6 = lookup_table;
    R7 = param7;
    GUEST_CALL(renderTextGfx_8045F8C);
}

/* Redraw the currently highlighted entry of the mail list: its icon tiles,
 * the name strip next to it, and the text the reader shows for it. */
static void draw_highlighted_mail_entry(u32 entry, u32 toolkit)
{
    u32 cursor = LD8(entry + ENTRY_CURSOR_INDEX);
    u32 info_value = LD32(LD32(entry + ENTRY_INFO_PTR));

    /* Info value 1 selects the alternate icon tile table. */
    u32 icon_tile_ids = (info_value == 1) ? A_unk_2019404 : A_unk_2019204;

    /* Icon: 9x5 tiles out of background block 2, one row higher per entry. */
    copy_background_tiles_sub_80478D4(0x1E - (cursor - 0xC), 0, 2, icon_tile_ids, 9, 5);

    /* Name strip: 7x2 tiles out of background block 0, just below the icon. */
    copy_background_tiles_sub_80478D4(0x1E - (cursor - 0xD), 2, 0, A_byte_8046B58, 7, 2);

    /* Remember which entry is selected, then render its text into VRAM. */
    ST32(LD32(toolkit + oToolkit_Unk200a220_Ptr) + MENU_STATE_ID_SLOT,
         LD32(entry + ENTRY_ID));
    render_text_gfx(LD32(entry + ENTRY_TEXT_PTR),
                    (info_value == 1) ? 0xD : 0xE,
                    A_unk_2015A00,
                    0x600A400 /* reader text tiles in VRAM */,
                    7, 1, A_dword_86A5D60, 0);
}

/* mail_drawSelectedEntry_80478D4: r5 = mail list entry, r10 = toolkit.
 * The routine itself preserves r4-r9 (the original push/pop), and the callees
 * leave r10-r12 alone, so only the callee-clobbered r4-r7 need restoring. */
Next mail_drawSelectedEntry_80478D4(void) {
    u32 keep_r4 = R4, keep_r5 = R5, keep_r6 = R6, keep_r7 = R7;
    draw_highlighted_mail_entry(R5, R10);
    R4 = keep_r4;
    R5 = keep_r5;
    R6 = keep_r6;
    R7 = keep_r7;
    RETURN_TO_CALLER();
}

/* ── menu_showDialogOnUnselect_8047A80 ── */
/* menu_showDialogOnUnselect_8047A80(r5 = struct ptr):
 * Puts the owner struct into its "show dialog" state (0x14), makes the
 * chatbox run text script #2 from the text archive stored in the struct,
 * and plays the menu-unselect sound. */
Next menu_showDialogOnUnselect_8047A80(void) {
    /* 0x14 = this struct's dialog/state id */
    ST8(R5, 0x14);

    /* chatbox_runScript(archive, script 2) */
    R0 = LD32(R5 + 0x24);
    R1 = 2;
    GUEST_CALL(chatbox_runScript);

    /* menu-unselect sound effect */
    R0 = SOUND_UNSELECT_68;
    GUEST_CALL(PlaySoundEffect);

    RETURN_TO_CALLER();
}

/* ── sub_8047B88 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2; } call_ZeroFillByWord_result_sub_8047B88;
/* register form: generated wrapper: ZeroFillByWord has no typed C interface yet */
static inline call_ZeroFillByWord_result_sub_8047B88 call_ZeroFillByWord_sub_8047B88(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ZeroFillByWord);
    return (call_ZeroFillByWord_result_sub_8047B88){ R1, R2 };
}

typedef struct { u32 r0; } call_GetCurPETNavi_result_sub_8047B88;
/* register form: generated wrapper: GetCurPETNavi has no typed C interface yet */
static inline call_GetCurPETNavi_result_sub_8047B88 call_GetCurPETNavi_sub_8047B88(void) {
    GUEST_CALL(GetCurPETNavi);
    return (call_GetCurPETNavi_result_sub_8047B88){ R0 };
}

#define ITEM_ENTRY_SIZE 8
#define ITEM_TYPE      0
#define ITEM_STOCK     1
#define ITEM_NUM       2
#define ITEM_CATEGORY  4
#define ITEM_FLAG      5
#define ITEM_EXTRA     6

#define oModeData_Mode      0
#define oModeData_ItemBase  8
#define oModeData_ItemCount 0xC

#define BYTE_2019A00  0x2019A00U  /* output shop-item buffer (16 slots × 8 bytes) */
#define BYTE_201AE00  0x201AE00U  /* PET-navi source item list */

/* register form: wrapper call_sub_8047CB0 declares no inputs, but the original
 * passes r0=type, r1=num through registers that the wrapper does not set. */
static bool shop_item_filter(u8 item_type, u16 item_num) {
    R0 = item_type;
    R1 = item_num;
    GUEST_CALL(sub_8047CB0);
    return ZERO_FLAG();
}

/* register form: wrapper call_sub_8047CA8 declares no inputs, but the original
 * passes r0=item_num through a register the wrapper does not set. */
static bool pet_navi_filter(u16 item_num) {
    R0 = item_num;
    GUEST_CALL(sub_8047CA8);
    return ZERO_FLAG();
}

/* Result of building a shop item list. */
struct ShopBuildResult {
    u32 count;  /* number of valid entries written to byte_2019A00 */
};

/* Regular shop path: scan inventory, deduplicate, filter, copy survivors. */
static struct ShopBuildResult build_regular_shop_list(u32 mode_data, u32 toolkit_ptr) {
    u32 item_count = LD32(mode_data + oModeData_ItemCount);
    u32 src = LD32(mode_data + oModeData_ItemBase)
            + LD32(toolkit_ptr + oToolkit_ShopDataPtr);

    u32 dest = BYTE_2019A00 - ITEM_ENTRY_SIZE;
    u32 last_type = 0, last_num = 0, last_category = 0;

    u32 i = 0;
    do {
        u8  type     = LD8(src + ITEM_TYPE);
        u8  stock    = LD8(src + ITEM_STOCK);
        u16 num      = LD16(src + ITEM_NUM);
        u8  category = LD8(src + ITEM_CATEGORY);

        bool copy = true;

        if (type == 0 || stock == 0) {
            copy = false;
        } else if (type == last_type && num == last_num && category == last_category) {
            copy = false;
        } else if (!shop_item_filter(type, num)) {
            copy = false;
        } else {
            u32 pet = call_GetCurPETNavi_sub_8047B88().r0;
            if (pet != 0 && type == 1 && num == 0x70)
                copy = false;
        }

        if (copy) {
            dest += ITEM_ENTRY_SIZE;
            ST8(dest + ITEM_TYPE,     LD8(src + ITEM_TYPE));
            ST8(dest + ITEM_STOCK,    LD8(src + ITEM_STOCK));
            ST16(dest + ITEM_NUM,     LD16(src + ITEM_NUM));
            ST8(dest + ITEM_CATEGORY, LD8(src + ITEM_CATEGORY));
            ST8(dest + ITEM_FLAG,     LD8(src + ITEM_FLAG));
            ST16(dest + ITEM_EXTRA,   LD16(src + ITEM_EXTRA));

            last_type     = LD8(dest + ITEM_TYPE);
            last_num      = LD16(dest + ITEM_NUM);
            last_category = LD8(dest + ITEM_CATEGORY);
        }

        src += ITEM_ENTRY_SIZE;
        i++;
    } while ((s32)i < (s32)item_count);

    /* Count output slots whose stock byte is non-zero */
    u32 count = 0;
    u32 ptr = BYTE_2019A00;
    u32 j = 0;
    do {
        if (LD8(ptr + ITEM_STOCK) != 0)
            count++;
        ptr += ITEM_ENTRY_SIZE;
        j++;
    } while ((s32)j < (s32)item_count);

    return (struct ShopBuildResult){ count };
}

/* PET-navi path: scan byte_201AE00, filter via sub_8047CA8, copy survivors,
 * decrement stock for the requested target item. */
static struct ShopBuildResult build_pet_navi_list(u32 mode_data, u32 target) {
    u32 item_count = LD32(mode_data + oModeData_ItemCount);
    u32 src  = BYTE_201AE00;
    u32 dest = BYTE_2019A00 - ITEM_ENTRY_SIZE;

    u32 i = 0;
    do {
        u16 item_num = LD16(src + ITEM_NUM);

        if (pet_navi_filter(item_num)) {
            dest += ITEM_ENTRY_SIZE;
            ST8(dest + ITEM_TYPE,     LD8(src + ITEM_TYPE));
            ST16(dest + ITEM_NUM,     LD16(src + ITEM_NUM));
            ST8(dest + ITEM_CATEGORY, LD8(src + ITEM_CATEGORY));
            ST8(dest + ITEM_FLAG,     LD8(src + ITEM_FLAG));
            ST16(dest + ITEM_EXTRA,   LD16(src + ITEM_EXTRA));

            u8 copied_stock = LD8(src + ITEM_STOCK);
            ST8(dest + ITEM_STOCK, copied_stock);

            if (LD16(dest + ITEM_NUM) == target && copied_stock != 0xFF) {
                copied_stock--;
                ST8(src + ITEM_STOCK,  copied_stock);
                ST8(dest + ITEM_STOCK, copied_stock);
            }
        }

        src += ITEM_ENTRY_SIZE;
        i++;
    } while ((s32)i < (s32)item_count);

    /* Count output slots whose item-number halfword is non-zero */
    u32 count = 0;
    u32 ptr = BYTE_2019A00;
    u32 j = 0;
    do {
        if (LD16(ptr + ITEM_NUM) != 0)
            count++;
        ptr += ITEM_ENTRY_SIZE;
        j++;
    } while ((s32)j < (s32)item_count);

    return (struct ShopBuildResult){ count };
}

/* Master entry: build a filtered shop item list into byte_2019A00. */
static u32 build_shop_list(u32 target, u32 state_ptr, u32 toolkit_ptr) {
    call_ZeroFillByWord_sub_8047B88(BYTE_2019A00, 0x80);

    u32 mode_data = LD32(state_ptr + 0x1C);

    if (LD32(mode_data + oModeData_Mode) == 2)
        return build_pet_navi_list(mode_data, target).count;

    return build_regular_shop_list(mode_data, toolkit_ptr).count;
}

/* sub_8047B88(r0 = target item num, r5 = state ptr, r10 = toolkit ptr) → r0 = entry count */
Next sub_8047B88(void) {
    R0 = build_shop_list(R0, R5, R10);
    RETURN_TO_CALLER();
}

/* ── stub_doNothingReturnZero_8047CA0 ── */
/* stub_doNothingReturnZero_8047CA0: stub that does nothing except return 0 in r0.
 * Sets the flags as `tst r0, r0` with r0 = 0 would (Z set, N clear);
 * r2-r7 are preserved. */
Next stub_doNothingReturnZero_8047CA0(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8047CB0 ── */
/* sub_8047CB0: a stub that always reports "no"/zero; callers branch on the
 * flags it leaves (Z set, N clear, as produced by `mov r0, #0`). */
Next sub_8047CB0(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── menu_clampListScrollAfterCountChange_8047CB6 ── */
/* menu_clampListScrollAfterCountChange_8047CB6(r5 = object):
 * Re-clamp the two u16 fields at +0x14 and +0x18 of the object at r5 after the
 * value at +0x12 changed: +0x14 is pulled down to value-1 (when value is 1..4)
 * and +0x18 is pulled down to value-5 (when value is 5 or more), each only if
 * it is currently above that bound. Callers only inspect the V flag, which
 * reflects the last comparison performed. */
Next menu_clampListScrollAfterCountChange_8047CB6(void) {
    u32 value = LD16(R5 + 0x12);

    if ((s32)value > 4) {
        /* Clamp +0x18 to at most value-5: the test is value < (+0x18)+5. */
        u32 limit = LD16(R5 + 0x18) + 5;
        SET_FLAGS_CMP(value, limit);
        if ((s32)value >= (s32)limit)
            RETURN_TO_CALLER();             /* already low enough */
        /* The subtract below is flag-setting in the original; V ends up clear. */
        SET_FLAGS_CMP(value, 5);
        ST16(R5 + 0x18, value - 5);
    } else {
        /* For small values the +0x14 check runs instead; the TST used to skip
         * the zero case leaves V untouched, so V still comes from this compare
         * when we return early. */
        SET_FLAGS_CMP(value, 4);
        if (value != 0) {
            u32 reduced = value - 1;
            u32 index = LD16(R5 + 0x14);
            SET_FLAGS_CMP(reduced, index);
            if ((s32)reduced <= (s32)index) /* only lower it, never raise it */
                ST16(R5 + 0x14, reduced);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_8047CDC ── */
/* sub_8047CDC(r1 = value): scale a value by 100 unless a mode flag says otherwise.
 * Loads the first word of the struct at [r5 + 0x1C]; when it equals 1 the value
 * is left untouched, otherwise it is multiplied by 100. All registers except
 * r1 are preserved for the caller. */
Next sub_8047CDC(void) {
    u32 modePtr = LD32(R5 + 0x1C);

    if (LD32(modePtr) != 1)
        R1 = R1 * 100;

    RETURN_TO_CALLER();
}

/* ── copy_table_80469F0_to_202F800_8047CEC ── */
/* copy_table_80469F0_to_202F800_8047CEC: copy the 0x168-word table at byte_80469F0 (ROM) into
 * the working buffer at byte_202F800 (RAM). */
Next copy_table_80469F0_to_202F800_8047CEC(void) {
    R0 = A_byte_80469F0;   /* source: word table in ROM */
    R1 = A_byte_202F800;   /* destination: EWRAM buffer */
    R2 = 0x168;            /* word count */
    GUEST_CALL(CopyWords);
    RETURN_TO_CALLER();
}

/* ── sub_8047D84 ── */
/* Address of one battle grid cell: (column + row) entries of 8 bytes each,
 * offset from the base of the cell data table at byte_2019A00.
 * The column and row live as u16s at word_2011C24 / word_2011C28 in the
 * battle state block at byte_2011C10.
 *
 * Note: the original pushes/pops r4-r7, so r4-r6 and r7 are left exactly as
 * they came in; only r0 carries the result (r1 and the flags are scratch). */
static u32 battle_cell_address(void) {
    u32 column = LD16(A_word_2011C24);
    u32 row = LD16(A_word_2011C28);
    return (column + row) * 8 + A_byte_2019A00;
}

/* sub_8047D84() -> r0 (r4-r7 preserved; r1, nz/c/v scratch) */
Next sub_8047D84(void) {
    R0 = battle_cell_address();
    RETURN_TO_CALLER();
}

/* ── sub_8048C24 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_GetCurPETNavi_result_sub_8048C24;
/* register form: generated wrapper: GetCurPETNavi has no typed C interface yet */
static inline call_GetCurPETNavi_result_sub_8048C24 call_GetCurPETNavi_sub_8048C24(void) {
    GUEST_CALL(GetCurPETNavi);
    return (call_GetCurPETNavi_result_sub_8048C24){ R0 };
}

/* Walk a shop's item entries and report whether any "unique" item (kind byte == 1)
 * with a non-sentinel price is being offered while no PET-navi is active.
 * Returns 1 in r0 (and leaves r2 = entry pointer, r3 = active-flag) if such an
 * item is found; returns 0 (with r2 past the last entry, r3 = last flag read) otherwise. */

typedef struct { u32 r0; u32 r2; u32 r3; } ShopCheckResult;

#define SHOP_TABLE_STRIDE   0x10
#define SHOP_ENTRY_SIZE     8
#define SHOP_KIND_UNIQUE    1
#define SHOP_PRICE_SENTINEL 0x70

static ShopCheckResult check_shop_entry(u32 shop_index, u32 toolkit) {
    /* Locate the per-shop descriptor in the global table at dword_8046B98. */
    u32 table_base   = A_dword_8046B98;
    u32 descriptor   = table_base + shop_index * SHOP_TABLE_STRIDE;
    u32 slot_count   = LD32(descriptor + 0xC);
    u32 data_offset  = LD32(descriptor + 8);

    /* Shop data lives at a toolkit-relative pointer + an offset from the descriptor. */
    u32 shop_ptr     = LD32(toolkit + oToolkit_ShopDataPtr);
    u32 cur          = data_offset + shop_ptr;
    u32 counter      = 0;
    u32 flag;

    /* Do-while: the body always executes at least once (matches the assembly
     * entering loc_8048C3A before the first cmp/blt). */
    for (;;) {
        flag = LD8(cur + 1);

        if (flag != 0) {
            /* Query whether a PET-navi is currently active. */
            u32 navi = call_GetCurPETNavi_sub_8048C24().r0;

            if (navi == 0) {
                /* No navi: this is an invalid offering -> return 1. */
                ShopCheckResult res = { 1, cur, flag };
                return res;
            }

            if (LD8(cur) != SHOP_KIND_UNIQUE) {
                /* Not a "unique" item -> return 1 (unexpected kind). */
                ShopCheckResult res = { 1, cur, flag };
                return res;
            }

            if (LD16(cur + 2) == SHOP_PRICE_SENTINEL) {
                /* Price is the special sentinel -> skip this slot, advance. */
            } else {
                /* Real price on a unique item with navi active -> return 1. */
                ShopCheckResult res = { 1, cur, flag };
                return res;
            }
        }

        cur     += SHOP_ENTRY_SIZE;
        counter += 1;

        if ((s32)counter >= (s32)slot_count)
            break;
    }

    /* All slots examined without finding a problem. */
    ShopCheckResult res = { 0, cur, flag };
    return res;
}

Next sub_8048C24(void) {
    ShopCheckResult res = check_shop_entry(R0, R10);
    R0 = res.r0;
    R2 = res.r2;
    R3 = res.r3;
    RETURN_TO_CALLER();
}

/* ── shop_resetStockTable_8048C68 ── */
/* shop_resetStockTable_8048C68:
 * (Re)initialize the shop data buffer owned by the toolkit: clear all 0xE80
 * bytes, then copy the default stock table from ROM over it. The table is a
 * run of 8-byte entries; an entry whose second byte is 0 is an empty slot and
 * is left zeroed instead of copied.
 *
 * inputs:  r2 r3 r10 (toolkit) lr
 * outputs: r2 (leftover byte count from the copy loop, negative on exit)
 * clobbers: r0 r1 r3 lr, flags
 */
Next shop_resetStockTable_8048C68(void) {
    u32 shopData = LD32(R10 + oToolkit_ShopDataPtr);

    R0 = shopData;
    R1 = 0xE80;
    GUEST_CALL(ZeroFillByWord);

    u32 src = A_byte_8047DA0;
    u32 dest = shopData;
    u32 remaining = 0xE80;

    for (;;) {
        remaining -= 8;
        if ((s32)remaining < 0)
            break;

        if (LD8(src + 1) != 0) {
            R0 = src;
            R1 = dest;
            R2 = 8;
            GUEST_CALL(CopyBytes);
        }

        src += 8;
        dest += 8;
    }

    R2 = remaining;
    RETURN_TO_CALLER();
}

/* ── shop_copyItemTableToShopData_8048C98 ── */
/* shop_copyItemTableToShopData_8048C98: copy the shop item table (byte_8047DA0) into the current
 * shop's data buffer (toolkit->shopDataPtr). Each entry is 8 bytes:
 *   +0: item id (byte)   +2: price (halfword)
 *   +4: extra (byte)     +6: count/flags (halfword)
 * Entries whose byte at +1 is zero are empty slots and are skipped, but the
 * destination still advances so slots line up. The table is 0xE80 bytes
 * (464 entries) total. Leaves R2 = remaining byte count after the final
 * iteration (0xFFFFFFF8 when done) and R3 = the last value loaded (on a
 * skipped entry that is the empty-slot check byte itself), as the original
 * does. */
Next shop_copyItemTableToShopData_8048C98(void) {
    u32 src = A_byte_8047DA0;
    u32 dst = LD32(R10 + oToolkit_ShopDataPtr);
    u32 bytesLeft = 0xE80;
    u32 lastLoaded = 0;

    for (;;) {
        bytesLeft -= 8;
        if ((s32)bytesLeft < 0)
            break;

        lastLoaded = LD8(src + 1);    /* empty-slot check byte */
        if (lastLoaded != 0) {
            lastLoaded = LD8(src);        /* item id */
            ST8(dst, lastLoaded);
            lastLoaded = LD16(src + 2);   /* price */
            ST16(dst + 2, lastLoaded);
            lastLoaded = LD8(src + 4);    /* extra byte */
            ST8(dst + 4, lastLoaded);
            lastLoaded = LD16(src + 6);   /* count/flags */
            ST16(dst + 6, lastLoaded);
        }

        src += 8;
        dst += 8;
    }

    R2 = bytesLeft;
    R3 = lastLoaded;
    RETURN_TO_CALLER();
}

/* ── initSaveStateBlock_8048F9C ── */
/* initSaveStateBlock_8048F9C(r0 = save_arg, r10 = toolkit):
 *   Zero-fill the save state block at eS200A290 (0x1B0 bytes),
 *   then write a small header and set *toolkit->main_jumptable_index_ptr
 *   to 0x30. Preserves r2..r7; leaves r0=0xA, r1=*main_jumptable_index_ptr. */
Next initSaveStateBlock_8048F9C(void) {
    u32 save_arg = R0;
    u32 state = A_eS200A290;

    /* ZeroFillByWord(state, 0x1B0) */
    R0 = state;
    R1 = 0x1B0;
    GUEST_CALL(ZeroFillByWord);

    /* *toolkit->main_jumptable_index_ptr = 0x30 */
    R1 = LD32(R10 + oToolkit_MainJumptableIndexPtr);
    R0 = 0x30;
    ST8(R1, R0);

    /* header fields */
    ST16(state + 0x22, save_arg);
    ST16(state + 0x24, 8);
    R0 = 0xA;
    SET_FLAGS_NZ(R0);
    ST16(state + 0x26, R0);

    /* r4 and r5 are preserved across this function (push/pop) */
    RETURN_TO_CALLER();
}

/* ── cb_8048FD4 ── */
/* cb_8048FD4: subsystem tick callback.
 * Reads the state byte at eS200A290 (0/4/8/12 selects one of four
 * subsystem handlers from a jump table), calls the selected handler,
 * then checks whether the subsystem has finished; if so it fires
 * event 0xDA with argument 2 via battleObject_setupCustomChipCursorState_803C59C.
 *
 * R4–R7 are preserved across this function (the original uses
 * push/pop {r4-r7}); R5 is temporarily set to the state pointer for
 * the handler's use but must be restored before returning. */
Next cb_8048FD4(void) {
    u32 statePtr = A_eS200A290;
    u8 offset = LD8(statePtr);

    /* Handlers index [R5] for state; temporarily set R5, save/restore. */
    u32 savedR5 = R5;
    R1 = offset;
    R5 = statePtr;

    switch (offset) {
    case 0:  GUEST_CALL(sub_804900C); break;
    case 4:  GUEST_CALL(sub_8049058); break;
    case 8:  GUEST_CALL(sub_8049080); break;
    case 12: GUEST_CALL(sub_804971C); break;
    }

    R5 = savedR5;

    /* If the subsystem is still in use (non-zero), skip the completion event. */
    GUEST_CALL(IsCurSubsystemInUse);
    if (!ZERO_FLAG()) {
        RETURN_TO_CALLER();
    }

    /* Subsystem finished – signal completion. */
    R0 = 0xDA;
    R1 = 2;
    GUEST_CALL(battleObject_setupCustomChipCursorState_803C59C);
    RETURN_TO_CALLER();
}

/* ── sub_804900C ── */
/* sub_804900C: scene shutdown for the area-transition/menu-exit path.
 *
 * Tears down the chatbox, blanks the LCD through the render-info layer,
 * releases all graphics state, then runs the two sub_8049* cleanup steps.
 * Afterwards it writes the next scene state into the struct at r5:
 *   - state 0xC (+0x01 = 0) if sub_8049D42 returned zero, or if the
 *     halfword at [r5+0x22] is nonzero (something still pending),
 *   - state 0x8 (+0x01 = 0) otherwise.
 *
 * Flags/registers handed to the caller: c and v as left by sub_8049CBC,
 * r3 as left by sub_8049D42. r4-r7 are preserved. */
Next sub_804900C(void) {
    /* Close any open chatbox/textarea. */
    GUEST_CALL(chatbox_8040818);

    /* Blank the display: LCD control value 0x40, then refresh the
     * render-info layers. */
    R0 = 0x40;
    GUEST_CALL(SetRenderInfoLCDControl);
    GUEST_CALL(renderInfo_8001788);
    GUEST_CALL(renderInfo_80017A0);

    /* Release palette/DMA slot 0x11 and reset all graphics state. */
    R0 = 0x11;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);
    GUEST_CALL(resetAllGraphicsState_8005F40);
    GUEST_CALL(sub_803BEC0);

    /* Final cleanup passes; sub_8049CBC leaves c/v for our caller,
     * sub_8049D42 leaves Z (and r3) which we branch on here. */
    GUEST_CALL(sub_8049CBC);
    GUEST_CALL(sub_8049D42);

    if (ZERO_FLAG() || LD16(R5 + 0x22) != 0) {
        /* Transition requested, or work still pending: state 0xC. */
        ST8(R5, 0xC);
    } else {
        /* Nothing left to do: state 0x8. */
        ST8(R5, 8);
    }
    ST8(R5 + 1, 0);

    RETURN_TO_CALLER();
}

/* ── sub_804971C ── */
/* sub_804971C: chatbox state dispatch.
 *
 * r5 points to the chatbox work area; the byte at offset 1 is the current
 * state, used as a raw byte offset into the thumb handler jump table at
 * off_8049730. The original loads the selected handler address and does
 * `mov lr, pc; bx r0`, so the callee is entered with r0 = its own address
 * and r1 = the state byte, while r2 and r4-r12 pass through untouched.
 * The push/pop of r4-r7 need not be reproduced: the callee preserves them
 * per the ABI, so r4-r7 simply flow through to the caller, and the
 * handler's results (r3, r8-r12, c, v) are what this function leaves
 * behind in the guest registers for its caller.
 */

/* Call the selected chatbox state handler through its code address.
 * Register form: indirect call through a jump-table address (the original's
 * `mov lr, pc; bx r0`), which no generated wrapper covers. The callee is
 * entered with r0 = handler address and r1 = state byte, matching the
 * original dispatch; its register and flag results stay in the guest state. */
static void call_chatbox_state_handler(u32 handler, u32 state) {
    R0 = handler;
    R1 = state;
    GUEST_CALL_ADDR(handler);
}

/* Read the chatbox state and tail-jump to that state's handler. */
static void chatbox_dispatch(u32 ctx) {
    u32 state   = LD8(ctx + 1);
    u32 handler = LD32(A_off_8049730 + state);
    call_chatbox_state_handler(handler, state);
}

Next sub_804971C(void) {
    chatbox_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8049770 ── */
/* sub_8049770: scene/transition entry point.
 * Called as part of a screen/state change. Performs LCD/replay state setup,
 * configures the screen fade, clears several state bytes in the r5 struct,
 * runs the standard sub_803A740/sub_803A752/sub_803A764 object-link trio,
 * then enables chatbox displays 0 and 1 and writes state byte 4 to the
 * struct at [r5 + 1]. */
Next sub_8049770(void) {
    /* SetRenderInfoLCDControl(0x1F40) */
    R0 = 0x1F40;
    GUEST_CALL(SetRenderInfoLCDControl);

    /* Run the render-info refresh pair. */
    GUEST_CALL(renderInfo_8001788);
    GUEST_CALL(renderInfo_80017A0);

    /* toolkit_applyOverlayPalette_80015FC(0x13) */
    R0 = 0x13;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);

    GUEST_CALL(battle_resetEStructRecords_8046664);
    GUEST_CALL(copyData_8049A18);

    /* SetScreenFade(8, 0x10) -- fade-in: 8 frames, 0x10 step. */
    R0 = 8;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    /* Zero out a handful of state bytes in the struct pointed to by r5. */
    R0 = 0;
    ST8(R5 + 0x08, R0);
    ST8(R5 + 0x12, R0);
    ST8(R5 + 0x1C, R0);
    ST8(R5 + 0x1D, R0);
    ST8(R5 + 0x13, R0);

    GUEST_CALL(sub_803BEC0);
    GUEST_CALL(sub_8049CBC);
    GUEST_CALL(sub_803A740);
    GUEST_CALL(sub_803A752);
    GUEST_CALL(sub_803A764);

    /* sub_803BE30(0) then sub_803BE30(1): enable chatbox layers 0 and 1. */
    R0 = 0;
    GUEST_CALL(sub_803BE30);
    R0 = 1;
    GUEST_CALL(sub_803BE30);

    GUEST_CALL(chatbox_8040818);

    /* Write the state value (4) to byte +1 of the r5 struct.
     * The MOV sets nz; preserve it per the contract. */
    R0 = 4;
    SET_FLAGS_NZ((u32)R0);
    ST8(R5 + 0x01, R0);

    RETURN_TO_CALLER();
}

/* ── sub_80497D0 ── */
/* sub_80497D0: called while a screen fade is running. If a fade is still in
 * progress, run the fade-related updates (sub_803BB2C with id 0x14, then
 * sub_803BB7C with id 0xF) and write 8 to byte [r5+1] (the caller's fade/state
 * counter). Afterwards, always call battle_moduleInit_803A820 (per-frame fade processor).
 * Flags and r3/r8-r12 on return come straight from that final call. */
Next sub_80497D0(void) {
    /* r5 points to the caller's state struct; the callees may clobber R5
     * (the original saves it on the stack), so keep the pointer in a local. */
    u32 state = R5;

    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG()) {
        R0 = 0x14;
        GUEST_CALL(sub_803BB2C);
        R0 = 0xF;
        GUEST_CALL(sub_803BB7C);
        ST8(state + 1, 8);
    }

    GUEST_CALL(battle_moduleInit_803A820);
    RETURN_TO_CALLER();
}

/* ── sub_80497EE ── */
/* sub_80497EE (r5 = state object, r10 = caller context):
 * Checks flag 0x80 of eFlags2009F38. If it is clear, asks sub_803BB80
 * (entered with r0 still holding the flag-test result); if that reports
 * non-zero, the object's state byte at [r5+1] is set to 0xC. Finally
 * battle_moduleInit_803A820 always runs, receiving in r0 the value the original carries
 * along: 0x80 when nothing was written, 0xC after the state write.
 * r3, r8-r12 and the c/v flags are left exactly as battle_moduleInit_803A820 left them. */
Next sub_80497EE(void) {
    u32 r0Value = 0x80;         /* what the original keeps in r0 */

    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);

    if (ZERO_FLAG()) {          /* flag 0x80 not set -> check further */
        GUEST_CALL(sub_803BB80);
        if (!ZERO_FLAG()) {     /* sub_803BB80 returned non-zero */
            r0Value = 0xC;
            ST8(R5 + 1, 0xC);
        }
    }

    R0 = r0Value;
    GUEST_CALL(battle_moduleInit_803A820);
    RETURN_TO_CALLER();
}

/* ── sub_8049808 ── */
/* sub_8049808(r2, r4, r5, r10, lr): jump-table dispatcher.
 *   Calls sub_803AFC2(0); if it returns nonzero, the result indexes a small Thumb
 *   function-pointer table (dword_804982C: entry 1 = handleExitSubmenu_803AA24, 2 = battleObj_dispatchSubstate_803AA40,
 *   3 = sub_803ABF4, 4 = sub_803AB6C, 5 = sub_803AB94) which is tail-called with
 *   LR = 0x0804981C (the fallthrough into the default cleanup). On zero it skips
 *   the table, runs sub_803A7B8, then sub_803BC7C(0), and returns to the caller. */
Next sub_8049808(void) {
    R0 = 0;
    GUEST_CALL(sub_803AFC2);
    if (!ZERO_FLAG()) {
        u32 idx = R0;
        /* `lsl r0, r0, #2` -> table offset. Replicated for flag-setting only;
         * the switch handles the actual dispatch. */
        SET_FLAGS_NZ(idx << 2);
        switch (idx) {
            case 1: GUEST_CALL(handleExitSubmenu_803AA24); break;
            case 2: GUEST_CALL(battleObj_dispatchSubstate_803AA40); break;
            case 3: GUEST_CALL(sub_803ABF4); break;
            case 4: GUEST_CALL(sub_803AB6C); break;
            case 5: GUEST_CALL(sub_803AB94); break;
        }
    }
    /* Fallthrough / zero path: default cleanup. */
    GUEST_CALL(sub_803A7B8);
    R0 = 0;
    GUEST_CALL(sub_803BC7C);
    RETURN_TO_CALLER();
}

/* ── chatbox_inputHandler_8049888 ── */
/* chatbox_inputHandler_8049888: chatbox input handler dispatcher.
 * Inputs:  r5 = chatbox state struct, r10, lr
 * Outputs: r3, r8, r9, r10, r11, r12 + flags c, v
 * Writes one byte into chatbox state based on a flag check and the current
 * chatbox sub-state, then ticks the chatbox state machine. */
Next chatbox_inputHandler_8049888(void) {
    u32 r0;

    /* Probe flag 0x80; if it isn't set, the state byte stays untouched. */
    r0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (ZERO_FLAG()) {
        /* Flag 0x80 unset: pick the next state from the chatbox sub-state. */
        GUEST_CALL(chatbox_GetState_8045F4C);
        if (R0 != 0) {
            ST8(R5 + 1, 0x0C);
        } else {
            ST8(R5 + 1, 0x20);
        }
    }

    /* Run the chatbox state machine tick. */
    GUEST_CALL(battle_moduleInit_803A820);

    RETURN_TO_CALLER();
}

/* ── sub_80498D4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r8, r9, r10, r11, r12; bool z, n, c, v; } call_battle_moduleInit_803A820_result;
/* register form: generated wrapper: battle_moduleInit_803A820 has no typed C interface yet */
static inline call_battle_moduleInit_803A820_result call_battle_moduleInit_803A820(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_moduleInit_803A820);
    return (call_battle_moduleInit_803A820_result){ R0, R1, R2, R3, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_80498D4: reset a battle work structure before the battle module takes it over.
 * Clears the byte at +8, stores the init-state marker 0x24 at +1, then hands the
 * structure to battle_moduleInit_803A820, which fills in the rest.
 * Inputs:  r5 = structure pointer (r10 is passed through untouched).
 * Outputs: whatever battle_moduleInit_803A820 leaves behind: r3, r8-r12, C, V. */

/* Body of sub_80498D4. Returns the module initializer's register results and flags. */
static call_battle_moduleInit_803A820_result init_battle_work_area(u32 work)
{
    ST8(work + 8, 0);      /* clear the state byte at +8 */
    ST8(work + 1, 0x24);   /* store the 0x24 init marker at +1 */
    return call_battle_moduleInit_803A820(work);
}

Next sub_80498D4(void) {
    call_battle_moduleInit_803A820_result res = init_battle_work_area(R5);
    R3 = res.r3, R8 = res.r8, R9 = res.r9, R10 = res.r10, R11 = res.r11, R12 = res.r12;

    /* Reproduce the initializer's C/V flags with one ADDS: pick operands whose
     * carry and signed overflow match the pair (res.c, res.v):
     *   c=0,v=0 -> 0x00000000 + 0x00000000   (C=0, V=0)
     *   c=1,v=0 -> 0x7FFFFFFF + 0x80000001   (C=1, V=0)
     *   c=0,v=1 -> 0x7FFFFFFF + 0x7FFFFFFF   (C=0, V=1)
     *   c=1,v=1 -> 0xBFFFFFFF + 0xBFFFFFFF   (C=1, V=1) */
    u32 c = res.c, v = res.v;
    u32 addend_a = 0x7FFFFFFF * (c + v) - 0x3FFFFFFF * (c * v);
    u32 addend_b = 0x80000001 * c + 0x7FFFFFFF * v - 0x40000001 * (c * v);
    SET_FLAGS_ADD(addend_a, addend_b);
    RETURN_TO_CALLER();
}

/* ── sub_80498E4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool z, n; } call_chatbox_mask_eFlags2009F38_result;
/* register form: generated wrapper: chatbox_mask_eFlags2009F38 has no typed C interface yet */
static inline call_chatbox_mask_eFlags2009F38_result call_chatbox_mask_eFlags2009F38(u32 r0) {
    R0 = r0;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return (call_chatbox_mask_eFlags2009F38_result){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r8, r9, r10, r11, r12; bool z, n, c, v; } call_battle_moduleInit_803A820_result_sub_80498E4;
/* register form: generated wrapper: battle_moduleInit_803A820 has no typed C interface yet */
static inline call_battle_moduleInit_803A820_result_sub_80498E4 call_battle_moduleInit_803A820_sub_80498E4(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_moduleInit_803A820);
    return (call_battle_moduleInit_803A820_result_sub_80498E4){ R0, R1, R2, R3, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_80498E4: battle module init hook.
 *
 * If bit 0x80 of the chatbox eFlags word (eFlags2009F38) is clear, mark the
 * module struct at [r5+1] with the "waiting for chatbox" byte 0x28, then run
 * the shared battle module initializer, which produces all the outputs
 * (r3, r8-r12, c, v) that callers of this routine see. */
static void init_battle_module(u32 modulePtr) {
    /* chatbox_mask_eFlags2009F38: Z set when the tested bit is clear */
    call_chatbox_mask_eFlags2009F38_result mask =
        call_chatbox_mask_eFlags2009F38(0x80);

    if (mask.z) {                       /* bit 0x80 not set: store the marker */
        ST8(modulePtr + 1, 0x28);
    }

    /* Shared initializer; leaves r3, r8-r12 and c/v as its callers expect. */
    call_battle_moduleInit_803A820_sub_80498E4(modulePtr);
}

Next sub_80498E4(void) {
    init_battle_module(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80498F8 ── */
/* sub_80498F8: step the chip-select state machine once.
 *
 * sub_803AFC2 reports the active chip-select state in R0. State 0 means no
 * submenu is active (its zero flag is set), so only the default processing
 * runs: sub_803A7EC followed by sub_803BC7C(0). Any other state selects a
 * handler from the ROM table at dword_804991C; the handler is entered with
 * R0 = its own table entry (Thumb address) and R1 = table base, and on
 * return the same default processing runs. */
Next sub_80498F8(void) {
    R0 = 0;
    GUEST_CALL(sub_803AFC2);

    if (!ZERO_FLAG()) {
        u32 state = R0;
        u32 handler = LD32(A_dword_804991C + state * 4);

        R1 = A_dword_804991C;   /* table base, as the original passes it */
        R0 = handler;           /* the callee sees its own table entry in R0 */

        switch (state) {
        case 1: GUEST_CALL(sub_803AD04);                    break;
        case 2: GUEST_CALL(chipSelect_dispatchState_803AD20); break;
        case 3: GUEST_CALL(sub_803AED8);                    break;
        case 4: GUEST_CALL(sub_803AE50);                    break;
        case 5: GUEST_CALL(sub_803AE78);                    break;
        }
    }

    /* default step, on both paths */
    GUEST_CALL(sub_803A7EC);
    R0 = 0;
    GUEST_CALL(sub_803BC7C);

    RETURN_TO_CALLER();
}

/* ── chatbox_stepState_8049984 ── */
/* chatbox_stepState_8049984:
 *   inputs:  r5 r10 lr
 *   outputs: c v r3 r8 r9 r10 r11 r12
 *   clobbers (unused by callers): nz r0 r1 r2 lr
 *   preserved: r4 r5 r6 r7 r8 r9 r12
 *
 * One step of the chatbox state machine. If the chatbox's eFlags2009F38
 * already has bit 0x80 set, fall through to the epilogue call. Otherwise
 * query the chatbox state: state != 0 advances to phase 0x28, while state == 0
 * kicks off a screen fade (0xc / 0x10) and advances to phase 0x3c. R5 points
 * to a control byte whose byte [1] is the next-phase slot. */
Next chatbox_stepState_8049984(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (ZERO_FLAG()) {
        GUEST_CALL(chatbox_GetState_8045F4C);
        if (R0 != 0) {
            ST8(R5 + 1, 0x28);
        } else {
            R0 = 0xC;
            R1 = 0x10;
            GUEST_CALL(SetScreenFade);
            ST8(R5 + 1, 0x3C);
        }
    }
    GUEST_CALL(battle_moduleInit_803A820);
    RETURN_TO_CALLER();
}

/* ── chatbox_closeFadeHandler_80499CE ── */
/* chatbox_closeFadeHandler_80499CE: 080499CE-08049A18 THUMB
 * Inputs:  r5 r6 r7 r8 r9 r10 r11 r12 lr
 * Outputs: c v r3 r8 r9 r10 r11 r12
 * Clobbers: nz r0 r1 r2 lr
 * Preserved: r4 r5 r6 r7
 *
 * The original push {r4-r7,lr} / pop {r4-r7,pc} is reproduced because
 * the callee in the else-path (battle_moduleInit_803A820) writes 0x20093b0 to the
 * saved r7 slot on the stack, and the final pop surfaces that value in r7.
 */
Next chatbox_closeFadeHandler_80499CE(void) {
    /* push {r4-r7,lr} */
    u32 sp = SP - 20;
    ST32(sp,     R4);
    ST32(sp + 4, R5);
    ST32(sp + 8, R6);
    ST32(sp + 12, R7);
    ST32(sp + 16, LR);
    SP = sp;

    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG()) {
        /* Screen fade not active: full reset path. */
        u32 fade_state = 0;
        GUEST_CALL(sub_8049D42);
        if (!ZERO_FLAG()) {
            fade_state = 1;
        }
        ST8(R5 + 4, fade_state);
        GUEST_CALL(sub_8049D70);
        GUEST_CALL(sub_8049D4A);
        GUEST_CALL(sub_8049D3A);
        GUEST_CALL(sub_803F79E);
        GUEST_CALL(zeroFillVRAM);
        GUEST_CALL(ZeroFillGFX30025c0);
        GUEST_CALL(copyMemory_8001850);
        R0 = 0x40;
        GUEST_CALL(SetRenderInfoLCDControl);
        GUEST_CALL(chatbox_8040818);
        GUEST_CALL(sub_811F758);
    } else {
        /* Screen fade still active: bail out via the alternate path. */
        GUEST_CALL(battle_moduleInit_803A820);
    }

    /* pop {r4-r7,pc} */
    u32 a = SP;
    R4 = LD32(a);
    R5 = LD32(a + 4);
    R6 = LD32(a + 8);
    R7 = LD32(a + 12);
    u32 ret = LD32(a + 16);
    SP = a + 20;
    LR = ret;
    RETURN_TO_CALLER();
}

/* ── copyData_8049A18 ── */
/* copyData_8049A18: clear VRAM, decompress init data, shrink the screen viewport,
 * and trigger a screen transition. r10 holds a Toolkit pointer whose render info
 * struct (offsets 0x14, 0x16, 0x18, 0x1A) holds the current viewport bounds.
 *
 *   inputs:  r10 lr
 *   outputs: r5
 *   clobbers: nz c v r0 r1 r2 r3 lr
 *   preserved: r4 r6 r7 */
#define A_initRefs_8049A54 0x08049A54u

Next copyData_8049A18(void) {
    /* Clear video RAM and the secondary GFX buffer, then stream any packed
     * intro graphics still pending in the copy queue. */
    GUEST_CALL(zeroFillVRAM);
    GUEST_CALL(ZeroFillGFX30025c0);
    GUEST_CALL(copyMemory_8001850);

    /* Walk the initRefs table: each entry is (srcPtr, dstPtr, size) and either
     * decompresses from ROM into RAM/VRAM or copies a raw block. */
    {
        u32 *initRefs = (u32 *)A_initRefs_8049A54;
        R0 = (u32)initRefs;
        GUEST_CALL(decompAndCopyData);
    }

    /* Shrink the current render viewport inward by a tile on each side so the
     * next scene can draw a border. The offsets hold: 0x14 = top, 0x16 = bottom,
     * 0x18 = left, 0x1A = right (all in pixels). */
    {
        u32 renderInfoPtr = LD32(R10 + oToolkit_RenderInfoPtr);
        ST16(renderInfoPtr + 0x16, (u16)(LD16(renderInfoPtr + 0x16) - 4)); /* bottom */
        ST16(renderInfoPtr + 0x14, (u16)(LD16(renderInfoPtr + 0x14) - 2)); /* top    */
        ST16(renderInfoPtr + 0x1A, (u16)(LD16(renderInfoPtr + 0x1A) - 2)); /* right  */
        ST16(renderInfoPtr + 0x18, (u16)(LD16(renderInfoPtr + 0x18) - 4)); /* left   */
    }

    /* Kick off transition effect #3 with the new viewport in place. */
    R0 = 3;
    GUEST_CALL(sub_803B91C);

    RETURN_TO_CALLER();
}

/* ── sub_8049CBC ── */
/* sub_8049CBC: Copy two 0x20-byte blocks from word_2001800 into the caller's
 * struct (r5), then fill in two small fields at offsets 0x10 and 0x11 by
 * calling sub_803C132(0x10, 0x30) and sub_803C132(0x11, 0x90).
 *
 * Inputs:  r5 = destination struct pointer
 * Outputs: r1, r2, r3, r5 unchanged; r0 garbage
 */
Next sub_8049CBC(void) {
    /* base = word_2001800 */
    u32 base = A_word_2001800;

    /* CopyBytes(base + 0x08, r5 + 0x30, 0x20) */
    R0 = base + 0x08;
    R1 = R5 + 0x30;
    R2 = 0x20;
    GUEST_CALL(CopyBytes);

    /* CopyBytes(base + 0x28, r5 + 0x90, 0x20) */
    R0 = base + 0x28;
    R1 = R5 + 0x90;
    R2 = 0x20;
    GUEST_CALL(CopyBytes);

    /* r5[0x10] = sub_803C132(0x10, 0x30) */
    R0 = 0x10;
    R1 = 0x30;
    GUEST_CALL(sub_803C132);
    ST8(R5 + 0x10, R0);

    /* r5[0x11] = sub_803C132(0x11, 0x90) */
    R0 = 0x11;
    R1 = 0x90;
    GUEST_CALL(sub_803C132);
    ST8(R5 + 0x11, R0);

    RETURN_TO_CALLER();
}

/* ── chatbox_initTextLineBuffer_8049CF8 ── */
/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_words(u32 dst, u32 byte_count) {
    R0 = dst;
    R1 = byte_count;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls HalfwordFill outside this file, which has no typed C interface yet */
static void fill_halfwords(u32 dst, u32 count, u32 value) {
    R0 = dst;
    R1 = count;
    R2 = value;
    GUEST_CALL(HalfwordFill);
}

/* Register state the original leaves behind: r2/r3 still hold the fill value
 * setup from the last HalfwordFill call (HalfwordFill preserves them), and
 * r4-r7 are untouched. */
typedef struct {
    u32 r2, r3, r4, r5, r6, r7;
} ExitRegs;

/* Clear the 0x48-byte work buffer at word_2001800, then lay down blank
 * background tiles (0xE5E5 halfwords) in its two 0x20-entry lines and set
 * each line's trailing attribute byte to 0xE6. */
static ExitRegs init_work_buffer(u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    u32 work = A_word_2001800;

    zero_fill_words(work, 0x48);

    fill_halfwords(work + 8, 0x20, 0xE5E5);
    ST8(work + 0x18, 0xE6);   /* byte_2001818 */

    fill_halfwords(work + 0x28, 0x20, 0xE5E5);
    ST8(work + 0x3C, 0xE6);   /* byte_200183C */

    /* The original still carries the last fill's argument values in r2/r3. */
    {
        ExitRegs out = { 0xE5E5, 0xE5 << 8, r4, r5, r6, r7 };
        return out;
    }
}

/* chatbox_initTextLineBuffer_8049CF8: r2/r3 exit as the last fill setup left them; r4-r7 preserved. */
Next chatbox_initTextLineBuffer_8049CF8(void) {
    ExitRegs out = init_work_buffer(R2, R3, R4, R5, R6, R7);
    R2 = out.r2;
    R3 = out.r3;
    R4 = out.r4;
    R5 = out.r5;
    R6 = out.r6;
    R7 = out.r7;
    RETURN_TO_CALLER();
}

/* ── sub_8049D3A ── */
Next sub_8049D3A(void) {
    /* Write 1 to byte_2001802 (the "enable" flag at offset 2 of the
     * word_2001800 block). Returns the written value in r1, per contract. */
    ST8(A_word_2001800 + 2, 1);
    R0 = A_word_2001800;
    R1 = 1;
    RETURN_TO_CALLER();
}

/* ── sub_8049D42 ── */
Next sub_8049D42(void) {
    /* Read the per-battle state byte at 0x2001802 and return it (preserving NZ). */
    u32 state = LD8(A_word_2001800 + (A_byte_2001802 - 0x2001800));
    SET_FLAGS_NZ(state);
    R0 = state;
    RETURN_TO_CALLER();
}

/* ── sub_8049D4A ── */
/* sub_8049D4A: Copy two 0x20-byte blocks from eS200A290 into word_2001800.
 *   inputs:  lr
 *   outputs: r2 r3 r4 r5 r6 r7
 *   clobbers (unused by callers): nz c v r0 r1 lr
 *   preserved: r4 r5 r6 r7 */
Next sub_8049D4A(void) {
    /* First block: A_eS200A290 + 0x70  ->  A_word_2001800 + 0x08  (0x20 bytes) */
    R0 = A_eS200A290 + 0x70;
    R1 = A_word_2001800 + 0x08;
    R2 = 0x20;
    GUEST_CALL(CopyBytes);

    /* Second block: A_eS200A290 + 0xD0  ->  A_word_2001800 + 0x28  (0x20 bytes) */
    R0 = A_eS200A290 + 0xD0;
    R1 = A_word_2001800 + 0x28;
    R2 = 0x20;
    GUEST_CALL(CopyBytes);

    RETURN_TO_CALLER();
}

/* ── sub_8049D70 ── */
/* sub_8049D70: store the next RNG value into the global RNG seed (word_2001800).
 * inputs:  lr
 * outputs: r4 r5 r6 r7
 * clobbers: nz c v r0 r1 r2 r3 lr */
Next sub_8049D70(void) {
    /* Draw a fresh random u32 from the RNG and stash the low 16 bits as the
     * next RNG seed used by the rest of the game. */
    GUEST_CALL(GetRNG);
    ST16((u32)A_word_2001800, (u16)R0);
    RETURN_TO_CALLER();
}

/* ── sub_8049DDC ── */
/* sub_8049DDC(r0 = value for the work block's +0x22 field, r10 = toolkit):
 * Zeroes the 0x1B0-byte work block at eS200A290, switches the main jumptable
 * to state 0x4C, and fills in the block's first fields.
 * r0/r1 are scratch; r5 (used here as the block pointer) is preserved for
 * the caller, exactly as the original's push/pop does. */
Next sub_8049DDC(void) {
    u32 fieldValue = R0;

    u32 block = A_eS200A290;
    R0 = block;
    R1 = 0x1B0;
    GUEST_CALL(ZeroFillByWord);

    /* advance the main state machine to state 0x4C */
    ST8(LD32(R10 + oToolkit_MainJumptableIndexPtr), 0x4C);

    /* initialise the first fields of the work block */
    ST16(block + 0x22, fieldValue);
    ST16(block + 0x24, 8);
    ST16(block + 0x26, 1);

    RETURN_TO_CALLER();
}

/* ── cb_8049E04 ── */
/* cb_8049E04: phase-step dispatcher for the E-Reader battle screens.
 *
 * eS200A290 holds the current phase byte. This callback indexes the jump
 * table at off_8049E18 with it and calls the matching handler
 * (chatbox_initNewScreen_8049E28 / sub_8049E5C / sub_8049E84), passing the
 * phase byte in r1 and a pointer to the phase byte in r5. The handler also
 * sees the dispatcher's own table entry in r0, exactly as the original's
 * "ldr r0,[table,phase]; mov lr,pc; bx r0" leaves the register file.
 * r4-r7 are callee-saved and restored by the trailing pop {r4-r7,pc};
 * r8-r12 end as the handler left them. */

/* r8-r12 as the handler leaves them (r4-r7 come back unchanged). */
struct cb_8049E04_tail {
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
};

/* register form: indirect call through the phase jump table. The handler
 * runs on the live guest register file: r0 = its own table entry (thumb bit
 * set), r1 = phase byte, r5 = &phase byte, with r2/r4/r10 and every other
 * register passed through untouched. Afterwards r4-r7 are put back the way
 * the original's pop {r4-r7,pc} does, and r8-r12 are collected as results. */
static struct cb_8049E04_tail call_phase_handler_cb_8049E04(u32 handler, u32 phase,
                                                 u32 r2in, u32 r4in, u32 r5in,
                                                 u32 r6in, u32 r7in, u32 r10in) {
    R0 = handler;
    R1 = phase;
    R2 = r2in;
    R4 = r4in;
    R5 = A_eS200A290;
    R10 = r10in;
    GUEST_CALL_ADDR(handler);

    /* pop {r4-r7,pc}: the saved low registers come back unchanged. */
    R4 = r4in;
    R5 = r5in;
    R6 = r6in;
    R7 = r7in;

    struct cb_8049E04_tail out;
    out.r8 = R8;
    out.r9 = R9;
    out.r10 = R10;
    out.r11 = R11;
    out.r12 = R12;
    return out;
}

/* Typed body: pick the handler for the current phase and run it. */
static struct cb_8049E04_tail cb_8049E04_body(u32 r2in, u32 r4in, u32 r5in,
                                              u32 r6in, u32 r7in, u32 r10in) {
    u32 phase = LD8(A_eS200A290);
    u32 handler = LD32(A_off_8049E18 + phase);
    return call_phase_handler_cb_8049E04(handler, phase, r2in, r4in, r5in, r6in, r7in, r10in);
}

Next cb_8049E04(void) {
    struct cb_8049E04_tail out = cb_8049E04_body(R2, R4, R5, R6, R7, R10);
    R8 = out.r8;
    R9 = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── chatbox_initNewScreen_8049E28 ── */
/* chatbox_initNewScreen_8049E28: chatbox/dialog init for a new screen — clears chatbox state,
 * sets up render info and LCD control, then resets the graphics subsystem
 * and writes the initial chatbox state bytes at the object pointed to by R5.
 *   inputs:  r5 = pointer to chatbox state struct
 *   outputs: -
 *   clobbers: r0 r1 r2 r3 lr (nz c v from callees)
 *   preserved: r4 r6 r7 */
Next chatbox_initNewScreen_8049E28(void) {
    GUEST_CALL(chatbox_8040818);

    R0 = 0x40;
    GUEST_CALL(SetRenderInfoLCDControl);

    GUEST_CALL(renderInfo_8001788);
    GUEST_CALL(renderInfo_80017A0);

    R0 = 0x11;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);

    GUEST_CALL(resetAllGraphicsState_8005F40);
    GUEST_CALL(sub_803BEC0);
    GUEST_CALL(sprite_loadTilesAndPlaySfx_804A14C);

    /* Initialize the chatbox state struct: state = 8 (opening/animating in),
     * sub-state = 0 (frame 0). */
    ST8(R5, 8);
    ST8(R5 + 1, 0);

    RETURN_TO_CALLER();
}

/* ── sub_8049E84 ── */
/* sub_8049E84: dispatch a chatbox/scene/fade state step based on the sub-state
 * byte stored at r5+1. The original is a 20-byte trampoline that pushes
 * {r4-r7,lr}, loads a function pointer from a 9-entry table indexed by that
 * byte, sets lr to the return address, and tail-calls the handler via bx.
 *
 * The two indirections in the original (off_8049E94 -> off_8049E98 -> [r5+1])
 * collapse here to a single load through a pointer symbol. The byte is the
 * only index value observed; the table is small and fixed.
 *
 * Register contract: r4 r5 r6 r7 must come back unchanged (preserved), r8 r9
 * r10 r11 r12 are the outputs the lifter attributes to the callee (so we don't
 * touch them), and r0 r1 r2 r3 lr/nz/c/v are scratch. */
Next sub_8049E84(void) {
    /* Index by the sub-state byte the caller placed at r5+1. */
    u32 sub_state = LD8(R5 + 1);
    /* The original does: ldr r0, =off_8049E94 (= &off_8049E98);
     *                     ldrb r1, [r5,#1];
     *                     ldr r0, [r0, r1]
     * which is LD32(A_off_8049E98 + sub_state). The low bit is the Thumb bit
     * the original `bx r0` requires; we mask it off before calling. */
    u32 target_thumb = LD32(A_off_8049E98 + sub_state);
    u32 target = target_thumb & ~1u;
    /* The callee returns to whichever value we leave in lr, matching the
     * original's tail-call semantics. The callee is responsible for
     * r4..r7 preservation. */
    GUEST_CALL_ADDR(target);
    RETURN_TO_CALLER();
}

/* ── sub_8049EBC ── */
/* sub_8049EBC: scene initializer / fade-in setup.
 * Inputs: r5 = state struct pointer, r10 = preserved across the call, lr = return address.
 * Outputs: r8/r9/r11/r12 untouched (preserved); r10 preserved; ends with state->field_1 = 4. */
Next sub_8049EBC(void) {
    u32 state = R5;

    /* Configure LCD for the upcoming transition. */
    R0 = 0x1F40;
    GUEST_CALL(SetRenderInfoLCDControl);

    GUEST_CALL(renderInfo_8001788);
    GUEST_CALL(renderInfo_80017A0);

    /* 0x13 = palette slot count or bg layer id for the fade prep. */
    R0 = 0x13;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);

    GUEST_CALL(battle_resetEStructRecords_8046664);
    GUEST_CALL(copyData_804A0AC);

    /* SetScreenFade(speed=8, target=0x10) -- a two-tone screen fade in. */
    R0 = 8;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    /* Zero out a batch of state flags / counters. */
    ST8(state + 8, 0);
    ST8(state + 0x1C, 0);
    ST8(state + 0x1D, 0);
    ST8(state + 0x13, 0);
    ST8(state + 0x12, 0);

    GUEST_CALL(sub_803BEC0);
    GUEST_CALL(sprite_loadTilesAndPlaySfx_804A14C);
    GUEST_CALL(sub_803A740);
    GUEST_CALL(sub_803A752);
    GUEST_CALL(sub_8049F20);

    /* Run the per-frame setup hooks twice (idx 0 then idx 1). */
    R0 = 0;
    GUEST_CALL(sub_803BE30);
    R0 = 1;
    GUEST_CALL(sub_803BE30);

    GUEST_CALL(chatbox_8040818);

    /* Mark the next scene state (0x04 = "ready / transitioned in"). */
    ST8(state + 1, 4);

    RETURN_TO_CALLER();
}

/* ── sub_8049F20 ── */
/* sub_8049F20(r10 = ?, lr):
 * Call sub_803B674 with the args 0x29, 0x2A and 0x2B (then terminate).
 * Preserved: r4 r5 r6 r7. Output: r5 (sub_803B674's R5 is observed by callers). */
Next sub_8049F20(void) {
    for (u32 arg = 0x29; (s32)arg <= 0x2C; arg++) {
        R0 = arg;
        GUEST_CALL(sub_803B674);
    }
    RETURN_TO_CALLER();
}

/* ── screenFadeUpdateTick_8049F32 ── */
/* screenFadeUpdateTick_8049F32: 08049F32-08049F50 THUMB, bn6f asm03_2.s:6567
 *
 * Screen-fade update tick. If a screen fade is currently active, advance
 * its animation (timer/draw) and update a state byte held in r5+1; then
 * fall through to sub_8049F70 to continue with the next stage of update.
 *
 *   inputs:  r4 r5 r10 lr
 *   outputs: r8 r9 r10 r11 r12
 *   clobbers: nz c v r0 r1 r2 r3 lr
 *   preserved: r4 r5 r6 r7 r8 r9 r12
 */
Next screenFadeUpdateTick_8049F32(void) {
    /* IsScreenFadeActive() -> zf */
    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG()) {
        /* Fade is mid-animation: nudge the fade one step further. */
        R0 = 0x14;
        GUEST_CALL(sub_803BB2C);

        R0 = 0x0F;
        GUEST_CALL(sub_803BB7C);

        /* Stamp "8" into the second byte of r5's struct as the new state. */
        ST8(R5 + 1, 8);
    }

    /* Continue to the next screen/fade update routine. */
    GUEST_CALL(sub_8049F70);
    RETURN_TO_CALLER();
}

/* ── sub_8049F50 ── */
Next sub_8049F50(void) {
    GUEST_CALL(sub_803BA64);
    R0 = 0;
    GUEST_CALL(sub_803BAE2);
    R0 = 0x0A;
    GUEST_CALL(sub_803B522);
    R0 = 0x28;
    GUEST_CALL(sub_803B674);
    R0 = 1;
    GUEST_CALL(sub_803A8B4);
    RETURN_TO_CALLER();
}

/* ── sub_8049F70 ── */
/* sub_8049F70: initialize something (likely a battle/scene setup).*/
Next sub_8049F70(void) {
    GUEST_CALL(sub_803BA64);

    R0 = 0xA;
    GUEST_CALL(sub_803B522);

    R0 = 0x28;
    GUEST_CALL(sub_803B674);

    R0 = 1;
    GUEST_CALL(sub_803A8B4);

    RETURN_TO_CALLER();
}

/* ── sub_8049F8A ── */
/* sub_8049F8A: chatbox/process-style hook.
 *   inputs:  r5 (some chatbox control struct), r10, lr
 *   outputs: r8, r9, r10, r11, r12
 *   clobbers: r0, r1, r2, r3, nz, c, v, lr
 *   preserved: r4, r5, r6, r7, r8, r9, r12
 *
 * If flag 0x80 is currently masked out by chatbox_mask_eFlags2009F38
 * (result != 0), skip the inner block. Otherwise call sub_803BB80;
 * if it returns non-zero, write 0x0C to r5+1.
 * Then fall through to sub_8049F70. */
Next sub_8049F8A(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (ZERO_FLAG()) {
        GUEST_CALL(sub_803BB80);
        if (!ZERO_FLAG()) {
            ST8(R5 + 1, 0x0C);
        }
    }
    GUEST_CALL(sub_8049F70);
    RETURN_TO_CALLER();
}

/* ── sub_8049FA4 ── */
/* sub_8049FA4: dispatch the current menu/battle substate.
 *
 * sub_803AFC2(1) returns a substate id (0 = none).  For a non-zero id
 * the matching handler from the jump table is run, then the submenu
 * and the battle-object state are refreshed. */
Next sub_8049FA4(void) {
    R0 = 1;
    GUEST_CALL(sub_803AFC2);

    if (R0 != 0) {
        switch (R0) {
        case 1: GUEST_CALL(handleExitSubmenu_803AA24);          break;
        case 2: GUEST_CALL(battleObj_dispatchSubstate_803AA40); break;
        case 3: GUEST_CALL(sub_803ABF4);                        break;
        case 4: GUEST_CALL(sub_803AB6C);                        break;
        case 5: GUEST_CALL(sub_803AB94);                        break;
        }
    }

    GUEST_CALL(sub_8049F50);
    GUEST_CALL(sub_803BC7C);
    RETURN_TO_CALLER();
}

/* ── chatbox_stateStep_804A024 ── */
/* chatbox_stateStep_804A024: Handle a chatbox state transition for the script runner.
 * r5 = state struct pointer; r10 = some context used by sub_8049F70.
 * Drives either a flag-clear path (sets r5[1]=0x0C) or a fade-and-finalize path
 * (SetScreenFade(0x0C, 0x10) then r5[1]=0x20), then runs sub_8049F70. */
Next chatbox_stateStep_804A024(void) {
    /* If chatbox flag 0x80 is set, skip the state-change logic entirely. */
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (!ZERO_FLAG())
        goto done_with_state;

    GUEST_CALL(chatbox_GetState_8045F4C);
    if (R0 != 0) {
        /* Chatbox still busy: signal that and leave. */
        ST8(R5 + 1, 0x0C);
    } else {
        /* Chatbox idle: trigger a screen fade, then mark this state complete. */
        R0 = 0x0C;
        R1 = 0x10;
        GUEST_CALL(SetScreenFade);
        ST8(R5 + 1, 0x20);
    }

done_with_state:
    GUEST_CALL(sub_8049F70);
    RETURN_TO_CALLER();
}

/* ── sceneUpdate_804A078 ── */
/* sceneUpdate_804A078: scene-update hook.
 * inputs:  r5, r10, lr
 * outputs: r8, r9, r10, r11, r12
 * clobbers: r0, r1, r2, r3, lr, nz, c, v
 *
 * If a screen fade is in progress, tear down the current scene's graphical
 * state (clear VRAM, clear GFX buffer, restore tile memory, set LCD mode,
 * reset chatbox, run system init). Otherwise delegate to sub_8049F70. */
Next sceneUpdate_804A078(void) {
    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG()) {
        R0 = LD16(R5 + 0x22);
        GUEST_CALL(sub_804A230);
        GUEST_CALL(zeroFillVRAM);
        GUEST_CALL(ZeroFillGFX30025c0);
        GUEST_CALL(copyMemory_8001850);
        R0 = 0x40;
        GUEST_CALL(SetRenderInfoLCDControl);
        GUEST_CALL(chatbox_8040818);
        GUEST_CALL(sub_8123408);
    } else {
        GUEST_CALL(sub_8049F70);
    }
    RETURN_TO_CALLER();
}

/* ── copyData_804A0AC ── */
/* copyData_804A0AC: reset GFX/VRAM, decompress init data, shrink render info box by 4x2. */
Next copyData_804A0AC(void) {
    GUEST_CALL(zeroFillVRAM);
    GUEST_CALL(ZeroFillGFX30025c0);
    GUEST_CALL(copyMemory_8001850);

    /* Walk the initRefs table and decompress/copy each entry. */
    R0 = (u32)A_byte_804A0E8;
    GUEST_CALL(decompAndCopyData);

    /* Shrink the render info rectangle by (4,2) on each side. */
    u32 renderInfo = LD32(R10 + oToolkit_RenderInfoPtr);

    u16 rightEdge = LD16(renderInfo + 0x16) - 4;
    ST16(renderInfo + 0x16, rightEdge);

    u16 leftEdge = LD16(renderInfo + 0x14) - 2;
    ST16(renderInfo + 0x14, leftEdge);

    u16 bottomEdge = LD16(renderInfo + 0x1a) - 2;
    ST16(renderInfo + 0x1a, bottomEdge);

    u16 topEdge = LD16(renderInfo + 0x18) - 4;
    ST16(renderInfo + 0x18, topEdge);

    R0 = 3;
    GUEST_CALL(sub_803B91C);

    RETURN_TO_CALLER();
}

/* ── sprite_loadTilesAndPlaySfx_804A14C ── */
Next sprite_loadTilesAndPlaySfx_804A14C(void) {
    /* Look up a per-id offset from byte_804A178, copy 0x20 bytes from unk_2001850+offset
     * into the object at R5+0x30, then play SFX 0x10/0x30 and stash the handle at R5+0x10. */
    u32 table = A_unk_2001850;
    u32 offsets = A_byte_804A178;
    u32 sprite_id = LD16(R5 + 0x22);
    u8  offset   = LD8(offsets + sprite_id);
    u8 *src      = (u8 *)(table + offset);
    u8 *dest     = (u8 *)(R5 + 0x30);

    /* CopyBytes(src, dest, n) */
    R0 = (u32)src;
    R1 = (u32)dest;
    R2 = 0x20;
    GUEST_CALL(CopyBytes);

    /* sub_803C132(0x10, 0x30) -> sound handle */
    R0 = 0x10;
    R1 = 0x30;
    GUEST_CALL(sub_803C132);

    ST8(R5 + 0x10, (u8)R0);
    RETURN_TO_CALLER();
}

/* ── textScript_buildFolderNameBuffers_804A17A ── */
/* textScript_buildFolderNameBuffers_804A17A: build the two text-script folder name buffers at unk_2001850.
 *
 * Each buffer is 0x30 bytes (0x20 halfwords of 0xE5E5, then the folder name in
 * wide chars, then a 0xE6 terminator at offset 0x10). The first half of the
 * 0x60-byte block is folder index 0, the second half (at +0x20) is index 1.
 *
 * Inputs/outputs: r4..r7 are preserved by the caller; r2/r3 likewise. Only r0,
 * r1 and flags are touched (all internally to set up the four calls).
 */

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word_sub_804A17A(void *dst, u32 num_bytes) {
    R0 = (u32)dst;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls HalfwordFill outside this file, which has no typed C interface yet */
static void halfword_fill(void *dst, u32 count, u32 halfword_value) {
    R0 = (u32)dst;
    R1 = count;
    R2 = halfword_value;
    GUEST_CALL(HalfwordFill);
}

/* register form: calls textScript_copyFolderNameToWide_804A1FC outside this file, which has no typed C interface yet */
static void copy_folder_name_to_wide(u32 folder_index, void *dst) {
    R0 = folder_index;
    R1 = (u32)dst;
    GUEST_CALL(textScript_copyFolderNameToWide_804A1FC);
}

static void build_folder_name_buffers_typed(void) {
    u32 memBlock = A_unk_2001850;

    /* Clear both buffers. */
    zero_fill_by_word_sub_804A17A((void *)memBlock, 0x60);

    /* Buffer 0: fill 0x20 halfwords with 0xE5E5, then copy folder name, then 0xE6. */
    halfword_fill((void *)memBlock, 0x20, 0xE5E5);
    copy_folder_name_to_wide(0, (void *)memBlock);
    ST8(memBlock + 0x10, 0xE6);

    /* Buffer 1: same layout, 0x20 bytes further on. */
    halfword_fill((void *)(memBlock + 0x20), 0x20, 0xE5E5);
    copy_folder_name_to_wide(1, (void *)(memBlock + 0x20));
    ST8(memBlock + 0x20 + 0x10, 0xE6);
}

Next textScript_buildFolderNameBuffers_804A17A(void) {
    build_folder_name_buffers_typed();
    RETURN_TO_CALLER();
}

/* ── textScript_copyFolderNameToWide_804A1FC ── */
/* textScript_copyFolderNameToWide_804A1FC(r0 = folder index, r1 = destination buffer):
 * Copy a folder name from the text script folder-name table into the destination
 * buffer, which stores one character per 2 bytes. Copying stops at the 0xE6 end
 * marker (not copied). An 0xE4 escape byte is copied together with the byte that
 * follows it. R4-R7 are preserved; R0/R1 and the flags are clobbered. */
Next textScript_copyFolderNameToWide_804A1FC(void) {
    u32 table = A_TextScriptFolderNames;
    u32 src = table + LD16(table + (R0 << 1));
    u32 dst = R1;

    for (;;) {
        u32 ch = LD8(src);
        if (ch == 0xE6)
            break;
        if (ch == 0xE4) {
            /* two-byte escape sequence: copy it and the next byte verbatim */
            ST8(dst, ch);
            ST8(dst + 1, LD8(src + 1));
            src += 2;
        } else {
            ST8(dst, ch);
            src += 1;
        }
        dst += 2;
    }
    RETURN_TO_CALLER();
}

/* ── sub_804A230 ── */
/* sub_804A230(r0 = chip ID)
 * Copy a chip's 0x20-byte data block into the chip queue slot.
 * Looks up a per-chip table at dword_804A25C to find which chip slot
 * to fill, then copies 0x20 bytes from unk_2001850 (chip data) into
 * eS200A290 + 0x70 (chip slot in the battle queue / toolkit area). */
Next sub_804A230(void) {
    u8 slot = LD8(A_dword_804A25C + R0);
    u32 dest = A_eS200A290 + 0x70;
    u32 src = A_unk_2001850 + slot;

    R0 = dest;
    R1 = src;
    R2 = 0x20;
    GUEST_CALL(CopyBytes);

    RETURN_TO_CALLER();
}

/* ── sub_804A24C ── */
/* sub_804A24C: looks up a byte at the dword_804A25C table using r0 as the index,
 * then returns unk_2001850 + that byte. r4-r7 and flags are preserved per the
 * register contract. */
static u32 lookup_table_entry(u32 index) {
    u8 entry = LD8(A_dword_804A25C + index);
    return A_unk_2001850 + entry;
}

/* register form: thin adapter - typed body lives in lookup_table_entry. */
Next sub_804A24C(void) {
    R0 = lookup_table_entry(R0);
    RETURN_TO_CALLER();
}

/* ── sub_804C058 ── */
/* sub_804C058: tail-dispatcher for an object whose "handler selector" byte
 * lives at [R5+0x10]. The selector is used as a byte offset into a two-entry
 * handler table (off_804C06C); the chosen handler is entered with
 * r0 = its own entry address and r1 = the selector byte, and everything it
 * leaves behind (notably r2, r8-r12) passes straight through to our caller. */
Next sub_804C058(void) {
    u32 selector = LD8(R5 + 0x10);
    u32 handler  = LD32(A_off_804C06C + selector);

    R0 = handler;
    R1 = selector;

    if (handler == LD32(A_off_804C06C))
        GUEST_CALL(sub_804C08C);
    else if (handler == LD32(A_off_804C06C + 4))
        GUEST_CALL(chatbox_stepIfGatesOpen_804C11C);

    RETURN_TO_CALLER();
}

/* ── sub_804C074 ── */
/* sub_804C074: clear flag EVENT_4FE (flag id 4, bit 0xFE), then clear the 0x400 bit
 * in the toolkit's render-info word (dismisses a render/info state in the battle UI).
 * inputs:  r10, lr
 * outputs: r2, r3, r5
 * clobbers: nz, c, v, r0, r1, lr */
Next sub_804C074(void) {
    /* ClearEventFlagFromImmediate(flag_id = 4, bit = 0xFE) */
    R0 = 4;
    R1 = 0xFE;
    GUEST_CALL(ClearEventFlagFromImmediate);

    /* Edit the toolkit's render-info flags: clear bit 0x400.
     * (Original loads the 0x400 mask into r2 and leaves it there as the
     * function's r2 output; keep r2 = 0x400 on return.) */
    u32 toolkit = R10;
    u32 render_info_ptr = LD32(toolkit + oToolkit_RenderInfoPtr);
    u32 flags = LD16(render_info_ptr);
    R2 = 0x400;
    flags &= ~R2;
    ST16(render_info_ptr, (u16)flags);

    RETURN_TO_CALLER();
}

/* ── sub_804C08C ── */
/* sub_804C08C: initialize the battle toolkit/HUD for a request.
 * Loads palette/tilemap GFX, configures render info and an internal palette
 * blob, zeroes several HUD fields on r5, optionally calls sub_804C2D0 based
 * on event flags, then finalizes by calling sub_804C144/chatbox_stepIfGatesOpen_804C11C and
 * setting the EVENT_4FE flag.
 *   inputs:  r5 (HUD/battle object ptr), r10 (toolkit ptr), lr
 *   outputs: r2
 *   clobbers: nz c v r0 r1 r3 lr
 *   preserved: r4 r5 r6 r7
 */
Next sub_804C08C(void) {
    /* Queue the main palette/tilemap GFX transfer into VRAM. */
    R0 = (u32)A_byte_869D76C;
    R1 = 0x0600CC00;
    R2 = 0xC0;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    /* Queue the secondary tile/palette transfer. */
    R0 = (u32)A_byte_869D74C;
    R1 = (u32)A_byte_3001B00;
    R2 = 0x20;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    /* Configure the toolkit's render info. */
    u32 toolkit = R10;
    u32 render_info = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(render_info + 8, 0x1E09);
    ST16(render_info + 0x16, 0);
    ST16(render_info + 0x14, 0);

    /* Write the 4-byte palette setup to the unk2009740 blob. */
    u32 palette = LD32(toolkit + oToolkit_Unk2009740_Ptr);
    ST8(palette + 0, 0x44);
    ST8(palette + 1, 0x3B);
    ST8(palette + 2, 0x00);
    ST8(palette + 3, 0x10);

    /* Clear HUD fields and set initial state on the r5 battle object. */
    u32 hud = R5;
    ST8(hud + 0x14, 0x0A);
    ST8(hud + 0x13, 0);
    ST8(hud + 0x15, 0);
    ST8(hud + 0x16, 0);
    ST8(hud + 0x17, 0);
    ST8(hud + 0x11, 0);
    ST8(hud + 0x18, 2);

    /* If both EVENT_4FB and EVENT_4FE are unset, do the extra setup. */
    R0 = 4;
    R1 = 0xFB;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (!ZERO_FLAG()) {
        R0 = 4;
        R1 = 0xFE;
        GUEST_CALL(TestEventFlagFromImmediate);
        if (!ZERO_FLAG()) {
            GUEST_CALL(sub_804C2D0);
        }
    }

    GUEST_CALL(sub_804C144);

    ST8(hud + 0x10, 4);

    R0 = 4;
    R1 = 0xFE;
    GUEST_CALL(SetEventFlagFromImmediate);

    GUEST_CALL(chatbox_stepIfGatesOpen_804C11C);

    RETURN_TO_CALLER();
}

/* ── chatbox_stepIfGatesOpen_804C11C ── */
/* chatbox_stepIfGatesOpen_804C11C: gates two chatbox/cutscene steps behind a flag test, a pair of cutscene
 * position checks, and a chatbox flag mask. If all four pass, runs sub_804C144 then
 * sub_804C298. Inputs: r5, r10, lr. Outputs: r2. Clobbers: nz, c, v, r0, r1, r3, lr.
 * Preserved: r4, r5, r6, r7. */
Next chatbox_stepIfGatesOpen_804C11C(void) {
    /* TestPETMenuDataFlag(1) must return zero (flag not set). */
    R0 = 1;
    GUEST_CALL(TestPETMenuDataFlag);
    if (!ZERO_FLAG()) RETURN_TO_CALLER();

    /* Original cutscene position must NOT match 8036F40. */
    GUEST_CALL(cutscene_checkOriginalCutsceneScriptPos_8036F40);
    if (!ZERO_FLAG()) RETURN_TO_CALLER();

    /* Original cutscene position must NOT match 8036F58. */
    GUEST_CALL(cutscene_checkOriginalCutsceneScriptPos_8036F58);
    if (!ZERO_FLAG()) RETURN_TO_CALLER();

    /* Chatbox flag 0x80 must be clear. */
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (!ZERO_FLAG()) RETURN_TO_CALLER();

    /* All gates passed: run the two-step update. */
    GUEST_CALL(sub_804C144);
    GUEST_CALL(sub_804C298);

    RETURN_TO_CALLER();
}

/* ── sub_804C144 ── */
/* sub_804C144: handle the screen-flash / background-fill sequence used while a
 * battle intro or special effect is running. Reads/writes a small state block
 * pointed to by r5 (timers and frame index), reads a counter at word_2013974
 * to decide which sub-effect is active, and copies a 32x32 tile block. */
Next sub_804C144(void) {
    /* The original saves {r4-r7, lr} on entry; reproduce that on the guest
     * stack so the toolkit/state pointers survive the helper calls below. */
    u32 sp = SP;
    SP -= 20;
    ST32(SP + 0,  R4);
    ST32(SP + 4,  R5);
    ST32(SP + 8,  R6);
    ST32(SP + 12, R7);
    ST32(SP + 16, LR);

    /* Pull the render-info pointer out of the toolkit. */
    u32 render_info = LD32(R10 + oToolkit_RenderInfoPtr);
    R4 = render_info;
    ST16(render_info, LD16(render_info) | 0x400);

    u32 state_base = A_unk_2013960;
    R6 = state_base;
    u16 effect_counter = LD16(state_base + 0x14);

    if ((s32)effect_counter > 0x24) {
        GUEST_CALL(tickActionTimer_804C214);
    } else if ((s32)effect_counter > 0) {
        GUEST_CALL(sub_804C244);
    } else {
        /* Counter has reached 0: tear down the effect. */
        R4 = LD32(R10 + oToolkit_RenderInfoPtr);
        ST8(R4, 4);
        ST8(R5 + 0x11, 0);

        /* Epilogue: restore the saved registers and return. */
        R4 = LD32(SP + 0);
        R5 = LD32(SP + 4);
        R6 = LD32(SP + 8);
        R7 = LD32(SP + 12);
        u32 ret = LD32(SP + 16);
        SP = sp;
        RETURN_TO_CALLER();
    }

    /* Frame-advance: hand off to the per-frame helper. */
    GUEST_CALL(applyScrollOffset_804C1D4);

    /* Frame-index / per-frame timer handling, then copy the 32x32 tile block
     * for the current frame into character block 2. */
    u8 frame_index = LD8(R5 + 0x13);
    u8 timer       = LD8(R5 + 0x14) - 1;
    ST8(R5 + 0x14, timer);
    if (timer == 0) {
        ST8(R5 + 0x14, 0xA);
        if (frame_index < 7)
            frame_index++;
        else
            frame_index = 0;
        ST8(R5 + 0x13, frame_index);
    }

    R0 = 0;
    R1 = 0;
    R2 = 2;
    R3 = LD32(A_off_804C1B4 + frame_index * 4);
    R4 = 0x20;
    R5 = 0x20;
    GUEST_CALL(CopyBackgroundTiles);

    /* Epilogue: restore the saved registers and return. */
    R4 = LD32(SP + 0);
    R5 = LD32(SP + 4);
    R6 = LD32(SP + 8);
    R7 = LD32(SP + 12);
    u32 ret = LD32(SP + 16);
    SP = sp;
    RETURN_TO_CALLER();
}

/* ── applyScrollOffset_804C1D4 ── */
/* applyScrollOffset_804C1D4: applies a scroll offset to the RenderInfo X/Y scroll regs and
 * to the caller's r5 object's local X/Y offsets, indexed by the
 * screen/menu index byte at 0x2013965.
 *   inputs:  r5 (object), r10 (toolkit), lr
 *   outputs: r5
 *   clobbers: nz c v r0 r1 r2 r3
 *   preserved: r4 r5 r6 r7
 */
Next applyScrollOffset_804C1D4(void) {
    /* Parallel lookup tables at 0x0804C208 (X addend) and 0x0804C20C
     * (Y addend), indexed by the menu/screen index byte at 0x2013965. */
    u32 idx = LD8(A_unk_2013960 + 5);
    u32 dx = LD8(A_dword_804C208 + idx);
    u32 dy = LD8(A_byte_804C20C + idx);

    u32 render = LD32(R10 + oToolkit_RenderInfoPtr);

    /* X axis. The object offset add may overflow 8 bits; the original
     * `add r0, r0, r3` uses the full 32-bit r3, so the carry bit must
     * reach the RenderInfo scroll. Store the low byte to the object but
     * add the full sum to the scroll. */
    u32 sum_x = (u32)LD8(R5 + 0x15) + dx;
    ST8(R5 + 0x15, (u8)sum_x);
    ST16(render + 0x14, (u16)(LD16(render + 0x14) + sum_x));

    /* Y axis: same pattern at +0x16 / +0x16. */
    u32 sum_y = (u32)LD8(R5 + 0x16) + dy;
    ST8(R5 + 0x16, (u8)sum_y);
    ST16(render + 0x16, (u16)(LD16(render + 0x16) + sum_y));

    RETURN_TO_CALLER();
}

/* ── tickActionTimer_804C214 ── */
/* tickActionTimer_804C214(r5 = battle object?, r10 = toolkit, lr):
 *   Tick down the object's pending-action timer (offset 0x18).
 *   When it expires, reset it to 2 and advance the toolkit's
 *   animation/state counters at oToolkit_Unk2009740_Ptr:
 *     - offset 2 is an index that wraps at 0x10 (then triggers state 4 at [0])
 *     - offset 3 is a separate countdown that gets one step consumed each lap
 */
Next tickActionTimer_804C214(void) {
    u8 timer = LD8(R5 + 0x18);
    timer--;
    SET_FLAGS_NZ(timer);
    ST8(R5 + 0x18, timer);
    if (!ZERO_FLAG())
        RETURN_TO_CALLER();

    /* Timer hit zero: reload to 2. */
    ST8(R5 + 0x18, 2);

    u32 tk = LD32(R10 + oToolkit_Unk2009740_Ptr);
    u8 idx = LD8(tk + 2);
    if (idx >= 0x10) {
        ST8(tk, 4);
    } else {
        idx++;
        ST8(tk + 2, idx);
        u8 sub = LD8(tk + 3);
        if (sub != 0) {
            sub--;
            ST8(tk + 3, sub);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_804C298 ── */
/* sub_804C298:
 *   inputs:  r5 (source struct), r10 (toolkit), lr
 *   outputs: r2 (pointer to unk_2009740)
 *   clobbers: r0, r3
 *   preserved: r5
 *
 * Copies a handful of fields from the source struct (r5) and from the
 * toolkit's render-info / unk_2009740 pointers into a global scratch buffer
 * at unk_2000AC0, then returns the unk_2009740 pointer in r2.
 */
Next sub_804C298(void) {
    u32 buf = A_unk_2000AC0;
    u32 r10 = R10;

    /* Bytes copied from the source struct (r5). */
    ST8(buf + 0x0F, LD8(R5 + 0x10));
    ST8(buf + 0x11, LD8(R5 + 0x11));
    ST8(buf + 0x13, LD8(R5 + 0x13));
    ST8(buf + 0x15, LD8(R5 + 0x15));
    ST8(buf + 0x16, LD8(R5 + 0x17));
    ST8(buf + 0x17, LD8(R5 + 0x18));

    /* Two halfwords from toolkit->RenderInfo (offsets 0x14, 0x16). */
    u32 renderInfo = LD32(r10 + oToolkit_RenderInfoPtr);
    ST16(buf + 0x1A, LD16(renderInfo + 0x14));
    ST16(buf + 0x1C, LD16(renderInfo + 0x16));

    /* Two bytes from toolkit->Unk2009740 (offsets 2, 3); result in r2. */
    u32 unk2009740 = LD32(r10 + oToolkit_Unk2009740_Ptr);
    R2 = unk2009740;
    ST8(buf + 0x18, LD8(unk2009740 + 2));
    ST8(buf + 0x19, LD8(unk2009740 + 3));

    RETURN_TO_CALLER();
}
