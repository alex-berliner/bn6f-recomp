/*
 * npc_script.c — the overworld NPC state machine (bn6f npc.s).
 *
 * TryUpdateEachOverworldNPCObject_800461E (npc_update.c) calls
 * npc_dispatch_809E570 for every active NPC, which runs the NPC's current
 * state (CurState is a byte offset into jt_809E580): 0 init, 4 standard,
 * 8 in a chatbox. The standard state steps the NPC's script and its current
 * action (NPCCurActionJumptable: wait, walk, hop, wait for an animation
 * frame, native callback, wait for a cutscene variable, wait for mystery
 * data), then brings the sprite up to date with what the script changed.
 *
 * The sprite and script routines are still guest code: each call gets the
 * argument registers its lifted contract lists as inputs, set the way the
 * original sets them; nothing else is touched in between, so their other
 * inputs and the registers this leaves behind are the original's.
 */
#include "game.h"
#include "runtime.h"

#define NPC_STATE_TABLE   0x0809E580u     /* jt_809E580: init, standard, in chatbox */
/* bn6f include/structs/OverworldNPCObject.inc, for what the shared header
 * does not export (it has OW_NPC_UNK_FLAGS_60_0x40 and _SHADOW_FLAG_0x80) */
enum {
    NPC_CUR_STATE = 0x08,            /* byte offset into jt_809E580 */
    NPC_CUR_ACTION = 0x09,           /* byte offset into NPCCurActionJumptable */
    NPC_UNK_07 = 0x07,
    NPC_CUR_STATE_STANDARD = 0x4,
    NPC_FLAGS_60_SPRITE_MIRRORED = 0x8,
    NPC_FLAGS_60_SHADOW_0x100 = 0x100,   /* with SHADOW_FLAG_0x80: set = no shadow, clear = shadow ignores Z */
};
#define NPC_SPRITE_SLOT   0x80            /* sprite_load's first argument for NPC sprites */

/* Run the NPC's current state (r5 = NPC). */
Next npc_dispatch_809E570(void) {
    R0 = LD8(R5 + NPC_CUR_STATE);
    R7 = LD32(NPC_STATE_TABLE + R0);
    GUEST_CALL_ADDR(R7);
    RETURN_TO_CALLER();
}

/* State 0: set up a freshly spawned NPC from the script pointer its spawner
 * left in UnkFlags_60, load its default sprite, and run the standard state
 * once. */
Next npc_init_809E590(void) {
    u32 npc = R5;
    R0 = LD32(npc + oOverworldNPCObject_UnkFlags_60);
    GUEST_CALL(setNPCScript_809F506);

    ST32(npc + oOverworldNPCObject_HiddenOAMPieces, 0);
    ST32(npc + oOverworldNPCObject_HiddenOAMPiecesUpdate, 0);
    ST8(npc + oOverworldNPCObject_MovementDirection, 0);
    ST8(npc + oOverworldNPCObject_Unk_0f, 0);
    ST8(npc + oOverworldNPCObject_InteractionLocked, 0);
    ST8(npc + NPC_UNK_07, 0);
    ST8(npc + oOverworldNPCObject_PaletteIndex, 0);
    ST32(npc + oOverworldNPCObject_UnkFlags_60, 0);
    ST32(npc + oOverworldNPCObject_LayerPriorityOverride, 0);
    ST32(npc + oOverworldNPCObject_Unk_50, 0);
    ST32(npc + oOverworldNPCObject_Unk_54, 0);
    ST32(npc + oOverworldNPCObject_Unk_58, 0);
    ST8(npc + oOverworldNPCObject_CenterOffsetX, 0);
    ST8(npc + oOverworldNPCObject_CenterOffsetY, 0);
    ST8(npc + oOverworldNPCObject_CenterOffsetZ, 0);
    ST16(npc + oOverworldNPCObject_NPCSprite, 0xFF);           /* no sprite yet: the */
    ST16(npc + oOverworldNPCObject_NPCSpriteUpdate, 0xFF);     /* script picks one   */
    ST8(npc + oOverworldNPCObject_AnimationSelect, 0xFF);
    ST8(npc + oOverworldNPCObject_AnimationSelectUpdate, 0xFF);
    ST8(npc + NPC_CUR_STATE, NPC_CUR_STATE_STANDARD);
    ST8(npc + oOverworldNPCObject_CollisionRadius, 4);
    ST8(npc + oOverworldNPCObject_ZReach, 8);

    R0 = NPC_SPRITE_SLOT; R1 = 0x1C; R2 = 0xA0;
    SET_FLAGS_NZ(R2);                     /* as the original (its last movs) */
    GUEST_CALL(sprite_load);
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(npc_standard_809E5E2);
    RETURN_TO_CALLER();
}

/* State 4, every frame: step the script and the current action (or the
 * chat reaction if the player started a conversation), then apply what
 * changed to the sprite: mirroring, a new sprite and its shadow mode, a new
 * animation, palette and hidden pieces; rebuild the interaction areas and
 * pick the drawing layer. */
Next npc_standard_809E5E2(void) {
    u32 npc = R5;
    R0 = LD8(npc + oOverworldNPCObject_ChatTriggered);
    if (R0) {
        GUEST_CALL(npc_beginChatboxState_809F5FC);
    } else {
        GUEST_CALL(npc_runPrimaryScript_809ebdc);
        R0 = LD8(npc + NPC_CUR_ACTION);
        R7 = LD32(A_NPCCurActionJumptable + R0);
        GUEST_CALL_ADDR(R7);

        R2 = LD32(npc + oOverworldNPCObject_UnkFlags_60);
        R1 = NPC_FLAGS_60_SPRITE_MIRRORED;
        R0 = (R2 & R1) ? 1 : 0;
        GUEST_CALL(sprite_setFlip);

        R0 = LD16(npc + oOverworldNPCObject_NPCSprite);
        R1 = LD16(npc + oOverworldNPCObject_NPCSpriteUpdate);
        if (R0 != R1) {                   /* the script changed the sprite */
            R2 = R0;
            R0 = NPC_SPRITE_SLOT;
            R1 = LD32(npc + oOverworldNPCObject_NPCSpriteCategory);
            GUEST_CALL(sprite_load);
            GUEST_CALL(sprite_loadAnimationData);
            u32 flags60 = LD32(npc + oOverworldNPCObject_UnkFlags_60);
            if (!(flags60 & OW_NPC_UNK_FLAGS_60_SHADOW_FLAG_0x80))
                GUEST_CALL(sprite_noShadow);
            else if (flags60 & NPC_FLAGS_60_SHADOW_0x100)
                GUEST_CALL(sprite_hasShadow);
            else
                GUEST_CALL(sprite_removeShadow);
            ST8(npc + oOverworldNPCObject_AnimationSelectUpdate, 0xFF);   /* reload the animation too */
        }
        R0 = LD8(npc + oOverworldNPCObject_AnimationSelect);
        R1 = LD8(npc + oOverworldNPCObject_AnimationSelectUpdate);
        if (R0 != R1) {
            GUEST_CALL(sprite_setAnimation);
            GUEST_CALL(sprite_loadAnimationData);
        }
    }
    GUEST_CALL(sprite_update);
    R0 = LD8(npc + oOverworldNPCObject_PaletteIndex);
    GUEST_CALL(sprite_setPalette);
    R0 = LD32(npc + oOverworldNPCObject_HiddenOAMPieces);
    R1 = LD32(npc + oOverworldNPCObject_HiddenOAMPiecesUpdate);
    if (R0 != R1)
        GUEST_CALL(sprite_setUnk0x2c);

    /* remember what the sprite now shows */
    ST8(npc + oOverworldNPCObject_Unk_0f, LD8(npc + oOverworldNPCObject_MovementDirection));
    ST8(npc + oOverworldNPCObject_AnimationSelectUpdate, LD8(npc + oOverworldNPCObject_AnimationSelect));
    ST16(npc + oOverworldNPCObject_NPCSpriteUpdate, LD16(npc + oOverworldNPCObject_NPCSprite));
    R0 = LD32(npc + oOverworldNPCObject_HiddenOAMPieces);
    ST32(npc + oOverworldNPCObject_HiddenOAMPiecesUpdate, R0);

    GUEST_CALL(npc_createInteractionAreas_809F526);
    R0 = LD32(npc + oOverworldNPCObject_LayerPriorityOverride);
    if (!R0) {
        R0 = npc + oOverworldNPCObject_Coords;
        GUEST_CALL(applyLayerEffectToOWObject_8035694);
    }
    GUEST_CALL(sprite_setPriority_8002E14);              /* r0 = the drawing layer */

    R0 = LD32(npc + oOverworldNPCObject_UnkFlags_60);
    R1 = OW_NPC_UNK_FLAGS_60_0x40;
    SET_FLAGS_NZ(R0 & R1);                /* as the original (tst r0, r1) */
    if (R0 & R1)
        GUEST_CALL(sub_809F922);
    RETURN_TO_CALLER();
}
