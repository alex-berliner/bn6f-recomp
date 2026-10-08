#!/usr/bin/env python3
"""Append input steps to a game-frame keyed trace (gbarecomp keyinput format).

    trace_add.py TRACE START STEP [STEP ...]

STEP is KEYS:FRAMES (hold KEYS for FRAMES game frames, then release for
the same gap) or KEYS*N@P (tap KEYS N times, one every P frames, 4 frames
each). KEYS is a '+'-joined list of A B SELECT START RIGHT LEFT UP DOWN R L,
or "-" (nothing). Prints the next free game frame.
"""
import sys
BITS = dict(A=0, B=1, SELECT=2, START=3, RIGHT=4, LEFT=5, UP=6, DOWN=7, R=8, L=9)


def mask(keys):
    v = 0x3FF
    if keys != "-":
        for k in keys.split("+"):
            v &= ~(1 << BITS[k.upper()])
    return v


path, t = sys.argv[1], int(sys.argv[2])
lines = [l for l in open(path).read().splitlines() if l and (l.startswith("#") or int(l.split(",")[0]) < t)]
for step in sys.argv[3:]:
    if "*" in step:
        keys, rest = step.split("*")
        n, p = rest.split("@")
        for _ in range(int(n)):
            lines += [f"{t},{mask(keys):#05X}".replace("0X", "0x"), f"{t + 4},0x3FF"]
            t += int(p)
    else:
        keys, frames = step.split(":")
        lines += [f"{t},{mask(keys):#05X}".replace("0X", "0x"), f"{t + int(frames)},0x3FF"]
        t += int(frames) + 8
open(path, "w").write("\n".join(lines) + "\n")
print(t)
