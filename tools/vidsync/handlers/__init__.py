"""Repair handlers outside autosync.py: one module per handler, written for a loss the built-in ones could not
fix (the vidsync-repair workflow: an outside model writes it from a brief, Claude reviews it, a test from the
loss checkpoint must sync 600 frames past the loss). autosync.py loads every handlers/NAME.py at start and tries
them in name order: those whose match() says yes first, the rest after its built-in handlers, before the RNG
search.

A handler module defines

    NAME = "weather_orbs"                  # the name autosync logs and --handlers selects

    def try_fix(D: int, sync) -> tuple | None:
        '''The sync lost the video near game frame D: a fix, or None.'''

and, optionally,

    def match(D: int, sync) -> bool:
        '''Cheap (a few state reads, no syncs): is this loss the kind this handler fixes?'''

A handler with match() is tried first, before the built-in handlers, at every loss it matches; one without it
is tried after them. `sync` is the running autosync module: its beam (`sync.b`), the ring of past machines (`sync.state_at(g)`,
`sync.ancestor(g)`), the candidate judges (`sync.path_reach(w, extra, until, D)` on this machine and
`sync.reach_many(w, {id: extra}, until, D)` for many writes at once on the workers, `sync.grid(w, cands, keys,
vid, span)` for a grid of RAM writes judged by the picture, `sync.evaluate(tasks)` for syncs and quick passes on
the workers, `sync.try_at(w, D, which, jmax)` for RNG writes), `sync.log(...)`, and
the game's addresses by name (`sync.syms`, tools/vidsync/syms.py). A fix is `(w, pokes)`: `w` the ring-style
entry the sync goes on from (as state_at returns it), `pokes` the RAM writes `[[game_frame, "ADDR=VALUE/SIZE,..."]]`
that join the run's (the .poke format: hex, applied at main_awaitFrame entry). Model a handler on the built-in
ones (try_position, try_relocate, try_tornado in autosync.py): find what differs from the video, try a family of
writes, judge each by how far the path then holds, return the best only if it holds well past D.

Every address a handler reads or writes comes from syms (bn6f's names) or is a struct offset bn6f names; a new
fact about the game it relies on goes into symbols/db through the `record` workflow (docs/PROCESS.md §6).
"""
