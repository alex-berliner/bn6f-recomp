"""compstate.py STATE RESULTJSON D G1 G2: dump the weather comp state's first 0x50 bytes each frame (u16s),
with MegaMan's position, to find the fields that track the visible storm cloud."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms
from emu import Emu
import beam

state, rj, D = sys.argv[1], sys.argv[2], int(sys.argv[3])
G1, G2 = int(sys.argv[4]), int(sys.argv[5])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); M4 = 0x80 >> (0xBE4 & 7); M6 = 0x80 >> (0xBE6 & 7)
CS = syms.WEATHER_COMP_STATE
s32 = lambda v: v - (1 << 32) if v >= 1 << 32 else v
hi = G2 + 4
lv = r['vid'][-1]; n0 = len(r['keys'])
ks = list(zip(r['keys'], r['vid'])) + [(b.key(lv + int(round(k * beam.RATE))), lv + int(round(k * beam.RATE)))
                                       for k in range(1, hi - p0 - n0 + 3)]
e = Emu(); e.loadf(state)
g0, _ = e.gf()
hit = None
for g in range(g0, D + 9):
    if g > g0: e.step(ks[g - p0][0])
    if e.read(syms.EVENT_FLAGS + (0xBE6 >> 3), 1)[0] & M6 and hit is None: hit = g
gc = hit - 2
pre = [[gc, f'{FA4:#x}={(e.read(FA4, 1)[0] & ~M4) & ~M6:#04x}/1']]
e.loadf(state)
pend = {g: [spec] for g, spec in pre}
print('CS at', hex(CS))
for g in range(g0, hi):
    for spec in pend.pop(g, []): e.poke(g, spec)
    e.step(ks[g - p0][0])
    if g < G1: continue
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    mx, my = s32(e.u32(p + 0x1C)) >> 16, s32(e.u32(p + 0x20)) >> 16
    mx = mx - 65536 if mx > 32768 else mx; my = my - 65536 if my > 32768 else my
    buf = bytes(e.read(CS, 0x50))
    words = ' '.join(f'{i * 2:02x}:{int.from_bytes(buf[i * 2:i * 2 + 2], "little"):04x}'
                     for i in range(0x28)) 
    print(f'{g} mm {mx:4d},{my:4d} | {words}')
e.close()
