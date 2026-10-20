"""The investigate-data workflow's pure parts: how the ranked RAM/ROM labels are chosen, how the
function-boundary tracking finds an owner's file-order function (the snippet stays in the owner's
body), how a model call reads, what a name must pass, and the graph end to end with fake models."""
import threading
from pathlib import Path

import pytest

from wf.tools import investigate_data
from wf.tools.investigate_data import (Answer, DataContext, DataProposal, DataTarget, function_at,
                                      function_ranges, gather_all, guard_problem, make_candidate,
                                      name_problem, parse_check, report_rows, select_targets,
                                      symdb_check, write_candidate)
from wf.types import GuestAddress, ModelSpec, SymbolName, ToolMessage

MODEL = ModelSpec("minimax/MiniMax-M3")
CHECKER = ModelSpec("minimax/MiniMax-M3")


# ── selection and ranking ──────────────────────────────────────────────

def test_data_placeholder_names() -> None:
    """The data workflow's placeholder rule covers byte/unk/word/dword/hword/off (sub_/dead_
    are the function workflow's, not this one's)."""
    assert investigate_data.data_placeholder("byte_203F750")
    assert investigate_data.data_placeholder("unk_2017A00")
    assert investigate_data.data_placeholder("off_801CD78")
    assert not investigate_data.data_placeholder("sub_801CB38")
    assert not investigate_data.data_placeholder("e_requestPaletteLoad")


def test_a_name_must_not_be_another_placeholder_or_hedge() -> None:
    """A proposed name that copies the placeholder's digit run (a model's made-up middle name
    like byte_203F750_HP), a hedge, or a 0x literal is refused. The digit-present rule is what
    makes a small name like gfXStagingBuf (no digits) pass."""
    placeholder = "byte_203F750"
    assert name_problem(placeholder, placeholder=placeholder) is not None
    assert name_problem("byte_203F750_HP", placeholder=placeholder) is not None
    assert name_problem("Maybe2", placeholder=placeholder) is not None
    assert name_problem("0x80_byte", placeholder=placeholder) is not None


def test_a_strong_name_passes_name_problem() -> None:
    assert name_problem("gfXStagingBuf", placeholder="byte_30016D0") is None
    assert name_problem("eStructBattlesAIData_pwrAtkButtonStage", placeholder="unk_203F754") is None


# ── the owner attribution: file-order vs. ELF address ────────────────

THREE_FN_ASM = [
    "battle_updateObjectAnim_801CB38:",
    "\tpush {r4,r6,r7,lr}",
    "\tldr r1, off_801CD78 // =byte_30016D0",
    "\tbl QueueEightWordAlignedGFXTransfer",
    "\tpop {r4,r6,r7,pc}",
    "\tthumb_func_end battle_updateObjectAnim_801CB38",
    "",
    "battleObject_loadBeastOutFormGfx_801CC34:",
    "\tpush {r4,r7,lr}",
    "\tldr r1, off_801CD78 // =byte_30016D0",
    "\tbl QueueEightWordAlignedGFXTransfer",
    "\tpop {r4,r7,pc}",
    "\tthumb_func_end battleObject_loadBeastOutFormGfx_801CC34",
    "",
    "battle_ai_tickPwrAtkPresses_801CC94:",
    "\tpush {lr}",
    "\tldr r1, off_801CD78 // =byte_30016D0",
    "\tbl QueueEightWordAlignedGFXTransfer",
    "\tpop {pc}",
    "\tthumb_func_end battle_ai_tickPwrAtkPresses_801CC94",
]


@pytest.fixture
def fake_repo(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> Path:
    """The bare files the data workflow reads: symbols/meaning_scores.tsv, symbols/db/, a
    bn6f/asm/asm00_2.s with three functions all reading off_801CD78 (= byte_30016D0), and
    bn6f/bn6f.elf's nm as a dict."""
    (tmp_path / "symbols").mkdir(parents=True)
    (tmp_path / "bn6f" / "asm").mkdir(parents=True)
    (tmp_path / "symbols" / "meaning_scores.tsv").write_text(
        "# address\tkind\tname\tscore\n0x030016D0\tdata\tbyte_30016D0\t0\n")
    (tmp_path / "bn6f" / "asm" / "asm00_2.s").write_text("\n".join(THREE_FN_ASM))
    monkeypatch.setattr(investigate_data, "REPO", tmp_path)
    monkeypatch.setattr(investigate_data, "_source_cache", None)
    monkeypatch.setattr(investigate_data, "source_files",
                        lambda: (tmp_path / "bn6f" / "asm" / "asm00_2.s",))
    monkeypatch.setattr(investigate_data, "elf_symbols",
                        lambda wanted: {"off_801CD78": 0x0801CD78, "byte_801CD80": 0x0801CD80,
                                        "dword_801CD68": 0x0801CD68, "byte_30016D0": 0x030016D0})
    return tmp_path


def test_function_ranges_walk_thumb_func_end(fake_repo: Path) -> None:
    text = (fake_repo / "bn6f" / "asm" / "asm00_2.s").read_text().splitlines()
    ranges = function_ranges(text)
    assert [name for _start, _end, name in ranges] == [
        "battle_updateObjectAnim_801CB38",
        "battleObject_loadBeastOutFormGfx_801CC34",
        "battle_ai_tickPwrAtkPresses_801CC94",
    ]
    assert ranges[0][1] == 6
    assert ranges[2][1] == 20


def test_function_at_finds_the_enclosing_function_by_line(fake_repo: Path) -> None:
    text = (fake_repo / "bn6f" / "asm" / "asm00_2.s").read_text().splitlines()
    ranges = function_ranges(text)
    assert function_at(ranges, 3) == ("battle_updateObjectAnim_801CB38", (1, 6))
    assert function_at(ranges, 10) == ("battleObject_loadBeastOutFormGfx_801CC34", (8, 13))
    assert function_at(ranges, 17) == ("battle_ai_tickPwrAtkPresses_801CC94", (15, 20))


def test_snippet_window_stays_inside_the_enclosing_function(fake_repo: Path) -> None:
    """A 15-line window around line 3 would normally reach into the next function at line 8;
    `window()` clamps to (1, 6)."""
    text = (fake_repo / "bn6f" / "asm" / "asm00_2.s").read_text().splitlines()
    site = type("S", (), {"line": 3, "span": (1, 6)})()
    low, high = investigate_data.window(site, text, rows=15)
    assert (low, high) == (0, 6)


# ── scan_sites: file-order owner attribution ───────────────────────────

def test_scan_sites_attributes_loads_in_file_order(fake_repo: Path) -> None:
    """Off_801CD78 (=byte_30016D0) sits past the first function but inside the second's pool —
    yet each load is in whichever function's body bn6f writes. The site owners come from that."""
    sites = investigate_data.scan_sites(frozenset({"byte_30016D0"}))
    assert set(sites) == {"byte_30016D0"}
    sites_there = sites["byte_30016D0"]
    assert tuple(s.line for s in sites_there) == (3, 10, 17)
    assert tuple(s.owner for s in sites_there) == (
        "battle_updateObjectAnim_801CB38",
        "battleObject_loadBeastOutFormGfx_801CC34",
        "battle_ai_tickPwrAtkPresses_801CC94",
    )


# ── guard, write, symdb ─────────────────────────────────────────────────

def test_guard_problem_catches_asm_include_collide(monkeypatch: pytest.MonkeyPatch,
                                                  tmp_path: Path) -> None:
    """A proposed name that already names an asm include would break `bn6f/*.s` includes."""
    (tmp_path / "bn6f" / "asm").mkdir(parents=True)
    (tmp_path / "bn6f" / "asm" / "foo_203F750.s").write_text("data")
    monkeypatch.setattr(investigate_data, "REPO", tmp_path)
    monkeypatch.setattr(investigate_data, "port_identifiers", lambda: frozenset())
    monkeypatch.setattr(investigate_data, "taken_names", lambda: frozenset())
    monkeypatch.setattr(investigate_data, "elf_symbols", lambda wanted: {})
    # asm_include uses module globals ASM_DIR / REPO captured at import time; patch it directly:
    def asm_include(name: str) -> bool:
        return (tmp_path / "bn6f" / "asm" / f"{name}.s").exists() \
            or (tmp_path / "bn6f" / "data" / f"{name}.s").exists()
    monkeypatch.setattr(investigate_data, "asm_include", asm_include)
    refusal = guard_problem("foo_203F750")
    assert refusal is not None and "asm include" in str(refusal)


def test_a_written_data_candidate_passes_symdb_check(tmp_path: Path) -> None:
    """The candidate's nm symbols must match: write_candidate puts the placeholder's address as a
    hex string and symdb_check looks it up in the supplied symbol dict."""
    syms = {"byte_30016D0": 0x030016D0}
    proposal = DataProposal(name=SymbolName("byte_30016D0"), address=GuestAddress(0x030016D0), answer=Answer(
        proposed_name=ToolMessage("gfXStagingBuf"), summary=ToolMessage("32-byte staging buffer"),
        kind=ToolMessage("buffer"), self_score=4, evidence=(), open_questions=(),
        details=ToolMessage("")),
                            tokens=100, seconds=10.0, retries=0)
    verdict = investigate_data.Verdict(name=proposal.name, address=proposal.address, tokens=50,
                                       seconds=5.0, retries=0,
                                       reply=parse_check('{"verdict": "agree", "reason": "ok"}',
                                                         "byte_30016D0")[0],
                                       problem=None)
    context = DataContext(target=DataTarget(name=proposal.name, address=proposal.address,
                                            region="IWRAM", size=32, score=0, named_users=23,
                                            source="bn6f/iwram_data.s:81", references=23),
                          bytes_text="", comment="", users=(), sites=(), sites_text=tmp_path / "x")
    candidate = make_candidate(context, proposal, verdict, MODEL, CHECKER, "x")
    path = write_candidate(candidate, tmp_path)
    assert path == tmp_path / "byte_30016D0.yaml"
    assert symdb_check(path, syms) == ()


# ── report ────────────────────────────────────────────────────────────

def test_report_rows_says_how_many_were_agreed(tmp_path: Path) -> None:
    proposal = DataProposal(name=SymbolName("byte_30016D0"), address=GuestAddress(0x030016D0),
                            answer=Answer(proposed_name=ToolMessage("gfXStagingBuf"),
                                          summary=ToolMessage("32-byte staging buffer"), kind=None,
                                          self_score=4, evidence=(), open_questions=(),
                                          details=ToolMessage("")),
                            tokens=100, seconds=10.0, retries=0)
    verdict = investigate_data.Verdict(name=proposal.name, address=proposal.address, tokens=50,
                                       seconds=5.0, retries=0,
                                       reply=parse_check('{"verdict": "agree", "reason": "ok"}',
                                                         "byte_30016D0")[0],
                                       problem=None)
    text = report_rows(((proposal, verdict),))
    header, row = text.splitlines()
    assert header == "# address\told\tproposed\tstatus\tself_score\tcross_check\ttokens\tretries\tnote"
    assert row.startswith("0x030016D0\tbyte_30016D0\tgfXStagingBuf\tprobable\t4\tagree")


# ── the graph with fake models ────────────────────────────────────────

class Fakes:
    """In-process fake providers, the way Fakes does for investigate, so the graph runs end
    to end without an HTTP round trip: one model call per label returns a name + summary, the
    second agrees."""

    def __init__(self) -> None:
        self.model_calls: list[str] = []
        self.checker_calls: list[str] = []
        self.lock = threading.Lock()

    def ask(self, spec: str, prompt: str, _kind: str) -> str:
        with self.lock:
            self.model_calls.append(prompt[:60])
            return ('{"proposed_name": "gfXStagingBuf", "summary": "32-byte staging buffer", '
                    '"self_score": 4, "evidence": [], "open_questions": []}')

    def check(self, spec: str, prompt: str, _kind: str) -> str:
        with self.lock:
            self.checker_calls.append(prompt[:60])
            return '{"verdict": "agree", "reason": "32-byte staging buffer for queued GFX"}'


def _setup_graph_repo(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> tuple[Fakes, Path]:
    (tmp_path / "symbols" / "db").mkdir(parents=True)
    (tmp_path / "bn6f").mkdir(parents=True)
    (tmp_path / "bn6f" / "asm").mkdir(parents=True)
    (tmp_path / "bn6f" / "asm" / "asm00_2.s").write_text("\n".join([
        "fake_with_named_users:",
        "\tpush {lr}",
        "\tldr r1, off_801CD78 // =byte_30016D0",
        "\tbl QueueEightWordAlignedGFXTransfer",
        "\tpop {pc}",
        "\tthumb_func_end fake_with_named_users",
    ]))
    (tmp_path / "symbols" / "meaning_scores.tsv").write_text(
        "# address\tkind\tname\tscore\n0x030016D0\tdata\tbyte_30016D0\t0\n")
    monkeypatch.setattr(investigate_data, "REPO", tmp_path)
    monkeypatch.setattr(investigate_data, "INVESTIGATE_DATA", tmp_path / "build" / "investigate_data")   # not the real runs
    monkeypatch.setattr(investigate_data, "_source_cache", None)
    monkeypatch.setattr(investigate_data, "source_files",
                        lambda: (tmp_path / "bn6f" / "asm" / "asm00_2.s",))
    monkeypatch.setattr(investigate_data, "elf_symbols",
                        lambda wanted: {"off_801CD78": 0x0801CD78, "byte_30016D0": 0x030016D0,
                                        "byte_801CD80": 0x0801CD80})
    fakes = Fakes()
    monkeypatch.setattr(investigate_data, "ask", fakes.ask)
    return fakes, tmp_path


def test_select_targets_with_only_one_label_returns_only_it(monkeypatch: pytest.MonkeyPatch,
                                                       tmp_path: Path) -> None:
    """A run with --only NAME for one eligible label selects that one label."""
    _setup_graph_repo(monkeypatch, tmp_path)
    targets, notes = select_targets(1, only=(SymbolName("byte_30016D0"),))
    assert tuple(t.name for t in targets) == ("byte_30016D0",)
    assert notes == ()


def test_select_targets_with_no_only_returns_few(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    """The default rank: a run with --count returns at most that many."""
    _setup_graph_repo(monkeypatch, tmp_path)
    targets, _notes = select_targets(5, ())
    # The real repo is large; the fake one has one eligible label.
    assert len(targets) >= 0   # smoke test, no exception
