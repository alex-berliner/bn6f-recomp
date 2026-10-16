"""tornstate.py RESULT.json STATE G2 [STEP]: from STATE (a ring state on RESULT's path), step the path's keys
(past its end the video's display keys at the handlers' mapping) to game frame G2 and print every STEP frames
the Mr. Weather comp's tornadoes (map object type 0x1A): slot, state +8, mode +9 (0 moving, else paused),
direction +5, move countdown +0x24 / period +0x26, pause countdown +0x25 / length +0x27, moves since the last
pause +0x28 / per pause +7, speed +0x2C (s16.16 px per frame), X/Y px; MegaMan's place; eWeatherCompState
+6 (orbs laid), +8, +0xa/+0xb (tornadoes / left), +0x16 (strikes); flag bytes 0xBE0-0xBEF; the primary RNG."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import syms
import beam
from emu import Emu

r = json.load(open(sys.argv[1])); p0 = r['press_gf']
G2 = int(sys.argv[3]); STEP = int(sys.argv[4]) if len(sys.argv) > 4 else 1
b = beam.Beam('data/full')
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
COMP, FA4 = syms.WEATHER_COMP_STATE, syms.EVENT_FLAGS + (0xBE0 >> 3)
s16 = lambda v: (v >> 16) - (1 << 16) if v >> 16 >= 1 << 15 else v >> 16
last = p0 + len(r['keys']) - 1
import re as _re
_m = _re.search(r'loss_(\d+)\.json$', sys.argv[1])
if _m: last = min(last, int(_m[1]))      # past a loss the path's mapping is the sync losing it
vL = r['vid'][last - p0]


def key(g):
    if g < last: return r['keys'][g - p0]
    return b.key(vL + int(round((g - last) * beam.RATE)))


e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for pg, ps in r['pokes']:
    if pg >= g: e.poke(pg, ps)
while g <= G2:
    if (g % STEP) == 0:
        buf = bytes(e.read(objs, osz * 56))
        tor = []
        for i in range(56):
            o = buf[i * osz:(i + 1) * osz]
            if o[0] & 1 and o[1] == 0x1A:
                u32 = lambda k: int.from_bytes(o[k:k + 4], 'little')
                tor.append(f'[{i}: st {o[8]} md {o[9]} dir {o[5]} mv {o[0x24]}/{o[0x26]} ps {o[0x25]}/{o[0x27]} '
                           f'n {o[0x28]}/{o[7]} sp {u32(0x2C) / 65536:.2f} ({s16(u32(0xC))},{s16(u32(0x10))})]')
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        mm = (s16(e.u32(p + 0x1C)), s16(e.u32(p + 0x20))) if p else None
        c = bytes(e.read(COMP, 0x18))
        print(f'{g}: key {key(g):03x} mm {mm} orbs {c[6] | c[7] << 8} c8 {c[8]} tor {c[0xa]}/{c[0xb]} '
              f'strikes {c[0x16]} f {bytes(e.read(FA4, 2)).hex()} P {e.u32(syms.PRIMARY_RNG_SEED):08x} ' + ' '.join(tor),
              flush=True)
    e.step(key(g)); g += 1
