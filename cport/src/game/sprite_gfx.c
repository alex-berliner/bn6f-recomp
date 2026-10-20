/*
 * sprite_gfx.c — per-frame sprite placement and graphics residency
 * (sprite_projectAndCull, sub_300631C, sub_300638C, sprite_ensureGfxResident, sprite_acquireObjPaletteSlot and their
 * helpers in bn6f asm38.s; IWRAM routines in the original).
 *
 * For every visible object the overworld calls, in order:
 *   sprite_projectAndCull  project the object's world position to the screen (through
 *                the current camera mode) and mark which pieces of its
 *                animation frame are on screen;
 *   sprite_ensureGfxResident  make sure the sprite's tiles are in OBJ VRAM (a cache keyed
 *                by graphics pointer; misses allocate tiles and queue a DMA
 *                upload) and that its palette has one of the 15 OBJ palette
 *                slots (shared by content);
 *   sprite_queueFramePieces  build the OAM entries (sprite_oam.c).
 *
 * All three take r5 = object and leave it unchanged; callers rely on nothing
 * else (r10 = the game's toolkit pointer, as everywhere).
 */
#include "game.h"
#include "runtime.h"

/* ── layouts ─────────────────────────────────────────────────────────── */
enum {  /* object header */
    OBJ_FLAGS = 0x00,           /* bit3: cull by position only */
    OBJ_SPRITE_OFFSET = 0x02,   /* high nibble: offset of the sprite struct / 16 */
};
enum {  /* object sprite (see sprite_oam.c) */
    SPR_FLAGS = 0x03,           /* bit1 visible, bit2 single-position, bit3 any non-first piece
                                   visible, bit4 not drawable this frame, bit5 alt Y everywhere */
    SPR_PAL_BANK = 0x04, SPR_PAL_INDEX = 0x05,
    SPR_PAL_EFFECT = 0x06,      /* u16 effect colour; bit15 selects subtract */
    SPR_TILE = 0x08, SPR_DEPTH = 0x0A, SPR_X = 0x0C, SPR_Y = 0x0E, SPR_Y_FIRST = 0x0F,
    SPR_FLIP = 0x13,            /* bit4 h-flip, bit5 v-flip */
    SPR_ATTR2_HI = 0x15,        /* high nibble: OBJ palette slot (0xF: fixed) */
    SPR_MIRROR = 0x16,          /* bits4-5 facing; bit1 keep facing when the camera mirrors */
    SPR_DATA = 0x18, SPR_HEADER = 0x1C, SPR_FRAME = 0x20,
    SPR_UPLOADED = 0x24,        /* graphics pointer currently in VRAM for this sprite */
    SPR_PIECE_MASK = 0x28,
};
enum { POS_X = 0x00, POS_Y = 0x04, POS_Z = 0x08 };   /* 16.16 fixed point */
enum { TK_CAMERA = 0x0C };
enum { CAM_X = 0x3C, CAM_Y = 0x40, CAM_Z = 0x44, CAM_X_OFFSET = 0x48 };

#define CAMERA_SCROLL    0x020099CCu   /* eCamera+0x4C: screen-space X scroll / mirror flag */
#define SPRITE_LIMITS    0x020098A8u   /* byte_20098A8: [0] palette slots, [1] camera mode*4, [2] u16 tile limit */
#define TILE_ALLOCATORS  0x030060E8u   /* off_30060E8 (IWRAM routine block): u16 next free tile */
#define TILE_CACHE_COUNT 0x020093A8u   /* dword_20093A8 */
#define TILE_CACHE       0x0200A890u   /* dword_200A890: {graphics, tile} */
#define PALETTE_COUNT    0x0200F340u   /* dword_200F340 */
#define PALETTE_CACHE    0x03001550u   /* byte_3001550: 15 x 16 colours */
#define PALETTE_SCRATCH  0x03001750u   /* iPallete3001750 */
#define OBJ_VRAM         0x06010000u
#define FIRST_ALLOC_TILE 0x30

#define U8(a)  (*(u8 *)(uintptr_t)(u32)(a))
#define S8(a)  (*(s8 *)(uintptr_t)(u32)(a))
#define U16(a) (*(u16 *)(uintptr_t)(u32)(a))
#define S16(a) (*(s16 *)(uintptr_t)(u32)(a))
#define U32(a) (*(u32 *)(uintptr_t)(u32)(a))

static inline u32 sprite_of(u32 obj) { return obj + (U8(obj + OBJ_SPRITE_OFFSET) & 0xF0); }

static inline u32 frame_pieces(u32 spr) {
    u32 data = U32(spr + SPR_DATA) + U32(U32(spr + SPR_HEADER) + 0x0C);
    return data + U32(data + U8(U32(spr + SPR_FRAME)) * 4);
}

/* ── projection ──────────────────────────────────────────────────────── */

typedef struct { s32 x, y, y_first; } ScreenPos;

/* Isometric camera (sub_300631C): screen x from x+y, screen y from (y-x)/2
 * minus height. The first piece may use a separate height (e.g. a shadow on
 * the floor below a jumping object). */
IWRAM_HELPER static ScreenPos project_iso(u32 pos, u32 spr) {
    u32 z_first;
    if (!(U8(spr + SPR_FLAGS) & 4)) {
        R0 = pos;
        GUEST_CALL(checkZCoordModifiers_8031612);   /* floor height under the object */
        z_first = R0 << 16;
    } else {
        z_first = U32(pos + POS_Z);
    }
    u32 cam = U32(R10 + TK_CAMERA);
    u32 dx = U32(pos + POS_X) - U32(cam + CAM_X);
    u32 dy = U32(pos + POS_Y) - U32(cam + CAM_Y);
    ScreenPos s;
    s.x = ((s32)(dx + dy) >> 16) + 0x78;
    U16(spr + SPR_X) = (u16)s.x;
    u32 half = (u32)((s32)(dy - dx) >> 1);
    U16(spr + SPR_DEPTH) = (u16)((s32)(U32(cam + CAM_Z) + 0x800000 + half) >> 16);
    s.y = ((s32)(half - U32(pos + POS_Z) + U32(cam + CAM_Z)) >> 16) + 0x52;
    U8(spr + SPR_Y) = (u8)s.y;
    s.y_first = ((s32)(half - z_first + U32(cam + CAM_Z)) >> 16) + 0x52;
    U8(spr + SPR_Y_FIRST) = (u8)s.y_first;
    return s;
}

/* Side-view camera (sub_300638C), optionally mirrored horizontally. */
IWRAM_HELPER static ScreenPos project_side(u32 pos, u32 spr, u32 mirror) {
    u32 cam = U32(R10 + TK_CAMERA);
    ScreenPos s;
    s32 x = S16(pos + POS_X + 2) - ((s32)(U32(cam + CAM_X) + U32(cam + CAM_X_OFFSET)) >> 16);
    u32 facing = U8(spr + SPR_MIRROR) & 0x30;
    if (mirror) {
        x = -x;
        if (!(U8(spr + SPR_MIRROR) & 2))
            facing = (U8(spr + SPR_MIRROR) ^ 0x10) & 0x30;
    }
    U8(spr + SPR_FLIP) = (u8)((U8(spr + SPR_FLIP) & ~0x30u) | facing);
    s.x = x + 0x78;
    U16(spr + SPR_X) = (u16)s.x;
    s32 ground = S16(pos + POS_Y + 2) - ((s32)U32(cam + CAM_Y) >> 16) + 0x50;
    U16(spr + SPR_DEPTH) = (u16)ground;
    s.y = ground - (S16(pos + POS_Z + 2) - ((s32)U32(cam + CAM_Z) >> 16));
    U8(spr + SPR_Y) = (u8)s.y;
    U8(spr + SPR_Y_FIRST) = (u8)ground;
    u32 flags = U8(spr + SPR_FLAGS);
    if (flags & 0x20)
        U8(spr + SPR_Y_FIRST) = (u8)s.y;
    s.y_first = flags & 4 ? s.y : ground;
    return s;
}

static inline int on_screen(s32 x, s32 y, u32 scroll) {
    return x >= (s32)scroll - 0x40 && x < (s32)scroll + 0x131 && y >= -0x20 && y < 0xC1;
}

/* sprite_projectAndCull(r0 = position, r5 = object) */
IWRAM_CODE Next sprite_projectAndCull(void) {
    u32 obj = R5, pos = R0;
    u32 obj_flags = U8(obj + OBJ_FLAGS);
    u32 mirror = U8(CAMERA_SCROLL);
    u32 spr = sprite_of(obj);
    ScreenPos s;
    switch (U8(SPRITE_LIMITS + 1)) {
    case 0: s = project_iso(pos, spr); break;
    case 4: s = project_side(pos, spr, mirror); break;
    default: rt_fatal("sprite camera mode", U8(SPRITE_LIMITS + 1));
    }
    u32 scroll = U8(CAMERA_SCROLL);

    /* visible: bit 31 = first piece, bit 30 = second, ... (or 1 when only
     * the object's position is tested) */
    u32 visible = 0;
    if (obj_flags & 8) {
        visible = on_screen(s.x, s.y, scroll);
    } else {
        s32 y = U8(spr + SPR_FLAGS) & 1 ? s.y_first : s.y;
        int hflip = (U8(spr + SPR_FLIP) & 0x10) != 0, vflip = (U8(spr + SPR_FLIP) & 0x20) != 0;
        u32 bit = 0x80000000u;
        for (u32 p = frame_pieces(spr); U8(p) != 0xFF; p += 5, bit >>= 1) {
            s32 px = hflip ? -S8(p + 1) : S8(p + 1);
            s32 py = vflip ? -S8(p + 2) : S8(p + 2);
            if (on_screen(px + s.x, py + y, scroll))
                visible |= bit;
            y = s.y;
        }
    }
    u32 flags = U8(spr + SPR_FLAGS);
    if (!(flags & 4)) {
        flags = (flags & ~0x0Au) | (visible & 0x7FFFFFFF ? 8 : 0) | (visible ? 2 : 0);
    } else {
        flags = (flags & ~0x0Au) | (visible ? 0x0A : 0);
    }
    U8(spr + SPR_FLAGS) = (u8)flags;
    U32(spr + SPR_PIECE_MASK) = visible;
    RETURN_TO_CALLER();
}

/* ── tiles ───────────────────────────────────────────────────────────── */

IWRAM_HELPER static int tile_cache_find(u32 gfx, u32 *tile) {
    u32 e = TILE_CACHE;
    for (u32 n = U32(TILE_CACHE_COUNT); n; n--, e += 8)
        if (U32(e) == gfx) {
            *tile = U32(e + 4);
            return 1;
        }
    return 0;
}

IWRAM_HELPER static void tile_cache_add(u32 gfx, u32 tile) {
    u32 n = U32(TILE_CACHE_COUNT);
    U32(TILE_CACHE_COUNT) = n + 1;
    U32(TILE_CACHE + n * 8) = gfx;
    U32(TILE_CACHE + n * 8 + 4) = tile;
}

/* ── palettes ────────────────────────────────────────────────────────── */

static inline void copy_palette(u32 dst, u32 src) {
    for (s32 i = 0x1C; i >= 0; i -= 4)
        U32(dst + i) = U32(src + i);
}

static inline int palettes_equal(u32 a, u32 b) {
    for (s32 i = 0x1C; i >= 0; i -= 4)
        if (U32(a + i) != U32(b + i))
            return 0;
    return 1;
}

/* A tinted copy of a palette in the scratch buffer (palette_effects.c);
 * rare, so it stays out of IWRAM. */
COLD_PATH static u32 tinted_palette(u32 pal, u32 effect) {
    copy_palette(PALETTE_SCRATCH, pal);
    R0 = effect; R2 = (effect >> 15) << 2; R6 = 0x20; R7 = PALETTE_SCRATCH;
    GUEST_CALL(palette_addOrSubtractColor);
    return PALETTE_SCRATCH;
}

/* OBJ palette slot for the sprite's palette (sprite_acquireObjPaletteSlot): an identical
 * palette already loaded is shared, else the next free slot is taken. With
 * every slot taken, the sprite is skipped this frame (and, as in the
 * original, the palette's address comes back instead of a slot). */
IWRAM_HELPER static u32 palette_slot(u32 spr) {
    u32 pal = U32(U32(spr + SPR_HEADER) + 4) + 4 + U32(spr + SPR_DATA)
              + (U8(spr + SPR_PAL_BANK) + U8(spr + SPR_PAL_INDEX)) * 32;
    u32 effect = U16(spr + SPR_PAL_EFFECT);
    if (effect)
        pal = tinted_palette(pal, effect);
    s32 used = (s32)U32(PALETTE_COUNT);
    if (used >= 0xF)
        return 0xF;
    s32 i = 0;
    for (u32 slot = PALETTE_CACHE; i < used; i++, slot += 0x20)
        if (palettes_equal(slot, pal))
            return (u32)i;
    if (i >= (s32)U8(SPRITE_LIMITS)) {
        U8(spr + SPR_FLAGS) |= 0x10;
        U32(spr + SPR_UPLOADED) = 0;
        return pal;
    }
    copy_palette(PALETTE_CACHE + i * 0x20, pal);
    U32(PALETTE_COUNT) = (u32)used + 1;
    return (u32)i;
}

/* sprite_ensureGfxResident(r0 = tile allocator index, r5 = object) */
IWRAM_CODE Next sprite_ensureGfxResident(void) {
    u32 obj = R5, which = R0;
    u32 spr = sprite_of(obj);
    u32 flags = U8(spr + SPR_FLAGS) & ~0x10u;
    U8(spr + SPR_FLAGS) = (u8)flags;
    if (!(flags & 2)) {
        U32(spr + SPR_UPLOADED) = 0;
        RETURN_TO_CALLER();
    }
    /* every entry of the allocator table is dword_200F350 */
    u32 alloc = U32(IWRAM_ROM(TILE_ALLOCATORS) + which * 4);
    u32 gfx = U32(U32(spr + SPR_HEADER)) + U32(spr + SPR_DATA);   /* {u32 size; tiles...} */
    u32 tile;
    if (tile_cache_find(gfx, &tile)) {
        U16(spr + SPR_TILE) = (u16)tile;
        U32(spr + SPR_UPLOADED) = gfx;
    } else {
        u32 next = U16(alloc);
        if (next != U16(spr + SPR_TILE))
            U32(spr + SPR_UPLOADED) = 0;
        u32 tiles = U32(gfx) >> 5;
        if ((s32)(next - FIRST_ALLOC_TILE + tiles) >= (s32)U16(SPRITE_LIMITS + 2)) {
            U8(spr + SPR_FLAGS) |= 0x10;            /* out of OBJ VRAM: skip this frame */
            U32(spr + SPR_UPLOADED) = 0;
            RETURN_TO_CALLER();
        }
        tile_cache_add(gfx, next);
        U16(spr + SPR_TILE) = (u16)next;
        U16(alloc) = (u16)(next + tiles);
        if (U32(spr + SPR_UPLOADED) != gfx) {
            U32(spr + SPR_UPLOADED) = gfx;
            R0 = gfx + 4;
            R1 = OBJ_VRAM + U16(spr + SPR_TILE) * 32;
            R2 = U32(gfx);
            GUEST_CALL(QueueEightWordAlignedGFXTransfer);
        }
    }
    if ((U8(spr + SPR_ATTR2_HI) & 0xF0) < 0xF0 && !(U8(spr + SPR_FLAGS) & 0x10)) {
        u32 slot = palette_slot(spr);
        U8(spr + SPR_ATTR2_HI) = (u8)((U8(spr + SPR_ATTR2_HI) & ~0xF0u) | slot << 4);
    }
    RETURN_TO_CALLER();
}
