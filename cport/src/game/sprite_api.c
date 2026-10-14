/*
 * sprite_api.c — the object sprite API (bn6f sprite.s): update, flip,
 * palette, layer bits, palette upload, and the per-group marker.
 *
 * Drafted by Hyper GLM-5.3 Flash with tools/convert/farm.py (accepted on 40
 * calls captured from the original on the recorded routes, and random
 * inputs), reviewed and integrated by Claude; checked with the host
 * difftest, the CPU oracle and lockstep. Evidence per function: symbols/db.
 */
#include "game.h"
#include "runtime.h"

enum { OBJ_TYPE_AND_SPRITE_OFFSET = 0x02 };   /* bn6f include/structs/ObjectHeader.inc: high nibble = sprite struct offset / 16 */

/* sprite_update(r5 = object header, r4 = caller's saved register):
 * Dispatch to the per-type sprite updater. The high nibble of the header's
 * type/sprite-offset byte selects one of the 16-byte sprite records stored
 * inside the object; r5 is advanced to that record and _sprite_update is
 * called on it, with the record offset also passed in r3. r4 and r5 are
 * restored afterwards (the original pushes/pops them around the call), while
 * r0-r3, LR and the condition flags are handed back exactly as the callee
 * left them. */
Next sprite_update(void) {
    u32 header = R5;
    u32 callerR4 = R4;

    /* High nibble of the type byte: record index * 16 == byte offset. */
    u32 typeOffset = (u32)LD8(header + OBJ_TYPE_AND_SPRITE_OFFSET) & 0xF0;

    R3 = typeOffset;            /* record offset, read by the callee */
    R4 = A__sprite_update | 1;  /* Thumb entry point of _sprite_update */
    R5 = header + typeOffset;   /* point at this type's sprite record */
    GUEST_CALL(_sprite_update);

    /* Original epilogue: pop {r4,r5,pc} — restore the caller's registers. */
    R4 = callerR4;
    R5 = header;
    RETURN_TO_CALLER();
}

/* sprite_setFlip(r0 = flip mode, r5 = object header):
 * Applies the requested flip mode to an object's sprite. The sprite block
 * lives at header + (sprite-offset byte at header+0x02, rounded down to a
 * 16-byte boundary). The two flip bits (mask 0x30) are written into sprite
 * byte 0x13 and mirrored into sprite byte 0x16.
 * Leaves r0 = 0x30 (the flip mask), r1 = new byte 0x16 value,
 * r2 = extracted flip bits, r3 = pointer to the sprite block.
 * Flags: N/Z from the new byte 0x16 value, C from the carry-out of
 * "lsl flipMode, #4" (bit 28 of the flip mode), V from the sprite-address
 * addition "add r3, r3, r5" (the last instruction on this path that
 * affects V; the later ALU ops only overwrite N/Z/C). */
Next sprite_setFlip(void) {
    u32 flipMode = R0;

    /* sprite block = header + (sprite offset byte at header+0x02, 16-byte aligned) */
    u32 spriteOffset = LD8(R5 + 2) & 0xF0;
    u32 sprite = R5 + spriteOffset;
    SET_FLAGS_ADD(spriteOffset, R5); /* matches "add r3, r3, r5"; N/Z/C get overwritten below */

    /* set the flip bits (0x30) in sprite byte 0x13 */
    u32 flipped = (LD8(sprite + oObjectSprite_Unk_13) & ~0x30) | (flipMode << 4);
    ST8(sprite + oObjectSprite_Unk_13, flipped);

    /* mirror the flip bits into sprite byte 0x16, replacing its old ones */
    R1 = (LD8(sprite + oObjectSprite_Unk_16) & ~0x30) | (flipped & 0x30);
    ST8(sprite + oObjectSprite_Unk_16, R1);

    R0 = 0x30;
    R2 = flipped & 0x30;
    R3 = sprite;

    SET_FLAGS_NZ(R1);
    cpu.c = (flipMode >> 28) & 1; /* carry out of "lsl r0, r0, #4" */
    RETURN_TO_CALLER();
}

/* sprite_setPalette(r0 = palette index, r5 = object header):
 * The header's type/sprite-offset byte holds the sprite offset in its high
 * nibble; use it to find the sprite struct, then store the new palette index
 * there. Leaves r3 = sprite pointer and the flags from the pointer addition. */
Next sprite_setPalette(void) {
    u32 header = R5;

    /* sprite base: keep only the high nibble of the type/sprite-offset byte */
    u32 base = (LD8(header + 2) >> 4) << 4;
    u32 sprite = base + header;
    SET_FLAGS_ADD(base, header);

    ST8(sprite + 4, R0);
    R3 = sprite;
    RETURN_TO_CALLER();
}

/* sprite_setPriority_8002E14(r0 = sprite slot index, r5 = object header):
 * Selects the sprite-data block belonging to the object's type (header base
 * plus high nibble of the type byte * 0x10) and patches bits 2-3 of that
 * block's Unk_15 byte with (r0 << 2).
 * Outputs: R0 = r0 << 2, R1 = value stored, R2 = 0xC, R3 = block base;
 * C = carry out of "lsl r0, r0, #2", N/Z from the ORRed value,
 * V from the "add r3, r3, r5" (the last flag-setting instruction for V). */
Next sprite_setPriority_8002E14(void) {
    u32 type = LD8(R5 + OBJ_TYPE_AND_SPRITE_OFFSET);

    /* lsr #4 / lsl #4: keep only the high nibble (the type's sprite block) */
    u32 blockOffset = type & 0xF0;

    /* add r3, r3, r5 sets V (C is overwritten by the shift below) */
    SET_FLAGS_ADD(blockOffset, R5);
    R3 = blockOffset + R5;

    /* lsl r0, r0, #2: carry out is bit 30 of the original value */
    cpu.c = (R0 >> 30) & 1;
    R0 <<= 2;

    R2 = 0xC;
    R1 = LD8(R3 + oObjectSprite_Unk_15) & ~R2;
    R1 |= R0;
    SET_FLAGS_NZ(R1);           /* flags of the final ORR */
    ST8(R3 + oObjectSprite_Unk_15, R1);

    RETURN_TO_CALLER();
}

/* sprite_recordPaletteCountToGroupSlot_80028C0(r0 = index):
 * Write the low byte of the value at dword_200F340 into byte_200F389[index].
 * Leaves r1 = value read, r2 = base address of byte_200F389. */
Next sprite_recordPaletteCountToGroupSlot_80028C0(void) {
    R1 = LD32(A_dword_200F340);
    R2 = 0x0200F389u;        /* byte_200F389 */
    ST8(R2 + R0, R1);
    RETURN_TO_CALLER();
}

/* sprite_loadPalette_8002818 (R5 = sprite resource descriptor):
 * Copies the sprite's 16-colour palette (0x20 bytes) into the sprite palette
 * buffer at byte_3001550 + slot*0x20, where the slot number is the high nibble
 * of byte 0x15.  The palette data is read from field 0x34 when that pointer is
 * set, otherwise from [field 0x1C][4] + 4 + field 0x18, advanced past the
 * sprite's pixel tiles by (byte 4 + byte 5) * 0x20 bytes.
 *
 * If halfword 6 is nonzero, its low 15 bits hold an RGB15 colour that
 * palette_addOrSubtractColor applies to every copied colour (bit 15 picks the
 * operation, passed as R2 = flag * 4).  The routine's own entry address is
 * passed in R1, as in the original.  On return R3 holds the destination slot
 * and R6 = 0x20; R5 is preserved. */
Next sprite_loadPalette_8002818(void) {
    /* Source of the palette data. */
    if (LD32(R5 + 0x34) == 0) {
        u32 sheet = LD32(R5 + 0x1C);
        R0 = LD32(sheet + 4) + 4 + LD32(R5 + 0x18);
    } else {
        R0 = LD32(R5 + 0x34);
    }

    /* Skip past the sprite's pixel tiles to reach its palette:
     * (byte 4 + byte 5) tiles, 0x20 bytes each. */
    u32 tiles = LD8(R5 + 4) + LD8(R5 + 5);
    R0 += tiles << 5;

    /* Destination: palette slot (high nibble of byte 0x15) * 0x20 bytes. */
    R3 = A_byte_3001550 + (LD8(R5 + 0x15) >> 4) * 0x20;

    for (R6 = 0; (s32)R6 < 0x20; R6 += 4)
        ST32(R3 + R6, LD32(R0 + R6));

    /* Optional tint: halfword 6 = bit15 (add/subtract flag) | RGB15 colour. */
    R7 = LD16(R5 + 6);
    if (R7 != 0) {
        R0 = (R7 << 17) >> 17;          /* tint colour, 15 bits */
        R2 = (R7 >> 15) << 2;           /* operation selector: 0 = add, 4 = subtract */
        R6 = 0x20;                      /* 16 colours, 0x20 bytes */
        R7 = R3;                        /* palette slot just written */
        R1 = A_palette_addOrSubtractColor | 1;
        GUEST_CALL(palette_addOrSubtractColor);
    }

    RETURN_TO_CALLER();
}

