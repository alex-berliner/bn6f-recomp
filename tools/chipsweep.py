#!/usr/bin/env python3
"""Generate battle traces that use every chip in the game.

  tools/chipsweep.py OUTDIR [BATTLE_FRAMES]

Every standard, mega and giga chip (bn6f ChipDataArr, 0x08021DA8, 0x2C per
chip, valid codes in bytes 0-3, library type in byte 7) goes into a 30-chip
folder (at most 2 gigas per folder: the deck shuffle needs 13 non-gigas).
For each folder, OUTDIR/chipsNN.trace replays battle.trace up to its battle
start and then plays biased random battle input; OUTDIR/chipsNN.poke starts
one of several normal battles (battle settings with byte 3 = 0, which use
the player's folder) and writes, at the same moment:
  - the folder into save folder 0 (0x02002178, u16 chipId | code << 9) and
    selects it (0x020047F9 = 0), with no reg or tag chips;
  - the "chip owned" anti-cheat entries (0x2004C20 + id = 0x20008A0 + id ^ 0x81)
    so the chips are not replaced by error chips.
Run them with tools/tracesweep.sh.
"""
import os, random, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHIPS, CHIP_SIZE, NCHIPS = 0x21DA8, 0x2C, 411
SETTINGS = [0x080AEE90, 0x080AEEB0, 0x080AEF00, 0x080AEF40, 0x080AEF80, 0x080AEFC0]
BITS = dict(A=0, B=1, SELECT=2, START=3, RIGHT=4, LEFT=5, UP=6, DOWN=7, R=8, L=9)


def mask(*keys):
    v = 0x3FF
    for k in keys:
        v &= ~(1 << BITS[k])
    return v


def chips():
    rom = open(os.path.join(ROOT, "bn6f/bn6f.gba"), "rb").read()
    out = {0: [], 1: [], 2: []}
    for cid in range(1, NCHIPS):
        e = rom[CHIPS + cid * CHIP_SIZE: CHIPS + (cid + 1) * CHIP_SIZE]
        codes = [c for c in e[0:4] if c != 0xFF and c <= 0x1A]
        if e[7] in out and codes:
            out[e[7]].append((cid, codes[0]))
    return out


def main():
    outdir = sys.argv[1]
    battle_frames = int(sys.argv[2]) if len(sys.argv) > 2 else 5000
    os.makedirs(outdir, exist_ok=True)
    c = chips()
    std, mega, giga = c[0], c[1], c[2]
    ngroups = max((len(std) + 23) // 24, (len(mega) + 3) // 4, (len(giga) + 1) // 2)
    prefix = [l for l in open(os.path.join(ROOT, "tools/traces/battle.trace")) if l[0].isdigit() and int(l.split(",")[0]) < 16100]
    for g in range(ngroups):
        folder = [std[(g * 24 + i) % len(std)] for i in range(24)] + \
                 [mega[(g * 4 + i) % len(mega)] for i in range(4)] + [giga[(g * 2 + i) % len(giga)] for i in range(2)]
        writes = [f"0x2001b9c={SETTINGS[g % len(SETTINGS)]:#x}/4", "0x2001b8c=*0x2001b84/2", "0x2001b80=8/1"]
        writes += [f"{0x2002178 + 2 * i:#x}={cid | code << 9:#x}/2" for i, (cid, code) in enumerate(folder)]
        writes += ["0x20047f9=0/1", "0x20047fa=0xff/1", "0x2004822=0xff/1", "0x2004823=0xff/1"]
        writes += [f"{0x2004C20 + cid:#x}=*{0x20008A0 + cid:#x}^0x81/1" for cid in sorted({cid for cid, _ in folder})]
        name = os.path.join(outdir, f"chips{g:02d}")
        with open(name + ".poke", "w") as f:
            f.write("16100 " + ",".join(writes) + "\n")
        rng = random.Random(g)
        ev, t = [], 16100
        while t < 16100 + battle_frames:
            r = rng.random()
            if r < 0.55:                                   # A: select chips in the custom screen, use them
                ev += [(t, mask("A")), (t + 4, 0x3FF)]; t += rng.randint(8, 30)
            elif r < 0.70:                                 # move MegaMan / the custom cursor
                ev += [(t, mask(rng.choice(["UP", "DOWN", "LEFT", "RIGHT"]))), (t + 6, 0x3FF)]; t += rng.randint(8, 20)
            elif r < 0.82:                                 # L/R: open the custom screen, OK
                ev += [(t, mask(rng.choice(["L", "R"]))), (t + 4, 0x3FF)]; t += rng.randint(10, 30)
            elif r < 0.92:                                 # charge the buster
                n = rng.randint(30, 90)
                ev += [(t, mask("B")), (t + n, 0x3FF)]; t += n + 4
            else:
                t += rng.randint(20, 60)
        with open(name + ".trace", "w") as f:
            f.write("# gbarecomp-keyinput-v1\n# battle.trace to its battle start, then random battle input (tools/chipsweep.py)\n")
            f.writelines(prefix)
            last = None
            for fr, k in ev:
                if k != last:
                    f.write(f"{fr},0x{k:03X}\n")
                    last = k
    print(f"{ngroups} folders: {len(std)} standard, {len(mega)} mega, {len(giga)} giga chips")


if __name__ == "__main__":
    main()
