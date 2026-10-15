"""stagedump.py RESULT.json STATE D HIT [G1]: replay the path's keys to the loss D and the video's display
keys past it, with every live tornado map object pinned away (the far corner) from D-120 to HIT-1 (step 16)
and, when HIT > 0, the tornado written onto MegaMan's free-walk place every frame HIT..HIT+16 (the staged
capture); HIT = 0 pins away only (no hit at all), HIT < 0 leaves the tornado alone (the natural loss).
Per game frame from D-20 to G1 (default D+130): the video frame, the frame's sync cost (min over the
offset window, as the beam scores it), the video thumbnail's and the emulator shot's mean brightness over
the unmasked area, flags 0xBE4 (the comp's capture armed) and 0xBE6 (tornado hit), eWeatherCompState +0x02
(reset step) and +0x16 (strike counter), MegaMan's X/Y (px) and every live tornado's X/Y (px)."""
import os, sys, json
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import syms
from emu import Emu
from feat import video, emu_thumb, MASK

r = json.load(open(sys.argv[1]))
D = int(sys.argv[3]); HIT = int(sys.argv[4]); G1 = int(sys.argv[5]) if len(sys.argv) > 5 else D + 130
SHIFT = int(os.environ.get('SHIFT', '0'))   # replay the keys shifted by this many frames (a state/key off-by-one check)
RATE = 60 / 59.7275
p0 = r['press_gf']
ks = list(zip(r['keys'], r['vid']))                # the path's own keys to the loss
lv = ks[-1][1]
B = np.load('data/full/bits.npy')                  # the input display, as beam.key reads it
key = lambda v: 0x3FF & ~int(B[min(max(v, 0), len(B) - 1)])
full = list(ks) + [(key(lv + int(round(k * RATE))), lv + int(round(k * RATE))) for k in range(1, G1 - D + 2)]
T, _ = video('data/full')

objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
W = D - 120
s32 = lambda v: v - (1 << 32) if v >= 1 << 31 else v
STATE = sys.argv[2]

def replay(pokes, end):
    """Step the game from the state to end with pokes in; MegaMan's place each frame."""
    e = Emu(); e.loadf(STATE)
    g0, _ = e.gf()
    for pg, ps in list(r['pokes']) + pokes:
        if pg >= g0: e.poke(pg, ps)
    mm = {}
    for gg in range(g0, end + 1):
        if gg > g0: e.step(full[gg - p0 + SHIFT][0])
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        if p: mm[gg] = (e.u32(p + 0x1C), e.u32(p + 0x20))
    e.close()
    return mm

mm_nat = replay([], D + 8)                         # the natural walk to just past the loss: the corner
xs = [s32(v[0]) >> 16 for v in mm_nat.values()]; ys = [s32(v[1]) >> 16 for v in mm_nat.values()]
corner = ((min(xs) - 1024) << 16) & 0xFFFFFFFF, ((min(ys) - 768) << 16) & 0xFFFFFFFF

# the tornado slots at W
e = Emu(); e.loadf(STATE)
g0, _ = e.gf()
for gg in range(g0, W): e.step(full[gg - p0 + SHIFT][0])
slots = [i for i in range(56)
         if (buf := bytes(e.read(objs + i * osz, 2)))[0] & 1 and buf[1] == 0x1A]
e.close()
away = []
if HIT >= 0:
    spec = ','.join(f'{objs + i * osz + o:#x}={corner[0] if o == 0xC else corner[1]:#x}/4'
                    for i in slots for o in (0xC, 0x10))
    away = [[g, spec] for g in range(W, HIT or D + 120, 16)]
mm = replay(away, min(G1, D + 150))                # the free walk (the tornado pinned away): the hit writes
pokes = list(away)
if HIT > 0:
    hspec = lambda g: ','.join(f'{objs + i * osz + o:#x}={mm[g][0] if o == 0xC else mm[g][1]:#x}/4'
                               for i in slots for o in (0xC, 0x10))
    pokes += [[g, hspec(g)] for g in range(HIT, min(HIT + 17, D + 150) + 1) if g in mm]

# the dump replay with the pokes in
e = Emu(); e.loadf(STATE)
g0, _ = e.gf()
print(f'state game frame {g0}, path press_gf {p0}, path len {len(ks)}, tornado slots {slots}')
for pg, ps in list(r['pokes']) + pokes:
    if pg >= g0: e.poke(pg, ps)
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); FA6 = syms.EVENT_FLAGS + (0xBE6 >> 3)
M4, M6 = 0x80 >> (0xBE4 & 7), 0x80 >> (0xBE6 & 7)
for gg in range(g0, min(G1, D + 150) + 1):
    if gg > g0: e.step(full[gg - p0 + SHIFT][0])
    if gg < D - 20: continue
    v = full[gg - p0][1]
    cost = min(e.errs(v - 3, 7))
    et = emu_thumb(e.shot())
    vt = T[min(max(v, 0), len(T) - 1)]
    f4 = 1 if e.read(FA4, 1)[0] & M4 else 0
    f6 = 1 if e.read(FA6, 1)[0] & M6 else 0
    comp = bytes(e.read(syms.WEATHER_COMP_STATE, 0x18))
    pp = e.u32(syms.GS_OW_PLAYER_PTR)
    mx, my = (s32(e.u32(pp + 0x1C)) >> 16, s32(e.u32(pp + 0x20)) >> 16) if pp else (0, 0)
    torx = ' '.join(str(s32(int.from_bytes(bytes(e.read(objs + i * osz + 0xC, 4)), 'little')) >> 16) for i in slots)
    tory = ' '.join(str(s32(int.from_bytes(bytes(e.read(objs + i * osz + 0x10, 4)), 'little')) >> 16) for i in slots)
    vm, em = vt[MASK].mean(), et[MASK].mean()
    print(f'{gg} v{v} cost {cost:5.2f} vid {vm:6.1f} emu {em:6.1f} 0xBE4={f4} 0xBE6={f6} '
          f'step={comp[2]:3d} strikes={comp[0x16]:3d} mm {mx:4d},{my:4d} torX {torx} torY {tory}')