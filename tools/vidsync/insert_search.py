"""insert_search.py RESULT.json CKPT AT KMAX SCORE_FROM SCORE_TO [WORKERS]: from checkpoint CKPT (a beam
checkpoint before AT), replay RESULT's keys, stand still (no keys) for k frames just before game frame AT,
then carry on with the same keys; score each k against the video over SCORE_FROM..SCORE_TO (game frames of
the original timeline). A random roll (an area's mystery data, an encounter) that came out differently can
come out the video's way after a few frames' wait."""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from multiprocessing import Pool
res, ckpt, AT, KMAX, SF, ST = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6])
WORKERS = int(sys.argv[7]) if len(sys.argv) > 7 else 14
R = json.load(open(res)); P0 = R['press_gf']; KEYS = R['keys']; VID = R['vid']
C = json.load(open(ckpt + '.json')); G0 = C['g']

def score(k):
    os.environ['VIDSYNC_THUMBS'] = 'data/full/thumb.u8'
    from emu import Emu
    e = Emu(); e.loadf(ckpt + '.state')
    errs = []
    g = G0
    while g < ST:
        if g == AT:
            e.step(0x3FF, k)
        e.step(KEYS[g - P0])
        if g >= SF and (g - SF) % 2 == 0:
            errs.append(e.errs(VID[g - P0], 1)[0])
        g += 1
    e.close()
    errs = np.array(errs)
    return k, float(errs.mean()), float(np.percentile(errs, 90))

if __name__ == '__main__':
    with Pool(WORKERS) as pool:
        rows = sorted(pool.map(score, range(0, KMAX + 1)), key=lambda r: r[1])
    for r in rows[:8]: print('wait', r[0], 'frames: mean err', round(r[1], 2), 'p90', round(r[2], 2))
    print('no wait:', [r for r in rows if r[0] == 0])
