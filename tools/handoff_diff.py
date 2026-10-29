#!/usr/bin/env python3
"""handoff_diff.py DUMP_DIR ROUTE [FRAME...] — given the dump pair DUMP_DIR/
{ROUTE}_orig_<frame>.{ewram,iwram,vram,pal,oam,save,ior}.bin and
DUMP_DIR/{ROUTE}_port_<frame>.{ewram,iwram,vram,pal,oam,save,ior}.bin,
diff each region byte-by-byte and classify differing RANGES using bn6f
symbols (lifter.elfinfo).

Regions and class labels:

  EWRAM  02000000-0203FFFF      game working memory (timers, RNG, sound
                                buffers, decompress staging, scene state)
  IWRAM-game 03000000-030056FF game's 22 KB scratch (iSoundInfo,
                                iCgbChannels, sprite shadow, etc.)
  IWRAM-port 03005700-030079CF the port's runtime region (linker script
                                gba.ld: dispatch, hosttest, ABI vars)
  IWRAM-stacks 030079D0-03007DFF port's IRQ + SYS stacks
  IWRAM-stack 03007E00-03007FFB game's user-mode stack
  IWRAM-irqvec 03007FFC        port's BIOS IRQ vector (4 bytes)
  VRAM   06000000-06017FFF     BG and OBJ tiles + tilemaps
  PAL    05000000-050003FF
  OAM    07000000-070003FF
  SAVE   0E000000-0E007FFF     SRAM

What the handoff has to copy: EWRAM, IWRAM-game, IWRAM-stack, VRAM, PAL,
OAM, SAVE.  What it must LEAVE alone: IWRAM-port, IWRAM-stacks,
IWRAM-irqvec.  "sound(real-time)" labels mark ranges whose differences
are real-time artifacts (audio buffer fills per hw frame); lockstep.sh
already accepts those as timing.

Without an explicit list of frames, diffs every FRAME present in DUMP_DIR.
"""
from __future__ import annotations
import bisect, glob, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from lifter import elfinfo

REGIONS = [   # (suffix, base, size, label)
    ("ewram", 0x02000000, 0x40000, "EWRAM"),
    ("iwram", 0x03000000, 0x8000,  "IWRAM"),
    ("vram",  0x06000000, 0x18000, "VRAM"),
    ("pal",   0x05000000, 0x400,   "PAL"),
    ("oam",   0x07000000, 0x400,   "OAM"),
    ("save",  0x0E000000, 0x8000,  "SAVE"),
    # 5 packed little-endian 32-bit I/O regs: DISPSTAT (0x04000004),
    # KEYINPUT (0x04000130), IF (0x04000202), IE (0x04000200),
    # IME (0x04000208).  From --dump-mem-at-when's ior.bin block.
    ("ior",   0x04000000, 0x14,    "IOR"),
]

# iwr-classify by port-linker layout (cport/gba.ld, src/gba/boot.c). game
# bytes always live below 0x03005700; port runtime between 0x03005700 and
# 0x030079D0; IRQ/SYS stacks between 0x030079D0 and 0x03007E00; game stack
# from 0x03007E00 down to wherever SP sits; BIOS_IRQ_VECTOR at 0x03007FFC.
def classify_iwram(addr):
    if addr < 0x03005700:         return "IWRAM-game"
    if addr < 0x030079D0:         return "IWRAM-port"
    if addr < 0x03007E00:         return "IWRAM-stacks"
    if addr < 0x03007FFC:         return "IWRAM-stack"
    return "IWRAM-irqvec"

# EWRAM ranges known to hold real-time data the original updates at
# variable rates (audio, counters): differences here are necessarily
# benign -- the lockstep script already routes them to "timing skew".
EWARM_SOUND = [(0x02007E00, 0x020335FF), (0x02033C00, 0x02033FFF)]  # m4a mixes
EWARM_VBLANK = [(0x02000000, 0x020000BF)]                           # timer_2000000..dword_2000060
IWRAM_SOUND = [(0x030045C0, 0x03005700)]                            # iSoundInfo + iCgbChannels

def classify(addr, region):
    if region == "iwram":
        c = classify_iwram(addr)
        if c == "IWRAM-game":
            for r in IWRAM_SOUND:
                if r[0] <= addr < r[1]: return "IWRAM-game sound"
        return c
    if region == "ewram":
        for r in EWARM_SOUND:
            if r[0] <= addr < r[1]: return "EWRAM-sound"
        for r in EWARM_VBLANK:
            if r[0] <= addr < r[1]: return "EWRAM-rt"
        return "EWRAM"
    if region == "vram":  return "VRAM"
    if region == "pal":   return "PAL"
    if region == "oam":   return "OAM"
    if region == "save":  return "SAVE"
    if region == "ior":   return "IOR"
    return region

# Per-region symbol table so a differing offset gets a name.
SYMS = {   # region -> sorted list of (addr, name)
}
for region_suffix, base, size, _ in REGIONS:
    info = elfinfo.load(os.path.join(ROOT, "bn6f"))
    p = sorted((v, n) for n, v in info.equates.items() if base <= v < base + size)
    for v, n in info.labels.items():
        if base <= v < base + size:
            p.append((v, n))
    SYMS[region_suffix] = sorted(p)

def run_diff(a, b, base, size):
    """Yield (lo_addr, hi_addr) for each differing run, where lo_addr and
    hi_addr are absolute GBA addresses (base offset into file)."""
    lo = 0
    n_a = min(len(a), len(b))
    while lo < n_a:
        # find next start of a mismatch
        while lo < n_a and a[lo] == b[lo]:
            lo += 1
        if lo >= n_a:
            break
        # extend while bytes still differ (loose definition: include
        # "ongoing" bytes regardless of length)
        hi = lo
        while hi < n_a and a[hi] != b[hi]:
            hi += 1
        yield (base + lo, base + hi)
        lo = hi

def classify_run(lo, hi):
    """Classify a run by the FIRST byte's region label; for spans spanning
    region boundaries just take the first label (each label's tally is
    approximate by run).  Run spans are usually 1 byte plus a few bytes."""
    return classify(lo, "iwram" if 0x03000000 <= lo < 0x03008000 else
                       "ewram" if 0x02000000 <= lo < 0x02040000 else
                       "vram"  if 0x06000000 <= lo < 0x06018000 else
                       "pal"   if 0x05000000 <= lo < 0x05000400 else
                       "oam"   if 0x07000000 <= lo < 0x07000400 else
                       "save"  if 0x0E000000 <= lo < 0x0E008000 else
                       "ior"   if 0x04000000 <= lo < 0x04000400 else None)

def symbol_for(addr, region):
    syms = SYMS.get(region)
    if not syms:
        return None
    addrs = [v for v, _ in syms]
    k = bisect.bisect_right(addrs, addr) - 1
    if k < 0: return "<no-symbol>"
    return f"{syms[k][1]}+{addr - syms[k][0]:#x}"

def main():
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(2)
    dump_dir = sys.argv[1]
    route = sys.argv[2]
    frames = None
    if len(sys.argv) > 3:
        frames = sorted(int(f) for f in sys.argv[3:])
    else:
        seen = set()
        for p in glob.glob(os.path.join(dump_dir, f"{route}_port_*.bin")):
            base = os.path.basename(p)
            try:
                f = int(base.split("_")[2].split(".")[0])
                seen.add(f)
            except (IndexError, ValueError):
                continue
        frames = sorted(seen)
    print(f"=== route {route}  frames {frames}  dump dir {dump_dir} ===\n")

    grand_total = {}
    for f in frames:
        print(f"--- frame {f} ---")
        per_label = {}
        per_label_details = {}
        for region, base, size, label in REGIONS:
            orig = os.path.join(dump_dir, f"{route}_orig_{f}.{region}.bin")
            port = os.path.join(dump_dir, f"{route}_port_{f}.{region}.bin")
            if not (os.path.exists(orig) and os.path.exists(port)):
                continue
            a, b = open(orig, 'rb').read(), open(port, 'rb').read()
            runs = list(run_diff(a, b, base, size))
            if not runs:
                print(f"  {label:8s}: identical")
                continue
            tbytes = sum(h - l for l, h in runs)
            print(f"  {label:8s}: {len(runs):5d} runs, {tbytes:6d} bytes differing")
            for lo, hi in runs:
                c = classify(lo, region)
                per_label[c] = per_label.get(c, 0) + (hi - lo)
                per_label_details.setdefault(c, []).append((lo, hi, region))
        # One-line summary
        keys = sorted(per_label.keys())
        parts = [f"frame {f:7d}"]
        for k in keys:
            parts.append(f"{k}={per_label[k]}")
        print("    " + "  ".join(parts))
        print()
        # Top-N ranges per label
        for c, lst in per_label_details.items():
            lst.sort(reverse=True, key=lambda x: x[1] - x[0])
            print(f"    {c} top ranges:")
            for lo, hi, region in lst[:3]:
                sym = symbol_for(lo, region)
                print(f"      0x{lo:08x}-0x{hi:08x} ({hi - lo}b)  {sym}")
        print()
        for k, v in per_label.items():
            grand_total[k] = grand_total.get(k, 0) + v

    print("\n=== totals across all frames ===")
    for k in sorted(grand_total.keys()):
        print(f"  {k:30s} {grand_total[k]:6d}")

if __name__ == "__main__":
    main()
