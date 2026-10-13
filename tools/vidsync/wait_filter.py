"""wait_filter.py RESULT.json AT KEYS_SHIFT KMAX G_END OUT.jsonl [WORKERS]: particle filter over a wait of k
frames (no input) before game frame AT. RESULT's keys are used with KEYS_SHIFT frames already inserted at AT
(so its keys from AT+KEYS_SHIFT on are the original timeline). For each k: the Central Area 1 mystery data
roll (after the jack-in) and the first frame from game frame 10900 on (original timeline) where the screen
turns white (a battle transition)."""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from multiprocessing import Pool
res, AT, SH, KMAX, GEND, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
W = int(sys.argv[7]) if len(sys.argv) > 7 else 14
R = json.load(open(res)); P0 = R['press_gf']
AFTER = R['keys'][AT - P0 + SH:]          # keys from AT on, original timeline
AFTER = AFTER + [AFTER[-1]] * 400            # past the result's end: keep doing what it last did
E = None
def init():
    global E
    from emu import Emu
    E = Emu(); E.loadf('data/full/run1_path9760.state'); E.save(0)
def run(k):
    e = E; e.load(0)
    if k: e.step(0x3FF, k)
    gmd = None; white = None
    g = AT
    i = 0
    while g < GEND:
        if g < 10900:
            stop = min(10300 if gmd is None and g < 10300 else 10900, GEND)
            j = i
            while AT + j < stop and AFTER[j] == AFTER[i]: j += 1
            e.step(AFTER[i], j - i); g += j - i; i = j
            if g == 10300 and gmd is None: gmd = e.read(0x0200434C, 4).hex()
            continue
        e.step(AFTER[i]); i += 1; g += 1
        if e.shot().mean() > 200: white = g - 1; break
    return k, gmd, white
if __name__ == '__main__':
    with Pool(W, initializer=init) as pool, open(out, 'w') as f:
        for r in pool.imap_unordered(run, range(KMAX + 1), chunksize=4):
            f.write(json.dumps(r) + '\n'); f.flush()
    rows = [json.loads(l) for l in open(out)]
    ok = sorted(r for r in rows if r[1] and r[1][:4] == '0005')
    print(len(rows), 'waits;', len(ok), 'give the video\'s mystery data roll')
    hit = [r for r in ok if r[2] == 10965]
    print('of those, encounter starting at game frame 10965 like the video:', hit[:30], 'count', len(hit))
    from collections import Counter
    print('encounter frames among the mystery-data matches:', Counter(r[2] for r in ok).most_common(8))
