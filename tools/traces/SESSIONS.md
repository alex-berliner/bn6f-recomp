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
