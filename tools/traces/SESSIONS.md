# Recorded play sessions

Each session is a trace recorded with `tools/record.sh NAME START.sav` and
checked with `tools/check_recording.sh NAME`: the replay reproduces the
recorded final state and ending save, and the C port is identical to the
original over the whole trace. A session starts from an earlier session's
ending save (`NAME.end.sav`), so they form a chain through the game.

| Session | Starts from | Ends at | Covers | Check | New code |
|---|---|---|---|---|---|
| menus2 | new game (blank save) | Central Town after the tutorial, saved | intro, tutorial, PET menus, saving | replay exact, port identical (23040 frames) | 2051 functions alone; 12.6% → 15.9% with the old routes |
| story1 | menus2.end.sav | jacked in at Central Area 1, saved | Central Town, PET/mail tutorial (NetBtlIntro), Lan's house, jack-in to Lan's HP | replay exact, port identical (9680 frames) | 1114 functions alone; 15.9% → 16.5% |
| battles1 | story1.end.sav | Central Area 1, saved (36 chips, 100z) | Mettaur virus battles, chips (Vulcan, AirShot, BusterUp, Sword…), Double Delete, folder edit, net shop, results screens | replay exact, port identical (20100 frames) | 2263 functions alone; 33.7% → 36.0% (with the 461-battle sweep) |
| story2 | battles1.end.sav | Central Area 2, saved (42 chips, 850z, play time 0:23) | next day (Lan's room), Gunner/Mettaur battles, chip-data transmission, WhiCapsl, mystery data, Central Area 2 | replay exact, port identical (33540 frames) | 2559 functions alone; 36.0% → 37.6% |
| story3 | story2.end.sav | Cyber Academy, Class 6-1, saved (HP 120) | walk to school, security bots, school halls, classroom scenes (fast-forward used) | replay exact, port identical (17560 frames) | 1075 functions alone; 37.6% → 37.8% (39.8% of code bytes) |
| story4 | story3.end.sav | Cyber Academy classroom as the fire starts (scripted disaster), saved | school day, classroom scenes, the fire breaking out | replay exact; the port diverged at game frame 32905 (the notification cutscene: 4 readable functions read TestEventFlag's answer from r0), fixed in 67673ff; then port identical (33700 frames) | 2370 functions alone; 40.6% → 40.6% (+11) |
| story5 | story4.end.sav | jacked in at RobCtrlComp2, saved (HP 120/140, 2250z) | the burning school, halls, jack-in to the robot control comp, virus battles | replay exact, port identical after 67673ff (52700 frames; before it diverged at frame 1120) | 2257 functions alone; 40.6% → 41.2% (+72) |
| story6 | story5.end.sav | RobCtrlComp2, saved (HP 110/140, 2450z) | RobCtrlComp virus battles (Mettaur, OldStove), folder edit, chip select | replay exact, port identical (36220 frames) | 2412 functions alone; 41.2% → 41.3% (+13) |
| story7 | story6.end.sav | Cyber Academy, Class 6-1, saved (HP 140/140, 3450z) | navi boss battle (WideSwrd), back in the classroom, cutscenes | replay exact, port identical (12600 frames) | 2051 functions alone; 41.3% → 41.3% (+8) |
| story8 | story7.end.sav | Central Area 2, saved (HP 140/140, 3450z, 1 bugfrag) | folder edit from the pack (LongSwrd, AreaGrab…), Central Area virus battles | replay exact, port identical (22480 frames) | 2333 functions alone; 41.3% → 41.5% (+22; 51.2% of code bytes) |
| menus3 | story8.end.sav | Central Area 2, saved again at the end (it also saved mid-route through the wireless features) | PET menu sweep: Key Items (first time), SubChip used from the menu, folder edits (swap, sort, folder/pack moves), Library tabs, Mail, every Network entry up to the cable/Wireless Adapter prompt; NaviCust not available yet | replay exact, port identical (12940 frames; port needed 15039 hardware frames to the original's 13581 here) | 2406 functions alone; 41.5% → 42.6% (51.9% of code bytes); 30 indirect call sites no route had fired |
| battles2 | menus3.end.sav | stopped at 20k frames; its last in-game save is battles2.end.sav | chip-variety virus battles in Central Area | replay exact, port identical (20600 frames; port 22682 hardware frames to the original's 20860) | 2328 functions alone; 42.6% → 42.6% (+3); 3 new indirect call sites: the battle code these chips use was already covered |
| story9 | battles2.end.sav | saved at the end | the story continued toward the next navi bosses | replay exact, port identical (38700 frames; port 43061 hardware frames to the original's 39053) | 2523 functions alone; 42.6% → 43.2% (+80; 52.3% of code bytes); 19 new indirect call sites |

## beastsp: a speedrun rebuilt from video (not recorded here)

`beastsp` is not a tools/record session: it is Komari's BeastSP run of BN6 Falzar (YouTube
`tAicOP_JS4A`) rebuilt frame by frame from the video, from power-on with a blank save, in
~/Code/vidsync (its `ready/README.md` says how). The video can't show the run's luck, so
`beastsp.poke` writes it in (RNG seeds, battle setups, rewards, and enemies' HP at 0 where the
video's battle ends), one line per game frame. It is long, and grows as the sync goes on: 220000
game frames (about an hour of the 8 h 44 min run) when it came in.

- Bring it in or refresh it: `tools/import_route.py [~/Code/vidsync/ready]`. It copies the trace,
  .poke and .sav, replays them on the original to the trace's `# end` digest, saves a checkpoint
  every 10000 game frames in build-checkpoints/beastsp/ (game RAM, never committed), profiles and
  observes call edges window by window from them (coverage/beastsp.prof, .edges), and sets the
  length in tools/wf/wf/routes.py. Then `python3 tools/lifter/golden.py update` (the edges changed).
- `wf capture` replays only the windows that run each function, from their checkpoints.
- The pokes need mgba_run's repeatable `--poke-at`, its game frame counter unwrapped past 65535
  and `--resume-game-frame` (the three vidsync patches, in since this route came in).
- New code: 3993 lifted functions alone, 1004 of them on no other lockstep route, 676 in no
  profile at all.
- Check: the replay reaches the trace's `# end` digest (hardware frame 223194) and the vidsync
  checkpoints byte for byte. The port is identical (3 isolated frames one frame early or caught
  mid-update) once each write lands by where the original's landed (`G:L` lines) and
  pet_getCurNaviChainedLookup_800A4E0 was taken out (it chose the regular chip from the wrong
  folder: the hand dealt at game frame 142918 differed). Lockstep takes about 12 minutes on it
  (the others take 10 s to 2 min), so it is in routes.py's LONG_ROUTES, which verify leaves out.
