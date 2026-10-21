"""The runtime-facts layer's pure parts: how a route's `.prof` and `.edges` become a
function's coverage (executed yes/no, cycles, runtime callers, calls per 1000 frames), how
the records are planned and cached, what one function's replay produces and what a crashing
one records instead, and the graph end to end with fakes (no mGBA, no model).

The integration test at the end is the only one that needs the project: it runs two real
functions with captured calls through mGBA, and skips when build-tools/mgba_run is not built.
"""
import json
import os
import tempfile
from pathlib import Path

import funcindex
import pytest

from wf.graphs import facts as graph
from wf.graphs.facts import FactsState, run_facts, summarise_report
from wf.tools import facts as ft
from wf.tools import runtime_evidence as rt
from wf.tools.facts import (DEFAULT_JOBS, DEFAULT_LIMIT, DEFAULT_STATES, BY_ADDRESS, Caller,
                            CoverageIndex, FunctionFacts, Inputs, RouteCoverage, Target,
                            build_coverage_index, coverage_fingerprint, coverage_files, gather,
            plan, read_record, record_path, register_stats, route_lengths, summarise,
                            targets_by_address, units, write_index)
from wf.types import FunctionName, GuestAddress, RouteName, ToolMessage

# ── fakes: three lifted functions and three recorded routes ────────────

FOO = 0x08001234        # foo_08001234: the routes reach it, one of them calls it
BAR = 0x08005678        # bar_08005678: only the sweep profile reaches it
CALLER = 0x08009000     # caller_08009000: the unit holding the indirect call site
SITE = CALLER + 4      # the indirect call instruction inside `caller`'s range


def fake_units() -> dict[str, funcindex.Unit]:
    """Three lifted units as the lifted headers would give them."""
    contract = funcindex.Contract(("r0",), (), ("r1",))
    return {"foo_08001234": funcindex.Unit(FOO, FOO + 0x20, contract, "asm00_0.s:1"),
            "bar_08005678": funcindex.Unit(BAR, BAR + 0x10, contract, "asm00_0.s:2"),
            "caller_08009000": funcindex.Unit(CALLER, CALLER + 0x40, contract, "asm00_0.s:3")}


def fake_ranges() -> dict[str, tuple[int, int]]:
    return {name: (unit.lo, unit.hi) for name, unit in fake_units().items()}


def fake_facts(name: FunctionName, address: GuestAddress, probes: tuple[rt.Probe, ...],
               coverage: tuple[RouteCoverage, ...]) -> rt.RuntimeFacts:
    """The RuntimeFacts a replay folds into (the real `summarise` is exercised where a
    replay is real; the graph tests only need something with the right shape)."""
    return rt.RuntimeFacts(name=name, address=address, probes=len(probes),
                           returned=sum(1 for probe in probes if probe.returned),
                           register_inputs=(ToolMessage("inputs r0 = 0x00000012"),),
                           register_outputs=(), registers_written=(ToolMessage("writes r0"),),
                           written_patterns=(), io_writes=(), memory_writes=(), route_coverage=())


def write_prof(folder: Path, name: str, lines: dict[int, int],
               header: str = "# game 0..1000 cycles 1") -> Path:
    """A `.prof` file: one `ADDRESS CYCLES` line per executed PC halfword."""
    path = folder / f"{name}.prof"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(header + "\n" + "".join(f"{address:08x} {cycles}\n"
                                            for address, cycles in sorted(lines.items())))
    return path


def write_edges(folder: Path, name: str, rows: tuple[tuple[int, int, int, int], ...]) -> Path:
    """A `.edges` file: one `SITE TARGET LR COUNT` line per observed indirect call."""
    path = folder / f"{name}.edges"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("# site target lr count (mgba_run --call-edges)\n"
                    + "".join(f"{site:08x} {target:08x} {lr:08x} {count}\n"
                              for site, target, lr, count in rows))
    return path


@pytest.fixture
def recordings(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    """A fake tools/traces: `wander` (1000 frames) reaches `foo` at its entry and in its
    body and calls it twice from `caller`; `battle` (2000 frames) reaches `foo`; `sweep` is
    a union profile that names no route and no game-frame span."""
    traces = tmp_path / "traces"
    coverage = traces / "coverage"
    write_prof(coverage, "wander", {FOO: 40, FOO + 2: 6, CALLER: 3})
    write_edges(coverage, "wander", ((SITE, FOO | 1, FOO + 0x18, 2),))
    write_prof(coverage, "battle", {FOO: 10})
    write_prof(coverage, "sweep", {BAR: 1}, header="# union of tools/tracesweep.sh runs, PCs only")
    monkeypatch.setattr(ft, "units", fake_units)
    monkeypatch.setattr(ft, "ranges", fake_ranges)
    monkeypatch.setattr(ft, "_unit_finder", lambda: funcindex.unit_finder(fake_ranges()))
    monkeypatch.setattr(ft, "route_lengths", lambda: {"wander": 1000, "battle": 2000})
    monkeypatch.setattr(ft, "TRACES", traces)
    monkeypatch.setattr(ft, "SNAPSHOTS", tmp_path / "snapshots")
    monkeypatch.setattr(ft, "ROM", tmp_path / "bn6f.gba")
    (tmp_path / "bn6f.gba").write_bytes(b"ROM")
    return traces


def capture(tmp_path: Path, name: str, count: int = 2) -> Path:
    """A capture directory for NAME: COUNT entry snapshots the size mGBA writes."""
    folder = tmp_path / "snapshots" / name
    folder.mkdir(parents=True, exist_ok=True)
    for index in range(count):
        (folder / f"snap_route_{index}.bin").write_bytes(
            b"\0" * (rt.SNAP_HEADER_BYTES + rt.SNAP_MEMORY_BYTES))
    return folder


def probe(returned: bool, r0: int = 0x12, out_r0: int = 0x34) -> rt.Probe:
    """One probe as `replay` hands it over: the entry state, and the exit when it returned."""
    header = [0x53364E42, 1, r0, 0x13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x03007F00, 0x08001235,
              0x08001234, 0x9F | 0x20]
    entry = rt.Snap(header=tuple(header), memory=b"\0" * rt.SNAP_MEMORY_BYTES, path=Path("entry.bin"))
    if not returned:
        return rt.Probe(returned=False, entry=entry, problem=ToolMessage("did not return"))
    out = list(header)
    out[2], out[17] = out_r0, 0x08001235
    return rt.Probe(returned=True, entry=entry,
                    exit=rt.Snap(header=tuple(out), memory=b"\0" * rt.SNAP_MEMORY_BYTES,
                                 path=Path("exit.bin")),
                    exit_pc=GuestAddress(0x08001235),
                    memory_changes=(rt.MemoryChange(address=GuestAddress(0x03000010), size=4,
                                                    before=0, after=0x2A),))


@pytest.fixture
def fake_replay(monkeypatch: pytest.MonkeyPatch) -> list[FunctionName]:
    """`replay` stands in for mGBA (it records which functions it was asked for and answers
    with PROBES) and `summarise` with one built from them: no emulator, no model."""
    asked: list[FunctionName] = []
    probes: tuple[rt.Probe, ...] = (probe(True), probe(True))
    errors: set[FunctionName] = set()

    def replay(name: FunctionName, states: int, work_dir: Path, snapshots: Path | None = None,
               timeout: int = ft.CALL_TIMEOUT) -> tuple[rt.Probe, ...]:
        asked.append(name)
        work_dir.mkdir(parents=True, exist_ok=True)
        if name in errors:
            raise RuntimeError("mgba_run did not run")
        return probes
    monkeypatch.setattr(ft, "replay", replay)
    monkeypatch.setattr(ft, "summarise", fake_facts)
    return asked


# ── the coverage index: .prof cycles and .edges callers ────────────────

def test_prof_cycles_and_execution_become_per_route_coverage(recordings: Path) -> None:
    index = build_coverage_index(recordings)
    assert {route.name for route in index.routes} == {"battle", "sweep", "wander"}
    foo = index.for_function(GuestAddress(FOO), GuestAddress(FOO + 0x20))
    wander = next(row for row in foo if row.route == "wander")
    assert wander.executed and wander.body_executed
    assert wander.entry_cycles == 40              # the .prof counts CYCLES at the entry PC
    assert wander.function_cycles == 46           # the entry's 40 plus the body's 6
    assert wander.body_instructions == 2          # two distinct PCs of the function
    assert wander.frames == 1000 and "routes.py" in str(wander.frames_from)
    battle = next(row for row in foo if row.route == "battle")
    assert battle.executed and battle.function_cycles == 10 and battle.calls is None


def test_a_route_that_never_reached_the_function_is_still_a_row(recordings: Path) -> None:
    """'did not happen' is a fact: every recorded route is reported, executed false."""
    bar = build_coverage_index(recordings).for_function(GuestAddress(BAR), GuestAddress(BAR + 0x10))
    wander = next(row for row in bar if row.route == "wander")
    assert wander.executed is False and wander.function_cycles == 0 and wander.calls is None
    assert wander.frames == 1000                      # the length still comes from wf/routes.py
    sweep = next(row for row in bar if row.route == "sweep")
    assert sweep.executed and sweep.frames is None    # the union sweep names no route length


def test_a_profile_with_no_route_length_falls_back_to_its_own_header(tmp_path: Path,
                                                                      monkeypatch: pytest.MonkeyPatch) -> None:
    traces = tmp_path / "traces"
    write_prof(traces / "coverage", "solo", {FOO: 5}, header="# game 0..777 cycles 5")
    monkeypatch.setattr(ft, "ranges", fake_ranges)
    row = build_coverage_index(traces, lengths={}).for_function(GuestAddress(FOO),
                                                                GuestAddress(FOO + 0x20))[0]
    assert row.frames == 777 and "header" in str(row.frames_from)


def test_edges_name_the_caller_and_match_the_thumb_bit(recordings: Path) -> None:
    """An `.edges` TARGET carries the Thumb bit; the record is keyed by the even address,
    and the site's caller is the lifted unit holding it."""
    row = next(row for row in build_coverage_index(recordings).for_function(
        GuestAddress(FOO), GuestAddress(FOO + 0x20)) if row.route == "wander")
    assert row.calls == 2
    assert row.callers[0].name == "caller_08009000"
    assert row.callers[0].caller == f"0x{CALLER:08X}"
    assert row.callers[0].site == f"0x{SITE:08X}"
    assert row.calls_per_1000_frames == 2.0          # 2 calls over 1000 frames


def test_a_call_site_no_unit_holds_is_reported_unnamed(recordings: Path) -> None:
    write_edges(recordings / "coverage", "wander", ((0x03F00000, FOO | 1, 0x03000000, 5),))
    row = next(row for row in build_coverage_index(recordings).for_function(
        GuestAddress(FOO), GuestAddress(FOO + 0x20)) if row.route == "wander")
    assert row.calls == 5
    assert row.callers[0].name == "(no unit)" and row.callers[0].caller == ""


def test_calls_per_1000_frames_follow_the_route_length(recordings: Path,
                                                       monkeypatch: pytest.MonkeyPatch) -> None:
    """The normaliser uses the route's own length, so a short and a long route compare."""
    monkeypatch.setattr(ft, "route_lengths", lambda: {"wander": 4000, "battle": 2000})
    row = next(row for row in build_coverage_index(recordings).for_function(
        GuestAddress(FOO), GuestAddress(FOO + 0x20)) if row.route == "wander")
    assert row.calls_per_1000_frames == 0.5          # 2 calls over 4000 frames


def test_route_lengths_come_from_wf_routes_at_run_time() -> None:
    """`beastsp` keeps growing as the vidsync recording is imported, so the length is read
    from wf/routes.py when the run starts, never written down here."""
    from wf.routes import LONG_ROUTES
    beastsp = next(route for route in LONG_ROUTES if route.name == "beastsp")
    assert route_lengths()["beastsp"] == int(beastsp.length)
    assert route_lengths()["wander"] > 0


def test_coverage_fingerprint_moves_when_a_recording_moves(recordings: Path) -> None:
    first = coverage_fingerprint(recordings)
    prof = recordings / "coverage" / "wander.prof"
    earlier = prof.stat().st_mtime - 60
    os.utime(prof, (earlier, earlier))
    assert coverage_fingerprint(recordings) != first


def test_coverage_files_lists_every_recorded_route(recordings: Path) -> None:
    assert [name for name, _prof, _edges in coverage_files(recordings)] == ["battle", "sweep", "wander"]


# ── the plan step: what needs mGBA and what does not ───────────────────

def test_plan_writes_the_functions_without_captures_and_leaves_the_rest(tmp_path: Path,
                                                                       recordings: Path) -> None:
    capture(tmp_path, "foo_08001234")
    facts_dir = tmp_path / "facts"
    targets, to_replay, routes, _notes = plan(facts_dir)
    assert [target.name for target in targets] == ["foo_08001234", "bar_08005678", "caller_08009000"]
    assert [target.name for target in to_replay] == ["foo_08001234"]     # the only captured one
    assert "wander" in routes
    record = read_record(record_path(facts_dir, GuestAddress(BAR)))
    assert record is not None and record.captured is False and record.facts is None
    assert record.reached is True                       # the sweep profile reached bar
    assert [row.route for row in record.routes] == ["battle", "sweep", "wander"]


def test_plan_only_and_limit_take_functions_by_name_and_address(tmp_path: Path,
                                                                recordings: Path) -> None:
    targets, to_replay, _routes, _notes = plan(tmp_path / "facts",
                                               only=["bar_08005678", f"0x{FOO:08X}"])
    assert [target.name for target in targets] == ["foo_08001234", "bar_08005678"]
    assert [target.name for target in to_replay] == []      # neither of them has captures
    limited, _replay, _routes, _notes = plan(tmp_path / "facts2", limit=2)
    assert [target.name for target in limited] == ["foo_08001234", "bar_08005678"]


def test_plan_says_so_about_a_name_that_is_not_a_lifted_function(tmp_path: Path,
                                                                recordings: Path) -> None:
    _targets, _replay, _routes, notes = plan(tmp_path / "facts", only=["not_a_function"])
    assert any("not_a_function" in note for note in notes)


def test_a_record_whose_inputs_are_unchanged_is_left_alone(tmp_path: Path, recordings: Path,
                                                            fake_replay: list[FunctionName]) -> None:
    capture(tmp_path, "foo_08001234")
    facts_dir = tmp_path / "facts"
    _targets, to_replay, _routes, _notes = plan(facts_dir)
    gather(facts_dir, to_replay, jobs=1)
    path = record_path(facts_dir, GuestAddress(FOO))
    before = path.read_text()
    _targets, to_replay, _routes, _notes = plan(facts_dir)
    assert to_replay == ()                       # the facts and the coverage are both current
    assert path.read_text() == before            # and the file was not even rewritten
    assert len(fake_replay) == 1                 # one replay in total


def test_a_recording_that_moved_refreshes_the_coverage_without_a_replay(tmp_path: Path,
                                                                      recordings: Path,
                                                                      fake_replay: list[FunctionName]) -> None:
    capture(tmp_path, "foo_08001234")
    facts_dir = tmp_path / "facts"
    _targets, to_replay, _routes, _notes = plan(facts_dir)
    gather(facts_dir, to_replay, jobs=1)
    # the route profile is re-run: same calls, more cycles
    write_prof(recordings / "coverage", "wander", {FOO: 80, FOO + 2: 6, CALLER: 3})
    prof = recordings / "coverage" / "wander.prof"
    earlier = prof.stat().st_mtime - 60                    # the cache reads the mtime
    os.utime(prof, (earlier, earlier))
    _targets, to_replay, _routes, _notes = plan(facts_dir)
    assert to_replay == ()                                    # the captured calls did not change
    assert len(fake_replay) == 1                              # so nothing was replayed
    record = read_record(record_path(facts_dir, GuestAddress(FOO)))
    assert record is not None and record.facts is not None    # the facts were kept
    assert next(row for row in record.routes if row.route == "wander").entry_cycles == 80


def test_new_captured_calls_put_the_function_back_in_the_queue(tmp_path: Path,
                                                               recordings: Path) -> None:
    facts_dir = tmp_path / "facts"
    plan(facts_dir)                              # no captures yet: nothing to replay
    capture(tmp_path, "foo_08001234")
    _targets, to_replay, _routes, _notes = plan(facts_dir)
    assert [target.name for target in to_replay] == ["foo_08001234"]


# ── the gather step: the replay and what a failing one records ────────

def test_gather_records_the_facts_the_arguments_and_the_return(tmp_path: Path, recordings: Path,
                                                               fake_replay: list[FunctionName]
                                                               ) -> None:
    capture(tmp_path, "foo_08001234")
    fake_replay_result = (probe(True), probe(True, r0=0x40, out_r0=0x41))
    ft.replay = lambda name, states, work_dir, snapshots=None, timeout=ft.CALL_TIMEOUT: (   # type: ignore[assignment]
        fake_replay.append(name) or fake_replay_result)
    facts, errors, _skipped, _seconds, _notes = gather(tmp_path / "facts",
                                                       [Target(address=GuestAddress(FOO),
                                                               name=FunctionName("foo_08001234"))],
                                                       states=2, jobs=2)
    assert (facts, errors) == (1, 0)
    assert fake_replay == [FunctionName("foo_08001234")]
    record = read_record(record_path(tmp_path / "facts", GuestAddress(FOO)))
    assert record is not None and record.facts is not None and record.returned == 2
    assert record.probes == 2 and record.captured is True
    r0 = next(row for row in record.arguments if row.reg == "r0")
    assert (r0.distinct, r0.lowest, r0.highest) == (2, 0x12, 0x40)
    returned = record.returns[0]
    assert returned.reg == "r0" and returned.distinct == 2 and returned.values == (0x34, 0x41)


def test_a_replay_that_crashes_is_recorded_and_the_run_carries_on(tmp_path: Path,
                                                                  recordings: Path,
                                                                  monkeypatch: pytest.MonkeyPatch
                                                                  ) -> None:
    capture(tmp_path, "foo_08001234")
    capture(tmp_path, "bar_08005678")

    def replay(name: FunctionName, states: int, work_dir: Path, snapshots: Path | None = None,
               timeout: int = ft.CALL_TIMEOUT) -> tuple[rt.Probe, ...]:
        work_dir.mkdir(parents=True, exist_ok=True)
        if name == "foo_08001234":
            raise RuntimeError("mgba_run did not run")
        return (probe(True),)
    monkeypatch.setattr(ft, "replay", replay)
    monkeypatch.setattr(ft, "summarise", fake_facts)
    facts, errors, _skipped, _seconds, notes = gather(
        tmp_path / "facts",
        [Target(address=GuestAddress(FOO), name=FunctionName("foo_08001234")),
         Target(address=GuestAddress(BAR), name=FunctionName("bar_08005678"))], jobs=2)
    assert (facts, errors) == (1, 1)
    broken = read_record(record_path(tmp_path / "facts", GuestAddress(FOO)))
    other = read_record(record_path(tmp_path / "facts", GuestAddress(BAR)))
    assert broken is not None and broken.facts_error and "mgba_run" in broken.facts_error
    assert broken.facts is None and broken.reached          # the coverage still stands
    assert other is not None and other.facts is not None     # the next function was gathered
    assert any("foo_08001234" in note for note in notes)


def test_a_function_that_never_returns_records_the_reason(tmp_path: Path, recordings: Path,
                                                          fake_replay: list[FunctionName]) -> None:
    capture(tmp_path, "foo_08001234")
    ft.replay = lambda name, states, work_dir, snapshots=None, timeout=ft.CALL_TIMEOUT: (   # type: ignore[assignment]
        fake_replay.append(name) or (probe(False), probe(False)))
    facts, errors, _skipped, _seconds, _notes = gather(
        tmp_path / "facts", [Target(address=GuestAddress(FOO), name=FunctionName("foo_08001234"))])
    assert (facts, errors) == (0, 1)
    record = read_record(record_path(tmp_path / "facts", GuestAddress(FOO)))
    assert record is not None and record.facts_error and "did not return" in record.facts_error


def test_gather_of_nothing_does_nothing(tmp_path: Path, recordings: Path) -> None:
    assert gather(tmp_path / "facts", []) == (0, 0, 0, 0.0, ())


# ── the register stats and the index ──────────────────────────────────

def test_register_stats_keep_a_range_and_the_first_eight_values() -> None:
    stats = register_stats("r0", [3, 1, 2, 1] + list(range(10, 30)))
    assert stats.distinct == 23 and stats.lowest == 1 and stats.highest == 29
    assert stats.values == (1, 2, 3, 10, 11, 12, 13, 14)     # eight of them, ascending
    assert stats.samples == 24


def test_write_index_writes_a_row_per_function(tmp_path: Path, recordings: Path,
                                               fake_replay: list[FunctionName]) -> None:
    capture(tmp_path, "foo_08001234")
    facts_dir = tmp_path / "facts"
    targets, to_replay, _routes, _notes = plan(facts_dir)
    gather(facts_dir, to_replay, jobs=1)
    report = write_index(facts_dir, targets)
    rows = (facts_dir / "coverage.tsv").read_text().splitlines()
    assert rows[0].startswith("# address\tname\treached\troutes\tentries\tentries/1k\t"
                              "calls/1k\tcaptured\tfacts\terror")
    assert len(rows) == len(targets) + 1
    foo = next(row for row in rows if "foo_08001234" in row).split("\t")
    assert foo[:3] == [f"0x{FOO:08X}", "foo_08001234", "yes"]
    # routes, entries, entries/1k, calls/1k, states, facts (no counts were counted here)
    assert foo[3] == "battle,wander" and foo[4] == "-" and foo[5] == "-"
    assert foo[7] == "2" and foo[8] == "yes"
    assert float(foo[6]) == pytest.approx(2 * 1000 / 3000, abs=1e-3)   # 2 calls / 3000 frames
    bar = next(row for row in rows if "bar_08005678" in row).split("\t")
    assert bar[2:9] == ["yes", "sweep", "-", "-", "0", "0", "no"]    # reached, never captured
    assert report.functions == 3 and report.with_facts == 1 and report.captured == 1
    assert report.rows == 3 and report.examples


def test_the_report_groups_the_error_reasons(tmp_path: Path, recordings: Path,
                                             fake_replay: list[FunctionName]) -> None:
    capture(tmp_path, "foo_08001234")
    ft.replay = lambda name, states, work_dir, snapshots=None, timeout=ft.CALL_TIMEOUT: (   # type: ignore[assignment]
        fake_replay.append(name) or (probe(False),))
    facts_dir = tmp_path / "facts"
    _targets, to_replay, _routes, _notes = plan(facts_dir)
    gather(facts_dir, to_replay, jobs=1)
    targets = tuple(sorted(targets_by_address().values(), key=lambda target: target.address))
    report = write_index(facts_dir, targets)
    assert report.facts_errors == 1
    assert report.error_reasons[0].startswith("did not return")
    assert "with runtime facts: 0" in summarise_report(report, 0.0)


# ── the graph, end to end, with fakes ─────────────────────────────────

def test_the_graph_records_every_function_without_mgba(tmp_path: Path, recordings: Path,
                                                      fake_replay: list[FunctionName]) -> None:
    capture(tmp_path, "foo_08001234")
    facts_dir = tmp_path / "facts"
    report = run_facts(FactsState(facts_dir=facts_dir, states=2, jobs=2, limit=DEFAULT_LIMIT),
                       run_id="facts-test-1", tracked=False)
    assert fake_replay == [FunctionName("foo_08001234")]
    assert report.functions == 3 and report.reached == 3 and report.captured == 1
    assert report.with_facts == 1 and report.facts_errors == 0
    assert "wander" in report.routes
    for target in targets_by_address().values():
        assert record_path(facts_dir, target.address).exists()
    assert (facts_dir / "coverage.tsv").exists()
    progress = (facts_dir / "progress.txt").read_text()
    assert "facts-test-1 started" in progress and "replaying 1 function(s)" in progress
    assert "functions: 3" in summarise_report(report, report.seconds)


def test_the_graph_with_only_takes_the_functions_it_was_given(tmp_path: Path, recordings: Path,
                                                              fake_replay: list[FunctionName]) -> None:
    facts_dir = tmp_path / "facts"
    report = run_facts(FactsState(facts_dir=facts_dir, only=(FunctionName("bar_08005678"),),
                                  states=DEFAULT_STATES, jobs=DEFAULT_JOBS),
                       run_id="facts-test-2", tracked=False)
    assert report.functions == 1
    assert [path.name for path in (facts_dir / BY_ADDRESS).iterdir()] == ["0x08005678.json"]


# ── the record as the next steps read it ──────────────────────────────

def test_a_record_round_trips_as_json(tmp_path: Path, recordings: Path) -> None:
    facts_dir = tmp_path / "facts"
    plan(facts_dir)
    path = record_path(facts_dir, GuestAddress(BAR))
    loaded = json.loads(path.read_text())
    assert loaded["address_hex"] == f"0x{BAR:08X}" and loaded["name"] == "bar_08005678"
    assert loaded["inputs"]["coverage"] and loaded["built"]
    assert [route["route"] for route in loaded["routes"]] == ["battle", "sweep", "wander"]
    assert read_record(path) is not None


def test_progress_lines_carry_the_time(tmp_path: Path) -> None:
    ft.note_progress(tmp_path, "one line")
    line = (tmp_path / "progress.txt").read_text().strip()
    assert line.endswith("one line") and len(line.splitlines()[0].split()) > 2


def test_the_state_carries_what_the_steps_read_and_write() -> None:
    """Every step model's fields are fields of the state: burr builds each step's input from
    the state by name, so a field the state does not have would fail at wiring time."""
    state = FactsState.model_fields
    for model in (graph.RecordingsInput, graph.RecordingsOutput, graph.GatherInput,
                  graph.GatherOutput, graph.WriteInput, graph.WriteOutput):
        for field in model.model_fields:
            assert field in state, f"{model.__name__}.{field} is not in FactsState"
    assert state["facts_dir"].default == ft.FACTS
    assert (DEFAULT_STATES, DEFAULT_JOBS, DEFAULT_LIMIT) == (8, 6, 0)
    assert ft.record_path(Path("build/facts"), GuestAddress(0x08001234)).name == "0x08001234.json"


def test_the_small_models_serialise() -> None:
    row = RouteCoverage(route=RouteName("wander"), executed=True, function_cycles=46, calls=2,
                        calls_per_1000_frames=2.0, frames=1000,
                        callers=(Caller(site="0x03005C68", caller=f"0x{CALLER:08X}",
                                        name=FunctionName("caller_08009000"), count=2),))
    assert row.model_dump(mode="json")["callers"][0]["count"] == 2
    assert Inputs(mgba_run=ToolMessage("a"), rom=ToolMessage("b"),
                  snapshots=ToolMessage("c")).states == DEFAULT_STATES


# ── the integration test: two real functions, through mGBA ────────────

@pytest.mark.integration
@pytest.mark.skipif(not ft.ROM.exists() or not rt.PROJECT_ROOT_BIN.exists(),
                    reason="bn6f/bn6f.gba or build-tools/mgba_run is not built here")
def test_two_real_functions_with_captures(tmp_path: Path) -> None:
    """The layer end to end on the project's own recordings and captures: two lifted
    functions that build-snapshots holds, replayed in mGBA from K = 2 captured entry
    states each. (`pytest -m "not integration"` skips it; so does a tree without the
    runner.)"""
    captured = sorted(name for name in units() if _has_capture(name))
    assert len(captured) >= 2, "no captured lifted functions to replay"
    targets = tuple(Target(address=GuestAddress(units()[name].lo), name=FunctionName(name))
                    for name in captured[:2])
    facts_dir = tmp_path / "facts"
    coverage = build_coverage_index()
    for target in targets:
        ft.write_record(record_path(facts_dir, target.address),
                        _real_record(target, units()[target.name], coverage))
    report = write_index(facts_dir, targets)
    assert report.functions == 2 and report.captured == 2 and report.with_facts == 2
    for target in targets:
        record = read_record(record_path(facts_dir, target.address))
        assert record is not None
        assert record.facts is not None and record.returned == 2
        assert record.returns[0].reg == "r0"
        assert any(route.executed or route.body_executed for route in record.routes)


def _has_capture(name: str) -> bool:
    from wf.repo import SNAPSHOTS
    folder = SNAPSHOTS / name
    return folder.is_dir() and any(path.name.endswith(".bin") and not path.name.endswith(".exit.bin")
                                   for path in folder.glob("*.bin"))


def _real_record(target: Target, unit: funcindex.Unit, coverage: CoverageIndex) -> FunctionFacts:
    """One function's record from a real replay of two captured entry states."""
    routes = coverage.for_function(target.address, GuestAddress(unit.hi))
    with tempfile.TemporaryDirectory() as work:
        probes = ft.replay(target.name, 2, Path(work))
    facts = summarise(target.name, target.address, probes, routes)
    arguments, returns = ft.argument_stats(probes)
    inputs = ft.inputs_for(target.name, 2, Path("/no/snapshots"), Path("/no/traces"))
    return ft._record(target, unit, routes, inputs, snaps=len(probes), facts=facts, error=None,
                      probes=len(probes), returned=sum(1 for probe in probes if probe.returned),
                      arguments=arguments, returns=returns)