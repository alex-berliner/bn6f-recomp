"""battle_check.py: (p, k) pairs whose primary passes the mystery-data/encounter table (pt_table.py), checked
directly on the first battle: replay to the encounter, then the reference run's keys after its white frame, and
compare the screen with the reference's at white + CHECKS (the reference matches the video there).
  battle_check.py RUN.json TABLE.jsonl PRESS.jsonl REF P_FROM P_TO OUT.jsonl [WORKERS] [KMAX]
REF: a make_pk checkpoint and the beam result r_ beside it (REF.json/.state, r_*.json).
A row per p: [p, [[k, white, [screen diff at each check]], ...]] for the table's pairs."""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from pk_search import AT, BASE_P, BASE_K, ENC
from pt_table import to_r_press
CHECKS = (156, 192, 250, 300, 350, 400, 450, 500)
def ref_screens():
    from emu import Emu
    c = json.load(open(REF + '.json')); r = json.load(open(os.path.join(os.path.dirname(REF), 'r_' + os.path.basename(REF)[2:] + '.json')))
    post = r['keys'][len(c['keys']):]
    e = Emu(); e.loadf(REF + '.state'); white = None; shots = {}
    for i, k in enumerate(post):
        e.step(k)
        if white is None:
            if e.shot().mean() > 200: white = i
            continue
        if i - white in CHECKS: shots[i - white] = e.shot().astype(np.int16)
        if i - white >= max(CHECKS): break
    e.close()
    return post[white + 1:], shots
def init(run, tab, prs, ref, kmax):
    global PRE, AFTER, T, PRESS, REF, KMAX, POST, SHOTS
    REF, KMAX = ref, kmax
    R = json.load(open(run)); K = R['keys']; P0 = R['press_gf']
    PRE = K[:AT - P0]; AFTER = K[AT - P0 + BASE_K:]; AFTER += [AFTER[-1]] * 400
    T = set()
    for l in open(tab):
        x, f, b, white, pw = json.loads(l)
        if white is not None and abs(white - ENC) <= 1: T.add((x, f))
    PRESS = {}
    for l in open(prs):
        p, c, s, rate, g = json.loads(l)
        if c is not None and rate == 2.0: PRESS[p] = c
    POST, SHOTS = ref_screens()
def run_p(p):
    from emu import Emu
    if p not in PRESS: return p, []
    c = PRESS[p]
    ks = [k for k in range(KMAX + 1) if (c + k, (p - BASE_P + k - BASE_K) & 3) in T]
    if not ks: return p, []
    e = Emu(); to_r_press(e, PRE, p); e.save(0); kat = 0; out = []
    for k in ks:
        e.load(0)
        if k > kat: e.step(0x3FF, k - kat); kat = k; e.save(0)
        g, i = AT, 0
        while g < 10900:
            j = i
            while AT + j < 10900 and AFTER[j] == AFTER[i]: j += 1
            e.step(AFTER[i], j - i); g += j - i; i = j
        white = None
        while g < 11040:
            e.step(AFTER[i]); i += 1; g += 1
            if e.shot().mean() > 200: white = g - 1; break
        if white is None: out.append([k, None, []]); continue
        d = []
        for n, key in enumerate(POST, 1):
            e.step(key)
            if n in SHOTS:
                d.append(round(float(np.abs(e.shot().astype(np.int16) - SHOTS[n]).mean()), 2))
                if d[-1] > 0 and len(d) >= 2: break
            if n >= max(CHECKS): break
        out.append([k, white, d])
    e.close()
    return p, out
if __name__ == '__main__':
    from multiprocessing import Pool
    run, tab, prs, ref = sys.argv[1:5]; a, b, outf = int(sys.argv[5]), int(sys.argv[6]), sys.argv[7]
    w = int(sys.argv[8]) if len(sys.argv) > 8 else 15
    kmax = int(sys.argv[9]) if len(sys.argv) > 9 else 6000
    with Pool(w, initializer=init, initargs=(run, tab, prs, ref, kmax)) as pool, open(outf, 'a') as fo:
        for r in pool.imap_unordered(run_p, range(a, b), chunksize=1):
            fo.write(json.dumps(r) + '\n'); fo.flush()
