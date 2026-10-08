/*
 * npc_update.c — the overworld NPC pool, once per frame: run each NPC's
 * update handler, then draw their sprites (bn6f asm00_1.s).
 *
 * The pool is 16 OverworldNPCObjects (0xD8 bytes) from eOverworldNPCObjects;
 * each starts with an ObjectHeader (Flags, Index) and has its sprite struct
 * at +0xA0 (oOverworldNPCObject_SizeBeforeObjectSprite).
 *
 * The NPC handlers and sprite routines are still called through the guest
 * calling convention, so these functions hand them the registers and flags
 * the originals do (marked "as the original") and restore r4-r9/r12 on exit.
 */
#include "game.h"
#include "runtime.h"

#define NPC_POOL         0x020057B0u  /* eOverworldNPCObjects */
#define NPC_POOL_END     0x02006530u  /* byte_2006530: just past the 16th NPC */
#define NPC_COUNT        16
#define NPC_HANDLERS     0x080045BCu  /* off_80045BC: by ObjectHeader.Index; one entry, npc_dispatch_809E570 */
#define EVENT_NPC_OBJECTS_DISABLED 0x1701

enum {  /* bn6f include/structs/ObjectHeader.inc */
    OBJECT_FLAGS = 0x00, OBJECT_INDEX = 0x01,
    OBJECT_FLAG_ACTIVE = 0x01, OBJECT_FLAG_VISIBLE = 0x02, OBJECT_FLAG_PAUSE_UPDATE = 0x04,
};
enum {
    NPC_SPRITE = 0xA0,               /* oOverworldNPCObject_SizeBeforeObjectSprite */
    NPC_POSITION = 0x24,             /* what sprite_projectAndCull projects */
    SPRITE_PIECE_MASK = 0x24,        /* sprite struct: on-screen piece mask */
    NPC_TILE_ALLOCATOR = 2,          /* sprite_ensureGfxResident / sub_80028C0 index for NPCs */
    NPC_SPRITE_LAYER = 2, NPC_LIST_BIAS = 0x60,
};

/* Every active NPC runs its update handler, unless the NPCs are switched off
 * (EVENT_NPC_OBJECTS_DISABLED) or the game is battle-paused and the NPC is
 * not marked to update while paused. */
Next TryUpdateEachOverworldNPCObject_800461E(void) {
    u32 in4 = R4, in5 = R5, in6 = R6, in7 = R7, in8 = R8, in9 = R9, in12 = R12;
    R4 = in8; R6 = in12;                                  /* as the original (saving r8, r12 through r4, r6) */

    R0 = EVENT_NPC_OBJECTS_DISABLED >> 8;
    R1 = EVENT_NPC_OBJECTS_DISABLED & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (cpu.nz == 0) {                                    /* flag clear: update */
        for (R5 = NPC_POOL; (s32)R5 < (s32)NPC_POOL_END; R5 += oOverworldNPCObject_Size) {
            u32 flags = LD8(R5 + OBJECT_FLAGS);
            if (flags & OBJECT_FLAG_ACTIVE) {
                R7 = LD32(R10 + oToolkit_GameStatePtr);
                if (!LD8(R7 + oGameState_BattlePaused) || (flags & OBJECT_FLAG_PAUSE_UPDATE)) {
                    /* the handler for the object's Index, with r0/r1 and the
                     * flags of `lsl r1, #2` as the original */
                    R1 = LD8(R5 + OBJECT_INDEX) << 2;
                    SET_FLAGS_NZ(R1);
                    cpu.c = 0;
                    R0 = LD32(NPC_HANDLERS + R1);
                    GUEST_CALL_ADDR(R0);
                }
            }
            SET_FLAGS_CMP(R5 + oOverworldNPCObject_Size, NPC_POOL_END);   /* cmp r5, r0: V for the next handler */
        }
    }
    R4 = in4; R5 = in5; R6 = in6; R7 = in7; R8 = in8; R9 = in9; R12 = in12;
    RETURN_TO_CALLER();
}

/* Draw the NPCs: a visible NPC is projected, its graphics made resident and
 * its frame's pieces queued on sprite layer 2; an invisible one has no
 * pieces on screen, but an active one is still projected. Then the NPC tile
 * allocator's slot is recorded (sub_80028C0). */
Next npc_drawSprites_800467C(void) {
    u32 in4 = R4, in5 = R5, in6 = R6, in7 = R7, in8 = R8, in9 = R9, in12 = R12;
    R4 = in8; R6 = in12;                                  /* as the original */
    R0 = NPC_COUNT;
    u32 npc = NPC_POOL;
    for (u32 n = 0; n < NPC_COUNT; n++, npc += oOverworldNPCObject_Size) {
        u32 flags = LD8(npc + OBJECT_FLAGS);
        u32 sprite = npc + NPC_SPRITE;
        R1 = n;
        R2 = flags;
        if (flags & OBJECT_FLAG_VISIBLE) {
            R5 = npc;
            R0 = npc + NPC_POSITION;
            GUEST_CALL(sprite_projectAndCull);
            R0 = NPC_TILE_ALLOCATOR;
            GUEST_CALL(sprite_ensureGfxResident);
            R0 = NPC_SPRITE_LAYER;
            R1 = NPC_LIST_BIAS;
            R5 = sprite;
            GUEST_CALL(sprite_queueFramePieces);
        } else {
            R4 = sprite;
            ST32(sprite + SPRITE_PIECE_MASK, 0);
            R3 = 1;                                       /* as the original (`mov r3, #1; tst r2, r3`) */
            if (flags & OBJECT_FLAG_ACTIVE) {
                R5 = npc;
                R0 = npc + NPC_POSITION;
                GUEST_CALL(sprite_projectAndCull);
            }
        }
        R0 = NPC_COUNT;                                   /* pop {r0, r1, r5} */
    }
    SET_FLAGS_CMP(NPC_COUNT, NPC_COUNT);                  /* the loop's last cmp: C = 1, V = 0 */
    R0 = NPC_TILE_ALLOCATOR;
    GUEST_CALL(sub_80028C0);
    R4 = in4; R5 = in5; R6 = in6; R7 = in7; R8 = in8; R9 = in9; R12 = in12;
    RETURN_TO_CALLER();
}
