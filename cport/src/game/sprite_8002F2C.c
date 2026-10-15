/*
 * sprite_8002F2C.c: readable functions of bn6f sprite.s, from 0x08002F2C, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sprite_clearMosaic ── */
/* Field offsets the disassembly references symbolically. Define them here in
 * case they are not already provided by the shared header. */


/* sprite_clearMosaic(r5 = object):
 * Clears the mosaic bit (0x10) in the object's sprite flag byte.
 * The sprite struct is selected by the high nibble of the object's
 * type/sprite-offset byte, so the flag byte lives at
 * r5 + (typeAndSpriteOffset & 0xF0) + oObjectSprite_SpriteFlags.
 * Leaves r0 = 0x10 (the cleared mask), r1 = new flag byte,
 * r3 = address of the sprite struct, and flags from the address add. */
Next sprite_clearMosaic(void) {
    u32 obj = R5;

    u32 spriteOffset = LD8(obj + oObjectHeader_TypeAndSpriteOffset) & 0xF0;
    u32 sprite = obj + spriteOffset;

    /* ADDS sets c/v/nz, which callers inspect */
    R3 = sprite;
    SET_FLAGS_ADD(spriteOffset, obj);

    u32 flags = LD8(sprite + oObjectSprite_SpriteFlags) & ~0x10;
    ST8(sprite + oObjectSprite_SpriteFlags, flags);

    R0 = 0x10;
    R1 = flags;
    RETURN_TO_CALLER();
}

/* ── obj_getSpriteFlag10AndRenderInfoHigh_8002F3E ── */
/* obj_getSpriteFlag10AndRenderInfoHigh_8002F3E(r0 = object header, r10 = toolkit):
 *   r0 = high byte of the render info halfword at renderInfo+2
 *        (low 8 bits masked off, then shifted down),
 *   r2 = bit 4 (0x10) of the sprite's Unk_11 flag byte. */
Next obj_getSpriteFlag10AndRenderInfoHigh_8002F3E(void) {
    u32 object = R0;

    /* The sprite struct for this object lives at object + (spriteOffset & 0xF0):
     * the header byte at +2 holds its index in the low nibble and its page in bits 4-7. */
    u32 spriteOffset = LD8(object + 2); /* oObjectHeader_SpriteOffset */
    u32 sprite = object + (spriteOffset & 0xF0);

    u32 flag = LD8(sprite + 0x11) & 0x10; /* oObjectSprite_SpriteFlags */

    u32 renderInfo = LD32(R10 + 8); /* oToolkit_RenderInfoPtr */
    u32 value = LD16(renderInfo + 2) & ~0xFFu; /* oRenderInfo_MosaicSize */

    R0 = value >> 8;
    R2 = flag;
    RETURN_TO_CALLER();
}

/* ── sprite_getFlip ── */
/* sprite_getFlip(r0 = object header pointer):
 * Returns the flip value (0-3) stored in bits 4-5 of the sprite's Unk_16
 * field. The sprite offset byte's low nibble is masked off (the offset is
 * always a multiple of 16), then used to step from the header to the
 * sprite struct.
 * Outputs: r0 = flip value. */
Next sprite_getFlip(void) {
    u32 header = R0;

    /* oObjectHeader_SpriteOffset (byte at +2): sprite data offset, 16-aligned */
    u32 sprite = header + (LD8(header + 2) & 0xF0);
    R0 = (LD8(sprite + oObjectSprite_CurFrame) & 0x30) >> 4;

    RETURN_TO_CALLER();
}

/* ── sprite_noShadow ── */
/* sprite_noShadow(r5 = object):
 * Marks the object's current sprite as having no shadow. The sprite record
 * lives at obj + (oObjectHeader_SpriteOffset & ~0xF) (offsets 0x02 in the
 * header, sprite records are 16-byte aligned), and the flag byte at offset 3
 * of the sprite (the "Unk_03" attribute) gets bit 2 set and bit 0 cleared.
 *
 * Returns:
 *   r0 = updated attribute byte, r1 = 1, r3 = address of the sprite record
 *   nz = from the final BIC on the attribute byte; c/v = from the ADDS that
 *   forms the sprite address (logical ops in Thumb don't touch c/v). */
Next sprite_noShadow(void) {
    u32 offset = LD8(R5 + 2);               /* oObjectHeader_SpriteOffset */
    u32 sprite = R5 + (offset & ~0xFu);     /* round down to 16-byte record */
    SET_FLAGS_ADD(sprite, R5);

    u32 attrs = LD8(sprite + 3);            /* oObjectSprite_SpriteFlags2 */
    attrs |= 4;                             /* no-shadow bit on */
    attrs &= ~1u;                           /* bit 0 off */
    SET_FLAGS_NZ(attrs);

    ST8(sprite + 3, attrs);

    R0 = attrs;
    R1 = 1;
    R3 = sprite;
    RETURN_TO_CALLER();
}

/* ── sprite_setUnk0x2c ── */
/* sprite_setUnk0x2c(r0 = value, r5 = object header):
 * The object header's TypeAndSpriteOffset byte (offset 2) holds the sprite ID
 * in its high nibble. Compute the address of the object's sprite struct from
 * that ID and store the value into its field at oObjectSprite_ScriptParam.
 * Leaves r3 pointing at that sprite struct; flags come from the address add. */
Next sprite_setUnk0x2c(void) {
    u32 typeAndSprite = LD8(R5 + 2);            /* oObjectHeader_TypeAndSpriteOffset */
    u32 spriteId = (typeAndSprite >> 4) << 4;   /* keep only the sprite ID nibble */

    R3 = spriteId + R5;
    SET_FLAGS_ADD(spriteId, R5);

    ST32(R3 + oObjectSprite_ScriptParam, R0);
    RETURN_TO_CALLER();
}

/* ── sprite_getUnk0x2c ── */
/* sprite_getUnk0x2c(r5 = object):
 * The object header's TypeAndSprite field (offset 2) packs the sprite's
 * offset within the object into its high nibble; use it (as a multiple of
 * 0x10) to reach the object's sprite record and return its Unk_2c word.
 * The ADD is the last flag-setting instruction, so carry is produced. */
Next sprite_getUnk0x2c(void) {
    u32 spriteOffset = LD8(R5 + 2) & 0xF0;

    R0 = LD32(R5 + spriteOffset + oObjectSprite_ScriptParam);
    SET_FLAGS_ADD(spriteOffset, R5);

    RETURN_TO_CALLER();
}

/* ── object_writeSubstructField30_8002FEC ── */
/* object_writeSubstructField30_8002FEC(r0 = value, r5 = object):
 * Store `value` into a field of one of the sub-structures attached to `object`.
 * The high nibble of the byte at object+2 selects which sub-structure
 * (index * 0x10 bytes), and the value goes to its field at +0x30.
 * Returns the address written to in r3; the v flag reflects the address
 * computation (ADDS), as callers expect. */
Next object_writeSubstructField30_8002FEC(void) {
    u32 base = R5;
    u32 offset = (LD8(base + 2) >> 4) << 4; /* high nibble -> byte offset */
    u32 target = offset + base;

    ST32(target + 0x30, R0);

    R3 = target;
    SET_FLAGS_ADD(offset, base);
    RETURN_TO_CALLER();
}

/* ── sprite_setSubEntryPaletteBank_800304A ── */
/* sprite_setSubEntryPaletteBank_800304A(r0 = palette bank, r5 = object base):
 * The high nibble of the object's byte at +2 selects one 0x10-byte
 * sub-entry within the object; store the palette bank (r0) into the high
 * nibble of that sub-entry's byte at +0x15, leaving its low nibble intact.
 * Leaves r0 = bank << 4, r1 = new byte value, r2 = 0xF0, r3 = byte address.
 * Flags: NZ from the stored value, C from the carry-out of the LSL #4,
 * V from the pointer addition. */
Next sprite_setSubEntryPaletteBank_800304A(void) {
    u32 slot = LD8(R5 + 2) & 0xF0;   /* sub-entry index * 0x10 */
    u32 addr = R5 + slot;

    SET_FLAGS_ADD(slot, R5);         /* adds r3, r3, r5 */

    R2 = 0xF0;
    R1 = LD8(addr + 0x15) & ~R2;     /* bic: clear palette nibble */

    cpu.c = (R0 >> 28) & 1;          /* carry out of lsl r0, r0, #4 */
    R0 <<= 4;

    R1 |= R0;                        /* orr: insert new palette nibble */
    SET_FLAGS_NZ(R1);

    ST8(addr + 0x15, R1);
    R3 = addr;
    RETURN_TO_CALLER();
}

/* ── sprite_setSubRecordFieldA_8003060 ── */
/* sprite_setSubRecordFieldA_8003060(r0 = value, r5 = sprite object):
 * Store r0 into a field of the sprite object's 16-byte-aligned record.
 * The object holds a bank/index in bits 4-7 of its byte at +2; masking it
 * back down selects the base of the record, and the value is written to
 * that record's byte at +0xA. r3 is left pointing at the record base.
 * Flags: c/v come from the ADD that forms the record base. */
Next sprite_setSubRecordFieldA_8003060(void) {
    u32 record = R5 + ((LD8(R5 + 2) >> 4) << 4);

    ST8(record + 0xA, R0);

    R3 = record;
    SET_FLAGS_ADD(record, R5);
    RETURN_TO_CALLER();
}

/* ── slotList_storeHalfwordAndClearField_800306C ── */
/* slotList_storeHalfwordAndClearField_800306C(r0 = value, r5 = list base):
 * The high nibble of the byte at r5+2 selects a 16-byte slot in the list.
 * Store r0 as a halfword at slot+8 and clear the 32-bit field at slot+0x24
 * (which extends into the following slot). */
Next slotList_storeHalfwordAndClearField_800306C(void) {
    u32 slot = R5 + (LD8(R5 + 2) & 0xF0);

    ST16(slot + 8, R0);
    ST32(slot + 0x24, 0);

    RETURN_TO_CALLER();
}

/* ── sprite_setCoordinates ── */
/* sprite_setCoordinates(r0 = x, r1 = y, r5 = sprite resource pointer):
 * Byte 2 of the resource holds an index whose high nibble selects a
 * sub-entry 0x10 bytes into the resource; store the coordinates into
 * that entry's position fields.
 * Returns the selected entry address in r3, flags from its computation. */
Next sprite_setCoordinates(void) {
    u32 entry = R5 + (LD8(R5 + 2) & 0xF0);

    ST16(entry + 0xC, R0); /* x */
    ST16(entry + 0xE, R1); /* y */

    R3 = entry;
    SET_FLAGS_ADD(entry, R5);
    RETURN_TO_CALLER();
}
