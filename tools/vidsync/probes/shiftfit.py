"""shiftfit.py STATE RESULTJSON D G1 G2: replay the free walk (0xBE4 clear + the tornado at the far corner
from just before the natural catch); per frame find the thumbnail shift (dx,dy) that best aligns our picture
to the video's -- i.e. how the video's camera (its MegaMan) sits relative to ours."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms, numpy as np
from emu import Emu
import beam

state, rj, D = sys.argv[1], sys.argv[2], int(sys.argv[3])
G1, G2 = int(sys.argv[4]), int(sys.argv[5])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); M4 = 0x80 >> (0xBE4 & 7); M6 = 0x80 >> (0xBE6 & 7)
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
s32 = lambda v: v - (1 << 32) if v >= 1 << 32 else v
N_OBJ = 56; hi = G2 + 4
lv = r['vid'][-1]; n0 = len(r['keys'])
ks = list(zip(r['keys'], r['vid'])) + [(b.key(lv + int(round(k * beam.RATE))), lv + int(round(k * beam.RATE)))
                                       for k in range(1, hi - p0 - n0 + 3)]
e = Emu(); e.loadf(state)
g0, _ = e.gf()
slots, pos0, hit, fnat = set(), {}, None, {}
for g in range(g0, D + 9):
    if g > g0: e.step(ks[g - p0][0])
    fnat[g] = e.read(FA4, 1)[0]
    if e.read(syms.EVENT_FLAGS + (0xBE6 >> 3), 1)[0] & M6 and hit is None: hit = g
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    if p: pos0[g] = (e.u32(p + 0x1C), e.u32(p + 0x20))
    buf = bytes(e.read(objs, osz * N_OBJ))
    for i in range(N_OBJ):
        o = i * osz
        if buf[o] & 1 and buf[o + 1] == 0x1A: slots.add(i)
gc = hit - 2
cx = (min(s32(v[0]) >> 16 for v in pos0.values()) - 1024) << 16 & 0xFFFFFFFF
cy = (min(s32(v[1]) >> 16 for v in pos0.values()) - 768) << 16 & 0xFFFFFFFF
corner = ','.join(f'{objs + i * osz + o:#x}={(cx if o == 0xC else cy):#x}/4' for i in sorted(slots) for o in (0xC, 0x10))
pre = [[gc, f'{FA4:#x}={(fnat[gc] & ~M4) & ~M6:#04x}/1'], [gc, corner]]
e.loadf(state)
byf = {}
for g, spec in pre: byf.setdefault(g, []).append(spec)
pend = dict(byf)
R = 24
for g in range(g0, hi):
    for spec in pend.pop(g, []): e.poke(g, spec)
    e.step(ks[g - p0][0])
    if g < G1: continue
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    mx, my = s32(e.u32(p + 0x1C)) >> 16, s32(e.u32(p + 0x20)) >> 16
    mx = mx - 65536 if mx > 32768 else mx; my = my - 65536 if my > 32768 else my
    img = e.shot().astype(np.float32).mean(axis=2)
    th = img.reshape(img.shape[0] // 2, 2, img.shape[1] // 2, 2).mean(axis=(1, 3))
    v = ks[g - p0][1]
    V = b.T[min(max(v, 0), b.N - 1)].astype(np.float32)
    H, W = th.shape
    best = (1e9, 0, 0)
    for dy in range(-R, R + 1, 2):
        for dx in range(-R, R + 1, 2):
            ys0, ys1 = max(0, dy), min(H, H + dy)
            xs0, xs1 = max(0, dx), min(W, W + dx)
            d = np.abs(V[ys0:ys1, xs0:xs1] - th[ys0 - dy:ys1 - dy, xs0 - dx:xs1 - dx]).mean()
            if d < best[0]: best = (d, dx, dy)
    print(f'{g} v{v} mm {mx:4d},{my:4d} shift ({best[1]:+d},{best[2]:+d}) thumb px'
          f' = ({2 * best[1]:+d},{2 * best[2]:+d}) game px  vid mm ({mx - 2 * best[1]},{my - 2 * best[2]}) cost {best[0]:5.2f}')
e.close()
