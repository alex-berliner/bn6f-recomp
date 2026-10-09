/* gen.py --test: readable code in the target convention (docs/C_CALLING_CONVENTION.md §1),
 * compiled against the generated abi.h, lifted_calls.h and gen/structs.h for the GBA and
 * the 64-bit host. Not part of any build. */
#include "game.h"
#include "abi.h"
#include "lifted_calls.h"

/* The pilot: the whole body, no guest registers (doc §1). */
void setBGTileIdsBlocks1And2_8030628(u16 tileId1, u16 tileId2, u32 col, u32 row) {
    u16 *blocks = G(u16, toolkit.iBGTileIdBlocks_Ptr);
    u32 entry = row * 32 + col;
    blocks[1 * 0x400 + entry] = tileId1;
    blocks[2 * 0x400 + entry] = tileId2;
}

/* Readable code asking a register-convention callee through its typed wrapper: the answer
 * is a bool taken from Z, not whatever byte the callee left in r0. */
bool abiTest_flagAndKeep(OverworldNPCObject *npc, u32 count) {
    if (call_TestEventFlag(0x1234) && !call_TestEventFlagFromImmediate(0x12, 0x34))
        npc->Flags |= OBJECT_FLAG_ACTIVE;
    return count > npc->Index;
}

U32Pair abiTest_pairAndCursor(u32 x, u32 *cursor) {
    u32 in = *cursor;
    u8 *p = call_abiTest_lifted(G(Toolkit, 0x020093B0u), cursor, -1);
    U32Pair r = call_abiTest_liftedPair();
    return (U32Pair){ x + in + *p, r.first ^ r.second };
}

bool abiTest_carry(s16 value) {
    return value < 0;
}
