/*
 * oam_build.c — build the OAM shadow for this frame
 * (copyTo_iObjectAttr3001D70_3006814 in bn6f asm38.s, an IWRAM routine).
 *
 * Sprites are queued per priority layer as linked lists in the sprite pool;
 * each layer has an array of list heads. Layers are emitted in order, each
 * head array from its end, each list from its head, until the 128 OAM
 * entries are used up. Unused entries are hidden, the affine parameters are
 * interleaved into the OBJ attribute slots, and the per-frame queues are
 * reset for the next frame.
 */
#include "game.h"

#define OAM_SHADOW      0x03001D70u   /* iObjectAttr3001D70: 128 x {attr0..2, affine} */
#define OAM_SHADOW_END  0x03002170u
#define LAYER_HEADS     0x03002590u   /* tupleArr_3002590: {u8 *heads, u32 count}[4] */
#define LAYER_HEADS_END 0x030025B0u
#define SPRITE_POOL     0x03001150u   /* iObjectAttr3001150: {attr0, attr1, attr2, ?, next} */
#define FREE_OAM_COUNT  0x0200B1A8u   /* dword_200B1A8 */
#define AFFINE_PARAMS   0x0200A6F0u   /* word_200A6F0: {pa, pb, pc, pd, ...} x 32, stride 12 */
#define FRAME_RESETS    0x0802FD70u   /* off_802FD70: {u32 *buffer, u32 size}[4] */
#define SPRITE_COUNT    0x03001950u   /* byte_3001950 */

#define NO_SPRITE 0xFF
#define OAM_HIDDEN_ATTR01 0x000000F0u /* y = 240: off screen */
#define OAM_HIDDEN_ATTR2  0x0C00u     /* priority 3 */

IWRAM_CODE Next copyTo_iObjectAttr3001D70_3006814(void) {
    u32 ret = LR, saved_r12 = R12;
    u32 oam = OAM_SHADOW;

    for (u32 layer = LAYER_HEADS; layer < LAYER_HEADS_END && oam < OAM_SHADOW_END; layer += 8) {
        u32 heads = LD32(layer);
        for (u32 head = heads + LD32(layer + 4); head-- > heads && oam < OAM_SHADOW_END;) {
            u32 idx = LD8(head);
            while (idx != NO_SPRITE && oam < OAM_SHADOW_END) {
                u32 sprite = SPRITE_POOL + idx * 8;
                ST32(oam, LD32(sprite));             /* attr0, attr1 */
                ST16(oam + 4, LD16(sprite + 4));      /* attr2 */
                oam += 8;
                idx = LD8(sprite + 7);
            }
        }
    }
    ST32(FREE_OAM_COUNT, (OAM_SHADOW_END - oam) >> 3);
    for (; oam < OAM_SHADOW_END; oam += 8) {
        ST32(oam, OAM_HIDDEN_ATTR01);
        ST16(oam + 4, OAM_HIDDEN_ATTR2);
    }

    /* Affine matrices live in the 4th halfword of every OBJ entry:
     * matrix n uses entries 4n..4n+3. */
    u32 src = AFFINE_PARAMS, dst = OAM_SHADOW;
    for (int n = 0; n < 32; n++, src += 12, dst += 0x20) {
        ST16(dst + 0x06, LD16(src + 0));
        ST16(dst + 0x0E, LD16(src + 2));
        ST16(dst + 0x16, LD16(src + 4));
        ST16(dst + 0x1E, LD16(src + 6));
    }

    /* Reset the per-frame sprite queues (WordFill(buffer, size, 0xFFFFFFFF)). */
    for (int i = 0; i < 4; i++) {
        R0 = LD32(FRAME_RESETS + i * 8);
        R1 = LD32(FRAME_RESETS + i * 8 + 4);
        R2 = 0xFFFFFFFFu;
        GUEST_CALL(WordFill);
    }
    ST32(SPRITE_COUNT, 0);

    R0 = saved_r12;            /* registers as the original leaves them */
    R1 = 0;
    R2 = 0xFFFFFFFFu;
    R3 = 0;
    R12 = saved_r12;
    RETURN_TO(ret);
}
