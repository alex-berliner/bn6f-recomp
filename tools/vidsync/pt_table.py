"""pt_table.py: the first random battle's primary-RNG conditions, factored by what they depend on.
In Lan's room the primary moves 1 a frame and the secondary 2, and iCurrFrame is the game frame, so at the
second jack-in's R press (wait k after NEW GAME at p) primary position = c(p) + k, iCurrFrame & 3 =
(p + k + AT - BASE_P) & 3, secondary position = s(p) + 2k. The mystery data rolls and the walk to the
encounter take only the primary and iCurrFrame & 3, so:
  pt_table.py table RUN.json X_FROM X_TO OUT.jsonl [WORKERS]
    for each primary position x and phase f, poke it at the R press of the base path (BASE_P, BASE_K + f):
    [x, f, mystery data bytes, white frame or null, primary at the white frame] when both rolls match;
  pt_table.py press RUN.json P_FROM P_TO OUT.jsonl [WORKERS]
    for each NEW GAME press p: [p, c(p), s(p), secondary step per frame in the room, game frame at k=0]."""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
from pk_search import AT, BASE_P, BASE_K, ENC
GMD = {0x0200434C: '0005', 0x0200435E: '0106'}
def step(x): return (((((x << 1) | (x >> 31)) & 0xffffffff) + 1) & 0xffffffff) ^ 0x873CA9E5
def stream(seed, n):
    out, x = [], seed
    for _ in range(n): out.append(x); x = step(x)
    return out
def to_r_press(e, keys, p):
    e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=p - 402)
    i = 0
    while i < len(keys):
        j = i
        while j < len(keys) and keys[j] == keys[i]: j += 1
        e.step(keys[i], j - i); i = j
def init(run):
    global PRE, AFTER, SEEDS, POS1, POS2
    R = json.load(open(run)); K = R['keys']; P0 = R['press_gf']
    PRE = K[:AT - P0]; AFTER = K[AT - P0 + BASE_K:]; AFTER += [AFTER[-1]] * 400
    SEEDS = stream(0xA338244F, 60000)
    POS1 = {s: i for i, s in enumerate(SEEDS)}; POS2 = {s: i for i, s in enumerate(stream(0, 60000))}
def table_chunk(xs):
    from emu import Emu
    e = Emu(); to_r_press(e, PRE, BASE_P)
    e.step(0x3FF, BASE_K)
    for f in range(4): e.save(f); e.step(0x3FF)
    out = []
    for x in xs:
        for f in range(4):
            e.load(f); e.cmd(f'write32 20013f0 {SEEDS[x]:x}')
            g, i = AT, 0
            while g < 10300:
                j = i
                while AT + j < 10300 and AFTER[j] == AFTER[i]: j += 1
                e.step(AFTER[i], j - i); g += j - i; i = j
            b = e.read(0x0200434C, 20).hex()
            if any(e.read(a, 2).hex() != v for a, v in GMD.items()): continue
            while g < 10900:
                j = i
                while AT + j < 10900 and AFTER[j] == AFTER[i]: j += 1
                e.step(AFTER[i], j - i); g += j - i; i = j
            white = None
            while g < 11040:
                e.step(AFTER[i]); i += 1; g += 1
                if e.shot().mean() > 200: white = g - 1; break
            out.append([x, f, b, white, POS1.get(e.u32(0x020013F0))])
    e.close()
    return out
def press_one(p):
    from emu import Emu
    e = Emu(); to_r_press(e, PRE, p)
    c = POS1.get(e.u32(0x020013F0)); s = POS2.get(e.u32(0x02001120)); g, _ = e.gf()
    e.step(0x3FF, 4); s4 = POS2.get(e.u32(0x02001120)); e.close()
    return [p, None if c is None else c, s, None if s is None or s4 is None else (s4 - s) / 4, g]
if __name__ == '__main__':
    from multiprocessing import Pool
    mode, run, a, b, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
    w = int(sys.argv[6]) if len(sys.argv) > 6 else 15
    init(run)
    if mode == 'table':
        jobs = [list(range(x, min(x + 50, b))) for x in range(a, b, 50)]; fn = table_chunk
    else:
        jobs = list(range(a, b)); fn = press_one
    with Pool(w) as pool, open(out, 'a') as fo:
        for r in pool.imap_unordered(fn, jobs, chunksize=1):
            for row in (r if mode == 'table' else [r]): fo.write(json.dumps(row) + '\n')
            fo.flush()
