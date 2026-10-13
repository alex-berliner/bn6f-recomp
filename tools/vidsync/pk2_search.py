"""pk2_search.py: pk_search with Central Area 2's mystery data too (0x140B: placement 1, 600z, which the video
takes at video ~12470) and waits up to KMAX, the waits stepped incrementally (k ascending, the room state saved
at each k).
  pk2_search.py run RUN.json SEC_GOOD.json P_FROM P_TO OUT.jsonl [WORKERS] [KMAX]
A hit: [p, k, white frame, secondary, primary] at the encounter."""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import syms   # game addresses by name from bn6f
from pk_search import AT, BASE_P, BASE_K, S0, ENC
GMD = {syms.MYSTERY_DATA + 4: '0005', syms.MYSTERY_DATA + 0x16: '0106'}
CACHE, STATS = {}, [0, 0, 0]   # skipped, rechecked, recheck found the cache wrong
def run_p(p):
    from emu import Emu
    R = json.load(open(RUN)); K = R['keys']; P0 = R['press_gf']
    pre = K[:AT - P0]
    after = K[AT - P0 + BASE_K:]
    after = after + [after[-1]] * 400
    good = set(json.load(open(SEC)))
    ks = [k for k in range(0, KMAX + 1) if S0 + (p - BASE_P) + 2 * (k - BASE_K) in good]
    e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=p - 402)
    i = 0
    while i < len(pre):
        j = i
        while j < len(pre) and pre[j] == pre[i]: j += 1
        e.step(pre[i], j - i); i = j
    e.save(0); hits = []; kat = 0; gmd_ok = 0
    for k in ks:
        e.load(0)
        if k > kat: e.step(0x3FF, k - kat); kat = k; e.save(0)
        # the rolls take only the primary and iCurrFrame & 3 at the jack-in: skip a known failure (and
        # recheck one in 16 of them, to see the cache is right)
        key = (e.u32(syms.PRIMARY_RNG_SEED), e.u32(syms.ICURR_FRAME) & 3)
        if CACHE.get(key) is False:
            STATS[0] += 1
            if STATS[0] % 16: continue
            STATS[1] += 1
        g = AT; i = 0; ok = True
        for stop in (10300, 10900):
            while g < stop:
                j = i
                while AT + j < stop and after[j] == after[i]: j += 1
                e.step(after[i], j - i); g += j - i; i = j
            if stop == 10300:
                ok = all(e.read(a, 2).hex() == v for a, v in GMD.items())
                if CACHE.get(key) is False and ok: STATS[2] += 1
                CACHE[key] = ok
                if not ok: break
                gmd_ok += 1
        if not ok: continue
        white = None
        while g < 11040:
            e.step(after[i]); i += 1; g += 1
            if e.shot().mean() > 200: white = g - 1; break
        if white is not None and abs(white - ENC) <= 1:
            hits.append([p, k, white, e.u32(syms.SECONDARY_RNG_SEED), e.u32(syms.PRIMARY_RNG_SEED)])
    e.close()
    return p, len(ks), gmd_ok, hits, list(STATS)
if __name__ == '__main__':
    from multiprocessing import Pool
    RUN, SEC, a, b, out = sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
    w = int(sys.argv[7]) if len(sys.argv) > 7 else 14
    KMAX = int(sys.argv[8]) if len(sys.argv) > 8 else 6000
    with Pool(w, initializer=globals().update, initargs=({'RUN': RUN, 'SEC': SEC, 'KMAX': KMAX},)) as pool, \
            open(out, 'a') as f:
        for r in pool.imap_unordered(run_p, range(a, b), chunksize=1):
            f.write(json.dumps(r) + '\n'); f.flush()
    hits = [h for l in open(out) for h in json.loads(l)[3]]
    print(len(hits), 'pairs pass:', hits[:20])
