"""stream_pos.py PRESS...: the primary RNG's position (calls since the boot seed 0xA338244F) at game frame
10000 when NEW GAME is pressed on each given game frame and run1's keys follow."""
import sys, os, json, subprocess; sys.path.insert(0, os.path.dirname(__file__))
from multiprocessing import Pool
from emu import Emu
R = json.load(open('data/full/run1.json')); P0 = R['press_gf']; K = R['keys']
def pos(press):
    e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=press - 402)
    n = 10000 - P0; i = 0
    while i < n:
        j = i
        while j < n and K[j] == K[i]: j += 1
        e.step(K[i], j - i); i = j
    seed = e.u32(0x020013F0); e.close()
    d = subprocess.run(['build/rng_dist', 'a338244f', f'{seed:08x}', '2000000000'], capture_output=True, text=True).stdout.strip()
    return press, f'{seed:08x}', int(d)
if __name__ == '__main__':
    with Pool(8) as p:
        for press, seed, d in sorted(p.map(pos, [int(x) for x in sys.argv[1:]])):
            print(f'press {press}: seed {seed}, stream position {d:,}')
