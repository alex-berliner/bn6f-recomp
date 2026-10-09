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
