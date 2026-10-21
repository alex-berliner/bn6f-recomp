"""The investigate workflow's pure parts: what ranks a target first, how a model's answers are read,
what a candidate must pass, and the graph end to end with fake models."""
import json
import threading
import time
from pathlib import Path

from typing import cast

import pytest
import yaml

from wf.graphs import investigate as graph
from wf.graphs.investigate import InvestigateState, run_investigate
from wf.tools import investigate
from wf.tools.investigate import (Answer, Candidate, CheckReply, Context, DataSymbol, EvidenceItem, Proposal,
                                  Report, Target, Verdict, name_problem, parse_answer, parse_check,
                                  placeholder, rank, select_targets, status_of, symdb_check)
from wf.types import FunctionName, GuestAddress, ModelSpec, RouteName, SymbolName, SymbolStatus, ToolMessage

MODEL = ModelSpec("hyper/glm-5.3-flash")
CHECKER = ModelSpec("hyper/qwen3.8-flash")


def target(name: str, instructions: int = 10, captured: bool = False, routes: tuple[str, ...] = (),
           named: int = 0, address: int = 0x08000000) -> Target:
    return Target(name=FunctionName(name), address=GuestAddress(address), score=1, instructions=instructions,
                  captured=captured, routes=tuple(RouteName(route) for route in routes), named_neighbours=named,
                  source="asm00_0.s:10")


# ── selection and ranking ──────────────────────────────────────────────

def test_placeholder_names() -> None:
    assert placeholder("sub_3005CDA") and placeholder("asm38")
    assert not placeholder("sprite_queueOamEntry")
    assert not placeholder("nullsub_38") and not placeholder("dead_1") and not placeholder("unused_2")


def test_the_active_small_ones_with_named_neighbours_rank_first() -> None:
    ranked = rank((target("sub_4", instructions=200), target("sub_3", named=1), target("sub_2", routes=("wander",)),
                   target("sub_1", instructions=200, routes=("wander",))))
    assert [t.name for t in ranked] == ["sub_2", "sub_1", "sub_3", "sub_4"]


def fake_selection(monkeypatch: pytest.MonkeyPatch, tmp_path: Path,
                   finished: frozenset[str] = frozenset()) -> tuple[Target, ...]:
    """A scores.tsv of five functions, one already investigated, no coverage, no calls captured."""
    scores = ("".join(f"0x{0x08000000 + 2 * i:08X}\tfunction\tsub_{0x08000000 + 2 * i:08X}\t{i % 2}\n"
                      for i in range(4))
              + "0x08000008\tfunction\tsprite_named\t1\n0x0800000A\tfunction\tnullsub_9\t0\n")
    scores_path = tmp_path / "meaning_scores.tsv"
    scores_path.write_text("# address\tkind\tname\tscore\n" + scores)

    import funcindex
    units = {name: funcindex.Unit(0x08000000 + 2 * i, 0x08000002 + 2 * i, funcindex.Contract((), (), ()),
                                  "asm00_0.s:10")
             for i, name in enumerate(("sub_08000000", "sub_08000002", "sub_08000004"))}
    units["sub_08000004"] = funcindex.Unit(0x08000004, 0x08000010, funcindex.Contract((), (), ()), "asm00_0.s:10")

    def read_scores(path: Path = investigate.SCORES) -> tuple[investigate.ScoreRow, ...]:
        rows: list[investigate.ScoreRow] = []
        for line in scores_path.read_text().splitlines():
            if line.startswith("#") or not line.strip():
                continue
            fields = line.split("\t")
            rows.append((FunctionName(fields[2]), GuestAddress(int(fields[0], 16)), int(fields[3])))
        return tuple(rows)

    def nothing(*arguments: object) -> dict[int, tuple[frozenset[int], frozenset[int]]]:
        return {}

    def no_units(*arguments: object) -> dict[str, tuple[int, int]]:
        return {}

    def no_routes() -> dict[RouteName, frozenset[int]]:
        return {}

    def no_db() -> frozenset[str]:
        return frozenset()

    def none_finished() -> frozenset[str]:
        return finished

    monkeypatch.setattr(investigate, "read_scores", read_scores)
    monkeypatch.setattr(investigate.funcindex, "lifted_units", lambda *a: units)   # type: ignore[arg-type, return-value]
    monkeypatch.setattr(investigate.funcindex, "unit_ranges", no_units)
    monkeypatch.setattr(investigate, "call_graph", nothing)
    monkeypatch.setattr(investigate, "coverage_routes", no_routes)
    monkeypatch.setattr(investigate, "db_names", no_db)
    monkeypatch.setattr(investigate, "finished_names", none_finished)
    monkeypatch.setattr(investigate, "SNAPSHOTS", tmp_path)   # no captured calls
    monkeypatch.setattr(investigate, "GRAPH", tmp_path / "graph.json")
    return select_targets(tmp_path, 500, ())


def test_select_targets_skips_the_finished_and_the_named(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    selected = fake_selection(monkeypatch, tmp_path, finished=frozenset({"sub_08000002"}))
    # sub_08000002 had a candidate; sprite_named and nullsub_9 are not placeholders
    assert [t.name for t in selected] == ["sub_08000000", "sub_08000004"]


def test_select_targets_honours_count(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    selected = fake_selection(monkeypatch, tmp_path)   # the fake lifted units cover three of the five rows
    assert len(select_targets(tmp_path, 2, ())) == 2
    assert len(selected) == 3


def test_an_explicitly_named_function_is_taken_even_with_an_earlier_candidate(monkeypatch: pytest.MonkeyPatch,
                                                                             tmp_path: Path) -> None:
    """--only asks for a function by name: an earlier candidate (one holding no name) is a reason to
    ask again, not a reason to skip it — which is what a second run over the same set needs."""
    fake_selection(monkeypatch, tmp_path, finished=frozenset({"sub_08000002"}))
    # nobody asks: the one with an earlier candidate is out, the other two are not named
    assert "sub_08000002" not in [target.name for target in select_targets(tmp_path, 500, ())]
    asked = select_targets(tmp_path, 500, (FunctionName("sub_08000002"),))
    assert [target.name for target in asked] == ["sub_08000002"]


# ── the names a proposal may carry ────────────────────────────────────────

def test_a_name_must_not_carry_the_placeholder_or_hedge() -> None:
    for bad in ("battleObject_init_80D0FB0", "t3_0x65_init", "Maybe_battleInit", "init80C4766",
                "battle_0x2F_tick"):
        assert name_problem(bad) is not None, bad
    for good in ("sprite_queueOamEntry", "battle_dispatchAI", "setBGTileIdsBlocks1And2"):
        assert name_problem(good) is None, good


def test_an_answer_with_an_unusable_name_is_no_name_but_still_read() -> None:
    answer, problem = parse_answer(json.dumps({
        "proposed_name": "battleObject_init_80D0FB0", "summary": "Initializes the object.",
        "evidence": ["asm00_0.s:12 stores the type"], "self_score": 3, "open_questions": []}))
    assert problem is None and answer is not None
    assert answer.proposed_name is None and answer.summary == "Initializes the object."
    assert any("rejected" in question for question in answer.open_questions)


def test_a_verdict_s_revised_name_must_pass_the_name_rules_too() -> None:
    reply, _ = parse_check(json.dumps({"verdict": "revise", "reason": "r",
                                       "revised_name": "battle_init_80D0FB0"}))
    assert reply is not None and reply.revised_name is None
    reply, _ = parse_check(json.dumps({"verdict": "revise", "reason": "r",
                                       "revised_name": "battle_initType58"}))
    assert reply is not None and reply.revised_name == "battle_initType58"


def name_prefix_rows() -> tuple[investigate.ScoreRow, ...]:
    """A slice of bn6f's names: the placeholders must not shape the prefix list."""
    names = (("sub_3005CDA", 40), ("sprite_queueOamEntry", 41), ("sprite_setPalette", 42),
             ("battle_dispatchAI", 43), ("setBGTileIdsBlocks1And2", 44))
    return tuple((FunctionName(name), GuestAddress(address), 1) for name, address in names)


def test_prefixes_come_from_bn6f_s_names_not_the_placeholders(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(investigate, "read_scores", name_prefix_rows)
    monkeypatch.setattr(investigate, "PREFIX_MIN", 1)
    prefixes = investigate.name_prefixes()
    assert "sub" not in prefixes and prefixes[0] == "sprite" and "battle" in prefixes


def test_the_prompt_names_the_rules(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    monkeypatch.setattr(investigate, "read_scores", name_prefix_rows)
    monkeypatch.setattr(investigate, "PREFIX_MIN", 1)
    prompt = investigate.prompt_investigate(context_of(target("sub_8000000"), tmp_path),
                                            investigate.name_prefixes())
    assert "sprite, battle" in prompt
    assert "0x65" in prompt and "Maybe" in prompt and "80C4766" in prompt


def test_the_assembly_ends_at_its_func_end(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    """The lifted header's line is the first instruction, one below the label: the label is found by
    name, and the assembly stops at the function's own func_end (not the rest of the file)."""
    (tmp_path / "asmT.s").write_text("other_start:\n\tbx lr\n\tthumb_func_end other_start\n\n"
                                     "sub_1:\n\tpush {lr}\n\tbx lr\n\tthumb_func_end sub_1\n\n"
                                     "sub_2:\n\tnop\n")
    monkeypatch.setattr(investigate, "ASM_DIR", tmp_path)
    text = investigate.assembly_of("asmT.s:6", "sub_1")   # line 6 is the first instruction
    assert text.startswith("sub_1:") and text.rstrip().endswith("thumb_func_end sub_1")
    assert "sub_2" not in text and "other_start" not in text


# ── reading the answers ────────────────────────────────────────────────

def test_an_answer_in_a_json_block_is_read() -> None:
    answer, problem = parse_answer('Reasoning...\n```json\n' + json.dumps({
        "proposed_name": "sprite_queueOamEntry", "summary": "Queues a sprite.",
        "interface": "(r0 = obj) -> r0 = 0xFF if queued", "details": "Queues the entry.",
        "evidence": ["callers all pass an object", "asm00_0.s:12 reads oObjectSprite_Flags"],
        "self_score": 4, "open_questions": ["who clears the queue"]}) + "\n```")
    assert problem is None
    assert answer is not None
    assert answer.proposed_name == "sprite_queueOamEntry" and answer.self_score == 4
    assert len(answer.evidence) == 2 and answer.open_questions == ("who clears the queue",)


def test_a_null_proposal_is_an_answer_too() -> None:
    answer, problem = parse_answer(json.dumps({
        "proposed_name": None, "summary": None, "details": None, "evidence": [],
        "self_score": None, "open_questions": ["what its callers pass"]}))
    assert problem is None
    assert answer is not None
    assert answer.proposed_name is None and answer.self_score is None
    assert answer.open_questions == ("what its callers pass",)


def test_a_name_without_a_summary_or_evidence_is_refused() -> None:
    for reply in (json.dumps({"proposed_name": "sprite_x"}),
                  json.dumps({"proposed_name": "sprite_x", "summary": "s", "evidence": []}),
                  "no json here", json.dumps([1, 2])):
        answer, problem = parse_answer(reply)
        assert answer is None and problem is not None


def test_a_verdict_is_read_and_must_cite_something() -> None:
    reply, problem = parse_check(json.dumps({
        "verdict": "revise", "reason": "asm00_0.s:15 stores to eSaveIcon", "revised_name": "save_writeIcon",
        "revised_summary": "Writes the save icon."}))
    assert problem is None and reply is not None
    assert reply.verdict == "revise" and reply.revised_name == "save_writeIcon"
    assert parse_check(json.dumps({"verdict": "maybe", "reason": "r"}))[0] is None
    assert parse_check(json.dumps({"verdict": "agree"}))[0] is None   # no reason: not a check


def named_proposal() -> Proposal:
    return Proposal(name=FunctionName("sub_1"), address=GuestAddress(0x08000000), tokens=0, seconds=0.0,
                    answer=Answer(proposed_name=SymbolName("sprite_x"), summary=ToolMessage("s"),
                                  interface=None, details=None, evidence=(ToolMessage("e"),),
                                  self_score=3, open_questions=()))


def test_status_probable_only_when_the_checker_agreed() -> None:
    checks = (CheckReply(verdict="agree", reason=ToolMessage("r"), revised_name=None, revised_summary=None),
              CheckReply(verdict="disagree", reason=ToolMessage("r"), revised_name=None, revised_summary=None),
              None)
    expected = (SymbolStatus.PROBABLE, SymbolStatus.HYPOTHESIS, SymbolStatus.HYPOTHESIS)
    for reply, status in zip(checks, expected):
        assert status_of(named_proposal(), Verdict(name=FunctionName("sub_1"), address=GuestAddress(0x08000000),
                                                   reply=reply, tokens=0, seconds=0.0)) is status


def test_a_proposal_without_a_name_stays_hypothesis() -> None:
    proposal = Proposal(name=FunctionName("sub_1"), address=GuestAddress(0x08000000), tokens=0, seconds=0.0,
                        answer=Answer(proposed_name=None, summary=None, interface=None, details=None,
                                      evidence=(), self_score=None,
                                      open_questions=(ToolMessage("what callers pass"),)))
    assert status_of(proposal, Verdict(name=FunctionName("sub_1"), address=GuestAddress(0x08000000),
                                       reply=None, tokens=0, seconds=0.0)) is SymbolStatus.HYPOTHESIS


# ── the candidates ─────────────────────────────────────────────────────

def candidate(name: str = "sub_8000000", address: int = 0x08000000) -> Candidate:
    return Candidate(name=FunctionName(name), address=GuestAddress(address), kind="function",
                     status=SymbolStatus.PROBABLE,
                     summary=ToolMessage("Proposed name: sprite_queueOamEntry. Queues an OAM entry."),
                     interface=ToolMessage("(r0) -> r0"), details=ToolMessage("Reads the object's sprite."),
                     evidence=(EvidenceItem(method="assembly-reading", date="2026-10-18",
                                            detail=ToolMessage("asm00_0.s:12 reads oObjectSprite_Flags"),
                                            refs=("bn6f/asm/asm00_0.s:10",),
                                            reproduce=ToolMessage("python3 tools/asmcat.py 0x08000000"),
                                            result=ToolMessage("proposed by hyper/glm-5.3-flash, self_score 4; "
                                                               "cross-checked by hyper/qwen3.8-flash: agree")),),
                     coverage=None, open_questions=(), sources=("wf investigate x", "models"))


def test_a_written_candidate_passes_symdb_s_own_check(tmp_path: Path) -> None:
    syms = {"sub_8000000": 0x08000000}
    path = investigate.write_candidate(candidate(), tmp_path)
    assert symdb_check(path, syms) == ()


def test_symdb_s_check_catches_the_candidate_s_problems(tmp_path: Path) -> None:
    syms = {"sub_8000000": 0x08000000}
    path = investigate.write_candidate(candidate(address=0x08000004), tmp_path)
    assert symdb_check(path, syms) == (ToolMessage("address 0x8000004 but bn6f.elf has 0x8000000"),)
    path = investigate.write_candidate(candidate(name="sub_8"), tmp_path)
    assert symdb_check(path, syms) == (ToolMessage("sub_8 is not a symbol of bn6f.elf"),)
    misplaced = candidate().evidence[0].model_copy(update={"method": "vibes"})
    path = investigate.write_candidate(candidate().model_copy(update={"evidence": (misplaced,)}), tmp_path)
    assert symdb_check(path, syms) == (ToolMessage("unknown evidence method vibes"),)


def test_a_candidate_s_address_is_a_hex_string_and_only_spec_keys_are_written(tmp_path: Path) -> None:
    path = investigate.write_candidate(candidate(), tmp_path)
    assert path == tmp_path / "sub_8000000.yaml"
    raw: object = cast(object, yaml.safe_load(path.read_text()))
    loaded = cast_mapping(raw)
    # only the database's keys, in its order: the proposal's name and the cross-check live in the
    # summary, the evidence and the report, not in extra keys
    assert list(loaded) == ["name", "address", "kind", "status", "summary", "interface", "details",
                            "evidence", "open_questions", "sources"]   # coverage left out when empty
    assert loaded["address"] == "0x08000000"   # a hex string, as the database writes addresses
    assert str(loaded["summary"]).startswith("Proposed name: sprite_queueOamEntry. ")
    assert "proposed_name" not in loaded and "cross_check" not in loaded and "coverage" not in loaded
    first = cast_mapping(cast_list(loaded["evidence"])[0])
    assert list(first) == ["method", "date", "detail", "refs", "reproduce", "result"]
    assert str(first["result"]).startswith("proposed by hyper/glm-5.3-flash, self_score 4")
    assert loaded["sources"] == ["wf investigate x", "models"]


def test_the_candidate_carries_the_cross_check_in_its_spec_keys(tmp_path: Path) -> None:
    """The checker's verdict and reason land in the evidence's result text and open_questions."""
    proposal = named_proposal()
    verdict = Verdict(name=proposal.name, address=proposal.address, tokens=0, seconds=0.0,
                      reply=CheckReply(verdict="revise", reason=ToolMessage("the type is 0x58, not 0x5D"),
                                       revised_name=SymbolName("battle_initType58"),
                                       revised_summary=ToolMessage("Initializes a type-0x58 object.")))
    candidate_out = investigate.make_candidate(context_of(target("sub_1", routes=("battlesweep",)), tmp_path),
                                               proposal, verdict,
                                               ModelSpec("m/p"), ModelSpec("c/q"), "run1")
    assert candidate_out.coverage == ("battlesweep",)   # a spec key, only when there are routes
    assert str(candidate_out.summary).startswith("Proposed name: battle_initType58. ")
    assert "self_score 3" in candidate_out.evidence[0].result
    assert "revise — the type is 0x58, not 0x5D" in candidate_out.evidence[0].result
    assert "the type is 0x58, not 0x5D" in candidate_out.open_questions
    assert candidate_out.status is SymbolStatus.HYPOTHESIS   # revise is not agreement


def test_a_candidate_without_a_name_states_what_is_missing(tmp_path: Path) -> None:
    proposal = Proposal(name=FunctionName("sub_1"), address=GuestAddress(0x08000000), tokens=0, seconds=0.0,
                        answer=Answer(proposed_name=None, summary=None, interface=None, details=None,
                                      evidence=(), self_score=None,
                                      open_questions=(ToolMessage("what callers pass"),)))
    verdict = Verdict(name=proposal.name, address=proposal.address, reply=None, tokens=0, seconds=0.0,
                      problem=ToolMessage("no proposal to check"))
    candidate_out = investigate.make_candidate(context_of(target("sub_1"), tmp_path), proposal, verdict,
                                               ModelSpec("m/p"), ModelSpec("c/q"), "run1")
    assert candidate_out.status is SymbolStatus.HYPOTHESIS
    assert not str(candidate_out.summary).startswith("Proposed name:")
    assert "what callers pass" in candidate_out.open_questions


def cast_mapping(loaded: object) -> dict[str, object]:
    """The yaml as a mapping, for pyright's sake (the entries a candidate writes)."""
    assert isinstance(loaded, dict)
    return cast(dict[str, object], loaded)


def cast_list(loaded: object) -> list[object]:
    assert isinstance(loaded, list)
    return cast(list[object], loaded)


def test_report_rows(tmp_path: Path) -> None:
    proposal = Proposal(name=FunctionName("sub_8000000"), address=GuestAddress(0x08000000), tokens=100, seconds=1.0,
                        retries=1,
                        answer=Answer(proposed_name=SymbolName("sprite_queueOamEntry"),
                                      summary=ToolMessage("Queues an OAM entry."), interface=None, details=None,
                                      evidence=(ToolMessage("e"),), self_score=4, open_questions=()))
    verdict = Verdict(name=FunctionName("sub_8000000"), address=GuestAddress(0x08000000), tokens=50, seconds=1.0,
                      reply=CheckReply(verdict="agree", reason=ToolMessage("fits the callers"),
                                       revised_name=None, revised_summary=None))
    rows = investigate.report_rows(((proposal, verdict),)).splitlines()
    assert rows[0].startswith("# address\told\tproposed\tstatus\tself_score\tcross_check\ttokens\tretries")
    assert rows[1] == "0x08000000\tsub_8000000\tsprite_queueOamEntry\tprobable\t4\tagree\t150\t1"


# ── the graph, end to end, with fake models ────────────────────────────

def context_of(target: Target, tmp_path: Path, comment: ToolMessage | None = None,
               data: tuple[DataSymbol, ...] = ()) -> Context:
    return Context(target=target, assembly=tmp_path / f"{target.name}.asm", comment=comment,
                   callers=(), callees=(), data=data)


class Fakes:
    """What the fake models did, for the assertions."""
    asked: list[str] = []


def fake_graph(monkeypatch: pytest.MonkeyPatch, tmp_path: Path, contexts: tuple[Context, ...],
               proposals: tuple[Proposal, ...], verdicts: tuple[Verdict, ...], asked: list[str]) -> None:
    def select(run_dir: Path, count: int, only: tuple[FunctionName, ...],
               retry: frozenset[str] = frozenset()) -> tuple[Target, ...]:
        return tuple(context.target for context in contexts)

    def gather(targets: tuple[Target, ...], run_dir: Path, gather: bool = False) -> tuple[Context, ...]:
        return contexts

    def investigate_all(model: ModelSpec, given: tuple[Context, ...], run_dir: Path, run_id: str,
                        jobs: int, prefixes: tuple[str, ...]) -> tuple[Proposal, ...]:
        asked.append("investigate")
        return proposals

    def check_all(model: ModelSpec, given: tuple[Context, ...], given_proposals: tuple[Proposal, ...],
                  run_dir: Path, run_id: str, jobs: int) -> tuple[Verdict, ...]:
        asked.append("check")
        return verdicts

    def symbols(wanted: frozenset[str]) -> dict[str, int]:
        return {name: 0x08000000 for name in wanted}

    monkeypatch.setattr(graph, "select_targets", select)
    monkeypatch.setattr(graph, "gather_all", gather)
    monkeypatch.setattr(graph, "investigate_all", investigate_all)
    monkeypatch.setattr(graph, "check_all", check_all)
    monkeypatch.setattr(investigate, "elf_symbols", symbols)


def test_the_graph_writes_candidates_and_a_report(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    asked: list[str] = []
    contexts = (context_of(target("sub_8000000", named=2), tmp_path, ToolMessage("(r0 = obj) -> void"),
                           (DataSymbol(name=SymbolName("byte_2000000"), comment=None),)),
                context_of(target("sub_8000002"), tmp_path))
    proposals = (
        Proposal(name=FunctionName("sub_8000000"), address=GuestAddress(0x08000000), tokens=120, seconds=1.0,
                 answer=Answer(proposed_name=SymbolName("sprite_queueOamEntry"),
                               summary=ToolMessage("Queues an OAM entry."), interface=ToolMessage("(r0) -> r0"),
                               details=None, evidence=(ToolMessage("callers all pass an object"),),
                               self_score=4, open_questions=())),
        Proposal(name=FunctionName("sub_8000002"), address=GuestAddress(0x08000000), tokens=90, seconds=1.0,
                 answer=Answer(proposed_name=None, summary=None, interface=None, details=None, evidence=(),
                               self_score=None, open_questions=(ToolMessage("what callers pass"),))))
    verdicts = (Verdict(name=FunctionName("sub_8000000"), address=GuestAddress(0x08000000), tokens=60, seconds=1.0,
                        reply=CheckReply(verdict="agree", reason=ToolMessage("the callers say so"),
                                         revised_name=None, revised_summary=None)),
                Verdict(name=FunctionName("sub_8000002"), address=GuestAddress(0x08000000), reply=None, tokens=0,
                        seconds=0.0, problem=ToolMessage("no proposal to check")))
    fake_graph(monkeypatch, tmp_path, contexts, proposals, verdicts, asked)

    run_dir = tmp_path / "investigate" / "run1"
    report = run_investigate(InvestigateState(run_dir=run_dir), run_id="investigate-1", tracked=False)

    assert asked == ["investigate", "check"]
    assert (report.written, report.agreed, report.nulls, report.tokens) == (2, 1, 1, 270)
    assert report.stopped is None and report.problems == ()
    assert sorted(path.name for path in run_dir.glob("*.yaml")) == ["sub_8000000.yaml", "sub_8000002.yaml"]
    assert (run_dir / "report.tsv").exists()
    assert "agreed" in report.totals()


def test_the_graph_finishes_with_nothing_selected(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    fake_graph(monkeypatch, tmp_path, (), (), (), [])
    report = run_investigate(InvestigateState(run_dir=tmp_path), run_id="investigate-2", tracked=False)
    assert report.written == 0 and report.selected == 0
    assert not list(tmp_path.glob("*.yaml"))


def test_a_run_out_of_credits_keeps_what_is_done() -> None:
    """A 402 problem stops the run cleanly: the report says so (the graph's write step keeps going)."""
    from wf.tools.investigate import out_of_credits
    assert out_of_credits(ToolMessage("hyper/glm-5.3-flash: HTTP 402: out of credits"))
    assert not out_of_credits(ToolMessage("hyper/glm-5.3-flash: HTTP 500: gone"))
    report = Report(selected=5, written=2, agreed=1, revised=0, disagreed=0, nulls=1, invalid=0, retries=1,
                    tokens=900,
                    problems=(ToolMessage("sub_3: hyper/glm-5.3-flash: HTTP 402: out of credits"),),
                    stopped=ToolMessage("out of credits"))
    assert "2 candidates of 5" in report.totals() and "out of credits" in report.totals()
    assert "1 retries" in report.totals()

# ── the retry of an earlier run's unnamed ones ─────────────────────────

def write_proposal(runs: Path, run_id: str, name: str, proposed: str | None, finished: bool = False,
                   problem: str | None = None) -> None:
    folder = runs / run_id / "proposals"
    folder.mkdir(parents=True, exist_ok=True)
    answer = None if proposed is None else {"proposed_name": proposed, "summary": "s", "interface": None,
                                            "details": None, "evidence": ["e"], "self_score": 3,
                                            "open_questions": []}
    (folder / f"{name}.json").write_text(json.dumps(
        {"name": name, "address": 0x08000000, "answer": answer, "tokens": 1, "seconds": 1.0,
         "retries": 0, "problem": problem, "finished": finished}))


def test_unnamed_names_takes_a_run_s_unnamed_not_landed_ones(monkeypatch: pytest.MonkeyPatch,
                                                             tmp_path: Path) -> None:
    runs = tmp_path / "investigate"
    write_proposal(runs, "run1", "sub_1", None)
    write_proposal(runs, "run1", "sub_2", "sprite_named")
    write_proposal(runs, "run1", "sub_3", None, finished=True)
    write_proposal(runs, "run1", "sub_4", None)          # but landed since: it has a db entry
    monkeypatch.setattr(investigate, "INVESTIGATE", runs)
    monkeypatch.setattr(investigate, "db_names", lambda: frozenset({"sub_4"}))
    assert investigate.unnamed_names("run1") == frozenset({"sub_1"})
    assert investigate.unnamed_names("no-such-run") == frozenset()


def test_a_retry_run_selects_the_unnamed_despite_the_earlier_candidate(monkeypatch: pytest.MonkeyPatch,
                                                                       tmp_path: Path) -> None:
    fake_selection(monkeypatch, tmp_path, finished=frozenset({"sub_08000000", "sub_08000004"}))
    retry = frozenset({"sub_08000004"})
    assert [t.name for t in select_targets(tmp_path, 500, (), retry=retry)] == ["sub_08000004"]
    # without the retry the earlier candidate keeps the function out
    assert [t.name for t in select_targets(tmp_path, 500, ())] == ["sub_08000002"]


# ── the call sites in the graph and the callers' snippets ──────────────

def test_the_call_graph_keeps_the_call_sites(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    graph = {"entries": {"caller_a": {"addr": "0x08001000", "unit": "caller_a"},
                         "sub_1": {"addr": "0x08002000", "unit": "sub_1"}},
             "sites": [{"addr": "0x08001004", "caller": "caller_a", "kind": "call", "target": "sub_1"},
                       {"addr": "0x08001008", "caller": "caller_a", "kind": "call", "target": "sub_1"},
                       {"addr": "0x08002000", "caller": "sub_1", "kind": "call", "target": "sub_1"}]}
    (tmp_path / "graph.json").write_text(json.dumps(graph))
    monkeypatch.setattr(investigate, "GRAPH", tmp_path / "graph.json")
    monkeypatch.setattr(investigate, "_graph_cache", None)
    callers, callees, sites = investigate.call_graph()[0x08002000]
    assert callers == frozenset({0x08001000})
    assert callees == frozenset()                             # its only call is to itself: not a site
    assert sites == ((0x08001004, 0x08001000), (0x08001008, 0x08001000))


def test_snippet_around_takes_the_instructions_around_the_call() -> None:
    lines = tuple("\tmov r0, r1" if index != 20 else "\tbl sub_1" for index in range(40))
    text = investigate.snippet_around(lines, 20)
    shown = text.splitlines()
    assert len(shown) == 25                                   # 12 before, the call, 12 after
    assert shown[12] == "\tbl sub_1" and "\tmov r0, r1" in shown[0]


def test_call_lines_finds_the_call_by_label_or_address(monkeypatch: pytest.MonkeyPatch) -> None:
    import funcindex
    assembly = "\tpush {lr}\n\tbl sub_1\n\tbl wrapper_2000\n\tbl elsewhere\n\tpop {pc}"
    units = {"wrapper_2000": funcindex.Unit(0x08002000, 0x08002010, funcindex.Contract((), (), ()), None),
             "elsewhere": funcindex.Unit(0x08009000, 0x08009010, funcindex.Contract((), (), ()), None)}
    calls = investigate.call_lines(assembly, "sub_1", GuestAddress(0x08002000), units)
    assert calls == (1, 2)                                    # by label, then by the alias's address


def test_the_call_sites_show_how_the_callers_use_it(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    (tmp_path / "caller.s").write_text(
        "caller:\n\tpush {lr}\n\tldr r0, =byte_2000000\n\tmov r1, #8\n\tbl sub_1\n"
        "\tstr r0, [r4]\n\tpop {pc}\n\tthumb_func_end caller\n")
    monkeypatch.setattr(investigate, "ASM_DIR", tmp_path)
    import funcindex
    units = {"caller": funcindex.Unit(0x08001000, 0x08001010, funcindex.Contract((), (), ()), "caller.s:5")}
    sites = (investigate.CallSite(caller=FunctionName("caller"), address=GuestAddress(0x08001000),
                                  site=GuestAddress(0x08001006), summary=None),)
    text = investigate.sites_text_of(target("sub_1", address=0x08002000), sites, "sub_1", units)
    assert "calls it at 0x08001006" in text
    assert "\tbl sub_1" in text and "str r0, [r4]" in text


def test_a_call_site_outside_the_caller_s_assembly_says_so(monkeypatch: pytest.MonkeyPatch,
                                                           tmp_path: Path) -> None:
    (tmp_path / "caller.s").write_text("caller:\n\tpop {pc}\n\tthumb_func_end caller\n")
    monkeypatch.setattr(investigate, "ASM_DIR", tmp_path)
    sites = (investigate.CallSite(caller=FunctionName("caller"), address=GuestAddress(0x08001000),
                                  site=GuestAddress(0x08001006), summary=None),)
    text = investigate.sites_text_of(target("sub_1", address=0x08002000), sites, "sub_1", {})
    assert "not in the caller's bn6f assembly" in text


def test_the_shown_call_sites_prefer_what_the_project_knows(monkeypatch: pytest.MonkeyPatch) -> None:
    known = {0x08001000: "sub_a", 0x08002000: "battle_named", 0x08003000: "sub_c", 0x08004000: "sub_d"}

    def find(address: int) -> str | None:
        return known.get(address)

    site_list = ((0x08001004, 0x08001000), (0x08002004, 0x08002000), (0x08003004, 0x08003000),
                 (0x08004004, 0x08004000))
    sites = investigate.sites_of(target("sub_1", address=0x08009000), site_list, find,
                                 {"battle_named": "Runs the battle objects."})
    assert len(sites) == 3 and sites[0].caller == "battle_named"   # the known one first, 3 at most


# ── the empty-listing fix: a unit name's functions are labelled inside ──

def test_an_asm_named_unit_s_function_finds_its_label(monkeypatch: pytest.MonkeyPatch,
                                                      tmp_path: Path) -> None:
    (tmp_path / "asm33.s").write_text(
        "\tthumb_func_start sub_8123198\nsub_8123198:\n\tldr r2, =word_200DCF0\n\tmov pc, lr\n"
        "\tthumb_func_end sub_8123198\n")
    monkeypatch.setattr(investigate, "ASM_DIR", tmp_path)
    text = investigate.assembly_of("asm33.s:4", "asm33")      # the scores' name is the unit's
    assert text.startswith("sub_8123198:") and text.rstrip().endswith("thumb_func_end sub_8123198")
    assert investigate.label_at(GuestAddress(0x08123198), FunctionName("asm33"),
                                lambda address: "sub_8123198" if address == 0x08123198 else None) \
        == "sub_8123198"
    assert investigate.label_at(GuestAddress(0x08123198), FunctionName("asm33"), lambda address: None) \
        == "asm33"


def test_the_label_index_covers_the_asm_and_data_chunk_files(monkeypatch: pytest.MonkeyPatch,
                                                             tmp_path: Path) -> None:
    (tmp_path / "bn6f" / "data").mkdir(parents=True)
    (tmp_path / "bn6f" / "asm").mkdir(parents=True)
    (tmp_path / "bn6f" / "ewram.s").write_text("eS200BC50:: // 0x200bc50\n")
    (tmp_path / "bn6f" / "data" / "dat00.s").write_text("byte_3006F6C:\n\t.byte 0x3\n")
    (tmp_path / "bn6f" / "asm" / "asm38.s").write_text("word_200DCF0:: // 0x200dcf0\n")
    monkeypatch.setattr(investigate, "REPO", tmp_path)
    monkeypatch.setattr(investigate, "ASM_DIR", tmp_path / "bn6f" / "asm")
    labels = investigate.label_index()
    assert labels["eS200BC50"][0] == tmp_path / "bn6f" / "ewram.s"
    assert labels["byte_3006F6C"][0] == tmp_path / "bn6f" / "data" / "dat00.s"
    assert labels["word_200DCF0"][0] == tmp_path / "bn6f" / "asm" / "asm38.s"


# ── the context: the call sites are the evidence ───────────────────────

def test_the_context_shows_the_call_sites_and_keeps_the_assembly_last(tmp_path: Path) -> None:
    sites_file = tmp_path / "sub_8000000.sites.asm"
    sites_file.write_text("== 0x08001000 caller calls it at 0x08001004\n\tbl sub_8000000\n")
    context = context_of(target("sub_8000000"), tmp_path).model_copy(update={
        "sites": (investigate.CallSite(caller=FunctionName("caller"), address=GuestAddress(0x08001000),
                                       site=GuestAddress(0x08001004), summary=None),),
        "sites_text": sites_file})
    text = investigate.context_text(context)
    assert "Call sites (how its callers use it):" in text
    assert "bl sub_8000000" in text
    assert text.index("Call sites") < text.index("Assembly:")   # the target's assembly is still last
    assert text.rstrip().endswith("```")


def test_the_prompt_says_the_call_sites_are_the_evidence(monkeypatch: pytest.MonkeyPatch,
                                                         tmp_path: Path) -> None:
    monkeypatch.setattr(investigate, "read_scores", name_prefix_rows)
    monkeypatch.setattr(investigate, "PREFIX_MIN", 1)
    prompt = investigate.prompt_investigate(context_of(target("sub_8000000"), tmp_path),
                                            investigate.name_prefixes())
    assert "call sites are the evidence" in prompt
    assert "used only as" in prompt
    assert investigate.CONTEXT_TOKENS == 6_000


# ── the runtime facts a --facts run shows (`wf facts` records) ─────────

def fake_record(address: int = 0x08001234, name: str = "sub_08001234",
                **over: object) -> dict[str, object]:
    """One `wf facts` record in the shape the layer writes it (docs/workflows/facts-notes.md), with
    the fields the block reads: a battle and an overworld route that reached it, an indirect caller,
    the RAM and I/O the replayed calls wrote, and the arguments and returns."""
    record: dict[str, object] = {
        "address": address, "address_hex": f"0x{address:08X}", "name": name, "end": address + 0x10,
        "asm": "asm00_0.s:10", "reached": True, "captured": True, "captured_states": 8, "states": 8,
        "probes": 8, "returned": 8, "calls": 6, "calls_per_1000_frames": 0.02, "cycles": 900,
        "routes_reached": 2,
        "routes": [
            {"route": "battle", "executed": True, "body_executed": True, "frames": 20000, "calls": 4,
             "callers": [{"site": "0x03005C68", "caller": "0x0803E18C",
                          "name": "battle_dispatchAI_8003E18", "count": 4}]},
            {"route": "story2", "executed": True, "body_executed": True, "frames": 33546, "calls": 2,
             "callers": [{"site": "0x03005C70", "caller": "0x0803E18C",
                          "name": "battle_dispatchAI_8003E18", "count": 2},
                         {"site": "0x03005C74", "caller": "0x08123456", "name": "sub_8123456", "count": 1}]},
            {"route": "wander", "executed": False, "body_executed": False, "frames": 8000, "calls": None,
             "callers": []},
        ],
        "arguments": [{"reg": "r0", "samples": 8, "distinct": 1, "lowest": 12, "highest": 12,
                       "values": [12]},
                      {"reg": "r1", "samples": 8, "distinct": 3, "lowest": 1, "highest": 9,
                       "values": [1, 2, 9]}],
        "returns": [{"reg": "r0", "samples": 8, "distinct": 2, "lowest": 1, "highest": 4,
                     "values": [1, 4]}],
        "facts": {
            "name": name, "address": address, "probes": 8, "returned": 8,
            "register_inputs": ["inputs r0 = 0x0000000C"], "register_outputs": ["outputs r0 = 0x00000001"],
            "registers_written": ["writes r0"], "written_patterns": ["r0: changed 0x0000000C->0x00000001"],
            "io_writes": [{"name": "REG_DISPSTAT", "address": 0x04000004, "size": 2, "value": 0x0020,
                           "matched": 8}],
            "memory_writes": [{"symbol": "eBattleState_Flags", "address": 0, "bytes": 4,
                               "distinct_values": 7},
                              {"symbol": "IWRAM", "address": 0, "bytes": 32, "distinct_values": 1}],
            "route_coverage": []},
        "facts_error": None, "built": "2026-10-21T11:16:00Z"}
    record.update(over)
    return record


def write_record_file(directory: Path, address: int = 0x08001234, name: str = "sub_08001234",
                      **over: object) -> Path:
    """A `wf facts` record on disk, where the layer keeps them (by_address/0xADDR.json)."""
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / f"0x{address:08X}.json"
    path.write_text(json.dumps(fake_record(address, name, **over)))
    return path


def block_for(monkeypatch: pytest.MonkeyPatch, tmp_path: Path, address: int = 0x08001234,
              name: str = "sub_08001234", **over: object) -> str:
    """The --facts block for a function whose record was written into a facts dir of its own."""
    facts_dir = tmp_path / "by_address"
    facts_dir.mkdir(parents=True, exist_ok=True)
    write_record_file(facts_dir, address, name, **over)
    monkeypatch.setattr(investigate, "FACTS_BY_ADDRESS", facts_dir)
    record = investigate.facts_record(GuestAddress(address))
    assert record is not None
    return investigate.runtime_facts_text(record, FunctionName(name))


def test_the_facts_block_says_what_the_original_game_was_observed_doing(monkeypatch: pytest.MonkeyPatch,
                                                                       tmp_path: Path) -> None:
    block = block_for(monkeypatch, tmp_path)
    assert "battle (battle)" in block and "overworld (story2)" in block     # the scenes, and the routes
    assert "wander" not in block                                            # a route that did not reach it
    assert "reached by 2 of 3 recorded routes" in block and "6 calls" in block
    assert "battle_dispatchAI_8003E18 (6 calls)" in block                   # the runtime callers, by name
    assert "eBattleState_Flags (4 bytes, 7 values)" in block                # RAM written, distinct values
    assert "REG_DISPSTAT = 0x00000020 (2 bytes, in 8 probes)" in block      # I/O written
    assert "r0 = 0x0000000C (8 calls, one value)" in block                  # the arguments
    assert "r1 = 0x00000001..0x00000009 (8 calls, 3 values)" in block
    assert "returns: r0 = 0x00000001..0x00000004" in block                  # and what it returned
    assert "argument registers left unchanged: r1" in block
    assert "observed by running the ORIGINAL game" in block


def test_the_facts_block_is_short_and_hides_the_blind_spots_it_has(monkeypatch: pytest.MonkeyPatch,
                                                                   tmp_path: Path) -> None:
    block = block_for(monkeypatch, tmp_path)
    assert len(block.splitlines()) <= investigate.FACTS_LINES
    assert "write-only I/O registers read back as junk" in block   # an empty I/O line is not evidence


def test_the_ram_symbols_are_capped_and_a_bare_region_sorts_last(monkeypatch: pytest.MonkeyPatch,
                                                                 tmp_path: Path) -> None:
    writes = [{"symbol": f"sym_{index}", "address": 0, "bytes": index, "distinct_values": index}
              for index in range(1, 15)]
    writes.append({"symbol": "IWRAM", "address": 0, "bytes": 999, "distinct_values": 3})
    block = block_for(monkeypatch, tmp_path, facts={**fake_record()["facts"],   # type: ignore[dict-item]
                                                   "memory_writes": writes})
    symbols = block.split("RAM symbols it wrote: ")[1].splitlines()[0]
    assert symbols.count("sym_") == investigate.MAX_FACTS_SYMBOLS and "+5 more" in symbols
    assert symbols.index("sym_14") < symbols.index("sym_5")        # widest first
    assert "IWRAM" not in symbols                                 # a bare region name says least


def test_a_function_no_recording_reached_is_told_so(monkeypatch: pytest.MonkeyPatch,
                                                    tmp_path: Path) -> None:
    """Silence is information too, and it is not evidence for a name either way."""
    block = block_for(monkeypatch, tmp_path, reached=False, routes=[])
    assert "never executed by the recordings" in block
    assert "reached by" not in block and len(block.splitlines()) == 2


def test_a_reached_function_with_no_replayed_call_says_what_is_unknown(monkeypatch: pytest.MonkeyPatch,
                                                                       tmp_path: Path) -> None:
    block = block_for(monkeypatch, tmp_path, facts=None, facts_error="state 0 did not return within "
                                                                     "5000000 instructions", probes=0,
                      returned=0, arguments=[], returns=[])
    assert "unknown -- 0 captured call(s) were replayed and none returned" in block
    assert "did not return within 5000000 instructions" in block


def test_a_function_with_no_facts_record_is_not_a_crash(monkeypatch: pytest.MonkeyPatch,
                                                         tmp_path: Path) -> None:
    monkeypatch.setattr(investigate, "FACTS_BY_ADDRESS", tmp_path / "nothing-here")
    assert investigate.facts_record(GuestAddress(0x08001234)) is None
    assert "none recorded for sub_1" in investigate.runtime_facts_text(None, FunctionName("sub_1"))


def test_a_half_written_record_reads_as_none(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    facts_dir = tmp_path / "by_address"
    facts_dir.mkdir()
    (facts_dir / "0x08001234.json").write_text('{"address": 1, "nam')
    monkeypatch.setattr(investigate, "FACTS_BY_ADDRESS", facts_dir)
    assert investigate.facts_record(GuestAddress(0x08001234)) is None


def test_the_routes_map_to_the_scenes_they_were_played_in() -> None:
    assert investigate.scene_of("battle") == "battle" and investigate.scene_of("battlesweep") == "battle"
    assert investigate.scene_of("chips") == "battle"
    assert investigate.scene_of("story3") == "overworld" and investigate.scene_of("maps") == "overworld"
    assert investigate.scene_of("wander") == "overworld"
    assert investigate.scene_of("menus2") == "menu" and investigate.scene_of("title_mash_a") == "title"
    assert investigate.scene_of("beastsp") == "mixed" and investigate.scene_of("fuzz") == "mixed"


def test_the_record_is_looked_up_by_entry_address(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    """A record is filed by address, so the thumb bit of a name's address finds it either way."""
    facts_dir = tmp_path / "by_address"
    write_record_file(facts_dir, 0x08001234)
    monkeypatch.setattr(investigate, "FACTS_BY_ADDRESS", facts_dir)
    assert investigate.facts_record(GuestAddress(0x08001235)) is not None


def test_the_prompt_says_the_facts_are_observed_and_the_name_must_fit(monkeypatch: pytest.MonkeyPatch,
                                                                      tmp_path: Path) -> None:
    context = context_of(target("sub_8000000"), tmp_path).model_copy(
        update={"facts": block_for(monkeypatch, tmp_path)})
    prompt = investigate.prompt_investigate(context, ("sprite",))
    assert "Runtime facts (observed by running the ORIGINAL game" in prompt
    assert "It is evidence, not a guess" in prompt and "must be consistent with it" in prompt
    assert prompt.index("Runtime facts") < prompt.index("Assembly:")
    checked = investigate.prompt_check(context, named_proposal(), ("sprite",))
    assert "It is evidence, not a guess" in checked and "Runtime facts" in checked


def test_a_run_without_facts_asks_exactly_as_before(monkeypatch: pytest.MonkeyPatch,
                                                    tmp_path: Path) -> None:
    """The flag is off by default: without it the brief carries no facts and no rule about them."""
    context = context_of(target("sub_8000000"), tmp_path)
    assert context.facts == ""
    prompt = investigate.prompt_investigate(context, ("sprite",))
    assert "Runtime facts" not in prompt and "It is evidence, not a guess" not in prompt
    assert "Runtime facts" not in investigate.prompt_check(context, named_proposal(), ("sprite",))


def test_the_graph_gathers_the_facts_only_when_asked(monkeypatch: pytest.MonkeyPatch,
                                                      tmp_path: Path) -> None:
    """`--facts` reaches the gathering step through the run state (off unless the flag says so)."""
    asked: list[bool] = []
    targets = (target("sub_8000000"),)
    contexts = (context_of(targets[0], tmp_path),)

    def select(run_dir: Path, count: int, only: tuple[FunctionName, ...],
               retry: frozenset[str] = frozenset()) -> tuple[Target, ...]:
        return targets

    def gather(targets: tuple[Target, ...], run_dir: Path, gather: bool = False) -> tuple[Context, ...]:
        asked.append(gather)
        return contexts

    monkeypatch.setattr(graph, "select_targets", select)
    monkeypatch.setattr(graph, "gather_all", gather)
    run_investigate(InvestigateState(run_dir=tmp_path / "run", gather=True),
                    run_id="investigate-facts", tracked=False)
    assert asked == [True]
    run_investigate(InvestigateState(run_dir=tmp_path / "run2"), run_id="investigate-plain",
                    tracked=False)
    assert asked == [True, False]      # the flag is off by default


# ── the stragglers: give up, do not cache, do not block the exit ───────

def test_a_straggler_is_abandoned_after_the_pass_s_others_finish(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(investigate, "STRAGGLER_SECONDS", 1)
    items = (0, 1, 2, 3)

    def one(item: int) -> str:
        if item == 3:
            time.sleep(3.0)                               # the straggler: every other one is done
        return f"done {item}"

    def stub(item: int, why: str) -> str:
        return f"stub {item}: {why}"

    cancelled = threading.Event()
    started = time.monotonic()
    out = investigate._run_pass(items, 4, one, stub, cancelled)  # pyright: ignore[reportPrivateUsage]
    assert out[:3] == ("done 0", "done 1", "done 2")
    assert out[3].startswith("stub 3: abandoned:")
    assert time.monotonic() - started < 2.5               # the run did not wait for the straggler
    assert cancelled.is_set()


def test_an_abandoned_request_is_not_cached(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    def no_json(*arguments: object, **keyword: object) -> str:
        return "not a json reply"

    monkeypatch.setattr(investigate, "_complete", no_json)
    cancelled = threading.Event()
    cancelled.set()
    proposal = investigate.ask_investigate(MODEL, context_of(target("sub_1"), tmp_path), tmp_path,
                                           "run1", (), cancelled)
    assert proposal.problem is not None
    assert not (tmp_path / "proposals" / "sub_1.json").exists()   # a later run asks again
    answered = investigate.ask_investigate(MODEL, context_of(target("sub_1"), tmp_path), tmp_path,
                                           "run2", ())
    assert (tmp_path / "proposals" / "sub_1.json").exists()       # a normal answer is cached
    assert answered.problem is not None


def test_abandoned_problems_are_recognised() -> None:
    assert investigate.abandoned(ToolMessage("abandoned: still unanswered 300 s after the pass's "
                                             "last other request finished"))
    assert not investigate.abandoned(ToolMessage("skipped: the model is out of credits"))
    assert not investigate.abandoned(None)


def test_a_straggler_is_counted_and_still_gets_its_candidate(monkeypatch: pytest.MonkeyPatch,
                                                             tmp_path: Path) -> None:
    contexts = (context_of(target("sub_8000000"), tmp_path),)
    proposals = (Proposal(name=FunctionName("sub_8000000"), address=GuestAddress(0x08000000),
                          answer=None, tokens=0, seconds=0.0,
                          problem=ToolMessage("abandoned: still unanswered 300 s after the pass's "
                                              "last other request finished")),)
    verdicts = (Verdict(name=FunctionName("sub_8000000"), address=GuestAddress(0x08000000), reply=None,
                        tokens=0, seconds=0.0, problem=ToolMessage("no proposal to check")),)
    fake_graph(monkeypatch, tmp_path, contexts, proposals, verdicts, [])
    run_dir = tmp_path / "investigate" / "run9"
    report = run_investigate(InvestigateState(run_dir=run_dir), run_id="investigate-9", tracked=False)
    assert report.stragglers == 1 and "1 abandoned" in report.totals()
    assert (run_dir / "sub_8000000.yaml").exists()            # its candidate says what is missing
