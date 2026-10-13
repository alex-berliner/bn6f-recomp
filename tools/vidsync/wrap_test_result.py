"""wrap_test_result.py OUT.json: a synthetic vidsync result that runs past the game frame counter's wrap at
65535 (idle on the title screen; key presses and pokes after 65536, two pokes on one frame), for
export_trace.py --verify: emu_server and the patched mgba_run must agree across the wrap."""
import json, sys
P0, END = 600, 66600
keys = [0x3FF] * (END - P0)
def press(g0, n, k):
    for g in range(g0, g0 + n): keys[g - P0] = k
press(65700, 2, 0x3FF & ~8)      # START
press(65900, 2, 0x3FF & ~1)      # A
press(66100, 11, 0x3FF & ~16)    # RIGHT
press(66300, 3, 0x3FF & ~1)      # A
pokes = [[30000, '0x20013f0=0x12345678/4'], [65540, '0x20013f0=0x9abcdef0/4'],
         [65540, '0x2001120=0x0badf00d/4'], [66250, '0x20013f0=0x55aa55aa/4']]
json.dump({'press_gf': P0, 'keys': keys, 'pokes': pokes}, open(sys.argv[1], 'w'))
