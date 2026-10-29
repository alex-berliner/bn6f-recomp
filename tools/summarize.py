#!/usr/bin/env python3
"""summarize.py — print a comparison table of orig-vs-port at every captured
game frame across multiple routes.

Usage: summarize.py DIR_PREFIX ROUTE1 [ROUTE2 ...]
  Each DIR is a ramdump.sh output dir like /tmp/ramdump_story1/.
  Each ROUTE is a label chosen for printing.

Cross-references handoff_diff.py's classify_*.  Without running that script,
this script just reads the binary files (orig vs port) at every game frame
and counts bytes that differ per quadrant of the bus.
"""
import os, sys, hashlib, struct
from collections import defaultdict

def read_region(path, size):
    """Return region as bytes (zero-padded to SIZE if shorter)."""
    with open(path, 'rb') as f:
        b = f.read()
    return b.ljust(size, b'\0')

# layout of /tmp/ramdump_story1/<route>_<tag>_<g>.<region>.bin files
# tag is "orig" or "port"; region is one of ewram/iwram/vram/pal/oam/save/ior
REGIONS = {
    'ewram': (0x02000000, 0x40000),
    'iwram': (0x03000000, 0x8000),
    'vram':  (0x06000000, 0x18000),
    'pal':   (0x05000000, 0x400),
    'oam':   (0x07000000, 0x400),
    'save':  (0x0E000000, 0x8000),
    'ior':   (0x04000000, 0x14),
}

# IWRAM sub-class
def iwram_sub(a):
    if 0x030079D0 <= a < 0x03007E00: return 'IWRAM-stacks'
    if a == 0x03007FFC:              return 'IWRAM-irqvec'
    if 0x03007E00 <= a:              return 'IWRAM-stack'
    if 0x03005700 <= a < 0x030079D0: return 'IWRAM-port'
    if 0x03004D00 <= a < 0x03005700: return 'IWRAM-game sound'  # rough
    return 'IWRAM-game'

def iwram_sub2(a):
    """More accurate sound detection (matches handoff_diff's iwram data table)."""
    # bn6f IWRAM data: sound drivers end at 0x0300562C; rest is game globals.
    if 0x03005700 <= a < 0x030079D0: return 'IWRAM-port'
    if 0x030079D0 <= a < 0x03007E00: return 'IWRAM-stacks'
    if 0x03007E00 <= a:              return 'IWRAM-stack'
    if a == 0x03007FFC:              return 'IWRAM-irqvec'
    # sound drivers: iSoundInfo range, iCgbChannels at 0x3004D00
    if 0x3004540 <= a < 0x3005700:   return 'IWRAM-game+sound'  # too wide; refine below
    return 'IWRAM-game'

DIR = sys.argv[1]
routes = sys.argv[2:] if len(sys.argv) > 2 else sorted(set(
    f.rsplit('_', 1)[0]
    for f in os.listdir(DIR) if f.endswith('.ewram.bin') and '_orig_' in f))

print(f"{'route':<10} {'frames':>22} || {'ewram':>6} {'vram':>5} {'pal':>4} {'oam':>4} {'save':>5} {'ior':>4} || {'IWR-port':>9} {'IWR-game':>9} {'IWR-snd':>8} {'IWR-stk':>9} {'IWR-stks':>9} {'IWR-v':>7}")
for route in routes:
    # find frames
    fcands = sorted(set(
        int(f.split('_')[-1].split('.')[0])
        for f in os.listdir(DIR)
        if f.startswith(f'{route}_') and f.endswith('.ewram.bin')))
    fcands = [g for g in fcands if g >= 0]
    if not fcands: continue
    bytes_differ = defaultdict(int)
    for g in fcands:
        for reg, (base, size) in REGIONS.items():
            try:
                o = read_region(f'{DIR}/{route}_orig_{g}.{reg}.bin', size)
                p = read_region(f'{DIR}/{route}_port_{g}.{reg}.bin', size)
            except FileNotFoundError: continue
            for i, (a, b) in enumerate(zip(o, p)):
                if a != b:
                    bytes_differ[reg] += 1
                    if reg == 'iwram':
                        bytes_differ[iwram_sub(base + i)] += 1
                    elif reg == 'ewram':
                        # EWRAM sound driver at unk_2006EF8..end
                        if 0x02006EF8 + 0x1100 <= base + i < 0x020093D4:
                            bytes_differ['EWRAM-sound'] += 1
    frame_str = '/'.join(str(g) for g in fcands[:4]) + ('...' if len(fcands) > 4 else '')
    print(f"{route:<10} {frame_str:>22} || "
          f"{bytes_differ.get('ewram',0):>6} "
          f"{bytes_differ.get('vram',0):>5} "
          f"{bytes_differ.get('pal',0):>4} "
          f"{bytes_differ.get('oam',0):>4} "
          f"{bytes_differ.get('save',0):>5} "
          f"{bytes_differ.get('ior',0):>4} || "
          f"{bytes_differ.get('IWRAM-port',0):>9} "
          f"{bytes_differ.get('IWRAM-game',0):>9} "
          f"{bytes_differ.get('IWRAM-game+sound',0):>8} "
          f"{bytes_differ.get('IWRAM-stack',0):>9} "
          f"{bytes_differ.get('IWRAM-stacks',0):>9} "
          f"{bytes_differ.get('IWRAM-irqvec',0):>7}")
