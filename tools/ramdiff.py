#!/usr/bin/env python3
"""ramdiff.py A.ewram.bin B.ewram.bin [base] — list differing ranges, named
with the nearest bn6f symbol."""
import bisect, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lifter import elfinfo
a, b = open(sys.argv[1], 'rb').read(), open(sys.argv[2], 'rb').read()
base = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x02000000
info = elfinfo.load(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'bn6f'))
syms = sorted((v, n) for n, v in list(info.equates.items()) + [(n, v) for v, n in info.labels.items()]
              if base <= v < base + len(a))
addrs = [v for v, _ in syms]
runs, i = [], 0
while i < len(a):
    if a[i] != b[i]:
        j = i
        while j < len(a) and (a[j] != b[j] or (j + 1 < len(a) and a[j + 1] != b[j + 1])):
            j += 1
        runs.append((i, j))
        i = j
    i += 1
for lo, hi in runs[:60]:
    addr = base + lo
    k = bisect.bisect_right(addrs, addr) - 1
    name = f"{syms[k][1]}+{addr - syms[k][0]:#x}" if k >= 0 else "?"
    print(f"{addr:#010x} len {hi - lo:4d}  {name:40s} A={a[lo:min(hi, lo + 8)].hex()} B={b[lo:min(hi, lo + 8)].hex()}")
print(f"{len(runs)} differing ranges, {sum(h - l for l, h in runs)} bytes")
