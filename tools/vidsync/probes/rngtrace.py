"""rngtrace.py RESULT STATE G1 [NOPOKES]: replay the path's keys from the state; every 10th frame print the
game frame, the primary/secondary RNG words (0x20013f0/0x20013f4) and MegaMan's place; NOPOKES=1 drops the
run's queued pokes (their past effects stay in the state)."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms
from emu import Emu

r = json.load(open(sys.argv[1]))
G1 = int(sys.argv[3])
NOP = len(sys.argv) > 4 and sys.argv[4] == '1'
p0 = r['press_gf']
e = Emu(); e.loadf(sys.argv[2])
g0, _ = e.gf()
print(f'state at {g0}, press_gf {p0}, pokes {"dropped" if NOP else "queued"}')
if not NOP:
    for pg, ps in r['pokes']:
        if pg >= g0: e.poke(pg, ps)
for gg in range(g0, G1 + 1):
    if gg > g0: e.step(r['keys'][gg - p0])
    if (gg - g0) % 10: continue
    P = e.u32(0x20013f0); S = e.u32(0x20013f4)
    pp = e.u32(syms.GS_OW_PLAYER_PTR)
    mm = f"{e.u32(pp + 0x1C) >> 16},{e.u32(pp + 0x20) >> 16}" if pp else '-'
    print(f'{gg} P {P:08x} S {S:08x} mm {mm}')
e.close()
