# Symbol database

One YAML file per symbol of the bn6f disassembly whose meaning this project
established, confirmed or changed. The disassembly carries the result (names,
doc comments); this database carries how we know it, so every claim can be
checked and reproduced. `tools/symdb.py` validates it against the disassembly
and renders `symbols/SYMBOLS.md` (index) and `symbols/symdb.sqlite` (queries).

File name: the symbol's current name in the disassembly, `NAME.yaml`.

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
  - method: assembly-reading   # assembly-reading | caller-analysis | cpu-oracle | difftest | lockstep |
                               # coverage | readable-c | library-match | data-inspection | hardware
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
```

Methods: **cpu-oracle** = the original (mGBA, real BIOS) and the port run
from the same real-memory entry states (tools/oracle.py); **difftest** =
the port's readable version against its lifted original on random or
captured inputs (cport/host/difftest); **lockstep** = whole-game replay of
recorded play against the original (tools/lockstep.sh); **coverage** = which
recorded routes execute it (so lockstep checks it there).

Status: **confirmed** = established from the code and checked against the
original (oracle, lockstep or a readable replacement verified both ways);
**probable** = read from the code and consistent with every use seen, not
independently checked; **hypothesis** = a guess with partial evidence (no
rename; comment only, marked as a guess).
