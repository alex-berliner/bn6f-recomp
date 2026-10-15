"""reenter.py STATE RESULTJSON G1 G2: with 0xBE4 cleared from just before the natural catch, replay the path's
keys (the video's display keys past the loss) and print MegaMan's place and his distance to the parked
tornado, plus whether he is inside the 0x14 box."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms
from emu import Emu

state, rj, g1, g2 = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
r = json.load(open(rj)); p0 = r['press_gf']
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); M4 = 0x80 >> (0xBE4 & 7)
ks = r['keys']
import beam
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
lv = r['vid'][-1]
ks = list(ks) + [b.key(lv + int(round(k * beam.RATE))) for k in range(1, g2 - (p0 + len(ks)) + 3)]
e = Emu(); e.loadf(state)
g0, _ = e.gf()
fnat = {}
# natural replay to find the catch and the parked tornado
for gg in range(g0, g0 + 130):
    if gg > g0: e.step(r['keys'][gg - p0])
    fnat[gg] = e.read(FA4, 1)[0]
hit = None
e.loadf(state)
for gg in range(g0, g0 + 130):
    if gg > g0: e.step(ks[gg - p0])
    if e.read(syms.EVENT_FLAGS + (0xBE6 >> 3), 1)[0] & (0x80 >> (0xBE6 & 7)) and hit is None:
        hit = gg
print(f'natural catch at {hit}')
# suppressed walk
e.loadf(state)
gc = hit - 2
for gg in range(g0, gc):
    e.step(ks[gg - p0])
e.poke(gc, f'{FA4:#x}={(fnat[gc] & ~M4) & ~0x02:#04x}/1')
objs = syms.OW_MAP_OBJECTS
# the parked tornado's coords from the current frame
buf = bytes(e.read(objs, syms.OW_MAP_OBJECT_SIZE * 56))
tor = None
for i in range(56):
    o = i * syms.OW_MAP_OBJECT_SIZE
    if buf[o] & 1 and buf[o + 1] == 0x1A:
        tor = (int.from_bytes(buf[o + 0xC:o + 0x10], 'little'), int.from_bytes(buf[o + 0x10:o + 0x14], 'little'))
s32 = lambda v: v - (1 << 32) if v >= 1 << 32 else v
tx, ty = s32(tor[0]) >> 16, s32(tor[1]) >> 16
print(f'tornado parked at ({tx},{ty}); walking with 0xBE4 clear from {gc}')
for gg in range(gc, g2 + 1):
    if gg > gc: e.step(ks[gg - p0])
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    mx, my = s32(e.u32(p + 0x1C)) >> 16, s32(e.u32(p + 0x20)) >> 16
    d2 = (mx - tx) ** 2 + (my - ty) ** 2
    if gg >= g1:
        print(f'{gg} mm {mx:4d},{my:4d} d {d2 ** 0.5:6.1f} {"IN BOX" if d2 <= 400 else ""}')
e.close()
