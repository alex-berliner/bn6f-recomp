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
| Compute-PC job scripts | `tools/vidsync/remote/`, deployed to the PCs by `tools/vidsync/deploy.py` (below) |
| Repair handlers written for a loss | `tools/vidsync/handlers/NAME.py` (interface: `handlers/__init__.py`; the vidsync-repair workflow) |
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
| Export | `export_trace.py`, `segments.py` (the vidsync-ready workflow) | `ready/`: the trace, pokes and save (verified with `mgba_run`), and save-file segments every ~20k frames for the port validator |

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

Heavy compute runs only on these two, never on the main machine: this machine sends commands and reads JSON,
logs and a few pictures back. The LAN is a slow WireGuard link: video never crosses it, each PC keeps its own
`~/vidsync/data`.

| PC | Role | WSL home |
|---|---|---|
| 192.168.1.175 | driver (`supervise.sh` → `autosync.py`, 13 local workers), exports, segment cuts, pathcheck, handler tests | `/home/alex` |
| 192.168.1.167 | worker server (15 workers, TCP 7777) | `/root` |

**Deploy** (`tools/vidsync/deploy.py`, or the deploy step of `wf vidsync-run`): the part of this tree vidsync
needs (`tools/vidsync`, `tools/gamepaths.py`, `tools/paths.env`, `tools/mgba_run.c`, `tools/Makefile`, the files
git tracks or would) plus bn6f's symbols as `syms.py --json` writes them and a `DEPLOYED.json` (commit, whether
it had uncommitted changes) go to `C:/Users/alex/vidsync_deploy.tgz` on each PC; `remote/install.sh` unpacks it
beside the old one, links the PC's data in (`bn6f/bn6f.gba`, `roms/gba_bios.bin`, `build-vidsync/data` →
`~/vidsync/data`), builds `mgba_run` and `emu_server` in `~/vidsync/deploys/STAMP`, and points the link
`~/vidsync/recomp` at it (running jobs keep their own tree; WSL1 on .167 cannot rename a directory in use; the
three newest deploys are kept). `remote/env.sh` sets `VIDSYNC_SYMS` (there is no bn6f
there) and works in `~/vidsync/recomp/build-vidsync`, so `data/...` paths mean what they meant before.

**Jobs.** Anything that must outlive the SSH session goes through WMI (`remote/launch.ps1`, copied to
`C:/Users/alex/vidsync_launch.ps1`): SSH sessions to Windows kill their children. Short commands go as a bash
script on stdin (`ssh PC wsl -d Ubuntu -- bash -s`: no quoting through cmd.exe).

| Script (`tools/vidsync/remote/`) | Does |
|---|---|
| `job_driver.sh RUN START [REPLAY]` | the driver as `data/auto/RUN.0, .1, ...` from checkpoint START; the run's name to `~/vidsync/runbase`, its start to `data/auto/RUN.start`, its process group to `~/vidsync/driver.pgid` |
| `job_workers.sh [VDIR]` | the worker server; process group in `~/vidsync/workers.pgid` |
| `job.sh NAME SCRIPT ARGS` | any script as a job: `~/vidsync/jobs/NAME.log`, `.done` (exit code), `.pgid` |
| `stop.sh driver\|workers\|JOB` | stops by process group (an export or a segment cut goes on when the driver stops) |
| `status.py [RUN]`, `status.sh` | the run's state as JSON (furthest game frame, alive, how it ended, the loss, the log's tail) / one screen |
| `newest.py RUN [MAXG]`, `loss.py RUN [D]`, `render.py` | the newest checkpoint; a loss with a checkpoint before it; the probes' pictures of a loss |
| `py.sh SCRIPT ARGS` | any `tools/vidsync` script with the PC's environment (`py.sh pathcheck.py X.json`, `py.sh probes/trig.py ...`) |
| `export.sh`, `segments.sh` | export a checkpoint (verified), cut segments |

By hand: `echo 'bash ~/vidsync/recomp/tools/vidsync/remote/status.sh' | ssh alex@192.168.1.175 wsl -d Ubuntu -- bash -s`,
or `./wf vidsync-status`.

## Workflows

| Workflow | Does |
|---|---|
| `wf vidsync-run --start CHECKPOINT\|RUN` | deploy to both PCs, restart the worker server and the driver (the next run name: run7k → run7l), watch: every 5 min the furthest game frame; stuck = it has not moved for 30 min (not log silence: the driver logs while it searches); hourly `pathcheck` of the newest checkpoint (a path that no longer holds restarts from a clean checkpoint 300 frames before its first loss, `pathcheck.py --save`); a stuck driver (or one that gave up at a loss) opens a `vidsync-repair`. `--no-deploy` without `--start` only watches; `--watch-hours H` ends the watch |
| `wf vidsync-repair --run RUN [--loss D]` | the loss rendered with the probes (`compare.png`: emulator beside the video before the loss; `video.png`: the video after it) → Claude classifies it (pause: `LossClass`) → a brief with the symbols/db entries and the routines' assembly → an outside model (tier/strong) writes `handlers/NAME.py` → Claude reviews (pause: `HandlerReview`) → test from the loss checkpoint on the driver PC's own workers (`autosync.py --handlers NAME --until D+600`, then `pathcheck`) → Claude records what was learnt (pause: `Findings`; the `record` workflow) → commit, deploy, restart past the loss |
| `wf vidsync-ready [--checkpoint C\|RUN] [--import]` | export (verified with mgba_run and emu_server), cut segments, check every segment follows the run (long segments and save-chain breaks reported), copy here (`build-vidsync/ready`); `--import` runs `tools/import_route.py` and lockstep on `beastsp` (that replays the route on this machine, so it is asked for) |

## Next (from the transfer plan)

1. (Done 2026-10-13) The dossier went into symbols/db and bn6f; some renames are deferred (its section 3).
2. (Done 2026-10-13) The deploy step and the vidsync-run, vidsync-repair and vidsync-ready workflows (above).
3. Retire `~/Code/vidsync` after a full cycle from here (its `data/` stays where it is).

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
