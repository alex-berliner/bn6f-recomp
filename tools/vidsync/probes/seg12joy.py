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
for g in range(lo, 325626):
    s.step(key_at(s.gf()[0])); m.step(R['keys'][g - g0])
    if g + 1 >= 325618:
        print(g + 1, hex(R['keys'][g - g0]), hex(key_at(s.gf()[0] - 1)), 'joy run', m.read(0x0200A270, 0x20).hex(), 'seg', s.read(0x0200A270, 0x20).hex(), 'gf', m.gf(), s.gf())
A = m.read(0x02000000, 0x40000); B = s.read(0x02000000, 0x40000)
print([hex(0x02000000 + k) + ':' + A[k:k+1].hex() + '/' + B[k:k+1].hex() for k in range(0x40000) if A[k] != B[k] and not (0xDF40 <= k < 0xE000)][:120])
