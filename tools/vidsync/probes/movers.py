"""movers.py RESULT STATE G: from STATE play the result's keys to G and 60 more frames; words in EWRAM that look
like moving map coordinates (s16.16, changing most frames by small steps), grouped as X/Y pairs."""
import os, sys, json, numpy as np
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import syms   # game addresses by name from bn6f
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; G = int(sys.argv[3])
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, G): e.step(r['keys'][gg - g0])
po = e.u32(syms.GS_OW_PLAYER_PTR)
rows = []
for k in range(60):
    e.step(r['keys'][G + k - g0] if G + k - g0 < len(r['keys']) else 0x3FF)
    rows.append(np.frombuffer(e.read(0x02000000, 0x40000), np.int32).copy())
A = np.array(rows)                     # [60, words]
d = np.diff(A, axis=0)
moving = (np.abs(d) > 0) & (np.abs(d) < 0x40000)
cnt = moving.sum(0)
cand = np.where((cnt >= 30) & (np.abs(A).max(0) < 0x4000000))[0]
print('player obj', hex(po), 'pos', [hex(x & 0xffffffff) for x in A[-1][(po + 0x1C - 0x02000000) // 4:(po + 0x28 - 0x02000000) // 4]])
for w in cand:
    a = 0x02000000 + 4 * w
    if po <= a < po + 0xD8: continue
    print(hex(a), cnt[w], [hex(x & 0xffffffff) for x in A[::15, w]])
