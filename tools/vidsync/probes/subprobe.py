"""subprobe.py STATE RESULTJSON G1 G2 [NOPOKES]: replay the path's keys from the state; every 5th frame print
the game frame, subsystem byte, primary RNG, mean brightness, MegaMan's place."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms
from emu import Emu

state, rj, g1, g2 = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
NOP = len(sys.argv) > 5 and sys.argv[5] == '1'
r = json.load(open(rj)); p0 = r['press_gf']
e = Emu(); e.loadf(state)
g0, _ = e.gf()
print(f'state at {g0}, pokes {"dropped" if NOP else "queued"}')
if not NOP:
    for pg, ps in r['pokes']:
        if pg >= g0: e.poke(pg, ps)
for gg in range(g0, g2 + 1):
    if gg > g0: e.step(r['keys'][gg - p0])
    if gg < g1 or (gg - g0) % 5: continue
    sub = e.read(syms.GS_SUBSYSTEM, 1)[0]
    P = e.u32(0x20013f0)
    pp = e.u32(syms.GS_OW_PLAYER_PTR)
    mm = f"{e.u32(pp + 0x1C) >> 16},{e.u32(pp + 0x20) >> 16}" if pp else '-'
    print(f'{gg} sub {sub:#04x} P {P:08x} mm {mm}')
e.close()
