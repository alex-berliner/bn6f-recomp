# vidsync progress log

| time | furthest game frame | what happened / fixed the last stall | coverage (route) |
|---|---|---|---|
| 2026-10-14 (start of this session) | 432,728 (run7m stuck; run6y.0/fix_0026 = 432,312 is the newest clean checkpoint) | session start; run7m stuck at the Mr. Weather comp (rainbow-orb trail / tornado), no RNG write found | route at 220,000: 4009/13690 lifted units (29.3%), 40.0% of bytes |
| 2026-10-14 ~10:50 | 432,312 (route at 432,312) | exported `bn6f_beastsp.{trace,poke,sav}` from run6y.0/fix_0026 (g=432312) via export_trace.py --verify (pathcheck held). 18 segments cut (0..17, ~30K each; seg 04 has the long-vs-LINKAREA chain break, seg 14 has 1-frame pos drift at run frame 363390, seg 14's save chain also breaks). Manually imported with `tools/import_route.py` and lockstepped the port on the new route (verify-20261014-102408 PASSED, 0 failed routes / 0 failed functions / 4 timing skews at 9000, 141200, 142640, 222000). lifter golden refresh: edges 1822→2396, overrides 2588→2566 (stale catch-up of 2 readable functions already in cport/src/game/). | route at 432,312: 4917/13690 lifted units (35.9%), 118919 unique blocks (+23003 vs 220k baseline). New functions-by-entry: 905, new functions-by-body: 907, new edges: 574. |
| 2026-10-14 ~11:44 | 434,762 (run7n stuck; previous loss 432728 at Mr. Weather comp, fixed) | vidsync-repair-20261014-105758 for run7m's Mr. Weather comp loss at 432728. LossKind.PUZZLE: orb-trail counter unk_2000310+0x06 (eWeatherCompState.orbTrail) was one tick behind the video, so the orb ring never closed on the tornado and the comp's outcome never played. Model (zai/glm-5.3) wrote tools/vidsync/handlers/weather_comp_orb_trail.py: place the tornado at MegaMan's own place 1+ raise-periods back, pinned for a raise period, at each frame around the loss; the game's own capture (owMapObject_weatherTornado_caught) then runs the video's outcome. Test from data/auto/run6y.0/fix_0026 reached 433328 (D+600) end-to-end. Restarted as run7n, which immediately went past 432728 and reached 434,762 (next loss: mystery data 8301 at 434760). +2034 frames so far. | route still 432,312 (no new export yet; +2k frames below the +50k threshold for re-export). |
| 2026-10-14 ~14:25 | 435,876 (run7n.1 stuck; previous losses 432728 and 434762 fixed) | run7n kept syncing: at 12:23 an RNG write 0x20013f0=0xecadad8/4 (P-111) at game 434676 fixed mystery data 8301, taking the run to 435,484; a second fix at 12:38 took it to 435,876. Then a new loss at 435,876 (also map 8301, records 154a/154b) - try_gmd, try_relocate, try_mapobj, try_encounter, try_position all stalled, RNG search is now in the wider |j|<=2000 stage (1h47m on this loss, no fix yet). The driver is still alive and searching. | route still 432,312 (no new export yet; +3,564 frames is still well below the +50k threshold for re-export). |



## 2026-10-15 (session 3, continuing) — the loss at 435876 is the tornado catch, 91 frames early

Big cleanups this session:

- **The phantom white fade explained.** Static replays of the recorded keys from
  `run7n.1/ckpt_0435000.state` "encountered" (sub 0x04→0x08→0x0C, white 255) at 435565 while every live
  machine synced through. Cause found: `ckpt_0435000.state` carries `poked_upto < 434064`, so re-queueing
  the run's 217 pokes re-fires the fix-2 RNG write (434064, `0x20013f0=0x1fb6e550/4` + battle-seed words)
  at the first stepped frame (~435000), setting the RNG ~936 frames back. Verified with
  probes/pokeprobe.py (the 432724/432725 orb-trail pokes are inert, the 434064 poke is pending) and
  probes/subprobe.py (the battle happens with or without the pokes queued — the state's own lineage
  battles there; the resumed *beam* avoids it by re-searching keys, hence 0.60 at 435500).
  Consequence: **judge handlers and searches from ring states (`w_*.state`), not from this checkpoint** —
  ring-state replays are faithful (verified: digest equal at 435793/435816; the walk to the loss costs
  1.4–3.7 with no battle).
- **The real loss.** From `w_435758.state`: our tornado map object slot 1 sits parked at (48,232) px
  (mode = paused, `+9 != 0`), MegaMan walks into its 0x14 box at **435874** (flag 0xBE6 sets, strikes
  2→1), the cutscene slides him to the comp's put-back point (-82,230) where he stands — while the
  video's tornado wanders on and catches him only at **~435970** (video flash ramp 435971→976, brightness
  142→172; the tornado's dark cloud over him 435963–970). So the hit fires ~96 frames early; the video's
  catch cutscene plays at ~435970. The video's tornado is still at (48,232) when he passes (pictures
  matched to 435874, cost 2-3) — the divergence is the tornado's move/pause pick, not its parked place.
- **The earlier round-4 "video hit ~435929-943" was derived on the corrupted lineage**; with the resync's
  own video mapping (ground truth) the flash is at 435971-976, i.e. t ≈ D+94..D+100 — the staged-hit
  sweep (D+34..D+80) never reached it. Its away-pin control broke ~435926 because the video's tornado
  sprite is visible approaching 435896+; a tornado pinned at the far corner can't match that.
- **Fix direction: RNG.** The tornado's pause (ours) vs move (video's) is an RNG draw
  (owMapObject_weatherTornado_moveOrPause_80A8F48: mode +9, pause timer +0x25, moves-since-pause +0x28,
  pause due at ≥+7). RNG events on the live lineage: 21, latest …435658P 435689/690PS 435703P 435710P
  **435711P (big jump — the tornado's draw burst)** 435760/761PS 435795P. The driver's earlier search
  only got through the 435795 and 435761 waves (24/600, 36/1200) before it crashed — the 435711 wave,
  where the pick actually lives, never ran.
- Launched a full autosync (built-ins + RNG search, 14 workers) from ckpt_0435000 →
  data/auto/repair435876-full-1. It re-forms the loss at 435876 (verified twice now) and will walk the
  search back to the 435711 wave.
- New probes this session: probes/pokeprobe.py (which queued pokes actually fire from a state),
  probes/subprobe.py (subsystem/RNG/position per frame), probes/rngtrace.py (RNG trace; keys are flat
  ints in result jsons, not (k,v) pairs), probes/bisectstates.py (static-vs-live ring digests),
  probes/vbright.py (video thumbnail brightness + display key per game frame).

## Session 4 (2026-10-15, ~11:00-13:10) — root cause found; loss declared unrepairable

### The gating chain (from decomp asm29.s + live dumps; all verified on .175)
- Tornado (type 26) per-frame update `sub_80A8F08`:
  1. `TestEventFlag(0xBEF)` — if CLEAR → `currState=8; sub_80A8FA0` (pause state, **no move, no check**).
     If SET → cutscene gates (`cutscene_checkOriginalCutsceneScriptPos_8036F40/58`) and
     `chatbox_mask_eFlags2009F38(0x80)`, then move `sub_80A8F48` + **catch check `sub_80A91A4`**.
  2. `sub_80A91A4`: if 0xBE4 set, compare `[obj+0xC]/[obj+0x10]` (s16.16 X/Y) vs
     `ReadOWPlayerObjectCoords()` with threshold 0x14<<16 (20 px); pass → `SetEventFlag(0xBE6)`.
  3. Tail `sub_80A91F4`: if 0xBE6 → `sub_8002E14(1)`; else layer-effect. Tail `sub_80A9218` →
     state dispatcher `sub_80A9430` (table off_80A9444 by `[obj+8]`; state 0 = spawn cloud sprite 0x6b).
  4. `sub_80A8FA0` (state 8): per-instance enable `sub_80A91D4` = `TestEventFlag(byte_80A91EC[2*obj+4])`
     with table `byte_80A91EC` = {0xBE9, 0xBEA, 0xBEB, 0} (tornado instances 0/1/2); enable SET →
     back to state 4; CLEAR → anim 0x19 + `currState=12` + sound + free (dissipation).
- Comp script phase `*(u16*)0x200031A`: flips 0x001a→0x0004 at **435934** on our suppressed walk
  (same beat as video). Comp state +0x16 = strikes; +0x3e := 1 at the flip; +0x38 timer := 45;
  +0x40/+0x46 = tornado 0 X/Y; +0x48 ≈ tornado 1 Y.
- **Our suppressed walk has 0xBEF CLEAR from ≤435920** (the script holds the tornado paused after the
  2nd strike). The video's tornado roams and grabs at ~435930-936, i.e. the video's 0xBEF was SET.
  THIS is the root divergence — everything visual downstream (roaming dark cloud, whirl, funnel on
  MegaMan) follows from it.

### What works now (probes steerdry8-14.py)
- Flag write that survives: clear 0xBE4 at natural-catch−2 (=435872), then at **435926** set
  0xBE4|0xBE5-preserving FA4 write + 0xBEA+0xBEF in FA5 (`0x200017d |= 0xa0`). Flags stick (f4=cc,
  f9=a0 verified through 435959). Arm written ≥435930 is wiped by the script phase change ~435934.
- With 0xBEF+0xBEA+0xBE4 set and the tornado slot-1 X/Y (+0xC/+0x10) poked onto MegaMan's
  (+0x1C/+0x20), **the catch check fires**: 0xBE6 sets at the pin's first frame ≥435933 (gate opens
  ~435933; pin 435934-936 → catch at 435935 — inside the video's ~435930-936 window).
- Carry motion `sub_80A93E8` is **deterministic** (fixed ±0x4000 bob of +0x14, 24-frame period,
  counters +6/+7) — no RNG in the carry itself.

### Why it still fails (cost traces, steerdry14.py)
- The video's "whirl" is **not** the tornado sprite teleporting onto MegaMan. Costs show the video's
  whirl *builds* 435930-935 (cost rises 2.2→5.3 while we sit still, because the video's t29 clouds
  converge on MegaMan at RNG-drawn spawn spots we don't share), and after our (correctly timed!) grab
  the cost caps at 6.00 from ~435938: our instant-teleported tornado + deterministic carry ≠ the
  video's converged cloud pictures.
- The t29 cloud spawn positions come from RNG draws that diverged **before the earliest ring state**
  (w_435100). The 14-worker RNG search (built-ins + |j|≤2000 patch search, walked back to W=435469)
  never beat 435929 — no single-event RNG patch reproduces the video's cloud layout.
- Every design tried: re-arm sweeps, staged hits, teleports at D+26..62, enable-flag variants,
  pin windows 435926-936 — all cap at 435929-938 with the same signature.

### Verdict
- 3 formal handler attempts used (tornado_clear pin, staged hit, sweep hit); every subsequent
  dry-run design (8+ variants) fails identically at 435929-938. Loss 435876 on map 0x83:0x01 is
  **unrepairable with poke handlers**; fixing it needs the pre-435100 divergence (not reachable —
  ckpt_0435000 lineage battles at 435565 and can't be used).
- Video brightness landmarks for any future retry: dip 131-135 @435882-898, 141-143 @435899-906,
  127-135 @435963-970, flash peak 172 @435974-976, LEFT turn @~435957; grab window ~435930-936;
  script catch 0xBE6-equivalent ~435964-966; phase flip 0x1a→0x04 @435934 (both machines).
- New probes: chkN.py (video beam N=1,903,380 — video does NOT end at the wall; wall is real),
  steerdry8.py (enable-flag variant), steerdry9/10/11/14.py (pin/arm/enable/0xBEF sweeps with
  flag+cost traces).
