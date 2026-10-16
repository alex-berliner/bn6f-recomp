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

## weather_comp_tornado_nocatch (2026-10-15, session 3)
The loss at 435876 (our tornado catches MegaMan ~91 frames before the video's 435967) is repaired by
handlers/weather_comp_tornado_nocatch.py, which stages BOTH catches at the video's own frames with RAM pokes
only:
- 435873: flag 0xBE4 cleared (no capture possible; MegaMan free-walks the video's path).
- 435959: 0xBE4 re-armed; 435965/435966: tornado slot 1 written onto MegaMan's own free-walk place +0x14
  south (from a gate-off free-walk replay); the game's own capture runs at **435967 = the video's frame**.
- 435968: 0xBEA|0xBEF (0x21) = the caught tornado dissipates; without it the catch re-fires every frame and
  the cutscene text loops (found this session; the reason every earlier staged run looked "late").
- 435975: 0xBEF cleared (the next tornado back to hunter state 4) and 0xBE4 cleared.
- 436394: 0xBE4 re-armed; 436400/436401: tornado slot 0 (now hunter state 4, isCaught-clear) written onto
  MegaMan's staged-run place +0x14 south; the capture runs at **436402 = the video's frame**; 436403 disarms.
Both captures verified frame-exact against the video's white flashes (435971-987, 436406-420).  The
comp-end cutscene after the second catch still runs ~180 frames late (its text waits consume the runner's
taps differently), so the reload fade lands late; the handler's mask window (thumb rows 30-75, cols 28-91)
covers the tornado sprite / text box / dark dip while the ground rows 76-79 stay compared (the camera
follows MegaMan, so the scrolling ground is his per-frame place seen through the picture), and the handler
checks MegaMan's per-frame place against its own two replays before returning the fix.  Key lessons that
cost the session: the input stream's A-bit is active-low (the box-skip window is 435966-435968, not
435969-435973), the thumb is 80x120 (early mask rows were nonsense), and a teleport-pinned tornado catches
only from hunter state 4 with 0xBE9 clear.
Deployed (the deploy also needed syms.py updated for the other session's eStruct2001c04/S20364C0 renames)
and the driver restarted: `wf vidsync-run --start run7n` -> run7o from run7n.1/ckpt_0435000 at 14:02.
Findings recorded to bn6f (wf record c29c7f9).

## Session 5 (2026-10-16, dedicated vidsync agent)

| time | furthest game frame | what happened | result |
|---|---|---|---|
| 08:30 | 435,876 (run7x stuck since 23:47) | audit fixes committed: ring entries past min(D, g) dropped + state_at skips off-path entries (3c008cd4); RNG search skips FREE windows (78d5a817); plugins with match() first, per-loss budget --budget-hours 4, failed handlers kept in checkpoints, RNG stage logs (6b66b492); status.py progress only from "game N video" lines (1ac3cd36); failed handler test restarts the driver + smoke test (c68d3c69); nocatch fade off-by-one (fb047b29); clutter probes moved to build-vidsync/old-probes | driver logic otherwise unchanged |
| 08:40 | 435,876 | **the loss re-read from pictures** (probes/postcatch.py, tornfit.py): the video's MegaMan is never caught here. At ~435930 his rainbow-orb ring closes around tornado 1 ("The Rainbow Power washed the typhoon away"), at ~436402 around tornado 0 ("...The rain's stopped!": the comp is won). The old nocatch handler staged two catches of MegaMan: wrong premise, retired | |
| 08:55 | 435,876 | two causes found: (1) the input display drops A for 1-2 frames (video 443917, 443929-30, 443938, 443944) while the trail is laid; any frame without A resets the trail (sub_809005C), but the video's trail never resets, so the display is wrong: bits_fixes.json corrects it (d5432e5b). (2) our tornado 1 sits paused at (48,232) on his path (160-frame pause since 435704) while the video's moves on and pauses at ~(1,204): one movement-state write at 435799 (moving, dir 3, 10 frames left, 0 segments) puts it on the video's track; with both, our ring captures it at 435930-932 as the video's and the capture cutscene matches (cost ~2) | new handler weather_comp_tornado_state (searches that write per tornado); test running from run7n.1/ckpt_0435000 |
| 09:20 | 435,876 | handler test (test-tstate-1): movement write to tornado 1 at 435769 syncs to 435999 (mean 1.60), short of the 150-frame bar; after the capture our game runs ~4 frames behind and tornado 0 (shown by the video near (-95,67) from ~436016) is off our screen | handler extended: placements (a tornado put near MegaMan, held paused; moving ones on blocked tiles loop forever in repickDirection) and a 100-frame bar, so the driver chains tornado 1, then tornado 0 |
| 09:58 | 435,876 | deployed (fa804f68 + working tree), `wf vidsync-run --start data/auto/run7n.1/ckpt_0435000 --watch-hours 8 --no-repair` -> run7y | |
| 10:18 | 435,999 | run7y fix 1: tornado 1 placed at (0,208), paused 128, at 435805 (the handler's pick; mean cost 1.67); next loss 435999 = tornado 0, handler searching | symbols/db entries for the orb ring written, record running |
| 10:19 | 436,362 | run7y fix 2 (built-in relocate): MegaMan +(6,-10) px at 435999 (our capture ran ~2 frames late, he walked on a little); synced to 436362, just before the video's second capture | handler did not match there (tornadoes held in state 8 by 0xBEF): fixed for the next restart |
| 10:29 | **438,428** | run7y fix 3 (tornado_state): tornado 0 placed at (-238,98), paused 199, at 436243; the second ring capture and "The rain's stopped!" play as in the video; **the comp-2 loss is past (+2,550 frames)** | departures in comp 2: A held where the display drops it (bits_fixes.json), tornado 1 placed at (0,208) at 435805, MegaMan +(6,-10) at 435999, tornado 0 placed at 436243 |
| 10:30 | 438,428 | next loss: comp 3 (map 0x83:0x02); the video's tornado catches MegaMan ("Uwaaaah!!") at ~438430, ours is elsewhere; handler searching | symbols/db: 6 ring routines + eWeatherCompState fields (650dbc05); bn6f renames pushed (fork/recomp-fixes 0b2a8a49); recomp's bn6f pointer not moved: the port no longer builds against bn6f HEAD because of the struct-field batches (oGameState_Unk_12 etc.), not these renames |
| 10:51 | 440,026 | run7y fix 4 (tornado_state): comp 3, tornado 0 placed at (244,181), paused 68, at 438341: it catches MegaMan as the video's does ("Uwaaaah!!"), mean cost 0.78; then fix 5 formation at 439316 (ordinary random battle); loss 440026 in that battle, built-ins working | pathcheck of the path through 438428: **holds to the end** (220 pokes) |

## 498358 (2026-10-16, benchmark session "originput")

| time | furthest | what happened |
|---|---|---|
| 12:18 | 498,358 | loss at a battle's start; `formation at 498269: entry 80b7254 syncs best to 498358 (0.644, next 0.646; the path has 80b7224: 0.664)` was logged, but the rule "clearly cheaper (< 0.85x)" rejected it: the scoring ended at D, inside the white transition |
| 12:40-16:25 | 498,358 | RNG search, wait, restart from before the encounter fix (deterministic repeat), escalate |
| 16:25 | | probe: entry 80b7254 written at 498269 gives the video's field (grass/poison panels, Piranha2, mystery data) for every RNG value: the field comes with the entry, not the RNG |
| 16:29 | | try_formations syncs past the loss (D+150) and ranks by reach (64e0dadc); restart via vidsync-tick (run8a) |
| 16:31 | | run8a: formations never ran: the ring began at the next multiple of 30 after the resume (498270), the trigger at 498269 out of reach; fixed: a ring entry at the resume point (1e481cab) |
| 16:43 | 498,700 | run8b fix 1: 80b7254 at 498269 holds to 498700; then a formation fix's own write was taken for a new trigger and 80b7264 (8 frames further, cost 2.09 vs 1.67) replaced it; fixed: reach wins by 60 frames only, triggers within 2 frames of the last write skipped (commit after 1e481cab) |
| 16:56 | 503,540 | run8c (from run8b fix_0001): hand fix, formation kept the path's entry at 500080, on to 503,540 |
