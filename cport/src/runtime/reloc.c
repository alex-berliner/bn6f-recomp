/*
 * reloc.c — position-independent routines the game copies into RAM.
 *
 * The SRAM library (ReadSram / VerifySram) copies a small core routine onto
 * the stack and calls it there. Its address depends on SP, so it cannot be a
 * static dispatch entry; instead a RAM target whose bytes match the ROM
 * original runs the lifted ROM version (the code has no PC-relative operands).
 */
#include "lifted.h"
#include "runtime.h"

typedef struct {
    u32 rom;
    u32 size;
    GuestFn fn;
} Relocatable;

static const Relocatable kRelocatable[] = {
    { 0x0814D8CCu, 0x24, copyBytes },   /* ReadSram core   (copied by sub_814D8F0) */
    { 0x0814D994u, 0x30, copy_compareToFirstMismatch },   /* VerifySram core (copied by sub_814D9C4) */
};

GuestFn reloc_lookup(u32 addr) {
    if (addr < 0x02000000u || addr >= 0x04000000u)
        return 0;
    for (unsigned i = 0; i < sizeof kRelocatable / sizeof kRelocatable[0]; i++) {
        const Relocatable *r = &kRelocatable[i];
        u32 k;
        for (k = 0; k < r->size; k += 2)
            if (LD16(addr + k) != LD16(r->rom + k))
                break;
        if (k == r->size)
            return r->fn;
    }
    return 0;
}
