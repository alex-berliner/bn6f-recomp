#!/usr/bin/env python3
"""Entry states that reach every state handler of a subsystem's dispatcher: the captured calls of an
entry point with its state bytes set to each valid value (and a few timer values).

    python3 tools/sig/state_sweep.py SNAPDIR OUTDIR "ADDR:SIZE=V1|V2|..." [...]

Each spec names an EWRAM field (address, size 1/2/4) and its values; every combination is written
for every captured call (snap_*.bin) as OUTDIR/snap_sweep_N.bin. The difftest (`-S OUTDIR NAME`)
and the CPU oracle (`--base` with OUTDIR's parent) then run each state handler from real memory.
"""
import itertools
import os
import struct
import sys

HDR = 76
EWRAM = 0x02000000


def main() -> None:
    src, out, specs = sys.argv[1], sys.argv[2], sys.argv[3:]
    fields = []
    for s in specs:
        where, vals = s.split("=")
        addr, size = where.split(":")
        fields.append((int(addr, 16), int(size), [int(v, 0) for v in vals.split("|")]))
    os.makedirs(out, exist_ok=True)
    n = 0
    for f in sorted(os.listdir(src)):
        if not f.startswith("snap_") or f.endswith(".exit.bin"):
            continue
        base = bytearray(open(os.path.join(src, f), "rb").read())
        for combo in itertools.product(*[v for _a, _s, v in fields]):
            d = bytearray(base)
            for (addr, size, _v), val in zip(fields, combo):
                off = HDR + addr - EWRAM
                d[off:off + size] = val.to_bytes(size, "little")
            open(os.path.join(out, f"snap_sweep_{n}.bin"), "wb").write(d)
            n += 1
    print(f"{n} states in {out}")


if __name__ == "__main__":
    main()
