"""inputs.py: input-display bits per video frame (bit order = GBA KEYINPUT: A B Sel St R L U D R L)."""
import sys, os, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from classify import load, scores, ORDER
# display slot -> KEYINPUT bit
BIT = {'A': 0, 'B': 1, 's': 2, 'S': 3, 'R': 4, 'L': 5, 'U': 6, 'D': 7, 'r': 8, 'l': 9}
def bits(strip, tmpl, thresh=110, chunk=20000):
    T = load(tmpl); out = np.zeros(len(strip), np.uint16)
    for i in range(0, len(strip), chunk):
        sc = scores(np.asarray(strip[i:i + chunk]), T)
        for j, k in enumerate(ORDER):
            out[i:i + chunk] |= ((sc[:, j] > thresh).astype(np.uint16) << BIT[k])
    return out
def name(b):
    return ''.join(c for c, k in zip('ABsS><^vrl', range(10)) if b >> k & 1) or '-'
if __name__ == '__main__':
    from feat import video
    t, s = video(sys.argv[1]); b = bits(s, sys.argv[2]); np.save(f'{sys.argv[1]}/bits.npy', b)
    ch = np.nonzero(np.diff(b.astype(int)))[0] + 1
    print('changes', len(ch)); print(' '.join(f'{v}:{name(b[v])}' for v in ch[:60]))
