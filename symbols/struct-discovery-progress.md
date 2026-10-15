# Struct-field discovery progress

Goal: name the `Unk_XX` / offset-named fields of the heavily accessed structs
(BattleObject, AIData, BattleState, Chatbox, GameState, Toolkit, OWPlayerObject,
the `eStruct*` RAM blocks), so the C port types them with self-explanatory
field names. Method per batch: gather all bn6f asm accesses and the
accessing functions' summaries, ask the outside model
(tools/llm.py; minimax/MiniMax-M3, one field per request), verify every
claim against real grep output, write a `struct-field` symbols/db entry,
rename probable/hypothesis ones (the .inc macro input AND the asm offset
symbol via tools/rename_symbol.py, byte-identical ROM), then
`tools/symdb.py check` and `tools/gate.py --all`. The bn6f-changing
commands are wrapped in `flock /tmp/bn6f-record.lock` and wait for any
in-flight `wf record` to finish first; bn6f is committed and pushed to
`fork` per batch, recomp is committed locally with explicit paths only.

Helpers (gitignored under `build/repro/sf/`):
- `probe.py`     — builds struct-field context, calls the LLM, stores
                    /tmp/llm_<field>.txt
- `process.py`   — orchestrator: probe + verify + write entry + rename
                    + commit (.inc change first, then asm offset)

## Tally

| batch | fields examined | renamed (hypothesis or probable) | hypothesis-only | dropped |
|-------|-----------------|----------------------------------|-----------------|---------|
| 1     | 1               | 1                                | 1               | 0       |

Running total: named 1, hypothesis 1, dropped 0 (struct-field kind only).

## Batch 1 (2026-10-15)

1 struct-field renamed (hypothesis), committed on bn6f 480b9356 + 0b071414
(pushed to fork) and recomp f66a93fd:

- **oBattleObject_Unk_0c -> oBattleObject_DirectionAngle** (hypothesis): u8,
  offset 0xC of BattleObject. Holds the result of `calcAngle_800117C`
  (0..255 over a full turn) and is paired with `XVelocity`/`YVelocity`
  writes in at least 4 hit-movement/AI handlers — the classic
  "compute direction to target, set angle then push velocity" pattern. In
  `sub_80BEC84` it is XORed with `DirectionFlip` to flip the sprite.
  Set to sentinel `0xFF` (panel-direction awaiting target) in
  `sub_80D6580` and to `#1` / `Param2` (initial palette/bank) in a
  number of spawn handlers; `sub_80BD374` reuses it as a ColorShader
  table index (Lsl5+add+Lsl5+add = x*0x480 stride) and the
  `sub_80BD084` flash timer decrements it by 2 per frame, so it is
  genuinely multi-purpose across t1/t3/t4 handlers. The
  `DirectionAngle` name fits the calcAngle-driven accesses; the
  flash/timer/sentinel uses are t1-specific overrides. C struct
  regenerated to `u8 DirectionAngle; /* 0x0C */`.

Tool: tools/abi/gen_structs.py refreshed `cport/include/gen/structs.h`
(gitignored); symdb check + gate pass. The C struct rename in `cport/`
isn't tracked; the asm rename lives in bn6f.
