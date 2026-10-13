# Symbol database

One YAML file per symbol of the bn6f disassembly whose meaning this project
established, confirmed or changed. The disassembly carries the result (names,
doc comments); this database carries how we know it, so every claim can be
checked and reproduced. `tools/symdb.py` validates it against the disassembly
and renders `symbols/SYMBOLS.md` (index) and `symbols/symdb.sqlite` (queries).

This file is the specification of an entry. `tools/symdb.py check` enforces the
parts marked **checked** (a problem fails the check and the commit gate); the
parts marked **warned** are reported but do not fail yet.

## Example

```yaml
name: sprite_queueOamEntry     # current name in bn6f (must exist, at `address`)
address: 0x030068E8            # where it lives (IWRAM routines: their run address)
kind: function                 # function | data | ram | constant | struct-field
previous_names: [sub_30068E8]  # every earlier name, oldest first
status: confirmed              # confirmed | probable | hypothesis
summary: One line, in game terms.
interface: >-                  # functions: registers in and out, flags, side effects
  (r0 = attr01, r1 = attr2, r2 = layer, r3 = list position) -> r0 = 0xFF if queued
details: |                     # what it does, structures it touches, quirks
  ...
evidence:                      # each way the meaning was established or checked
  - method: assembly-reading   # see "Methods" below
    date: 2026-10-08
    detail: what was found, concretely
    refs: [bn6f/asm/asm38.s:1234]  # files/lines, other symbols
    reproduce: |               # exact commands (paths relative to the recomp repo)
      ...
    result: what the command showed (numbers, verdicts)
changes:                       # what this project changed because of it
  - date: 2026-10-08
    what: rename sub_30068E8 -> sprite_queueOamEntry; doc comment at the definition
    where: bn6f recomp-fixes <commit>; recomp <commit>
    tool: tools/rename_symbol.py
readable_c: cport/src/game/sprite_oam.c   # if a readable version exists
coverage: [battle, wander, story1]        # recorded routes that execute it (lockstep checks it there)
related: [sprite_queueOamEntries, iSpritePool]
open_questions: []
sources: [session 2026-10-08]  # where the work happened (session, commit, tool run)
stamp:                         # readable functions only: what the readable C was made from
  rom: rom:ba2da9126a2aad7d
  range: 0x030068E8-0x03006920
  contract: r0 r1 r2 r3 r4 r5 r6 r7 lr -> nz c v r0 r4 r5 r6 r7; clobbers -
  callees: {}
  headers: e02f5271a0f7
  lifter: 20e11122b33e
  prompt: legacy
  model: hand (Claude Opus 5.5)
  run: claude session_01MV9x1XXyanCy3FkvpEo7b7
  body: e58fd80bf5ab1d59
  installed: ec5c0ac
  edited:
  - b389d9b
  - ec5c0ac
  stamped: '2026-10-10'
```

## Fields

File: `symbols/db/NAME.yaml`, named after the symbol's current name (**checked**;
`tools/rename_symbol.py` moves it on a rename). Keys appear in the order above;
tools that rewrite an entry (`rename_symbol.py`, `symdoc.py`) keep the order.

| Key | Required | Meaning and rules |
|---|---|---|
| `name` | yes (**checked**) | The current name in bn6f. Must be a symbol of `bn6f/bn6f.elf` (except `struct-field`), unique in the database. |
| `address` | yes (**checked**) | Hex string. Must equal the ELF symbol's address (Thumb bit ignored). IWRAM code: its run address. |
| `kind` | yes (**checked**) | `function`, `data`, `ram`, `constant` or `struct-field`. |
| `previous_names` | if renamed | Every earlier name, oldest first. None may still exist in bn6f.elf (**checked**). The first one is the symbol's *root*, which stamps and hashes use so renames change nothing. A rename back to an earlier name drops that name from the list. |
| `status` | yes (**checked**) | `confirmed`, `probable` or `hypothesis` (see "Status"). |
| `summary` | yes (**checked**) | One line in game terms. Rendered into `SYMBOLS.md`. |
| `interface` | functions | Registers in and out, flags, side effects; the C signature once typed. |
| `details` | no | What it does, structures it touches, quirks. |
| `evidence` | yes (**checked**) | A list; each item needs a known `method` and a `detail` (**checked**), see below. |
| `changes` | no | A list of `{date, what, where, tool}`: what the project changed because of this entry (renames, comments, readable C). `where` names commits (`bn6f <hash>`, `recomp <hash>`). |
| `readable_c` | if readable | The `cport/src/game` file with its readable version. Must exist (**checked**). |
| `coverage` | no | Recorded routes (`tools/traces`) that execute it; lockstep checks it there. |
| `related` | no | Other symbols worth reading with this one: chosen by hand, or the other functions of a subsystem converted together. Never a whole farm group (unrelated small functions converted in one run): of its members only those that call this function or that it calls (direct call sites in `build-abi/graph.json`, `tools/funcindex.py`) are listed, so usually none and the key is left out. The rest of the group is the run, in the evidence and `sources`. |
| `open_questions` | no | What is still unknown. |
| `sources` | no | Where the work happened: a Claude Code session, a workflow run id (`tools/wf run convert-20261009-035817`), a commit (`recomp 1a2b3c4`, the one the run produced). A run's text logs stay in `build-wf-archive/RUN` after its folder in `build-wf-runs` is pruned (`./wf archive`). |
| `abi` | not yet | The typed C interface: the single source for the adapter, typed wrapper, difftest contract and bn6f signature comment. Format: `docs/C_CALLING_CONVENTION.md` §2 (`c`, `args`, `ret`, `globals`, `keep`, `convention`, `evidence`). No entry has one yet, and `symdb check` does not validate it yet. |
| `signature` | functions (generated) | The knowledge layer's proposed C signature (docs/reviews/2026-10-12-knowledge-first.md): `c` (the prototype), `params` (each `{reg, type, name}`: the register it arrives in, a C type, a bn6f struct pointer where captured calls pointed at one), `ret` (`{reg: r0 \| r0:r1 \| flag:z \| none, type}`), `globals` (`toolkit` = r10), `keep` (registers lifted callers still read, for an adapter), `confidence` (`confident`: the mechanical pass and an outside model agree, or the mechanical pass alone settles every register and type; `unsure`: they disagree or nothing decides; `none`: returns elsewhere than its caller), `decided_by`, `evidence` (one line per source: contract bounds, liveness, captured calls, bn6f comment, readable C, the model's reason, disagreements), `reproduce`, `derived`. Written by `tools/sig/write_db.py`, which replaces the block and changes nothing else; every function (with or without an entry) is also in `symbols/signatures.tsv`. A proposal, not the installed interface (that is `abi`). |
| `stamp` | readable functions | See "The stamp" below. |

### Evidence items

| Key | Meaning |
|---|---|
| `method` | One of the methods below (**checked**). |
| `date` | When it was done. |
| `detail` | What was found, concretely (**checked**: not empty). |
| `refs` | Files and lines, other symbols, commits. |
| `reproduce` | Exact commands, runnable today from the recomp repo root (a `cd` inside the command is followed), with symbols under their current names. Every path they name must exist (**checked** in the entries a commit touches, **warned** in the others; generated build output under `build*/`, such as `cport/build-host/difftest`, is not counted, since the commands make it). Never a `/tmp` path (**checked** the same way): those folders are gone after a reboot; a command's scratch output goes under `build/repro/`. The forms in use are below the table. |
| `result` | What the command showed: numbers, verdicts, the run id or commit it was run at. |

The `reproduce:` forms (`./wf` is the workflows' launcher at the repo root; `build-snapshots` is the
store of the original game's captured calls, `build-snapshots/FUNCTION`, which `./wf capture` fills):

| Evidence | `reproduce:` |
|---|---|
| difftest, cpu-oracle, lockstep of a converted function | `./wf capture --function NAME`, then `./wf verify --function NAME --snapshots build-snapshots` |
| readable-c, a farm group | `./wf convert --subsystem 'small functions from the model farm, set N' --file leaves_farm_N.c NAME`: this function alone, though the run converted the group together. The group is the entries whose readable-c evidence cites the same run in its `detail`. |
| readable-c, a subsystem | the whole convert command: `./wf convert --subsystem WHAT --file FILE.c F1 F2 ...` |
| readable-c, one function from the farm | `./wf farm --model PROVIDER/MODEL --snapshots build-snapshots NAME` |
| coverage | `grep -l '^ADDRESS ' tools/traces/coverage/*.prof` |
| captured calls of a hand-made check | the capture command itself, into `build/repro/` (a replay of a recorded route is deterministic, so it makes the same calls again): `tools/convert/capture.sh build/repro/snaps tools/traces/ROUTE.trace G0 G1 N NAME`, then e.g. `python3 tools/oracle.py --base build/repro/snaps ... --out build/repro/oracle NAME` |

`tools/symdb_rewrite.py` (review 2 Area 6, joint plan S1) brought the older entries to these forms
on 2026-10-10: the `cd tools/wf && .venv/bin/python -m wf` spelling became `./wf`, the farm-group
commands and `related:` lists that repeated a whole group (half the database) became the forms
above, and the `/tmp` folders became `build/repro/`. It changes nothing else in an entry, a second
run changes nothing, and `--verify REV` shows that every other field is as it was at REV.

Methods: **assembly-reading**; **caller-analysis**; **cpu-oracle** = the original (mGBA, real BIOS) and the port run
from the same real-memory entry states (tools/oracle.py); **difftest** =
the port's readable version against its lifted original on random or
captured inputs (cport/host/difftest); **lockstep** = whole-game replay of
recorded play against the original (tools/lockstep.sh); **coverage** = which
recorded routes execute it (so lockstep checks it there); **readable-c** = a
readable version exists (say who wrote it: model and run, or by hand);
**library-match** = the same code as a known library (m4a, librfu);
**data-inspection**; **hardware** = observed on a real GBA.

Status: **confirmed** = established from the code and checked against the
original (oracle, lockstep or a readable replacement verified both ways);
**probable** = read from the code and consistent with every use seen, not
independently checked; **hypothesis** = a guess with partial evidence (no
rename; comment only, marked as a guess).

## The stamp

Every readable function (a `Next NAME(void)` body in `cport/src/game`, 1,435 of
them on 2026-10-10) has one `stamp:` in its entry, saying what its readable C
was made from (joint plan S1, docs/reviews/2026-10-10-4-joint-plan.md). An IWRAM
copy the lifter names `NAME_iwram_ADDR` keeps its stamp in `NAME`'s entry, with
`function:` naming it. `tools/stamps.py` computes the fields; all values are
strings (so tools that re-dump entries keep them as they are), `callees` a
mapping of strings.

| Field | What it holds | Source |
|---|---|---|
| `function` | Only when the readable function is not the entry's own name. | |
| `rom` | A hash of the function's original bytes: `rom:` + 16 hex digits of SHA-1. Same as `tools/convert/farm.py` `input_key` (and `symbols/pool.tsv`) for ROM code; for IWRAM code the bytes come from where the ELF (`iwram_text`) or the lifter's `COPIES` put them in ROM. | `bn6f/bn6f.gba` |
| `range` | The lifted unit's address range, `0xLO-0xHI`. | lifted header |
| `contract` | Own register contract: `INPUTS -> OUTPUTS; clobbers REGS` (`-` for none), as the lifter computed it. | the lifted header in `cport/src/lifted/difftest_lifted_*.c` |
| `callees` | Each direct callee of the original (`bl` and branches to other units) and the registers and flags it returns (its outputs). | lifted headers |
| `headers` | A hash of the header definitions (macros, inline functions, constants, types from `cport/include`) the readable code uses. | `cport/include` |
| `lifter` | A hash of the lifter's sources (`tools/lifter/*.py`). | |
| `prompt` | The farm prompt's hash (`tools/wf/wf/tools/models.py` `prompt_hash`) for a model's code, from the install or the passing round in `tools/convert/log`; `legacy` for code installed before stamps existed (before recomp 8b64ff5); `none` for a hand edit; `unknown` (e.g. the convert workflow, which records no prompt hash yet). | the install, the farm log |
| `model` | Who wrote the body: a provider/model id (`hyper/glm-5.3-flash`), `Claude Opus 5.5` (Claude as the convert workflow's model), `hand (Claude Opus 5.5)` (written by hand in a session), or `unknown`. | the install; for legacy code the entry's readable-c evidence, `pool.tsv`, the commit's trailers |
| `run` | The run that made it: a workflow run id (`convert-...`, `farm-batch-...`), `farm.py batch N`, `claude session_ID` for hand-written code, or `unknown` (farm.py runs before run ids existed). | same |
| `body` | A hash of the readable code: the `Next` function and every definition of `cport/src/game` it uses (typed helpers, tables, macros, enumerators), comments and layout ignored, every name under its root (so renames and moves between files change nothing). Other readable functions it calls are left out: they have stamps of their own. | `cport/src/game` |
| `installed` | The commit that made the body what it is (renames aside). | git history |
| `edited` | Later commits that changed a body first installed earlier (hand or tool edits), when there are any. | git history |
| `stamped` | When the stamp was computed. | |

Stamps record committed code: commit the body, then run `tools/symdb.py stamp
NAME` (facts from HEAD; the existing provenance is kept when the body is
unchanged, a changed body is stamped as a hand edit), and commit the entry. An
install gives its provenance: `tools/symdb.py stamp NAME --model M --run R
--prompt P`. `tools/symdb.py stamp --unstamped` stamps new functions from their
entries; `--changed` re-stamps every hand edit; `--all` recomputes everything.
A tool's edit (a mechanical rename, a codemod) is stamped with `--tool-edit`
(e.g. `tools/symdb.py stamp --changed --tool-edit`): the author stays and the
commit that changed the body goes into `edited`.

`tools/symdb.py check` (and the commit gate) compares every stamp with the tree
and names one action per change, the fixed table of the joint plan. It is a
report: nothing here calls a model or runs lockstep. The prompt is recorded, not
compared (a newer prompt does not make older code wrong).

| What changed | Action |
|---|---|
| Own outputs widened (an output added, or a register no longer free), or the original bytes or range changed | back to a model |
| Own outputs narrowed | re-verify, and queue the dead writes for removal (review 1 A6) |
| Own inputs changed (not in the plan's table; the lifter's analysis moved) | re-verify |
| A callee returns fewer registers, and this code mentions one of them | re-verify (a stand-in until the R1-A21 leftover-register lint can be asked about one caller) |
| A callee returns fewer registers this code never mentions | informational |
| The header definitions it uses (body unchanged) | recompile and re-verify |
| The body, with no new stamp | re-stamp as a hand edit |
| The lifter | informational: golden tests and lockstep cover lifter changes |
| A readable function without a stamp | stamp it |

`tools/symdb.py stamps [NAME...]` lists the findings per function
(`--json` for tools).
