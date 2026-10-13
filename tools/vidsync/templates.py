"""templates.py STRIP.u8 OUT.npz: glyph masks for every input-display slot, from frames where the slot
is clearly lit (white letter with a dark outline), sampled across the whole strip file."""
import sys, numpy as np
SLOTS = {'U': 339, 'D': 350, 'L': 363, 'R': 376, 'S': 389, 's': 402, 'B': 413, 'A': 426, 'l': 439, 'r': 452}
Y0, Y1 = 91 - 84, 111 - 84
strip = np.memmap(sys.argv[1], np.uint8, 'r').reshape(-1, 36, 150, 3)
want = 400
samples = {k: [] for k in SLOTS}
step = max(1, len(strip) // 4000) if len(strip) > 400000 else 1
for i0 in range(0, len(strip), 20000):
    a = np.asarray(strip[i0:i0 + 20000:max(1, step // 50 or 1)]).astype(np.int16)
    mn = a.min(3); mx = a.max(3)
    for k, x0 in SLOTS.items():
        if len(samples[k]) >= want: continue
        x = x0 - 330
        W = (mn[:, 93 - 84:109 - 84, x + 2:x + 11] > 190).mean((1, 2)); D = (mx[:, 93 - 84:109 - 84, x:x + 13] < 50).mean((1, 2))
        b = (mx[:, 108 - 84, x + 2:x + 11] < 60).mean(1)
        sel = np.nonzero((W > 0.12) & (W < 0.7) & (D > 0.15) & (b > 0.6))[0]
        for j in sel[:want - len(samples[k])]:
            samples[k].append((mn[j, Y0:Y1, x - 2:x + 15], mx[j, Y0:Y1, x - 2:x + 15]))
    if all(len(v) >= want for v in samples.values()): break
out = {}
for k, x0 in SLOTS.items():
    sm = samples[k]
    if not sm:
        print(k, 'NO SAMPLES'); continue
    med = np.median([m for m, _ in sm], 0); medx = np.median([m for _, m in sm], 0)
    out[k + '_w'] = med > 200; out[k + '_d'] = medx < 45; out[k + '_x'] = np.array(x0)
    print(k, x0, len(sm), 'white', int(out[k + '_w'].sum()), 'dark', int(out[k + '_d'].sum()))
    for r in range(Y1 - Y0):
        print('   ', ''.join('#' if out[k + '_w'][r, c] else (' ' if out[k + '_d'][r, c] else '.') for c in range(17)))
np.savez(sys.argv[2], **out)
