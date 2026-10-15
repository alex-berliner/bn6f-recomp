import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import numpy as np
import beam
b = beam.Beam('data/full')
print('N =', b.N)
for v in (443950, 443975, 443990, 444000, 444005, 444010, 444020, 444040, 444060, 444079, 444090, 444100):
    if v < b.N:
        T = b.T[v].astype(np.float32)
        print(f'v{v}: mean {T.mean():6.1f} std {T.std():6.1f} min {T.min():5.1f} max {T.max():5.1f}')
    else:
        print(f'v{v}: PAST END (N={b.N})')
