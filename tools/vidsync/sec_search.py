"""sec_search.py: which secondary-RNG position deals the video's hand? (The secondary RNG, eSecondaryRngSeed
0x02001120, starts at 0 at power-on, steps with the same rule as the primary, and deals the battle chips.)

  sec_search.py state RESULT.json G OUT      replay RESULT straight to game frame G and save (OUT.state/.json)
  sec_search.py run STATE REF.png D N_FROM N_TO OUT.jsonl [WORKERS]
       from STATE, write the secondary seed of each position, play D frames (the path's own keys) and compare the
       custom screen's chip panel (x<118, y<128) with REF (the video's open custom screen at 240x160)
"""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
S_ADDR = 0x02001120
def nxt(x): return ((((x << 1) + (x >> 31)) + 1) ^ 0x873CA9E5) & 0xFFFFFFFF
def seed_at(n):
    x = 0
    for _ in range(n): x = nxt(x)
    return x
def pos_of(seed, cap=10**8):
    x = 0
    for i in range(cap):
        if x == seed: return i
        x = nxt(x)
E = None; REF = None; D = None; KA = None
def init(state, ref, d):
    global E, REF, D, KA
    from emu import Emu
    from PIL import Image
    E = Emu(); E.loadf(state + '.state'); E.save(0)
    KA = json.load(open(state + '.json'))['keys_after']
    REF = np.asarray(Image.open(ref).convert('RGB')).astype(np.int16)[:128, :118]; D = d
def score(ns):
    n, s = ns
    E.load(0); E.cmd(f'write32 {S_ADDR:x} {s:x}')
    i = 0
    while i < D:
        j = i
        while j < D and KA[j] == KA[i]: j += 1
        E.step(KA[i], j - i); i = j
    img = E.shot().astype(np.int16)[:128, :118]
    return n, f'{s:08x}', float(np.abs(img - REF).mean())
if __name__ == '__main__':
    if sys.argv[1] == 'state':
        from emu import Emu
        R = json.load(open(sys.argv[2])); P0 = R['press_gf']; K = R['keys']; G = int(sys.argv[3])
        e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=P0 - 402)
        i = 0
        while P0 + i < G:
            j = i
            while P0 + j < G and K[j] == K[i]: j += 1
            e.step(K[i], j - i); i = j
        s = e.u32(S_ADDR); e.savef(sys.argv[4] + '.state')
        after = K[G - P0:G - P0 + 600]          # the path's own keys from G on: they walk into the battle
        json.dump({'g': G, 'secondary': s, 'secondary_pos': pos_of(s), 'keys_after': after}, open(sys.argv[4] + '.json', 'w'))
        print('saved; secondary', hex(s), 'position', pos_of(s))
    else:
        from multiprocessing import Pool
        state, ref, d, a, b, out = sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6]), sys.argv[7]
        w = int(sys.argv[8]) if len(sys.argv) > 8 else 14
        x = seed_at(max(a, 0)); todo = []
        for n in range(max(a, 0), b): todo.append((n, x)); x = nxt(x)
        with Pool(w, initializer=init, initargs=(state, ref, d)) as pool, open(out, 'a') as f:
            for r in pool.imap_unordered(score, todo, chunksize=32):
                f.write(json.dumps(r) + '\n')
        rows = sorted((json.loads(l) for l in open(out)), key=lambda r: r[2])
        print('best', rows[:5])
