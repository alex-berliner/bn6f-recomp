/* Cart probe: can this cart read and run code above 8 MB? Mode 3 bars, one
 * per test, top to bottom:
 *   0 reached main (always green if it runs at all)
 *   1-4 read a marker word at 4 MB, 8 MB, 12 MB, 13.5 MB
 *   5-7 call a tiny function at 8 MB, 12 MB, 13.5 MB
 * green = correct, yellow = the cart mirrors the first 8 MB there,
 * red = something else (shows the low half of the value read as grey). */
typedef unsigned short u16;
typedef unsigned int u32;
#define VRAM ((volatile u16 *)0x06000000)
#define GREEN 0x03E0
#define YELLOW 0x03FF
#define RED 0x001F

static void bar(int i, u16 c) {
    for (int y = i * 20; y < i * 20 + 17; y++)
        for (int x = 0; x < 240; x++)
            VRAM[y * 240 + x] = c;
}

static const u32 marker_off[4] = { 0x00400000, 0x00800000, 0x00C00000, 0x00D7FF00 };
static const u32 marker_val[4] = { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
static const u32 func_off[3] = { 0x00800100, 0x00C00100, 0x00D7FF80 };
static const u32 func_ret[3] = { 0x5A, 0x6B, 0x7C };

int main(void) {
    *(volatile u16 *)0x04000000 = 0x0403;          /* mode 3, BG2 */
    bar(0, GREEN);
    for (int i = 0; i < 4; i++) {
        u32 v = *(volatile u32 *)(0x08000000 + marker_off[i]);
        u32 low = *(volatile u32 *)(0x08000000 + (marker_off[i] & 0x7FFFFF));
        bar(1 + i, v == marker_val[i] ? GREEN : (v == low && marker_off[i] >= 0x800000) ? YELLOW : RED);
    }
    for (int i = 0; i < 3; i++) {
        u32 (*fn)(void) = (u32 (*)(void))(0x08000000 + func_off[i] + 1);
        bar(5 + i, fn() == func_ret[i] ? GREEN : RED);
    }
    for (;;)
        ;
}
