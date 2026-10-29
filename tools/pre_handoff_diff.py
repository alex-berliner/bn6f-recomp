#!/usr/bin/env python3
"""Pre-handoff diff.

Given two dirs DIR_ORIG (the original's --dump-mem-at-when output) and
DIR_PORT (the port's --dump-pre-handoff output), both with files
named <prefix>_G.<region>.bin (G = the handoff game frame), diff byte by
byte per region.  Output a per-region byte count and the FIRST ten
differing ranges (address + length + bn6f symbol), with IWRAM-port
range (0x03005700-0x03007E00 + 0x03007FFC-0x03007FFF) highlighted as
"port identity" (those differences are the port keeping its own
runtime, NOT a real divergence driver).
"""
import os, sys
from collections import defaultdict

if len(sys.argv) < 4:
    sys.stderr.write("usage: %s DIR_ORIG DIR_PORT G [GAME_FRAME_DIR]\n" % sys.argv[0])
    sys.stderr.write("  DIR_ORIG/port_dump_<G>.<region>.bin are loaded; the\n")
    sys.stderr.write("  bn6f symbols come from GAME_FRAME_DIR/symbols or the\n")
    sys.stderr.write("  elfinfo for bn6f next to GAME_FRAME_DIR.\n")
    sys.exit(2)

orig_dir, port_dir, g = sys.argv[1], sys.argv[2], int(sys.argv[3])
ROOT = os.environ.get("ROOT", "/home/alex/Code/recomp/build-wf-worktrees/handoff")
sys.path.insert(0, os.path.join(ROOT, "tools"))
from lifter import elfinfo

REGIONS = [
    ("ewram", 0x02000000, 0x40000),
    ("iwram", 0x03000000, 0x8000),
    ("vram",  0x06000000, 0x18000),
    ("pal",   0x05000000, 0x400),
    ("oam",   0x07000000, 0x400),
    ("save",  0x0E000000, 0x8000),
]

# Per-region classification (skip the IWRAM-port range: differences there mean
# "port keeps its own runtime", which is what the handoff is designed to do).
def classify(suf, addr):
    if suf == "iwram":
        if 0x03005700 <= addr < 0x03007E00: return "IWRAM-port-runtime"
        if 0x03007FFC <= addr < 0x03008000: return "IWRAM-port-biosvec"
        if 0x03007E00 <= addr < 0x03007FFC: return "IWRAM-port-stack"
        if 0x030045C0 <= addr < 0x03005700: return "IWRAM-game-sound"
        return "IWRAM-game"
    if suf == "ewram":
        if 0x02007E00 <= addr < 0x02033600: return "EWRAM-sound"
        return "EWRAM-game"
    return suf.upper()

info = elfinfo.load(os.path.join(ROOT, "bn6f"))
SYMS = []
for nm, val in info.equates.items():
    if isinstance(val, int): SYMS.append((val, nm))
for val, nm in info.labels.items():
    if isinstance(val, int) and isinstance(nm, str): SYMS.append((val, nm))
SYMS.sort()

def name_at(addr):
    pick = None
    for v, n in SYMS:
        if v <= addr: pick = (v, n)
        else: break
    if pick is None: return "(no symbol)"
    off = addr - pick[0]
    return f"{pick[1]}+0x{off:x}" if off else pick[1]

def merge(chunks, gap=32):
    """Merge a list of differing offsets into length-aligned ranges.
    Offsets within `gap` of the previous range extend it; chunk-aligned
    starts become new ranges."""
    if not chunks: return []
    rng = [[chunks[0], chunks[0] + 1]]
    for c in chunks[1:]:
        # round to nearest 16 bytes (or 32 for vram)
        if c - rng[-1][1] <= gap:
            rng[-1][1] = c + 1
        else:
            rng.append([c, c + 1])
    return rng

print(f"=== orig (dump-mem-at-when) vs port (dump-pre-handoff) at G={g} ===")
print(f"orig dir: {orig_dir}")
print(f"port dir: {port_dir}")
print()

for suf, base, size in REGIONS:
    # The default name comes from --dump-mem-at-when PREFIX, which is
    # "<prefix>_<g>.bin" (no .region in the path). pre_handoff.sh uses
    # PREFIX = "$PREFIX_<g>.<region>.bin". Accept either form by
    # globbing on the suffix.
    import glob
    orig_path = None
    port_path = None
    for cand in [
        os.path.join(orig_dir, f"orig_{g}.{suf}.bin"),
        os.path.join(orig_dir, f"orig_dump_{g}.{suf}.bin"),
        os.path.join(orig_dir, f"{g}.{suf}.bin"),
    ]:
        if os.path.exists(cand): orig_path = cand; break
    for cand in [
        os.path.join(port_dir, f"port_{g}.{suf}.bin"),
        os.path.join(port_dir, f"port_pre_{g}.{suf}.bin"),
        os.path.join(port_dir, f"{g}.{suf}.bin"),
    ]:
        if os.path.exists(cand): port_path = cand; break
    if not (orig_path and port_path):
        print(f"region={suf}: missing files -- orig={1 if orig_path else 0} port={1 if port_path else 0}")
        continue
    a = open(orig_path, "rb").read()
    b = open(port_path, "rb").read()
    if len(a) != len(b):
        print(f"region={suf}: SIZE DIFF orig={len(a)} port={len(b)}")
        continue
    by_class = defaultdict(list)
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            by_class[classify(suf, base + i)].append((base + i, x, y))
    print(f"region={suf:7s}  total diffs={sum(len(v) for v in by_class.values()):6d}  breakdown:")
    for k, v in by_class.items():
        if not v: continue
        print(f"  class={k:24s} count={len(v):6d}")
    if any(by_class.values()):
        print(f"  --- first 10 differing ranges class=non-port-runtime ---")
        all_non_port = []
        for k, v in by_class.items():
            if k.startswith("IWRAM-port"): continue  # port keeps its own runtime
            all_non_port.extend(v)
        all_non_port.sort()
        # merge into ranges
        rng = merge([i for (i, _, _) in all_non_port])
        for k, (lo, hi) in enumerate(rng[:10]):
            entries = [(addr, x, y) for (addr, x, y) in all_non_port if lo <= addr < hi]
            n = len(entries)
            # find a representative symbol
            symbol = name_at(entries[0][0]) if entries else "(empty)"
            print(f"    range {k+1}: [0x{lo:08x} + {hi-lo:5d} B  {n:5d} differing bytes]  ex:{symbol}")
            # first 2 entries detailed
            for addr, x, y in entries[:3]:
                print(f"      0x{addr:08x} {name_at(addr)}  orig=0x{x:02x} port=0x{y:02x}")
            if n > 3: print(f"      ... {n-3} more in range")
    print()
