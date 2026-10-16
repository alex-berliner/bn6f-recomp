/*
 * bg_scroll.c — the overworld BG tile map follows the camera (bn6f asm03_0.s).
 *
 * A map's tile ids live in a decompressed tilemap with two layers (u16
 * entries, MapWidth per row, each layer found through a table at +4 / +8).
 * The BG tile-id blocks hold a 32x32 window of it, wrapping like a GBA BG
 * map. Every frame scrollBGTileMapToCamera_8030580 compares the camera's tile position with the
 * one the window was last drawn for (eMapTilesState200be70 +4/+6) and draws
 * the columns, then the rows, that scrolled into view, one entry at a time
 * through the map's put-tile-ids callback (+0x18). A jump of 5 tiles or more
 * redraws the whole window through the redraw callback (+0x20) instead.
 *
 * Every map's callback table (bn6f data/SpritePointersList.s) holds the same
 * functions: +0x18 setBGTileIdsBlocks1And2_8030628 (reads r0, r1, r3, r4,
 * r10) and +0x20 redrawBGTileMapWholeWindow_803086C (reads r0, r1, r5); those are the registers set
 * for them here.
 *
 * Written by Claude through the convert workflow (tools/wf).
 */
#include "game.h"
#include "runtime.h"
#include <stdbool.h>

enum {  /* bn6f include/structs/MapTilesState200be70.inc, not in the shared header */
    TILES_MAP_WIDTH = 0x00, TILES_MAP_HEIGHT = 0x01,
    TILES_WINDOW_X = 0x04, TILES_WINDOW_Y = 0x06,  /* Unk_04/06: the camera tile the window was drawn for */
};
enum {  /* bn6f include/structs/Camera.inc */
    CAMERA_X = 0x04, CAMERA_Y = 0x06,              /* Unk_04/06: position in pixels */
};
#define MAP_TILES_STATE A_eMapTilesState200be70  /* eMapTilesState200be70 */
#define WINDOW_TILES    32  /* the BG tile-id blocks are 32x32 */
#define REDRAW_DISTANCE 5   /* the camera jumped this many tiles: redraw everything */

/* ── the map's tile ids ──────────────────────────────────────────────── */

typedef struct { u32 first, second; } TileIds;  /* a map cell's ids in the tilemap's two layers */

static bool on_map(u32 tiles, u32 x, u32 y) {
    return (s32)x >= 0 && (s32)x < (s32)LD8(tiles + TILES_MAP_WIDTH)
        && (s32)y >= 0 && (s32)y < (s32)LD8(tiles + TILES_MAP_HEIGHT);
}

/* The tile ids of map cell (x, y); off the map, those of cell (0, 0). */
static TileIds tile_ids_at(u32 tiles, u32 x, u32 y) {
    if (!on_map(tiles, x, y))
        x = y = 0;
    u32 tilemap = LD32(tiles + oMapTilesState200be70_UnkCallback0C);
    u32 cell = x * 2 + y * (LD8(tiles + TILES_MAP_WIDTH) * 2);
    return (TileIds){
        .first = LD16(tilemap + LD32(tilemap + 4) + cell),
        .second = LD16(tilemap + LD32(tilemap + 8) + cell),
    };
}

/* getMapCellTileIds_80305E4(r0 = map x, r1 = map y, r5 = eMapTilesState200be70) -> r0, r1 = the
 * cell's tile ids. redrawBGTileIdsWindowCellByCell_8030808 (still lifted) passes the registers it leaves
 * on to the put-tile-ids callback, so they are left as the original's loop
 * leaves them. */
Next getMapCellTileIds_80305E4(void) {
    u32 tiles = R5, x = R0, y = R1;
    if (!on_map(tiles, x, y))
        x = y = 0;
    TileIds ids = tile_ids_at(tiles, x, y);
    u32 row_bytes = LD8(tiles + TILES_MAP_WIDTH) * 2;

    R0 = ids.first;
    R1 = ids.second;
    R2 = ids.first;                                   /* as the original: the last id loaded */
    R3 = LD32(tiles + oMapTilesState200be70_UnkCallback0C);  /* as the original: the tilemap */
    R4 = 0;                                           /* as the original: its loop counter, run down */
    R6 = y * row_bytes;
    R7 = row_bytes;
    SET_FLAGS_CMP(0, 0);                              /* as the original: `cmp r4, #0` ends its loop */
    RETURN_TO_CALLER();
}

/* ── drawing what scrolled into view ─────────────────────────────────── */

/* Which entries a scroll in one direction brings into view. Positions are
 * camera tiles (pixels >> 3). Along an edge, camera tile (cx, cy) shows map
 * cell (cx + map_dx + MapWidth/2, cy + map_dy + MapHeight/2) in window entry
 * ((cx + window_dx) & 31, (cy + window_dy) & 31). */
typedef struct {
    bool column;          /* a column (the camera moved sideways) or a row (up or down) */
    s32 next_strip;       /* where the next strip is when several scrolled into view */
    s32 map_dx, map_dy;
    s32 window_dx, window_dy;
} Edge;

/* (The original adds -32 for the left and top edges: the same window entries.) */
static const Edge LEFT_EDGE   = { .column = true,  .next_strip = +1, .map_dx = -15, .map_dy = -10, .window_dx = 0 };
static const Edge RIGHT_EDGE  = { .column = true,  .next_strip = -1, .map_dx = +15, .map_dy = -10, .window_dx = 30 };
static const Edge TOP_EDGE    = { .column = false, .next_strip = +1, .map_dx = -15, .map_dy = -10, .window_dy = 0 };
static const Edge BOTTOM_EDGE = { .column = false, .next_strip = -1, .map_dx = -15, .map_dy = +10, .window_dy = 20 };

static void put_tile_ids(u32 tiles, TileIds ids, u32 column, u32 row) {
    R0 = ids.first;
    R1 = ids.second;
    R3 = column;
    R4 = row;
    R5 = tiles;
    GUEST_CALL_ADDR(LD32(tiles + oMapTilesState200be70_UnkCallback_18));
}

/* The camera moved `count` tiles toward `edge` and is now at camera tile (x, y):
 * draw the strips that scrolled into view (at least one, as the original), or
 * redraw the whole window when it moved REDRAW_DISTANCE tiles or more.
 * Returns whether it redrew everything. */
static bool draw_edge(u32 tiles, u32 x, u32 y, u32 count, const Edge *edge) {
    if (edge->column)
        ST16(tiles + TILES_WINDOW_X, x);
    else
        ST16(tiles + TILES_WINDOW_Y, y);

    if ((s32)count >= REDRAW_DISTANCE) {
        R0 = x;
        R1 = y;
        R5 = tiles;
        GUEST_CALL_ADDR(LD32(tiles + oMapTilesState200be70_UnkCallback_20));
        return true;
    }
    do {
        for (u32 i = 0; i < WINDOW_TILES; i++) {
            u32 cx = edge->column ? x : x + i;
            u32 cy = edge->column ? y + i : y;
            TileIds ids = tile_ids_at(tiles, cx + edge->map_dx + (LD8(tiles + TILES_MAP_WIDTH) >> 1),
                                      cy + edge->map_dy + (LD8(tiles + TILES_MAP_HEIGHT) >> 1));
            put_tile_ids(tiles, ids, (cx + edge->window_dx) & 31, (cy + edge->window_dy) & 31);
        }
        if (edge->column)
            x += edge->next_strip;
        else
            y += edge->next_strip;
    } while ((s32)--count > 0);
    return false;
}

/* The four edges, for their own callers' sake: their only caller is
 * scrollBGTileMapToCamera_8030580 below, which calls draw_edge directly, so they promise only
 * r5 and r10 (cport/host/difftest.cfg).
 * (r0 = camera tile x, r1 = camera tile y, r2 = tiles moved, r5 = eMapTilesState200be70) */
Next drawBGTileMapLeftColumns_803066E(void) { draw_edge(R5, R0, R1, R2, &LEFT_EDGE);   RETURN_TO_CALLER(); }
Next drawBGTileMapRightColumns_80306C8(void) { draw_edge(R5, R0, R1, R2, &RIGHT_EDGE);  RETURN_TO_CALLER(); }
Next drawBGTileMapTopRows_8030722(void) { draw_edge(R5, R0, R1, R2, &TOP_EDGE);    RETURN_TO_CALLER(); }
Next drawBGTileMapBottomRows_803077C(void) { draw_edge(R5, R0, R1, R2, &BOTTOM_EDGE); RETURN_TO_CALLER(); }

/* ── every frame ─────────────────────────────────────────────────────── */

/* scrollBGTileMapToCamera_8030580(r10 = toolkit): bring the BG window up to date with the camera,
 * columns first, then rows. */
Next scrollBGTileMapToCamera_8030580(void) {
    /* the original pushes these and pops them at the end */
    const u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    const u32 saved_r8 = R8, saved_r9 = R9, saved_r12 = R12;

    u32 camera = LD32(R10 + oToolkit_CameraPtr);
    u32 x = (u32)((s32)LDS16(camera + CAMERA_X) >> 3);
    u32 y = (u32)((s32)LDS16(camera + CAMERA_Y) >> 3);
    u32 tiles = MAP_TILES_STATE;

    s32 window_x = (s32)LDS16(tiles + TILES_WINDOW_X);
    if ((s32)x < window_x)
        draw_edge(tiles, x, y, (u32)window_x - x, &LEFT_EDGE);
    else if ((s32)x > window_x)
        draw_edge(tiles, x, y, x - (u32)window_x, &RIGHT_EDGE);

    s32 window_y = (s32)LDS16(tiles + TILES_WINDOW_Y);
    bool redrew = false;
    if ((s32)y < window_y)
        redrew = draw_edge(tiles, x, y, (u32)window_y - y, &TOP_EDGE);
    else if ((s32)y > window_y)
        redrew = draw_edge(tiles, x, y, y - (u32)window_y, &BOTTOM_EDGE);

    /* flags as the original leaves them: its `cmp r1, r2` when the window did not
     * move vertically, the redraw callback's after a redraw, else the row loop's
     * last `cmp r2, #0` */
    if (y == (u32)window_y)
        SET_FLAGS_CMP(y, (u32)window_y);
    else if (!redrew)
        SET_FLAGS_CMP(0, 0);

    /* registers as the original's pops leave them (`pop {r1-r3}` brings back r8, r9, r12) */
    R1 = saved_r8;
    R2 = saved_r9;
    R3 = saved_r12;
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    R8 = saved_r8; R9 = saved_r9; R12 = saved_r12;
    RETURN_TO_CALLER();
}
