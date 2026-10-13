"""stream_search.py: which position along the primary RNG's stream fits the video?

The RNG is one stream from the boot seed (SeedRNG: 0xA338244F), so its whole state is how many calls
have been made. From a checkpoint, write the seed of each candidate position, replay the keys, and
score against the video, mostly where a random outcome showed (the tail).

  stream_search.py prep RESULT.json STATE G1 TAIL_FROM OUT.npz   bundle what a search needs
  stream_search.py run PREP.npz STATE N_FROM N_TO OUT.jsonl [WORKERS]
"""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
SEED_ADDR = 0x020013F0
BOOT_SEED = 0xA338244F

def seed_at(n):
    x = BOOT_SEED
    for _ in range(n):
        x = (((x << 1) + (x >> 31)) + 1 ^ 0x873CA9E5) & 0xFFFFFFFF
    return x

def prep(res, state, g1, tail_from, out):
    from feat import video
    R = json.load(open(res)); p0 = R['press_gf']
    g0 = json.load(open(state + '.json'))['g']
    step = max(25, (tail_from - g0) // 100)
    frames = sorted(set(list(range(g0 + step - 1, tail_from, step)) + list(range(tail_from, g1, 4))))
    T, _ = video('data/full')
    vids = [R['vid'][g - p0] for g in frames]
    np.savez_compressed(out, g0=g0, keys=np.array(R['keys'][g0 - p0:g1 - p0], np.uint16), frames=np.array(frames),
                        tail_from=tail_from, thumbs=np.stack([np.asarray(T[v]) for v in vids]))
    print('prepared', len(frames), 'frames from game frame', g0, 'to', g1, '->', out)

P = None
E = None
def init(path, state):
    """One emulator per worker: the checkpoint is loaded once, then rewound from memory per candidate."""
    global P, E
    d = np.load(path); P = {k: d[k] for k in d.files}; P['state'] = state
    from emu import Emu
    E = Emu(); E.loadf(state + '.state'); E.save(0)

def score(n_seed):
    from scipy import ndimage
    import beam as bm
    from feat import emu_thumb, MASK
    from emu import Emu
    n, seed = n_seed
    e = E; e.load(0); e.cmd(f'write32 {SEED_ADDR:x} {seed:x}')
    g0 = int(P['g0']); keys = P['keys']; want = {int(g): i for i, g in enumerate(P['frames'])}
    errs = np.zeros(len(want))
    # step runs of equal keys at once; stop on each sample frame for its picture
    stops = sorted(want)
    i = 0; nk = len(keys)
    for gs in stops + [g0 + nk]:
        last = min(gs - g0, nk - 1)          # index of the sample frame's own key
        while i < last:
            j = i
            while j < last and keys[j] == keys[i]: j += 1
            e.step(int(keys[i]), j - i); i = j
        if gs >= g0 + nk: break
        e.step(int(keys[i])); i += 1
        jj = want[gs]
        Et = ndimage.gaussian_filter(emu_thumb(e.shot()), bm.BLUR)
        V = ndimage.gaussian_filter(P['thumbs'][jj].astype(np.float32), bm.BLUR)
        edge = (np.abs(np.diff(V, axis=0))[MASK[1:]].mean() + np.abs(np.diff(V, axis=1))[MASK[:, 1:]].mean()) / 2
        x = 0.8 + 0.62 * float(edge); d = float(Et[MASK].mean() - V[MASK].mean())
        errs[jj] = min((float(np.abs(Et - V - d)[MASK].mean()) + 0.25 * abs(d)) / x, bm.CAP)
    tail = P['frames'] >= int(P['tail_from'])
    return n, f'{seed:08x}', float(errs[tail].mean()), float(errs[~tail].mean())

def run(prep_path, state, a, b, out, workers):
    from multiprocessing import Pool
    done = set()
    if os.path.exists(out):
        done = {json.loads(l)[0] for l in open(out)}
    x = seed_at(a); todo = []
    for n in range(a, b):
        if n not in done: todo.append((n, x))
        x = (((x << 1) + (x >> 31)) + 1 ^ 0x873CA9E5) & 0xFFFFFFFF
    with Pool(workers, initializer=init, initargs=(prep_path, state)) as pool, open(out, 'a') as f:
        for k, r in enumerate(pool.imap_unordered(score, todo, chunksize=8)):
            f.write(json.dumps(r) + '\n'); f.flush()
            if (k + 1) % 1000 == 0: print(k + 1, 'of', len(todo), flush=True)
    rows = sorted((json.loads(l) for l in open(out)), key=lambda r: r[2])
    print('best', rows[:5])

if __name__ == '__main__':
    if sys.argv[1] == 'prep':
        prep(sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), sys.argv[6])
    else:
        run(sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), sys.argv[6],
            int(sys.argv[7]) if len(sys.argv) > 7 else os.cpu_count() - 2)
