/*
 * npc.c: readable functions of bn6f npc.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── npc_inChatbox_809EADA ── */
/* npc_inChatbox_809EADA(self: r5 = OverworldNPCObject):
 * Overworld NPC state handler for an NPC standing in a chatbox. Runs the
 * action handler picked by CurAction (field at offset 9) out of the table
 * at off_809EAFC (its only entry is npc_inChatbox_curAction_809EB04), then
 * refreshes the NPC's sprite and interaction areas. If bit 0x40 of
 * UnkFlags_60 is set, sub_809F922 runs afterwards as well. */
Next npc_inChatbox_809EADA(void) {
    /* oOverworldNPCObject_CurAction is at offset 9 */
    u32 action = LD8(R5 + 9);

    /* dispatch to the CurAction handler; r0 carries the action index and
     * r7 the table entry, as in the original */
    R0 = action;
    R7 = LD32(A_off_809EAFC + action);
    GUEST_CALL(npc_inChatbox_curAction_809EB04);

    GUEST_CALL(sprite_update);
    GUEST_CALL(npc_createInteractionAreas_809F526);

    R0 = LD32(R5 + oOverworldNPCObject_UnkFlags_60);
    R1 = OW_NPC_UNK_FLAGS_60_0x40;
    if (R0 & R1) {
        GUEST_CALL(sub_809F922);
    } else {
        /* leave NZ as the TST r0, r1 left them (C/V untouched) */
        SET_FLAGS_NZ(R0 & R1);
    }
    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_active_and_visible ── */
/* NPCCommand_set_active_and_visible(r5 = object header, r6 = NPC script pointer):
 * Script command: mark the object as both active and visible by writing the
 * flags byte, then advance the script pointer by one. The r6 increment is the
 * original's last flag-setting instruction, so nz/c/v come from it. */
Next NPCCommand_set_active_and_visible(void) {
    R0 = OBJECT_FLAG_ACTIVE | OBJECT_FLAG_VISIBLE;
    ST8(R5, R0);

    R6 = R6 + 1;
    SET_FLAGS_ADD(R6, 1);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_active_and_invisible ── */
/* NPCCommand_set_active_and_invisible(r5 = object header, r6 = script pointer):
 * Marks the object active (flags byte = 1, the OBJECT_FLAG_ACTIVE bit) and
 * advances the NPC script pointer past this command. Sets flags from the
 * pointer increment; r0 is left holding the flag value written. */
Next NPCCommand_set_active_and_invisible(void) {
    R0 = 1; /* OBJECT_FLAG_ACTIVE */
    ST8(R5, R0);

    R6 = R6 + 1;
    SET_FLAGS_ADD(R6 - 1, 1);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_collision_radius ── */
/* NPCCommand_set_collision_radius(r5 = NPC object, r6 = command stream cursor):
 * read a one-byte radius argument from the NPC script and store it as the
 * object's collision radius, then advance the cursor past opcode + argument.
 * Returns r0 = the radius byte, r6 = advanced cursor; flags reflect r6 += 2. */
Next NPCCommand_set_collision_radius(void) {
    u32 radius = LD8(R6 + 1);
    ST8(R5 + oOverworldNPCObject_CollisionRadius, radius);
    R0 = radius;
    R6 = R6 + 2;
    SET_FLAGS_ADD(R6, 2);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_shift_center ── */
/* NPCCommand_shift_center(r5 = NPC object, r6 = script data pointer):
 * Reads the 3-byte "center offset" payload (X, Y, Z) that follows this script
 * command, copies it into the NPC's center-offset fields, and advances the
 * script data pointer past the command (1 opcode byte + 3 payload bytes).
 * Leaves r0 = Z offset, r1 = 3, flags from "adds r6, #4". */
Next NPCCommand_shift_center(void) {
    s8 centerX = (s8)LD8(R6 + 1);
    s8 centerY = (s8)LD8(R6 + 2);
    s8 centerZ = (s8)LD8(R6 + 3);

    ST8(R5 + oOverworldNPCObject_CenterOffsetX, centerX);
    ST8(R5 + oOverworldNPCObject_CenterOffsetY, centerY);
    ST8(R5 + oOverworldNPCObject_CenterOffsetZ, centerZ);

    R0 = (s8)centerZ;   /* ldrsb sign-extends; last value stays in r0 */
    R1 = 3;
    {
        u32 oldPtr = R6;
        R6 = oldPtr + 4;
        SET_FLAGS_ADD(oldPtr, 4);
    }

    RETURN_TO_CALLER();
}

/* ── NPCCommand_disable_npc_interaction ── */
/* NPCCommand_disable_npc_interaction(r5 = NPC object, r6 = script instruction pointer):
 * sets the NPC's "disable interaction" flag and advances the script by one command.
 * Leaves r0 = flag bit, r1 = updated flags value, r6 = next instruction index. */

Next NPCCommand_disable_npc_interaction(void) {
    R0 = OW_NPC_UNK_FLAGS_60_DISABLE_INTERACTION;

    u32 flags = LD32(R5 + oOverworldNPCObject_UnkFlags_60);
    flags |= R0;
    ST32(R5 + oOverworldNPCObject_UnkFlags_60, flags);
    R1 = flags;

    R6 = R6 + 1;
    SET_FLAGS_ADD(R6, 1);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_npc_palette_index ── */
/* NPCCommand_set_npc_palette_index (r5 = NPC object, r6 = script pointer):
 * Reads the palette byte from the NPC script command stream, stores it into
 * the NPC's palette index field, then advances the script pointer past the
 * command. Returns with r0 = the byte read, r6 = updated script pointer,
 * and flags from the pointer increment (ADDS r6, #2). */
Next NPCCommand_set_npc_palette_index(void) {
    u8 palette = LD8(R6 + 1);

    ST8(R5 + oOverworldNPCObject_PaletteIndex, palette);
    R0 = palette;
    R6 += 2;
    SET_FLAGS_ADD(R6, 2);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_init_hop ── */
/* NPCCommand_init_hop(r5 = NPC object, r6 = current script command pointer):
 * "hop" script command — read the hop speed and hop distance from the two
 * bytes following the opcode, put the NPC into the hop/leap action state,
 * clear its movement flags, and disable its script. The script pointer is
 * then advanced past the 3-byte command (r6 += 3), and NZC are left from
 * that ADDS; r0 ends as whatever the script-disable call returned. */

Next NPCCommand_init_hop(void) {
    ST8(R5 + 4 /* oOverworldNPCObject_MovementSpeed */, LD8(R6 + 1));
    ST8(R5 + 5 /* oOverworldNPCObject_MovementDistance */, LD8(R6 + 2));

    ST8(R5 + 9 /* oOverworldNPCObject_CurAction */, MOVEMENT_FLAG_HOP_OR_LEAP);
    ST16(R5 + oOverworldNPCObject_MovementFlag_0a_Unk_0b, 0);

    GUEST_CALL(npc_disableScript0x19_809f51e);

    u32 oldPtr = R6;
    R6 = oldPtr + 3;
    SET_FLAGS_ADD(oldPtr, 3);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_do_not_face_player_when_interacted ── */
/* NPCCommand_do_not_face_player_when_interacted(r5 = NPC object, r6 = script counter):
 * Sets the "face player when interacted" flag (bit 1 of UnkFlags_60) on the NPC
 * and advances the script counter. Flags come from the counter increment. */

Next NPCCommand_do_not_face_player_when_interacted(void) {
    R0 = OW_NPC_UNK_FLAGS_60_FACE_PLAYER_WHEN_INTERACTED;
    R1 = LD32(R5 + oOverworldNPCObject_UnkFlags_60) | R0;
    ST32(R5 + oOverworldNPCObject_UnkFlags_60, R1);
    R6 = R6 + 1;
    SET_FLAGS_ADD(R6, 1);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_coords ── */
/* NPCCommand_set_coords(r5 = NPC object, r6 = NPC script pointer):
 * Script command that sets the NPC's position. Reads three halfwords from
 * the script (at offsets +1, +3, +5) — X, Y and Z — and stores each into the
 * NPC object as a fixed-point coordinate (halfword shifted into the upper 16
 * bits). Advances the script pointer past the command (7 bytes: opcode +
 * three halfwords). Flags on exit reflect the pointer update (ADDS r6, #7). */
Next NPCCommand_set_coords(void) {
    u32 script = R6;

    R0 = script + 1;
    GUEST_CALL(ReadNPCScriptHalfword);
    R0 <<= 16;
    ST32(R5 + oOverworldNPCObject_X, R0);

    R0 = script + 3;
    GUEST_CALL(ReadNPCScriptHalfword);
    R0 <<= 16;
    ST32(R5 + oOverworldNPCObject_Y, R0);

    R0 = script + 5;
    GUEST_CALL(ReadNPCScriptHalfword);
    R0 <<= 16;
    ST32(R5 + oOverworldNPCObject_Z, R0);

    /* add r6, #7 (flag-setting): advance the script read pointer */
    u32 prev = R6;
    R6 = prev + 7;
    SET_FLAGS_ADD(prev, 7);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_move_in_direction ── */
/* Offsets/constants used here that aren't in game.h (from the bn6f disassembly). */
#define oOverworldNPCObject_MovementSpeed   0x04 /* NPC movement speed byte        */
#define oOverworldNPCObject_MovementDistance 0x05 /* NPC movement distance byte   */
#define oOverworldNPCObject_CurAction       0x09 /* NPC current action byte        */
#define MOVEMENT_FLAG_DEFAULT_MOVING        0x04 /* default "moving" action id     */

/* NPCCommand_move_in_direction (0x0809EEDA), r5 = NPC object, r6 = script stream.
 *
 * Interprets a "move in direction" script command. The three parameter bytes
 * that follow the opcode (direction, speed, distance) are copied into the NPC's
 * movement state, the NPC is switched to its default moving action with the
 * movement flags cleared, the NPC's script is disabled, and the stream pointer
 * is advanced past the command (opcode + 3 parameter bytes).
 *
 * Register notes: r2 is left holding the last byte read (the distance), the
 * ADDS on r6 sets the flags, and r0 comes back from the callee. */
Next NPCCommand_move_in_direction(void) {
    u32 npc = R5;
    u32 cmd = R6;
    u32 direction, speed, distance;

    direction = LD8(cmd + 1);
    speed     = LD8(cmd + 2);
    distance  = LD8(cmd + 3);

    ST8(npc + oOverworldNPCObject_MovementDirection, direction);
    ST8(npc + oOverworldNPCObject_MovementSpeed, speed);
    ST8(npc + oOverworldNPCObject_MovementDistance, distance);

    ST8(npc + oOverworldNPCObject_CurAction, MOVEMENT_FLAG_DEFAULT_MOVING);
    ST16(npc + oOverworldNPCObject_MovementFlag_0a_Unk_0b, 0);

    GUEST_CALL(npc_disableScript0x19_809f51e);

    /* Advance the script stream past this command; the ADDS sets the flags
     * the callers branch on, and r2 keeps the last byte loaded. */
    R2 = distance;
    R6 = cmd + 4;
    SET_FLAGS_ADD(cmd, 4);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_animation ── */
/* NPCCommand_set_animation(r5 = NPC object, r6 = script command pointer):
 * Script opcode handler: read the animation ID byte that follows the opcode,
 * store it into the NPC's animation selector, and advance the script pointer
 * past the opcode and its argument. Flags reflect the pointer increment. */
Next NPCCommand_set_animation(void) {
    u32 animation_id = LD8(R6 + 1);

    ST8(R5 + oOverworldNPCObject_AnimationSelect, animation_id);
    R0 = animation_id;

    R6 += 2;
    SET_FLAGS_ADD(R6, 2);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_sprite ── */
/* Struct-layout constants for the OverworldNPCSpriteVariations lookup table
 * (entries of {u8 SpriteId, u8 HiddenOAMPieces, u8 PaletteIndex}, 3 bytes each,
 * terminated by a SpriteId of 0xFF). */
#define oOverworldNPCSpriteVariations_SpriteId        0
#define oOverworldNPCSpriteVariations_HiddenOAMPieces 1
#define oOverworldNPCSpriteVariations_PaletteIndex    2
#define oOverworldNPCSpriteVariations_Size            3

/* NPCCommand_set_sprite (0809EF00):
 * r5 = overworld NPC object, r6 = pointer into the NPC command stream.
 * The command's second byte (at [r6+1]) is a sprite id: it is written to the
 * object and looked up in OverworldNPCSpriteVariations. Special sprites
 * override the hidden-OAM piece count and palette index; the 0xFF terminator
 * (or any other sprite) leaves the defaults. The sprite category is forced to
 * 0x18 and the shadow flag is cleared. r6 is advanced past the 2-byte command;
 * the trailing ADDS sets nz/c/v for the caller. */
Next NPCCommand_set_sprite(void) {
    u32 npc = R5;
    u32 cmd = R6;

    u32 spriteId = LD8(cmd + 1);
    ST16(npc + oOverworldNPCObject_NPCSprite, spriteId);
    ST8(npc + oOverworldNPCObject_PaletteIndex, 0);
    ST32(npc + oOverworldNPCObject_HiddenOAMPieces, 0);

    u32 table = A_OverworldNPCSpriteVariations;
    u32 entry = oOverworldNPCSpriteVariations_SpriteId;
    u8 id;
    for (;;) {
        id = LD8(table + entry);
        R2 = id;
        if (id == spriteId || id == 0xFF)
            break;
        entry += oOverworldNPCSpriteVariations_Size;
    }

    if (id == spriteId) {
        /* Special sprite: copy its hidden-OAM count (stored shifted into the
         * top byte, as the original's lsl r0,#24 does) and palette index. */
        R3 = entry + oOverworldNPCSpriteVariations_HiddenOAMPieces;
        R0 = LD8(table + R3) << 24;
        ST32(npc + oOverworldNPCObject_HiddenOAMPieces, R0);
        R3 = entry + oOverworldNPCSpriteVariations_PaletteIndex;
        R0 = LD8(table + R3);
        ST8(npc + oOverworldNPCObject_PaletteIndex, R0);
    } else {
        R3 = entry;
    }

    R0 = 0x18;
    ST32(npc + oOverworldNPCObject_NPCSpriteCategory, R0);

    R0 = LD32(npc + oOverworldNPCObject_UnkFlags_60);
    R1 = OW_NPC_UNK_FLAGS_60_SHADOW_FLAG_0x80;
    R0 &= ~R1;
    ST32(npc + oOverworldNPCObject_UnkFlags_60, R0);

    R6 = cmd + 2;
    SET_FLAGS_ADD(cmd, 2);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_text_script_index ── */
/* NPCCommand_set_text_script_index(r5 = NPC object, r6 = script cursor):
 * Read the byte after the current opcode (the text script index), store it in
 * the NPC's TextScriptIndex field, and advance the cursor past both bytes.
 * Returns: r0 = the index that was read, r6 = cursor advanced by 2.
 * Flags reflect the cursor increment. */
Next NPCCommand_set_text_script_index(void) {
    u32 script_index = LD8(R6 + 1);
    ST8(R5 + oOverworldNPCObject_TextScriptIndex, script_index);
    R6 += 2;
    SET_FLAGS_ADD(R6 - 2, 2);
    R0 = script_index;
    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_layer_priority_override_to_2 ── */
/* NPCCommand_set_layer_priority_override_to_2: an NPC script command that
 * sets the NPC's layer priority override to 2 (r5 = NPC object), then
 * advances the script cursor (r6). Leaves r0 = 2 and flags from r6 + 1. */
Next NPCCommand_set_layer_priority_override_to_2(void) {
    ST32(R5 + oOverworldNPCObject_LayerPriorityOverride, 2);
    R0 = 2;
    R6 = R6 + 1;
    SET_FLAGS_ADD(R6, 1);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_write_hidden_oam_pieces ── */
/* NPCCommand_write_hidden_oam_pieces(r5 = NPC object, r6 = script pointer):
 * Script opcode handler: reads a 32-bit operand from the NPC script and stores
 * it in the NPC's "hidden OAM pieces" field, then skips past the opcode byte
 * and the 4-byte operand (script pointer += 5).
 * Returns the operand in r0; flags come from the pointer update. */
Next NPCCommand_write_hidden_oam_pieces(void) {
    /* The word operand starts one byte after the opcode. */
    R0 = R6 + 1;
    GUEST_CALL(ReadNPCScriptWord);

    ST32(R5 + oOverworldNPCObject_HiddenOAMPieces, R0);

    R6 += 5; /* opcode byte + 32-bit operand */
    SET_FLAGS_ADD(R6, 5);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_disable_collision ── */
/* NPCCommand_disable_collision(r5 = NPC object):
 * Sets the NPC's "disable collision" flag so the player can walk through it,
 * then advances the NPC script program counter (r6).
 * r0 = flag value, r1 = new flags word; nz/c/v come from the final "adds r6, #1". */


Next NPCCommand_disable_collision(void) {
    u32 flags = LD32(R5 + oOverworldNPCObject_UnkFlags_60);

    R0 = OW_NPC_UNK_FLAGS_60_DISABLE_COLLISION;
    R1 = flags | R0;
    ST32(R5 + oOverworldNPCObject_UnkFlags_60, R1);

    R6 += 1;
    SET_FLAGS_ADD(R6, 1);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_give_attached_shadow ── */
/* NPCCommand_give_attached_shadow (0x0809EFB4)
 * NPC script command: detach the NPC's shadow. Clears the shadow flag in the
 * NPC's flags_60 word, tells the sprite layer to stop drawing a shadow, then
 * advances the script instruction pointer (r6) by one. The flags left behind
 * are those of that pointer increment.
 *
 * inputs:  r5 = NPC object, r6 = script instruction pointer, lr
 * outputs: nz c v (from ADDS r6, #1), r0/r1/r3 as left by sprite_noShadow,
 *          r6 = pointer + 1, lr = return address of the sprite_noShadow call */
Next NPCCommand_give_attached_shadow(void) {
    u32 npc = R5;

    /* clear the "has attached shadow" bit */
    u32 flags = LD32(npc + oOverworldNPCObject_UnkFlags_60);
    flags &= ~OW_NPC_UNK_FLAGS_60_SHADOW_FLAG_0x80;
    ST32(npc + oOverworldNPCObject_UnkFlags_60, flags);

    sprite_noShadow();

    /* advance the script pointer; ADDS sets nz/c/v for the caller */
    u32 next = R6 + 1;
    SET_FLAGS_ADD(R6, 1);
    R6 = next;

    RETURN_TO_CALLER();
}

/* ── NPCCommand_give_detatched_shadow ── */
/* NPCCommand_give_detatched_shadow(r5 = NPC object, r6 = command stream position):
 * NPC script command that detaches the NPC's shadow: sets the shadow flags
 * (0x100 | 0x80) in the object's UnkFlags_60, then calls sprite_hasShadow to
 * set the detached shadow up. Advances the command stream position by one
 * (flags reflect the ADDS increment). */
Next NPCCommand_give_detatched_shadow(void) {
    /* ldr r0, =(...) — the shadow flag pair; only the 0x80 macro exists in this
     * build's headers, so spell the pair out as 0x100 | ..._FLAG_0x80 (== 0x180). */
    u32 shadowFlags = 0x100 | OW_NPC_UNK_FLAGS_60_SHADOW_FLAG_0x80;

    u32 flags = LD32(R5 + oOverworldNPCObject_UnkFlags_60);
    ST32(R5 + oOverworldNPCObject_UnkFlags_60, flags | shadowFlags);

    /* sprite_hasShadow(r5 = NPC object, r6 = command stream position) */
    GUEST_CALL(sprite_hasShadow);

    /* add r6, #1 — the original's ADDS sets the flags */
    R6 = R6 + 1;
    SET_FLAGS_ADD(R6 - 1, 1);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_sprite_with_category ── */
/* NPCCommand_set_sprite_with_category (r5 = NPC object, r6 = command stream cursor):
 * Handles the "set NPC sprite" command: the next two bytes of the stream give the
 * sprite index (stored as a halfword) and sprite category (stored as a word).
 * Also clears the object's "is a shadow" flag. Advances the cursor past the
 * operands (3 bytes). Returns the updated flags word in r0, the shadow flag
 * bit in r1, and the advanced cursor in r6; flags reflect (r6 + 3). */
Next NPCCommand_set_sprite_with_category(void) {
    u32 npc = R5;
    u32 cmd = R6;

    /* Sprite index: halfword field in the NPC object. */
    u32 sprite = LD8(cmd + 1);
    ST16(npc + oOverworldNPCObject_NPCSprite, sprite);

    /* Sprite category: word field in the NPC object. */
    ST32(npc + oOverworldNPCObject_NPCSpriteCategory, (u32)LD8(cmd + 2));

    /* This sprite is a regular one, not a shadow: clear the shadow flag. */
    u32 flags = LD32(npc + oOverworldNPCObject_UnkFlags_60);
    flags &= ~OW_NPC_UNK_FLAGS_60_SHADOW_FLAG_0x80;
    ST32(npc + oOverworldNPCObject_UnkFlags_60, flags);

    /* Advance the command cursor past opcode + operands, then return. */
    u32 nextCmd = cmd + 3;
    SET_FLAGS_ADD(cmd, 3);

    R0 = flags;
    R1 = OW_NPC_UNK_FLAGS_60_SHADOW_FLAG_0x80;
    R6 = nextCmd;

    RETURN_TO_CALLER();
}

/* ── NPCCommand_disable_collision_alternate ── */
/* NPCCommand_disable_collision_alternate(r5 = NPC object):
 * Sets the NPC's "collision disabled" flag (0x10) in its UnkFlags_60 field,
 * then advances the NPC script command counter in r6. Flags are those left
 * by the final ADDS (r6 + 1). r0 = flag bit, r1 = updated flags. */
Next NPCCommand_disable_collision_alternate(void) {
    R0 = OW_NPC_UNK_FLAGS_60_DISABLE_COLLISION_0x10;
    R1 = LD32(R5 + oOverworldNPCObject_UnkFlags_60) | R0;
    ST32(R5 + oOverworldNPCObject_UnkFlags_60, R1);
    R6 = R6 + 1;
    SET_FLAGS_ADD(R6, 1);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_play_sound ── */
/* NPCCommand_play_sound (0809F048): "play sound" NPC script command.
 * r6 points at the script instruction: a one-byte opcode followed by a
 * halfword sound effect id. Read the id, play it, and advance r6 past the
 * whole 3-byte instruction. The caller inspects the flags left by the final
 * "adds r6, r6, #3". */
Next NPCCommand_play_sound(void) {
    u32 scriptPos = R6;

    R0 = scriptPos + 1;                 /* id halfword starts after the opcode */
    GUEST_CALL(ReadNPCScriptHalfword);  /* r0 = sound effect id */
    GUEST_CALL(PlaySoundEffect);

    SET_FLAGS_ADD(scriptPos, 3);
    R6 = scriptPos + 3;                 /* skip opcode + halfword */
    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_alpha ── */
/* NPCCommand_set_alpha (0x0809F15A): NPC script command handler.
 *
 * First turns alpha blending off on the NPC's sprite (sprite_disableAlpha).
 * Then it reads the command's parameter byte at [r6+1] into r0; if it is
 * nonzero, that value is applied as the new alpha level via
 * sprite_setAlpha_8002c7a (overriding the earlier disable).
 *
 * r6 points at the current script command and is advanced past the 2-byte
 * command on exit. The final flags come from `adds r6, #2`. */
Next NPCCommand_set_alpha(void) {
    GUEST_CALL(sprite_disableAlpha);

    R0 = LD8(R6 + 1);           /* parameter byte; r0 is set even if zero */
    if (R0 != 0)
        GUEST_CALL(sprite_setAlpha_8002c7a);

    SET_FLAGS_ADD(R6, 2);
    R6 += 2;
    RETURN_TO_CALLER();
}

/* ── NPCCommand_run_secondary_script ── */
/* NPCCommand_run_secondary_script (0x0809F16E):
 * Script opcode handler. R6 points just past the opcode byte; read the 32-bit
 * address stored there (at R6+1) and install it as the NPC's secondary script
 * pointer, then advance the script cursor past the opcode + operand (5 bytes).
 * Inputs:  R5 = NPC object, R6 = script cursor, LR = return address.
 * Outputs: R0..R3 as left by ReadNPCScriptWord, R6 advanced, flags from the
 *          final cursor increment. */
Next NPCCommand_run_secondary_script(void) {
    R0 = R6 + 1;
    GUEST_CALL(ReadNPCScriptWord);
    ST32(R5 + oOverworldNPCObject_UnkNPCScriptPtr_5c, R0);
    R6 += 5;
    SET_FLAGS_ADD(R6, 5);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_pause_secondary_script ── */
/* NPCCommand_pause_secondary_script(r5 = NPC object, r6 = script instruction pointer):
 * Script command "pause": clears the NPC's pause-related byte, reads the pause
 * length from the operand byte into the object's timer, flags the script to
 * stop running, and steps the pointer past the opcode+operand.
 * Returns r0 = 1, r6 = script pointer advanced by 2, and nz/c/v as set by the
 * ADDS r6, #2 pointer increment. */
Next NPCCommand_pause_secondary_script(void) {
    ST8(R5 + oOverworldNPCObject_Unk_0b, 0);

    R0 = LD8(R6 + 1);
    ST16(R5 + oOverworldNPCObject_Timer_22, R0);

    R0 = 1;
    ST8(R5 + oOverworldNPCObject_TerminateScript_1f, R0);

    SET_FLAGS_ADD(R6, 2);
    R6 += 2;

    RETURN_TO_CALLER();
}

/* ── NPCCommand_end_secondary_script ── */
/* NPCCommand_end_secondary_script(r5 = NPC object):
 * Terminate the NPC's secondary (event) script: clear its script pointer and
 * script timer, clear r6 to signal the interpreter that the script ended, and
 * return r0 = NULL. Flags reflect the final MOV (r6 == 0). */
Next NPCCommand_end_secondary_script(void) {
    R0 = 0;
    ST32(R5 + oOverworldNPCObject_UnkNPCScriptPtr_5c, R0);
    ST16(R5 + oOverworldNPCObject_Timer_22, R0);
    R6 = 0;
    SET_FLAGS_NZ(R6);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_jump_with_link ── */
/* NPCCommand_jump_with_link(r5 = NPC object, r6 = script program counter):
 * "call subroutine" opcode of the NPC script VM. Stores the address of the
 * next opcode (PC + 5) in the NPC's link register, then fetches the 32-bit
 * destination address located at PC + 1 and resumes the script there.
 * Leaves r0 = r6 = destination, r4 = link register field offset, and the
 * flags as the final "mov r6, r0" (adds r6, r0, #0) set them. */
Next NPCCommand_jump_with_link(void) {
    /* Link register := return address (opcode is 1 byte + 4-byte operand). */
    R4 = oOverworldNPCObject_LinkRegister;
    ST32(R5 + R4, R6 + 5);

    /* Read the jump destination and continue the script there. */
    R0 = R6 + 1;
    GUEST_CALL(ReadNPCScriptWord);
    R6 = R0;
    SET_FLAGS_ADD(R6, 0);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_init_movement ── */
/* NPCCommand_init_movement(r5 = NPC object, r6 = pointer to the "init movement"
 * command in the NPC's script):
 *
 * Command layout in the script:
 *   +0: opcode
 *   +1: movement direction
 *   +2: movement speed
 *   +3: movement distance
 *   +4: start of the next script command
 *
 * Copies the movement parameters into the NPC object, points the object's
 * script link register past this command, then reads the next script word
 * (the command's argument) via ReadNPCScriptWord.
 *
 * Outputs: r0 = next script word (also in r6), r1-r3 = whatever
 * ReadNPCScriptWord leaves, r4 = oOverworldNPCObject_MovementDistanceReload,
 * flags set from the new r6. */
Next NPCCommand_init_movement(void) {
    u32 npc = R5;
    u32 script = R6;

    /* the NPC's script resumes right after this command's fixed part */
    ST32(npc + oOverworldNPCObject_LinkRegister, script + 8);

    ST8(npc + oOverworldNPCObject_MovementDirectionReload, LD8(script + 1));
    ST8(npc + oOverworldNPCObject_MovementSpeedReload,     LD8(script + 2));
    ST8(npc + oOverworldNPCObject_MovementDistanceReload,  LD8(script + 3));

    R4 = oOverworldNPCObject_MovementDistanceReload;

    /* fetch the word following the command */
    R0 = script + 4;
    GUEST_CALL(ReadNPCScriptWord);

    R6 = R0;
    SET_FLAGS_ADD(R0, 0);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_change_movement_direction ── */
/* NPCCommand_change_movement_direction (0x0809F23E):
 * NPC script command that makes the NPC start walking in a new direction.
 *   r5 = NPC object, r6 = current script read position.
 * The operand at [r6+1] is a turn amount relative to the current movement
 * direction (0 = 0 degrees, 2 = 90 degrees, 4 = 180 degrees, 6 = 270 degrees).
 * After re-aiming, the NPC's speed/distance reload values are copied into the
 * active movement parameters and its action is set to "walking".
 * Leaves r2 = movement distance, r4 = movement distance reload offset (as the
 * original does), r0 = whatever the disable-script helper returns, and
 * flags from `add r6, #2`. */
Next NPCCommand_change_movement_direction(void) {
    u32 turn = LD8(R6 + 1);

    /* new direction = current direction + turn amount, wrapped to 8
     * directions (0, 2, 4, 6 don't actually move the NPC: the movement
     * offsets for those directions are undefined) */
    R4 = oOverworldNPCObject_MovementDirectionReload;
    u32 direction = LD8(R5 + R4) + turn;
    direction &= 7;
    ST8(R5 + oOverworldNPCObject_MovementDirection, direction);

    /* transfer movement speed and distance reload values into the active
     * slots (oOverworldNPCObject_MovementSpeed = 4,
     * oOverworldNPCObject_MovementDistance = 5) */
    R2 = LD8(R5 + oOverworldNPCObject_MovementSpeedReload);
    ST8(R5 + 4, R2);

    R2 = LD8(R5 + oOverworldNPCObject_MovementDistanceReload);
    ST8(R5 + 5, R2);
    R4 = oOverworldNPCObject_MovementDistanceReload; /* r4 left as in the original */

    /* set the action to "default moving" (MOVEMENT_FLAG_DEFAULT_MOVING = 4,
     * oOverworldNPCObject_CurAction = 9) and clear the movement state */
    ST8(R5 + 9, 4);
    ST16(R5 + oOverworldNPCObject_MovementFlag_0a_Unk_0b, 0);

    GUEST_CALL(npc_disableScript0x19_809f51e);

    /* advance the script read position past the command and its operand,
     * setting flags from the add as the original does */
    SET_FLAGS_ADD(R6, 2);
    R6 = R6 + 2;

    RETURN_TO_CALLER();
}

/* ── NPCCommand_jump_if_progress_in_range ── */
/* NPCCommand_jump_if_progress_in_range: 0809F270-0809F292 THUMB, bn6f npc.s:2339
 * NPC script opcode: "jump if the game progress is within a range".
 * Script operands at r6: +1 = minimum progress, +2 = maximum progress,
 * +3..+4 = 16-bit jump target (fetched via ReadNPCScriptWord).
 * If min <= game progress <= max, r6 is set to the jump target word;
 * otherwise r6 is advanced past the whole 7-byte command.
 * r0 is the game progress on the skip path, the fetched target on the jump
 * path; r1/r2 hold the bounds on the paths where they were loaded. */
Next NPCCommand_jump_if_progress_in_range(void) {
    u32 scriptPtr = R6;
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 progress = LD8(gameState + 6);      /* oGameState_GameProgress */

    R0 = progress;
    R1 = LD8(scriptPtr + 1);                /* minimum progress */
    if ((s32)progress >= (s32)R1) {
        R2 = LD8(scriptPtr + 2);            /* maximum progress */
        if ((s32)progress <= (s32)R2) {
            /* In range: read the 16-bit target word following the operands. */
            R0 = scriptPtr + 3;
            GUEST_CALL(ReadNPCScriptWord);
            R6 = R0;                        /* movs r6, r0: sets flags like adds r6, r0, #0 */
            SET_FLAGS_ADD(R0, 0);
            RETURN_TO_CALLER();
        }
    }
    /* Out of range: skip this 7-byte command. */
    R6 = scriptPtr + 7;
    SET_FLAGS_ADD(scriptPtr, 7);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_write_cutscene_var ── */
/* NPCCommand_write_cutscene_var: "setvar" opcode handler for cutscene scripts.
 *
 * R6 points into the running cutscene script at the current opcode:
 *   [opcode][var index][value]
 * It stores the value byte into the cutscene's variable buffer at the given
 * index, then advances the script pointer past all three bytes.
 *
 * Outputs: R0 = variable index, R1 = value, R4 = cutscene state pointer,
 * R6 = advanced script pointer, flags set by the pointer update (ADDS r6,#3).
 */
Next NPCCommand_write_cutscene_var(void) {
    u32 cutsceneState = LD32(R10 + oToolkit_CutsceneStatePtr);
    u32 varIndex = LD8(R6 + 1);
    u32 value = LD8(R6 + 2);

    ST8(cutsceneState + varIndex, value);

    R0 = varIndex;
    R1 = value;
    R4 = cutsceneState;
    R6 += 3;
    SET_FLAGS_ADD(R6, 3);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_wait_cutscene_var ── */
/* NPCCommand_wait_cutscene_var (0x0809F2DE):
 * NPC script command "wait for cutscene variable". Puts the NPC into the
 * WAIT_CUTSCENE_VAR action (its script stays paused until the watched
 * cutscene variable reaches the given value) and stores the two operand
 * bytes from the script: the variable index and the value to wait for.
 * Inputs:  r5 = NPC object, r6 = script read cursor (operand bytes at +1/+2)
 * Outputs: r6 advanced past the command, flags from `adds r6, #3`, r0 from
 *          the called helper; r7 preserved. */
Next NPCCommand_wait_cutscene_var(void) {
    /* Enter the wait-for-cutscene-var state and clear the movement flag.
     * Offset 9 is the NPC object's current-action byte (CurAction). */
    R0 = MOVEMENT_FLAG_WAIT_CUTSCENE_VAR;
    ST8(R5 + 9, R0);

    R0 = 0;
    ST16(R5 + oOverworldNPCObject_MovementFlag_0a_Unk_0b, R0);

    /* Operand 1: index of the cutscene variable to watch. */
    R0 = LD8(R6 + 1);
    ST32(R5 + oOverworldNPCObject_CutsceneVarIndexToWaitFor, R0);

    /* Operand 2: value the variable must reach. */
    R0 = LD8(R6 + 2);
    ST32(R5 + oOverworldNPCObject_CutsceneVarValueToWaitFor, R0);

    /* Mark the NPC's script as suspended (action 0x19 handling). */
    GUEST_CALL(npc_disableScript0x19_809f51e);

    /* Consume the command (opcode byte + two operands). */
    R6 += 3;
    SET_FLAGS_ADD(R6, 3);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_wait_mystery_data_taken ── */
/* NPCCommand_wait_mystery_data_taken (0x0809F338):
 * NPC script command handler for "wait until the mystery data has been taken".
 * Marks the NPC as waiting (CurAction = MOVEMENT_FLAG_WAIT_MYSTERY_DATA_TAKEN),
 * clears its movement flags, reads the mystery data id (halfword at script
 * pointer + 1) and stores it into the NPC object, disables the NPC's script
 * processing, then advances the script pointer past the command
 * (opcode + 2-byte id = 3 bytes).
 *
 * inputs:  R5 = NPC object, R6 = script pointer
 * outputs: R6 = script pointer + 3, flags from that ADDS; R0/R1 as left by
 *          the callees; R7 preserved (the original pushes/pops it, but only
 *          uses it as a scratch offset register, so we never touch it). */
Next NPCCommand_wait_mystery_data_taken(void) {
    /* CurAction lives at offset 9 in the NPC object (no named macro). */
    ST8(R5 + 9, MOVEMENT_FLAG_WAIT_MYSTERY_DATA_TAKEN);
    ST16(R5 + oOverworldNPCObject_MovementFlag_0a_Unk_0b, 0);

    R0 = R6 + 1;
    GUEST_CALL(ReadNPCScriptHalfword);
    ST32(R5 + oOverworldNPCObject_MysteryDataId, R0);

    GUEST_CALL(npc_disableScript0x19_809f51e);

    {
        u32 prev = R6;
        R6 = prev + 3;
        SET_FLAGS_ADD(prev, 3);
    }

    RETURN_TO_CALLER();
}

/* ── NPCCommand_play_music ── */
/* NPCCommand_play_music (0809F36E):
 * Script command handler; r6 points at the current NPC script command.
 * Reads a 16-bit song ID at r6+1, starts it playing, then advances the
 * script pointer by 3 bytes (opcode + halfword). Leaves NZCV from the
 * final pointer update, as the original's ADDS does. */
Next NPCCommand_play_music(void) {
    R0 = R6 + 1;
    GUEST_CALL(ReadNPCScriptHalfword); /* R0 = song id */
    GUEST_CALL(PlayMusic);

    {
        u32 next = R6 + 3;
        SET_FLAGS_ADD(R6, 3);
        R6 = next;
    }
    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_text_script_index_and_ptr_to_decomp_buffer ── */
/* NPCCommand_set_text_script_index_and_ptr_to_decomp_buffer (r5 = NPC object,
 * r6 = pointer to the command's arguments):
 * Text script start command. Reads the script index byte that follows the
 * opcode, stores it in the NPC, marks the NPC as having a text script
 * (chatbox flag 0x400 in UnkFlags_60) and points its script pointer at the
 * text decompression buffer at byte_202FA04. Advances r6 past the command
 * (opcode + index byte) and leaves the ADDS flags for callers to branch on. */
Next NPCCommand_set_text_script_index_and_ptr_to_decomp_buffer(void) {
    u32 npc = R5;
    u32 cmd = R6;

    ST8(npc + oOverworldNPCObject_TextScriptIndex, LD8(cmd + 1));

    ST32(npc + oOverworldNPCObject_UnkFlags_60,
         LD32(npc + oOverworldNPCObject_UnkFlags_60) | 0x400);

    ST32(npc + oOverworldNPCObject_TextScriptPtr, A_byte_202FA04);

    R6 = cmd + 2;
    SET_FLAGS_ADD(cmd, 2);

    /* Registers the original happens to leave behind: */
    R0 = 0x400;
    R1 = oOverworldNPCObject_TextScriptPtr;
    R3 = A_byte_202FA04;

    RETURN_TO_CALLER();
}

/* ── setNPCScript_809F506 ── */
/* setNPCScript_809F506(r0 = script, r5 = npc):
 * Assign a new animation/script to an overworld NPC and reset its
 * script-run state (termination flags, chat flag, pending script ptr). */
Next setNPCScript_809F506(void) {
    ST32(R5 + oOverworldNPCObject_AnimationScriptPtr, R0);

    ST8(R5 + oOverworldNPCObject_TerminateScript_19, 0);
    ST8(R5 + oOverworldNPCObject_ChatTriggered, 0);
    ST8(R5 + oOverworldNPCObject_TerminateScript_1f, 0);
    ST32(R5 + oOverworldNPCObject_UnkNPCScriptPtr_5c, 0);

    RETURN_TO_CALLER();
}

/* ── npc_beginChatboxState_809F5FC ── */
/* Offsets not present in game.h; taken from the disassembly. */
#define oOverworldNPCObject_CurState   8   /* byte  */
#define oOverworldNPCObject_CurAction  9   /* byte  */
#define NPC_CUR_STATE_IN_CHATBOX       8

/* npc_beginChatboxState_809F5FC (r5 = overworld NPC object):
 * Begin talking to an NPC: back up its current action and movement flags,
 * switch it to the "in chatbox" state, and clear the action/movement
 * fields while the conversation runs. */
Next npc_beginChatboxState_809F5FC(void) {
    ST8(R5 + oOverworldNPCObject_Unk_1d,
        LD8(R5 + oOverworldNPCObject_CurAction));
    ST8(R5 + oOverworldNPCObject_Unk_1e,
        LD8(R5 + oOverworldNPCObject_MovementFlag_0a));
    ST8(R5 + oOverworldNPCObject_CurState, NPC_CUR_STATE_IN_CHATBOX);
    ST8(R5 + oOverworldNPCObject_CurAction, 0);
    ST16(R5 + oOverworldNPCObject_MovementFlag_0a_Unk_0b, 0);
    RETURN_TO_CALLER();
}

/* ── npc_resetToStandardState_809F612 ── */
/* Field offsets / constants used here that the shared headers don't define
 * (from the bn6f disassembly of the overworld NPC object). */
#define oOverworldNPCObject_CurState      8
#define oOverworldNPCObject_CurAction_sub_809F612     9
#define NPC_CUR_STATE_STANDARD            4
#define OW_NPC_UNK_FLAGS_60_0x1000        0x1000
#define OW_NPC_UNK_FLAGS_60_FACE_PLAYER_WHEN_INTERACTED_sub_809F612 0x0002

/* npc_resetToStandardState_809F612(r5 = overworld NPC object):
 * Put an NPC back into its standard state: restore the current action and
 * movement flag stashed in its object, reselect its animation (unless the
 * object's UnkFlags_60 bits say to skip that), and clear the chat-triggered
 * flag. Ends with r0 = 0 and the flags left by `mov r0, #0`. */
Next npc_resetToStandardState_809F612(void) {
    u32 returnAddr = LR;

    ST8(R5 + oOverworldNPCObject_CurState, NPC_CUR_STATE_STANDARD);
    ST8(R5 + oOverworldNPCObject_CurAction_sub_809F612,
        LD8(R5 + oOverworldNPCObject_Unk_1d));
    ST8(R5 + oOverworldNPCObject_MovementFlag_0a,
        LD8(R5 + oOverworldNPCObject_Unk_1e));

    R1 = OW_NPC_UNK_FLAGS_60_0x1000 | OW_NPC_UNK_FLAGS_60_FACE_PLAYER_WHEN_INTERACTED_sub_809F612;
    if ((LD32(R5 + oOverworldNPCObject_UnkFlags_60) & R1) == 0) {
        R0 = LD8(R5 + oOverworldNPCObject_AnimationSelect);
        GUEST_CALL(sprite_setAnimation);
        GUEST_CALL(sprite_loadAnimationData);
    }

    R0 = 0;
    SET_FLAGS_NZ(0);
    ST8(R5 + oOverworldNPCObject_ChatTriggered, 0);

    /* the original returns via pop {pc}, so LR keeps its entry value */
    LR = returnAddr;
    RETURN_TO_CALLER();
}

/* ── npc_overworld_CheckStateFlags50000_809F638 ── */
/* npc_overworld_CheckStateFlags50000_809F638(r5 = NPC object):
 * Checks the NPC's state flags for bits 0x50000 (some movement/animation state
 * marker) and returns 1 in r0 if any of them is set, 0 otherwise.
 * Also leaves the flag word in r1 and the mask 0x50000 in r2, as the original does. */
Next npc_overworld_CheckStateFlags50000_809F638(void) {
    u32 flags = LD32(R5 + oOverworldNPCObject_Unk_54);

    R1 = flags;
    R2 = 0x50000;
    R0 = (flags & 0x50000) != 0;

    RETURN_TO_CALLER();
}

/* ── ReadNPCScriptHalfword ── */
/* ReadNPCScriptHalfword(r0 = script pointer):
 * Reads a little-endian 16-bit value from the NPC script and advances nothing
 * (the caller owns the script cursor). Returns the halfword in r0 and the low
 * byte in r1; carry is cleared, as by the original LSL #8 / ORR sequence. */
Next ReadNPCScriptHalfword(void) {
    u32 addr = R0;
    u32 lo = LD8(addr);
    u32 hi = LD8(addr + 1);

    /* lsls hi, hi, #8 shifts a byte left by 8, so the bit shifted out into
     * carry (bit 24 of the source) is always 0. ORR (Thumb ORRS) then sets
     * N/Z from the result but leaves carry untouched. */
    cpu.c = 0;

    R0 = (hi << 8) | lo;
    R1 = lo;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}
