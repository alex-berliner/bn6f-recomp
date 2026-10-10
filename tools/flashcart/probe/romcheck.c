/* ROM check: is the ROM on the cart identical to the file? Appended after a
 * port ROM by mkromcheck.py, which points the header at _start. Hashes every
 * 64 KB block (block 0 from 0x200: SuperFW patches the header and 0x108,
 * 0x1F8) and compares with the hashes in the table at 0x08D81000. Mode 3 grid,
 * one cell per block, 18 per row: grey = not checked yet, green = identical,
 * red = different. Then A boots the port (its _start at 0x08800000). */
typedef unsigned short u16;
typedef unsigned int u32;
#define VRAM ((volatile u16 *)0x06000000)
#define KEYS (*(volatile u16 *)0x04000130)
#define TABLE ((const volatile u32 *)0x08D81000)   /* [0] = block count, then one hash per block */
#define GREY 0x4210
#define GREEN 0x03E0
#define RED 0x001F
#define WHITE 0x7FFF

static void cell(u32 i, u16 c) {
    u32 row = 0;
    while (i >= 18) { i -= 18; row++; }
    int x0 = i * 13 + 3, y0 = row * 13 + 2;
    for (int y = y0; y < y0 + 12; y++)
        for (int x = x0; x < x0 + 12; x++)
            VRAM[y * 240 + x] = c;
}

static u32 hash(const volatile u32 *p, u32 n) {
    u32 h = 0x12345678;
    while (n--)
        h = ((h << 5) | (h >> 27)) ^ *p++;
    return h;
}

int main(void) {
    *(volatile u16 *)0x04000000 = 0x0403;          /* mode 3, BG2 */
    for (int i = 0; i < 240 * 160; i++)
        VRAM[i] = 0;
    u32 n = TABLE[0], bad = 0;
    for (u32 i = 0; i < n; i++)
        cell(i, GREY);
    for (u32 i = 0; i < n; i++) {
        u32 skip = i == 0 ? 0x200 : 0;
        u32 h = hash((const volatile u32 *)(0x08000000 + i * 0x10000 + skip), (0x10000 - skip) / 4);
        int ok = h == TABLE[1 + i];
        bad += !ok;
        cell(i, ok ? GREEN : RED);
    }
    /* bottom bar: white when every block matched, red otherwise */
    for (int y = 158; y < 160; y++)
        for (int x = 0; x < 240; x++)
            VRAM[y * 240 + x] = bad ? RED : WHITE;
    while (KEYS & 1)
        ;
    __asm__ volatile("bx %0" :: "r"(0x08800000));
    for (;;)
        ;
}
