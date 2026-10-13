import sys, os; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from feat import video, emu_thumb, MASK
from emu import Emu
T, _ = video('data/start'); B = np.load('data/start/bits.npy')
V_A = int(np.nonzero(B & 1)[0][0]); A_end = V_A + int(np.argmax((B[V_A:] & 1) == 0))
vb = T[:, MASK].mean(1)
print('video: A held', V_A, '..', A_end - 1)
print('video brightness after press:', ' '.join(f'{V_A+i}:{vb[V_A+i]:.0f}' for i in range(0, 760, 20)))
e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); g, h0 = e.step(n=600 - 402)
held = A_end - V_A; rows = []
for i in range(held): g, h = e.step(0x3FF & ~1); rows.append((g, h, emu_thumb(e.shot())[MASK].mean()))
for i in range(800): g, h = e.step(); rows.append((g, h, emu_thumb(e.shot())[MASK].mean()))
print('emu (game, hw-h0, brightness):', ' '.join(f'{g}/{h-h0}:{b:.0f}' for g, h, b in rows[::20]))
