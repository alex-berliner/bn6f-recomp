/*
 * sprite.c: readable functions of bn6f sprite.s, up to 0x08002F2C, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sprite_loadAnimationData ── */
/* sprite_loadAnimationData(r5 = object header, r4 = scratch):
 * Loads the animation data for the sprite attached to an object header.
 * The high nibble of the header's type/sprite-offset byte (at offset 2) is
 * an offset in 16-byte units; r5 is advanced past the header to the sprite
 * data and the shared animation loader is run on it. r4/r5 are preserved
 * for the caller (the original saves them on the stack around the call);
 * r0-r3 and the flags are whatever the loader leaves behind. */
Next sprite_loadAnimationData(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    /* Skip past the object header to this sprite's data block:
     * high nibble of the type/sprite-offset byte, times 16. */
    u32 offset = (LD8(R5 + 2) >> 4) << 4;
    R5 = savedR5 + offset;

    /* Hand the loader its own entry address (Thumb bit set), exactly as the
     * original computed-call does. */
    R4 = (A__sprite_loadAnimationData | 1);

    GUEST_CALL(_sprite_loadAnimationData);

    /* Restore the caller's r4/r5, as the original's pop does. */
    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── sprite_loadAnimationData_spritePtrProvidedDirectly ── */
/* sprite_loadAnimationData_spritePtrProvidedDirectly (0x080026B6):
 * Thin trampoline around _sprite_loadAnimationData. The callee receives the
 * caller's r4/r5 untouched and returns its results in r2/r3; this wrapper
 * exists only so that r4 survives the call (the original pushes/pops r4
 * around the branch). */
Next sprite_loadAnimationData_spritePtrProvidedDirectly(void) {
    u32 savedR4 = R4;

    GUEST_CALL(_sprite_loadAnimationData);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── sprite_chatbox_80026D6 ── */
/* sprite_chatbox_80026D6: trampoline that jumps to _sprite_update.
 * The sprite update routine is entered with r4 holding its own address
 * (thumb bit set) so it can find its data tables; this thunk sets that
 * up, then on return restores the caller's r4 and returns straight to
 * the caller (pop {r4,pc} uses the lr saved at entry). */
Next sprite_chatbox_80026D6(void) {
    u32 callerR4 = R4;

    R4 = (A__sprite_update | 1);   /* ldr r4, =_sprite_update+1 */
    GUEST_CALL(_sprite_update);    /* mov lr, pc; bx r4 */

    R4 = callerR4;                 /* pop {r4} */
    RETURN_TO_CALLER();            /* pop {pc} */
}

/* ── sprite_load ── */
/* sprite_load:
 *   inputs:  r0 r1 r2 r5 lr
 *   outputs: r1 r4 r5
 *   clobbers: nz c v r0 r2 r3 lr
 *   preserved: r4 r5
 *
 * Loads a sprite for an object. r0 points to sprite data, r1/r2 form a sprite ID,
 * and r5 points at the object header. Clears the object's "loaded" bit, locates the
 * sprite via the lookup table (falling back to SpritePointersList and a white-dot
 * placeholder), then installs it. r1 (the original r0) is stashed in the sprite's
 * Unk_03 field. */
Next sprite_load(void) {
    u32 object_base = R5;
    u32 r0_in = R0, r1_in = R1, r2_in = R2;

    /* Clear bit 3 (0x8) of the object's flags. */
    ST8(R5, LD8(R5) & (u8)~8);

    /* Advance R5 past the per-type header (high nibble of TypeAndSpriteOffset). */
    u8 tso = LD8(object_base + oObjectHeader_TypeAndSpriteOffset);
    u32 sprite = object_base + ((tso >> 4) << 4);
    R5 = sprite;

    /* push {r0-r2}, then r0 = (r1 << 8) | r2 for the lookup. */
    u32 sp = SP - 12; SP = sp;
    ST32(sp + 0, r0_in);
    ST32(sp + 4, r1_in);
    ST32(sp + 8, r2_in);
    R0 = (r1_in << 8) | r2_in;

    GUEST_CALL(lookupValueForIdInTableAt_200DCA0_8002986);
    u32 found = R0;

    /* pop {r1-r3} rotates r0->r1, r1->r2, r2->r3. */
    u32 new_r1 = LD32(sp + 0);  /* = original r0 */
    u32 new_r2 = LD32(sp + 4);  /* = original r1 */
    u32 new_r3 = LD32(sp + 8);  /* = original r2 */
    SP += 12;

    if (found == 0) {
        /* Fallback: SpritePointersList[r1], then index by r2*4. */
        u32 list_entry = LD32(A_SpritePointersList + new_r2);
        new_r3 <<= 2;
        u32 sprite_data = LD32(list_entry + new_r3);
        if ((s32)sprite_data < 0) {
            R0 = A_spriteWhiteDot;
        } else {
            R0 = sprite_data;
        }
    }

    /* push {r1}; bl sprite_initialize; pop {r1} -- preserve r1 across the call. */
    R1 = new_r1;
    GUEST_CALL(sprite_initialize);
    R1 = new_r1;

    /* Stash the requested id (original r0) into the sprite's Unk_03 field. */
    R5 = sprite;
    ST8(R5 + oObjectSprite_SpriteFlags2, (u8)R1);

    /* Restore preserved r5 (r4 is never touched in the register file). */
    R5 = object_base;
    RETURN_TO_CALLER();
}

/* ── sprite_initialize ── */
/* sprite_initialize(r0 = sprite resource id, r5 = sprite object):
 * reset an object's sprite state. Stores r0+4 (the first resource slot
 * used by the sprite) into Unk_18, zeroes all animation/position/timer
 * fields, marks the current frame as "none" (Unk_28 = 0xFFFFFFFF) and
 * sets the default 8x8 cell size in Unk_15/Unk_16.
 * The opening ADDS is the last instruction that touches c/v, so those
 * flags are outputs; later MOVs only change nz. */
Next sprite_initialize(void) {
    u32 obj = R5;

    /* flags must come from ADDS r0, #4 */
    u32 base = R0;
    R0 = base + 4;
    SET_FLAGS_ADD(base, 4);

    ST32(obj + oObjectSprite_AnimationResourcePtr, R0);

    /* clear state, timers and coordinates */
    ST8 (obj + 3, 0);
    ST8 (obj + 0, 0);
    ST32(obj + oObjectSprite_SpriteRenderDataPtr, 0);
    ST8 (obj + 4, 0);
    ST16(obj + oObjectSprite_SpriteExtraBytes, 0);
    ST16(obj + 6, 0);
    ST32(obj + oObjectSprite_ScriptParam, 0);
    ST16(obj + oObjectSprite_ScratchHalfword, 0);
    ST16(obj + oObjectSprite_FrameCounter, 0);
    ST8 (obj + oObjectSprite_InitFlag, 0);
    ST8 (obj + 5, 0);
    ST32(obj + oObjectSprite_SpriteFlags4Ptr, 0);
    ST32(obj + oObjectSprite_ScratchByte, 0);

    /* no active frame: all bits set */
    ST32(obj + oObjectSprite_SpriteAttrs2Ptr, 0xFFFFFFFF);

    /* default 8x8 sprite cell */
    ST8 (obj + oObjectSprite_OamAttributes, 8);
    ST16(obj + oObjectSprite_CurFrame, 8);

    RETURN_TO_CALLER();
}

/* ── spriteLoadMugshot_800275A ── */
/* spriteLoadMugshot_800275A(r0 = mugshot index, r5 = sprite struct):
 * initialize the mugshot sprite for the given index and mark r5's
 * halfword at +0x16 as 0x80 (sprite loaded). */
Next spriteLoadMugshot_800275A(void) {
    R0 = LD32(A_mugshotSpritePtrs + R0 * 4);
    GUEST_CALL(sprite_initialize);
    ST16(R5 + 0x16, 0x80);
    RETURN_TO_CALLER();
}

/* ── sprite_setRenderFlushState_80027B4 ── */
/* sprite_setRenderFlushState_80027B4: set the flag at dword_200F350 (A_dword_200F350) to 1.
 * Probably marks some sprite/resource table as loaded/available. */
Next sprite_setRenderFlushState_80027B4(void) {
    ST32(A_dword_200F350, 1);
    RETURN_TO_CALLER();
}

/* ── palette_InitTransferState_80027D4 ── */
/* palette_InitTransferState_80027D4: initialize the palette-transfer state at byte_20098A8.
 * Sets the entry size to 4, the entry count to 10, and the halfword
 * transfer length to 0x2FF for the upcoming palette DMA copy. */
Next palette_InitTransferState_80027D4(void) {
    u32 state = A_byte_20098A8;  /* = byte_20098A8 */

    ST8(state + 1, 4);        /* byte_20098A9 = 4  */
    ST8(state, 0xA);          /* byte_20098A8 = 10 */
    ST16(state + 2, 0x2FF);   /* word_20098AA = 0x2FF */

    RETURN_TO_CALLER();
}

/* ── sprite_resetPaletteCount_800286C ── */
/* sprite_resetPaletteCount_800286C: clear the 32-bit global dword_200F340 to zero (some
 * global counter/flag reset; no results, no flags). */
Next sprite_resetPaletteCount_800286C(void) {
    ST32(A_dword_200F340, 0);
    RETURN_TO_CALLER();
}

/* ── sprite_loadAnimFrameTiles_8002874 ── */
/* sprite_loadAnimFrameTiles_8002874(r5 = object animation state):
 * Advances an object's graphics frame. Fields used:
 *   +0x18  index of the current frame within the animation table
 *   +0x1C  pointer to the animation table pointer
 *   +0x24  pointer to the frame descriptor currently loaded in VRAM
 *   +0x08  VRAM slot number (each slot holds one 8-word tile block)
 * If the frame descriptor for the current index differs from the one already
 * uploaded, store it and queue a transfer that copies its tile data
 * (descriptor words, followed by the tiles) into vObjectTiles[slot].
 * Flags: on the "unchanged" path they come from the frame comparison;
 * on the "upload" path they come from QueueEightWordAlignedGFXTransfer. */
Next sprite_loadAnimFrameTiles_8002874(void) {
    u32 loadedFrame = LD32(R5 + 0x24);
    u32 table = LD32(LD32(R5 + 0x1C));
    u32 currentFrame = table + LD32(R5 + 0x18);

    if (currentFrame == loadedFrame) {
        /* Frame already in VRAM; nothing to do. */
        SET_FLAGS_CMP(currentFrame, loadedFrame);
        RETURN_TO_CALLER();
    }

    ST32(R5 + 0x24, currentFrame);

    u32 tileCount = LD32(currentFrame);              /* first word: tile count */
    u32 src = currentFrame + 4;                      /* tiles follow the header word */
    u32 dest = A_vObjectTiles + ((u32)LD16(R5 + 8) << 5);

    R0 = src;
    R1 = dest;
    R2 = tileCount;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    RETURN_TO_CALLER();
}

/* ── sprite_resetObjVars_800289C ── */
/* sprite_resetObjVars_800289C:
 * Reset the sprite object state: clear the counter at dword_20093A8, then
 * fill the 0x388-word table at dword_200A890 with 0xFFFFFFFF via WordFill.
 * Flags on return (including v) are those left by WordFill's last
 * flag-setting instruction; r3 is untouched. */
Next sprite_resetObjVars_800289C(void) {
    ST32(A_dword_20093A8, 0);

    R0 = A_dword_200A890;
    R1 = 0x388;
    R2 = 0xFFFFFFFF;
    GUEST_CALL(WordFill);

    RETURN_TO_CALLER();
}

/* ── initUncompSpriteState_80028d4 ── */
/* initUncompSpriteState_80028d4(r0 = sprite data pointer):
 * Resets the uncompressed-sprite work block at byte_200DCA0 before a new
 * sprite is decompressed into it:
 *   - the whole 0x50-byte block is zeroed,
 *   - the given data pointer is kept at byte_200DCEC (+0x4C),
 *   - the 0x18-halfword table at +0x04 is filled with 0x7FFF,
 *   - the 0x30-byte buffer at +0x1C is filled with 0xFF.
 * Returns with r2 = 0xFF (last fill value) and the flags left by ByteFill;
 * r3 and r5 are untouched as far as callers are concerned. */
Next initUncompSpriteState_80028d4(void) {
    u32 callerR5 = R5;   /* original pushes r5 and pops it back at the end */
    u32 dataPtr = R0;    /* original spills r0 around the first fill call */
    u32 block = A_byte_200DCA0;

    /* zero the entire state block */
    R0 = block;
    R1 = 0x50;
    GUEST_CALL(ZeroFillByWord);

    /* remember which sprite data this state belongs to */
    ST32(block + 0x4C, dataPtr); /* dword_200DCEC */

    /* clear the halfword table at +4 (0x80<<24 inverted, shifted = 0x7FFF) */
    R0 = block + 4;
    R1 = 0x18;
    R2 = 0x7FFF;
    GUEST_CALL(HalfwordFill);

    /* clear the tile buffer at +0x1C */
    R0 = block + 0x1C;
    R1 = 0x30;
    R2 = 0xFF;
    GUEST_CALL(ByteFill);

    R5 = callerR5;
    RETURN_TO_CALLER();
}

/* ── lookupValueForIdInTableAt_200DCA0_8002986 ── */
/* lookupValueForIdInTableAt_200DCA0_8002986(r0 = id, r5 preserved):
 * Search the 12-entry table rooted at byte_200DCA0. Each entry i has a
 * u16 id at table+4+i*2 and a u32 payload at table+0x1C+i*4. Returns the
 * payload of the entry whose id matches r0, or 0 if no entry matches.
 * r1/r2 are scratch in the original; they are set to the same values the
 * original leaves behind (last compared id / loop index) for exactness. */
Next lookupValueForIdInTableAt_200DCA0_8002986(void) {
    u32 table = A_byte_200DCA0;
    u32 key = R0;
    u32 result = 0;
    u32 found = 0;
    u32 lastId = 0;
    u32 i;

    for (i = 0; i < 12; i++) {
        lastId = (u16)LD16(table + 4 + i * 2);
        if (lastId == key) {
            result = LD32(table + 0x1C + i * 4);
            found = 1;
            break;
        }
    }

    if (found) {
        R0 = result;
        R1 = 0x1C + i * 4;      /* address offset used by the original's load */
        R2 = i;
    } else {
        R0 = 0;
        R1 = lastId;            /* id of the final (12th) entry */
        R2 = 12;
    }

    RETURN_TO_CALLER();
}

/* ── sprite_clearSlot_8002BCC ── */
/* sprite_clearSlot_8002BCC(r5 = sprite base):
 * Reset a sprite entry: mark its OAM attribute as hidden (0x7FFF) and its
 * link field as -1 (free slot marker). r0 is preserved from the caller
 * (the push/pop pair restores r0..r7 and pc).
 *   inputs:  r5 lr
 *   outputs: r0 r5
 *   clobbers: nz c v
 *   preserved: r0 r1 r2 r3 r4 r5 r6 r7 */
Next sprite_clearSlot_8002BCC(void) {
    u32 index = LD8(R5);

    /* Sprite attribute table: (u16 attr[index]) at offset (index*2 + 4). */
    ST16(R5 + ((index << 1) + 4), 0x7FFF);

    /* Sprite link/extra table: (u32 link[index]) at offset (index*4 + 0x1C). */
    ST32(R5 + ((index << 2) + 0x1C), 0xFFFFFFFFU);

    /* Flags from the last flag-setting op (strh does not affect flags; the
     * meaningful flag-setter is the implicit OR that built 0x7FFF). Use the
     * final stored value as the flags source. */
    SET_FLAGS_NZ(0x7FFF);
    RETURN_TO_CALLER();
}

/* ── sprite_setAlpha_8002c7a ── */
/* sprite_setAlpha_8002c7a(r0 = alpha, r2 = -, r5 = sprite object, r7 = preserved,
 *                         r10 = toolkit):
 * Trampoline for the shared sprite effect setter: select effect mode 4
 * (per-sprite blend/alpha mode in the object's sprite record) and tail-call
 * sprite_setAlpha, which stores the mode into the object's sprite entry and
 * reprograms the global blend block pointed at by the toolkit (blend mode
 * bits cleared, coefficient A = alpha, coefficient B = 0x10 - alpha, first
 * target bank emptied, second bank filled with 0x3F). All outputs
 * (r0 = 0x3F, r1 = masked target byte, r2 = 0, r3 = blend block, nz/c/v)
 * come from the callee; r5 and r10 pass through untouched and r7 is
 * preserved by it. */
Next sprite_setAlpha_8002c7a(void) {
    R2 = 4;
    GUEST_CALL(sprite_setAlpha);
    RETURN_TO_CALLER();
}

/* ── sprite_getMosaicScalingParameters ── */
/* sprite_getMosaicScalingParameters(r0 = object header, r10 = toolkit):
 * Looks up the mosaic/scaling entry for the object's sprite.
 *   r0 = parameter byte from toolkit->Unk2009740[2],
 *   r2 = mosaic flag bits (mask 0x0C) from the sprite's Unk_11 field. */
Next sprite_getMosaicScalingParameters(void) {
    u32 obj = R0;

    /* The high nibble of the header's type byte selects the sprite
     * attached to the object, in 16-byte units from the header. */
    u32 sprite = obj + (LD8(obj + 2) & 0xF0);

    R2 = LD8(sprite + oObjectSprite_SpriteFlags) & 0xC;

    u32 params = LD32(R10 + oToolkit_Unk2009740_Ptr);
    R0 = LD8(params + 2);

    RETURN_TO_CALLER();
}

/* ── sprite_makeScalable ── */
/* register form: calls loc_8002D22 outside this file, which has no typed C interface yet.
   The original push {r5,lr} must be replicated because loc_8002D22's pop {r5,lr}
   restores the caller's r5 and return address. */
static void tail_cont_scalable(u32 scaled_ptr, u32 rounded_offset) {
    SP -= 8;
    ST32(SP, R5);
    ST32(SP + 4, LR);
    R5 = scaled_ptr;
    R3 = rounded_offset;
    GUEST_CALL(loc_8002D22);
    /* loc_8002D22 pops {r5,lr} and returns to sprite_makeScalable's caller. */
}

/* Round the sprite offset in an object header down to a 16-byte boundary
 * and add it to the object pointer, yielding the scalable sprite data pointer.
 * Then tail-jump into the continuation routine at loc_8002D22. */
static void sprite_make_scalable(u32 obj_ptr) {
    u32 raw_off = LD8(obj_ptr + oObjectHeader_SpriteOffset);
    u32 rounded = (raw_off >> 4) << 4;
    u32 scaled  = obj_ptr + rounded;
    tail_cont_scalable(scaled, rounded);
}

/* sprite_makeScalable(r5 = object ptr) → tail-calls loc_8002D22 */
Next sprite_makeScalable(void) {
    sprite_make_scalable(R5);
    RETURN_TO_CALLER();
}

/* ── sprite_makeUnscalable ── */
/* Offset of the type/sprite-offset byte in an object header (the disassembly
 * reads it as [r5,#oObjectHeader_TypeAndSpriteOffset], i.e. byte 2). */
#define oObjectHeader_TypeAndSpriteOffset 2

/* sprite_makeUnscalable(r5 = object header):
 * Mark the object's sprite as non-scalable. The sprite block lives 16 bytes
 * per unit past the header, according to the high nibble of the header's
 * type/sprite-offset byte; the body works through that pointer (r5 is
 * temporarily redirected there, and the callee needs it), but the original
 * saves/restores r5 around the call, so the header pointer is what survives.
 * It then strips the "scalable" bits from the sprite state (Unk_11 bits 0-1,
 * Unk_13 bits 1-5) and clears the matching bit in the sprite flag word.
 * The Thumb `add r5, r5, r3` is an ADDS: it sets N/Z/C/V, and those C/V
 * survive the TST on the early-exit path (the callee overwrites all flags
 * on the normal path). NZ on the early-exit path comes from the TST. */
Next sprite_makeUnscalable(void) {
    u32 object = R5;

    R3 = LD8(object + oObjectHeader_TypeAndSpriteOffset) & 0xF0;
    SET_FLAGS_ADD(object, R3);          /* ADDS r5, r5, r3 */
    u32 sprite = object + R3;
    R5 = sprite;                        /* body and callee see the sprite block */

    R1 = 3;
    R0 = LD8(sprite + oObjectSprite_SpriteFlags);
    if ((R0 & R1) == 0) {
        SET_FLAGS_NZ(R0 & R1);          /* tst r1, r0: C/V stay from the ADDS */
        R5 = object;                    /* pop {r5} restores the header pointer */
        RETURN_TO_CALLER();
    }

    ST8(sprite + oObjectSprite_SpriteFlags, R0 & ~R1);

    R0 = LD8(sprite + oObjectSprite_SpriteFlags3);
    R2 = R0;
    R1 = 0x3E;
    R0 = (R0 & R1) >> 1;                /* bit index within the flag word */
    R2 &= ~R1;
    ST8(sprite + oObjectSprite_SpriteFlags3, R2);
    GUEST_CALL(flags_clearFlagWordBit_802FE6A);

    R5 = object;                        /* pop {r5} restores the header pointer */
    RETURN_TO_CALLER();
}

/* ── sprite_getPalette ── */
/* sprite_getPalette(r0 = object header pointer):
 * The header's type byte (offset 2) holds the sprite's offset from the
 * header in its high nibble (in units of 0x10 bytes). Add that to the
 * header to find the sprite data, and return its palette byte at +4.
 * Outputs: r0 = palette byte, r3 = sprite data pointer. */
Next sprite_getPalette(void) {
    u32 header = R0;

    u32 spriteOffset = (LD8(header + 2 /* oObjectHeader_TypeAndSpriteOffset */) >> 4) << 4;
    u32 spriteData = header + spriteOffset;

    R3 = spriteData;
    R0 = LD8(spriteData + 4 /* oObjectSprite_PaletteIndex */);
    RETURN_TO_CALLER();
}

/* ── sprite_setAnimation ── */
/* sprite_setAnimation(r0 = animation index, r5 = object header):
 * Select an animation for the object's sprite. The high nibble of the
 * object's TypeAndSpriteOffset byte (offset 2) holds the sprite slot
 * number; each slot is 16 bytes, so the slot's base is obj + (nibble << 4).
 * The new animation index is written to that slot's first byte. */
Next sprite_setAnimation(void) {
    u32 obj = R5;

    u32 slotOffset = LD8(obj + 2) & 0xF0;
    ST8(obj + slotOffset, R0);

    RETURN_TO_CALLER();
}

/* ── sprite_getFinalPalette ── */
/* Sprite palette lookup helpers. The object header's TypeAndSpriteOffset
 * byte is at offset 2; its upper nibble is the sprite's VRAM tile offset,
 * which indexes the object's sprite record list. oObjectSprite_OamAttributes is
 * the final palette byte of a sprite record. */
#define oObjectHeader_TypeAndSpriteOffset 2

/* sprite_getFinalPalette(r0 = object header pointer):
 * Compute the palette used by an object's current sprite. The header's
 * TypeAndSpriteOffset byte holds the sprite's VRAM offset (in 32-byte
 * tiles) in its upper nibble; that offset locates the sprite's record,
 * whose Unk_15 byte supplies the final palette index. Result: r1 = palette. */
Next sprite_getFinalPalette(void) {
    u32 spriteOffset = LD8(R0 + oObjectHeader_TypeAndSpriteOffset) & 0xF0;
    u32 sprite = R0 + spriteOffset;

    R1 = LD8(sprite + oObjectSprite_OamAttributes) & 0xF0;
    RETURN_TO_CALLER();
}

/* ── sprite_clearFinalPalette ── */
/* Clear the palette high-nibble from a sprite's final-palette byte.
 * The sprite is located at header + (type_nibble << 4); its byte at +0x15
 * is masked to the low nibble only.
 *
 * Flag sources (last-wins per flag):
 *   BIC Rd,Rm → N, Z   (masked byte, 0–15)
 *   Thumb ADD Rdn,Rm  → C, V preserved from   shifted + header  */
typedef struct {
    u32 sprite_ptr; /* computed sprite address (→ r3) */
    u8  masked;     /* low-nibble result             (→ N, Z)  */
    u8  shifted;    /* (type_nibble << 4)           (→ C, V)  */
} SpritePaletteInfo;

static SpritePaletteInfo clear_final_palette(u32 header) {
    u32 type_and_off = LD8(header + oObjectHeader_TypeAndSpriteOffset);
    u8  shifted      = (u8)((type_and_off >> 4) << 4); /* same as & 0xF0 */
    u32 sprite       = header + shifted;

    u8 masked = (u8)LD8(sprite + oObjectSprite_OamAttributes) & 0x0F;
    ST8(sprite + oObjectSprite_OamAttributes, masked);

    return (SpritePaletteInfo){ .sprite_ptr = sprite, .masked = masked, .shifted = shifted };
}

/* sprite_clearFinalPalette(r5 = object header ptr)
 * → r1 = 0xF0, r3 = sprite ptr;  N,Z from BIC(masked);  C,V from Thumb ADD(shifted,header) */
Next sprite_clearFinalPalette(void) {
    SpritePaletteInfo info = clear_final_palette(R5);
    R1 = 0xF0;
    R3 = info.sprite_ptr;
    SET_FLAGS_ADD(info.shifted, R5);   /* C, V from the header+offset addition */
    SET_FLAGS_NZ(info.masked);          /* N, Z from the BIC result (overwrites ADD's N, Z) */
    RETURN_TO_CALLER();
}

/* ── sprite_getFrameParameters ── */
/* sprite_getFrameParameters(r5 = object header):
 * Reads the sprite parameters of the object's current animation frame.
 * Computes the sprite data address from the object's type/sprite-offset byte
 * at header +2 (low nibble dropped, so the offset is a multiple of 0x10), then
 * loads:
 *   r0 = attribute byte at sprite +2 (with the 0xC0 flip bits masked off
 *        when the byte at sprite +1 is nonzero),
 *   r1 = r2 = frame index byte at sprite +0,
 *   r3 = sprite data address.
 * Flags: the CMP against #0 leaves c set and v clear, nz per the tested byte. */
Next sprite_getFrameParameters(void) {
    u32 sprite = R5 + (LD8(R5 + 2) & 0xF0);
    u32 flipTest = LD8(sprite + 1);
    u32 attrs = LD8(sprite + 2);

    SET_FLAGS_CMP(flipTest, 0); /* c = 1, v = 0 regardless of the branch below */
    if (flipTest != 0)
        attrs &= ~0xC0u; /* clear the h/v flip bits */

    R0 = attrs;
    R1 = LD8(sprite);
    R2 = R1; /* the original loads the same byte twice */
    R3 = sprite;
    RETURN_TO_CALLER();
}

/* ── checkSubEntryHiddenFlag_8002E04 ── */
/* checkSubEntryHiddenFlag_8002E04: read an attribute byte from one of an object's sub-entries.
 *
 * r5 points at an object; the high nibble of its byte at +2 selects one of
 * up to 16 sub-entries (16 bytes each, i.e. (byte & 0xF0) + r5). The entry's
 * byte at +3 is tested for bit 3 (a "hidden/disabled" style attribute).
 * Returns that masked value in r0 and the entry address in r3; the NZ flags
 * reflect the masked value, as the original's ANDS does.
 */
typedef struct {
    u32 attr_bit;   /* r0: entry byte & 0x08, source of the NZ flags */
    u32 entry_addr; /* r3: address of the selected sub-entry */
} SubEntryAttr;

static SubEntryAttr read_sub_entry_attr(u32 obj) {
    SubEntryAttr res;
    u32 entry = obj + (LD8(obj + 2) & 0xF0);
    res.entry_addr = entry;
    res.attr_bit = LD8(entry + 3) & 8;
    return res;
}

/* checkSubEntryHiddenFlag_8002E04(r5 = object pointer) -> nz from (entry byte & 8), r3 = entry address */
Next checkSubEntryHiddenFlag_8002E04(void) {
    SubEntryAttr res = read_sub_entry_attr(R5);
    R3 = res.entry_addr;
    SET_FLAGS_NZ(res.attr_bit);
    RETURN_TO_CALLER();
}

/* ── sprite_hasShadow ── */
/* Object struct offsets used here (from the disassembly; the shared header
 * doesn't export them, so define them locally):
 *   +2: type/sprite-select byte -- upper nibble picks which 0x10-byte sprite
 *       struct belongs to this object
 *   +3: sprite state byte within that struct */
#define oObjectHeader_TypeAndSpriteOffset_sprite_hasShadow 2
#define oObjectSprite_SpriteFlags2              3

/* sprite_hasShadow(r5 = object):
 * Mark the object's current sprite as casting a shadow: set bit 0 and clear
 * bit 2 of the sprite state byte.
 * Leaves r0 = new state byte, r1 = 4, r3 = sprite struct address.
 * Flags: N/Z come from the stored byte (the final BIC), while C/V come from
 * the earlier ADDS that built the sprite address -- BIC/ORR leave C/V alone. */
Next sprite_hasShadow(void) {
    u32 select = (LD8(R5 + oObjectHeader_TypeAndSpriteOffset_sprite_hasShadow) >> 4) << 4;

    /* adds r3, r3, r5: this is where C and V are established */
    u32 sprite = select + R5;
    SET_FLAGS_ADD(select, R5);

    u32 state = LD8(sprite + oObjectSprite_SpriteFlags2);
    state |= 1;   /* has shadow */
    state &= ~4;  /* clear the "no shadow" flag */
    ST8(sprite + oObjectSprite_SpriteFlags2, state);
    /* bic only updates N/Z; C/V from the ADDS above must survive */
    SET_FLAGS_NZ(state);

    R0 = state;
    R1 = 4;
    R3 = sprite;
    RETURN_TO_CALLER();
}

/* ── sprite_removeShadow ── */
/* Offsets from the disassembly that game.h doesn't define. */
#define oObjectHeader_TypeAndSpriteOffset_sprite_removeShadow 2 /* header byte: high nibble = sprite struct index */
#define oObjectSprite_Unk_03_sprite_removeShadow              3 /* sprite status byte */

/* sprite_removeShadow(r5 = object header):
 * The high nibble of the header's type byte selects the object's sprite
 * struct (scaled by 16 as the offset from the header). This clears the
 * shadow bit (bit 0) and the bit-2 flag in that sprite's status byte.
 * Leaves r0 = new status byte, r1 = 4, r3 = sprite struct address,
 * nz from the status byte and c/v from the address add. */
Next sprite_removeShadow(void) {
    u32 spriteOffset = LD8(R5 + oObjectHeader_TypeAndSpriteOffset_sprite_removeShadow) & 0xF0;
    u32 sprite = spriteOffset + R5;
    SET_FLAGS_ADD(spriteOffset, R5); /* flags of the original ADDS r3, r3, r5 */

    /* Drop the shadow flag and the bit-2 flag from the sprite status byte. */
    u32 status = LD8(sprite + oObjectSprite_Unk_03_sprite_removeShadow) & ~0x05u;
    SET_FLAGS_NZ(status);
    ST8(sprite + oObjectSprite_Unk_03_sprite_removeShadow, status);

    R0 = status;
    R1 = 4;
    R3 = sprite;
    RETURN_TO_CALLER();
}

/* ── sprite_setColorShader ── */
/* Field offsets used by sprite_setColorShader (not declared in game.h). */
enum { oObjectHeader_TypeAndSpriteOffset_sprite_setColorShader = 2 };

/* sprite_setColorShader(r0 = color shader id, r5 = object header):
 * Pick the sprite block selected by the high nibble of the object's
 * type/sprite-offset byte and store the color shader id into it.
 * Returns r3 = address of that sprite block, with flags from the ADD. */
Next sprite_setColorShader(void) {
    u32 header = R5;
    u32 shaderId = R0;

    /* Sprite block base = header + (typeAndSpriteOffset high nibble << 4) */
    u32 spriteBlock = header + (LD8(header + oObjectHeader_TypeAndSpriteOffset_sprite_setColorShader) & 0xF0);

    ST16(spriteBlock + oObjectSprite_ColorShader, shaderId);

    R3 = spriteBlock;
    SET_FLAGS_ADD(spriteBlock, header); /* ADDS r3, r3, r5 */
    RETURN_TO_CALLER();
}

/* ── sprite_getColorShader ── */
/* sprite_getColorShader(r0 = object header pointer):
 * Returns the color/palette shader halfword of the object's sprite. The sprite
 * data starts at the address given by the high nibble of the header's
 * TypeAndSpriteOffset field (a multiple-of-16 byte offset from the header),
 * and the shader value lives at +6 within it. Result in r0. */
Next sprite_getColorShader(void) {
    u32 header = R0;

    u32 spriteOffset = (LD8(header + 2) >> 4) << 4;
    R0 = LD16(header + spriteOffset + 6);

    RETURN_TO_CALLER();
}

/* ── sprite_zeroColorShader ── */
/* sprite_zeroColorShader(r5 = object header):
 * Compute the address of the object's sprite data (the sprite index in the
 * header's type/sprite byte selects which oObjectSprite struct follows the
 * header) and clear the halfword at offset +6 of that struct, resetting the
 * sprite's color shader / palette effect.
 * Returns r3 = address of the sprite data; c/v reflect the final ADD. */
Next sprite_zeroColorShader(void) {
    /* The low byte at +2 holds the sprite index in its high nibble; the
     * structs are 16 bytes apart, so shift left/right 4 to scale it. */
    u32 sprite = R5 + ((LD8(R5 + 2) >> 4) << 4);

    ST16(sprite + 6, 0);

    R3 = sprite;
    R0 = 0;
    SET_FLAGS_ADD(sprite, R5);
    RETURN_TO_CALLER();
}
