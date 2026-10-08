/*
 * sprite_oam.c — queue an animated sprite's pieces as OAM entries
 * (sprite_queueFramePieces, sub_3006594, sprite_queueOamEntries and sprite_queueOamEntry in bn6f asm38.s;
 * IWRAM routines in the original).
 *
 * A sprite's current animation frame is a list of 5-byte pieces (tile,
 * x, y, size, shape). Each visible piece becomes an OAM attribute set:
 * position relative to the sprite (mirrored for flipped sprites, or rotated
 * and scaled through the sprite's affine parameters), shape/size, palette and
 * tile. The entries are collected in a buffer and then linked into the
 * per-layer sprite lists that sprite_buildOamShadow (oam_build.c)
 * turns into the OAM shadow at the end of the frame.
 */
#include "game.h"

/* ── layouts ─────────────────────────────────────────────────────────── */
enum {  /* object sprite */
    SPR_FLAGS = 0x03,       /* bit0 first piece uses alt Y/layer, bit1 shown, bit2 draw first piece,
                               bit4 hidden, bit5 every piece is "first" */
    SPR_TILE = 0x08,        /* u16 VRAM tile number of the sprite's graphics */
    SPR_DEPTH = 0x0A,       /* s16 list position within the layer */
    SPR_X = 0x0C,           /* s16 */
    SPR_Y = 0x0E,           /* u8, [0x0F]: Y for the first piece */
    SPR_ATTR01 = 0x10,      /* u32 base OAM attr0|attr1<<16 */
    SPR_AFFINE = 0x13,      /* bit4 h-flip, bit5 v-flip, bits1-5 affine slot */
    SPR_ATTR2 = 0x14,       /* u16 base OAM attr2 */
    SPR_MOSAIC = 0x16,      /* bit6 mosaic, bit7 force mosaic off */
    SPR_DATA = 0x18,        /* u32 animation data base */
    SPR_HEADER = 0x1C,      /* u32 -> header; [0x0C] = frame table offset */
    SPR_FRAME = 0x20,       /* u32 -> current frame number (u8) */
    SPR_PIECE_MASK = 0x28,  /* u32 visible pieces, MSB first */
    SPR_PIECE_HIDE = 0x2C,  /* u32 pieces to hide this frame */
    SPR_PALETTE = 0x30,     /* u32 palette number + 1, or pointer to per-piece palettes */
};
enum { PC_TILE = 0, PC_X = 1, PC_Y = 2, PC_SIZE = 3, PC_SHAPE = 4, PIECE_SIZE = 5, PIECE_END = 0xFF };
enum { ENT_ATTR01 = 0, ENT_ATTR2 = 4, ENT_LAYER = 6, ENT_DEPTH = 8, ENTRY_SIZE = 12 };

#define ENTRY_BUFFER   0x03002400u   /* unk_3002400 */
#define MOSAIC_GLOBAL  0x020096D0u   /* dword_20096D0: mosaic bit for every sprite */
#define AFFINE_PARAMS  0x0200A6F0u   /* word_200A6F0: 12-byte slots; [8] angle, [9] x scale, [10] y scale */
#define SIN_TABLE      0x080065E0u   /* math_sinTable (s16, 1.12 fixed point) */
#define COS_TABLE      0x08006660u   /* math_cosTable */
#define SPRITE_COUNT   0x03001950u   /* iSpritePoolUsed: used sprite pool slots */
#define SPRITE_POOL    0x03001150u   /* iSpritePool: {attr01, attr2, 0xFF, next} x 128 */
#define LAYER_HEADS    0x03002590u   /* iSpriteLayerLists: {u8 *heads, u32 count}[] */
#define POOL_SLOTS     0x80

#define U8(a)  (*(u8 *)(uintptr_t)(u32)(a))
#define S8(a)  (*(s8 *)(uintptr_t)(u32)(a))
#define U16(a) (*(u16 *)(uintptr_t)(u32)(a))
#define S16(a) (*(s16 *)(uintptr_t)(u32)(a))
#define U32(a) (*(u32 *)(uintptr_t)(u32)(a))

/* Per OBJ shape*4+size: offsets that mirror a piece (minus its width or
 * height) and that centre it (minus half), in pixels. */
static const s8 kHalfWidth[12]  = { -4, -8, -16, -32, -8, -16, -16, -32, -4, -4, -8, -16 };   /* byte_30066F4 */
static const s8 kHalfHeight[12] = { -4, -8, -16, -32, -4, -4, -8, -16, -8, -16, -16, -32 };   /* byte_3006700 */
static const s8 kWidth[12]      = { -8, -16, -32, -64, -16, -32, -32, -64, -8, -8, -16, -32 }; /* byte_300670C */
static const s8 kHeight[12]     = { -8, -16, -32, -64, -8, -8, -16, -32, -16, -32, -32, -64 }; /* byte_3006718 */

static inline u32 shape_size(u32 piece) {
    return (U8(piece + PC_SHAPE) & 0xF) * 4 + (U8(piece + PC_SIZE) & 0xF);
}

/* Affine (rotated/scaled) sprite: the piece's offset from the sprite centre
 * is rotated and scaled, and the attribute's affine slot is chosen (slot + 16
 * for the first piece). Rare, so it stays out of IWRAM. */
COLD_PATH static void affine_piece_offset(u32 spr, u32 piece, u32 *attr, u32 first, s32 *dx, s32 *dy) {
    u32 k = shape_size(piece);
    u32 slot = (U8(spr + SPR_AFFINE) & 0x3E) >> 1;
    if (first) {
        *attr += 0x20000000u;
        slot += 16;
    } else {
        *attr &= ~0x20000000u;
    }
    u32 m = AFFINE_PARAMS + slot * 12;
    u32 angle = ((U8(m + 8) + 0x40) & 0xFF) * 2;
    s32 sx = (S8(piece + PC_X) - kHalfWidth[k]) * U8(m + 9);
    s32 sy = (S8(piece + PC_Y) - kHalfHeight[k]) * U8(m + 10);
    s32 sin = S16(SIN_TABLE + angle), cos = S16(COS_TABLE + angle);
    *dx = ((s32)((u32)sx * (u32)sin + (u32)sy * (u32)cos) >> 12) + kWidth[k];
    *dy = ((s32)((u32)sx * (u32)-cos + (u32)sy * (u32)sin) >> 12) + kHeight[k];
}

/* Offset of a piece from the sprite's position (sub_3006594): regular
 * sprites mirror it if flipped. */
IWRAM_HELPER static void piece_offset(u32 spr, u32 piece, u32 *attr, u32 first, s32 *dx, s32 *dy) {
    if (*attr & 0x300) {
        affine_piece_offset(spr, piece, attr, first, dx, dy);
        return;
    }
    u32 k = shape_size(piece);
    s32 x = S8(piece + PC_X), y = S8(piece + PC_Y);
    if (U8(spr + SPR_AFFINE) & 0x10)
        x = -x + kWidth[k];
    if (U8(spr + SPR_AFFINE) & 0x20)
        y = -y + kHeight[k];
    *dx = x;
    *dy = y;
}

/* Link one entry into its layer list. Returns 0 if its list position is
 * out of range (the caller stops queueing). */
IWRAM_HELPER static int link_entry(u32 attr01, u32 attr2, u32 layer, s32 depth) {
    u32 lh = LAYER_HEADS + layer * 8;
    if (depth >= (s32)U32(lh + 4) || depth < 0)
        return 0;
    u32 head = U32(lh) + depth;
    u32 n = U32(SPRITE_COUNT);
    U32(SPRITE_COUNT) = n + 1;
    u32 next = U8(head);
    U8(head) = (u8)n;
    u32 slot = SPRITE_POOL + n * 8;
    U32(slot) = attr01;
    U16(slot + 4) = (u16)attr2;
    U8(slot + 7) = (u8)next;
    U8(slot + 6) = 0xFF;
    return 1;
}

/* Queue `count` built entries, all or (if the pool would overflow) none. */
IWRAM_HELPER static void queue_entries(u32 buf, s32 count) {
    if ((s32)(count + U32(SPRITE_COUNT)) >= POOL_SLOTS)
        return;
    do {
        if (!link_entry(U32(buf + ENT_ATTR01), U16(buf + ENT_ATTR2), U8(buf + ENT_LAYER), S16(buf + ENT_DEPTH)))
            return;
        buf += ENTRY_SIZE;
    } while (--count > 0);
}

/* sprite_queueOamEntries(r0 = entries, r1 = count). Rarely used: runs from ROM. */
Next sprite_queueOamEntries(void) {
    queue_entries(R0, (s32)R1);
    RETURN_TO_CALLER();
}

/* sprite_queueOamEntry(r0 = attr01, r1 = attr2, r2 = layer, r3 = list position):
 * queue a single OAM entry; r0 = 0xFF if it was queued. The flags are the
 * original's too: callers return them up through void functions to
 * dispatchers, which the lifter cannot see past. */
IWRAM_CODE Next sprite_queueOamEntry(void) {
    s32 n = (s32)U32(SPRITE_COUNT);
    if (n >= POOL_SLOTS) {
        SET_FLAGS_CMP(n, POOL_SLOTS);                  /* cmp r6, #0x80 */
    } else if (link_entry(R0, R1, R2, (s32)R3)) {
        R0 = 0xFF;
        SET_FLAGS_ADD(0xFF, 0);                        /* adds (no carry); movs r0, #0xFF */
    } else {
        s32 depth = (s32)R3, len = (s32)U32(LAYER_HEADS + R2 * 8 + 4);
        if (depth >= len)
            SET_FLAGS_CMP(depth, len);                 /* cmp r3, r7 */
        else
            SET_FLAGS_CMP(depth, 0);                   /* cmp r3, #0 */
    }
    RETURN_TO_CALLER();
}

/* sprite_queueFramePieces(r0 = layer, r1 = list position bias, r5 = sprite) */
IWRAM_CODE Next sprite_queueFramePieces(void) {
    u32 spr = R5, layer = R0, bias = R1;
    u32 flags = U8(spr + SPR_FLAGS);
    if ((flags & 0x10) || !(flags & 0x02))
        RETURN_TO_CALLER();

    s32 palette = (s32)U32(spr + SPR_PALETTE) - 1;
    u32 data = U32(spr + SPR_DATA) + U32(U32(spr + SPR_HEADER) + 0x0C);
    u32 piece = data + U32(data + U8(U32(spr + SPR_FRAME)) * 4);
    u32 saved_mask = U32(spr + SPR_PIECE_MASK);
    U32(spr + SPR_PIECE_MASK) = saved_mask & ~U32(spr + SPR_PIECE_HIDE);

    u32 mosaic = U8(spr + SPR_MOSAIC) & ~0x40u;
    U8(spr + SPR_MOSAIC) = (u8)((mosaic | U8(MOSAIC_GLOBAL)) & ~((mosaic & 0x80) >> 1));

    u32 out = ENTRY_BUFFER;
    s32 count = 0;
    u32 first = U8(spr + SPR_FLAGS) & 1;
    if (!first && !(U8(spr + SPR_FLAGS) & 4)) {
        U32(spr + SPR_PIECE_MASK) <<= 1;        /* first piece not drawn */
        goto next_piece;
    }
    while (U8(piece) != PIECE_END) {
        palette++;
        u32 mask = U32(spr + SPR_PIECE_MASK);
        U32(spr + SPR_PIECE_MASK) = mask << 1;
        if (mask & 0x80000000u) {
            u32 attr = U32(spr + SPR_ATTR01) | (U8(spr + SPR_MOSAIC) & 0x40) << 6;
            s32 dx, dy;
            piece_offset(spr, piece, &attr, first, &dx, &dy);
            u32 size = U8(piece + PC_SIZE);
            attr |= (u32)((S16(spr + SPR_X) + dx) & 0x1FF) << 16;     /* X */
            attr |= size << 30;                                        /* size */
            attr ^= (size & 0xF0) << 22;                               /* flips */
            attr |= (U8(spr + SPR_Y + first) + dy) & 0xFF;             /* Y */
            attr |= (U8(piece + PC_SHAPE) & 0xF) << 14;                /* shape */
            u32 attr2 = U16(spr + SPR_ATTR2);
            if (palette > 0xFF)                                        /* per-piece palettes */
                attr2 = (attr2 & ~0xF000u) | U8(palette) << 12;
            attr2 |= U8(piece + PC_TILE) + U16(spr + SPR_TILE);
            /* the first piece goes to the front of the next layer */
            u32 depth = first ? 0 : (u32)(S16(spr + SPR_DEPTH) + bias);
            U32(out + ENT_ATTR01) = attr;
            U16(out + ENT_ATTR2) = (u16)attr2;
            U8(out + ENT_LAYER) = (u8)(layer + first);
            U16(out + ENT_DEPTH) = (u16)depth;
            out += ENTRY_SIZE;
            count++;
        }
    next_piece:
        piece += PIECE_SIZE;
        if (!(U8(spr + SPR_FLAGS) & 0x20))
            first = 0;
    }
    if (count)
        queue_entries(ENTRY_BUFFER, count);
    U32(spr + SPR_PIECE_MASK) = saved_mask;
    RETURN_TO_CALLER();
}
