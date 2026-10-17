"""killplay.py LOSS STATE D W0 END STEP OUT: from STATE the loss's keys to D, then the video's keys (the input
display, bits.npy) at the video's rate, every live enemy's HP set to 0 at W0; our screen every STEP frames."""
import sys, json, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import numpy as np
from PIL import Image
from emu import Emu
import beam as bm
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; D, W0, END, ST = map(int, sys.argv[3:7])
bits, _ = bm.load_bits('data/full')
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, D): e.step(r['keys'][gg - g0])
vD = r['vid'][D - g0 - 1]
fr = []
for gg in range(D, END):
    if gg == W0:
        act = e.u32(0x02034000); foes = []
        for n in range(16):
            a = 0x0203A9B0 + n * 0xD8
            if act >> (31 - n) & 1 and e.read(a + 0x16, 1)[0] == 1 and e.read(a + 0x24, 2) != b'\0\0':
                foes.append(a)
        print('foes', [hex(a) for a in foes])
        e.poke(W0, ','.join(f'{a + 0x24:#x}=0x0000/2' for a in foes))
    v = vD + int(round((gg - D) * bm.RATE)); e.step(0x3FF ^ int(bits[v]))
    if (gg - D) % ST == 0: fr.append(e.shot())
while len(fr) % 4: fr.append(np.zeros_like(fr[0]))
Image.fromarray(np.vstack([np.hstack(fr[i:i + 4]) for i in range(0, len(fr), 4)])).save(sys.argv[7])
