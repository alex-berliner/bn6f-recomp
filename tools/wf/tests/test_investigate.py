"""The investigate workflow's pure parts: what ranks a target first, how a model's answers are read,
what a candidate must pass, and the graph end to end with fake models."""
import json
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
    def select(run_dir: Path, count: int, only: tuple[FunctionName, ...]) -> tuple[Target, ...]:
        return tuple(context.target for context in contexts)

    def gather(targets: tuple[Target, ...], run_dir: Path) -> tuple[Context, ...]:
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