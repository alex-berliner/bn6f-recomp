"""divergence.py RESULT.json OUT [--every N] [--from G] [--to G]: where mgba_run and emu_server part ways.

export_trace.py --verify compares the two replays of a result only at its end. This replays both again,
digests every N game frames (only between --from and --to, if given), and prints the first game frame whose
digests differ and which parts (ewram, pal, vram, oam). It writes its own recording of RESULT to OUT.trace,
OUT.sav (blank save), as export_trace.py does: keep OUT apart from data/ready. Run a coarse pass (--every
10000), then a fine one over the window it names.

mgba_run's --digest-every digests when the game frame counter at the start of a hardware frame is a multiple
of N; emu_server's digest after the steps that reach game frame G. The first comparison (at N) checks that
the two line up: if it already differs, the alignment is off, not the game.
"""
import argparse, json, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import Emu, ROM, BIOS
from export_trace import GAME_FRAME_ADDR, AWAIT_FRAME_PC, MGBA_RUN, SAVE_SIZE, timeline, trace_lines, poke_lines

PARTS = ('ewram', 'pal', 'vram', 'oam')
LINE = re.compile(r'^digest game=(\d+) hw=(\d+) ewram=(\w+) pal=(\w+) vram=(\w+) oam=(\w+)$', re.M)


def wanted(g, a):
    return g % a.every == 0 and a.start <= g <= a.stop


def mgba_digests(out, pokes, end, a):
    cmd = [MGBA_RUN, ROM, '--quiet', '--frames', '100000000', '--bios', BIOS, '--input', out + '.trace',
           '--game-frame-addr', hex(GAME_FRAME_ADDR), '--until-game-frame', str(min(end, a.stop + 1)),
           '--digest-every', str(a.every), '--save', out + '.sav']
    for g, spec in pokes:
        cmd += ['--poke-at', hex(AWAIT_FRAME_PC), str(g), spec]
    text = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    return {int(m[1]): dict(zip(('hw',) + PARTS, m.groups()[1:])) for m in LINE.finditer(text) if wanted(int(m[1]), a)}


def emu_digests(R, keys, a):
    e = Emu()
    for g, spec in R.get('pokes', []):
        if g < len(keys):
            e.poke(g, spec)
    out, g = {}, 0
    stop = min(len(keys), a.stop + 1)
    while g < stop:
        n = a.every - g % a.every if g % a.every else a.every
        n = min(n, stop - g)
        i = g
        while i < g + n:            # one step call per run of equal keys
            j = i
            while j < g + n and keys[j] == keys[i]:
                j += 1
            e.step(keys[i], j - i)
            i = j
        g += n
        if wanted(g, a):
            d = e.digest()
            out[d['game']] = {'hw': str(d['hw']), **{p: d[p] for p in PARTS}}
    e.close()
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('result'); ap.add_argument('out')
    ap.add_argument('--every', type=int, default=10000)
    ap.add_argument('--from', dest='start', type=int, default=0)
    ap.add_argument('--to', dest='stop', type=int, default=1 << 40)
    a = ap.parse_args()
    R = json.load(open(a.result))
    keys = timeline(R)
    end = len(keys)
    pokes = poke_lines(R, end)
    os.makedirs(os.path.dirname(a.out) or '.', exist_ok=True)
    open(a.out + '.sav', 'wb').write(b'\xff' * SAVE_SIZE)
    open(a.out + '.trace', 'w').write('\n'.join(['# gbarecomp-keyinput-v1'] + trace_lines(keys)) + '\n')
    m = mgba_digests(a.out, pokes, end, a)
    e = emu_digests(R, keys, a)
    common = sorted(set(m) & set(e))
    print(f'compared {len(common)} digests ({len(m)} mgba_run, {len(e)} emu_server), every {a.every}')
    last_same = None
    for g in common:
        bad = [p for p in ('hw',) + PARTS if m[g][p] != e[g][p]]
        if bad:
            print(f'first difference at game frame {g}: {", ".join(bad)} (last equal: {last_same})')
            return 1
        last_same = g
    print(f'no difference up to game frame {last_same}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
