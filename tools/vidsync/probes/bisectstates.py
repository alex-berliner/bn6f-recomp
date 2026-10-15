"""bisect.py RESULT RUNDIR: replay the path's keys from the run's checkpoint state; at every ring w_G.state
of the run (the live machine's snapshots), compare its digest and RNG words with the static replay's."""
import os, sys, json, glob, re
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms
from emu import Emu

r = json.load(open(sys.argv[1]))
rundir = sys.argv[2]
p0 = r['press_gf']
wfiles = sorted((int(m[1]), f) for f in glob.glob(f'{rundir}/w_*.state') for m in [re.match(r'.*/w_(\d+)\.state', f)])
start = sys.argv[3] if len(sys.argv) > 3 else None
if start: wfiles = [(g, f) for g, f in wfiles if g >= int(start)]

startst = start or f'{rundir}/ckpt_0435000.state'
e = Emu(); e.loadf(startst)
g0, _ = e.gf()
print(f'static replay from {startst}, at {g0}')
snap = {}
for g, f in wfiles:
    x = Emu(); x.loadf(f); snap[g] = (x.digest(), x.u32(0x20013f0), x.u32(0x20013f4)); x.close()
for gg in range(g0, max(snap) + 1):
    if gg > g0: e.step(r['keys'][gg - p0])
    if gg in snap:
        d = e.digest(); P = e.u32(0x20013f0); S = e.u32(0x20013f4)
        ds, Ps, Ss = snap[gg]
        mark = 'SAME' if d == ds else 'DIFF'
        print(f'{gg} {mark} P {P:08x}{"="+f"{Ps:08x}" if P==Ps else " vs "+f"{Ps:08x}"} S {S:08x} vs {Ss:08x}')
        if mark == 'DIFF' and d['game'] == ds['game']:
            print('   static ', d)
            print('   live   ', ds)
            if P != Ps:
                # find the first frame the RNG differs, stepping the live file forward is not possible here;
                # report and stop
                pass
e.close()
