/*
 * asm00_1.c: readable functions of bn6f asm00_1.s, up to 0x08005C04, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── clearBattleObjectLinkedLists_800318C ── */
/* clearBattleObjectLinkedLists_800318C: reset the battle object linked lists
 * to an empty state before a battle is set up.
 *
 * The battle objects form a doubly linked list with a sentinel node:
 *   - eBattleObjectsLinkedListStart holds first / last pointers (0 / sentinel),
 *   - eBattleObjectsLinkedListSentinel_Prev is the sentinel, pointing back to
 *     the start block and having a NULL next pointer.
 * eUnkBattleObjectLinkedList is cleared to NULL as well.
 * Leaves R2 = list head address, R3 = 0 (as the original does). */
Next clearBattleObjectLinkedLists_800318C(void) {
    u32 start = A_eBattleObjectsLinkedListStart;      /* list head block */
    u32 sentinel = A_eBattleObjectsLinkedListSentinel_Prev; /* sentinel node */
    u32 unk = A_eUnkBattleObjectLinkedList;

    R2 = unk;
    R3 = 0;

    ST32(unk, R3);        /* eUnkBattleObjectLinkedList = NULL */
    ST32(start, R3);      /* list start: first object = NULL */
    ST32(start + 4, sentinel); /* list start: last = sentinel */
    ST32(sentinel, start);     /* sentinel: prev = list start */
    ST32(sentinel + 4, R3);    /* sentinel: next = NULL */

    RETURN_TO_CALLER();
}

/* ── object_spawnType1 ── */
/* object_spawnType1(r0 = obj_idx, r1 = x, r2 = y, r3 = z, r4 = params)
 * -> r5 = spawned BattleObject (0 if the spawn failed).
 *
 * Spawns a battle object of type 1. The five word arguments are copied into
 * a context block on the stack, which SpawnBattleObjectCommon reads while
 * creating the object. If spawning succeeded, sub_8003400 runs the
 * type-1 specific initialisation on the new object (passed in r5). */
Next object_spawnType1(void) {
    /* Frame layout, matching the original push {r7,lr} + 0x14 byte block:
     *   ctx+0x00..0x10  the five spawn arguments (r0-r4)
     *   ctx+0x14        saved r7 slot
     *   ctx+0x18        saved lr slot */
    u32 ctx = SP -= 0x1C;

    u32 r7_in = R7;
    u32 objIdx = R0, x = R1, y = R2, z = R3, params = R4;

    ST32(ctx + 0x00, objIdx);
    ST32(ctx + 0x04, x);
    ST32(ctx + 0x08, y);
    ST32(ctx + 0x0C, z);
    ST32(ctx + 0x10, params);
    ST32(ctx + 0x14, r7_in);
    ST32(ctx + 0x18, LR);

    /* While the callees run, r7 points just past the argument block,
     * exactly as it does in the original. */
    R7 = ctx + 0x14;

    R0 = 1;              /* object type 1 */
    R1 = ctx;            /* spawn context block */
    GUEST_CALL(SpawnBattleObjectCommon);

    u32 obj = R5;
    if (obj != 0) {
        /* Type-1 specific initialisation of the new object in r5;
         * r0-r3 still hold SpawnBattleObjectCommon's results, as in the
         * original. */
        GUEST_CALL(sub_8003400);
    } else {
        /* Spawn failed: the original's `tst r5, r5` is the last flag
         * setting on this path. */
        SET_FLAGS_NZ(obj);
    }

    R7 = r7_in;
    SP += 0x1C;
    RETURN_TO_CALLER();
}

/* ── object_spawnType3 ── */
/* object_spawnType3(r0-r4 = spawn arguments for the new object):
 * Spawns a battle object of type 3. The five arguments (r0-r4) are handed to
 * SpawnBattleObjectCommon through a 5-word block on the stack; r7 points just
 * past that block while the callee runs. If an object was actually created,
 * its type-3 initializer (sub_8003400) runs immediately.
 * Returns the new object pointer in r5; flags come from the initializer when
 * one ran, otherwise from testing the pointer against zero. */
Next object_spawnType3(void) {
    u32 savedR7 = R7;

    /* Frame: 0x14 bytes of spawn arguments (r0-r4) at the bottom, the saved
     * r7/lr pair above them - the same layout the original pushes/stores. */
    SP -= 0x1C;
    u32 args = SP;
    ST32(args, R0);
    ST32(args + 4, R1);
    ST32(args + 8, R2);
    ST32(args + 12, R3);
    ST32(args + 16, R4);
    ST32(args + 20, savedR7);
    ST32(args + 24, LR);

    /* stmia r7! leaves r7 pointing past the argument block */
    R7 = args + 0x14;

    R0 = 3;                        /* object type 3 */
    R1 = args;                     /* argument block on the stack */
    GUEST_CALL(SpawnBattleObjectCommon);

    /* tst r5, r5: did the spawn succeed? */
    SET_FLAGS_NZ(R5);
    if (R5 != 0)
        GUEST_CALL(sub_8003400);   /* run the new object's initializer */

    SP += 0x1C;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── object_spawnType4 ── */
/* object_spawnType4(r0-r4 = spawn parameters):
 * Spawns a type-4 battle object. The five parameter words are copied into a
 * context block on the stack, which is what SpawnBattleObjectCommon expects
 * in r1. If an object was actually created (non-zero pointer in r5), its
 * initializer sub_8003400 runs immediately with the spawn results still in
 * the registers.
 * Returns the object pointer; r4, r6 and r7 are preserved. */
Next object_spawnType4(void) {
    u32 saved_r7 = R7;

    /* Frame layout as in the original: 8 bytes for the saved r7/lr pair,
     * then the 0x14-byte parameter block at the bottom. */
    u32 block = SP - 0x1C;
    SP = block;
    ST32(block + 0x14, saved_r7);
    ST32(block + 0x18, LR);

    /* stmia block!, {r0-r4}: pack the spawn parameters */
    ST32(block + 0x00, R0);
    ST32(block + 0x04, R1);
    ST32(block + 0x08, R2);
    ST32(block + 0x0C, R3);
    ST32(block + 0x10, R4);

    R0 = 4;             /* object type 4 */
    R1 = block;         /* -> parameter block on the stack */
    R7 = block + 0x14;  /* r7 ends up pointing past the block, as in the original */
    GUEST_CALL(SpawnBattleObjectCommon);

    if (R5 != 0) {
        /* Object created: run its initializer, which reads r0-r5 and r7. */
        GUEST_CALL(sub_8003400);
    } else {
        /* No object: hand the tst r5, r5 flags to the caller. */
        SET_FLAGS_NZ(R5);
    }

    R7 = saved_r7;      /* pop {r7} */
    SP += 0x1C;
    RETURN_TO_CALLER();
}

/* ── UpdateBattleObjectLinkedList ── */
/* Offsets used by the battle object linked list (fields at the tail of each object,
 * which is why the caller passes r5 = object end address).
 * The sentinel's "prev" field sits at offset 0, like the objects' own prev field. */
#define oBattleObject_LinkedList_Prev           0
#define oBattleObject_LinkedList_Next           4
#define oBattleObjectsLinkedListSentinel_Prev   0

/* UpdateBattleObjectLinkedList (0x08003428):
 * Append the newly created battle object (whose end address the caller left in r5) to
 * the tail of the battle object list, i.e. right before the sentinel that marks the
 * end of the list.
 *   inputs:  r5 = end of the new object (object start = r5 - oBattleObject_LinkedList_Size)
 *   outputs: r0 = new object, r1 = sentinel, r2 = previous last object,
 *            nz/c/v from the subtraction that computed the object address
 * List layout: ... <-> prev <-> new <-> sentinel                             */
Next UpdateBattleObjectLinkedList(void) {
    /* C: the object being inserted (r5 points one object-size past its start) */
    u32 newObject = R5 - oBattleObject_LinkedList_Size;
    SET_FLAGS_CMP(R5, oBattleObject_LinkedList_Size);

    /* S: the list sentinel; its prev field holds the current last object */
    u32 sentinel = A_eBattleObjectsLinkedListSentinel_Prev;

    /* P: the object currently last in the list (the sentinel itself if the list is empty) */
    u32 prev = LD32(sentinel + oBattleObjectsLinkedListSentinel_Prev);

    /* prev <-> new: make the old last object point forward to the new one */
    ST32(prev + oBattleObject_LinkedList_Next, newObject);

    /* new's backward link goes to the old last object */
    ST32(newObject + oBattleObject_LinkedList_Prev, prev);

    /* new's forward link goes to the sentinel (it is now the last object) */
    ST32(newObject + oBattleObject_LinkedList_Next, sentinel);

    /* the sentinel's backward link now points at the new last object */
    ST32(sentinel + oBattleObjectsLinkedListSentinel_Prev, newObject);

    R0 = newObject;
    R1 = sentinel;
    R2 = prev;
    RETURN_TO_CALLER();
}

/* ── object_freeMemory ── */
/* object_freeMemory(r5 = battle object):
 * Releases a battle object: clears its flags byte, frees its slot in the
 * per-type allocation bitmap (slots are handed out from the top bit down,
 * so slot n is bit 31-n), unlinks it from the doubly-linked list of active
 * objects, and calls sprite_makeUnscalable on the list node/header.
 *
 * On return, r0-r3 and the flags are whatever sprite_makeUnscalable left;
 * r5 is preserved. */

/* Field offsets into a battle object (not yet named in game.h). */
#define oBattleObject_Flags         0x00  /* byte: status flags           */
#define oBattleObject_Type          0x02  /* byte: low nibble = type id   */
#define oBattleObject_ListIndex     0x03  /* byte: slot in the bitmap     */
#define oBattleObject_LinkedList_Prev 0x00 /* node = object - LinkedList_Size */
#define oBattleObject_LinkedList_Next 0x04

Next object_freeMemory(void) {
    u32 obj = R5;

    /* the object is no longer in use */
    ST8(obj + oBattleObject_Flags, 0);

    /* free the object's slot in its type's allocation bitmap */
    u32 type = LD8(obj + oBattleObject_Type) & 0xF;
    u32 bitmap = LD32(A_ActiveObjectBitfieldPointers + type * 4);
    u32 listIndex = LD8(obj + oBattleObject_ListIndex);
    u32 slotBit = (listIndex < 32) ? (0x80000000u >> listIndex) : 0;
    ST32(bitmap, LD32(bitmap) & ~slotBit);

    /* unlink the object from the doubly-linked active object list; the
     * list node sits in the header just before the object proper */
    u32 node = obj - oBattleObject_LinkedList_Size;
    u32 prev = LD32(node + oBattleObject_LinkedList_Prev);
    u32 next = LD32(node + oBattleObject_LinkedList_Next);
    ST32(prev + oBattleObject_LinkedList_Next, next);
    ST32(next + oBattleObject_LinkedList_Prev, prev);

    /* clear the sprite scaling of the object's header; pass the same
     * register state the original does at the call */
    R0 = node;
    R1 = prev;
    R2 = next;
    R3 = slotBit;
    GUEST_CALL(sprite_makeUnscalable);

    RETURN_TO_CALLER();
}

/* ── InitializeOWPlayerObjectStruct ── */
/* InitializeOWPlayerObjectStruct:
 * Initializes the player's overworld object struct (object type 0) by
 * forwarding to the generic struct initializer. All other arguments
 * (r2, r3, r4, r7) are passed through unchanged; results and flags come
 * straight from the callee. */
#define OVERWORLD_PLAYER_OBJECT 0

Next InitializeOWPlayerObjectStruct(void) {
    R0 = OVERWORLD_PLAYER_OBJECT;
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── InitializeT1BattleObjectStructs ── */

/* InitializeT1BattleObjectStructs(r2, r3, r4, r7 = args passed through):
 * reset all battle object structs of type T1_BATTLE_OBJECT. */
Next InitializeT1BattleObjectStructs(void) {
    R0 = T1_BATTLE_OBJECT;
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── InitializeT3BattleObjectStructs ── */
/* InitializeT3BattleObjectStructs(r2, r3, r4, r7 = pass-through arguments):
 * Reset all battle object structs of object type 3 (T3_BATTLE_OBJECT); the
 * caller's r2/r3/r4/r7 arguments are forwarded untouched to
 * InitializeStructsOfObjectType. */
Next InitializeT3BattleObjectStructs(void) {
    R0 = 3; /* T3_BATTLE_OBJECT */
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── InitializeT4BattleObjectStructs ── */
/* InitializeT4BattleObjectStructs:
 * Re-initialize every battle object struct of the T4 (tourney battle) object
 * type (type constant 4). The value is passed straight through to
 * InitializeStructsOfObjectType, which does all the work; the caller's
 * arguments in r2/r3 and its saved r4/r7 must reach it untouched, and the
 * outputs are simply whatever that callee leaves behind. */
Next InitializeT4BattleObjectStructs(void) {
    R0 = 4; /* T4_BATTLE_OBJECT */
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── InitializeOverworldNPCObjectStructs ── */
/* InitializeOverworldNPCObjectStructs: initialize every overworld object struct
 * of object type 2 (NPC). Delegates to InitializeStructsOfObjectType; r0-r3 are
 * that callee's arguments/scratch, r4 and r7 must survive the call. */
Next InitializeOverworldNPCObjectStructs(void) {
    R0 = 2; /* OVERWORLD_NPC_OBJECT */
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── InitializeOverworldMapObjectStructs ── */
/* InitializeOverworldMapObjectStructs: reinitialize every overworld-map
 * object struct (object type 5, the "overworld map object" type).
 * Its r2/r3/r4/r7 arguments are passed straight through to
 * InitializeStructsOfObjectType, which does all the work. */
Next InitializeOverworldMapObjectStructs(void) {
    R0 = 5; /* OVERWORLD_MAP_OBJECT */
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── object_Clear3RAMBytes_800371A ── */
/* object_Clear3RAMBytes_800371A:
 * zero out the three single-byte flags used to track object state. */
Next object_Clear3RAMBytes_800371A(void) {
    ST8(A_byte_2036778, 0);
    ST8(A_dword_203CA7C, 0);
    ST8(A_byte_2036830, 0);
    RETURN_TO_CALLER();
}

/* ── ZeroFillObjectInteractionAreas_800378C ── */
/* ZeroFillObjectInteractionAreas_800378C: clear the overworld object
 * interaction-area pool and reset the number of entries it holds.
 * Flags on return come from ZeroFillByWord's final flag-setting instruction. */
Next ZeroFillObjectInteractionAreas_800378C(void) {
    R0 = A_eOWObjectInteractionArea_0_X;
    R1 = 0x280; /* pool size in bytes */
    GUEST_CALL(ZeroFillByWord);

    ST32(A_eNumOWObjectInteractionAreas, 0);
    RETURN_TO_CALLER();
}

/* ── Clear_eStruct2000780 ── */
/* Clear_eStruct2000780: zero out the 0x48-byte scenario effect state block
 * at eScenarioEffectState2000780. Doesn't touch r2-r7. */
Next Clear_eStruct2000780(void) {
    R0 = A_eScenarioEffectState2000780;
    R1 = 0x48;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── scenario_initEffectState_8003962 ── */
/* scenario_initEffectState_8003962: initialize the scenario effect state block at 0x2000780.
 * If the block is not already initialized (byte at +2 is zero), clear its
 * 0x48 bytes, then mark it initialized by storing 1 at byte +3. */
Next scenario_initEffectState_8003962(void) {
    u32 state = A_eScenarioEffectState2000780;

    if (LD8(state + 2) == 0) {
        R0 = state;
        R1 = 0x48;
        GUEST_CALL(ZeroFillByWord);
    }

    ST8(state + 3, 1);
    RETURN_TO_CALLER();
}

/* ── scenario_ClearEffectActiveFlag_800399A ── */
/* scenario_ClearEffectActiveFlag_800399A: clear the top bit of the scenario effect state byte, keeping
 * the low 7 bits (the effect id/state) intact. Clobbers r0/r1 only; the
 * original's r4-r7 push/pop just preserves them, which locals already do. */
Next scenario_ClearEffectActiveFlag_800399A(void) {
    u32 state = A_eScenarioEffectState2000780;

    ST8(state, LD8(state) & 0x7F);

    RETURN_TO_CALLER();
}

/* ── battle_ClearEStruct2001010_8003A58 ── */
/* battle_ClearEStruct2001010_8003A58: clear the eStruct2001010 battle work area (0x48 bytes).
 * Takes no arguments and returns nothing; r2-r7 are untouched. */
Next battle_ClearEStruct2001010_8003A58(void) {
    R0 = A_eStruct2001010;
    R1 = 0x48;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── zeroFill_8003AB2 ── */
/* zeroFill_8003AB2: clear the eStruct2001010 work block (0x48 bytes) once, then
 * flag it as initialized.
 *
 * The block is only zeroed while the "already filled" flag at +0x02 is still 0;
 * afterwards the flag at +0x03 is set so callers know the fill has happened.
 * Inputs come in via r4-r7 but are only read to satisfy the calling
 * convention; nothing the caller passes is actually used.
 * Leaves r1 = 0x48 (the size argument handed to ZeroFillByWord) and the
 * C/V flags untouched, matching the original's exit state. */
Next zeroFill_8003AB2(void) {
    u32 eStruct = A_eStruct2001010;

    if (LD8(eStruct + 2) == 0) {
        /* First run: wipe the whole block. */
        R0 = eStruct;
        R1 = 0x48;
        GUEST_CALL(ZeroFillByWord);
    }

    /* Mark the block as filled. */
    ST8(eStruct + 3, 1);

    RETURN_TO_CALLER();
}

/* ── event_ackNewFlagByte_8003AEA ── */
/* event_ackNewFlagByte_8003AEA: clear bit 7 of the event-status byte at eStruct2001010.
 * This acknowledges the pending event flag so it no longer reads as "new".
 * Returns r1 = 0x7F (the mask used) with nz flags set from the masked byte. */
Next event_ackNewFlagByte_8003AEA(void) {
    u32 flags = LD8(A_eStruct2001010) & 0x7F;
    ST8(A_eStruct2001010, flags);
    R1 = 0x7F;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── SpawnOWPlayerObject ── */
/*
 * Overworld player object pool.
 *
 * eOWPlayerObject holds a pool of fixed 0xC8-byte object slots; the u32 at
 * eActiveOWPlayerObjectBitfield tracks which slots are live, one bit per
 * slot, scanned from bit 31 (first slot) downwards.
 */
#define OW_PLAYER_SLOT_SIZE 0xC8

/* SpawnOWPlayerObject(r0..r4 = initialiser values for the new object):
 * Claims the first free slot in the overworld player object pool, stores the
 * caller's arguments into it and returns the slot address in r5 (r5 = 0 if
 * the pool is full). r0 = 0 on success; r1-r4 pass through unchanged on both
 * paths. Flags: on failure they come from the final CMP slot/poolEnd
 * (carry set, since the pool end was reached); on success N=0/Z=1 with
 * carry clear (the mask-building LSL and the in-loop CMP both shift/compare
 * out zero bits, and the free-slot TST clears N and sets Z). */
Next SpawnOWPlayerObject(void) {
    u32 arg0 = R0, arg1 = R1, arg2 = R2, arg3 = R3, arg4 = R4;

    u32 bitfieldAddr = A_eActiveOWPlayerObjectBitfield;
    u32 bits = LD32(bitfieldAddr);
    u32 mask = 0x80000000u;                          /* r1 = 0x80 << 24 */
    u32 slot = A_eOWPlayerObject;
    u32 poolEnd = A_eOWPlayerObject_SpriteData_Size; /* = eOWPlayerObjectEnd */

    /* The LSL that builds the mask shifts a zero bit out, clearing carry;
       it stays clear through the loop (each LSR shifts out a zero, and the
       in-loop CMP has slot < poolEnd, which also produces carry = 0). */
    cpu.c = 0;

    /* Scan the bitfield from the top bit for the first free (clear) slot. */
    while ((bits & mask) != 0) {
        mask >>= 1;
        slot += OW_PLAYER_SLOT_SIZE;
        SET_FLAGS_CMP(slot, poolEnd);
        if ((s32)slot >= (s32)poolEnd) {
            /* Pool exhausted: report failure in r5; the CMP above leaves
               the flags as the original does (MOV does not touch them). */
            R5 = 0;
            RETURN_TO_CALLER();
        }
    }

    /* Mark the slot as in use. */
    ST32(bitfieldAddr, bits | mask);

    /* Initialise the new object from the caller's arguments. */
    ST8(slot, 9);                               /* object kind: overworld player */
    ST8(slot + 1, arg0);
    ST32(slot + 4, arg4);
    ST32(slot + 8, 0);
    ST32(slot + 0x1C, arg1);
    ST32(slot + 0x20, arg2);
    ST32(slot + 0x24, arg3);

    R0 = 0;
    R5 = slot;
    SET_FLAGS_NZ(0);   /* same N/Z the free-bit TST leaves behind */
    RETURN_TO_CALLER();
}

/* ── sub_8003C70 ── */
/* sub_8003C70: reset the something tracked by sub_80028C0 (r0 = 0 as its argument). */
Next sub_8003C70(void) {
    R0 = 0;
    GUEST_CALL(sub_80028C0);
    RETURN_TO_CALLER();
}

/* ── sub_8004298 ── */
/* sub_8004298: invoke sub_80028C0 with argument 3 and return.
 * (The mov r0, #0x30 / ldr r3 bytes after the pop are dead code from a
 * previous tail of the function and are never executed.) */
Next sub_8004298(void) {
    R0 = 3;
    GUEST_CALL(sub_80028C0);
    RETURN_TO_CALLER();
}

/* ── sub_8004590 ── */
/* sub_8004590: wrapper that invokes sub_80028C0 with argument 4 (tail call:
 * returns directly to our caller, LR is passed through unchanged). */
Next sub_8004590(void) {
    R0 = 4;
    GUEST_CALL(sub_80028C0);
    RETURN_TO_CALLER();
}

/* ── FreeOverworldNPCObject ── */
/* FreeOverworldNPCObject(r5 = overworld NPC object header):
 * Frees an overworld NPC's object slot: clears the NPC's bit in the
 * active-overworld-NPC bitfield (the slot number is encoded as a mask with
 * bit (31 - listIndex) set), clears the object's flags byte to mark it
 * unused, and tells the sprite system to stop scaling the NPC's sprite.
 * Ends in a call, so r0-r3 and the flags are whatever the callee leaves. */
Next FreeOverworldNPCObject(void) {
    /* Object header offset 3 holds the NPC's index in the object list. */
    u32 listIndex = LD8(R5 + 3);

    /* Slot mask: bit (31 - listIndex) of the shared bitfield. ARM LSR by a
     * register >= 32 yields 0, so out-of-range indices clear nothing. */
    u32 slotMask = (listIndex < 32) ? (0x80000000u >> listIndex) : 0;

    /* Clear this NPC's bit in the active-overworld-NPC bitfield. */
    u32 bitfieldAddr = A_eActiveOverworldNPCObjectsBitfield;
    u32 newBits = LD32(bitfieldAddr) & ~slotMask;
    ST32(bitfieldAddr, newBits);

    /* Mark the object header as unused. */
    ST8(R5, 0);

    /* Release the sprite's scaler state; the callee's register results and
     * flags are this function's results, so just pass the live values on. */
    R0 = slotMask;
    R1 = 0;
    R2 = newBits;
    GUEST_CALL(sprite_makeUnscalable);

    RETURN_TO_CALLER();
}

/* ── gameMode_switchToMode2_80046F8 ── */
/* gameMode_switchToMode2_80046F8: switch the game into mode 2 by calling sub_80028C0.
 * No inputs; sub_80028C0 receives r0 = 2. */
Next gameMode_switchToMode2_80046F8(void) {
    R0 = 2;
    GUEST_CALL(sub_80028C0);
    RETURN_TO_CALLER();
}

/* ── sub_8004702 ── */
/* ---------------------------------------------------------------------------
 * sub_8004702 — reset one state word of every overworld NPC object.
 *
 * The original walks the 16 fixed slots of the overworld NPC object table
 * (0xD8 bytes per slot) and clears the u32 at slot + 0xC4 (reached in the
 * assembly as base + 0xA0, then storing at +0x24). This looks like the
 * "active object" / behavior pointer word being invalidated before the
 * overworld map is (re)loaded.
 *
 * Register contract: the caller sees the loop's leftover registers:
 *   r0 = 0x10 (loop counter after the final CMP r0, #0x10)
 *   r1 = base + 15*0xD8 + 0xA0  (address used by the last store)
 *   r2 = 0
 *   r3 = base + 16*0xD8         (one past the last slot)
 * and the flags from that final CMP (16 - 16 == 0: Z=1, C=1, V=0, N=0).
 * ------------------------------------------------------------------------- */

/* Result of the typed worker: the values the original leaves in r0..r3. */
typedef struct {
    u32 count;        /* r0: iterations performed (always 16) */
    u32 lastWordAddr; /* r1: address of the last cleared word */
    u32 zero;         /* r2: the constant stored (always 0) */
    u32 tableEnd;     /* r3: pointer just past the last slot */
} NpcResetResult;

/* Clear the active/behavior word (offset 0xC4) of all 16 overworld NPC slots.
 * A_eOverworldNPCObject0_Flags is the table base (== eOverworldNPCObjects in
 * the disassembly's literal pool). */
static NpcResetResult clear_overworld_npc_active_words(void)
{
    u32 base = A_eOverworldNPCObject0_Flags;
    u32 lastWordAddr = 0;

    for (u32 slot = 0; slot < 16; slot++) {
        u32 entry = base + slot * 0xD8;

        lastWordAddr = entry + 0xA0;
        ST32(lastWordAddr + 0x24, 0);   /* entry + 0xC4: clear active word */
    }

    NpcResetResult result;
    result.count        = 16;
    result.lastWordAddr = lastWordAddr;
    result.zero         = 0;
    result.tableEnd     = base + 16 * 0xD8;
    return result;
}

/* sub_8004702() -> r0..r3, nz c v (no inputs) */
Next sub_8004702(void)
{
    NpcResetResult result = clear_overworld_npc_active_words();

    R0 = result.count;
    R1 = result.lastWordAddr;
    R2 = result.zero;
    R3 = result.tableEnd;

    /* Final CMP r0, #0x10 of the loop: compares the finished count to 16. */
    SET_FLAGS_CMP(R0, 0x10);
    RETURN_TO_CALLER();
}

/* ── FreeOverworldMapObject ── */
/* FreeOverworldMapObject(r5 = overworld map object):
 * Releases one overworld map object: clears its "active" bit in the global
 * active-objects bitfield, zeroes the object's first byte (its state/slot
 * marker), and tells the sprite system the object is no longer scalable.
 *
 * The bit for a slot is 0x80000000 rotated right by the slot's list index,
 * i.e. index 0 is bit 31 of word 0, index 1 is bit 30, ... index 32 is bit 31
 * of word 1, and so on. R0-R3 come back holding whatever sprite_makeUnscalable
 * left in them. R5 is preserved. */

/* The list-index field sits at offset 3 in the object struct; no named macro
 * for it exists in the headers, so spell the offset out. */
#define oOverworldMapObject_ListIndex 3

Next FreeOverworldMapObject(void) {
    u32 index = LD8(R5 + oOverworldMapObject_ListIndex);

    /* Mask with the single bit owned by this index. ROR by a multiple of 32
     * (including 0) leaves the mask unchanged. */
    u32 shift = index & 0xFF;
    u32 mask  = 0x80000000u;
    if (shift & 31)
        mask = (mask >> (shift & 31)) | (mask << (32 - (shift & 31)));

    u32 bitfield   = A_eActiveOverworldMapObjectsBitfield;
    u32 wordOffset = (index >> 5) << 2;
    u32 word       = LD32(bitfield + wordOffset) & ~mask;
    ST32(bitfield + wordOffset, word);

    ST8(R5, 0); /* object slot is now free */

    R0 = mask;
    R1 = 0;
    R2 = bitfield;
    R3 = word;
    GUEST_CALL(sprite_makeUnscalable);

    RETURN_TO_CALLER();
}

/* ── sub_80049B0 ── */
/* sub_80049B0: invoke sub_80028C0 with argument 5; result comes back in r2. */
Next sub_80049B0(void) {
    R0 = 5;
    GUEST_CALL(sub_80028C0);
    RETURN_TO_CALLER();
}

/* ── mainState_resetGameAndGoto28_8005524 ── */
/* mainState_resetGameAndGoto28_8005524: tear down and re-initialize the current game mode.
 *
 * Frees every object of type 0x21, runs the standard init passes
 * (scene setup, work-area clear, scenario effect state, toolkit
 * pointer randomization, navi stat boost reload), then advances the
 * main jumptable to state 0x28. R10 holds the toolkit pointer on
 * entry; the jumptable index byte lives at offset 0 in the toolkit
 * (oToolkit_MainJumptableIndexPtr), and is reached via r7 = r10. */
Next mainState_resetGameAndGoto28_8005524(void) {
    R0 = 0x21; /* object type to free */
    GUEST_CALL(FreeAllObjectsOfSpecifiedTypes);

    GUEST_CALL(sub_8004702);
    GUEST_CALL(resetAllGraphicsState_8005F40);
    GUEST_CALL(runSetupStepAndClearWorkArea_8005F6C);
    GUEST_CALL(sub_80027C4);
    GUEST_CALL(zeroFill_80024A2);
    GUEST_CALL(scenario_initEffectState_8003962);
    GUEST_CALL(zeroFill_8003AB2);
    GUEST_CALL(RandomizeExtraToolkitPointers);
    GUEST_CALL(reloadCurNaviStatBoosts_813c3ac);

    /* Advance the main jumptable: r7 = r10 (toolkit), then store the
     * next state index 0x28 into *(&toolkit + oToolkit_MainJumptableIndexPtr).
     * That pointer field is at offset 0, so it is simply *toolkit. */
    R7 = R10;
    R0 = LD32(R7);
    R1 = 0x28;
    SET_FLAGS_NZ(R1); /* movs r1, #0x28 is the last flag-setting instruction */
    ST8(R0, R1);

    RETURN_TO_CALLER();
}

/* ── gameState_beginEndingSequence_800596C ── */
/* gameState_beginEndingSequence_800596C (self: *GameState in r5): begin the ending sequence.
 *
 * Sets the two Gregar-story event flags, starts a screen fade, ticks the
 * countdown timer table, then hands control to subsystem 0x10 of the game
 * state machine (which drives sub_800536E via cbGameState_80050EC).
 *
 * Flag contract: NZ comes from the final `movs r0, #0x10` (R0 = 0x10, so
 * N=0 Z=0); C and V are whatever timer_tickCountdownTable_8035738 left
 * behind.  The subsystem index lives at offset 0 of GameState, so the
 * store goes straight through the r5 pointer. */
Next gameState_beginEndingSequence_800596C(void) {
    /* EVENT_1703: flag group 0x17, bit 3 */
    R0 = 0x17;
    R1 = 3;
    GUEST_CALL(SetEventFlagFromImmediate);

    /* EVENT_1738: flag group 0x17, bit 0x38 */
    R0 = 0x17;
    R1 = 0x38;
    GUEST_CALL(SetEventFlagFromImmediate);

    /* fade the screen out (fade kind 0xC, speed 0x10) */
    R0 = 0xC;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    GUEST_CALL(timer_tickCountdownTable_8035738);

    /* switch the game state machine over to the ending subsystem */
    R0 = 0x10;
    SET_FLAGS_NZ(R0);
    ST8(R5, R0);

    RETURN_TO_CALLER();
}

/* ── map_onMapWarpCutscene_80059B4 ── */
/* map_onMapWarpCutscene_80059B4 (self: * GameState in r5): on-map warp handler.
 *
 * Clears EVENT_1703, sets EVENT_1738, then plays the on-map warp cutscene
 * (CutsceneScriptOnMapWarp_8098a02) with parameter 0. Disabling the cutscene
 * call makes warps like the one in Lan's HP stop reacting, though they still
 * animate. */

Next map_onMapWarpCutscene_80059B4(void) {
    /* ClearEventFlagFromImmediate/SetEventFlagFromImmediate take the event id
     * split as (bank in r0, index in r1). */
    R0 = EVENT_1703 >> 8;
    R1 = EVENT_1703 & 0xFF;
    GUEST_CALL(ClearEventFlagFromImmediate);

    R0 = EVENT_1738 >> 8;
    R1 = EVENT_1738 & 0xFF;
    GUEST_CALL(SetEventFlagFromImmediate);

    R0 = A_CutsceneScriptOnMapWarp_8098a02;
    R1 = 0;
    GUEST_CALL(StartCutscene);

    RETURN_TO_CALLER();
}

/* ── startInMapWarpCutscene_80059D0 ── */
/* startInMapWarpCutscene_80059D0: in-map warp handler. Raises the two story event flags that
 * mark the warp event as started, then kicks off the "on in-map warp"
 * cutscene. Event flags live in word 0x17; this sets bits 3 and 0x38. */
Next startInMapWarpCutscene_80059D0(void) {
    R0 = 0x17;
    R1 = 3;
    GUEST_CALL(SetEventFlagFromImmediate);   /* set EVENT_1703 */

    R0 = 0x17;
    R1 = 0x38;
    GUEST_CALL(SetEventFlagFromImmediate);   /* set EVENT_1738 */

    R0 = A_CutsceneScriptOnInMapWarp_8098a78;
    R1 = 0;
    GUEST_CALL(StartCutscene);

    RETURN_TO_CALLER();
}

/* ── checkThenStartBattle_8005A8C ── */
/* Event flags (bank 0x17) that suppress an automatic battle start.
 * TestEventFlagFromImmediate takes the bank byte in r0 and the index byte in r1. */
#define EVENT_FLAG_PLAYER_ADVANCE_FORWARD 0x1717
#define EVENT_FLAG_1700                   0x1700
#define EVENT_FLAG_173E                   0x173E

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static u32 event_flag_set_checkThenStartBattle_8005A8C(u16 flag) {
    R0 = flag >> 8;          /* flag bank */
    R1 = flag & 0xFF;        /* flag index within the bank */
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();     /* NE = flag is set */
}

/* register form: calls sub_809E462 outside this file, which has no typed C interface yet */
static u32 sub_809E462_blocks_battle(void) {
    GUEST_CALL(sub_809E462);
    return !ZERO_FLAG();     /* NE = the guard trips */
}

/* register form: calls warp_LoadCurrentAndReadUnk10_8005F28 outside this file, which has no typed C interface yet */
static u32 warp_check_blocks_battle(void) {
    GUEST_CALL(warp_LoadCurrentAndReadUnk10_8005F28);
    return !ZERO_FLAG();     /* NE = a warp is in progress */
}

/* register form: calls TestPETMenuDataFlag outside this file, which has no typed C interface yet */
static u32 pet_menu_data_flag_set(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestPETMenuDataFlag);
    return !ZERO_FLAG();     /* NE = flag is set */
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static u32 screen_fade_active_checkThenStartBattle_8005A8C(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();     /* NE = a fade is running */
}

/* register form: calls IsCutsceneScriptNonNull outside this file, which has no typed C interface yet */
static u32 cutscene_script_non_null(void) {
    GUEST_CALL(IsCutsceneScriptNonNull);
    return !ZERO_FLAG();     /* NE = a cutscene script is running */
}

/* register form: calls chatbox_mask_eFlags2009F38 outside this file, which has no typed C interface yet */
static u32 chatbox_flag_set(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return !ZERO_FLAG();     /* NE = any of the masked chatbox flags is set */
}

/* register form: calls sub_80AA4C0 outside this file, which has no typed C interface yet */
static u32 get_battle_settings(void **settings_out) {
    GUEST_CALL(sub_80AA4C0);
    *settings_out = (void *)R0;
    return !ZERO_FLAG();     /* NE = pending battle settings are available */
}

/* register form: calls StartBattle outside this file, which has no typed C interface yet */
static void start_battle(void *settings, u32 flag) {
    R0 = (u32)settings;
    R1 = flag;
    GUEST_CALL(StartBattle);
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void set_screen_fade(u32 kind, u32 param) {
    R0 = kind;
    R1 = param;
    GUEST_CALL(SetScreenFade);
}

/* Every guard for an automatic battle start must pass; the first failing one
 * sends the original straight to its epilogue (the bne/beq to locret_8005AF2),
 * which here is a plain early return. */
static void check_then_start_battle_body(u32 gameState) {
    if (LD8(gameState) != 4)     /* oGameState_SubsystemIndex is at offset 0 */
        return;                  /* only while in the field subsystem */

    if (event_flag_set_checkThenStartBattle_8005A8C(EVENT_FLAG_PLAYER_ADVANCE_FORWARD))
        return;                  /* player is stepping forward */
    if (sub_809E462_blocks_battle())
        return;
    if (event_flag_set_checkThenStartBattle_8005A8C(EVENT_FLAG_1700))
        return;
    if (event_flag_set_checkThenStartBattle_8005A8C(EVENT_FLAG_173E))
        return;
    if (warp_check_blocks_battle())
        return;                  /* a warp is in progress */
    if (pet_menu_data_flag_set(1))
        return;                  /* a PET menu is open */
    if (!screen_fade_active_checkThenStartBattle_8005A8C())
        return;                  /* an active fade is required */
    if (cutscene_script_non_null())
        return;                  /* a cutscene is running */
    if (chatbox_flag_set(0x80))
        return;                  /* chatbox text is still on screen */

    /* Everything is clear: take the pending battle settings and start the fight. */
    void *settings;
    if (!get_battle_settings(&settings))
        return;                  /* no battle queued */

    start_battle(settings, 1);
    set_screen_fade(0x2C, 0x10); /* fade out into the battle */
}

static void check_then_start_battle(u32 toolkit) {
    /* register form: the original keeps the game-state pointer in guest r5 across
     * every call below (mov r5,r10; ldr r5,[r5,#oToolkit_GameStatePtr]) and only
     * restores the caller's value with the final pop {r5}, so it has to live in R5
     * while the callees run. */
    u32 savedR5 = R5;
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    R5 = gameState;

    check_then_start_battle_body(gameState);

    R5 = savedR5;                /* the original's pop {r5} */
}

/* checkThenStartBattle_8005A8C: checks the field-state guards and, if all pass,
 * starts the queued battle and fades the screen. r10 = toolkit. */
Next checkThenStartBattle_8005A8C(void) {
    check_then_start_battle(R10);
    RETURN_TO_CALLER();
}

/* ── nullsub_8005B6A ── */
/* nullsub_8005B6A: an empty stub; it preserves r5 and does nothing else. */
Next nullsub_8005B6A(void) {
    RETURN_TO_CALLER();
}

/* ── StartBattle ── */
/* StartBattle (0x08005BC8): start a battle from the overworld.
 * r0 = pointer to the battle data for the fight, r1 = flag ("also refresh the
 * player's overworld state"), r10 = toolkit. Preserves r2, r4-r7 and r10.
 *
 * Stores the battle data pointer, snapshots the current map group so the
 * post-battle code can return to the right place, optionally updates the
 * player's game state, switches the state machine to the battle subsystem
 * (index 8) and plays the battle-start jingle. */
Next StartBattle(void) {
    u32 battleData = R0;
    u32 updatePlayerState = R1;

    /* r4-r7 are scratch in the original (pushed/popped), so their entry
     * values have to be restored before returning. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 workArea = LD32(R10 + oToolkit_S2001c04_Ptr);

    ST32(gameState + oGameState_CurBattleDataPtr, battleData);

    /* Bit 0x4000 of the battle settings' effect mask: the battle data
     * pointer is also handed to the S2001c04 work area. */
    GUEST_CALL(GetBattleEffectsFromBattleSettings);
    u32 effects = R0;
    if (effects & 0x4000)
        ST32(workArea + oS2001c04_Unk_1c, battleData);

    /* Remember the map group the battle was started from
     * (oGameState_MapGroup is at offset 4 of the game state). */
    u32 mapGroup = LD16(gameState + 4);
    ST16(gameState + oGameState_LastMapGroup, mapGroup);

    if (updatePlayerState != 0)
        GUEST_CALL(updatePlayerGameState_800107A);

    /* Queue the battle init state machine as the next one; the subsystem
     * index is the first byte of the game state (offset 0). */
    ST8(gameState, 8);

    /* musicGameState_8000784 also looks at the game state pointer, the
     * work area pointer and the update flag (r5-r7); it is entered with
     * r0 = 8 like in the original. */
    R0 = 8;
    R5 = gameState;
    R6 = workArea;
    R7 = updatePlayerState;
    GUEST_CALL(musicGameState_8000784);

    R0 = SOUND_BATTLE_START;
    R5 = gameState;
    R6 = workArea;
    R7 = updatePlayerState;
    GUEST_CALL(PlaySoundEffect);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}
