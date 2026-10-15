"""vidblobs.py VDIR RESULTJSON STATE G1 G2: per game frame, dark blobs (>=10 thumb px) in the video's and in
our free-walk picture's thumbnails, with centroid offsets from the picture centre."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms, numpy as np
from scipy import ndimage
from emu import Emu
import beam

vdir, rj, state = sys.argv[1], sys.argv[2], sys.argv[3]
g1, g2 = int(sys.argv[4]), int(sys.argv[5])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(vdir)
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); M4 = 0x80 >> (0xBE4 & 7); M6 = 0x80 >> (0xBE6 & 7)
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
s32 = lambda v: v - (1 << 32) if v >= 1 << 32 else v
N_OBJ = 56
hi = g2 + 4
lv = r['vid'][-1]; n0 = len(r['keys'])
ks = list(zip(r['keys'], r['vid'])) + [(b.key(lv + int(round(k * beam.RATE))), lv + int(round(k * beam.RATE)))
                                       for k in range(1, hi - p0 - n0 + 3)]
e = Emu(); e.loadf(state)
g0, _ = e.gf()
slots, pos0, hit, fnat = set(), {}, None, {}
for g in range(g0, g0 + 130):
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

def blobs(T):
    H, W = T.shape
    lab, n = ndimage.label(T < 105)
    out = []
    for k in range(1, n + 1):
        ys, xs = np.nonzero(lab == k)
        if len(ys) < 10: continue
        out.append((len(ys), round(xs.mean() - W / 2, 1), round(ys.mean() - H / 2, 1)))
    out.sort(reverse=True)
    return out[:3]

for g in range(g0, hi):
    for spec in pend.pop(g, []): e.poke(g, spec)
    e.step(ks[g - p0][0])
    if g < g1: continue
    img = e.shot().astype(np.float32).mean(axis=2)
    th = img.reshape(img.shape[0] // 2, 2, img.shape[1] // 2, 2).mean(axis=(1, 3))
    v = ks[g - p0][1]
    V = b.T[min(max(v, 0), b.N - 1)].astype(np.float32)
    print(f'{g} v{v} OUR {blobs(th)}  VID {blobs(V)}')
e.close()
