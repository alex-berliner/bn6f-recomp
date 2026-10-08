#!/usr/bin/env python3
"""Generate traces that visit every map of the game.

  tools/mapsweep.py OUTDIR START.sav [FRAMES_AFTER_WARP]

Each OUTDIR/map_GG_MM.trace continues START.sav past the title screen,
stands still until game frame 1200 (free roam), and then plays random
input (tools/fuzzplay.py's model: walking, talking, the PET menu). Its
.poke performs, at game frame 1200, the warp the game's own door code
performs (asm00_1.s sub_80058D0 / map_triggerEnterMapOnWarp_8005C04):
the warp entry pointer, warp index 1, transition 0, the pending flag, and
GameState.SubsystemIndex 0x10 (same world) or 0x14 (jack in/out, when the
warp crosses between the real and the cyber world). Entries are the game's
own warp/jack-in records for each map (checked: their first two bytes are
the map's group and number). Maps whose scripts are NULL in Falzar (95:03,
96:00, 96:02) and those with NULL warp tables (88:02, 88:04) are left out.
"""
import os, random, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
ENTRIES = """
00:00 804CE70 00:01 8097120
01:00 804E3F0 01:01 804E3A0 01:02 804E410 01:03 804E400 01:04 804E3D0
02:00 8052360 02:01 8052380 02:02 8052300 02:03 8052320 02:04 80522B0 02:05 8052270 02:06 804E3B0
02:07 80523D0 02:08 80523E0 02:09 8052410 02:0A 8052420
03:00 80592D8 03:01 80592B8 03:02 80592F8 03:03 80592E8 03:04 80592C8
04:00 805DD78 04:01 805DD48 04:02 805DD98 04:03 805DD88 04:04 805DD58
05:00 80601D4 05:01 80601A4 05:02 80601B4 05:03 80601C4
06:00 804E3C0 06:01 8062770 06:02 8052450 06:03 8062760 06:04 8062780 06:05 80627B0
80:00 8066038 80:01 8066028 81:00 8067704 81:01 80676F4 81:02 8067714
82:00 8068CE8 82:01 8068CD8 82:02 8068CF8 83:00 80698F8 83:01 80698E8 83:02 8069908
85:00 80986BC 85:01 80986D0 85:02 80986E4 85:03 80986F8 85:04 809870C
88:00 806C0C4 88:01 806C064 88:03 806C034 88:05 806C044 88:06 806C054
8C:00 80984C8 8C:01 80715D0 8C:02 80984F0 8C:03 8098518 8C:04 809852C 8C:05 8098540 8C:06 8098554
8C:07 8098608 8C:08 80985E0 8C:09 80985A4 8C:0A 80776D0 8C:0B 809861C 8C:0C 8098630 8C:0D 8098644
8C:0E 8098658 8C:0F 8079FE0
8D:00 807C86C 8D:01 807C8FC 8D:02 8098720 8D:03 8098734 8D:04 8098748 8D:05 809875C 8D:06 8098770
8D:07 8098784 8D:08 8071600 8D:09 80987AC 8D:0A 80987C0 8D:0B 80987D4 8D:0C 80987E8 8D:0D 8074F6C
8D:0E 80987FC 8D:0F 8098810
90:00 806C024 90:01 806FB88 90:02 80715F0 91:00 806FBD8 91:01 806C0E4 91:02 8071610
92:00 806C114 92:01 8077630 93:00 8071670 93:01 80790BC 94:00 806C154 94:01 806D810 94:02 806C084
95:00 806FAE8 95:01 8077710 95:02 806FB08 96:01 807C8CC
"""
WARP_FRAME = 1200


def main():
    outdir, save = sys.argv[1], sys.argv[2]
    after = int(sys.argv[3]) if len(sys.argv) > 3 else 2500
    os.makedirs(outdir, exist_ok=True)
    rom = open(os.path.join(ROOT, "bn6f/bn6f.gba"), "rb").read()
    toks = ENTRIES.split()
    import fuzzplay
    kept = skipped = 0
    for i in range(0, len(toks), 2):
        g, m = (int(x, 16) for x in toks[i].split(":"))
        addr = int(toks[i + 1], 16) | 0x08000000
        e = rom[addr - 0x08000000: addr - 0x08000000 + 4]
        if e[0] != g or e[1] != m:
            skipped += 1
            continue
        name = os.path.join(outdir, f"map_{g:02X}_{m:02X}")
        cross = g >= 0x80       # START.sav is in the real world
        writes = [f"0x2011bc4={addr:#x}/4", "0x2011bc1=1/1", "0x2011bc2=0/1", "0x2011bc0=1/1", "0x2001be8=0/4",
                  f"0x2001b80={0x14 if cross else 0x10:#x}/1"]
        with open(name + ".poke", "w") as f:
            f.write(f"{WARP_FRAME} " + ",".join(writes) + "\n")
        # random play after the warp, from fuzzplay's model; standing still before it
        tmp = name + ".tmp.trace"
        fuzzplay_trace(fuzzplay, tmp, save, g * 256 + m, WARP_FRAME + after)
        lines = [l for l in open(tmp) if l[0] == "#" or int(l.split(",")[0]) < 800]
        rest = [l for l in open(tmp) if l[0].isdigit() and int(l.split(",")[0]) >= WARP_FRAME + 120]
        with open(name + ".trace", "w") as f:
            f.writelines(lines + [f"800,0x3FF\n"] + rest)
        os.remove(tmp)
        os.replace(tmp[:-len(".trace")] + ".sav", name + ".sav")
        kept += 1
    print(f"{kept} maps, {skipped} entries skipped (first bytes not the map's group/number)")


def fuzzplay_trace(fuzzplay, out, save, seed, end):
    argv = sys.argv
    sys.argv = ["fuzzplay.py", out, save, str(seed), str(end)]
    try:
        fuzzplay.main()
    finally:
        sys.argv = argv


if __name__ == "__main__":
    main()
