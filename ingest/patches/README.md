# Tooling and docs from the orphaned branches: none taken

A commit was to be kept only if it applies cleanly to the fork's `master`, builds, and has a clear
use today. Each candidate was cherry-picked (`--no-commit`) onto `origin/master` in a scratch
worktree. None meets all three conditions, so this folder has no `.patch` files.
`docs/reviews/2026-10-20-orphaned-branches.md` has the context.

| Commit | Branch | What | Applies | Why not taken |
|---|---|---|---|---|
| `74950c78` | old-master | CI: toolchain, SHA-exact ROM, harness tests | yes | runs `make setup-toolchain`, `make funcmap` and `cargo test` in `tools/harness`, none of which exist on master: it would fail |
| `9cd3a637` | old-master | CI cache key on the mgba pin | no (`ci.yml`, `docs/development_plan.md`) | needs `74950c78` and the `tools/mgba` submodule |
| `dd330a78` | old-master | `make setup-toolchain` | no (`Makefile`) | the project builds the toolchain itself (`tools/build_toolchain.sh`) |
| `4c741d60` | old-master | profiler brick 1: function map (`scripts/gen_function_map.py`) | no | reads the `src/asm` layout; `tools/funcindex.py` and `tools/profile.sh` cover it |
| `048e39c2` | old-master | pilot fixture recorder, `fixtures/*.inputs` | no | Rust harness format; `tools/traces` has routes from power-on |
| `b7740580` | old-master | development plan v1 | yes | plan for the shelved Rust harness; v2 (`cf8140f9`) supersedes it and conflicts |
| `cf8140f9`, `43f454dd` | old-master | development plan v2, phase-1 status | no | as above |
| `41392a07` | old-master | remove IDA-era tools | yes | `bn6f/docs/documenting/*.md` still refers to `tools/doc_scripts`; a cleanup, not knowledge |
| `1a9ff518` | old-master | `issues/decomp-blockers.md` | yes | blockers of the shelved trampoline toolkit; the lifter-based port does not have them |
| `d2927bcb` | old-master | `docs/pitfalls.md` | yes | pitfalls of the shelved harness (trampolines, x264, bk2 replay) |
| `605a1194` | old-master | pitfalls, one file per entry | no (`docs/pitfalls.md`) | as above |
| `017880f4` | old-master | `tools/classify_origin.py`, compiled vs hand-written | no (`cyow/00-index.md`) | the heuristic was not checked; see open question 12 of the review |
| `4b822ba6` | old-master | history of the asm split (IDA + PseudoTerminal) | no (`cyow/00-index.md`) | history only |
| `aed299bf` | optimizations | Ghidra pre-pass (`tools/ghidra_decomp.py`) | not tried alone | bundled with a C conversion (9 files); Ghidra is not installed here, so it cannot be checked |
| `49f59e81` | optimizations | `tools/struct_inc_to_h.py` | not tried | `tools/abi/gen_structs.py` does this |
| `ae4d57f9` | old-master | revert of a conversion | no | part of the shelved conversions |
