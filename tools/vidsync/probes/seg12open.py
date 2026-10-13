import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
from emu import Emu
import rngfix as rf
R = json.load(open('data/ready/bn6f_beastsp.result.json')); g0 = R['press_gf']
d = 'data/ready/segments'; base = f'{d}/beastsp_seg_12'
seg = [l.strip().split(',') for l in open(base + '.trace') if l[0].isdigit()]
spk = [l.split(' ', 1) for l in open(base + '.poke')]
# the run up to 326100
m = Emu()
for p in R['pokes']: m.poke(*p)
m.step(n=400); m.step(0x3FF & ~8, 2); m.step(n=g0 - 402)
s = Emu(save=base + '.sav')
for g, sp in spk: s.poke(int(g), sp.strip())
# segment timeline: replay its trace (key holds from line g)
tl = [(int(g), int(k, 16)) for g, k in seg]
lo = 300080
for g in range(g0, lo): m.step(R['keys'][g - g0])
# step segment until its counter reads the run's
def seg_key(f):
    k = 0x3FF
    for g, kk in tl:
        if g <= f: k = kk
        else: break
    return k
f = 0
while f < 584: s.step(seg_key(f)); f = s.gf()[0]
# now advance both
import bisect
gs = [g for g, k in tl]
def key_at(f):
    i = bisect.bisect_right(gs, f) - 1
    return tl[i][1] if i >= 0 else 0x3FF
n = 0; prev = None; hist = []
for g in range(lo, 326001):
    s.step(key_at(s.gf()[0])); m.step(R['keys'][g - g0])
    a = m.read(0x0200DF20, 0x18); c = s.read(0x0200DF20, 0x18)
    ka = (a[4], a[5], a[0xA:0x10]); kc = (c[4], c[5], c[0xA:0x10])
    hist = (hist + [(g + 1, hex(R['keys'][g - g0]), a.hex(), c.hex())])[-6:] if 'hist' in dir() else []
    if a != c and g > 312320:
        for h in hist: print(*h)
        n += 1
        if n > 3: break
