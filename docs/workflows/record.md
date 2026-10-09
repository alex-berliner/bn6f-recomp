# record

Put what was learnt about symbols into the symbol database and the
disassembly (docs/PROCESS.md §6, and the user's rule: every discovery goes
into bn6f and symbols/db with evidence and how to reproduce it).

Claude writes or updates the symbols/db entries first; this workflow checks
the database, renames symbols in bn6f (only confirmed or probable meanings),
writes the `// symdb:` comments into bn6f, renders SYMBOLS.md, re-verifies
the port when anything was renamed, and commits.

![record](record.png)

Runs are tracked in the Burr UI under project `recomp-record`.

## Steps

### `check_entries` (step)

Check the database, that each symbol has an entry, and that renames have a confirmed or probable meaning;
note what is uncommitted already (other work in the tree is never swept into this run's commit).

- reads: `symbols: tuple[SymbolName, ...]`, `renames: tuple[Rename, ...]`, `run_dir: Path`
- produces: `problems: tuple[ToolMessage, ...]`, `changed_before: tuple[Path, ...]`
- then: `rename_in_bn6f` when `clean_with_renames`; `write_comments_into_bn6f` when `clean_without_renames`; `write_report` when `problems_found`

### `rename_in_bn6f` (step)

Rename in bn6f (ROM stays identical, bn6f commit), in the port's code, and regenerate the lifted C.

- reads: `renames: tuple[Rename, ...]`, `run_dir: Path`
- produces: `renamed_in_bn6f: CommitHash`
- then: `refresh_pc_port_symbols_after_renames`

### `write_comments_into_bn6f` (step)

Write each entry's `// symdb:` block at its definition in bn6f (ROM stays identical, bn6f commit).

- reads: `run_dir: Path`
- produces: `comments_in_bn6f: typing.Optional[CommitHash]`
- then: `render_symbols`

### `write_report` (step)

Say what was recorded, or why nothing was.

- reads: `problems: tuple[ToolMessage, ...]`, `renamed_in_bn6f: typing.Optional[CommitHash]`, `comments_in_bn6f: typing.Optional[CommitHash]`, `verification: VerifyReport | None`, `commit: typing.Optional[CommitHash]`
- produces: `report: RecordReport`
- then: the run ends

### `refresh_pc_port_symbols_after_renames` (step)

Refresh the PC port's symbol overlay from the renamed bn6f (tools/import_symbols.sh).

- reads: `run_dir: Path`
- produces: nothing
- then: `write_comments_into_bn6f`

### `render_symbols` (step)

Regenerate symbols/SYMBOLS.md and symbols/symdb.sqlite.

- reads: `run_dir: Path`
- produces: nothing
- then: `reverify_port` when `symbols_were_renamed`; `commit_records` when `nothing_was_renamed`

### `reverify_port` (step)

Renames change the port's layout, not its behaviour: replay every route to make sure (a child verify run).

- reads: `run_dir: Path`
- produces: `verification: VerifyReport`
- then: `commit_records` when `port_still_verified`; `write_report` when `port_no_longer_verified`

### `commit_records` (step)

Commit the database, the bn6f submodule pointer and the renamed port code locally (and the emptied
list of deferred renames, when that is where this run's renames came from).

- reads: `commit_message: str`, `changed_before: tuple[Path, ...]`, `pending_renames: Path | None`
- produces: `commit: CommitHash`
- then: `write_report`
