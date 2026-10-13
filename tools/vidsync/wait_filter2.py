"""wait_filter2.py: particle filter over a wait of k frames (no input) before the second jack-in's R press.

  prep RESULT.json OUT.npz      keys (original timeline from the R press), video thumbnails and display
                                bits around the first random battle (video frames 10811..11031)
  run PREP.npz STATE KLIST OUT.jsonl [WORKERS]   KLIST: "a-b" or a file of k values

Per k: the Central Area 1 mystery data roll (want 0x1402 at spot 0 with 200 Zenny), the game frame
(original timeline) where the battle transition starts (the video's: ~10964-10965), and for those that
match, how well the battle's first ~200 frames (virus layout, chip draw) match the video, with keys read
from the video's input display.
"""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import syms   # game addresses by name from bn6f
import numpy as np
AT = 9760              # the R press
ENC_G, ENC_V = 10965, 10821   # the video's battle transition: game frame (original timeline), video frame
V_FROM, V_TO = ENC_V - 10, ENC_V + 210

def prep(res, out):
    from feat import video
    R = json.load(open(res)); P0 = R['press_gf']
    after = R['keys'][AT - P0 + 4:]
    after = after + [after[-1]] * 400
    T, _ = video('data/full'); B = np.load('data/full/bits.npy')
    np.savez_compressed(out, after=np.array(after, np.uint16), thumbs=np.stack([np.asarray(T[v]) for v in range(V_FROM, V_TO)]),
                        bits=B[V_FROM:V_TO])
    print('prepared ->', out)

P = None; E = None
def init(path, state):
    global P, E
    d = np.load(path); P = {k: d[k] for k in d.files}
    from emu import Emu
    E = Emu(); E.loadf(state + '.state'); E.save(0)

def vimg(v):
    from scipy import ndimage
    import beam as bm
    from feat import MASK
    V = ndimage.gaussian_filter(P['thumbs'][v - V_FROM].astype(np.float32), bm.BLUR)
    edge = (np.abs(np.diff(V, axis=0))[MASK[1:]].mean() + np.abs(np.diff(V, axis=1))[MASK[:, 1:]].mean()) / 2
    return V, 0.8 + 0.62 * float(edge)

def err(Et, v):
    import beam as bm
    from feat import MASK
    V, x = vimg(v); d = float(Et[MASK].mean() - V[MASK].mean())
    return min((float(np.abs(Et - V - d)[MASK].mean()) + 0.25 * abs(d)) / x, bm.CAP)

def run_k(k):
    from scipy import ndimage
    import beam as bm
    from feat import emu_thumb
    e = E; e.load(0)
    if k: e.step(0x3FF, k)
    after = P['after']; g = AT; i = 0; gmd = None; white = None
    while g < 10900:
        stop = 10300 if gmd is None else 10900
        j = i
        while AT + j < stop and after[j] == after[i]: j += 1
        e.step(int(after[i]), j - i); g += j - i; i = j
        if g == 10300 and gmd is None:
            gmd = e.read(syms.MYSTERY_DATA + 4, 4).hex()
            if gmd[:4] != '0005': return k, gmd, None, None
    while g < 11040:
        e.step(int(after[i])); i += 1; g += 1
        if e.shot().mean() > 200: white = g - 1; break
    if white is None or abs(white - ENC_G) > 2:
        return k, gmd, white, None
    # into the battle with keys from the video's display, scored against it
    errs = []
    for d in range(1, 200):
        gg = white + d
        v = ENC_V + (gg - ENC_G)
        if v >= V_TO - 4: break
        key = 0x3FF & ~int(P['bits'][v - V_FROM])
        e.step(key)
        if d >= 30 and d % 3 == 0:
            Et = ndimage.gaussian_filter(emu_thumb(e.shot()), bm.BLUR)
            errs.append(min(err(Et, v + o) for o in range(-3, 4) if V_FROM <= v + o < V_TO))
    return k, gmd, white, float(np.mean(errs))

def run(prep_path, state, klist, out, workers):
    from multiprocessing import Pool
    if '-' in klist and not os.path.exists(klist):
        a, b = map(int, klist.split('-')); ks = list(range(a, b))
    else:
        ks = [int(x) for x in open(klist).read().split()]
    with Pool(workers, initializer=init, initargs=(prep_path, state)) as pool, open(out, 'a') as f:
        for r in pool.imap_unordered(run_k, ks, chunksize=4):
            f.write(json.dumps(r) + '\n'); f.flush()
    rows = [json.loads(l) for l in open(out)]
    b = sorted((r for r in rows if r[3] is not None), key=lambda r: r[3])
    print(len(rows), 'waits;', len(b), 'reached the battle; best', b[:10])

if __name__ == '__main__':
    if sys.argv[1] == 'prep': prep(sys.argv[2], sys.argv[3])
    else: run(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5], int(sys.argv[6]) if len(sys.argv) > 6 else 14)
