# vidsync progress log

| time | furthest game frame | what happened / fixed the last stall | coverage (route) |
|---|---|---|---|
| 2026-10-14 (start of this session) | 432,728 (run7m stuck; run6y.0/fix_0026 = 432,312 is the newest clean checkpoint) | session start; run7m stuck at the Mr. Weather comp (rainbow-orb trail / tornado), no RNG write found | route at 220,000: 4009/13690 lifted units (29.3%), 40.0% of bytes |
| 2026-10-14 ~10:50 | 432,312 (route at 432,312) | exported `bn6f_beastsp.{trace,poke,sav}` from run6y.0/fix_0026 (g=432312) via export_trace.py --verify (pathcheck held). 18 segments cut (0..17, ~30K each; seg 04 has the long-vs-LINKAREA chain break, seg 14 has 1-frame pos drift at run frame 363390, seg 14's save chain also breaks). Manually imported with `tools/import_route.py` and lockstepped the port on the new route (verify-20261014-102408 PASSED, 0 failed routes / 0 failed functions / 4 timing skews at 9000, 141200, 142640, 222000). lifter golden refresh: edges 1822→2396, overrides 2588→2566 (stale catch-up of 2 readable functions already in cport/src/game/). | route at 432,312: 4917/13690 lifted units (35.9%), 118919 unique blocks (+23003 vs 220k baseline). New functions-by-entry: 905, new functions-by-body: 907, new edges: 574. |

