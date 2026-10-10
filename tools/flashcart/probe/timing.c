/* Cart timing probe: which WAITCNT settings does this cart read and run
 * code at correctly? Appended after a port ROM by mktiming.py.
 *
 * For each setting (rows, top to bottom; see kSettings) the test routine
 * runs from IWRAM, switches WAITCNT, then
 *   left cell:  hashes every 64 KB block of the ROM and compares with the
 *               hashes in the table at 0x08D88000 (data reads);
 *   right cell: runs exec_test, ~12 KB of straight-line Thumb code in ROM
 *               with a branch every 32 instructions, and checks its result
 *               (instruction fetches, prefetch);
 * and switches back to SuperFW's 4/2. Cells: white = running (a cell left
 * white means it hung there), green = correct, red = wrong. */
typedef unsigned short u16;
typedef unsigned int u32;
#define VRAM ((volatile u16 *)0x06000000)
#define WAITCNT (*(volatile u16 *)0x04000204)
#define TABLE ((const volatile u32 *)0x08D88000)
#define WHITE 0x7FFF
#define GREEN 0x03E0
#define RED 0x001F
#define GREY 0x4210

extern u32 exec_test(void);
extern const u32 exec_expected;

/* first access / following accesses (WS0), all with the prefetch buffer on */
static const u16 kSettings[] = {
    0x4000,   /* 4/2: SuperFW's setting */
    0x4004,   /* 3/2 */
    0x401C,   /* 8/1 */
    0x4010,   /* 4/1 */
    0x4014,   /* 3/1: the original game's */
    0x4008,   /* 2/2 */
    0x4018,   /* 2/1 */
};
#define NSET (sizeof kSettings / sizeof kSettings[0])

static void rect(int x0, int y0, int w, int h, u16 c) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            VRAM[y * 240 + x] = c;
}

__attribute__((section(".iwram"), target("arm"), noinline))
u32 test_reads(u16 wait) {
    WAITCNT = wait;
    u32 bad = 0, n = TABLE[0];
    for (u32 i = 0; i < n; i++) {
        const volatile u32 *p = (const volatile u32 *)(0x08000000 + i * 0x10000);
        u32 cnt = 0x4000;
        if (i == 0) { p += 0x80; cnt -= 0x80; }
        u32 h = 0x12345678;
        while (cnt--)
            h = ((h << 5) | (h >> 27)) ^ *p++;
        bad += h != TABLE[1 + i];
    }
    WAITCNT = 0x4000;
    return bad;
}

__attribute__((section(".iwram"), target("arm"), noinline))
u32 test_exec(u16 wait) {
    u32 (*volatile fn)(void) = exec_test;
    WAITCNT = wait;
    u32 r = fn();
    WAITCNT = 0x4000;
    return r;
}

int main(void) {
    WAITCNT = 0x4000;
    *(volatile u16 *)0x04000000 = 0x0403;          /* mode 3, BG2 */
    for (int i = 0; i < 240 * 160; i++)
        VRAM[i] = 0;
    for (unsigned s = 0; s < NSET; s++) {
        /* row marker: s + 1 small squares on the left, to count rows by */
        for (unsigned k = 0; k <= s; k++)
            rect(4 + k * 5, 8 + s * 21, 3, 3, GREY);
        rect(60, 4 + s * 21, 40, 14, GREY);
        rect(110, 4 + s * 21, 40, 14, GREY);
    }
    for (unsigned s = 0; s < NSET; s++) {
        rect(60, 4 + s * 21, 40, 14, WHITE);
        rect(60, 4 + s * 21, 40, 14, test_reads(kSettings[s]) ? RED : GREEN);
        rect(110, 4 + s * 21, 40, 14, WHITE);
        rect(110, 4 + s * 21, 40, 14, test_exec(kSettings[s]) == exec_expected ? GREEN : RED);
    }
    rect(0, 156, 240, 4, WHITE);                   /* done */
    for (;;)
        ;
}
