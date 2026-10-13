"""rng_search.py: replay a result's keys with NEW GAME pressed on each game frame of a range (keys
stay relative to the press) and score each press against the video where the result matched it:
a frame every 1000, and densely over the tail (where an RNG-dependent event showed it apart).

  rng_search.py prep RESULT.json VDIR TAIL_FROM TAIL_TO OUT.npz   the keys, frames and video thumbnails a search needs
  rng_search.py run PREP.npz FROM TO OUT.jsonl [WORKERS]           one line per press: press, tail score, body score
"""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np

def prep(res, vdir, tail_from, tail_to, out):
    from feat import video
    R = json.load(open(res)); p0 = R['press_gf']
    n = tail_to - p0
    samples = sorted(set(list(range(400, n, 1000)) + list(range(tail_from - p0, n, 4))))
    T, _ = video(vdir)
    vids = sorted({R['vid'][i] + o for i in samples for o in (0,)})
    np.savez_compressed(out, keys=np.array(R['keys'][:n + 1], np.uint16), samples=np.array(samples),
                        tail_from=tail_from - p0, svid=np.array([R['vid'][i] for i in samples]),
                        vids=np.array(vids), thumbs=np.stack([np.asarray(T[v]) for v in vids]))
    print('prepared', len(samples), 'sample frames,', len(vids), 'video thumbnails ->', out)

P = None
def init(path):
    global P
    d = np.load(path)
    P = {k: d[k] for k in d.files}

def score(press):
    from scipy import ndimage
    import beam as bm
    from feat import emu_thumb, MASK
    from emu import Emu
    keys, samples, svid = P['keys'], P['samples'], P['svid']
    vt = {int(v): t for v, t in zip(P['vids'], P['thumbs'])}
    e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=press - 402)
    errs = []
    i = 0
    for si, tgt in enumerate(samples):
        while i < tgt:
            j = i
            while j < tgt and keys[j] == keys[i]: j += 1
            e.step(int(keys[i]), j - i); i = j
        e.step(int(keys[i])); i += 1
        E = ndimage.gaussian_filter(emu_thumb(e.shot()), bm.BLUR)
        V = ndimage.gaussian_filter(vt[int(svid[si])].astype(np.float32), bm.BLUR)
        edge = (np.abs(np.diff(V, axis=0))[MASK[1:]].mean() + np.abs(np.diff(V, axis=1))[MASK[:, 1:]].mean()) / 2
        x = 0.8 + 0.62 * float(edge)
        d = float(E[MASK].mean() - V[MASK].mean())
        errs.append(min((float(np.abs(E - V - d)[MASK].mean()) + 0.25 * abs(d)) / x, bm.CAP))
    e.close()
    errs = np.array(errs); tail = samples >= int(P['tail_from'])
    return press, float(errs[tail].mean()), float(errs[~tail].mean())

def run(prep_path, a, b, out, workers):
    from multiprocessing import Pool
    done = set()
    if os.path.exists(out):
        done = {json.loads(l)[0] for l in open(out)}
    todo = [p for p in range(a, b) if p not in done]
    with Pool(workers, initializer=init, initargs=(prep_path,)) as pool, open(out, 'a') as f:
        for k, r in enumerate(pool.imap_unordered(score, todo, chunksize=2)):
            f.write(json.dumps(r) + '\n'); f.flush()
            if (k + 1) % 100 == 0: print(k + 1, 'of', len(todo), flush=True)
    rows = sorted((json.loads(l) for l in open(out)), key=lambda r: r[1])
    print('best', rows[:5])

if __name__ == '__main__':
    if sys.argv[1] == 'prep':
        prep(sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), sys.argv[6])
    else:
        run(sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), sys.argv[5], int(sys.argv[6]) if len(sys.argv) > 6 else os.cpu_count() - 2)
