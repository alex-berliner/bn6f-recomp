import sys, os; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
import beam as bm
b = bm.Beam('data/start', log=lambda *a: None)
b.start(600)
while b.g < 1744: b.advance()
for _ in range(20):
    b.advance()
    br = b.branches[0]
    i = int(np.argmin(br.cost))
    vb = b.vbase(br.h - 1)
    print(b.g - 1, 'branches', len(b.branches), 'h', br.h, 'vbase', vb, 'best off', int(bm.OFFS[i]), 'node v', br.node.v,
          'target v', b.g - 1 - 313, 'cost near', np.round(br.cost[max(0, i - 3):i + 4], 2).tolist())
