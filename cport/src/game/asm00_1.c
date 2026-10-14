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

/* ── RunBattleObjectLogic ── */
/* The battle-paused gate and the time-stop gate select which objects continue updating.
 * PAUSE_UPDATE (0x4) is predefined. The disassembly's `tst r4, #0x10` while in time-stop is the
 * "update while time is stopped" flag. */
#define OBJECT_FLAG_UPDATE_WHILE_TIME_STOPPED 0x10

/* register form: object_Clear3RAMBytes_800371A is a leaf defined elsewhere with no typed C
 * interface yet. It is called once per frame to reset the per-frame object scratch area. */
static void clear_object_scratch_bytes(void) {
    GUEST_CALL(object_Clear3RAMBytes_800371A);
}

/* register form: battle_isTimeStop has no typed C interface yet; it returns its answer in Z
 * (Z set = time is flowing). */
static bool battle_is_time_stopped(void) {
    GUEST_CALL(battle_isTimeStop);
    return !ZERO_FLAG();
}

/* register form: object_800372A has no typed C interface yet; it takes the object core in r5.
 * r5 is saved and restored because the original keeps the caller's r5 across its call. */
static void run_object_post_update(u32 objectCore) {
    u32 savedR5 = R5;
    R5 = objectCore;
    GUEST_CALL(object_800372A);
    R5 = savedR5;
}

/* register form: the per-object update routine is reached at run time via a 2-level
 * jumptable in ROM, so there is no fixed symbol for GUEST_CALL. The original does
 * `push {r7}; mov lr, pc; bx r0; pop {r7}`. The pushed r7 must be live again on return,
 * and r4..r7 / LR must be preserved across the call exactly as the original's push/pop
 * does, so the adapter sets LR to the return site and the dispatch returns to it. */
static void call_object_update_routine(u32 updateRoutine, u32 indexTimes4,
                                       u8 flags, u32 objectCore,
                                       u32 gameState, u32 node) {
    u32 savedR0 = R0, savedR1 = R1;
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;
    u32 savedLR = LR;

    R0 = updateRoutine;
    R1 = indexTimes4;
    R4 = flags;
    R5 = objectCore;
    R6 = gameState;
    R7 = node;
    /* dispatch returns through LR; setting it to the address of the original `pop {r7}` makes
     * the guest pop r7 back off the stack, exactly matching the disassembly. */
    LR = 0x08003200 | 1u;
    GUEST_CALL_ADDR(updateRoutine);

    R0 = savedR0; R1 = savedR1;
    R4 = savedR4; R5 = savedR5; R6 = savedR6; R7 = savedR7;
    LR = savedLR;
}

/* Run one frame of battle object logic. For each node of eBattleObjectsLinkedListStart walk the
 * inner linked list until the sentinel. For every non-sentinel node call the routine chosen by
 * its type and index through the per-type jumptable, then call object_800372A on it. While
 * the battle is paused (menu open) only objects flagged PAUSE_UPDATE keep updating; while
 * time is stopped only objects flagged UPDATE_WHILE_TIME_STOPPED do. The original zeroes a
 * never-read stack slot each frame, which is omitted. */
static void run_battle_object_logic(u32 toolkit) {
    clear_object_scratch_bytes();

    u32 node = LD32(A_eBattleObjectsLinkedListStart + oBattleObject_LinkedList_Next);
    for (;;) {
        if (node == A_eBattleObjectsLinkedListSentinel_Prev)
            break;

        ST32(A_eUnkBattleObjectLinkedList, node);

        u32 objectCore = node + 0x10; /* the object's own fields start 0x10 into the node */
        u8 flags = LD8(objectCore + oBattleObject_Flags);
        u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
        bool paused = LD8(gameState + oGameState_BattlePaused) != 0;

        /* while paused, only PAUSE_UPDATE objects run; outside pause, all objects are candidates */
        bool shouldRun = !paused || (flags & OBJECT_FLAG_PAUSE_UPDATE) != 0;

        /* while time is stopped, only objects flagged UPDATE_WHILE_TIME_STOPPED run */
        if (shouldRun) {
            bool timeStopped = battle_is_time_stopped();
            if (!timeStopped || (flags & OBJECT_FLAG_UPDATE_WHILE_TIME_STOPPED) != 0) {
                /* dispatch: outer index is the low 4 bits of Type, inner index is Index */
                u8 type = LD8(objectCore + oBattleObject_Type) & 0xF;
                u32 typeJumptable = LD32(A_RunBattleObjectLogic_BattleObjectFunctionJumptableTable + (u32)type * 4);
                u32 index = LD8(objectCore + oBattleObject_Index);
                u32 updateRoutine = LD32(typeJumptable + index * 4);
                /* the original uses index*4 in the second LDR, and reads index directly with
                 * LSL #2; reusing index*4 here keeps the dispatch identical */
                call_object_update_routine(updateRoutine, index * 4,
                                           flags, objectCore, gameState, node);
            }
        }

        run_object_post_update(objectCore);

        /* advance the walk: read the current node's `next` again, matching the original's
         * `ldr r7, [r7, #oBattleObject_LinkedList_Next]` at the top of the loop body */
        node = LD32(node + oBattleObject_LinkedList_Next);
    }

    ST32(A_eUnkBattleObjectLinkedList, 0);
}

/* RunBattleObjectLogic() -> () */
Next RunBattleObjectLogic(void) {
    run_battle_object_logic(R10); /* r10 = toolkit pointer on entry */
    RETURN_TO_CALLER();
}

/* ── SpawnBattleObjectCommon ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool c, v; } call_ZeroFillByWord_result_SpawnBattleObjectCommon;
/* register form: generated wrapper: ZeroFillByWord has no typed C interface yet */
static inline call_ZeroFillByWord_result_SpawnBattleObjectCommon call_ZeroFillByWord_SpawnBattleObjectCommon(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ZeroFillByWord);
    return (call_ZeroFillByWord_result_SpawnBattleObjectCommon){ CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Descriptor field offsets within the 16-byte type entry at dword_80032D0 */
#define DESC_ACTIVE_BITFIELD   0x00
#define DESC_LINKED_LIST       0x04
#define DESC_LINKED_LIST_END   0x08
#define DESC_OBJECT_SIZE       0x0c
#define DESC_USER_SIZE         0x0d
#define DESC_UNK_0E            0x0e

typedef struct {
    u32 r0, r1, r2, r3, r5;
    u32 cmp_a, cmp_b;
} SpawnResult;

/* Encode an (c, v) pair into operands for SET_FLAGS_CMP so that the resulting
 * CMP produces the same carry and overflow flags. */
static void encode_cv_for_cmp(bool c, bool v, u32 *a, u32 *b) {
    if (c && !v)        { *a = 1;          *b = 0; }
    else if (!c && !v)  { *a = 0;          *b = 1; }
    else if (c && v)    { *a = 0x80000000u; *b = 1; }
    else                { *a = 1;          *b = 0x80000000u; }
}

static SpawnResult spawn_battle_object(u32 obj_type, u32 ctx_stack) {
    SpawnResult res;

    u32 type = obj_type & 0xF;
    u32 desc = (type << 4) + A_dword_80032D0;

    u32 active_addr = LD32(desc + DESC_ACTIVE_BITFIELD);
    u32 linked_list = LD32(desc + DESC_LINKED_LIST);
    u32 pool_end    = LD32(desc + DESC_LINKED_LIST_END);
    u32 stride      = LD8(desc + DESC_OBJECT_SIZE);
    u32 user_size   = LD8(desc + DESC_USER_SIZE);
    u32 unk_0e      = LD8(desc + DESC_UNK_0E);

    u32 mask   = 1u << 31;
    u32 bits   = LD32(active_addr);
    u32 cursor = linked_list;

    /* Scan the active-object bitfield from MSB to LSB for a free slot. */
    while (bits & mask) {
        mask >>= 1;
        cursor += stride;
        if (cursor < pool_end)
            continue;
        /* Pool exhausted – return NULL object. */
        res.r0 = type << 4;
        res.r1 = active_addr;
        res.r2 = mask;
        res.r3 = pool_end;
        res.r5 = 0;
        /* CMP cursor, pool_end at exit: cursor >= pool_end unsigned (GBA
         * addresses < 0x08000000), so C=1, V=0. */
        encode_cv_for_cmp(true, false, &res.cmp_a, &res.cmp_b);
        return res;
    }

    /* Mark the slot active and advance past the 16-byte linked-list header. */
    bits |= mask;
    ST32(active_addr, bits);
    cursor += 0x10;   /* object pointer (r5 in the original) */

    /* Zero the user data fields (skip the first word: linked-list pointer). */
    call_ZeroFillByWord_result_SpawnBattleObjectCommon zfw = call_ZeroFillByWord_SpawnBattleObjectCommon(cursor + 4, user_size);
    encode_cv_for_cmp(zfw.c, zfw.v, &res.cmp_a, &res.cmp_b);

    /* Write the default sub-type byte at the start of the object. */
    ST8(cursor, unk_0e);

    /* Copy the 5-word init record from the caller's stack frame. */
    u32 init0 = LD32(ctx_stack);
    u32 init1 = LD32(ctx_stack + 4);
    u32 init2 = LD32(ctx_stack + 8);
    u32 init3 = LD32(ctx_stack + 12);
    u32 init4 = LD32(ctx_stack + 16);

    ST8(cursor + oBattleObject_Index,  init0);
    ST32(cursor + oBattleObject_X,     init1);
    ST32(cursor + oBattleObject_Y,     init2);
    ST32(cursor + oBattleObject_Z,     init3);
    ST32(cursor + oBattleObject_Params, init4);

    res.r0 = init0;
    res.r1 = init1;
    res.r2 = init2;
    res.r3 = init3;
    res.r5 = cursor;
    return res;
}

Next SpawnBattleObjectCommon(void) {
    SpawnResult res = spawn_battle_object(R0, R1);
    R0 = res.r0;
    R1 = res.r1;
    R2 = res.r2;
    R3 = res.r3;
    R5 = res.r5;
    SET_FLAGS_CMP(res.cmp_a, res.cmp_b);
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

/* ── sub_8003374 ── */
/* sub_8003374(r0..r4 = spawn context words):
 * Spawn a battle object of type 3, passing it a five-word context block that
 * is built on the stack from the incoming r0-r4. Returns the new object in r5
 * (NULL if the spawn failed, in which case the object list is left alone).
 * r4/r6/r7 come back unchanged, as callers expect. */
Next sub_8003374(void) {
    u32 savedR7 = R7;

    /* Reserve the same frame the original builds:
     * [ r0 ][ r1 ][ r2 ][ r3 ][ r4 ][ saved r7 ][ saved lr ] */
    SP -= 0x1C;
    ST32(SP + 0x00, R0);
    ST32(SP + 0x04, R1);
    ST32(SP + 0x08, R2);
    ST32(SP + 0x0C, R3);
    ST32(SP + 0x10, R4);
    ST32(SP + 0x14, savedR7);
    ST32(SP + 0x18, LR);

    /* Spawn the object: type 3, context block just above the saved registers.
     * (stmia r7! leaves r7 pointing one word past the block, and the callee
     * sees that value.) */
    R7 = SP + 0x14;
    R0 = 3;
    R1 = SP;
    GUEST_CALL(SpawnBattleObjectCommon);

    /* On success, link the new object (r5) into the battle object list. */
    if (R5 != 0)
        GUEST_CALL(UpdateBattleObjectLinkedList);

    R7 = savedR7;
    SP += 0x1C;
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

/* ── InitializeStructsOfObjectType ── */
/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word(u32 dst, u32 byte_count) {
    R0 = dst;
    R1 = byte_count;
    GUEST_CALL(ZeroFillByWord);
}

/* Values the original leaves in r0..r3 on return; the adapter also derives the
 * exit flags from the final CMP of the init loop. */
typedef struct {
    u32 cursor;        /* r0: array pointer advanced past the last initialized object */
    u32 counter;       /* r1: loop counter as of the final comparison */
    u32 sprite_data;   /* r2: sprite-data byte written to every object */
    u32 object_count;  /* r3: number of objects for this type */
} InitStructsResult;

/* Zero and initialize the object pool for one object type (r0 = table index):
 * clears the active-object bitfield, clears the whole object array, then for
 * each object writes the sprite-data byte at +2 and the object's index at +3. */
static InitStructsResult initialize_structs_of_object_type(u32 object_type) {
    u32 table_entry = A_InitializeStructsOfObjectType_ObjectInitializationTable + object_type * 16;

    /* Clear the active-object bitfield: one bit per object, rounded up to whole words. */
    u32 bitfield_words = ((u32)LD8(table_entry + 0xC) + 0x1F) >> 5;
    u32 bitfield_base = LD32(A_ActiveObjectBitfieldPointers + object_type * 4);
    zero_fill_by_word(bitfield_base, bitfield_words * 4);

    /* Clear the entire object array for this type. */
    u32 linked_list_base = LD32(table_entry);
    u32 array_size = LD16(table_entry + 8);
    zero_fill_by_word(linked_list_base, array_size);

    /* Per-object header: sprite-data byte at +2, objects are object_size apart. */
    u32 cursor = LD32(table_entry + 4);
    u32 sprite_data = LD8(table_entry + 0xA);
    u32 object_size = LD8(table_entry + 0xB);
    u32 object_count = LD8(table_entry + 0xC);

    /* Each object's +3 byte gets the loop counter (its index), not a constant.
     * The original tests the counter only after the first object is written
     * (do/while), so one object is initialized even when the count is zero. */
    u32 counter = 0;
    do {
        ST8(cursor + 2, sprite_data);
        ST8(cursor + 3, counter);
        cursor += object_size;
        counter++;
    } while ((s32)counter < (s32)object_count);

    InitStructsResult result;
    result.cursor = cursor;
    result.counter = counter;
    result.sprite_data = sprite_data;
    result.object_count = object_count;
    return result;
}

/* InitializeStructsOfObjectType(r0 = object type) -> r0..r3, flags from the loop's final CMP */
Next InitializeStructsOfObjectType(void) {
    InitStructsResult result = initialize_structs_of_object_type(R0);
    R0 = result.cursor;
    R1 = result.counter;
    R2 = result.sprite_data;
    R3 = result.object_count;
    SET_FLAGS_CMP(R1, R3);
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

/* ── projectObjectToScreen_800362C ── */
/* Address of the mirror-x flag (eCamera.unk_4C = eCamera + 0x4C). */
#define CAMERA_MIRROR_FLAG 0x20099CCu

/* Offsets into the object struct holding its world position (halfwords). */
#define OBJ_POS_X 2
#define OBJ_POS_Y 6
#define OBJ_POS_Z 0xA

/* Visible window: a 240x160 screen centred at (120, 80), clipped with a margin. */
#define SCREEN_CLIP_MIN_X (-0x20)
#define SCREEN_CLIP_MAX_X 0x110   /* 0xF0 + 0x20 */
#define SCREEN_CLIP_MIN_Y (-0x20)
#define SCREEN_CLIP_MAX_Y 0xE0    /* 0xA0 + 0x40 */
#define CLIP_FALLBACK_X   0xA0
#define CLIP_FALLBACK_Y   0x50

/* Projection result, in the layout the original leaves in the registers.
 * r4-r7 are preserved by the original (push/pop), so they are never touched. */
typedef struct {
    u32 x;       /* r0: screen x, or the 0xA0 fallback when clipped */
    u32 y;       /* r1: screen y, or the 0x50 fallback when clipped */
    u32 visible; /* r2: 1 if the point is on screen, 0 if clipped */
    u32 camera;  /* r3: camera pointer, which the original leaves behind */
} ScreenProjection;

/* Project the world position of the object at `object` into screen space,
 * relative to the camera's next-frame scroll. When eCamera.unk_4C is set the
 * x axis is mirrored. Points outside the clip bounds collapse to the fixed
 * fallback position with the visible flag cleared. */
static ScreenProjection project_object_to_screen(u32 object, u32 toolkit)
{
    ScreenProjection out;
    u32 camera = LD32(toolkit + oToolkit_CameraPtr);

    s32 x = LDS16(object + OBJ_POS_X) - ((s32)LD32(camera + oCamera_nextX_3c) >> 16);
    if (LD8(CAMERA_MIRROR_FLAG) != 0)   /* eCamera.unk_4C: mirror-x flag */
        x = -x;
    x += 0x78;                      /* centre horizontally on the screen */

    s32 y = LDS16(object + OBJ_POS_Y) - ((s32)LD32(camera + oCamera_nextY_40) >> 16);
    y += 0x50;                      /* centre vertically on the screen */
    s32 depth = LDS16(object + OBJ_POS_Z) - ((s32)LD32(camera + oCamera_nextZ_44) >> 16);
    y -= depth;                     /* height shifts with Z distance */

    if (x >= SCREEN_CLIP_MIN_X && x < SCREEN_CLIP_MAX_X &&
        y >= SCREEN_CLIP_MIN_Y && y < SCREEN_CLIP_MAX_Y) {
        out.x = (u32)x;
        out.y = (u32)y;
        out.visible = 1;
    } else {
        out.x = CLIP_FALLBACK_X;
        out.y = CLIP_FALLBACK_Y;
        out.visible = 0;
    }
    out.camera = camera;
    return out;
}

/* projectObjectToScreen_800362C(r0 = object ptr, r10 = toolkit) -> r0 = screen x, r1 = screen y,
 * r2 = visible flag, r3 = camera ptr; r4-r7 preserved. */
Next projectObjectToScreen_800362C(void) {
    ScreenProjection out = project_object_to_screen(R0, R10);
    R0 = out.x;
    R1 = out.y;
    R2 = out.visible;
    R3 = out.camera;
    RETURN_TO_CALLER();
}

/* ── sub_8003694 ── */
/* Project a world-space object position through the camera into screen
 * coordinates and depth, then clip against the visible viewport.
 *
 * obj_ptr  : pointer to object (u32 x, u32 y, u32 z at offsets 0, 4, 8)
 * toolkit  : pointer to the toolkit struct (used to fetch camera pointer)
 *
 * Returns screen_x, screen_y, visibility flag, and a depth word.
 * Flags (nzcv) come from the depth word + 0x60 addition. */

#define VIEWPORT_X_MIN   (-0x20)
#define VIEWPORT_X_MAX    0x110
#define VIEWPORT_Y_MIN   (-0x20)
#define VIEWPORT_Y_MAX    0xE0
#define SCREEN_X_OFFSET   0x78   /* half-width of the 240-pixel GBA screen */
#define SCREEN_Y_OFFSET   0x52   /* baseline offset for floor Y */
#define DEPTH_BIAS        0x800000u
#define DRAW_Z_OFFSET     0x60

typedef struct {
    u32 screen_x;
    u32 screen_y;
    u32 visible;       /* 1 if inside viewport, 0 otherwise */
    u32 depth_raw;     /* before +DRAW_Z_OFFSET, for flag setting */
    u32 depth_final;   /* depth_raw + DRAW_Z_OFFSET */
} ProjectionResult;

static ProjectionResult project_object_to_screen_sub_8003694(u32 obj_ptr, u32 toolkit_ptr) {
    u32 camera = LD32(toolkit_ptr + oToolkit_CameraPtr);

    /* World-to-camera offsets */
    u32 dx = LD32(obj_ptr) - LD32(camera + 0x3C);
    u32 dy = LD32(obj_ptr + 4) - LD32(camera + 0x40);

    /* Screen X: average of the two offsets, shifted down, then centered */
    u32 screen_x = (u32)((s32)(dx + dy) >> 16) + SCREEN_X_OFFSET;

    /* Half-difference of Y and X offsets: a depth-like intermediate */
    u32 half_diff = (u32)((s32)(dy - dx) >> 1);

    /* Depth Z: camera Z biased + 0x800000, plus the half-difference */
    u32 cam_z = LD32(camera + 0x44);
    u32 depth_raw = (u32)((s32)(cam_z + DEPTH_BIAS + half_diff) >> 16);

    /* Screen Y: half_diff minus object Z plus camera Z, shifted down */
    u32 obj_z = LD32(obj_ptr + 8);
    u32 screen_y = (u32)((s32)(half_diff - obj_z + cam_z) >> 16) + SCREEN_Y_OFFSET;

    /* Clip test against viewport bounds (signed comparisons) */
    bool inside = (s32)screen_x >= (s32)VIEWPORT_X_MIN && (s32)screen_x < (s32)VIEWPORT_X_MAX &&
                  (s32)screen_y >= (s32)VIEWPORT_Y_MIN && (s32)screen_y < (s32)VIEWPORT_Y_MAX;

    ProjectionResult res;
    res.screen_x   = screen_x;
    res.screen_y   = screen_y;
    res.visible    = inside ? 1 : 0;
    res.depth_raw  = depth_raw;
    res.depth_final = depth_raw + DRAW_Z_OFFSET;
    return res;
}

/* sub_8003694(r0 = obj_ptr, r10 = toolkit) -> r0..r3, nzcv */
Next sub_8003694(void) {
    ProjectionResult p = project_object_to_screen_sub_8003694(R0, R10);
    R0 = p.screen_x;
    R1 = p.screen_y;
    R2 = p.visible;
    R3 = p.depth_final;
    SET_FLAGS_ADD(p.depth_raw, DRAW_Z_OFFSET);
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

/* ── object_800372A ── */
/* Read the slot table's count pointer and slot byte offset (r2/r3 in the original).
 * Registers the object pointer at the slot selected by its Type nibble. */
typedef struct {
    u32 count_ptr;
    u32 slot_offset;
} SlotInfo;

static SlotInfo register_object_in_slot(u32 object) {
    u8 type = LD8(object + oBattleObject_Type);
    u32 pair_addr = A_object_PtrPairs_800374C + ((type & 0xF) << 3);
    u32 slot_base = LD32(pair_addr);
    u32 count_ptr = LD32(pair_addr + 4);
    u8 count = LD8(count_ptr);
    u32 slot_offset = (u32)count << 2;
    ST32(slot_base + slot_offset, object);
    ST8(count_ptr, count + 1);
    SlotInfo info;
    info.count_ptr = count_ptr;
    info.slot_offset = slot_offset;
    return info;
}

Next object_800372A(void) {
    SlotInfo info = register_object_in_slot(R5);
    R2 = info.count_ptr;
    R3 = info.slot_offset;
    RETURN_TO_CALLER();
}

/* ── ZeroFillObjectInteractionAreas_800378C ── */
/* ZeroFillObjectInteractionAreas_800378C: clear the overworld object
 * interaction-area pool and reset the number of entries it holds.
 * Flags on return come from ZeroFillByWord's final flag-setting instruction. */
Next ZeroFillObjectInteractionAreas_800378C(void) {
    R0 = A_eOWObjectInteractionAreas;
    R1 = 0x280; /* pool size in bytes */
    GUEST_CALL(ZeroFillByWord);

    ST32(A_eNumOWObjectInteractionAreas, 0);
    RETURN_TO_CALLER();
}

/* ── checkOWObjectInteractions_80037f4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_checkOWObjectInteractionAreasOverlap_8003894_result;
/* register form: generated wrapper: checkOWObjectInteractionAreasOverlap_8003894 has no typed C interface yet */
static inline call_checkOWObjectInteractionAreasOverlap_8003894_result call_checkOWObjectInteractionAreasOverlap_8003894(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(checkOWObjectInteractionAreasOverlap_8003894);
    return (call_checkOWObjectInteractionAreasOverlap_8003894_result){ R0, R2 };
}

/* Scan all ordered pairs (i, j) of OW interaction areas registered for this frame.
 * When areas i and j have overlapping mask bits AND their hitboxes overlap,
 * OR the combined bits into each area's event-flag word, and if area i carries
 * the snap-player flag (0x200000 in Unk_08), copy area j's X/Y/Z to the player.
 * Clears eNumOWObjectInteractionAreas to 0 before returning. */

typedef struct { u32 r4; u32 r6; } OWPairResult;

static OWPairResult scan_ow_pairs(u32 r4_in, u32 r6_in) {
    u32 num = LD32(A_eNumOWObjectInteractionAreas);
    if ((s32)num <= 1) {
        ST32(A_eNumOWObjectInteractionAreas, 0);
        OWPairResult passthrough = { r4_in, r6_in };
        return passthrough;
    }

    u32 base = A_eOWObjectInteractionAreas;
    u32 r4 = r4_in;
    u32 r6 = 0;

    /* Outer loop: i = 0 .. num-1 */
    for (u32 i = 0; i < num; i++) {
        u32 outer = base + i * oOWObjectInteractionArea_Size;

        /* Inner loop: j = 0 .. num-1, skip j == i */
        for (u32 j = 0; j < num; j++) {
            if (i == j)
                continue;
            u32 inner = base + j * oOWObjectInteractionArea_Size;
            r4 = inner;

            /* Quick reject: shared bits between outer Unk_0c and inner Unk_08 */
            u32 outer_mask = LD32(outer + oOWObjectInteractionArea_Unk_0c);
            u32 inner_mask = LD32(inner + oOWObjectInteractionArea_Unk_08);
            if ((outer_mask & inner_mask) == 0)
                continue;

            /* Full hitbox overlap test */
            OWPairResult dummy; /* not used, just for type; actual call below */
            (void)dummy;
            call_checkOWObjectInteractionAreasOverlap_8003894_result ov =
                call_checkOWObjectInteractionAreasOverlap_8003894(outer, inner);
            if (ov.r0 == 0)
                continue;

            /* OR combined mask into outer area's event-flag word (*Unk_10) */
            u32 combined = outer_mask & inner_mask;
            u32 outer_evt = LD32(outer + oOWObjectInteractionArea_Unk_10);
            ST32(outer_evt, combined | LD32(outer_evt));

            /* If outer has the snap-player flag, copy inner coords to player */
            u32 outer_unk08 = LD32(outer + oOWObjectInteractionArea_Unk_08);
            if (outer_unk08 & OW_OBJECT_INTERACTION_AREA_FLAG_0x200000) {
                u32 player = A_eOWPlayerObject;
                ST16(player + oOWPlayerObject_Unk_72,
                     LD16(inner + oOWObjectInteractionArea_X));
                ST16(player + oOWPlayerObject_Unk_74,
                     LD16(inner + oOWObjectInteractionArea_Y));
                ST16(player + oOWPlayerObject_Unk_76,
                     LD16(inner + oOWObjectInteractionArea_Z));
            }

            /* OR (outer Unk_08) & (inner Unk_0c) into inner area's event word */
            u32 inner_mask2 = LD32(inner + oOWObjectInteractionArea_Unk_0c);
            u32 combined2 = outer_unk08 & inner_mask2;
            u32 inner_evt = LD32(inner + oOWObjectInteractionArea_Unk_10);
            ST32(inner_evt, combined2 | LD32(inner_evt));
        }

        r6 = i + 1;
    }

    ST32(A_eNumOWObjectInteractionAreas, 0);
    OWPairResult out = { r4, r6 };
    return out;
}

/* checkOWObjectInteractions_80037f4() -> r4 r5 r6 */
Next checkOWObjectInteractions_80037f4(void) {
    OWPairResult result = scan_ow_pairs(R4, R6);
    R4 = result.r4;
    R5 = R5;
    R6 = result.r6;
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

/* ── sub_80039AA ── */
/* Indirect-dispatch helper for sub_80039AA.
 * The original sets r5=eScenarioEffectState, r7=jt-entry, r1=16, r0=handler,
 * then bx r0. The handler may clobber r2-r12; r4-r7 are saved/restored here. */
static u32 call_effect_handler(u32 handler, u32 status_addr, u32 entry_addr) {
    /* register form: bx through a runtime-computed handler address */
    SP -= 16;
    ST32(SP + 0, R4);
    ST32(SP + 4, R5);
    ST32(SP + 8, R6);
    ST32(SP + 12, R7);

    R0 = handler;
    R1 = 16;
    R5 = status_addr;
    R7 = entry_addr;

    GUEST_CALL_ADDR(handler);

    R4 = LD32(SP + 0);
    R5 = LD32(SP + 4);
    R6 = LD32(SP + 8);
    R7 = LD32(SP + 12);
    SP += 16;
    return R0;
}

/* If the scenario effect is active and incomplete, call its jump-table handler.
 * The handler's modifications to r2-r12 are the outputs; r4-r7 are preserved. */
static void dispatch_scenario_effect(u32 r2, u32 r3, u32 r4, u32 r6,
                                     u32 r8, u32 r9, u32 r10, u32 r11, u32 r12) {
    (void)r2; (void)r3; (void)r4; (void)r6;
    (void)r8; (void)r9; (void)r10; (void)r11; (void)r12;
    u32 status = LD8(A_eScenarioEffectState2000780);
    if (status == 0 || (status & 0x80))
        return;
    u32 sub  = LD8(A_eScenarioEffectState2000780 + 1);
    u32 entry = A_jt_80038E8 + sub * 16;
    u32 handler = LD32(entry);
    call_effect_handler(handler, A_eScenarioEffectState2000780, entry);
}

/* sub_80039AA: scenario-effect dispatcher.
 * Outputs: r2-r12. Preserved: r4-r7. Clobbers: nz c v r0 r1 lr. */
Next sub_80039AA(void) {
    u32 lr_save = LR;
    dispatch_scenario_effect(R2, R3, R4, R6, R8, R9, R10, R11, R12);
    LR = lr_save;
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
    u32 poolEnd = A_eOWPlayerObjectEnd; /* = eOWPlayerObjectEnd */

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

/* ── TryUpdateEachOWPlayerObject_8003BA2 ── */
/* Iterate over every OW player object slot (eOWPlayerObject array).
 * Active slots run their update routine through the off_8003B48 jump table, selected
 * by the routine-id byte at object+1. While the battle is paused (gameState flag),
 * only objects carrying OBJECT_FLAG_PAUSE_UPDATE keep updating.
 *
 * register form: the routines are dispatched through a jump table (`ldr r0,[table,id*4];
 * bx r0`) so no typed wrapper exists. They are raw GBA code that reads its arguments
 * (function ptr, table offset, object pointer, game state ptr, alliance ptr, misc ptr)
 * directly from R0-R7 of the guest register file. R4/R6 are set once from the caller's
 * R8/R12 before the loop (matching the prologue's movs) and flow through to callees.
 * All callee-saved registers and the caller's R8/R9/R12 are restored before returning,
 * replicating the original's push {r4-r6} … pop sequence. */
static void try_update_ow_players(u32 toolkit, u32 entryR8, u32 entryR12) {
    /* Save the registers that the loop and callees will modify */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;
    u32 savedR8 = R8, savedR9 = R9, savedR12 = R12;

    u32 object = A_eOWPlayerObject;
    u32 tableAddr = A_off_8003B48;
    u32 endAddr = A_eOWPlayerObjectEnd;

    /* Park the caller's R8/R12 in R4/R6 for the callees (matches `mov r4,r8; mov r6,r12`) */
    R4 = entryR8;
    R6 = entryR12;

    while (object < endAddr) {
        u8 flags = LD8(object + oObjectHeader_Flags);

        if (flags & OBJECT_FLAG_ACTIVE) {
            u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
            bool paused = (LD8(gameState + oGameState_BattlePaused) != 0);

            /* Skip if battle is paused and object doesn't opt into pause-update */
            if (!paused || (flags & OBJECT_FLAG_PAUSE_UPDATE)) {
                u8 routineId = LD8(object + 1);        /* update-routine id at +0x1 */
                u32 tableOffset = (u32)routineId << 2; /* byte offset into word table */
                u32 funcPtr = LD32(tableAddr + tableOffset);

                /* Set up callee's register-file arguments */
                R0 = funcPtr;
                R1 = tableOffset;
                R5 = object;
                R7 = gameState;
                /* R4, R6 retain their current values (callee-saved pass-through) */

                GUEST_CALL_ADDR(funcPtr);
            }
        }

        object += oOWPlayerObject_Size;
    }

    /* Restore callee-saved registers and the caller's R8/R9/R12 (the original's
     * pop {r4-r6}; mov r8,r4; mov r9,r5; mov r12,r6; pop {r4-r7,pc}) */
    R4 = savedR4; R5 = savedR5; R6 = savedR6; R7 = savedR7;
    R8 = savedR8; R9 = savedR9; R12 = savedR12;
}

/* TryUpdateEachOWPlayerObject_8003BA2(r8, r9, r10 = toolkit, r11, r12) -> ()
 * Returns with R8/R9/R12 restored to entry, R4-R7 preserved, R0/R1/R3/LR clobbered,
 * R2/R10/R11 passed through from callees. */
Next TryUpdateEachOWPlayerObject_8003BA2(void) {
    try_update_ow_players(R10, R8, R12);
    RETURN_TO_CALLER();
}

/* ── sub_8003BF4 ── */
/* Overworld object layout (offsets as in the original asm). */
#define OWOBJ_STRIDE    0xC8  /* bytes per entry in the overworld object list        */
#define OWOBJ_POSITION  0x1C  /* world position, input to the sprite projection      */
#define OWOBJ_SPRITE    0x90  /* sprite/OAM state that follows each object           */
#define OWOBJ_QUEUE_CNT 0x24  /* +0x24 into the sprite state: queued OAM piece count */

#define OW_FLAG_ACTIVE  0x01  /* object is taking part (bit 0 of the flag byte)      */
#define OW_FLAG_VISIBLE 0x02  /* OBJECT_FLAG_VISIBLE in the disassembly's enum       */

/* register form: calls sprite_projectAndCull outside this file, which has no typed C interface yet.
 * The owning object is also passed in r5, which the original keeps live across the call. */
static void sprite_project_and_cull(u32 position, u32 object) {
    R0 = position;
    R5 = object;
    GUEST_CALL(sprite_projectAndCull);
}

/* register form: calls sprite_ensureGfxResident outside this file, which has no typed C interface yet.
 * It also reads the owning object from r5, as in the original. */
static void sprite_ensure_gfx_resident(u32 slot, u32 object) {
    R0 = slot;
    R5 = object;
    GUEST_CALL(sprite_ensureGfxResident);
}

/* register form: calls sprite_queueFramePieces outside this file, which has no typed C interface yet.
 * The sprite state to queue from is a hidden argument in r5 (the original does
 * `add r5, #0x90` immediately before the call); r0/r1 carry the other two arguments. */
static void sprite_queue_frame_pieces(u32 a, u32 b, u32 sprite) {
    R0 = a;
    R1 = b;
    R5 = sprite;
    GUEST_CALL(sprite_queueFramePieces);
}

/* register form: calls sub_80028C0 outside this file, which has no typed C interface yet.
 * r5 must hold the address just past the walked object list, as the original leaves it,
 * and r1 is 0 there (the original's restored loop counter). */
static void commit_sprite_queue(u32 arg, u32 list_end) {
    R0 = arg;
    R1 = 0;
    R5 = list_end;
    GUEST_CALL(sub_80028C0);
}

/* Per-frame sprite update for the overworld object list that starts at the player.
 * The original walks r0 = 1 entry, i.e. the player object only. A visible object is
 * projected to the screen, its graphics kept in VRAM and its OAM pieces queued; a
 * hidden object forgets its queued pieces and is only re-projected while it is still
 * active (flag bit 0), so its screen position stays fresh. The pieces queued this
 * frame are committed at the end. */
static void update_overworld_player_sprites(void) {
    u32 object = A_eOWPlayerObject;

    for (u32 i = 0; i < 1; i++, object += OWOBJ_STRIDE) {  /* count hard-coded to 1 */
        u8 flags = LD8(object);

        if (flags & OW_FLAG_VISIBLE) {
            sprite_project_and_cull(object + OWOBJ_POSITION, object);
            sprite_ensure_gfx_resident(0, object);
            sprite_queue_frame_pieces(2, 0x60, object + OWOBJ_SPRITE);  /* original's draw arguments */
        } else {
            /* Hidden: drop anything still queued for this object. */
            ST32(object + OWOBJ_SPRITE + OWOBJ_QUEUE_CNT, 0);

            if (flags & OW_FLAG_ACTIVE)
                sprite_project_and_cull(object + OWOBJ_POSITION, object);
        }
    }

    /* object now points just past the walked list, as the original's r5 does here. */
    commit_sprite_queue(0, object);
}

/* sub_8003BF4(): no results in registers. r4-r7 and r8/r9/r12 are returned exactly as
 * the caller left them (the original pushes and pops them); save them here because the
 * guest calls above pass hidden arguments through r5 and may clobber the set. */
Next sub_8003BF4(void) {
    u32 keep_r4 = R4, keep_r5 = R5, keep_r6 = R6, keep_r7 = R7, keep_r8 = R8,
        keep_r9 = R9, keep_r12 = R12;
    update_overworld_player_sprites();
    R4 = keep_r4, R5 = keep_r5, R6 = keep_r6, R7 = keep_r7, R8 = keep_r8,
        R9 = keep_r9, R12 = keep_r12;
    RETURN_TO_CALLER();
}

/* ── sub_8003C70 ── */
/* sub_8003C70: reset the something tracked by sub_80028C0 (r0 = 0 as its argument). */
Next sub_8003C70(void) {
    R0 = 0;
    GUEST_CALL(sub_80028C0);
    RETURN_TO_CALLER();
}

/* ── sub_8003E18 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: sub_80028C0 has no typed C interface yet */
static inline void call_sub_80028C0(u32 r0) {
    R0 = r0;
    GUEST_CALL(sub_80028C0);
}

/* Sprite update pass over the battle object list.
 *
 * byte_2036778: number of object slots to visit.
 * dword_2039A10: address of an array of object pointers.
 *
 * For each object, status byte at offset 0:
 *   bit 1 set -> fully drawn: project/cull (transform at +0x34), ensure gfx resident,
 *                 queue frame pieces (piece list at +0x90).
 *   bit 1 clear -> zero the piece counter (+0x90+0x24); if bit 0 is set, still
 *                 project/cull so its transform stays up to date.
 * After the loop, sub_80028C0(1) finalizes the frame.
 *
 * Preserves r4-r9, r12 across callee clobbering; r3 left as last callee produces. */

/* --- Helpers for sprite calls through literal-pool addresses --- */

static void call_project_and_cull(u32 transform_ptr, u32 obj) {
    /* register form: sprite_projectAndCull has no generated wrapper;
       args passed in R0 and R5 via GUEST_CALL_ADDR */
    R0 = transform_ptr;
    R5 = obj;
    GUEST_CALL_ADDR(A_sprite_projectAndCull + 1);
}

static void call_ensure_gfx_resident(u32 obj) {
    /* register form: sprite_ensureGfxResident has no generated wrapper;
       args passed in R0 and R5 via GUEST_CALL_ADDR */
    R0 = 1;
    R5 = obj;
    GUEST_CALL_ADDR(A_sprite_ensureGfxResident + 1);
}

static void call_queue_frame_pieces(u32 piece_list) {
    /* register form: sprite_queueFramePieces has no generated wrapper;
       args passed in R0, R1 and R5 via GUEST_CALL_ADDR */
    R0 = 2;
    R1 = 0x40;
    R5 = piece_list;
    GUEST_CALL_ADDR(A_sprite_queueFramePieces + 1);
}

/* --- Main logic --- */

static void update_sprites(void) {
    /* register form: caller-preserved r4-r9, r12 must be saved/restored
       across sprite/sub_80028C0 calls which clobber them */
    u32 save_r4 = R4, save_r5 = R5, save_r6 = R6, save_r7 = R7;
    u32 save_r8 = R8, save_r9 = R9, save_r12 = R12;

    s32 remaining = (s32)LD8(A_byte_2036778);
    u32 slot = A_dword_2039A10;

    while (--remaining >= 0) {
        u32 obj = LD32(slot);
        u32 status = LD8(obj);
        slot += 4;

        if (status & 2) {
            /* fully visible: project, load gfx, queue sprite pieces */
            call_project_and_cull(obj + 0x34, obj);
            call_ensure_gfx_resident(obj);
            call_queue_frame_pieces(obj + 0x90);
        } else {
            /* not drawn: clear queued piece count */
            ST32(obj + 0x90 + 0x24, 0);
            /* if bit 0 still set, keep its transform projected/culled */
            if (status & 1)
                call_project_and_cull(obj + 0x34, obj);
        }
    }

    /* finalize this update pass */
    call_sub_80028C0(1);

    R4  = save_r4;
    R5  = save_r5;
    R6  = save_r6;
    R7  = save_r7;
    R8  = save_r8;
    R9  = save_r9;
    R12 = save_r12;
}

/* sub_8003E18 -> (void); clobbers r0-r2, r3, flags, lr; preserves r4-r9, r10-r12 */
Next sub_8003E18(void) {
    update_sprites();
    RETURN_TO_CALLER();
}

/* ── sub_8003E98 ── */
/* sub_8003E98: invoke the battle-object reset routine in mode 1
 * (the code following the pop {pc} in the original — a loop zeroing the
 * SpriteData of every eT1BattleObject slot — is unreachable and omitted).
 * No outputs; all registers may be clobbered. */
Next sub_8003E98(void) {
    R0 = 1;
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

/* ── SpawnOverworldNPCObject ── */
/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word_SpawnOverworldNPCObject(u32 address, u32 byte_count) {
    R0 = address;
    R1 = byte_count;
    GUEST_CALL(ZeroFillByWord);
}

/* Claim the first free slot in the overworld NPC table (one bit per object in
 * eActiveOverworldNPCObjectsBitfield, scanned from slot 0 = top bit), clear the
 * object's state and fill in its placement: index, x, y, z and the movement
 * word (speed/timer bytes). Returns the claimed object's address, or 0 when
 * every slot is already in use. */
static u32 spawn_overworld_npc(u32 index, u32 x, u32 y, u32 z, u32 movement) {
    u32 active = LD32(A_eActiveOverworldNPCObjectsBitfield);
    u32 slotBit = 0x80000000;             /* slot 0 owns the top bit */
    u32 npc = A_eOverworldNPCObject0_Flags;
    const u32 tableEnd = A_byte_2006530;  /* one past the last NPC object */

    while (active & slotBit) {            /* skip objects that are spawned */
        slotBit >>= 1;
        npc += oOverworldNPCObject_Size;
        if ((s32)npc >= (s32)tableEnd)    /* walked past the table: it is full */
            return 0;
    }

    ST32(A_eActiveOverworldNPCObjectsBitfield, active | slotBit);

    /* wipe everything from the movement byte to the end of the object,
     * then fill in the header and the placement fields */
    zero_fill_by_word_SpawnOverworldNPCObject(npc + oOverworldNPCObject_MovementSpeed_MovementTimer_Undetected_06_Unk_07, 0x9C);
    ST8(npc, OBJECT_FLAG_ACTIVE | OBJECT_FLAG_STOP_SPRITE_UPDATE);
    ST8(npc + oOverworldNPCObject_Index, index);
    ST32(npc + oOverworldNPCObject_X, x);
    ST32(npc + oOverworldNPCObject_Y, y);
    ST32(npc + oOverworldNPCObject_Z, z);
    ST32(npc + oOverworldNPCObject_MovementSpeed_MovementTimer_Undetected_06_Unk_07, movement);
    return npc;
}

/* SpawnOverworldNPCObject(r0 = index, r1 = x, r2 = y, r3 = z, r4 = movement)
 * -> r5 = new object (0 if the table is full); r0-r4 come back unchanged. */
Next SpawnOverworldNPCObject(void) {
    u32 index = R0, x = R1, y = R2, z = R3, movement = R4;
    /* A full table returns with C/V left over from the original's
     * "slot vs. table end" compare (equal: C=1, V=0). Prime them here; a
     * successful spawn overwrites every flag inside ZeroFillByWord. */
    SET_FLAGS_CMP(0, 0);
    R5 = spawn_overworld_npc(index, x, y, z, movement);
    R0 = index, R1 = x, R2 = y, R3 = z;   /* r0-r3 die in ZeroFillByWord, r4 survives it */
    SET_FLAGS_NZ(R5);                     /* success clears Z, a full table sets it */
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

/* ── npc_clearOverworldNpcStateWord_8004702 ── */
/* ---------------------------------------------------------------------------
 * npc_clearOverworldNpcStateWord_8004702 — reset one state word of every overworld NPC object.
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
 * A_eOverworldNPCObjects is the table base (== eOverworldNPCObjects in
 * the disassembly's literal pool). */
static NpcResetResult clear_overworld_npc_active_words(void)
{
    u32 base = A_eOverworldNPCObjects;
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

/* npc_clearOverworldNpcStateWord_8004702() -> r0..r3, nz c v (no inputs) */
Next npc_clearOverworldNpcStateWord_8004702(void)
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

/* ── scenario_prepareEnterMap_8004D48 ── */
/* Event flag identifiers (page<<8 | bit) */
#define EVT_1703  0x1703u
#define EVT_171B  0x171Bu
#define EVT_1715  0x1715u   /* EVENT_EVENT_CUR_DIR_LOCKED */
#define EVT_172D  0x172Du
#define EVT_0091  0x0091u

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void evt_clear(u16 flag) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* register form: calls SetEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void evt_set(u16 flag) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(SetEventFlagFromImmediate);
}

/* register form: calls SetPlayerCanMoveEventFlag outside this file, which has no typed C interface yet.
 * R5 = game-state pointer (ambient argument present in original register state). */
static void call_set_player_can_move(u32 gs) {
    R5 = gs;
    GUEST_CALL(SetPlayerCanMoveEventFlag);
}

/* register form: calls npc_clearOverworldNpcStateWord_8004702 outside this file, which has no typed C interface yet */
static void call_sub_8004702(u32 gs) {
    R5 = gs;
    GUEST_CALL(npc_clearOverworldNpcStateWord_8004702);
}

/* register form: calls scenario_ClearEffectActiveFlag_800399A outside this file, which has no typed C interface yet.
 * R5 = game-state pointer (implicit argument). */
static void call_clear_effect_active(u32 gs) {
    R5 = gs;
    GUEST_CALL(scenario_ClearEffectActiveFlag_800399A);
}

/* register form: calls event_ackNewFlagByte_8003AEA outside this file, which has no typed C interface yet */
static void call_ack_new_flag_byte(u32 gs) {
    R5 = gs;
    GUEST_CALL(event_ackNewFlagByte_8003AEA);
}

/* register form: calls pet_ClearMenuData_811EC00 outside this file, which has no typed C interface yet */
static void call_pet_clear_menu(u32 gs) {
    R5 = gs;
    GUEST_CALL(pet_ClearMenuData_811EC00);
}

/* register form: calls encryption_applyPackList_800B110 outside this file, which has no typed C interface yet */
static void call_encryption_apply_pack(u32 gs) {
    R5 = gs;
    GUEST_CALL(encryption_applyPackList_800B110);
}

/* register form: calls sub_803C3E0 outside this file, which has no typed C interface yet.
 * Returns R0; caller branches on Z (beq). */
/* register form: relies on sub_803C3E0 leaving r0: its Z is r0 == 0 (ldrb r0; tst r0, r0), so testing r0
 * is the original's beq */
static u32 call_sub_803C3E0(u32 gs) {
    R5 = gs;
    GUEST_CALL(sub_803C3E0);
    return R0;
}

/* register form: calls shop_copyItemTableToShopData_8048C98 outside this file, which has no typed C interface yet */
static void call_shop_copy_item_table(u32 gs) {
    R5 = gs;
    GUEST_CALL(shop_copyItemTableToShopData_8048C98);
}

/* Reset game state for entering a new map location.
 * Clears the game-state struct, resets auxiliary tracking data and event flags,
 * allows player movement, and refreshes the shop item table. */
static void reset_state_for_new_location(u32 toolkit) {
    u32 gs = LD32(toolkit + oToolkit_GameStatePtr);

    /* EnterMap subsystem: clear index, set fade params, zero interaction fields */
    ST8(gs + oGameState_SubsystemIndex,        0);
    ST8(gs + oGameState_EnterMapFadeParam1,    8);
    ST8(gs + oGameState_EnterMapFadeParam2,    0x10);
    ST8(gs + oGameState_CoordInteractionValue, 0);
    ST8(gs + 3,                                0);  /* Unk_03 */
    ST8(gs + oGameState_Unk_15,                0);
    ST8(gs + oGameState_BattlePaused,          0);
    ST32(gs + oGameState_Unk_74,              0);

    /* Default BGM indicator for overworld */
    ST8(gs + oGameState_BGMusicIndicator,     0x63);

    /* Clear per-map interaction flags */
    ST8(gs + oGameState_Unk_11,                0);
    ST8(gs + oGameState_Unk_10,                0);

    /* Map-transition tracking byte */
    ST8(0x200F410, 0);

    /* Auxiliary struct at 0x200ACE0: clear, set sentinel in byte_200ACFC */
    ST8(0x200ACE0,          0);
    ST32(0x200ACE0 + 0x18,  0);
    ST32(0x200ACE0 + 0x20,  0);
    ST8(0x200ACE0 + 0x1C,   0xFF);

    /* Allow the player to move in the new location */
    call_set_player_can_move(gs);

    /* Camera interaction flag and global transition byte */
    ST8(0x20099CC, 0);  /* eCamera + 0x4C */
    ST8(0x20096D0, 0);  /* dword_20096D0 */

    call_sub_8004702(gs);

    /* Clear navigation / interaction event flags */
    evt_clear(EVT_1703);
    evt_clear(EVT_171B);
    evt_clear(EVT_1715);  /* CUR_DIR_LOCKED */
    evt_clear(EVT_172D);

    /* Reset scenario effects, ack flag bytes, pet menu, encryption pack list */
    call_clear_effect_active(gs);
    call_ack_new_flag_byte(gs);
    call_pet_clear_menu(gs);
    call_encryption_apply_pack(gs);

    /* Clear "transition pending" (EVENT_91) */
    evt_clear(EVT_0091);

    /* Re-set EVENT_91 if transition-check returns non-zero (original beq skips the set) */
    if (call_sub_803C3E0(gs) != 0)
        evt_set(EVT_0091);

    /* Refresh shop inventory from item table */
    call_shop_copy_item_table(gs);
}

/* scenario_prepareEnterMap_8004D48(r10 = Toolkit) — preserves r4-r7; r2/r3 from callees are free */
Next scenario_prepareEnterMap_8004D48(void) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    reset_state_for_new_location(R10);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    RETURN_TO_CALLER();
}

/* ── initNewGameData_8004DF0 ── */
/* initNewGameData: sets all game state to fresh-start values and fires one-time init on subsystems */

#define INITIAL_MAP_GROUP   0x100
#define DEFAULT_FACING      4
#define CHIP_RECORD_BYTES   (0x3C * 3)

/* ---------- event-flag helpers ---------- */

/* register form: calls SetEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void set_event_flag(u8 entry_idx, u8 byte_idx) {
    R0 = entry_idx;
    R1 = byte_idx;
    GUEST_CALL(SetEventFlagFromImmediate);
}

/* register form: calls SetEventFlagRangeFromImmediate outside this file, which has no typed C interface yet */
static void set_event_flag_range(u8 entry_idx, u8 byte_idx, u32 count) {
    R0 = entry_idx;
    R1 = byte_idx;
    R2 = count;
    GUEST_CALL(SetEventFlagRangeFromImmediate);
}

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void clear_event_flag(u8 entry_idx, u8 byte_idx) {
    R0 = entry_idx;
    R1 = byte_idx;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* ---------- subsystem init helpers (no args) ---------- */

/* register form: calls toolkit_ClearAllEventFlags_802F0C4 outside this file, which has no typed C interface yet */
static void call_clear_all_event_flags(void) { GUEST_CALL(toolkit_ClearAllEventFlags_802F0C4); }

/* register form: calls encryption_initAll_8006d00 outside this file, which has no typed C interface yet */
static void call_encryption_init(void) { GUEST_CALL(encryption_initAll_8006d00); }

/* register form: calls keyItems_ResetInventoryAndGrantScenarioItems_803CD74 outside this file, which has no typed C interface yet */
static void call_reset_key_items(void) { GUEST_CALL(keyItems_ResetInventoryAndGrantScenarioItems_803CD74); }

/* register form: calls resetPaletteScratch_802F2C8 outside this file, which has no typed C interface yet */
static void call_sub_802F2C8(void) { GUEST_CALL(resetPaletteScratch_802F2C8); }

/* register form: calls sub_81284A4 outside this file, which has no typed C interface yet */
static void call_sub_81284A4(void) { GUEST_CALL(sub_81284A4); }

/* register form: calls owPlayer_setS2000aa0_param0x0to0x2_with0x40_0x40_0x0_respectively_809e2c2 outside this file, which has no typed C interface yet */
static void call_owplayer_init(void) { GUEST_CALL(owPlayer_setS2000aa0_param0x0to0x2_with0x40_0x40_0x0_respectively_809e2c2); }

/* register form: calls SetDefaultOWPlayerNaviColorShader outside this file, which has no typed C interface yet */
static void call_owplayer_shader(void) { GUEST_CALL(SetDefaultOWPlayerNaviColorShader); }

/* register form: calls sub_809E304 outside this file, which has no typed C interface yet */
static void call_sub_809E304(void) { GUEST_CALL(sub_809E304); }

/* register form: calls ZeroOWPlayerNaviPaletteIndex outside this file, which has no typed C interface yet */
static void call_owplayer_palette_idx(void) { GUEST_CALL(ZeroOWPlayerNaviPaletteIndex); }

/* register form: calls owPlayer_zeroS2000AA0Param0x4_809e312 outside this file, which has no typed C interface yet */
static void call_owplayer_zero4(void) { GUEST_CALL(owPlayer_zeroS2000AA0Param0x4_809e312); }

/* register form: calls sub_809E3AA outside this file, which has no typed C interface yet */
static void call_sub_809E3AA(void) { GUEST_CALL(sub_809E3AA); }

/* register form: calls resetStruct2000aa0State_809E3B2 outside this file, which has no typed C interface yet */
static void call_reset_2000aa0(void) { GUEST_CALL(resetStruct2000aa0State_809E3B2); }

/* register form: calls Clear_eStruct2000780 outside this file, which has no typed C interface yet */
static void call_clear_e2000780(void) { GUEST_CALL(Clear_eStruct2000780); }

/* register form: calls battle_ClearEStruct2001010_8003A58 outside this file, which has no typed C interface yet */
static void call_battle_clear_2001010(void) { GUEST_CALL(battle_ClearEStruct2001010_8003A58); }

/* register form: calls zeroFill_e2004348 outside this file, which has no typed C interface yet */
static void call_zero_fill_e2004348(void) { GUEST_CALL(zeroFill_e2004348); }

/* register form: calls mysteryData_InitMapGroupTables_809FAF4 outside this file, which has no typed C interface yet */
static void call_mystery_init(void) { GUEST_CALL(mysteryData_InitMapGroupTables_809FAF4); }

/* register form: calls mysteryData_processMapGroups_809F9DC outside this file, which has no typed C interface yet */
static void call_sub_809F9DC(void) { GUEST_CALL(mysteryData_processMapGroups_809F9DC); }

/* register form: calls chatbox_initTextLineBuffer_8049CF8 outside this file, which has no typed C interface yet */
static void call_sub_8049CF8(void) { GUEST_CALL(chatbox_initTextLineBuffer_8049CF8); }

/* register form: calls textScript_buildFolderNameBuffers_804A17A outside this file, which has no typed C interface yet */
static void call_sub_804A17A(void) { GUEST_CALL(textScript_buildFolderNameBuffers_804A17A); }

/* register form: calls clearS2001c04Byte6_811FB78 outside this file, which has no typed C interface yet */
static void call_clear_s2001c04_b6(void) { GUEST_CALL(clearS2001c04Byte6_811FB78); }

/* register form: calls shop_resetStockTable_8048C68 outside this file, which has no typed C interface yet */
static void call_shop_reset(void) { GUEST_CALL(shop_resetStockTable_8048C68); }

/* register form: calls clearSetFlags_80355a8 outside this file, which has no typed C interface yet */
static void call_clear_set_flags(void) { GUEST_CALL(clearSetFlags_80355a8); }

/* register form: calls memBlock_ClearWorkArea_802D638 outside this file, which has no typed C interface yet */
static void call_memblock_clear(void) { GUEST_CALL(memBlock_ClearWorkArea_802D638); }

/* register form: calls clearWorkBuffer_2000DC0_80356EC outside this file, which has no typed C interface yet */
static void call_clear_work_2000dc0(void) { GUEST_CALL(clearWorkBuffer_2000DC0_80356EC); }

/* register form: calls initGameProgressBuffer_803532c outside this file, which has no typed C interface yet */
static void call_init_progress_buf(void) { GUEST_CALL(initGameProgressBuffer_803532c); }

/* register form: calls sub_8021D36 outside this file, which has no typed C interface yet */
static void call_sub_8021D36(void) { GUEST_CALL(sub_8021D36); }

/* register form: calls zeroFill_e2002230 outside this file, which has no typed C interface yet */
static void call_zero_fill_e2002230(void) { GUEST_CALL(zeroFill_e2002230); }

/* register form: calls clearToolkitUnk2002230RecordFields_81376E8 outside this file, which has no typed C interface yet */
static void call_clear_record_fields(void) { GUEST_CALL(clearToolkitUnk2002230RecordFields_81376E8); }

/* register form: calls timer_setNoTimeout_8137700 outside this file, which has no typed C interface yet */
static void call_timer_no_timeout(void) { GUEST_CALL(timer_setNoTimeout_8137700); }

/* register form: calls clearChipGiftRecordAndTakeChipsSlot0_8137808 outside this file, which has no typed C interface yet */
static void call_clear_chip_gift(void) { GUEST_CALL(clearChipGiftRecordAndTakeChipsSlot0_8137808); }

/* register form: calls battle_initSlotsOrderPhaseB_80133EC outside this file, which has no typed C interface yet */
static void call_battle_init_slots(void) { GUEST_CALL(battle_initSlotsOrderPhaseB_80133EC); }

/* register form: calls toolkit_resetToolState_813B768 outside this file, which has no typed C interface yet */
static void call_reset_tool_state(void) { GUEST_CALL(toolkit_resetToolState_813B768); }

/* register form: calls zeroFill_813B934 outside this file, which has no typed C interface yet */
static void call_zero_fill_813B934(void) { GUEST_CALL(zeroFill_813B934); }

/* register form: calls eraseMem_ClearBuffer_813C324 outside this file, which has no typed C interface yet */
static void call_erase_clear(void) { GUEST_CALL(eraseMem_ClearBuffer_813C324); }

/* register form: calls clearWorkArea_200DCF0_8121144 outside this file, which has no typed C interface yet */
static void call_clear_work_200dcf0(void) { GUEST_CALL(clearWorkArea_200DCF0_8121144); }

/* register form: calls sub_800AAF2 outside this file, which has no typed C interface yet */
static void call_sub_800AAF2(void) { GUEST_CALL(sub_800AAF2); }

/* register form: calls clearWorkArea_2000FA0_800AB22 outside this file, which has no typed C interface yet */
static void call_clear_work_2000fa0(void) { GUEST_CALL(clearWorkArea_2000FA0_800AB22); }

/* register form: calls reqBBS_813E616 outside this file, which has no typed C interface yet */
static void call_reqbbs_1(void) { GUEST_CALL(reqBBS_813E616); }

/* register form: calls reqBBS_initMemory_813F9DA outside this file, which has no typed C interface yet */
static void call_reqbbs_init_mem(void) { GUEST_CALL(reqBBS_initMemory_813F9DA); }

/* register form: calls reqBBS_8140984 outside this file, which has no typed C interface yet */
static void call_reqbbs_2(void) { GUEST_CALL(reqBBS_8140984); }

/* ---------- parameterized helpers ---------- */

/* register form: calls copyOamEntry_80AA004 outside this file, which has no typed C interface yet */
static void call_sub_80aa004(u32 table_idx) {
    R0 = table_idx;
    GUEST_CALL(copyOamEntry_80AA004);
}

/* register form: calls palette_copyTableRow_80AA104 outside this file, which has no typed C interface yet */
static void call_palette_copy_row(u32 table_idx) {
    R0 = table_idx;
    GUEST_CALL(palette_copyTableRow_80AA104);
}

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void call_zero_fill_word(u32 mem, u32 num_bytes) {
    R0 = mem;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls sChip_loadRecordIntoSlot_8021AB4 outside this file, which has no typed C interface yet */
static void call_schip_load_record(u32 src, u32 slot) {
    R0 = src;
    R1 = slot;
    GUEST_CALL(sChip_loadRecordIntoSlot_8021AB4);
}

/* register form: calls loadIndexedPaletteToBuffer_802E240 outside this file, which has no typed C interface yet */
static void call_load_indexed_palette(u32 idx) {
    R0 = idx;
    GUEST_CALL(loadIndexedPaletteToBuffer_802E240);
}

/* ---------- main typed function ---------- */

static void init_new_game_data(u32 toolkit) {
    u32 gs = LD32(toolkit + oToolkit_GameStatePtr);

    /* Zero out / set initial GameState fields */
    ST32(gs + oGameState_Ptr_20, 0);
    ST16(gs + oGameState_LastMapGroup, 0xFFFF);
    ST32(gs + oGameState_ProtectedZenny, 0);
    ST32(gs + oGameState_ProtectedBugfrags, 0);
    ST32(gs + oGameState_Unk_74, 0);
    ST32(gs + oGameState_Unk_6c, 0xFFFFFFFF);
    ST32(gs + oGameState_Unk_70, 0xFFFFFFFF);
    ST32(gs + oGameState_CurBattleDataPtr, 0);
    ST8(gs + oGameState_Unk_02, 0);
    ST8(gs + oGameState_CurPETNavi, 0);
    ST8(gs + oGameState_Unk_12, 0xFF);

    /* Zero out / set initial S2001c04 (scenario-state) fields */
    u32 s2001c04 = LD32(toolkit + oToolkit_S2001c04_Ptr);
    ST32(s2001c04 + 0x18, 0);
    ST8(s2001c04 + 5, 1);
    ST16(s2001c04 + 0x12, 0);
    ST16(s2001c04 + 0x14, 0);
    ST32(s2001c04 + 0x1C, 0);
    ST16(s2001c04 + 0x8, 0);
    ST16(s2001c04 + 0xA, 0);
    ST16(s2001c04 + 0xC, 0);
    ST16(s2001c04 + 0x10, 0);
    ST32(s2001c04 + 0x0, 0);
    ST32(s2001c04 + 0x30, 0);
    ST32(s2001c04 + 0x24, 0);
    ST32(s2001c04 + 0x28, 0);
    ST8(s2001c04 + 4, 0x63);
    ST16(s2001c04 + 0x16, 0);

    /* Clear event flags, init encryption, reset key items */
    call_clear_all_event_flags();
    call_encryption_init();
    call_reset_key_items();

    ST8(gs + oGameState_Unk_13, 0);
    ST32(gs + 0x78, 0);

    /* Overworld player / struct reset */
    call_sub_802F2C8();
    call_sub_81284A4();
    call_owplayer_init();
    call_owplayer_shader();
    call_sub_809E304();
    call_owplayer_palette_idx();
    call_owplayer_zero4();
    call_sub_809E3AA();
    call_reset_2000aa0();
    call_clear_e2000780();
    call_battle_clear_2001010();
    call_zero_fill_e2004348();

    /* Mystery data, shop */
    call_mystery_init();
    call_sub_809F9DC();
    call_sub_8049CF8();
    call_sub_804A17A();
    call_clear_s2001c04_b6();
    call_shop_reset();

    /* Set scenario event flags */
    set_event_flag(0x17, 0x1D);          /* EVENT_PET_NAVI_ACTIVE */
    set_event_flag(4, 1);                /* EVENT_401 */
    set_event_flag_range(4, 0xE7, 2);    /* EVENT_4E7–4E8 */
    set_event_flag(4, 0x6D);             /* EVENT_46D */
    set_event_flag(4, 0x83);             /* EVENT_483 */
    set_event_flag_range(0, 1, 3);       /* EVENT_1–3 */
    set_event_flag(0, 4);                /* EVENT_4 */
    set_event_flag_range(0, 6, 8);       /* EVENT_6–D */
    set_event_flag_range(0, 0xF, 2);     /* EVENT_F–10 */
    set_event_flag_range(5, 0xEE, 3);    /* EVENT_5EE–5F0 */
    set_event_flag_range(0, 0x31, 2);    /* EVENT_31–32 */
    set_event_flag_range(0, 0x3B, 3);    /* EVENT_3B–3D */
    set_event_flag(0, 0x72);             /* EVENT_72 */
    set_event_flag_range(0, 0x73, 7);    /* EVENT_73–79 */
    set_event_flag_range(9, 0xF6, 9);    /* EVENT_9F6–9FE */
    set_event_flag(0, 0x87);             /* EVENT_87 */
    set_event_flag(0, 0x89);             /* EVENT_89 */
    set_event_flag(8, 0x7D);             /* EVENT_87D */
    set_event_flag(0xA, 0x99);           /* EVENT_A99 */
    set_event_flag_range(0xB, 0xFD, 3);  /* EVENT_BFD–BFF */
    set_event_flag(0, 0x70);             /* EVENT_70 */
    set_event_flag(0, 0x8A);             /* EVENT_8A */
    set_event_flag_range(0xC, 0xD7, 4);  /* EVENT_CD7–CD9 */
    set_event_flag(0, 0x8C);             /* EVENT_8C */
    set_event_flag(0xC, 0xE6);           /* EVENT_CE6 */
    set_event_flag_range(0xF, 0xF9, 7);  /* EVENT_FF9–FFF */
    set_event_flag(1, 0x36);             /* EVENT_136 */
    set_event_flag_range(1, 0x38, 2);    /* EVENT_138–139 */
    set_event_flag(1, 0x3A);             /* EVENT_13A */
    set_event_flag_range(1, 0x64, 0x19); /* EVENT_164–17C */
    set_event_flag(0, 0x7B);             /* EVENT_7B */
    set_event_flag(0, 0x7F);             /* EVENT_7F */
    set_event_flag(0, 0x81);             /* EVENT_81 */
    set_event_flag_range(1, 0x8E, 9);    /* EVENT_18E–196 */
    set_event_flag(0, 0x86);             /* EVENT_86 */
    set_event_flag_range(1, 0xCE, 6);    /* EVENT_1CE–1D3 */
    set_event_flag(0x10, 1);             /* EVENT_1001 */
    set_event_flag(6, 0x8E);             /* EVENT_68E */
    clear_event_flag(6, 0x8F);           /* EVENT_68F */
    set_event_flag_range(0, 0x82, 2);    /* EVENT_82–83 */
    set_event_flag(0, 0x85);             /* EVENT_85 */

    call_clear_set_flags();

    set_event_flag(0, 0x38);             /* EVENT_38 */
    set_event_flag(0, 0x39);             /* EVENT_39 */
    set_event_flag(0, 0x11);             /* EVENT_11 */

    /* Palette / memory work-area init */
    call_sub_80aa004(0);
    call_palette_copy_row(0);
    call_memblock_clear();
    call_clear_work_2000dc0();

    /* Set initial map / location / facing */
    ST16(gs + oGameState_MapGroup, INITIAL_MAP_GROUP);
    ST32(gs + oGameState_SavedRealWorldMapId, INITIAL_MAP_GROUP);
    ST32(gs + 0x58, INITIAL_MAP_GROUP);
    ST8(gs + oGameState_GameProgress, 0);
    ST8(gs + oGameState_Unk_07, 0);
    ST8(gs + oGameState_Unk_08, 0);
    ST32(gs + oGameState_PlayerX, 0);
    ST32(gs + oGameState_SavedRealWorldX, 0);
    ST32(gs + oGameState_SavedInternetX, 0);
    ST32(gs + oGameState_PlayerY, 0);
    ST32(gs + oGameState_SavedRealWorldY, 0);
    ST32(gs + 0x4C, 0);
    ST32(gs + oGameState_PlayerZ, 0);
    ST32(gs + oGameState_SavedRealWorldZ, 0);
    ST32(gs + 0x50, 0);
    ST32(gs + oGameState_FacingDirectionAfterWarp, DEFAULT_FACING);
    ST32(gs + oGameState_SavedRealWorldFacingDirection, DEFAULT_FACING);
    ST32(gs + 0x54, DEFAULT_FACING);

    /* Game progress buffer + chip/battle/tool init */
    call_init_progress_buf();
    call_sub_8021D36();

    /* Zero chip record table */
    u32 chip_mem = LD32(toolkit + oToolkit_S_Chip_2002178_Ptr);
    call_zero_fill_word(chip_mem, CHIP_RECORD_BYTES);

    call_zero_fill_e2002230();
    call_schip_load_record(A_byte_80213AC, 0);
    call_clear_record_fields();
    call_timer_no_timeout();
    call_clear_chip_gift();
    call_battle_init_slots();
    call_reset_tool_state();
    call_zero_fill_813B934();
    call_erase_clear();
    call_clear_work_200dcf0();
    call_sub_800AAF2();
    call_clear_work_2000fa0();

    /* Palette + reqBBS init */
    call_load_indexed_palette(1);
    call_reqbbs_1();
    call_reqbbs_init_mem();

    /* Clear late-game flags */
    clear_event_flag(0x17, 0x3A);         /* EVENT_173A */
    clear_event_flag(0x17, 0x3D);         /* EVENT_IN_SLIPRUN_STATE */
    clear_event_flag(0x17, 0x3E);         /* EVENT_173E */
    clear_event_flag(0x17, 0x3F);         /* EVENT_173F */

    call_reqbbs_2();
}

/* initNewGameData_8004DF0() -> (r2, r4-r12 preserved/callee-side-effects) */
Next initNewGameData_8004DF0(void) {
    init_new_game_data(R10);
    RETURN_TO_CALLER();
}

/* ── cbGameState_80050EC ── */
/*
 * cbGameState_80050EC — one tick of the top-level game state machine.
 *
 * Reads the current GameState from the toolkit, runs the subsystem handler
 * selected by its SubsystemIndex through GameStateJumptable, then ticks both
 * hardware RNGs. All registers the original leaves behind (r0-r3 and the
 * flags) come straight out of the last callee; r4-r7 are restored to the
 * caller's values, exactly as the original's push/pop does.
 */

/* register form: calls GetRNG outside this file, which has no typed C interface yet.
 * The result is discarded by the caller, so R0 is left as GetRNG leaves it. */
static void call_GetRNG(void) {
    GUEST_CALL(GetRNG);
}

/* register form: calls GetRNGSecondary outside this file, which has no typed C
 * interface yet. R0/R1 and the flags are left exactly as the callee leaves them,
 * since they are this function's observable outputs. */
static void call_GetRNGSecondary(void) {
    GUEST_CALL(GetRNGSecondary);
}

/* register form: the subsystem handler is selected at run time from
 * GameStateJumptable, so the call has no single typed C interface to wrap.
 * Call state matches the original `mov lr, pc / bx r0`: R0 holds the handler's
 * own address (left over from the table load), R1 the raw subsystem index byte,
 * and R5 the GameState pointer the handlers use as their self. */
static void call_subsystem_handler(u32 index, u32 game_state) {
    R0 = LD32(A_GameStateJumptable + index);
    R1 = index;
    R5 = game_state;
    switch (index) {
    case 0x00: GUEST_CALL(EnterMap); break;
    case 0x04: GUEST_CALL(gamestate_OnMapUpdate_8005268); break;
    case 0x08: GUEST_CALL(battle_init_80052D8); break;
    case 0x0C: GUEST_CALL(HandlesBattleMainUntilEndOfBattleThenTriggersEnterMap); break;
    case 0x10: GUEST_CALL(sub_800536E); break;
    case 0x14: GUEST_CALL(scene_nextOrUpdate_80053E4); break;
    case 0x18: GUEST_CALL(pet_menuUpdateAndHandleExit_8005462); break;
    case 0x1C: GUEST_CALL(sub_800555A); break;
    case 0x20: GUEST_CALL(scenario_finalizeFrameOrRestart_8005642); break;
    case 0x24: GUEST_CALL(sub_80056B8); break;
    case 0x28: GUEST_CALL(sub_800572C); break;
    case 0x2C: GUEST_CALL(sub_80057A0); break;
    case 0x30: GUEST_CALL(sub_80055CE); break;
    case 0x34: GUEST_CALL(mainState_runFrame_8005814); break;
    case 0x38: GUEST_CALL(sub_800585A); break;
    }
}

/* Run the active game state subsystem, then advance both RNG counters. */
static void game_state_update(u32 toolkit) {
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 subsystem  = LD8(game_state + oGameState_SubsystemIndex);

    call_subsystem_handler(subsystem, game_state);
    call_GetRNG();
    call_GetRNGSecondary();
}

/* cbGameState_80050EC (r10 = toolkit) */
Next cbGameState_80050EC(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    game_state_update(R10);
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── EnterMap ── */
/* EnterMap — map-entry gamestate routine (0x08005148).
 *
 * Prepares everything needed to walk around on the overworld: resets graphics
 * state, rebuilds the map object structs, clears scenario/event/cutscene state,
 * loads the destination map's graphics, then hands control to the on-map update
 * subsystem. While a screen fade is still running it does nothing and returns
 * to its caller immediately.
 */

/* IsScreenFadeActive answers in Z (set = a fade is running). When it does,
 * EnterMap returns straight to its caller, so the callee's leftover r0/r1 are
 * part of EnterMap's observable result and must be handed back untouched. */
struct FadeCheck_EnterMap {
    bool fade_active;
    u32 leftover_r0;
    u32 leftover_r1;
};

/* What the adapter has to write back. nz_value is the value whose N/Z the
 * adapter reproduces: on the normal path the original's last flag write is
 * `movs r0, #4` (N clear, Z clear); on the fade-active early return the
 * callee's boolean answer (Z set, N clear) is reproduced by 0. */
struct EnterMapExit {
    u32 r0;
    u32 r1;
    u32 nz_value;
};

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static struct FadeCheck_EnterMap check_screen_fade_EnterMap(void) {
    struct FadeCheck_EnterMap fade;
    GUEST_CALL(IsScreenFadeActive);
    fade.fade_active = ZERO_FLAG();   /* Z set: a fade is still running */
    fade.leftover_r0 = R0;
    fade.leftover_r1 = R1;
    return fade;
}

/* register form: calls resetAllGraphicsState_8005F40 outside this file, which has no typed C interface yet */
static void reset_all_graphics_state(void) {
    GUEST_CALL(resetAllGraphicsState_8005F40);
}

/* register form: calls runSetupStepAndClearWorkArea_8005F6C outside this file, which has no typed C interface yet */
static void run_setup_step_and_clear_work_area(void) {
    GUEST_CALL(runSetupStepAndClearWorkArea_8005F6C);
}

/* register form: calls sub_80027C4 outside this file, which has no typed C interface yet */
static void call_sub_80027C4_EnterMap(void) {
    GUEST_CALL(sub_80027C4);
}

/* register form: calls InitializeOverworldMapObjectStructs outside this file, which has no typed C interface yet */
static void initialize_overworld_map_object_structs(void) {
    GUEST_CALL(InitializeOverworldMapObjectStructs);
}

/* register form: calls copy_8002668 outside this file, which has no typed C interface yet */
static void call_copy_8002668(void) {
    GUEST_CALL(copy_8002668);
}

/* register form: calls zeroFill_80024A2 outside this file, which has no typed C interface yet */
static void zero_fill_80024A2(void) {
    GUEST_CALL(zeroFill_80024A2);
}

/* register form: calls scenario_initEffectState_8003962 outside this file, which has no typed C interface yet */
static void scenario_init_effect_state(void) {
    GUEST_CALL(scenario_initEffectState_8003962);
}

/* register form: calls zeroFill_8003AB2 outside this file, which has no typed C interface yet */
static void zero_fill_8003AB2(void) {
    GUEST_CALL(zeroFill_8003AB2);
}

/* register form: calls eventflags_ClearTwoHalfwordsAt2DE_802F0D8 outside this file, which has no typed C interface yet */
static void eventflags_clear_two_halfwords_at_2DE(void) {
    GUEST_CALL(eventflags_ClearTwoHalfwordsAt2DE_802F0D8);
}

/* register form: calls eventFlags_ClearRangeAt2C8_802F0F4 outside this file, which has no typed C interface yet */
static void eventflags_clear_range_at_2C8(void) {
    GUEST_CALL(eventFlags_ClearRangeAt2C8_802F0F4);
}

/* register form: calls cutscene_forceEndCurrent_8036EFE outside this file, which has no typed C interface yet */
static void cutscene_force_end_current(void) {
    GUEST_CALL(cutscene_forceEndCurrent_8036EFE);
}

/* register form: calls clearCutsceneScriptPosIfMagicValue0x1_8036F24 outside this file, which has no typed C interface yet */
static void clear_cutscene_script_pos_if_magic_value_1(void) {
    GUEST_CALL(clearCutsceneScriptPosIfMagicValue0x1_8036F24);
}

/* register form: calls clearBattleStateIfEvent1708Pending_809F90C outside this file, which has no typed C interface yet */
static void call_sub_809F90C(void) {
    GUEST_CALL(clearBattleStateIfEvent1708Pending_809F90C);
}

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void clear_event_flag_EnterMap(u16 flag) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_test(u16 flag) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();          /* Z clear: the flag is set */
}

/* register form: calls sub_803F6B0 outside this file, which has no typed C interface yet */
static void call_sub_803F6B0(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_803F6B0);
}

/* register form: calls sub_803F500 outside this file, which has no typed C interface yet */
static void call_sub_803F500(void) {
    GUEST_CALL(sub_803F500);
}

/* register form: calls RandomizeExtraToolkitPointers outside this file, which has no typed C interface yet */
static void randomize_extra_toolkit_pointers(void) {
    GUEST_CALL(RandomizeExtraToolkitPointers);
}

/* register form: calls reloadCurNaviStatBoosts_813c3ac outside this file, which has no typed C interface yet */
static void reload_cur_navi_stat_boosts_EnterMap(void) {
    GUEST_CALL(reloadCurNaviStatBoosts_813c3ac);
}

/* register form: calls sub_8000FAC outside this file, which has no typed C interface yet;
 * it takes the game state in r5 */
static void init_map_state_8000FAC(u32 game_state) {
    R5 = game_state;
    GUEST_CALL(sub_8000FAC);
}

/* register form: calls sub_80355EC outside this file, which has no typed C interface yet */
static void call_sub_80355EC(void) {
    GUEST_CALL(sub_80355EC);
}

/* register form: calls sub_8001172 outside this file, which has no typed C interface yet */
static void call_sub_8001172(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_8001172);
}

/* register form: calls triggerSomeUpdateForInternetOrRealWorld_8033948 outside this file, which has no typed C interface yet */
static void trigger_update_for_internet_or_real_world(void) {
    GUEST_CALL(triggerSomeUpdateForInternetOrRealWorld_8033948);
}

/* register form: calls SetRenderInfoLCDControl outside this file, which has no typed C interface yet */
static void set_render_info_lcd_control(u16 value) {
    R0 = value;
    GUEST_CALL(SetRenderInfoLCDControl);
}

/* register form: calls LoadGFXAnimsForMapGroup outside this file, which has no typed C interface yet */
static void load_gfx_anims_for_map_group(u8 map_group, u8 map_number) {
    R0 = map_group;
    R1 = map_number;
    GUEST_CALL(LoadGFXAnimsForMapGroup);
}

/* register form: calls map_lookupMapIdByGroupAndNumber_8001708 outside this file, which has no typed C interface yet */
static void map_lookup_map_id_by_group_and_number(u8 map_group, u8 map_number) {
    R0 = map_group;
    R1 = map_number;
    GUEST_CALL(map_lookupMapIdByGroupAndNumber_8001708);
}

/* register form: calls map_8034B4C outside this file, which has no typed C interface yet */
static void call_map_8034B4C(u8 map_group, u8 map_number) {
    R0 = map_group;
    R1 = map_number;
    GUEST_CALL(map_8034B4C);
}

/* register form: calls EnterMap_RunMapGroupAsmFunction_8030A00 outside this file, which has no typed C interface yet */
static void enter_map_run_map_group(u8 map_group) {
    R0 = map_group;
    GUEST_CALL(EnterMap_RunMapGroupAsmFunction_8030A00);
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void set_screen_fade_EnterMap(u32 param1, u32 param2) {
    R0 = param1;
    R1 = param2;
    GUEST_CALL(SetScreenFade);
}

/* Body of EnterMap. `toolkit` is the r10 toolkit base the caller provides;
 * the game state pointer is read from it. */
static struct EnterMapExit enter_map(u32 toolkit) {
    struct EnterMapExit out;

    /* While a screen fade is running, leave everything alone: the caller sees
     * the callee's registers and flags unchanged. */
    struct FadeCheck_EnterMap fade = check_screen_fade_EnterMap();
    if (fade.fade_active) {
        out.r0 = fade.leftover_r0;
        out.r1 = fade.leftover_r1;
        out.nz_value = 0;         /* boolean answer: N clear, Z set */
        return out;
    }

    reset_all_graphics_state();
    run_setup_step_and_clear_work_area();
    call_sub_80027C4_EnterMap();
    initialize_overworld_map_object_structs();
    call_copy_8002668();
    zero_fill_80024A2();
    scenario_init_effect_state();
    zero_fill_8003AB2();
    eventflags_clear_two_halfwords_at_2DE();
    eventflags_clear_range_at_2C8();
    cutscene_force_end_current();
    clear_cutscene_script_pos_if_magic_value_1();
    call_sub_809F90C();

    clear_event_flag_EnterMap(0x1731);     /* EVENT_1731 */

    call_sub_803F6B0(0);
    call_sub_803F500();
    randomize_extra_toolkit_pointers();

    /* Re-read the current navi's stat boosts unless EVENT_1741 suppresses it. */
    if (!event_flag_test(0x1741))
        reload_cur_navi_stat_boosts_EnterMap();

    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);

    init_map_state_8000FAC(game_state);
    call_sub_80355EC();
    call_sub_8001172(0);

    /* Clear leftover interaction/coordination state from the previous map. */
    ST8(game_state + oGameState_Unk_15, 0);
    ST8(game_state + oGameState_CoordInteractionValue, 0);
    ST8(game_state + 3 /* oGameState_Unk_03 */, 0);
    ST32(game_state + oGameState_Unk_68, 0);

    /* Drop all the temporary mode flags a cutscene/battle may have left set. */
    clear_event_flag_EnterMap(0x1719);     /* EVENT_1719 */
    clear_event_flag_EnterMap(0x1701);     /* EVENT_NPC_OBJECTS_DISABLED */
    clear_event_flag_EnterMap(0x1702);     /* EVENT_NON_NPC_ANIMATION_LOCKED */
    clear_event_flag_EnterMap(0x1700);     /* EVENT_1700 */
    clear_event_flag_EnterMap(0x1707);     /* EVENT_PET_DISABLED */
    clear_event_flag_EnterMap(0x1739);     /* EVENT_1739 */
    clear_event_flag_EnterMap(0x1732);     /* EVENT_1732 */
    clear_event_flag_EnterMap(0x01BB);     /* EVENT_1BB */

    trigger_update_for_internet_or_real_world();
    set_render_info_lcd_control(0x1740);

    /* Load the destination map's graphics and per-map tables. The group/number
     * bytes are re-read before each call, as the original does. */
    u8 map_group  = LD8(game_state + 4 /* oGameState_MapGroup */);
    u8 map_number = LD8(game_state + 5 /* oGameState_MapNumber */);
    load_gfx_anims_for_map_group(map_group, map_number);

    map_group  = LD8(game_state + 4 /* oGameState_MapGroup */);
    map_number = LD8(game_state + 5 /* oGameState_MapNumber */);
    map_lookup_map_id_by_group_and_number(map_group, map_number);

    map_group  = LD8(game_state + 4 /* oGameState_MapGroup */);
    map_number = LD8(game_state + 5 /* oGameState_MapNumber */);
    call_map_8034B4C(map_group, map_number);

    map_group = LD8(game_state + 4 /* oGameState_MapGroup */);
    enter_map_run_map_group(map_group);

    /* Leaving a map ends SlipRun mode and its follow-up flags. */
    clear_event_flag_EnterMap(0x173D);     /* EVENT_IN_SLIPRUN_STATE */
    clear_event_flag_EnterMap(0x173E);     /* EVENT_173E */
    clear_event_flag_EnterMap(0x173F);     /* EVENT_173F */

    /* Start the fade-in from the parameters the caller stored, then reset them
     * for the next transition. */
    set_screen_fade_EnterMap(LD8(game_state + oGameState_EnterMapFadeParam1),
                    LD8(game_state + oGameState_EnterMapFadeParam2));

    ST8(game_state + oGameState_EnterMapFadeParam1, 8);
    ST8(game_state + oGameState_EnterMapFadeParam2, 0x10);

    /* Clear the warp-state byte. */
    u32 warp = LD32(toolkit + oToolkit_Warp2011bb0_Ptr);
    ST8(warp + oWarp2011bb0_Unk_10, 0);

    /* Hand control to the on-map update subsystem (gamestate_OnMapUpdate). */
    ST8(game_state + 0 /* oGameState_SubsystemIndex */, 4);

    out.r0 = 4;
    out.r1 = 0;
    out.nz_value = 4;
    return out;
}

/* EnterMap(r10 = toolkit) -> r0 = 4, r1 = 0, r5 = game state, flags from `movs r0, #4`.
 * r2/r3 and r4–r12 are whatever the last callee left behind, which the guest
 * calls already placed in the registers, so the adapter leaves them alone. */
Next EnterMap(void) {
    struct EnterMapExit out = enter_map(R10);
    R0 = out.r0;
    R1 = out.r1;
    SET_FLAGS_NZ(out.nz_value);
    RETURN_TO_CALLER();
}

/* ── gamestate_OnMapUpdate_8005268 ── */
/* register form: calls clearCutsceneScriptPosIfMagicValue0x1_8036F24 outside this file, which has no typed C interface yet */
static void call_clearCutsceneScriptPos(void) {
    GUEST_CALL(clearCutsceneScriptPosIfMagicValue0x1_8036F24);
}

/* register form: calls cutscene_8034BB8 outside this file, which has no typed C interface yet */
static void call_cutsceneUpdate(void) {
    GUEST_CALL(cutscene_8034BB8);
}

/* register form: calls ZeroFillObjectInteractionAreas_800378C outside this file, which has no typed C interface yet */
static void call_zeroFillInteractionAreas(void) {
    GUEST_CALL(ZeroFillObjectInteractionAreas_800378C);
}

/* register form: calls TryUpdateEachOWPlayerObject_8003BA2 outside this file, which has no typed C interface yet */
static void call_updatePlayerObjects(void) {
    GUEST_CALL(TryUpdateEachOWPlayerObject_8003BA2);
}

/* register form: calls TryUpdateEachOverworldNPCObject_800461E outside this file, which has no typed C interface yet */
static void call_updateNpcObjects(void) {
    GUEST_CALL(TryUpdateEachOverworldNPCObject_800461E);
}

/* register form: calls TryUpdateEachOverworldMapObject_80048D2 outside this file, which has no typed C interface yet */
static void call_updateMapObjects(void) {
    GUEST_CALL(TryUpdateEachOverworldMapObject_80048D2);
}

/* register form: calls sub_809F942 outside this file, which has no typed C interface yet */
static void call_sub_809F942(void) {
    GUEST_CALL(sub_809F942);
}

/* register form: calls dispatch_80339CC outside this file, which has no typed C interface yet */
static void call_eventDispatch(void) {
    GUEST_CALL(dispatch_80339CC);
}

/* register form: calls sub_80039AA outside this file, which has no typed C interface yet */
static void call_sub_80039AA_gamestate_OnMapUpdate_8005268(void) {
    GUEST_CALL(sub_80039AA);
}

/* register form: calls sub_8003AFA outside this file, which has no typed C interface yet */
static void call_sub_8003AFA_gamestate_OnMapUpdate_8005268(void) {
    GUEST_CALL(sub_8003AFA);
}

/* register form: calls checkOWObjectInteractions_80037f4 outside this file, which has no typed C interface yet */
static void call_checkObjectInteractions(void) {
    GUEST_CALL(checkOWObjectInteractions_80037f4);
}

/* register form: calls camera_802FFF4 outside this file, which has no typed C interface yet */
static void call_updateCamera(void) {
    GUEST_CALL(camera_802FFF4);
}

/* register form: calls scrollBGTileMapToCamera_8030580 outside this file, which has no typed C interface yet */
static void call_scrollBGTileMap(void) {
    GUEST_CALL(scrollBGTileMapToCamera_8030580);
}

/* register form: calls sub_80027B4 outside this file, which has no typed C interface yet */
static void call_sub_80027B4_gamestate_OnMapUpdate_8005268(void) {
    GUEST_CALL(sub_80027B4);
}

/* register form: calls sub_800286C outside this file, which has no typed C interface yet */
static void call_sub_800286C_gamestate_OnMapUpdate_8005268(void) {
    GUEST_CALL(sub_800286C);
}

/* register form: calls sub_8003BF4 outside this file, which has no typed C interface yet */
static void call_sub_8003BF4_gamestate_OnMapUpdate_8005268(void) {
    GUEST_CALL(sub_8003BF4);
}

/* register form: calls sub_8003E98 outside this file, which has no typed C interface yet */
static void call_sub_8003E98_gamestate_OnMapUpdate_8005268(void) {
    GUEST_CALL(sub_8003E98);
}

/* register form: calls npc_drawSprites_800467C outside this file, which has no typed C interface yet */
static void call_npcDrawSprites(void) {
    GUEST_CALL(npc_drawSprites_800467C);
}

/* register form: calls sub_8004298 outside this file, which has no typed C interface yet */
static void call_sub_8004298_gamestate_OnMapUpdate_8005268(void) {
    GUEST_CALL(sub_8004298);
}

/* register form: calls sub_8004590 outside this file, which has no typed C interface yet */
static void call_sub_8004590_gamestate_OnMapUpdate_8005268(void) {
    GUEST_CALL(sub_8004590);
}

/* register form: calls sub_8004934 outside this file, which has no typed C interface yet */
static void call_sub_8004934_gamestate_OnMapUpdate_8005268(void) {
    GUEST_CALL(sub_8004934);
}

/* register form: calls event_runArmedCallback_80024AE outside this file, which has no typed C interface yet */
static void call_runArmedCallback(void) {
    GUEST_CALL(event_runArmedCallback_80024AE);
}

/* register form: calls sub_80058D0 outside this file, which has no typed C interface yet */
static void call_sub_80058D0(void) {
    GUEST_CALL(sub_80058D0);
}

/* register form: calls checkThenStartBattle_8005A8C outside this file, which has no typed C interface yet */
static void call_checkThenStartBattle(void) {
    GUEST_CALL(checkThenStartBattle_8005A8C);
}

/* register form: calls nullsub_8005B6A outside this file, which has no typed C interface yet */
static void call_nullsub_8005B6A(void) {
    GUEST_CALL(nullsub_8005B6A);
}

/* register form: calls gamestate_openPetMenuOnStartPress_8005AF4 outside this file, which has no typed C interface yet */
static void call_sub_8005AF4(void) {
    GUEST_CALL(gamestate_openPetMenuOnStartPress_8005AF4);
}

/* Per-frame overworld map update: cutscene, object movement, interactions,
 * camera, tile scroll, sprite drawing, events, and battle check. */
static void map_update_body(u32 game_state) {
    /* Clear any pending cutscene scripting position marker */
    call_clearCutsceneScriptPos();

    /* Advance active cutscene state machine */
    call_cutsceneUpdate();

    /* Reset coordinate-interaction flag for this frame */
    ST8(game_state + oGameState_CoordInteractionValue, 0);

    /* Wipe interaction areas (collision zones around objects) */
    call_zeroFillInteractionAreas();

    /* Update player, NPC, and map object positions/animation */
    call_updatePlayerObjects();
    call_updateNpcObjects();
    call_updateMapObjects();

    /* Miscellaneous state poll */
    call_sub_809F942();

    /* Process dispatched events */
    call_eventDispatch();

    /* Per-object update passes */
    call_sub_80039AA_gamestate_OnMapUpdate_8005268();
    call_sub_8003AFA_gamestate_OnMapUpdate_8005268();

    /* Check whether the player touched an interactive object */
    call_checkObjectInteractions();

    /* Update camera position toward the player */
    call_updateCamera();

    /* Scroll the BG tile map to match the camera */
    call_scrollBGTileMap();

    /* Additional overworld subsystems */
    call_sub_80027B4_gamestate_OnMapUpdate_8005268();
    call_sub_800286C_gamestate_OnMapUpdate_8005268();
    call_sub_8003BF4_gamestate_OnMapUpdate_8005268();
    call_sub_8003E98_gamestate_OnMapUpdate_8005268();

    /* Render NPC sprites into OAM */
    call_npcDrawSprites();

    /* Further sprite/object drawing passes */
    call_sub_8004298_gamestate_OnMapUpdate_8005268();
    call_sub_8004590_gamestate_OnMapUpdate_8005268();
    call_sub_8004934_gamestate_OnMapUpdate_8005268();

    /* Fire any armed event callback for this frame */
    call_runArmedCallback();

    /* Miscellaneous map-state update */
    call_sub_80058D0();

    /* Random encounter check → possibly start a battle */
    call_checkThenStartBattle();

    /* Stub (placeholder for a future feature) */
    call_nullsub_8005B6A();

    /* Final per-frame housekeeping */
    call_sub_8005AF4();
}

/* gamestate_OnMapUpdate_8005268(r5 = GameState*) */
Next gamestate_OnMapUpdate_8005268(void) {
    map_update_body(R5);
    RETURN_TO_CALLER();
}

/* ── battle_init_80052D8 ── */
/* register form: calls dispatch_80339CC outside this file, which has no typed C interface yet */
static void call_dispatch_80339CC(void) {
    GUEST_CALL(dispatch_80339CC);
}

/* register form: calls sub_80039AA outside this file, which has no typed C interface yet */
static void call_sub_80039AA(void) {
    GUEST_CALL(sub_80039AA);
}

/* register form: calls sub_8003AFA outside this file, which has no typed C interface yet */
static void call_sub_8003AFA(void) {
    GUEST_CALL(sub_8003AFA);
}

/* register form: calls sub_80027B4 outside this file, which has no typed C interface yet */
static void call_sub_80027B4(void) {
    GUEST_CALL(sub_80027B4);
}

/* register form: calls sub_800286C outside this file, which has no typed C interface yet */
static void call_sub_800286C(void) {
    GUEST_CALL(sub_800286C);
}

/* register form: calls sub_8003BF4 outside this file, which has no typed C interface yet */
static void call_sub_8003BF4(void) {
    GUEST_CALL(sub_8003BF4);
}

/* register form: calls sub_8003E98 outside this file, which has no typed C interface yet */
static void call_sub_8003E98(void) {
    GUEST_CALL(sub_8003E98);
}

/* register form: calls npc_drawSprites_800467C outside this file, which has no typed C interface yet */
static void call_npc_drawSprites_800467C(void) {
    GUEST_CALL(npc_drawSprites_800467C);
}

/* register form: calls sub_8004298 outside this file, which has no typed C interface yet */
static void call_sub_8004298(void) {
    GUEST_CALL(sub_8004298);
}

/* register form: calls sub_8004590 outside this file, which has no typed C interface yet */
static void call_sub_8004590(void) {
    GUEST_CALL(sub_8004590);
}

/* register form: calls sub_8004934 outside this file, which has no typed C interface yet */
static void call_sub_8004934(void) {
    GUEST_CALL(sub_8004934);
}

/* register form: calls event_runArmedCallback_80024AE outside this file, which has no typed C interface yet */
static void call_event_runArmedCallback_80024AE(void) {
    GUEST_CALL(event_runArmedCallback_80024AE);
}

/* register form: calls tickCountdownGate_803F530 outside this file, which has no typed C interface yet.
 * Returns true when the countdown has finished (Z set). */
static bool tick_countdown_gate_done(void) {
    GUEST_CALL(tickCountdownGate_803F530);
    return ZERO_FLAG();
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet.
 * Returns true while a screen fade is in progress (Z clear). */
static bool is_screen_fade_active_battle_init_80052D8(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();
}

/* register form: calls endBattleAndEnterMap_800531C outside this file, which has no typed C interface yet */
static void call_sub_800531C(void) {
    GUEST_CALL(endBattleAndEnterMap_800531C);
}

/* Battle scene init: runs the per-frame dispatch and the various sub-systems, then, once the
 * start countdown has finished and the screen fade is still in progress, hands control to the
 * battle main loop (endBattleAndEnterMap_800531C). */
static void do_battle_init(void) {
    call_dispatch_80339CC();
    call_sub_80039AA();
    call_sub_8003AFA();
    call_sub_80027B4();
    call_sub_800286C();
    call_sub_8003BF4();
    call_sub_8003E98();
    call_npc_drawSprites_800467C();
    call_sub_8004298();
    call_sub_8004590();
    call_sub_8004934();
    call_event_runArmedCallback_80024AE();

    if (tick_countdown_gate_done() && is_screen_fade_active_battle_init_80052D8()) {
        call_sub_800531C();
    }
}

Next battle_init_80052D8(void) {
    do_battle_init();
    RETURN_TO_CALLER();
}

/* ── endBattleAndEnterMap_800531C ── */
/* Register-form helpers: every callee in this sequence is a game function that
 * has no typed C interface yet, so each is invoked through the guest registers. */

/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void call_chatbox_8040818_sub_800531C(void) {
    GUEST_CALL(chatbox_8040818);
}

/* register form: calls FreeAllObjectsOfSpecifiedTypes outside this file, which has no typed C interface yet */
static void free_objects_of_type(u32 typeBits) {
    R0 = typeBits;
    GUEST_CALL(FreeAllObjectsOfSpecifiedTypes);
}

/* register form: calls npc_clearOverworldNpcStateWord_8004702 outside this file, which has no typed C interface yet */
static void call_sub_8004702_sub_800531C(void) {
    GUEST_CALL(npc_clearOverworldNpcStateWord_8004702);
}

/* register form: calls zeroFill_80024A2 outside this file, which has no typed C interface yet */
static void call_zero_fill_80024A2_sub_800531C(void) {
    GUEST_CALL(zeroFill_80024A2);
}

/* register form: calls scenario_initEffectState_8003962 outside this file, which has no typed C interface yet */
static void call_scenario_initEffectState(void) {
    GUEST_CALL(scenario_initEffectState_8003962);
}

/* register form: calls zeroFill_8003AB2 outside this file, which has no typed C interface yet */
static void call_zero_fill_8003AB2_sub_800531C(void) {
    GUEST_CALL(zeroFill_8003AB2);
}

/* register form: calls RandomizeExtraToolkitPointers outside this file, which has no typed C interface yet */
static void call_randomize_extra_toolkit_pointers(void) {
    GUEST_CALL(RandomizeExtraToolkitPointers);
}

/* register form: calls initBattleStructsAndVram_80071D4 outside this file, which has no typed C interface yet */
static void init_battle_structs_and_vram(u32 battleData, u32 gameState) {
    R0 = battleData;
    R5 = gameState;
    GUEST_CALL(initBattleStructsAndVram_80071D4);
}

/* register form: calls WordFill outside this file, which has no typed C interface yet */
static void word_fill(u32 dst, u32 word, u32 count) {
    R0 = dst;
    R1 = word;
    R2 = count;
    GUEST_CALL(WordFill);
}

/* End-of-battle teardown: free the chatbox and all battle objects, reset the
 * misc state blocks, then switch the game state to subsystem 0xC (the handler
 * cbGameState_80050EC waits for battle main to end and then triggers the map
 * entry), rebuild the battle structures/VRAM one last time, and fill the logo
 * screen state buffer with 0x2180. Leaves r0-r3 and the flags exactly as the
 * final WordFill call leaves them, matching the original. */
static void finish_battle_and_enter_map(u32 toolkit) {
    call_chatbox_8040818_sub_800531C();
    free_objects_of_type(0x21);
    call_sub_8004702_sub_800531C();
    call_zero_fill_80024A2_sub_800531C();
    call_scenario_initEffectState();
    call_zero_fill_8003AB2_sub_800531C();
    call_randomize_extra_toolkit_pointers();

    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);

    /* queue the enter-map transition (subsystem index 0xC) */
    ST8(gameState + oGameState_SubsystemIndex, 0xC);

    init_battle_structs_and_vram(LD32(gameState + oGameState_CurBattleDataPtr), gameState);

    word_fill(A_eLogoScreenState, 0x2180, ~0u);
}

Next endBattleAndEnterMap_800531C(void) {
    finish_battle_and_enter_map(R10);
    RETURN_TO_CALLER();
}

/* ── sub_800536E ── */
/* Sound ids for the early warp cue, selected by the GameState byte at +5
 * (ROM table hword_80053E0: 0xE7, 0xEC). */
static const u16 warp_entry_cues[2] = { 0xE7, 0xEC };

/* register form: calls the overworld per-frame update/draw routines
 * (dispatch_80339CC, sub_80039AA, sub_8003AFA, sub_80027B4, sub_800286C,
 * sub_8003BF4, sub_8003E98, npc_drawSprites_800467C, sub_8004298,
 * sub_8004590, sub_8004934, event_runArmedCallback_80024AE) outside this
 * file, which have no typed C interface yet */
static void run_overworld_updates(void) {
    GUEST_CALL(dispatch_80339CC);
    GUEST_CALL(sub_80039AA);
    GUEST_CALL(sub_8003AFA);
    GUEST_CALL(sub_80027B4);
    GUEST_CALL(sub_800286C);
    GUEST_CALL(sub_8003BF4);
    GUEST_CALL(sub_8003E98);
    GUEST_CALL(npc_drawSprites_800467C);
    GUEST_CALL(sub_8004298);
    GUEST_CALL(sub_8004590);
    GUEST_CALL(sub_8004934);
    GUEST_CALL(event_runArmedCallback_80024AE);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG(); /* answers in Z: set when no fade is running */
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_effect_sub_800536E(u32 sound_id) {
    R0 = sound_id;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void run_chatbox_8040818(void) {
    GUEST_CALL(chatbox_8040818);
}

/* register form: calls map_triggerEnterMapOnWarp_8005C04 outside this file, which has no typed C interface yet */
static void map_trigger_enter_on_warp(void) {
    GUEST_CALL(map_triggerEnterMapOnWarp_8005C04);
}

/* GameState::Tick (self in r5): run every overworld subsystem's frame update,
 * then, while a screen fade is running, step the warp countdown in
 * oGameState_Unk_68 — play cue sounds at fixed steps and, once it expires,
 * finish the warp via the chatbox and the map's on-warp-enter event. */
static void game_state_tick(u32 self) {
    run_overworld_updates();

    if (!screen_fade_active())
        return;

    s32 countdown = (s32)LD32(self + oGameState_Unk_68) - 1;
    if (countdown < 0) {
        /* Timer ran out (field is still 0 on disk): complete the map warp. */
        run_chatbox_8040818();
        map_trigger_enter_on_warp();
        return;
    }
    ST32(self + oGameState_Unk_68, countdown);

    if (countdown == 0x29) {
        play_sound_effect_sub_800536E(0x171); /* late warp cue */
    } else if (countdown == 0x9E) {
        u32 variant = LD8(self + 5); /* GameState byte at +5 picks the cue */
        if (variant <= 1)
            play_sound_effect_sub_800536E(warp_entry_cues[variant]);
    }
}

/* sub_800536E(r5 = self) -> r8..r12 (left by the callees) */
Next sub_800536E(void) {
    game_state_tick(R5);
    RETURN_TO_CALLER();
}

/* ── scene_nextOrUpdate_80053E4 ── */
/* register form: calls dispatch_80339CC outside this file, which has no typed C interface yet */
static void call_dispatch(void) { GUEST_CALL(dispatch_80339CC); }

/* register form: calls sub_80039AA outside this file, which has no typed C interface yet */
static void call_sub_80039AA_sub_80053E4(void) { GUEST_CALL(sub_80039AA); }

/* register form: calls sub_8003AFA outside this file, which has no typed C interface yet */
static void call_sub_8003AFA_sub_80053E4(void) { GUEST_CALL(sub_8003AFA); }

/* register form: calls sub_80027B4 outside this file, which has no typed C interface yet */
static void call_sub_80027B4_sub_80053E4(void) { GUEST_CALL(sub_80027B4); }

/* register form: calls sub_800286C outside this file, which has no typed C interface yet */
static void call_sub_800286C_sub_80053E4(void) { GUEST_CALL(sub_800286C); }

/* register form: calls sub_8003BF4 outside this file, which has no typed C interface yet */
static void call_sub_8003BF4_sub_80053E4(void) { GUEST_CALL(sub_8003BF4); }

/* register form: calls sub_8003E98 outside this file, which has no typed C interface yet */
static void call_sub_8003E98_sub_80053E4(void) { GUEST_CALL(sub_8003E98); }

/* register form: calls npc_drawSprites_800467C outside this file, which has no typed C interface yet */
static void call_npc_draw_sprites(void) { GUEST_CALL(npc_drawSprites_800467C); }

/* register form: calls sub_8004298 outside this file, which has no typed C interface yet */
static void call_sub_8004298_sub_80053E4(void) { GUEST_CALL(sub_8004298); }

/* register form: calls sub_8004590 outside this file, which has no typed C interface yet */
static void call_sub_8004590_sub_80053E4(void) { GUEST_CALL(sub_8004590); }

/* register form: calls sub_8004934 outside this file, which has no typed C interface yet */
static void call_sub_8004934_sub_80053E4(void) { GUEST_CALL(sub_8004934); }

/* register form: calls event_runArmedCallback_80024AE outside this file, which has no typed C interface yet */
static void call_event_callback(void) { GUEST_CALL(event_runArmedCallback_80024AE); }

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool is_screen_fade_active(void) {
    GUEST_CALL(IsScreenFadeActive);
    return ZERO_FLAG();  /* Z set → fade active → original takes beq */
}

/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void call_chatbox(void) { GUEST_CALL(chatbox_8040818); }

/* register form: calls FreeAllObjectsOfSpecifiedTypes outside this file, which has no typed C interface yet */
static void call_free_objects(u32 type) {
    R0 = type;
    GUEST_CALL(FreeAllObjectsOfSpecifiedTypes);
}

/* register form: calls clearWorkBuffer_2011A40_80341AA outside this file, which has no typed C interface yet */
static void call_clear_work_buffer(void) { GUEST_CALL(clearWorkBuffer_2011A40_80341AA); }

/* register form: calls resetAllGraphicsState_8005F40 outside this file, which has no typed C interface yet */
static void call_reset_all_graphics(u32 toolkit) {
    R7 = toolkit;
    GUEST_CALL(resetAllGraphicsState_8005F40);
}

/* register form: calls runSetupStepAndClearWorkArea_8005F6C outside this file, which has no typed C interface yet */
static void call_run_setup_step(void) { GUEST_CALL(runSetupStepAndClearWorkArea_8005F6C); }

/* register form: calls sub_80027C4 outside this file, which has no typed C interface yet */
static void call_sub_80027C4_sub_80053E4(void) { GUEST_CALL(sub_80027C4); }

/* register form: calls zeroFill_80024A2 outside this file, which has no typed C interface yet */
static void call_zero_fill_80024A2(void) { GUEST_CALL(zeroFill_80024A2); }

/* register form: calls scenario_initEffectState_8003962 outside this file, which has no typed C interface yet */
static void call_init_effect_state(void) { GUEST_CALL(scenario_initEffectState_8003962); }

/* register form: calls zeroFill_8003AB2 outside this file, which has no typed C interface yet */
static void call_zero_fill_8003AB2(void) { GUEST_CALL(zeroFill_8003AB2); }

/* register form: calls RandomizeExtraToolkitPointers outside this file, which has no typed C interface yet */
static void call_randomize_pointers(void) { GUEST_CALL(RandomizeExtraToolkitPointers); }

/* register form: calls reloadCurNaviStatBoosts_813c3ac outside this file, which has no typed C interface yet */
static void call_reload_navi_boosts(void) { GUEST_CALL(reloadCurNaviStatBoosts_813c3ac); }

/* register form: calls SetRenderInfoLCDControl outside this file, which has no typed C interface yet */
static void call_set_render_info(u32 value) {
    R0 = value;
    GUEST_CALL(SetRenderInfoLCDControl);
}

/* The 32-bit LDR of the literal pool entry word_8005460: the pool holds the
 * hword 0x0040, but ldr reads 4 bytes so the upper half picks up the adjacent
 * instruction encoding (push {r7,lr} = 0xB580), giving 0xB5800040.
 * The callee SetRenderInfoLCDControl uses the low 16 bits; the full word
 * remains in r0 on return and is an observable output. */
#define LCD_CTRL_32BIT  0xB5800040u

/* Per-frame scene update for the GameState dispatch table.
 * Runs NPC/sprite/event updates each frame; if a screen fade is active it
 * returns early. Otherwise it performs a full scene-transition reset:
 * frees objects, clears work buffers, sets the main jump table to step 8,
 * resets graphics, and configures the LCD for the next scene.
 *
 * Returns the final r5 value: the original gamestate pointer on the
 * fade-active path, or toolkit->gameStatePtr on the reset path. */
static u32 scene_update(u32 gamestate, u32 toolkit) {
    /* Per-frame updates: dispatch, NPC AI, sprites, events */
    call_dispatch();
    call_sub_80039AA_sub_80053E4();
    call_sub_8003AFA_sub_80053E4();
    call_sub_80027B4_sub_80053E4();
    call_sub_800286C_sub_80053E4();
    call_sub_8003BF4_sub_80053E4();
    call_sub_8003E98_sub_80053E4();
    call_npc_draw_sprites();
    call_sub_8004298_sub_80053E4();
    call_sub_8004590_sub_80053E4();
    call_sub_8004934_sub_80053E4();
    call_event_callback();

    if (is_screen_fade_active())
        return gamestate;

    /* Scene transition: clear the old scene and prepare the next */
    call_chatbox();
    call_free_objects(0x25);
    call_clear_work_buffer();

    /* Set main jump table index to 8 (next scene) */
    ST8(LD32(toolkit + oToolkit_MainJumptableIndexPtr), 8);

    call_reset_all_graphics(toolkit);
    call_run_setup_step();
    call_sub_80027C4_sub_80053E4();
    call_zero_fill_80024A2();
    call_init_effect_state();
    call_zero_fill_8003AB2();
    call_randomize_pointers();
    call_reload_navi_boosts();

    /* Configure LCD rendering for the new scene */
    call_set_render_info(LCD_CTRL_32BIT);

    /* r5 ends as the game state pointer from the toolkit */
    return LD32(toolkit + oToolkit_GameStatePtr);
}

/* scene_nextOrUpdate_80053E4(r5 = gamestate, r10 = toolkit) -> r5 */
Next scene_nextOrUpdate_80053E4(void) {
    R5 = scene_update(R5, R10);
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

    GUEST_CALL(npc_clearOverworldNpcStateWord_8004702);
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

/* ── scenario_finalizeFrameOrRestart_8005642 ── */
/* Main-jumptable state that scenario_finalizeFrameOrRestart_8005642 switches into once the scenario restart
 * sequence has run. */
#define MAIN_STATE_SCENARIO_RESTART 0x2C

/* Object-type selection passed to FreeAllObjectsOfSpecifiedTypes. The value is
 * still sitting in r0 when reloadCurNaviStatBoosts is called at the end of the
 * reset sequence, so it is passed on there as well. */
#define RESET_OBJECT_TYPES 0x21

/* IsScreenFadeActive answers in the Z flag, but on the early-exit path its r0/r1
 * leftovers are also caller-visible state, so the helper reports all three. */
struct FadeCheck_sub_8005642 {
    u32 r0;
    u32 r1;
    bool zero;
};

/* Final register values the adapter has to publish. Registers the original only
 * leaves behind from a callee (r2/r3, r8-r12, lr) and the C/V flags are already
 * correct in the guest register file and are left untouched by the adapter. */
struct ScenarioRestartResult {
    u32 r0;
    u32 r1;
    u32 r5;
    u32 r7;
    u32 nz_operand; /* value whose N/Z the adapter publishes, like the original's last flag write */
};

/* register form: calls dispatch_80339CC outside this file, which has no typed C interface yet */
static void call_dispatch_80339CC_sub_8005642(void) {
    GUEST_CALL(dispatch_80339CC);
}

/* register form: calls sub_80039AA outside this file, which has no typed C interface yet */
static void call_sub_80039AA_sub_8005642(void) {
    GUEST_CALL(sub_80039AA);
}

/* register form: calls sub_8003AFA outside this file, which has no typed C interface yet */
static void call_sub_8003AFA_sub_8005642(void) {
    GUEST_CALL(sub_8003AFA);
}

/* register form: calls sub_80027B4 outside this file, which has no typed C interface yet */
static void call_sub_80027B4_sub_8005642(void) {
    GUEST_CALL(sub_80027B4);
}

/* register form: calls sub_800286C outside this file, which has no typed C interface yet */
static void call_sub_800286C_sub_8005642(void) {
    GUEST_CALL(sub_800286C);
}

/* register form: calls sub_8003BF4 outside this file, which has no typed C interface yet */
static void call_sub_8003BF4_sub_8005642(void) {
    GUEST_CALL(sub_8003BF4);
}

/* register form: calls sub_8003E98 outside this file, which has no typed C interface yet */
static void call_sub_8003E98_sub_8005642(void) {
    GUEST_CALL(sub_8003E98);
}

/* register form: calls npc_drawSprites_800467C outside this file, which has no typed C interface yet */
static void call_npc_drawSprites_800467C_sub_8005642(void) {
    GUEST_CALL(npc_drawSprites_800467C);
}

/* register form: calls sub_8004298 outside this file, which has no typed C interface yet */
static void call_sub_8004298_sub_8005642(void) {
    GUEST_CALL(sub_8004298);
}

/* register form: calls sub_8004590 outside this file, which has no typed C interface yet */
static void call_sub_8004590_sub_8005642(void) {
    GUEST_CALL(sub_8004590);
}

/* register form: calls sub_8004934 outside this file, which has no typed C interface yet */
static void call_sub_8004934_sub_8005642(void) {
    GUEST_CALL(sub_8004934);
}

/* register form: calls event_runArmedCallback_80024AE outside this file, which has no typed C interface yet */
static void call_event_runArmedCallback_80024AE_sub_8005642(void) {
    GUEST_CALL(event_runArmedCallback_80024AE);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet;
 * r0/r1 are read here because on the early-exit path they are the last caller-visible
 * register state and nothing later in the sequence runs to leave them behind */
static struct FadeCheck_sub_8005642 check_screen_fade_sub_8005642(void) {
    struct FadeCheck_sub_8005642 result;
    GUEST_CALL(IsScreenFadeActive);
    result.r0 = R0;
    result.r1 = R1;
    result.zero = ZERO_FLAG();
    return result;
}

/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void call_chatbox_8040818_sub_8005642(void) {
    GUEST_CALL(chatbox_8040818);
}

/* register form: calls FreeAllObjectsOfSpecifiedTypes outside this file, which has no typed C interface yet */
static void free_all_objects_of_specified_types(u32 types) {
    R0 = types;
    GUEST_CALL(FreeAllObjectsOfSpecifiedTypes);
}

/* register form: calls npc_clearOverworldNpcStateWord_8004702 outside this file, which has no typed C interface yet */
static void call_sub_8004702_sub_8005642(void) {
    GUEST_CALL(npc_clearOverworldNpcStateWord_8004702);
}

/* register form: calls resetAllGraphicsState_8005F40 outside this file, which has no typed C interface yet */
static void call_resetAllGraphicsState_8005F40(void) {
    GUEST_CALL(resetAllGraphicsState_8005F40);
}

/* register form: calls runSetupStepAndClearWorkArea_8005F6C outside this file, which has no typed C interface yet */
static void call_runSetupStepAndClearWorkArea_8005F6C(void) {
    GUEST_CALL(runSetupStepAndClearWorkArea_8005F6C);
}

/* register form: calls sub_80027C4 outside this file, which has no typed C interface yet */
static void call_sub_80027C4_sub_8005642(void) {
    GUEST_CALL(sub_80027C4);
}

/* register form: calls zeroFill_80024A2 outside this file, which has no typed C interface yet */
static void call_zeroFill_80024A2(void) {
    GUEST_CALL(zeroFill_80024A2);
}

/* register form: calls scenario_initEffectState_8003962 outside this file, which has no typed C interface yet */
static void call_scenario_initEffectState_8003962(void) {
    GUEST_CALL(scenario_initEffectState_8003962);
}

/* register form: calls zeroFill_8003AB2 outside this file, which has no typed C interface yet */
static void call_zeroFill_8003AB2(void) {
    GUEST_CALL(zeroFill_8003AB2);
}

/* register form: calls RandomizeExtraToolkitPointers outside this file, which has no typed C interface yet */
static void call_RandomizeExtraToolkitPointers(void) {
    GUEST_CALL(RandomizeExtraToolkitPointers);
}

/* register form: calls reloadCurNaviStatBoosts_813c3ac outside this file, which has no typed C interface yet */
static void reload_cur_navi_stat_boosts_sub_8005642(u32 arg) {
    R0 = arg;
    GUEST_CALL(reloadCurNaviStatBoosts_813c3ac);
}

/* Finish the current frame's scenario bookkeeping, then, unless the screen fade
 * check says to hold off, tear the scenario down and step the main jumptable to
 * state 0x2C. r5_in/r7_in are returned unchanged on the early-exit path, where
 * the original never writes them. */
static struct ScenarioRestartResult restart_scenario(u32 toolkit, u32 r5_in, u32 r7_in) {
    struct ScenarioRestartResult res;

    /* end pending effects and draw the scene one final time */
    call_dispatch_80339CC_sub_8005642();
    call_sub_80039AA_sub_8005642();
    call_sub_8003AFA_sub_8005642();
    call_sub_80027B4_sub_8005642();
    call_sub_800286C_sub_8005642();
    call_sub_8003BF4_sub_8005642();
    call_sub_8003E98_sub_8005642();
    call_npc_drawSprites_800467C_sub_8005642();
    call_sub_8004298_sub_8005642();
    call_sub_8004590_sub_8005642();
    call_sub_8004934_sub_8005642();
    call_event_runArmedCallback_80024AE_sub_8005642();

    /* branch taken on the fade check: nothing to reset, leave the scene as-is */
    struct FadeCheck_sub_8005642 fade = check_screen_fade_sub_8005642();
    if (fade.zero) {
        res.r0 = fade.r0;
        res.r1 = fade.r1;
        res.r5 = r5_in;
        res.r7 = r7_in;
        res.nz_operand = 0; /* Z set and N clear, as the zero result of the check */
        return res;
    }

    /* full scenario reset: drop the chatbox, free objects, clear graphics and
     * work areas, reinitialize scenario state and the current navi's boosts */
    call_chatbox_8040818_sub_8005642();
    free_all_objects_of_specified_types(RESET_OBJECT_TYPES);
    call_sub_8004702_sub_8005642();
    call_resetAllGraphicsState_8005F40();
    call_runSetupStepAndClearWorkArea_8005F6C();
    call_sub_80027C4_sub_8005642();
    call_zeroFill_80024A2();
    call_scenario_initEffectState_8003962();
    call_zeroFill_8003AB2();
    call_RandomizeExtraToolkitPointers();
    reload_cur_navi_stat_boosts_sub_8005642(RESET_OBJECT_TYPES);

    /* park the game state pointer and step the main jumptable */
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 main_jumptable_index = LD32(toolkit + oToolkit_MainJumptableIndexPtr);
    ST8(main_jumptable_index, MAIN_STATE_SCENARIO_RESTART);

    res.r0 = main_jumptable_index;
    res.r1 = MAIN_STATE_SCENARIO_RESTART;
    res.r5 = game_state;
    res.r7 = toolkit;
    res.nz_operand = MAIN_STATE_SCENARIO_RESTART; /* mov r1,#0x2C leaves N clear, Z clear */
    return res;
}

/* scenario_finalizeFrameOrRestart_8005642: (r10 = toolkit, r5/r7 = live across the call) -> r0 r1 r5 r7, nz */
Next scenario_finalizeFrameOrRestart_8005642(void) {
    struct ScenarioRestartResult res = restart_scenario(R10, R5, R7);
    R0 = res.r0;
    R1 = res.r1;
    R5 = res.r5;
    R7 = res.r7;
    SET_FLAGS_NZ(res.nz_operand);
    RETURN_TO_CALLER();
}

/* ── mainState_runFrame_8005814 ── */
/* mainState_runFrame_8005814: per-frame main loop dispatch for the current game state.
 * Calls the state handlers in order; if a screen fade is not active, also
 * runs the chatbox/overlay updates and the "reset game and go to state 28"
 * path (e.g. title -> title). */
Next mainState_runFrame_8005814(void) {
    GUEST_CALL(dispatch_80339CC);
    GUEST_CALL(sub_80039AA);
    GUEST_CALL(sub_8003AFA);
    GUEST_CALL(sub_80027B4);
    GUEST_CALL(sub_800286C);
    GUEST_CALL(sub_8003BF4);
    GUEST_CALL(sub_8003E98);
    GUEST_CALL(npc_drawSprites_800467C);
    GUEST_CALL(sub_8004298);
    GUEST_CALL(sub_8004590);
    GUEST_CALL(sub_8004934);
    GUEST_CALL(event_runArmedCallback_80024AE);
    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG()) {
        GUEST_CALL(chatbox_8040818);
        GUEST_CALL(sub_811F6E0);
        GUEST_CALL(mainState_resetGameAndGoto28_8005524);
    }
    RETURN_TO_CALLER();
}

/* ── sub_80058D0 ── */
/* ---------------------------------------------------------------------------
 * sub_80058D0 -- overworld warp trigger check
 *
 * Runs from the overworld dispatcher.  While the on-map-update gamestate is
 * active and nothing else owns the screen (no fade, no cutscene, no PET menu
 * state), the player's coordinates are matched against the current map's
 * warp triggers.  When one fires and its "warp seen" event flag is still
 * clear, the warp is armed in the oWarp2011bb0 state block and the cutscene
 * handler matching the warp's type is started via JumpTable8005948.
 *
 * The jump table is indexed by the warp's type *byte* (ldr r0,[r0,r1] with no
 * shift), so warp types 0,4,8,...,0x20 select its nine entries:
 *   0x00 gameState_beginEndingSequence_800596C  fade to black warp
 *   0x04 sub_8005990                            fade to white warp
 *   0x08 map_onMapWarpCutscene_80059B4          jack out anim, fade to black,
 *                                               fade from black, jack in anim,
 *                                               walk in direction
 *   0x0C startInMapWarpCutscene_80059D0         jack out anim, auto camera
 *                                               scroll to another area in the
 *                                               same map, jack in anim, walk in
 *                                               direction
 *   0x10 sub_80059EC                            jack out warp
 *   0x14 sub_8005A00                            levbus warp
 *   0x18 sub_8005A0C                            underground warp
 *   0x1C sub_8005A28                            like 0x00, but with a delay +
 *                                               plays a hardcoded sound
 *   0x20 sub_8005A50                            same as above, different delay
 * ------------------------------------------------------------------------- */

/* oGameState_SubsystemIndex value while the map update gamestate is running. */
#define SUBSYSTEM_ON_MAP_UPDATE 4
/* Highest trigger index checkCoordinateTrigger_8031a7a may report. */
#define WARP_TRIGGER_MAX 0xF

/* register form: calls sub_809E462 outside this file, which has no typed C interface yet.
 * Answers in flags: Z set means the on-map update is ready for a warp to be checked. */
static bool on_map_update_ready(void) {
    GUEST_CALL(sub_809E462);
    return ZERO_FLAG();
}

/* register form: calls warp_LoadCurrentAndReadUnk10_8005F28 outside this file, which has no
 * typed C interface yet.  Answers in flags: Z set means the warp state block is free to arm. */
static bool warp_state_ready(void) {
    GUEST_CALL(warp_LoadCurrentAndReadUnk10_8005F28);
    return ZERO_FLAG();
}

/* register form: calls TestPETMenuDataFlag outside this file, which has no typed C interface yet.
 * Z set if the flag is clear. */
static bool pet_menu_data_flag_set_sub_80058D0(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestPETMenuDataFlag);
    return !ZERO_FLAG();
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet.
 * Answers in flags: Z set while a screen fade is running. */
static bool screen_fade_active_sub_80058D0(void) {
    GUEST_CALL(IsScreenFadeActive);
    return ZERO_FLAG();
}

/* register form: calls IsCutsceneScriptNonNull outside this file, which has no typed C interface
 * yet.  Answers in flags: Z clear while a cutscene script is running. */
static bool cutscene_script_non_null_sub_80058D0(void) {
    GUEST_CALL(IsCutsceneScriptNonNull);
    return !ZERO_FLAG();
}

/* register form: calls checkCoordinateTrigger_8031a7a outside this file, which has no typed C
 * interface yet.  Returns the trigger index delivered in r1; the original also copies it into
 * r4 (mov r4, r1) and that copy must stay live in guest r4 through later calls and early
 * returns up to the function's exit, which a typed C function cannot express. */
static s32 check_coordinate_trigger(u32 coords) {
    R0 = coords;
    GUEST_CALL(checkCoordinateTrigger_8031a7a);
    R4 = R1;
    return (s32)R1;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet.
 * Z set if the flag is clear. */
static bool event_flag_set(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: the warp cutscene handler is fetched from JumpTable8005948 at runtime and
 * reached through `mov lr, pc; bx r0`, so no typed wrapper exists for it.  Plants the register
 * state the original hands each handler at that call: r0 = handler address, r1 = jump table,
 * r2 = this warp's data entry, r4 = its byte offset into the warp data array, r5 = game state
 * (the handlers' self pointer).  The handler's leftover register state (including r8-r12,
 * which this function outputs) is left in the guest registers by the call itself. */
static u32 start_warp_cutscene(u32 handler, u32 warp_data, u32 data_offset, u32 game_state) {
    R0 = handler;
    R1 = A_JumpTable8005948;
    R2 = warp_data;
    R4 = data_offset;
    R5 = game_state;
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Check the player's position against the map's warp triggers and start the matching warp
 * cutscene when one fires.  Returns entry_r5 unchanged; the original's push/pop keeps r5
 * intact across every path, so the adapter writes it back. */
static u32 check_map_warp_start(u32 entry_r5, u32 toolkit) {
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);

    /* Warp triggers are only honoured while the on-map-update gamestate is active. */
    if (LD8(game_state + oGameState_SubsystemIndex) != SUBSYSTEM_ON_MAP_UPDATE)
        return entry_r5;

    /* Bail out while anything else rules out starting a warp. */
    if (!on_map_update_ready())
        return entry_r5;
    if (!warp_state_ready())
        return entry_r5;
    if (pet_menu_data_flag_set_sub_80058D0(1)) /* PET menu data flag 1 must be clear */
        return entry_r5;
    if (screen_fade_active_sub_80058D0())
        return entry_r5;
    if (cutscene_script_non_null_sub_80058D0())
        return entry_r5;

    /* Which warp trigger, if any, is the player standing on? */
    u32 player = LD32(game_state + oGameState_OverworldPlayerObjectPtr);
    s32 warp_index = check_coordinate_trigger(player + oOWPlayerObject_Coords);
    if (warp_index < 1 || warp_index > WARP_TRIGGER_MAX)
        return entry_r5;

    /* Each trigger has a "warp seen" flag; ignore warps already taken. */
    if (event_flag_set(EVENT_16F0 + (u32)warp_index))
        return entry_r5;

    /* Arm the warp in the shared warp state block. */
    u32 warp = LD32(toolkit + oToolkit_Warp2011bb0_Ptr);
    ST8(warp + oWarp2011bb0_Unk_10, 1);
    ST8(warp + oWarp2011bb0_WarpIndex, (u8)warp_index);
    ST8(warp + oWarp2011bb0_MapGroupTransitionType, 0);

    /* Hand control to the cutscene for this warp's type, pointed at its data entry.  The
     * type byte indexes the jump table directly, one 4-byte word per type. */
    u32 data_offset = (u32)(warp_index - 1) * oWarpData_Size;
    u32 warp_data = LD32(warp + oWarp2011bb0_WarpDataPtr) + data_offset;
    u32 warp_type = LD8(warp_data + oWarpData_warpType_02);
    u32 handler = LD32(A_JumpTable8005948 + warp_type);
    start_warp_cutscene(handler, warp_data, data_offset, game_state);
    return entry_r5;
}

/* sub_80058D0(r10 = toolkit) -> r1..r12; r5 preserved on every path.
 * r1/r2/r3/r4 on exit are whatever the last guest callee left behind (or the armed warp
 * values handed to the cutscene handler), which the GUEST_CALLs above already put in place.
 * r8-r12 are the original's outputs: unchanged on the early paths, and on the warp path
 * whatever the cutscene handler left, which the guest calls already left in place. */
Next sub_80058D0(void) {
    R5 = check_map_warp_start(R5, R10);
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

/* register form: calls battle_tryEncounter_80AA4C0 outside this file, which has no typed C interface yet */
static u32 get_battle_settings(void **settings_out) {
    GUEST_CALL(battle_tryEncounter_80AA4C0);
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
