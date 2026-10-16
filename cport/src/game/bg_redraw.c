/*
 * bg_redraw.c — full redraw of the 32x32 BG tile-id window from the map's
 * decompressed tilemap (bn6f asm03_0.s, 0x08030808-0x08030902).
 *
 * The BG tile-id blocks hold a 32x32 window on the map (bg_scroll.c keeps
 * it up to date as the camera scrolls). Two full-redraw paths live here:
 *
 *   - redrawBGTileIdsWindowCellByCell_8030808 redraws the window cell by cell: it moves the window
 *     origin (eMapTilesState200be70 +4/+6) to the camera's tile, then for
 *     each window cell asks getMapCellTileIds_80305E4 for the map cell's
 *     two tile ids and hands them to the map's put-tile-ids callback
 *     (+0x18).
 *
 *   - redrawBGTileMapWholeWindow_803086C is the maps' redraw callback (+0x20 in every callback
 *     table): it redraws the same window straight from the tilemap, one
 *     layer at a time through copyTilemapLayerRowsToWindow_8030892 — each on-map row is copied by
 *     bg_writeTileIdRow_80018E0, each off-map row is filled with tile 0 by
 *     call_sub_3005EBA. Only the row (map y) is bounds-checked.
 *
 * lr is never set here on purpose: each function ends right after its last
 * callee call, so the leftover lr is that callee's own, exactly as the
 * original leaves it.
 *
 * Written by an outside model through the convert workflow (tools/wf).
 */
#include "game.h"
#include "runtime.h"

enum {  /* bn6f include/structs/MapTilesState200be70.inc, not in the shared header */
    TILES_MAP_WIDTH = 0x00, TILES_MAP_HEIGHT = 0x01,
    TILES_WINDOW_X = 0x04, TILES_WINDOW_Y = 0x06,  /* Unk_04/06: the camera tile the window was drawn for */
};
enum {  /* bn6f include/structs/Camera.inc */
    CAMERA_X = 0x04, CAMERA_Y = 0x06,              /* Unk_04/06: position in pixels */
};
enum {  /* the decompressed tilemap's layer table (as in bg_scroll.c) */
    TILEMAP_LAYER_1 = 0x04, TILEMAP_LAYER_2 = 0x08,
};

#define WINDOW_TILES 32  /* the BG tile-id blocks are 32x32 */

/* ── cell-by-cell redraw ─────────────────────────────────────────────── */

/* What the cell-by-cell redraw carries across its calls: the map's tile
 * state in r5 and the window cell being drawn in r8/r9. The callees are
 * made to leave them alone; the loop reads them back to be exact. */
typedef struct { u32 tiles, cell_x, cell_y; } WindowCell;

/* Ask getMapCellTileIds_80305E4 for the tile ids of map cell (map_x, map_y);
 * they come back in r0/r1, ready for put_tile_ids. */
static WindowCell get_map_cell_tile_ids(u32 tiles, u32 cell_x, u32 cell_y,
                                        u32 map_x, u32 map_y) {
    R0 = map_x;
    R1 = map_y;
    R5 = tiles;
    R8 = cell_x;
    R9 = cell_y;
    GUEST_CALL(getMapCellTileIds_80305E4);
    return (WindowCell){ .tiles = R5, .cell_x = R8, .cell_y = R9 };
}

/* Hand the tile ids waiting in r0/r1 to the map's put-tile-ids callback,
 * for window entry (cell_x & 31, cell_y & 31). r5 is re-written with the
 * value it already holds, so the helper stands on its own. */
static WindowCell put_tile_ids(u32 callback, u32 tiles, u32 cell_x, u32 cell_y) {
    R3 = cell_x & 31;  /* the window entry */
    R4 = cell_y & 31;
    R5 = tiles;
    R6 = callback;
    GUEST_CALL_ADDR(callback);
    return (WindowCell){ .tiles = R5, .cell_x = R8, .cell_y = R9 };
}

/* Redraw the whole window through the map's put-tile-ids callback, moving
 * the window origin to the camera's tile. Returns the carried state as the
 * loops run out: cell_x is the row-start x popped one last time, cell_y is
 * 32 rows past the origin. */
static WindowCell redraw_window(u32 camera, u32 tiles) {
    u32 window_x = (u32)((s32)LDS16(camera + CAMERA_X) >> 3);
    u32 window_y = (u32)((s32)LDS16(camera + CAMERA_Y) >> 3);
    ST16(tiles + TILES_WINDOW_X, window_x);
    ST16(tiles + TILES_WINDOW_Y, window_y);

    u32 cell_x = window_x, cell_y = window_y;  /* r3/r4 as the loop carries them */
    for (u32 row = 0; row < WINDOW_TILES; row++) {
        u32 row_start_x = cell_x;  /* pushed at row start, popped at row end */
        for (u32 col = 0; col < WINDOW_TILES; col++) {
            /* the map cell drawn at window cell (cell_x, cell_y) */
            u32 map_x = cell_x - 15 + (LD8(tiles + TILES_MAP_WIDTH) >> 1);
            u32 map_y = cell_y - 10 + (LD8(tiles + TILES_MAP_HEIGHT) >> 1);
            WindowCell cell = get_map_cell_tile_ids(tiles, cell_x, cell_y, map_x, map_y);
            cell = put_tile_ids(LD32(cell.tiles + oMapTilesState200be70_UnkCallback_18),
                                cell.tiles, cell.cell_x, cell.cell_y);
            tiles = cell.tiles;
            cell_x = cell.cell_x;
            cell_y = cell.cell_y;
            cell_x++;  /* next cell of the row */
        }
        cell_x = row_start_x;
        cell_y++;
    }
    return (WindowCell){ .tiles = tiles, .cell_x = cell_x, .cell_y = cell_y };
}

/* redrawBGTileIdsWindowCellByCell_8030808(r5 = eMapTilesState200be70, r10 = toolkit): full window redraw,
 * cell by cell. r10-r12 are never touched. */
Next redrawBGTileIdsWindowCellByCell_8030808(void) {
    WindowCell end = redraw_window(LD32(R10 + oToolkit_CameraPtr), R5);
    /* registers as the original's loops run out: both counters reach 32,
     * r3 is the row-start value popped one last time, r4 has counted 32
     * rows; r2/r5/r6/r7 and lr stay the last callback's leftovers */
    R0 = WINDOW_TILES;
    R1 = WINDOW_TILES;
    R3 = end.cell_x;
    R4 = end.cell_y;
    SET_FLAGS_CMP(WINDOW_TILES, WINDOW_TILES);
    RETURN_TO_CALLER();
}

/* ── rows straight from the tilemap ──────────────────────────────────── */

/* r6-r9 as the row callees read them: the layer being copied, its tilemap
 * data, and the window origin. The original sets them once before its row
 * loop and relies on the callees preserving them. */
static void set_row_callee_context(u32 layer, u32 layer_data, u32 window_x, u32 window_y) {
    R6 = layer;
    R7 = layer_data;
    R8 = window_x;
    R9 = window_y;
}

/* Fill one window row (32 entries) with tile 0 through call_sub_3005EBA:
 * the row's map cells lie off the map, so there is nothing to copy. */
static void fill_window_row(u32 window_row, u32 layer) {
    R0 = 0;               /* start at window entry 0 */
    R1 = window_row;
    R2 = layer;
    R3 = 0;               /* fill with tile 0 */
    R4 = WINDOW_TILES;
    R5 = 1;
    GUEST_CALL(call_sub_3005EBA);
}

/* Copy 32 tilemap entries starting at row_start into the window row at
 * (window_x & 31, window_row) through bg_writeTileIdRow_80018E0; row is the redraw's row
 * counter, which the original passes on in r5. */
static void copy_tilemap_row(u32 window_x, u32 window_row, u32 layer,
                             u32 row_start, u32 row) {
    R0 = window_x & 31;
    R1 = window_row;
    R2 = layer;
    R3 = row_start;
    R4 = WINDOW_TILES;
    R5 = row;
    GUEST_CALL(bg_writeTileIdRow_80018E0);
}

/* Copy one tilemap layer's rows into the window: the 32 rows the window
 * shows when its origin is camera tile (window_x, window_y). On-map rows
 * go to bg_writeTileIdRow_80018E0 as a tilemap row pointer, off-map rows are filled with
 * tile 0 through call_sub_3005EBA. */
static void copy_layer_rows(u32 layer_data, u32 layer, u32 map_width, u32 map_height,
                            u32 window_x, u32 window_y) {
    u32 half_width = map_width >> 1, half_height = map_height >> 1;

    set_row_callee_context(layer, layer_data, window_x, window_y);
    for (u32 row = 0; row < WINDOW_TILES; row++) {
        s32 map_y = (s32)(window_y + row) - 10 + (s32)half_height;
        u32 window_row = (window_y + row) & 31;
        if (map_y < 0 || map_y >= (s32)map_height) {
            fill_window_row(window_row, layer);
        } else {
            s32 map_x = (s32)window_x - 15 + (s32)half_width;
            u32 row_start = layer_data + ((u32)map_x << 1) + (map_width << 1) * (u32)map_y;
            copy_tilemap_row(window_x, window_row, layer, row_start, row);
        }
    }
}

/* copyTilemapLayerRowsToWindow_8030892(r0 = layer data, r1 = layer, r2 = map width, r3 = map height,
 * r8 = window x, r9 = window y): copy the layer's rows into the window.
 * r4-r7 are restored; r0-r3 and lr are the last row call's leftovers. */
Next copyTilemapLayerRowsToWindow_8030892(void) {
    const u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    copy_layer_rows(R0, R1, R2, R3, R8, R9);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    SET_FLAGS_CMP(WINDOW_TILES, WINDOW_TILES);  /* the loop's last `cmp r5, #0x20` */
    RETURN_TO_CALLER();
}

/* Redraw both tilemap layers into the window whose origin is camera tile
 * (window_x, window_y). */
static void redraw_from_tilemap(u32 tiles, u32 window_x, u32 window_y) {
    u32 tilemap = LD32(tiles + oMapTilesState200be70_UnkCallback0C);
    u32 map_width = LD8(tiles + TILES_MAP_WIDTH);
    u32 map_height = LD8(tiles + TILES_MAP_HEIGHT);
    copy_layer_rows(tilemap + LD32(tilemap + TILEMAP_LAYER_1), 1, map_width, map_height,
                    window_x, window_y);
    copy_layer_rows(tilemap + LD32(tilemap + TILEMAP_LAYER_2), 2, map_width, map_height,
                    window_x, window_y);
}

/* redrawBGTileMapWholeWindow_803086C(r0 = window x, r1 = window y, r5 = eMapTilesState200be70): the
 * maps' redraw callback (+0x20). r4-r7 are restored, r8/r9 keep the window
 * origin, r0-r3 and lr are the last row call's leftovers. */
Next redrawBGTileMapWholeWindow_803086C(void) {
    const u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    R8 = R0;  /* the original moves the window origin into r8/r9, where the row helpers keep it */
    R9 = R1;
    redraw_from_tilemap(R5, R0, R1);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    SET_FLAGS_CMP(WINDOW_TILES, WINDOW_TILES);  /* the second copy's last `cmp r5, #0x20` */
    RETURN_TO_CALLER();
}
