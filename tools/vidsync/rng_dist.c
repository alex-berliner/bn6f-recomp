/* rng_dist A B [MAX]: steps from seed A to seed B along the primary RNG (GetRNG), or -1 within MAX */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
int main(int argc, char **argv) {
    uint32_t a = strtoul(argv[1], 0, 16), b = strtoul(argv[2], 0, 16);
    unsigned long max = argc > 3 ? strtoul(argv[3], 0, 0) : 100000000UL;
    for (unsigned long i = 0; i <= max; i++) {
        if (a == b) { printf("%lu\n", i); return 0; }
        a = (((a << 1) + (a >> 31)) + 1) ^ 0x873CA9E5u;
    }
    printf("-1\n");
    return 0;
}
