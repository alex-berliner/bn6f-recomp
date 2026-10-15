"""cloudtrack.py STATE RESULTJSON D T0 T1: replay the free walk (0xBE4 clear + the tornado at the far corner
from just before the natural catch); each frame T0..T1 diff our thumbnail against the video's thumbnail and
print the diff blob's centroid as an offset from MegaMan (thumb px and game px) -- the video tornado's cloud
track."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms, numpy as np
from emu import Emu
import beam

state, rj, D = sys.argv[1], sys.argv[2], int(sys.argv[3])
T0, T1 = int(sys.argv[4]), int(sys.argv[5])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); M4 = 0x80 >> (0xBE4 & 7); M6 = 0x80 >> (0xBE6 & 7)
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
s32 = lambda v: v - (1 << 32) if v >= 1 << 32 else v
N_OBJ = 56
hi = D + 130
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
for g in range(g0, hi):
    for spec in pend.pop(g, []): e.poke(g, spec)
    e.step(ks[g - p0][0])
    if g < T0 - 4: continue
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    mx, my = s32(e.u32(p + 0x1C)) >> 16, s32(e.u32(p + 0x20)) >> 16
    mx = mx - 65536 if mx > 32768 else mx; my = my - 65536 if my > 32768 else my
    img = e.shot().astype(np.float32).mean(axis=2)
    h, w = img.shape
    # downscale to the thumbnail grid (GBA px / 2): 240x160 -> 120x80
    th = img.reshape(h // 2, 2, w // 2, 2).mean(axis=(1, 3))
    v = ks[g - p0][1]
    V = b.T[min(max(v, 0), b.N - 1)].astype(np.float32)
    d = np.abs(V - th)
    d = d - d.mean()
    H, W = d.shape
    cy2, cx2 = H // 2, W // 2
    wgt = np.clip(d, 0, None) ** 2
    wgt[cy2 - 5:cy2 + 6, cx2 - 5:cx2 + 6] = 0          # MegaMan himself (a frame of wobble)
    tot = wgt.sum()
    if tot < 1:
        print(f'{g} v{v} no diff'); continue
    ys, xs = np.mgrid[0:H, 0:W]
    by = (wgt * ys).sum() / tot; bx = (wgt * xs).sum() / tot
    oy, ox = by - cy2, bx - cx2
    print(f'{g} v{v} mm {mx:4d},{my:4d} cloud off thumb ({ox:+6.1f},{oy:+6.1f}) = game ({2 * ox:+7.1f},{2 * oy:+7.1f})'
          f' -> tor ({mx + 2 * ox:+7.1f},{my + 2 * oy:+7.1f}) mass {tot:9.0f}')
e.close()
