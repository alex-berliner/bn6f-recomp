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
for g in range(lo, 326170):
    s.step(key_at(s.gf()[0])); m.step(R['keys'][g - g0])
    if g + 1 >= 326100 and (g + 1) % 5 == 0:
        print(g + 1, 'run', hex(m.u32(rf.PRIMARY)), 'seg', hex(s.u32(rf.PRIMARY)), 'map', m.read(0x02001B84, 2).hex(), s.read(0x02001B84, 2).hex(), 'sub', m.read(0x02001B80,1).hex(), s.read(0x02001B80,1).hex())
