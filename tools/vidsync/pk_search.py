"""pk_search.py: pairs (p = game frame NEW GAME is pressed, k = frames waited in Lan's room before the second
jack-in's R press) for which both RNGs match the video at the first random battle:
  - the secondary at the battle is one that deals the video's hand (a list from sec_search); it only counts
    frames: 1 a frame on the NEW GAME menu, 2 a frame in the overworld, so for the path base (p=600, k=1826)
    it is S0 + (p - 600) + 2 (k - 1826);
  - the primary (not linear in p) gives the video's Central Area 1 mystery data roll and starts the encounter
    on the video's frame: simulated.
  pk_search.py run RUN.json SEC_GOOD.json P_FROM P_TO OUT.jsonl [WORKERS]
"""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
AT, BASE_P, BASE_K, S0, ENC = 9760, 600, 1826, 23931, 10964
def run_p(p):
    from emu import Emu
    R = json.load(open(RUN)); K = R['keys']; P0 = R['press_gf']
    pre = K[:AT - P0]                         # NEW GAME .. the R press (relative to the press)
    after = K[AT - P0 + BASE_K:]              # from the R press on (original timeline)
    after = after + [after[-1]] * 400
    good = set(json.load(open(SEC)))
    ks = [k for k in range(0, 3001) if S0 + (p - BASE_P) + 2 * (k - BASE_K) in good]
    e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=p - 402)
    i = 0
    while i < len(pre):
        j = i
        while j < len(pre) and pre[j] == pre[i]: j += 1
        e.step(pre[i], j - i); i = j
    e.save(0); hits = []
    for k in ks:
        e.load(0)
        if k: e.step(0x3FF, k)
        g = AT; i = 0; ok = True
        for stop in (10300, 10900):
            while g < stop:
                j = i
                while AT + j < stop and after[j] == after[i]: j += 1
                e.step(after[i], j - i); g += j - i; i = j
            if stop == 10300 and e.read(0x0200434C, 2).hex() != '0005': ok = False; break
        if not ok: continue
        white = None
        while g < 11040:
            e.step(after[i]); i += 1; g += 1
            if e.shot().mean() > 200: white = g - 1; break
        if white is not None and abs(white - ENC) <= 1:
            hits.append([p, k, white, e.u32(0x02001120)])
    e.close()
    return p, len(ks), hits
if __name__ == '__main__':
    from multiprocessing import Pool
    RUN, SEC, a, b, out = sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
    w = int(sys.argv[7]) if len(sys.argv) > 7 else 14
    with Pool(w) as pool, open(out, 'a') as f:
        for r in pool.imap_unordered(run_p, range(a, b)):
            f.write(json.dumps(r) + '\n'); f.flush()
    hits = [h for l in open(out) for h in json.loads(l)[2]]
    print(len(hits), 'pairs pass both RNG checks:', hits[:20])
