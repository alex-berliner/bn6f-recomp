/*
 * difftest_gen.c — structured inputs for functions whose arguments are
 * whole data structures (random bytes would only exercise the error paths).
 */
#include <string.h>
#include "cpu.h"

typedef void (*GenFn)(Cpu *in, u32 (*rnd)(void));

#define U8(a)  (*(u8 *)(uintptr_t)(u32)(a))
#define U32(a) (*(u32 *)(uintptr_t)(u32)(a))

/* m4a mixer: a SoundInfo with random but valid channels and waves, entered
 * the way SoundMain enters it (frame on the guest stack). */
static void gen_mixer(Cpu *in, u32 (*rnd)(void)) {
    u32 info = 0x02010000u + (rnd() % 0x100) * 4;   /* EWRAM, word aligned */
    static const u32 samples_tab[] = { 96, 112, 128, 160, 176, 192, 224, 304 };
    u32 samples = samples_tab[rnd() % 8];
    u8 max_chans = (u8)(1 + rnd() % 12);
    U8(info + 0x05) = rnd() % 3 ? 0 : (u8)rnd();     /* reverb */
    U8(info + 0x06) = max_chans;
    U8(info + 0x07) = (u8)(rnd() % 3 ? rnd() % 16 : rnd());
    U32(info + 0x18) = rnd() % 0x40;                  /* divFreq */
    u32 dma = 1 + rnd() % 4;
    U8(info + 0x04) = (u8)dma;
    for (u32 c = 0; c < max_chans; c++) {
        u32 ch = info + 0x50 + c * 0x40;
        static const u8 status_tab[] = { 0x80, 0xC0, 0x83, 0x82, 0x81, 0x03, 0x02, 0x01,
                                         0x43, 0x42, 0x07, 0x13, 0x12, 0x00, 0x04, 0x44 };
        U8(ch + 0x00) = status_tab[rnd() % 16] | (rnd() % 4 ? 0 : 0x10);
        U8(ch + 0x01) = rnd() % 2 ? 0x08 : 0x00;     /* fixed / resampled */
        u32 wav = 0x02020000u + (rnd() % 0x400) * 4;
        u32 size = 1 + rnd() % 0x300;
        U32(wav + 0x08) = rnd() % size;               /* loop start */
        U32(wav + 0x0C) = size;
        U8(wav + 0x03) = rnd() % 2 ? 0x40 : 0;        /* looped */
        U32(ch + 0x24) = wav;
        U32(ch + 0x18) = 1 + rnd() % size;            /* count */
        U32(ch + 0x28) = wav + 0x10 + (size - U32(ch + 0x18));
        U32(ch + 0x1C) = rnd() & 0x7FFFFF;            /* fw */
        U32(ch + 0x20) = rnd() % 0x80000;             /* frequency */
        U8(ch + 0x0D) = (u8)(rnd() % 4);              /* echo length */
    }
    u32 sp = in->r[13];
    U32(sp + 0x14) = rnd() % 4 ? 0 : 160 + rnd() % 228;   /* scanline budget */
    U32(sp + 0x18) = info;
    u32 buf = info + 0x350 + (rnd() % 2) * samples;
    U32(sp + 0x08) = buf;
    in->r[0] = info;
    in->r[4] = dma;
    in->r[5] = buf;
    in->r[6] = 0x630;
    in->r[8] = samples;
}

/* OAM builder: four layers of sprite-list heads and a sprite pool whose
 * `next` links end in 0xFF (or loop, which fills OAM). */
static void gen_oam(Cpu *in, u32 (*rnd)(void)) {
    (void)in;
    for (u32 l = 0; l < 4; l++) {
        u32 heads = 0x02030000u + l * 0x100 + (rnd() % 0x40);
        u32 count = rnd() % 48;
        U32(0x03002590u + l * 8) = heads;
        U32(0x03002590u + l * 8 + 4) = count;
        for (u32 k = 0; k < count; k++)
            U8(heads + k) = rnd() % 3 ? (u8)(rnd() % 128) : 0xFF;
    }
    for (u32 i = 0; i < 128; i++)
        U8(0x03001150u + i * 8 + 7) = rnd() % 4 ? 0xFF : (u8)(rnd() % 128);
}

/* ── snapshot mutators: vary a captured real state (difftest -S -m N) ─ */

#define U16(a) (*(u16 *)(uintptr_t)(u32)(a))

/* sprite_queueFramePieces: the sprite's drawing options (flips, affine, first-piece
 * modes, per-piece palettes, mosaic) on real animation data. */
static void mut_sprite_pieces(Cpu *in, u32 (*rnd)(void)) {
    u32 spr = in->r[5];
    U8(spr + 0x03) = (u8)((U8(spr + 0x03) & ~0x25u) | (rnd() & 0x25));
    U8(spr + 0x13) = (u8)rnd();
    U32(spr + 0x10) ^= (rnd() & 3) << 8;                 /* affine / double size */
    U8(spr + 0x16) = (u8)((U8(spr + 0x16) & 0x3F) | (rnd() & 0xC0));
    U8(0x020096D0u) = rnd() % 2 ? 0x40 : 0;
    switch (rnd() % 4) {                                 /* palette: number or table */
    case 0: U32(spr + 0x30) = 0x08000000u + rnd() % 0x400000; break;
    case 1: U32(spr + 0x30) = rnd() % 0x110; break;
    default: break;
    }
    if (rnd() % 2) U32(spr + 0x28) = rnd();
    if (rnd() % 2) U32(spr + 0x2C) = rnd() % 4 ? 0 : rnd();
    U16(spr + 0x0A) = (u16)(rnd() % 4 ? rnd() % 64 : rnd());
    U16(spr + 0x0C) = (u16)rnd();
    in->r[1] = rnd() % 4 ? rnd() % 0x80 : rnd();
    for (u32 k = 0; k < 32 * 12; k++)                    /* affine slots */
        if (rnd() % 8 == 0) U8(0x0200A6F0u + k) = (u8)rnd();
}

static u32 sprite_of(u32 obj) { return obj + (U8(obj + 2) & 0xF0); }

/* sprite_projectAndCull: camera mode and mirroring, sprite flip/facing and flag
 * options, positions around the camera. */
static void mut_sprite_project(Cpu *in, u32 (*rnd)(void)) {
    u32 obj = in->r[5], spr = sprite_of(obj), pos = in->r[0];
    U8(obj) ^= rnd() % 4 ? 0 : 8;
    U8(spr + 0x03) = (u8)((U8(spr + 0x03) & ~0x25u) | (rnd() & 0x25));
    U8(spr + 0x13) ^= (u8)(rnd() & 0x30);
    U8(spr + 0x16) = (u8)((U8(spr + 0x16) & ~0x32u) | (rnd() & 0x32));
    U8(0x020099CCu) = (u8)(rnd() % 3 ? rnd() % 2 : rnd());
    U8(0x020098A9u) = rnd() % 2 ? 0 : 4;
    for (int k = 0; k < 3; k++)
        if (rnd() % 2) U32(pos + 4 * k) += (rnd() % 0x2000000) - 0x1000000;
}

/* sprite_ensureGfxResident: tile cache hit/miss, VRAM and palette slots running out,
 * palette effects, already-uploaded graphics. */
static void mut_sprite_gfx(Cpu *in, u32 (*rnd)(void)) {
    u32 obj = in->r[5], spr = sprite_of(obj);
    U8(spr + 0x03) ^= (u8)(rnd() & 0x12);
    if (rnd() % 2) U32(0x020093A8u) = rnd() % (U32(0x020093A8u) + 1);   /* forget cached tiles */
    if (rnd() % 4 == 0) U16(0x0200F350u) = (u16)(rnd() % 0x400);         /* tile cursor */
    if (rnd() % 4 == 0) U16(0x020098AAu) = (u16)(rnd() % 0x400);         /* tile limit */
    if (rnd() % 3 == 0) U32(0x0200F340u) = rnd() % 17;                   /* palettes used */
    if (rnd() % 4 == 0) U8(0x020098A8u) = (u8)(rnd() % 17);              /* palette slots */
    if (rnd() % 3 == 0) U16(spr + 0x06) = (u16)rnd();                    /* palette effect */
    if (rnd() % 4 == 0) U8(spr + 0x15) |= 0xF0;
    if (rnd() % 3 == 0) U32(spr + 0x24) = rnd() % 2 ? 0 : U32(spr + 0x24) + 4;
    if (rnd() % 4 == 0)                                                  /* scramble a cached palette */
        U32(0x03001550u + (rnd() % 15) * 0x20 + (rnd() % 8) * 4) ^= 1u << (rnd() % 32);
}

/* npc_runSecondaryScriptMaybe_809ebf8: captured NPCs rarely have a running
 * secondary script, so give the NPC a short one: set-palette (0x0F) and jump
 * (0x02) commands, ended by pause-secondary-script (0x33), which stops it. */
static void mut_npc_secondary(Cpu *in, u32 (*rnd)(void)) {
    u32 npc = in->r[5], p = 0x0203FF00u;            /* end of EWRAM */
    U8(npc + 0x1F) = 0;                              /* TerminateScript_1f */
    U32(npc + 0x5C) = p;                             /* UnkNPCScriptPtr_5c */
    for (int k = rnd() % 4; k > 0; k--) {
        if (rnd() % 2) {
            U8(p) = 0x0F; U8(p + 1) = (u8)(rnd() % 16); p += 2;
        } else {
            u32 to = p + 5;                          /* the next command; unaligned operand */
            U8(p) = 0x02;
            for (int b = 0; b < 4; b++) U8(p + 1 + b) = (u8)(to >> (8 * b));
            p = to;
        }
    }
    U8(p) = 0x33; U8(p + 1) = (u8)(rnd() % 8);
}

/* menu_moveCursor_811FA36 is the shared body of menu_moveCursorUpDown_811FA22 and
 * menu_moveCursorLeftRight_811FA0C, entered with their frame on the stack: both store r3 (the
 * current index) at [sp,#8] just before, and the body reads it back from there. */
static void gen_cursor_body(Cpu *in, u32 (*rnd)(void)) {
    (void)rnd;
    U32(in->r[13] + 8) = in->r[3];
}

/* dispatchStateHandler_81179AE jumps through a four-entry table on the state byte of the work area
 * sub_8047D84 finds (0x02019A00 + (h[0x2011C24] + h[0x2011C28]) * 8); a random byte indexes past the
 * table into whatever follows it. Give it a work area in EWRAM and a state the game has (0-3). */
static void gen_state_handler_81179AE(Cpu *in, u32 (*rnd)(void)) {
    (void)in;
    U16(0x02011C24u) = (u16)(rnd() % 0x100);
    U16(0x02011C28u) = (u16)(rnd() % 0x100);
    u32 work = 0x02019A00u + (U16(0x02011C24u) + U16(0x02011C28u)) * 8;
    U8(work) = (u8)(rnd() % 4);
}

/* Functions that jump through a table on a selector byte at [rB + OFF] (tools/abi/jump_tables.py writes
 * the list): the game only stores selectors the table has; a random byte indexes past it, where the
 * original jumps through whatever word follows (now and then code, so it returns: 0x6C sends
 * submenu_runModeHandler_811BD64 to sub_811BDEC) and readable C that switches on the table's values does
 * nothing. Give rB a work area in EWRAM and the selector a value the table has. */
typedef struct { const char *name; u8 base, off, entries; } JumpTableInput;
static const JumpTableInput kJumpTables[] = {
#define JT(n, b, o, e) { #n, b, o, e },
#include "jump_tables.h"
#undef JT
};
static const JumpTableInput *g_jump_table;
static void gen_jump_table(Cpu *in, u32 (*rnd)(void)) {
    u32 area = 0x02010000u + (rnd() % 0x1000) * 4;
    in->r[g_jump_table->base] = area;
    U8(area + g_jump_table->off) = (u8)((rnd() % g_jump_table->entries) * 4);
}

/* scriptListRun_8001158 walks {command, argument} pairs to a zero command, queueing each through
 * sub_802FE28 (sprite_queueOamEntry). Random memory has no zero for kilobytes: the walk runs into the
 * guest stack, where the original stops on a zero it pushed itself and the port, which pushes no
 * frame, runs off the end of IWRAM. Give it a short list, as the game's are. */
static void gen_script_list_8001158(Cpu *in, u32 (*rnd)(void)) {
    u32 list = 0x02030000u + (rnd() % 0x1000) * 4;
    int n = (int)(rnd() % 5);
    for (int k = 0; k < n; k++) {
        U32(list + 8 * k) = rnd() | 1;               /* a command (never 0) */
        U32(list + 8 * k + 4) = rnd();
    }
    U32(list + 8 * n) = 0;
    in->r[0] = list;
}

/* dispatchBattleObjectBehavior_802BF4C: a selector its table has (as kJumpTables gives the others), and
 * for its handler sub_802C044 row numbers the game has: that handler reads a halfword of its 0x74-byte
 * stack buffer at 4 + 2 * unk_2035320[[r5+0xB]], and a random byte reads past the buffer into its own
 * pushed return address, which is the original's return point in the dispatcher on one side and the
 * caller's on the port's (GUEST_CALL keeps LR). */
static void gen_dispatch_behavior_802BF4C(Cpu *in, u32 (*rnd)(void)) {
    u32 area = 0x02010000u + (rnd() % 0x1000) * 4;
    in->r[5] = area;
    U8(area + 3) = (u8)((rnd() % 3) * 4);
    for (u32 i = 0; i < 0x2A; i++)
        U8(0x02035320u + i) = (u8)(rnd() % 0x38);
}

/* sprite_repackAndQueueTiles_811F5DC copies r3 tiles' rows from r0 to r1 and queues the copy for VRAM
 * (r2): a random count runs the copy up through IWRAM into the original's own frame, where its loop
 * counters live and change, so it stops, while the port's counters are C locals. Real callers pass a
 * few tiles and buffers in EWRAM. */
static void gen_repack_tiles_811F5DC(Cpu *in, u32 (*rnd)(void)) {
    in->r[0] = 0x02020000u + (rnd() % 0x400) * 4;
    in->r[1] = 0x02030000u + (rnd() % 0x400) * 4;
    in->r[2] = 0x06010000u + (rnd() % 0x200) * 0x20;
    in->r[3] = 1 + rnd() % 8;
}

/* loadEntryPairTable_812B1DC walks 4-byte records from r0 up to one whose first byte is 0xFF (real
 * calls pass a short ROM table, byte_812913C). Random memory can hold no such record for thousands of
 * bytes: from r0 = 0x03006734 the walk ran through the stack, where it read the original's pushed
 * registers on one side and the port's frames (which are not the original's) on the other. Give it a
 * table in EWRAM: a first record, one to eight more, and the end mark. */
static void gen_entry_pair_table_812B1DC(Cpu *in, u32 (*rnd)(void)) {
    u32 table = 0x02030000u + (rnd() % 0x400) * 4;
    u32 records = 2 + rnd() % 8;
    for (u32 i = 0; i < records; i++) {
        U8(table + 4 * i) = (u8)(rnd() % 0xFF);
        U8(table + 4 * i + 1) = (u8)rnd();
        U16(table + 4 * i + 2) = (u16)rnd();
    }
    U8(table + 4 * records) = 0xFF;
    in->r[0] = table;
}

/* CutsceneCmd_ow_player_sprite_special_with_arg jumps through word [script + 1] of its six-entry
 * table off_8037B9C: as with kJumpTables, a random selector byte indexes past it (0xA0 once jumped
 * through code that returned, which the readable C does not call). Give the script command a selector
 * the table has, a value code (0xFF: the value is the next byte; else a variable at r5 + code), and r5
 * a variable area in EWRAM. */
static void gen_ow_player_sprite_special_8037B6C(Cpu *in, u32 (*rnd)(void)) {
    u32 script = 0x02030000u + rnd() % 0x1000;
    U8(script + 1) = (u8)((rnd() % 6) * 4);
    U8(script + 2) = (rnd() & 1) ? 0xFF : (u8)rnd();
    in->r[7] = script;
    in->r[5] = 0x02020000u + (rnd() % 0x400) * 4;
}

GenFn difftest_snap_mutator(const char *name) {
    if (!strcmp(name, "npc_runSecondaryScriptMaybe_809ebf8")) return mut_npc_secondary;
    if (!strcmp(name, "sprite_queueFramePieces")) return mut_sprite_pieces;
    if (!strcmp(name, "sprite_projectAndCull")) return mut_sprite_project;
    if (!strcmp(name, "sprite_ensureGfxResident")) return mut_sprite_gfx;
    return 0;
}

GenFn difftest_generator(const char *name) {
    if (!strcmp(name, "sprite_buildOamShadow")) return gen_oam;
    if (!strcmp(name, "SoundMainRAM_iwram_03005700")) return gen_mixer;
    if (!strcmp(name, "menu_moveCursor_811FA36")) return gen_cursor_body;
    if (!strcmp(name, "dispatchStateHandler_81179AE")) return gen_state_handler_81179AE;
    if (!strcmp(name, "scriptListRun_8001158")) return gen_script_list_8001158;
    if (!strcmp(name, "dispatchBattleObjectBehavior_802BF4C")) return gen_dispatch_behavior_802BF4C;
    if (!strcmp(name, "sprite_repackAndQueueTiles_811F5DC")) return gen_repack_tiles_811F5DC;
    if (!strcmp(name, "loadEntryPairTable_812B1DC")) return gen_entry_pair_table_812B1DC;
    if (!strcmp(name, "CutsceneCmd_ow_player_sprite_special_with_arg")) return gen_ow_player_sprite_special_8037B6C;
    for (size_t i = 0; i < sizeof kJumpTables / sizeof kJumpTables[0]; i++)
        if (!strcmp(name, kJumpTables[i].name)) {
            g_jump_table = &kJumpTables[i];
            return gen_jump_table;
        }
    return 0;
}
