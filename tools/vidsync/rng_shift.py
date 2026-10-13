"""rng_shift.py: does pressing NEW GAME d frames later only move the RNG d steps along its sequence?"""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import syms   # game addresses by name from bn6f
from emu import Emu
SEED = syms.PRIMARY_RNG_SEED
def nxt(x):
    return (((x << 1) | (x >> 31)) + 1 ^ 0x873CA9E5) & 0xFFFFFFFF
r = json.load(open('data/full/run1.json')); keys = r['keys'][:9400]
seeds = {}
for press in (600, 601, 602):
    e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=press - 402)
    out = []
    for i, k in enumerate(keys):
        e.step(k)
        if i in (100, 2000, 5000, 9399): out.append(e.u32(SEED))
    seeds[press] = out; e.close()
for i, s0 in enumerate(seeds[600]):
    print('checkpoint', i, 'press 600', hex(s0), 'press 601', hex(seeds[601][i]), 'next(600)', hex(nxt(s0)),
          'press 602', hex(seeds[602][i]), 'next(next(600))', hex(nxt(nxt(s0))))
