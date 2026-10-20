# Orphaned branches of alex-berliner/bn6f: audit (2026-10-20)

The fork's `master` was replaced by `recomp-fixes` (`1189566c`). This audit asks what the other
branches hold that `master` lacks, and prepares what is worth keeping. **Nothing here changes
`symbols/db`, `bn6f` or `cport`.** The deliverables are proposals in `ingest/`, for the owner to
adopt or reject.

## Results

| | |
|---|---|
| **Renames proposed** | **136**: 135 labels and 1 struct field. All are ROM-neutral: applied together in a scratch copy of bn6f, the build is still SHA-1 `0676ecd4d58a976af3346caebb44b9b6489ad099`. Confidence: 50 high, 55 medium, 31 low. |
| **Struct fields to add** | 4 `Struct2035280` fields master leaves unnamed. ROM-neutral in the same build. |
| **New symbol entries** | 170 drafts in `ingest/symbols_db/`. They pass `tools/symdb.py`'s own entry checks against the renamed scratch ELF: 0 problems, no duplicate names, every `reproduce` path exists. |
| **Evidence for existing entries** | 31 placeholder addresses already have a `symbols/db` entry. Their branch evidence is in `ingest/evidence_for_existing/`, to append if wanted; the existing entries are untouched. |
| **Aliases and disagreements** | 393 places where master already has a descriptive name and a branch used another: 364 labels and 29 struct fields, in `ingest/aliases.tsv`. No change made. |
| **Notes, constants** | 245 comment blocks (`ingest/notes.tsv`) and 59 named constants (`ingest/constants.tsv`), all from `bn-notes`, none in master. |
| **Tooling and docs** | Nothing qualified for `ingest/patches/`: no candidate applies cleanly, builds and has a use today. Every candidate is listed in `ingest/patches/README.md` with its hash and the reason. |

Where the knowledge sits:
- `optimizations` and both `worktree-agent-*` branches are **contained in `old-master-2026-07-12`'s
  history**; none has a commit of its own.
- Their tips, and old-master's tip, build a ROM whose symbol table is **identical to their merge
  base** (`37de3e85`), so none of them renamed anything.
- Symbol knowledge lives in two places:
  1. `bn-notes`: renames, notes, constants and struct fields from the `bn` reimplementation project.
  2. The C translations of the shelved decomp effort, on old-master's history before `31544bfe`
     ("Clean-slate reset").

## Method

- **Set-up.** Clone, `git submodule update --init`, fetch every fork branch,
  `tools/build_toolchain.sh`. The baseline build needed two steps the task's command leaves out, both
  as in `build.sh`: `make -C tools/gbagfx` and `make assets`. Without them the build stops at
  `data/textscript/compressed/*.s.lz: file not found`. `bn6f.sym` is made only by `make syms`.
  With those steps: `bn6f.gba: OK`, SHA-1 `0676ecd4d58a976af3346caebb44b9b6489ad099`.
- **Inventory.** `git cherry -v origin/master origin/BRANCH`, `git log --oneline origin/master..origin/BRANCH`
  and `git diff --stat MERGE_BASE origin/BRANCH` for each branch. No commit of any branch is already
  on master by patch id: `git cherry` marks none `-`.
- **Matching by address.** Every branch tip and both merge bases (`37de3e85`, `d57c1968`) were built
  in scratch worktrees; all build the original ROM. Every symbol of each ELF (`objdump -t`, with its
  section) was then joined to master's ELF by (address, section). A name the branch added is thus
  tied to whatever master calls that address today, whatever either side renamed since.
  Struct fields were joined by (struct, offset) the same way, from their `*ABS*` symbols.
- **Notes.** Every comment line `bn-notes` added was attached to the label it documents (a block
  directly above a label belongs to it; a note inside a body belongs to the enclosing function; row
  comments inside a table belong to the table). Each was resolved to an address with the branch's
  ELF and blamed for its commit.
- **Evidence.** The branch's own evidence for each rename is `docs/renames.md`, which its last commit
  deleted. It was read from `a5c3b9fe^`.
- **Drafting.** For every placeholder, the branch's evidence and master's assembly at the address
  were read side by side (five parallel drafting passes, each checked to return one record per
  symbol). The draft says whether the code agrees: 175 of 200 yes, 20 partly, 5 no.
  - **No:** no rename. The entry keeps the current name, status `hypothesis`.
  - **Partly**, where the name would mislead (6 cases, e.g. `ChargeShotHandlersByTransformation`
    for a table five dispatchers index by other keys): no rename, knowledge kept.
  - **Other partly cases:** confidence one step lower.
- **Name checks.** Every proposed name went through the project's own rules
  (`tools/investigate_land.py` `style_problem` and `address_digit_problem`: lowerCamel after one
  `prefix_`, no Maybe, no `0x`, no digit run copied from the address). It also had to be absent
  from `bn6f.elf`/`bn6f.sym` and `symbols/db`, absent from `cport/` (`grep -rw`), unique among the
  proposals, and not on a function named like an `asm/` include file. The checks were tested by
  feeding them known-bad names.
- **ROM check (step 4).** In a scratch worktree of bn6f at `origin/master`, the final 135 label
  renames were applied as whole-word replacements (599 replacements in 29 files). The struct-field
  rename and the four `Struct2035280` fields were added, then a full rebuild.
  - The SHA-1 is unchanged, every new name sits at its address, and no old name remains.
  - `tools/rename_symbol.py` was not run.
  - The scratch worktrees were removed afterwards.

## Branch `bn-notes` (32 commits plus 3 merges, 2026-09-06 to 09-25, base `d57c1968`)

Notes and renames from the `bn` reimplementation (a Rust port of the battle engine, outside this
repo; on the machine that ran this audit it is at `/home/box/Code/bn`). The tip `a5c3b9fe` rewrote
the notes down to facts about the game and deleted `docs/renames.md`.

| Group | Class | Why | Prepared | Skipped, and why |
|---|---|---|---|---|
| Labels at addresses master still calls `sub_`/`byte_`/`off_`/... (164 addresses: 145 renamed by the branch, 19 only noted) | (b) | names with stated roles, evidence in `docs/renames.md` | 150 drafted: 135 renames in `proposed_renames.tsv` (one, `EmotionFaceSlots`, named from the branch's note because the branch kept the placeholder) and 4 documented without a rename. 119 become new entries; the evidence for 31 addresses with an existing entry goes to `evidence_for_existing/` | 11 branch names rejected: 5 contradicted by the code, 6 that would mislead (see open questions). The notes at 14 addresses sat on pool words or belonged to a neighbour; they are kept in `notes.tsv` only |
| Label renames where master already has a descriptive name (364 labels) | (b) | the branch and master named the same address independently | `aliases.tsv`, with the branch's evidence | no change (task rule) |
| Notes (245 comment blocks, in asm, data and include files) | (b) | facts about what code does | all in `notes.tsv` by address. The ones on drafted symbols are summarised in the entries | not copied into bn6f comments: master's comments are generated from `symbols/db` (`tools/symdoc.py`) |
| Struct fields: D10 (`4ca08bf6`) and RenderInfo (`e343dee1`, `0d5c5f33`), 29 offsets master names | (b) | field roles | `aliases.tsv` (struct-field rows) | no change; several are real disagreements (open questions) |
| Struct field `oS20364C0_Extra_Unk_40` → `WindowFrameCounter` | (b) | master still calls it Unk_40 | rename, entry `ingest/symbols_db/oS20364C0_Extra_WindowFrameCounter.yaml` | |
| 4 `Struct2035280` fields (`CustGaugeFlowCounter` +0, `ChipWindowSlideX` +0x12, `CustGaugeValue` +0x20, `HudElementMask` +0x40) | (b) | struct layout; master has no field there | rows in `proposed_renames.tsv` (decision `add field`), ROM-neutral | no entry drafted: an entry needs the field to exist |
| 59 constants (`BATTLE_SEQ_*`, `BATTLE_FSM_STATE_*`, `METTAUR_*`, `GUNNER_*`, `CUST_GAUGE_*`, ...) | (b) | named magic numbers, many measured | `constants.tsv` (value, where defined on the branch, commit, comment) | not applied: using them means replacing literals at each use site, not a whole-word rename |
| `docs/decomp/*.c` edits | (a) | the same renames carried into the pseudo-C docs | nothing | covered by the renames |
| `docs/renames.md`, `tools/field_names.py` (added, then deleted by `a5c3b9fe`) | (c) | the evidence for the renames | used as evidence (`git -C bn6f show a5c3b9fe^:docs/renames.md`) | not restored: it describes the other project's process |
| `tools/analyze_source*`, `tools/misc_scripts` one-line edits, `maps/*/warps_gfx_anims.s` | (a) | follow-ups of the renames | nothing | |

## Branch `old-master-2026-07-12` (350 commits plus 3 merges, 2026-01-24 to 07-12, base `37de3e85`)

The tip moves every asm and data file under `src/asm/` without changing it, so its ROM and symbols
equal the merge base's. Its first 177 commits are `optimizations`. `previous_work.md` at the tip maps
the shelved work: base `adc8595b`, last commit before the reset `e6bb4969`.

| Group | Class | Why | Prepared | Skipped, and why |
|---|---|---|---|---|
| C translations of 534 functions (`src/c/`, `.ifndef DECOMP_*` trampolines, `tools/decomp_manifest.txt`), up to `e6bb4969` | (d), with a (b) part | the branch shelved them itself (`31544bfe`); the project's lifted port supersedes the approach | 50 entries for functions master still calls `sub_*` and that have no `symbols/db` entry (status `probable`, no rename; the C is the evidence) | 426 already have an entry (351 with readable C in `cport`); 58 already have a descriptive name |
| Commit one-liners of the conversions ("Convert sub_80AAB88 — 40-byte NOR-copy") | (b) | what the code does | in those 50 entries | |
| `constants/headers/*.h` (generated C structs), `tools/struct_inc_to_h.py` | (a) | `tools/abi/gen_structs.py` does this now | nothing | |
| `tools/bn6f-track`, vendored `tools/libmgba`, `tests/` (17k files: bk2 fixtures, harness scripts), lockstep, per-patch videos | (d) | the branch wiped it (`00120367`); `tools/lockstep.sh`, `tools/oracle.py` and `tools/traces` cover it | nothing | |
| `tools/bn6f-validate`, the canary | (d) | removed at the reset | nothing | |
| `optimizations/*.md`, `.claude/` artefacts (commands, cron history, decomp timings) | (d) | about the shelved harness | nothing | |
| `issues/` (concerns, `decomp-blockers.md`), `docs/pitfalls*` | (c) | gotchas of the shelved toolkit | nothing | `1a9ff518` and `d2927bcb` apply cleanly but have no use for the lifter-based port |
| `cyow/` reviews (origin classification, agbcc ABI, history of the asm split, ...) | (c) | design review of the shelved approach | nothing | `017880f4` and `4b822ba6` conflict (`cyow/00-index.md`). See the open questions for the origin classification |
| Clean-slate reset, repo layout under `src/` `ld/` `scripts/` (`31544bfe`, `9dff6d5f`, `3697ce7f`) | (d) | master keeps the flat layout | nothing | |
| Removal of IDA-era tools (`41392a07`) | (c) | cleanup | nothing | applies cleanly, but `bn6f/docs/documenting/*.md` still points at `tools/doc_scripts` |
| Harness v2 (`tools/harness`, Rust: B0-B3, frame-hash oracle, pilot recorder, profiler bricks 1-3), `tools/mgba` submodule, `make setup-toolchain` | (c) | emulator harness and profiler | nothing | superseded by `tools/oracle.py`, `tools/lockstep.sh`, `tools/profile.sh`, `tools/lineprof.py`, `tools/coverage.py`; the commits conflict outside the harness tree |
| CI workflow (`74950c78`, `9cd3a637`) | (c) | ROM SHA check on push | nothing | `74950c78` applies cleanly but runs `make setup-toolchain`, `make funcmap` and `cargo test` in `tools/harness`, none of which exist on master; it would fail |
| `fixtures/*.inputs` (boot to bedroom, boot to living room) | (c) | input recordings | nothing | `tools/traces` already has routes from power-on |
| `docs/development_plan.md` v1/v2, `previous_work.md` | (c) | plan for the shelved harness | nothing | v1 (`b7740580`) applies but v2 supersedes it, and v2 conflicts |

## Branch `optimizations` (175 commits plus 2 merges, to 2026-05-25)

| Group | Class | Why | Prepared | Skipped, and why |
|---|---|---|---|---|
| Everything | covered by old-master | every commit is an ancestor of `old-master-2026-07-12` (`git rev-list old-master..optimizations` is empty) | see old-master | its tip builds the original ROM with the merge base's symbol table: no renames |

## Branches `worktree-agent-a19a9926f93f4ac9c` (90) and `worktree-agent-ade579d7401977f6e` (88), 2026-05-24

| Group | Class | Why | Prepared | Skipped, and why |
|---|---|---|---|---|
| Everything (agent worktrees of the decomp effort: conversions, `structs_plan.md`, `token_todo.md`) | covered by old-master | both are ancestors of `optimizations`; their 3 and 1 commits not on each other are on `optimizations` | see old-master | tips build the original ROM, symbols identical to the merge base |

## Branch `ida` (2018)

| Group | Class | Why | Prepared | Skipped |
|---|---|---|---|---|
| Everything | (a) | 0 commits not in master | nothing | nothing |

## The files in `ingest/`

| File | What |
|---|---|
| `proposed_renames.tsv` | One row per decision, renames first. Columns, in the task's order: `address`, `current_name`, `proposed_name`, `source_branch`, `source_commit`, `evidence`, `confidence`. Then `kind`, `decision` (rename / no rename and why / add field), `branch_name`, `strict_style_alt`, `status`, `code_check` (what master's assembly shows), `existing_entry` (a `symbols/db` entry already at that address, and any rename `symbols/pending_renames.txt` queues for it), `rom_check`. |
| `symbols_db/*.yaml` | 170 new entries in `symbols/db/README.md`'s format. A renamed symbol's file and `name` are the proposed name, with `previous_names: [placeholder]`, so they become valid when the rename is applied; the others are valid now. Each entry has the branch's evidence (with a `git -C bn6f show` reproduce command), this audit's reading of master's code, and a `runtime` item where the branch says it measured the behaviour. That item says plainly that the measurement was not repeated. |
| `evidence_for_existing/*.yaml` | The same text for the 31 addresses whose entry already exists. Not db entries: material to append. |
| `aliases.tsv` | 393 rows: master's current name, the branch's name, the commit, the branch's evidence. |
| `notes.tsv` | The 245 note blocks by address and current name, with the file and line on `bn-notes` and the commit. |
| `constants.tsv` | The 59 constants. |
| `patches/README.md` | The tooling candidates and why none was taken. |

Every `reproduce` command first runs `git -C bn6f fetch origin bn-notes` (or
`old-master-2026-07-12`), because the submodule's default fetch does not bring those commits.

## Open questions for the owner

1. **Keep the old branches.** The evidence is reachable only through `bn-notes` (`a5c3b9fe`, and
   `a5c3b9fe^` for `docs/renames.md`) and through old-master (`e6bb4969` for the C). If they are
   deleted, the `reproduce` commands stop working.
2. **Name style for data and RAM.** The task says `prefix_lowerCamelName`. Master's data and RAM
   names follow another convention (`eGameState`, `ChipDataArr`, `OverworldMapObjectUpdateJumptable`),
   and so do the branch's. The proposals follow master (e.g. `BannerSequencerStates`,
   `eBattleSequencerState`); the strict form is in `strict_style_alt` (e.g.
   `battle_bannerSequencerStates`, `battle_sequencerState`). Functions all use `prefix_lowerCamel`.
3. **Names that only restate a table position** (31 low-confidence renames): `battle_chipWindowState28`,
   `battle_fsmState08`, `battle_bannerSeqState1C`, `BattleBackdropTiles0` to `6`, and so on. The
   digits are state values or frame indices, not the address, so the project's rules allow them.
   They say where a handler sits, not what it does. For four of them the code shows a role a better
   name could use: 0x1C is the paused-battle state, `sub_8009338` opens and waits on the custom
   screen, and `sub_8009552` and `sub_8009594` are tutorial text states.
4. **Gunner states 4-7** (`sub_81130E4`, `sub_81130F0`, `sub_81130FC`, `sub_8113108`). The branch
   calls them materialize steps. Master sets CurAction 4, 6 and 7 from
   `object_tickStatusEffectTimers_800E730` and 5 from `battle_801B1C4`, so they look like status and
   hit reactions, and they call four different shared routines. No rename; `hypothesis` entries.
5. **`sub_810A204`.** The branch says it arms the Mettaur's GUARD. The code writes 0xC to
   `oAIState_ScriptId`, back to choose-attack, and never sets CurAction 0xC. Guard is entered from
   `sub_810A004` instead (Version > 0). The same note's claim that the confused roll is the AI's only
   RNG use is also wrong: `sub_810A254` and `sub_810A1B4` call `GetPositiveSignedRNG`.
6. **Names dropped as misleading**, all with knowledge kept in their entries:
   - `ChargeShotHandlersByTransformation` (`off_80117D4`): indexed by BButton, BPwrAtk,
     NaviBonusIndex, BLeftAbility and more.
   - `NaviBaseHpByRow` (`byte_80210DD`): each 16-byte row holds a navi's full base stats.
   - `battle_shotImpact` (`sub_80F2180`): a phase of a player charge action that never touches a shot.
   - `ePAScratch` (`dword_2033000`): also used by folder shuffling and chip drops.
   - `eSelectedChipCodes` (`byte_20366C0`): the label starts a larger record.
   - `ObjPaletteBlendModes` (`off_3005F20`): the blend runs on any palette buffer.
7. **The RenderInfo fields.** Master names `oRenderInfo` +0x0A..+0x1A `DisplayControlBits`,
   `RenderEntryCountA/B`, `BGScrollXOffset`, `BGScrollY`, ... The branch (`e343dee1`, `0d5c5f33`)
   says they mirror the BG0-3 scroll registers and BG3 control, which it measured. These are not
   aliases: one side is wrong. Similar for `oAIState` +0/+2/+3/+8 (master `ScriptId`/`SubState`/
   `AttackDecisionFlag`/`Timer`, branch `DecideState`/`DecideSubState`/`DecideLatch`/`WanderWait`)
   and `oAIData_Unk_68` (master `TempAttackObjectPtr`, branch `BusterBarrelPtr`).
8. **Descriptive names that contradict the branch.** Among the 364 label aliases are clashes of
   meaning, not just of wording. Examples:
   - `0x0802C044`: master `battle_chipSelectAnimTileStep`, branch `revealResultReward` (the result
     window).
   - `0x08109DEC`: master `aiAttackVars_runKeyItemTrapStep`, branch `MettaurAttackSwing`.
   - `0x080081A4`: master `battle_runVictorySequence_80081A4`, branch `bannerSeqState0CWinCount`.
   - `0x08008064`: master `battle_runPerFrameStoryStep_8008064`, branch `bannerSeqState04BannerWait`.

   165 alias rows carry the branch's evidence line, a place to start.
9. **Renames that collide with the project's own queue.** 28 proposed renames sit on addresses that
   already have an entry, and several of those also have a different name queued in
   `symbols/pending_renames.txt`. For example, `sub_8026A28`: queued `stateMachine_runStep_8026A28`,
   branch `battle_isChipWindowReady`. See `existing_entry` in the TSV.
10. **Constants.** Adopting the 59 constants means editing the literal operands where the branch
    used them (its commits show each use site). Many carry measured values (frame counts, gauge
    periods).
11. **Notes on symbols that already have names** (the `notes.tsv` rows with `in_drafted_entry = no`).
    These could become evidence in those symbols' existing entries. That is a change to existing
    entries, so this audit left it.
12. **Origin classification.** Old-master `017880f4` has `tools/classify_origin.py`, a heuristic that
    finds about 96% of functions compiler-generated and lists 20 hand-written ones, concentrated in
    `start.s`, `libs.s` and `asm38.s`. It could help choose what to port by hand. Not taken: the
    commit conflicts, and the heuristic was not checked.
13. **CI for the fork.** Master has none. The old workflow cannot be reused as it is (see old-master).
    A short workflow running `make assets` and `make` with the recomp toolchain would be new work,
    not part of this audit.
14. **The task's baseline command** needs `make -C tools/gbagfx`, `make assets` and `make syms`
    added, as `build.sh` does.
