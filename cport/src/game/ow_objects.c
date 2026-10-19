/*
 * ow_objects.c — overworld objects: layer priority and layer effects, the
 * zone lookup they use, interaction areas, and a BG tile-map helper
 * (bn6f asm00_1.s / asm03_0.s / asm03_1.s / npc.s).
 *
 * Written by outside models with tools/convert/farm.py and accepted only
 * after matching the lifted functions on the original's own captured calls
 * (40 per function, from tools/traces/wander.trace) and random inputs;
 * reviewed and integrated by Claude, then checked with the host difftest,
 * the CPU oracle and lockstep (see symbols/db). Models: Hyper GLM-5.3 Flash
 * (most), Z.ai GLM-5.3 Flash (applyLayerEffectToOWObject_8035694), Z.ai
 * GLM-5.3 (setBGTileIdsBlocks1And2_8030628). Field offsets the shared header does not export are
 * defined below from bn6f include/structs.
 */
#include "game.h"
#include "runtime.h"

enum {  /* bn6f include/structs: GameState, OWObjectCoords, Struct8034460 */
    GAMESTATE_MAP_ID = 0x04,
    COORDS_X_FULL = 0x00, COORDS_Y_FULL = 0x04, COORDS_Z_FULL = 0x08,
    REGION_MAP_ID = 0x00, REGION_Z_LOW = 0x04, REGION_Z_HIGH = 0x08,
};
enum {  /* bn6f include/structs/OWObjectInteractionArea.inc */
    AREA_XY_PACKED = 0x00, AREA_Z_KIND = 0x04, AREA_WORD_08 = 0x08,
};

/* ── BG tile map ─────────────────────────────────────────────────────── */

/* iBGTileIdBlocks is a set of 32x32 tile-id maps (u16 entries, 0x800 bytes per
 * block); entry (row, col) of a block sits at row*0x40 + col*2. */
#define TILE_ID_BLOCK_BYTES 0x800

/* setBGTileIdsBlocks1And2_8030628(r0 = tileId1, r1 = tileId2, r3 = col, r4 = row, r10 = toolkit):
 * store one tile id into each of blocks 1 and 2 of the toolkit's BG tile map
 * buffers, at the same (row, col) entry of both blocks.
 * (The original fakes an array over its two arguments with push/pop and a
 * two-pass loop over block indices 1 and 2.) */
Next setBGTileIdsBlocks1And2_8030628(void) {
    u32 blocks = LD32(R10 + oToolkit_iBGTileIdBlocks_Ptr);
    u32 entryOffset = (R4 << 6) + (R3 << 1); /* row * 0x40 + col * 2 */

    ST16(blocks + 1 * TILE_ID_BLOCK_BYTES + entryOffset, R0); /* block 1 */
    ST16(blocks + 2 * TILE_ID_BLOCK_BYTES + entryOffset, R1); /* block 2 */

    /* Reproduce the loop's leftover registers and exit flags for callers. */
    R0 = R1;             /* the pop trick ends with the saved r1 loaded into r0 */
    R2 = 3;              /* block counter after the two iterations */
    R6 = R4 << 6;        /* last offsets computed inside the loop */
    R7 = blocks + 2 * TILE_ID_BLOCK_BYTES + (R3 << 1);
    SET_FLAGS_CMP(R2, 2); /* the loop's exit compare (3 > 2) */
    RETURN_TO_CALLER();
}

/* ── zone lookup ─────────────────────────────────────────────────────── */

/* getOWObjectGridCellIndex_80316F8(r0 = overworld object, r5 = Ex2011a20 struct):
 * Converts the object's pixel coordinates into an index into a grid whose
 * row stride is unk06 (low bit cleared), offset by unk07/2 rows and
 * unk06/2 columns, and stores the result at unk08. Also stores the
 * tile-aligned coordinates (x & ~7, y & ~7) at unk0c and unk10.
 * Returns the grid index in r1 (r3 ends as the 7 used for the tile mask). */
Next getOWObjectGridCellIndex_80316F8(void) {
    u32 obj = R0;
    u32 st = R5;

    s32 x = (s32)LDS16(obj + 2); /* oOWObjectCoords_X: ldrsh, as the original */
    s32 y = (s32)LDS16(obj + 6); /* oOWObjectCoords_Y: ldrsh */

    /* pixel -> tile coordinates (arithmetic shift, keeps the sign) */
    u32 tileX = (u32)((s32)x >> 3);
    u32 tileY = (u32)((s32)y >> 3);

    /* unk06 and unk07 are the grid's width/height in half-tiles?; the
     * index uses unk06/2 as column offset, unk07/2 as row offset, and
     * unk06 (with its low bit cleared) as the row stride */
    u32 colOff = LD8(st + 6) >> 1; /* unk06/2 */
    u32 rowOff = LD8(st + 7) >> 1; /* unk07/2 */

    /* index = (unk06 & ~1) * (y/8 + unk07/2) + x/8 + unk06/2 */
    ST16(st + 8, (colOff * 2) * (tileY + rowOff) + tileX + colOff);

    /* remember the top-left corner of the object's current tile */
    ST32(st + oUnk_Ex2011a20_Unk_0c, (u32)x & ~7u);
    ST32(st + oUnk_Ex2011a20_Unk_10, (u32)y & ~7u);

    R1 = LD16(st + 8);
    R3 = 7;
    RETURN_TO_CALLER();
}

/* findFirstEntryWithId_8030B1E(r1 = id, r5 = list header):
 * Binary-search the sorted table whose base pointer is stored at header+0
 * (halfword entries, 4 bytes apart) for the halfword id passed in r1. The
 * entry count is the halfword at header+4. If the id is present, walk
 * backwards over consecutive equal entries to find the first occurrence.
 * Returns r2 = address of the first matching entry, or NULL if not found.
 * Side results left for the caller: r8 = table base, r3 = final upper bound
 * of the search range, r7 = last compared halfword (all untouched when the
 * table is empty). */
Next findFirstEntryWithId_8030B1E(void) {
    u32 id = R1;
    u32 header = R5;
    u32 count = LD16(header + 4);   /* oUnk_Ex2011a20_Unk_04 */

    if (count == 0) {
        R2 = 0;
        RETURN_TO_CALLER();
    }

    u32 base = LD32(header);        /* oUnk_Ex2011a20_UnkPtr_00 */
    R8 = base;
    R3 = count;                     /* upper bound of the search range */

    u32 low = 0;
    u32 mid;
    u32 val;
    for (;;) {
        mid = (low + R3) >> 1;
        val = LD16(base + mid * 4);
        R7 = val;
        if ((s32)id == (s32)val)
            break;                  /* hit: keep val in r7 for the caller */
        if ((s32)id > (s32)val)
            low = mid + 1;          /* id lies above mid */
        else
            R3 = mid;               /* id lies below mid: shrink upper bound */
        if ((s32)low >= (s32)R3) {
            R2 = 0;                 /* range exhausted: id is not in the table */
            RETURN_TO_CALLER();
        }
    }

    /* Found: step back over duplicate entries to the first occurrence.
     * (The original re-reads the base pointer from header+0 here; it is
     * the same value that was already loaded above.) */
    u32 cur = base + mid * 4;
    for (;;) {
        cur -= 4;
        if ((s32)cur < (s32)base)
            break;
        R7 = LD16(cur);
        if (id != R7)
            break;
    }
    R2 = cur + 4;
    RETURN_TO_CALLER();
}

/* ── layer priority and layer effects ────────────────────────────────── */

/* checkLayerPriority_80318b0(r0 = object):
 * Records the object as the one under test, then walks the zone-entry list
 * built by findFirstEntryWithId_8030B1E. Each entry is { u16 id; u16 offset; } and points at
 * a record { s8 top; u8 pad; u8 height; u8 kind; } reached through the base
 * pointer at dword_2013940. When the object's Y coordinate (signed halfword
 * at obj+0xA) lies within [top, top+height], the record pointer is stored at
 * table+0x14 and the handler for the record's kind is invoked with
 * r0 = object, r1 = id, r2 = entry, r3 = height, r6 = kind*4, r7 = record.
 * Outputs: r0 = 2 if no entry matched, otherwise whatever the handler left
 * in r0. r4-r9 and r12 are restored to their incoming values on exit. */
Next checkLayerPriority_80318b0(void) {
    u32 obj = R0;
    u32 table = A_dword_2013940;

    /* The callees run with r0-r3/r12 as scratch and findFirstEntryWithId_8030B1E even returns
     * something in r8; the original pushes r4-r7 and r8/r9/r12 and pops them
     * back on exit, so save the incoming values here. */
    u32 inR4 = R4, inR5 = R5, inR6 = R6, inR7 = R7;
    u32 inR8 = R8, inR9 = R9, inR12 = R12;

    ST32(A_dword_200F3D0, obj);

    R5 = table;
    GUEST_CALL(getOWObjectGridCellIndex_80316F8);        /* -> R1 = zone id to look for */
    u32 id = R1;

    GUEST_CALL(findFirstEntryWithId_8030B1E);        /* -> R2 = first list entry, or NULL */
    u32 entry = R2;

    if (entry == 0) {
        R0 = 2;
    } else {
        for (;;) {
            /* Entries are grouped by id; a different id means no hit. */
            if (id != LD16(entry)) {
                R0 = 2;
                break;
            }

            u32 rec = LD32(table) + LD16(entry + 2);
            s32 y = (s32)LDS16(obj + 0xA);   /* ldrsh */
            s32 top = (s8)LD8(rec);
            if (y < top || y > top + (s32)LD8(rec + 2)) {
                entry += 4;         /* vertical span misses; try next entry */
                continue;
            }

            /* Hit: remember the record and dispatch on its kind. */
            ST32(table + 0x14, rec);
            u32 kind = LD8(rec + 3);
            R0 = obj;
            R1 = id;
            R2 = entry;
            R3 = LD8(rec + 2);
            R5 = table;
            R6 = kind * 4;
            R7 = rec;
            switch (kind) {
            case 1:  GUEST_CALL(battle_aiActorCountForTargetSide); break;
            case 2:  GUEST_CALL(sub_80319B6); break;
            case 3:  GUEST_CALL(sub_80319CE); break;
            case 4:  GUEST_CALL(battle_aiActorCountByTargetSide); break;
            case 9:  GUEST_CALL(battle_aiTargetInRangeActorCount); break;
            case 10: GUEST_CALL(sub_8031A16); break;
            case 11: GUEST_CALL(sub_8031A2E); break;
            case 12: GUEST_CALL(battle_aiActorCountByFieldSide); break;
            case 17: GUEST_CALL(battleObject_getChipLockColourOne_8031A5E); break;
            default: break;         /* table slot is NULL; never taken */
            }
            /* r0 already holds the handler's result. */
            break;
        }
    }

    R4 = inR4; R5 = inR5; R6 = inR6; R7 = inR7;
    R8 = inR8; R9 = inR9; R12 = inR12;
    RETURN_TO_CALLER();
}

/* applyLayerEffectToOWObject_8035694(r0 = overworld object, r10 = toolkit):
 * Work out which layer an overworld object is rendered on. A static table at
 * Struct8034460 describes per-map layer-effect regions: one record of
 * oStruct8034460_Size bytes per region, the end marked by a record with a
 * negative map id. The first region of the current map (GameState -> MapId)
 * whose signed X/Y/Z bounds contain the object's full coordinates wins, and
 * its value (Unk_0c) becomes the object's layer. If no region matches, the
 * object's own layer priority is used instead (checkLayerPriority_80318b0).
 * Result in r0; r4-r9 and r12 come back exactly as they entered. */

/* Is the object's full-precision position inside this region? Equal X bounds
 * act as a wildcard: the X and Y tests are skipped entirely and only Z
 * decides. Coordinates and bounds are signed 32-bit fixed point, so compare
 * them signed. */
static u32 layerRegionContainsObject(u32 object, u32 region)
{
    const s32 xLow = (s32)LD32(region + oStruct8034460_XLowerBound);
    const s32 xHigh = (s32)LD32(region + oStruct8034460_XUpperBound);

    if (xLow != xHigh) {
        const s32 x = (s32)LD32(object + COORDS_X_FULL);
        const s32 y = (s32)LD32(object + COORDS_Y_FULL);
        const s32 yLow = (s32)LD32(region + oStruct8034460_YLowerBound);
        const s32 yHigh = (s32)LD32(region + oStruct8034460_YUpperBound);

        if (x < xLow || x > xHigh || y < yLow || y > yHigh)
            return 0;
    }

    {
        const s32 z = (s32)LD32(object + COORDS_Z_FULL);
        const s32 zLow = (s32)LD32(region + REGION_Z_LOW);
        const s32 zHigh = (s32)LD32(region + REGION_Z_HIGH);
        return (u32)(z >= zLow && z <= zHigh);
    }
}

Next applyLayerEffectToOWObject_8035694(void)
{
    const u32 object = R0;

    /* The original borrows r4-r7 for its locals and pushes/pops them around the
     * body, so callers never see the borrowed values; r6 is never touched. */
    const u32 savedR4 = R4;
    const u32 savedR5 = R5;
    const u32 savedR6 = R6;
    const u32 savedR7 = R7;

    const u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    const u32 mapId = LD16(gameState + GAMESTATE_MAP_ID);

    u32 region = A_Struct8034460;
    u32 layer = 0;
    u32 matched = 0;

    while (!matched) {
        const s32 regionMapId = (s32)LD32(region + REGION_MAP_ID);

        if (regionMapId < 0)
            break; /* negative map id marks the end of the table */

        if (regionMapId == (s32)mapId && layerRegionContainsObject(object, region)) {
            layer = LD32(region + oStruct8034460_Unk_0c);
            matched = 1;
        } else {
            region += oStruct8034460_Size;
        }
    }

    if (matched) {
        /* The object stands inside a mapped region: that region's layer wins. */
        R0 = layer;
    } else {
        /* No region covers the object: fall back to its own layer priority.
         * The original keeps map id / object / table cursor in r4/r5/r7 across
         * this call (r7 still points at the terminator record); r6 is still
         * the caller's value. */
        R0 = object;
        R4 = mapId;
        R5 = object;
        R7 = region;
        GUEST_CALL(checkLayerPriority_80318b0);
    }

    /* pop {r4-r7}: the borrowed registers end as they entered. */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;

    RETURN_TO_CALLER();
}

/* ── interaction areas ───────────────────────────────────────────────── */

/* createOWObjectInteractionArea_80037ac(r0 = x, r1 = y, r2 = z, r3 = kind,
 *                                       r4, r5, r6 = payload words):
 * Append one entry to the overworld interaction-area array. Coordinates
 * arrive as 16.16 fixed point and are rounded to integers by adding 0x8000
 * and keeping the upper half; x and y are packed into one word (y in the
 * high half), z goes into the low half of the second word with the raw kind
 * in its high half. If the array already holds 32 entries the request is
 * dropped. Returns the packed words in r1/r2, leaves r3 shifted left by 16,
 * and sets the flags from the capacity comparison (array full) or from the
 * counter increment (entry stored). r7 and r4-r6 are untouched. */
Next createOWObjectInteractionArea_80037ac(void) {
    u32 x = (R0 + 0x8000) >> 16;
    u32 y = (R1 + 0x8000) >> 16;
    u32 z = (R2 + 0x8000) >> 16;

    R1 = (y << 16) | x;     /* oOWObjectInteractionArea.xyPacked_00 */
    R2 = z | (R3 << 16);    /* oOWObjectInteractionArea.Z_Radius_ZReachAmount */
    R3 <<= 16;              /* the original leaves r3 shifted after packing */

    u32 countPtr = A_eNumOWObjectInteractionAreas;
    u32 count = LD32(countPtr);
    SET_FLAGS_CMP(count, 0x20);
    if ((s32)count >= 0x20) {
        /* Array full: drop the request; flags stay from the comparison. */
        RETURN_TO_CALLER();
    }

    /* Entry slot = array base + index * entry size. */
    u32 entry = A_eOWObjectInteractionAreas
              + count * oOWObjectInteractionArea_Size;
    ST32(entry + AREA_XY_PACKED, R1);
    ST32(entry + AREA_Z_KIND, R2);
    ST32(entry + AREA_WORD_08, R4);
    ST32(entry + oOWObjectInteractionArea_InteractionFlagMask, R5);
    ST32(entry + oOWObjectInteractionArea_InteractionFlags, R6);

    ST32(countPtr, count + 1);
    SET_FLAGS_ADD(count, 1); /* mirrors the original's ADDS r0, #1 */
    RETURN_TO_CALLER();
}

/* npc_createInteractionAreas_809F526 (r5 = overworld NPC object):
 * Rebuilds the NPC's interaction areas for this frame, then clears the two
 * scratch records the area builder filled in.
 *
 * Register contract (matching the original's epilogue):
 *   - r5 and r7 are preserved: r5 is popped back, and the entry push of r7
 *     means the caller's r7 is restored even though r7 holds the NPC's
 *     UnkFlags_60 word while the routine runs.
 *   - r2/r3 carry the results of the last createOWObjectInteractionArea
 *     call; when the second call is skipped, the original pops the first
 *     call's arguments back into r0-r5, so r2-r6 keep those values instead.
 *   - c/v come from the last createOWObjectInteractionArea call; in the
 *     inactive-object path no flag-setting instruction runs, so they are
 *     left untouched.
 *   - r0/r1 are scratch.
 *
 * Field/macro notes: oOverworldNPCObject_ObjectHeader is offset 0 of the
 * struct, its bit 0 (OBJECT_FLAG_ACTIVE) marks the object as active,
 * OW_NPC_UNK_FLAGS_60_DISABLE_COLLISION is bit 2 of UnkFlags_60, and the
 * interaction-area flag words are plain constants.
 */
Next npc_createInteractionAreas_809F526(void) {
    u32 npc = R5;
    u32 savedR7 = R7;
    u32 flags60 = LD32(npc + oOverworldNPCObject_UnkFlags_60);

    /* Only active objects build interaction areas. */
    if (LD8(npc) & 1) { /* oOverworldNPCObject_ObjectHeader bit 0 = active */
        /* The first area request carries the 0x20000 flag unless the NPC
         * has collision disabled (UnkFlags_60 bit 2), in which case the
         * flag word is zero. */
        u32 areaFlag = 0x20000; /* OW_OBJECT_INTERACTION_AREA_FLAG_0x20000 */
        if (flags60 & 4)        /* OW_NPC_UNK_FLAGS_60_DISABLE_COLLISION */
            areaFlag = 0;

        /* Position plus sprite center offset, in fixed point. */
        u32 centerX = ((u32)(s8)LD8(npc + oOverworldNPCObject_CenterOffsetX)) << 16;
        u32 centerY = ((u32)(s8)LD8(npc + oOverworldNPCObject_CenterOffsetY)) << 16;
        u32 centerZ = ((u32)(s8)LD8(npc + oOverworldNPCObject_CenterOffsetZ)) << 16;
        u32 x = LD32(npc + oOverworldNPCObject_X) + centerX;
        u32 y = LD32(npc + oOverworldNPCObject_Y) + centerY;

        /* Height: when interaction is locked, take Z from the coordinate
         * modifiers at the NPC's position; otherwise use the object's own Z. */
        u32 locked = LD8(npc + oOverworldNPCObject_InteractionLocked);
        u32 z;
        if (locked != 0) {
            /* The callee sees the same register state as the original: the
             * pending area flag in r4, the NPC pointer and center-Y offset,
             * the flag word in r7, and the Y position / lock byte in r1/r2. */
            R0 = npc + oOverworldNPCObject_Coords;
            R1 = y;
            R2 = locked;
            R4 = areaFlag;
            R5 = npc;
            R6 = centerY;
            R7 = flags60;
            GUEST_CALL(checkZCoordModifiers_8031612);
            z = R0 << 16;
        } else {
            z = LD32(npc + oOverworldNPCObject_Z);
        }
        z += centerZ;

        u32 radius = LD16(npc + oOverworldNPCObject_CollisionRadius_ZReach);

        /* Area 1: the NPC's own footprint, written to the Unk_50 record. */
        R0 = x;
        R1 = y;
        R2 = z;
        R3 = radius;
        R4 = areaFlag;
        R5 = 0x50001; /* OW_OBJECT_INTERACTION_AREA_FLAG_0x1 | 0x10000 | 0x40000 */
        R6 = npc + oOverworldNPCObject_BusyFlag;
        R7 = flags60;
        GUEST_CALL(createOWObjectInteractionArea_80037ac);

        if (flags60 & (4 | OW_NPC_UNK_FLAGS_60_DISABLE_COLLISION_0x10)) {
            /* Collision fully disabled: no second area. The original pops
             * the first call's arguments back off the stack, so r2-r6 end
             * up holding the first request's coordinates/radius, flag word
             * and destination record. */
            R2 = z;
            R3 = radius;
            R4 = areaFlag;
            R6 = npc + oOverworldNPCObject_BusyFlag;
        } else {
            /* Area 2: the larger interaction range, written to Unk_58.
             * r2/r3 keep the callee's results; r4/r5/r6 stay as set here. */
            R0 = x;
            R1 = y;
            R2 = z;
            R3 = radius;
            R4 = 0x400000; /* OW_OBJECT_INTERACTION_AREA_FLAG_0x400000 */
            R5 = 0xA00000; /* OW_OBJECT_INTERACTION_AREA_FLAG_0x200000 | 0x800000 */
            R6 = npc + oOverworldNPCObject_InteractionAreaHandle;
            GUEST_CALL(createOWObjectInteractionArea_80037ac);
        }
        R5 = npc; /* r5 is popped back to the object pointer */
    }

    /* The scratch records are only needed while building the areas. */
    ST32(npc + oOverworldNPCObject_BusyFlag, 0);
    ST32(npc + oOverworldNPCObject_InteractionAreaHandle, 0);
    R5 = npc;
    R7 = savedR7; /* pop {r7,pc} restores the caller's r7 */
    RETURN_TO_CALLER();
}

/* ── interaction-area overlap (Hyper GLM-5.3 Flash, farm batch 3; ldrsh -> LDS16) ── */

enum {  /* OWObjectInteractionArea as halfwords: x, y, z, radius, z reach */
    AREA16_X = 0, AREA16_Y = 2, AREA16_Z = 4, AREA16_RADIUS = 6, AREA16_ZREACHAMOUNT = 7,
};

/* Struct offsets for the overworld object interaction area, matching the
 * disassembly's oOWObjectInteractionArea_* field references. */

/* checkOWObjectInteractionAreasOverlap_8003894(r0 = area A, r1 = area B):
 * Checks whether the interaction areas of two overworld objects overlap.
 * The X/Y check is a circle-circle test: the squared distance between the
 * two centers must be below the squared sum of the two radii. If it is,
 * the object with the lower Z coordinate extends its Z reach amount toward
 * the other one; the interaction succeeds if it can reach it.
 * Returns r0 = 1 on success, 0 otherwise.
 * r2 is left as a scratch value that differs per exit path (matching the
 * original): the squared distance on the early "too far apart" exit,
 * otherwise A's Z coordinate (plus A's reach amount when A was the one
 * reaching). */
Next checkOWObjectInteractionAreasOverlap_8003894(void) {
    u32 areaA = R0;
    u32 areaB = R1;

    /* Squared distance between the two centers in the X/Y plane, and the
     * squared sum of the two radii. All arithmetic is 32-bit like the
     * original's MUL/ADD instructions. */
    u32 dx = (u32)((s32)LDS16(areaA + AREA16_X)
                 - (s32)LDS16(areaB + AREA16_X));
    u32 dy = (u32)((s32)LDS16(areaA + AREA16_Y)
                 - (s32)LDS16(areaB + AREA16_Y));
    u32 distSq = dx * dx + dy * dy;

    u32 radiusSum = LD8(areaA + AREA16_RADIUS)
                  + LD8(areaB + AREA16_RADIUS);
    u32 radiusSumSq = radiusSum * radiusSum;

    if ((s32)distSq >= (s32)radiusSumSq) {
        R0 = 0;
        R2 = distSq;
        RETURN_TO_CALLER();
    }

    s32 zA = (s32)LDS16(areaA + AREA16_Z);
    s32 zB = (s32)LDS16(areaB + AREA16_Z);

    if (zA < zB) {
        /* A is below B, so A is the one reaching toward B */
        s32 zAReached = zA + (s32)LD8(areaA + AREA16_ZREACHAMOUNT);
        R2 = (u32)zAReached;
        R0 = (zB <= zAReached) ? 1 : 0;
    } else {
        /* B is below (or level with) A, so B is the one reaching toward A */
        s32 zBReached = zB + (s32)LD8(areaB + AREA16_ZREACHAMOUNT);
        R2 = (u32)zA;
        R0 = (zA <= zBReached) ? 1 : 0;
    }
    RETURN_TO_CALLER();
}
