"""segments.py RESULT.json OUTDIR [--every N]: cut a vidsync result into segments that each start from
power-on with a GBA save file, the way a person resumes a game, so a test need not replay from frame 0.

Segment 0 is a new game from power-on to the first checkpoint G1. Segment K (K >= 1) powers on with the
save segment K-1 wrote, presses START and CONTINUE ("bringup"), sets what a reload does not restore to the
recorded run's values at G_K (the RNG seeds and their battle copies, iCurrFrame: "junction" writes), then
plays the run's inputs and writes from G_K to G_K+1. Every segment but the last ends with "teardown": the
PET menu's Save (START, UP to Save, A, A on Yes, A). The save memory after that teardown is the next
segment's start save, so the game itself writes every checkpoint; a test powers each segment on (a hard reset)
with its save.

A checkpoint G_K is the first frame at or after K*N in an idle stretch (no input for 30 frames before and 60
after, so MegaMan stands and the reload has nothing under way to shift) where the teardown saves (the save
memory changes); some places don't allow the menu (cutscenes, battles, scenario modes).

Writes OUTDIR/segNN.trace, segNN.poke (the segment's writes, in its own game frames) and segNN.sav (the
save it starts from), and OUTDIR/segments.json (per segment: start/end frames of the recorded run, the
segment's junction frame, its end frame and digest, and the first run frame where its game state (RNG, iCurrFrame, map, MegaMan's place) differs from the recorded run's).
Environment as export_trace.py (VIDSYNC_ROM, VIDSYNC_BIOS, VIDSYNC_MGBA_RUN, VIDSYNC_THUMBS).
"""
import argparse, json, os, sys, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import Emu
import rngfix as rf
import export_trace as ex
import syms   # game addresses by name from bn6f

NONE, A, B, START, SELECT, UP = 0x3FF, 0x3FF & ~1, 0x3FF & ~2, 0x3FF & ~8, 0x3FF & ~4, 0x3FF & ~0x40
TEARDOWN = [(NONE, 20), (START, 2), (NONE, 70), (UP, 2), (NONE, 25), (A, 2), (NONE, 70), (A, 2), (NONE, 90),
            (A, 2), (NONE, 200)]
BRINGUP = [(NONE, 400), (START, 2), (NONE, 60), (A, 2), (NONE, 120)]   # title, CONTINUE (the default), load
ICURR = syms.ICURR_FRAME
# what a reload does not bring back as the recorded run had it at the checkpoint: the mystery data table (the
# load rolls it again, as a jack-in does), the overworld counters of eStruct2001c04 (encounter steps,
# SneakRun, LocEnemy), event flags and the last map; the junction writes the words that differ
REGIONS = [(syms.MYSTERY_DATA, 0x480), (syms.S2001C04, 0x30), (syms.GS_PLAYER_XYZ, 12),   # (GameState.PlayerX/Y/Z)
           (syms.EVENT_FLAGS, 0x400),     # eEventFlags: a load sets some of them its own way
           (syms.GS_LAST_MAP, 4),       # GameState LastMapGroup/LastMapNumber
           (syms.JOYPAD_HELD_TIMERS, 16)]      # eJoypad+8..+0x17: the keys' held times and the repeat timer (+0x13, 0-4):
                                    # a held key repeats on its phase (written always: they count every frame)
ALWAYS = {syms.JOYPAD_HELD_TIMERS}
SEEDS = (rf.PRIMARY, syms.BATTLE_SEED_COPY_LINK, syms.BATTLE_SEED_COPY)          # the primary and the battle intro's copies of it


def expand(seq):
    return [k for k, n in seq for _ in range(n)]


def save_hash(e, path):
    e.cmd(f'sram {path}')
    return hashlib.sha1(open(path, 'rb').read()).hexdigest()


STATE = {}          # the recorded run's game state every 30 game frames (pass 1 of find_checkpoints)
def game_state(e):
    """What a segment must have as the recorded run had it: the RNG seeds, iCurrFrame, the map, MegaMan's place,
    the key-repeat timer."""
    po = e.u32(syms.GS_OW_PLAYER_PTR)
    pos = e.read(po + 0x1C, 8).hex() if 0x02000000 <= po < 0x02040000 else ''
    return (e.u32(rf.PRIMARY), e.u32(rf.SECONDARY), e.read(ICURR, 2).hex(), e.read(syms.GS_MAP, 2).hex(), pos, e.read(syms.JOYPAD_REPEAT_TIMER, 1).hex())


def find_checkpoints(R, every, tmp):
    """Replay the run twice. First record, per game frame, the map, MegaMan's position and the game mode; a
    candidate frame has MegaMan standing for 8 frames on the map (not in a battle), the same map 60 frames before
    and 180 after (no transition the reload could shift). Then, from each multiple of EVERY on, the first
    candidate where the teardown saves (the save memory changes)."""
    g0, keys = R['press_gf'], R['keys']
    end = g0 + len(keys)
    def boot():
        e = Emu()
        for g, s in R.get('pokes', []): e.poke(g, s)
        e.step(n=400); e.step(START, 2); e.step(n=g0 - 402)
        return e
    e = boot(); info = {}
    for g in range(g0, end):
        po = e.u32(syms.GS_OW_PLAYER_PTR)
        pos = e.read(po + 0x1C, 8) if 0x02000000 <= po < 0x02040000 else b''
        info[g] = (e.read(syms.GS_MAP, 2), pos, e.read(syms.GS_SUBSYSTEM, 1)[0])
        if g % 30 == 0: STATE[g] = game_state(e)
        e.step(keys[g - g0])
    e.close()
    def good(g):
        if g - 60 < g0 or g + 180 >= end: return False
        m, pos, sub = info[g]
        return (sub == 4 and pos and all(info[g - k][1] == pos for k in range(1, 9))
                and all(info[g + k][0] == m for k in range(-60, 181, 10)))
    want, cand, target = [], [], every
    for g in range(g0, end):
        if g >= target and good(g):
            cand.append(g)
            if len(cand) >= 400 or g - target > 25000:
                want.append(cand); cand = []; target = (g // every + 1) * every
    if cand: want.append(cand)
    e = boot(); cks = []; g = g0; flat = sorted({c for cs in want for c in cs}); done_to = 0
    for c in flat:
        if c <= done_to: continue
        while g < c: e.step(keys[g - g0]); g += 1
        e.save(1); h0 = save_hash(e, tmp)
        for k in expand(TEARDOWN): e.step(k)
        ok = save_hash(e, tmp) != h0
        sav = open(tmp, 'rb').read()
        e.load(1)
        if not ok: continue
        # the junction write goes in at the end of the segment's frame that plays the run's frame g: the
        # values the run has after that frame
        e.step(keys[g - g0])
        po = e.u32(syms.GS_OW_PLAYER_PTR)
        ck = {'g': g, 'seeds': [e.u32(a) for a in SEEDS], 'secondary': e.u32(rf.SECONDARY),
              'icurr': int.from_bytes(e.read(ICURR, 2), 'little'),
              'regions': [(a, e.read(a, n).hex()) for a, n in REGIONS],
              # MegaMan's place and facing (the live player object; a load puts him a few pixels off)
              'player': (e.read(po + 0x1C, 0x30).hex(), e.read(po + 0x10, 1).hex()), 'sav': sav}
        e.load(1)
        # a trial: the save, bringup and junction, then 120 frames of the run; MegaMan where the run has him on
        # every frame (a standing MegaMan may be locked in the run, by a talk or a cutscene, and free after a
        # load: he would walk off)
        open(tmp + '.trial', 'wb').write(sav)
        t = Emu(save=tmp + '.trial')
        for k in expand(BRINGUP): t.step(k)
        for spec in junction_specs(ck, t): t.poke(t.gf()[0], spec)
        same = True
        for gg in range(g, min(g + 120, end)):
            t.step(keys[gg - g0])
            pt = t.u32(syms.GS_OW_PLAYER_PTR)
            if (t.read(pt + 0x1C, 8) if 0x02000000 <= pt < 0x02040000 else b'') != info.get(gg + 1, (0, b''))[1]:
                same = False; break
        t.close()
        if not same: continue
        cks.append(ck)
        print(f'checkpoint at {g}', flush=True)
        done_to = (g // every + 1) * every - 1          # the next one from the next multiple
    e.close()
    return cks


def junction_specs(ck, e):
    """The junction's writes, in poke lines of at most 16: the seeds, iCurrFrame, and the REGIONS words where
    the reloaded game (emulator e) differs from the recorded run at the checkpoint."""
    w = [f'{a:#x}={v:#x}/4' for a, v in zip(SEEDS, ck['seeds'])]
    w += [f'{rf.SECONDARY:#x}={ck["secondary"]:#x}/4', f'{ICURR:#x}={ck["icurr"]:#x}/2']
    for a, want in ck.get('regions', []):
        want = bytes.fromhex(want); have = e.read(a, len(want))
        for k in range(0, len(want), 4):
            if want[k:k + 4] != have[k:k + 4] or a in ALWAYS:
                w.append(f'{a + k:#x}={int.from_bytes(want[k:k + 4], "little"):#x}/4')
    if ck.get('player'):
        po = e.u32(syms.GS_OW_PLAYER_PTR); xyz, face = (bytes.fromhex(x) for x in ck['player'])
        if 0x02000000 <= po < 0x02040000:
            # X/Y/Z, NextX/Y/Z (where he walks to: the game moves him back towards it) and what follows
            w += [f'{po + 0x1C + k:#x}={int.from_bytes(xyz[k:k + 4], "little"):#x}/4' for k in range(0, len(xyz), 4)]
            w.append(f'{po + 0x10:#x}={face[0]:#x}/1')
    return [','.join(w[i:i + 16]) for i in range(0, len(w), 16)]


OVERLAP = 1000      # frames a segment plays past the next checkpoint (segments overlap there)


def build(R, lo, hi, ck_lo, start_sav, out, last):
    """Segment lo..hi in emu_server, two ways that share everything up to the run's frame hi: the segment proper
    plays on OVERLAP frames past hi; the teardown variant saves and soft-resets at hi (the save it writes ->
    OUT.endsav). Per way: its keys per game frame as the harness counts them, its writes, its end frame and
    digest; and the run frame where the game first differs from the recorded run's (None: never)."""
    g0, keys = R['press_gf'], R['keys']
    end_run = g0 + len(keys)
    e = Emu(save=start_sav) if start_sav else Emu()
    timeline, pokes = [], []
    follows = [None]

    def step(k):
        timeline.append((e.gf()[0], k)); e.step(k)

    def play(a, b):
        """the run's frames a..b-1, with its writes and the state check"""
        for g in range(a, b):
            f = e.gf()[0]
            for p in R.get('pokes', []):
                if p[0] == g: e.poke(f, p[1]); pokes.append((f, p[1]))
            if follows[0] is None and g in STATE and game_state(e) != STATE[g]:
                follows[0] = g; print(f'  differs at run frame {g}: {game_state(e)} != {STATE[g]}', flush=True)
            step(keys[g - g0])
    if ck_lo is None:                 # segment 0: a new game, the run's own boot, then its inputs
        tl = ex.timeline(R)
        for g in range(g0):
            if any(p[0] == g for p in R.get('pokes', [])):
                for p in R['pokes']:
                    if p[0] == g: e.poke(e.gf()[0], p[1]); pokes.append((e.gf()[0], p[1]))
            step(tl[g])
        junction = None
        play(g0, hi)
    else:
        for k in expand(BRINGUP): step(k)
        junction = e.gf()[0]
        for spec in junction_specs(ck_lo, e):
            e.poke(junction, spec); pokes.append((junction, spec))
        play(lo, hi)
    e.save(9); n_t, n_p = len(timeline), len(pokes)
    out_ = {}
    # the segment proper: on past the checkpoint
    play(hi, min(hi + OVERLAP, end_run))
    m = ex.DIGEST.search(' '.join(e.cmd('digest')))
    out_['seg'] = (list(timeline), list(pokes), e.gf()[0], m.group(0) if m else None)
    # the teardown variant: save and reset at the checkpoint
    if not last:
        e.load(9); del timeline[n_t:]; del pokes[n_p:]
        e.pokes_clear()
        for g, sp in pokes: e.poke(g, sp)
        for k in expand(TEARDOWN): step(k)          # (no reset: a test powers the next segment on with its save)
        e.cmd(f'sram {out}.endsav')
        m = ex.DIGEST.search(' '.join(e.cmd('digest')))
        out_['teardown'] = (list(timeline), list(pokes), e.gf()[0], m.group(0) if m else None)
    e.close()
    return out_, junction, follows[0]


def write_trace(path, timeline, end_g, note):
    with open(path, 'w') as f:
        f.write('# gbarecomp-keyinput-v1\n')
        f.write(f'# game_frame,keyinput_active_low ({note})\n')
        last = None
        for g, k in timeline:
            if k != last: f.write(f'{g},{k:#05X}\n'.replace('0X', '0x')); last = k


def write_files(base, way, sav, note):
    """BASE.trace / BASE.poke for one way of a segment, checked: mgba_run replays it from power-on with SAV to the
    same digest as emu_server."""
    timeline, pokes, end_g, emu_digest = way
    write_trace(base + '.trace', timeline, end_g, note)
    with open(base + '.poke', 'w') as f:
        for g, s in pokes: f.write(f'{g} {s}\n')
    hw, gg, digest = ex.mgba_replay(base + '.trace', sav, pokes, end_g)
    if digest != emu_digest: sys.exit(f'{base}: mgba_run {digest} != emu_server {emu_digest}')
    with open(base + '.trace', 'a') as f: f.write(f'# end hw_frames={hw} game_frame={end_g - 1} {digest}\n')
    return hw, digest


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('result'); ap.add_argument('out')
    ap.add_argument('--every', type=int, default=20000)
    A = ap.parse_args()
    R = json.load(open(A.result)); os.makedirs(A.out, exist_ok=True)
    cks = find_checkpoints(R, A.every, os.path.join(A.out, 'probe.sav'))
    end = R['press_gf'] + len(R['keys'])
    bounds = [(0, None)] + [(c['g'], c) for c in cks]
    manifest, verify = [], []
    for i, (lo, ck) in enumerate(bounds):
        hi = bounds[i + 1][0] if i + 1 < len(bounds) else end
        last = i + 1 == len(bounds)
        base = os.path.join(A.out, f'beastsp_seg_{i:02d}')
        # the save the teardown makes in the recorded run at the checkpoint (segment 0: a new game, blank)
        sav = base + '.sav'
        open(sav, 'wb').write(ck['sav'] if ck else b'\xff' * ex.SAVE_SIZE)
        ways, junction, follows = build(R, lo, hi, ck, sav if i else None, base, last)
        what = (f'vidsync segments.py: recorded run frames {lo}..{min(hi + OVERLAP, end)}'
                + ('' if i == 0 else f', power-on with {os.path.basename(sav)}, CONTINUE, junction at {junction}'))
        hw, digest = write_files(base, ways['seg'], sav, what)
        td = None
        if 'teardown' in ways:
            td = write_files(base + '_teardown', ways['teardown'], sav, what.replace(f'..{min(hi + OVERLAP, end)}', f'..{hi}, then the in-game Save'))
        nxt = bounds[i + 1][1] if not last else None
        chain = bool(nxt) and open(base + '.endsav', 'rb').read() == nxt['sav']
        if os.path.exists(base + '.endsav'): os.remove(base + '.endsav')
        m = {'segment': i, 'run_from': lo, 'checkpoint_to': hi, 'run_to': min(hi + OVERLAP, end),
             'junction': junction, 'end_game_frame': ways['seg'][2], 'hw_frames': hw, 'digest': digest,
             'teardown_digest': td[1] if td else None, 'differs_at': follows, 'teardown_save_is_next_start': chain}
        manifest.append(m)
        verify.append(f'seg {i:02d}: run frames {lo}..{m["run_to"]} (next checkpoint {hi}); '
                      + ('new game from power-on' if i == 0 else f'power-on with the save, CONTINUE; at segment frame {junction + 1} its game is the run\'s at frame {lo}')
                      + f'; # end digest {digest}' + (f'; teardown digest {td[1]}, its save is seg {i + 1:02d}\'s: {chain}' if td else '')
                      + ('; game state (RNG, iCurrFrame, map, MegaMan\'s place, every 30 frames) equal to the run throughout' if follows is None else f'; DIFFERS from the run at run frame {follows}'))
        print(verify[-1], flush=True)
    json.dump(manifest, open(os.path.join(A.out, 'segments.json'), 'w'), indent=1)
    open(os.path.join(A.out, 'VERIFY.txt'), 'w').write('\n'.join(verify) + '\n')


if __name__ == '__main__':
    main()
