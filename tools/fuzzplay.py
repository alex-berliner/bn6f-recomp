#!/usr/bin/env python3
"""Generate a random-play trace that continues a recorded session's save.

  tools/fuzzplay.py OUT.trace START.sav SEED [GAME_FRAMES]

The trace presses through the title screen to Continue (the inputs of the
menus2 recording, which reach the title menu, then A), then plays biased
random input until GAME_FRAMES: walking (sometimes running), many A taps
(talk, read, fire), the PET menu now and then, L/R. It never presses Select
(no soft reset). START.sav is copied next to the trace as OUT.sav so every
test script starts both ROMs from it.
"""
import os, random, shutil, sys

BITS = dict(A=0, B=1, SELECT=2, START=3, RIGHT=4, LEFT=5, UP=6, DOWN=7, R=8, L=9)
import gamepaths  # tools/gamepaths.py: the game profile (tools/paths.env)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def mask(keys):
    v = 0x3FF
    for k in keys:
        v &= ~(1 << BITS[k])
    return v


def main():
    out, save, seed = sys.argv[1], sys.argv[2], int(sys.argv[3])
    end = int(sys.argv[4]) if len(sys.argv) > 4 else 12000
    rng = random.Random(seed)
    title = [l for l in open(os.path.join(gamepaths.TRACES, "menus2.trace"))
             if l[0].isdigit() and int(l.split(",")[0]) < 330]
    ev = [(int(l.split(",")[0]), int(l.split(",")[1], 16)) for l in title]
    ev += [(450, mask("A")), (456, 0x3FF)]
    t = 800
    dirs = [("UP",), ("DOWN",), ("LEFT",), ("RIGHT",), ("UP", "LEFT"), ("UP", "RIGHT"), ("DOWN", "LEFT"), ("DOWN", "RIGHT")]
    while t < end:
        r = rng.random()
        if r < 0.40:        # walk, sometimes running (B), sometimes tapping A on the way
            keys = list(rng.choice(dirs)) + (["B"] if rng.random() < 0.3 else [])
            n = rng.randint(10, 120)
            ev.append((t, mask(keys)))
            t += n
        elif r < 0.75:      # A taps: talk, advance text, fire / use chips
            for _ in range(rng.randint(1, 6)):
                ev.append((t, mask(["A"]))); t += rng.randint(2, 6)
                ev.append((t, 0x3FF)); t += rng.randint(6, 40)
            continue
        elif r < 0.82:      # B taps: cancel, back out of menus
            for _ in range(rng.randint(1, 4)):
                ev.append((t, mask(["B"]))); t += 4
                ev.append((t, 0x3FF)); t += rng.randint(8, 30)
            continue
        elif r < 0.88:      # PET menu: open, move around, pick something, back out
            ev.append((t, mask(["START"]))); t += 4
            ev.append((t, 0x3FF)); t += 30
            for _ in range(rng.randint(1, 6)):
                ev.append((t, mask([rng.choice(["UP", "DOWN", "LEFT", "RIGHT", "A"])]))); t += 4
                ev.append((t, 0x3FF)); t += rng.randint(10, 40)
            for _ in range(4):
                ev.append((t, mask(["B"]))); t += 4
                ev.append((t, 0x3FF)); t += 20
            continue
        elif r < 0.95:      # L or R (jack in/out, custom screen, battle)
            ev.append((t, mask([rng.choice(["L", "R"])]))); t += rng.randint(4, 20)
        else:               # stand still
            ev.append((t, 0x3FF)); t += rng.randint(20, 120)
            continue
        ev.append((t, 0x3FF)); t += rng.randint(1, 12)
    with open(out, "w") as f:
        f.write(f"# gbarecomp-keyinput-v1\n# random play (tools/fuzzplay.py seed {seed}) from {os.path.basename(save)}\n")
        last = None
        for fr, k in ev:
            if k != last:
                f.write(f"{fr},0x{k:03X}\n")
                last = k
    shutil.copyfile(save, out[:-len(".trace")] + ".sav")


if __name__ == "__main__":
    main()
