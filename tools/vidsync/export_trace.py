"""export_trace.py RESULT.json OUT [--verify]: a vidsync result as a route-harness recording.

Writes, as recomp's tools/record does for a recording made by hand:
  OUT.trace  "# gbarecomp-keyinput-v1", then "game_frame,keyinput" lines (hex, active low) on every
             change, then "# end hw_frames=H game_frame=G <digest>";
  OUT.sav    the save memory the run starts from (blank: 32 KB of 0xFF, a new game, as emu_server);
  OUT.poke   the result's memory writes, "GAME_FRAME ADDR=VALUE/SIZE,..." one per line in game-frame
             order (only if it has any).

How the timelines line up. emu_server's `step KEY` runs hardware frames holding KEY while the game
frame counter (the u16 at *(u32*)GAME_FRAME_ADDR, unwrapped past 65535) still reads the frame it read
before the step; mgba_run --game-frame-addr reads the same counter before every hardware frame and
holds the key of the last trace line whose game frame is <= it. So "G,K" holds K over every hardware
frame that starts while the counter reads G..(next line's G - 1): the same hardware frames emu_server
ran for those steps. The counter reads 0 from reset (for the first 16 hardware frames, before the
game sets the pointer up) and goes up by one per step, so step i after reset is game frame i. The
vidsync boot (beam.py Beam.start, make_resume.py) is step(n=400); step(START, 2);
step(n=press_gf-402): START held on game frames 400 and 401. RESULT's keys[i] is game frame
press_gf+i; the result ends when the counter reaches END = press_gf+len(keys).

Pokes: emu_server and mgba_run --poke-at (one per .poke line) both write at
the first entry of main_awaitFrame (AWAIT_FRAME_PC) in the first hardware frame that starts on game
frame G. A poke at G >= END never happens inside the trace and is left out.

The "# end" line: hw_frames is the hardware frames the replay runs (the hardware frame in which the
counter reaches END is the last), game_frame is the counter at the start of that last hardware frame
(END-1: record.c writes the last game frame it latched keys for), and the digest is the state after
it. They are made by replaying the trace with mgba_run itself (--until-game-frame END
--final-digest; make -C tools mgba_run). tools/check_recording.sh replays
--frames hw_frames and compares that digest.

--verify also replays RESULT in emu_server (boot, pokes, keys, as beam.py) and checks the game
frame, hardware frame count and digest equal mgba_run's (and the result's own hw count 'h', if any).

Environment: VIDSYNC_ROM, VIDSYNC_BIOS (as emu.py), VIDSYNC_MGBA_RUN (default: tools/paths.env's
MGBA_RUN, build-tools/mgba_run).
"""
import argparse, json, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import Emu, ROM, BIOS, gamepaths
import syms   # game addresses by name from bn6f

GAME_FRAME_ADDR = syms.GAME_FRAME_ADDR
AWAIT_FRAME_PC = syms.AWAIT_FRAME_PC       # main_awaitFrame in bn6f.gba: where pokes go in
START = 0x3FF & ~8
BOOT_IDLE, BOOT_START = 400, 2    # Beam.start: step(n=400); step(START, 2); step(n=press_gf-402)
SAVE_SIZE = 0x8000
MGBA_RUN = os.environ.get('VIDSYNC_MGBA_RUN', gamepaths.MGBA_RUN)
DIGEST = re.compile(r'ewram=[0-9a-f]+ pal=[0-9a-f]+ vram=[0-9a-f]+ oam=[0-9a-f]+')


def timeline(R):
    """The key held on every game frame from reset to the end: a list indexed by game frame."""
    P0 = R['press_gf']
    assert P0 >= BOOT_IDLE + BOOT_START, f'press_gf {P0} is inside the boot sequence'
    return [0x3FF] * BOOT_IDLE + [START] * BOOT_START + [0x3FF] * (P0 - BOOT_IDLE - BOOT_START) + list(R['keys'])


def trace_lines(keys):
    """record.c's lines: the first game frame, then every change."""
    out, last = [], None
    for g, k in enumerate(keys):
        if k != last:
            out.append(f'{g},0x{k:03X}')
            last = k
    return out


def poke_lines(R, end):
    pokes = sorted(R.get('pokes', []), key=lambda p: p[0])     # stable: same-frame writes keep their order
    keep = [(g, spec) for g, spec in pokes if g < end]
    for g, spec in pokes:
        if g >= end:
            print(f'poke at game frame {g} is at or past the end ({end}): left out ({spec})', file=sys.stderr)
    return keep


def mgba_replay(trace, save, pokes, end):
    """Replay with the harness until the counter reaches END: (hw frames, final game frame, digest)."""
    cmd = [MGBA_RUN, ROM, '--quiet', '--frames', '100000000', '--bios', BIOS, '--input', trace,
           '--game-frame-addr', hex(GAME_FRAME_ADDR), '--until-game-frame', str(end), '--final-digest',
           '--save', save]
    for g, spec in pokes:
        cmd += ['--poke-at', hex(AWAIT_FRAME_PC), str(g), spec]
    out = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    m = re.search(r'^reached game frame (\d+) at hardware frame (\d+)$', out, re.M)
    f = re.search(r'^final game=(\d+) hw=(\d+) (.*)$', out, re.M)
    if not m or not f:
        sys.exit(f'mgba_run did not reach game frame {end}:\n{out[-2000:]}')
    applied = len(re.findall(r'^poke game=', out, re.M))
    nwrites = sum(len(spec.split(',')) for _, spec in pokes)
    if applied != nwrites:
        sys.exit(f'mgba_run applied {applied} of {nwrites} writes')
    return int(f.group(2)), int(f.group(1)), DIGEST.search(f.group(3)).group(0)


def emu_replay(R, keys):
    """The result as vidsync ran it: emu_server, pokes registered, boot, then the keys."""
    e = Emu()
    for g, spec in R.get('pokes', []):
        e.poke(g, spec)
    i = 0
    while i < len(keys):
        j = i
        while j < len(keys) and keys[j] == keys[i]:
            j += 1
        e.step(keys[i], j - i)
        i = j
    d = e.digest()
    e.close()
    return d['hw'], d['game'], ' '.join(f'{r}={d[r]}' for r in ('ewram', 'pal', 'vram', 'oam'))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('result'); ap.add_argument('out', help='OUT basename (OUT.trace, OUT.sav, OUT.poke)')
    ap.add_argument('--save', help='start save (default: blank, a new game, as emu_server without SAVE)')
    ap.add_argument('--verify', action='store_true', help='also replay in emu_server and compare')
    a = ap.parse_args()
    out = a.out[:-len('.trace')] if a.out.endswith('.trace') else a.out
    R = json.load(open(a.result))
    keys = timeline(R)
    end = len(keys)
    if 'g' in R and R['g'] != end:
        sys.exit(f"result's g {R['g']} is not press_gf + len(keys) = {end}")

    save = open(a.save, 'rb').read() if a.save else b'\xff' * SAVE_SIZE
    open(out + '.sav', 'wb').write(save)
    pokes = poke_lines(R, end)
    if os.path.exists(out + '.poke'):
        os.remove(out + '.poke')
    if pokes:
        with open(out + '.poke', 'w') as f:
            for g, spec in pokes:
                f.write(f'{g} {spec}\n')
    body = ['# gbarecomp-keyinput-v1',
            f'# game_frame,keyinput_active_low (vidsync scripts/export_trace.py {os.path.basename(a.result)}; '
            f'start save {a.save or "blank, new game"})'] + trace_lines(keys)
    open(out + '.trace', 'w').write('\n'.join(body) + '\n')

    hw, g, dig = mgba_replay(out + '.trace', out + '.sav', pokes, end)
    with open(out + '.trace', 'a') as f:
        f.write(f'# end hw_frames={hw} game_frame={end - 1} {dig}\n')
    print(f'{out}.trace: {len(body) - 2} input changes, game frames 0..{end - 1}, {hw} hardware frames, '
          f'{len(pokes)} poke lines; mgba_run: game={g} hw={hw} {dig}')

    if a.verify:
        ehw, eg, edig = emu_replay(R, keys)
        print(f'emu_server: game={eg} hw={ehw} {edig}')
        bad = [n for n, x, y in (('game', g, eg), ('hw', hw, ehw), ('digest', dig, edig)) if x != y]
        if 'h' in R and R['h'] != ehw:
            bad.append(f"result h={R['h']}")
        print('verify:', 'MISMATCH ' + ', '.join(bad) if bad else 'emu_server and mgba_run agree')
        sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
