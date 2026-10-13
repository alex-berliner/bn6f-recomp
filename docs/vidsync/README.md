# vidsync: the BeastSP route rebuilt from a speedrun video

vidsync rebuilds Komari's BN6 Falzar BeastSP speedrun (YouTube `tAicOP_JS4A`, BizHawk 2.4.2, 1,903,801
frames at 60 fps) as a recording for the route harness: a `gbarecomp-keyinput-v1` trace, a `.poke` file of
RAM writes and a start save, replaying on the original ROM. It is the `beastsp` route
(`tools/traces/SESSIONS.md`).

It moved here from `~/Code/vidsync` on 2026-10-13 (its git history stays there, up to commit `46ed417`).
What it learns about the game goes into bn6f and `symbols/db` like any other discovery (PROCESS.md §6), and
it reads its game addresses from bn6f by name (`tools/vidsync/syms.py`).

## Where things are

| What | Where |
|---|---|
| Scripts, emulator server source | `tools/vidsync/` (`emu_server.c`, `*.py`, `make_ready.sh`) |
| Probes (the repro commands of symbols/db entries) | `tools/vidsync/probes/` |
| Compute-PC job scripts | `tools/vidsync/remote/` (still run from the PCs' `~/vidsync` layout) |
| Game addresses | `tools/vidsync/syms.py`: bn6f's symbols and struct offsets; unnamed ones marked `TODO: name in bn6f` |
| emu_server, mgba_run | `make -C tools emu_server mgba_run` → `build-tools/` |
| Working directory | `build-vidsync/` (`VIDSYNC` in `tools/paths.env`, git-ignored): run the scripts from there |
| Data (55 GB: video, thumbnails, runs) | `build-vidsync/data` → `~/Code/vidsync/data` (`VIDSYNC_DATA`); never commit |
| Exported recording | `build-vidsync/ready/` (`VIDSYNC_READY`); `tools/import_route.py` reads it |
| Knowledge | `RANDOM_EVENTS.md` (every random decision, from code reading), `SYMBOL_DOSSIER.md` (everything learnt per symbol, with evidence; recorded into symbols/db and bn6f on 2026-10-13, kept as the archive) |

Environment overrides, as before: `VIDSYNC_ROM`, `VIDSYNC_BIOS`, `VIDSYNC_EMU`, `VIDSYNC_MGBA_RUN`,
`VIDSYNC_THUMBS` (`data/full/thumb.u8`), and `VIDSYNC_SYMS` (a `syms.py --json` file, for a machine without
bn6f).

## The parts

| Part | Files | Does |
|---|---|---|
| Video → features | `extract_thumb.py`, `feat.py` | 120×80 grey thumbnails of every video frame (`data/full/thumb.u8`, 18 GB); the input display read per frame |
| Emulator server | `emu_server.c` (mGBA core) | Steps the game, saves/loads states in slots, applies pokes at `main_awaitFrame` exactly as `mgba_run --poke-at` does, compares frames with video thumbnails (SIMD), `mask` command for stream overlays |
| Sync | `beam.py` | Beam search over keys and video offset: each game frame is matched to a video frame |
| Driver | `autosync.py`, `rngfix.py` | Runs the sync; when the video no longer matches (a loss), runs repair handlers, each a family of candidate RAM writes judged by syncing on. Checkpoints, logs, `loss_D.json`/`result.json` |
| Workers | `worker_server.py` | Candidate syncs on a second PC over TCP |
| Checks | `pathcheck.py` | Replays a result with plain steps: does the path hold against the video to the end |
| Export | `export_trace.py`, `segments.py`, `make_ready.sh` | `ready/`: the trace, pokes and save (verified with `mgba_run`), and save-file segments every ~20k frames for the port validator |

Repair handlers (`autosync.py`), in the order tried at each loss: formations (battle pointer), reward, video
offset, stream overlay, hand/folder, green mystery data, subchip expiry, position, relocate, Weather tornado,
map object, encounter, kill (enemy HP 0 when the video's battle ends), die (MegaMan HP 0), RNG writes at
random events (primary and secondary, widening), fixed-point fallbacks. A handler whose fix comes back to the
same loss is skipped there next time.

## State (2026-10-13)

- The sync stopped at game frame 432,728 (22.7%), run `run7k` (`data/auto/` on .175), at the Mr. Weather comp:
  the rainbow-orb trail and tornado outcome is not reproduced (`SYMBOL_DOSSIER.md` §4).
- `ready/` holds the recording to game frame 330,000 and 13 save-file segments with `VERIFY.txt`; the save
  chain holds except at seg 4→5. `tools/traces/beastsp.*` is at 220,000.

## The compute PCs

Heavy compute runs only on these two, never on the main machine.

| PC | Role | WSL home | Launch |
|---|---|---|---|
| 192.168.1.175 | driver (`supervise.sh` → `autosync.py`, 13 local workers), exports, segment cuts, pathcheck | `/home/alex/vidsync` | `powershell -File C:/Users/alex/launch_auto_175.ps1` (WMI, survives SSH) → `job_auto_175.sh` |
| 192.168.1.167 | worker server (15 workers, TCP 7777) | `/root/vidsync` | `launch_ws_167.ps1` → `job_ws_167.sh` |

Today the job scripts copy files from `C:/Users/alex/` into `~/vidsync/scripts`, run `make` and start. The run
name lives in `C:/Users/alex/runbase`; `status.sh` prints the furthest frame, the loss, and how long it has
been on it. `stop_auto.sh` also kills export and segment jobs. Keep the WMI launch: SSH sessions to Windows
kill their children.

## Next (from the transfer plan)

1. (Done 2026-10-13) The dossier went into symbols/db and bn6f; some renames are deferred (its section 3).
2. A deploy step (tar `tools/vidsync`, `tools/gamepaths.py`, `tools/paths.env`, `tools/mgba_run.c`,
   `tools/Makefile`, and a `syms.py --json` file; build there) and a `vidsync-run` workflow: restart from a
   checkpoint, watch progress ("stuck" = no progress for 30 min), hourly `pathcheck`, open a repair on a stall.
3. `vidsync-repair` (render the loss with the probes, classify, brief an outside model, test the handler from
   the loss checkpoint, record new symbols) and `vidsync-ready` (export, check `VERIFY.txt`, import the route).
4. Retire `~/Code/vidsync` after a full cycle from here (its `data/` stays where it is).

## Conventions and pitfalls

- **Never insert waits.** `iCurrFrame` decides encounter formations and mystery data, so the trace keeps the
  video's frame count.
- **Pokes go in at `main_awaitFrame` entry.** The `.poke` format is `GAME_FRAME ADDR=VALUE/SIZE,...` (hex).
  The harness unwraps the u16 game frame.
- **The battle seed is restored at phase 8** from 0x0203F4A4. A seed write in a battle intro must also write
  0x0203CBE4 and 0x0203F4A4.
- **Stream overlays** (sub/cheer alerts) cover the top of the video. They get windows with that area masked
  (`lenient` in results); `pathcheck` honours them and pads by 300 frames. The overlay handler accepts only a
  masked match under 1.6 (the mask can hide a battle that went another way).
- **Kill/die fixes past the loss** play the video's keys up to the deletion, add path nodes for those frames
  and leave a `free` window `pathcheck` skips. The deletion must lie inside the current battle; the fix needs
  a mean cost under 1.5.
- **Remote state uploads are keyed by content hash.** Pool workers restart every 100 tasks. `try_at` re-runs
  the chosen write locally (remote workers once returned stale results).
- **Segments:** checkpoints where MegaMan stands still on a map. The junction writes seeds, `iCurrFrame`,
  mystery data, `eStruct2001c04`, event flags, last map, MegaMan's object, and `eJoypad`+8..+0x17 (the
  key-repeat phase). Bringup is 584 frames (START at 400, A at 460).
