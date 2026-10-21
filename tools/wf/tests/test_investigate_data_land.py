"""The pure parts of tools/investigate_data_land.py: how a data label's proposed name is checked
against the data style rule (lowerCamel, PascalCase, e/o/s/i prefixes allowed where the project
already spells them with upper-case letters, no Maybe, no 0x literal, no digit run copied from
the label's address), how the offset-expression detector scans bn6f, how port A_<old> renames
are visible in cport/, and how select() filters a run's report. The runtime pieces (elf_symbols,
port_identifiers, symdb_check_all, run_record) need the real repo and are exercised by the
tool's --selfcheck instead."""
from __future__ import annotations

import os
import sys
import textwrap
from pathlib import Path

import pytest

# tools/investigate_data_land.py lives in tools/. The test runner is tools/wf/; add tools/ to
# sys.path so `import investigate_data_land` resolves it without an installed package.
_TOOLS = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(_TOOLS))

import investigate_data_land as data_land  # noqa: E402  (the tool under test)


# ── the placeholder rule ──────────────────────────────────────────────

class TestDataPlaceholder:
    def test_byte_is_a_placeholder(self) -> None:
        assert data_land.data_placeholder("byte_203F750") is True

    def test_unk_is_a_placeholder(self) -> None:
        assert data_land.data_placeholder("unk_2017A00") is True

    def test_dword_is_a_placeholder(self) -> None:
        assert data_land.data_placeholder("dword_2010CCC") is True

    def test_off_is_a_placeholder(self) -> None:
        assert data_land.data_placeholder("off_801CD78") is True

    def test_sub_is_a_code_placeholder_not_data(self) -> None:
        assert data_land.data_placeholder("sub_801CB38") is False

    def test_a_real_name_is_not_a_placeholder(self) -> None:
        assert data_land.data_placeholder("eBattleFolder") is False


# ── the style rule (data shapes) ──────────────────────────────────────

class TestStyleProblem:
    def test_lowerCamel_plain_passes(self) -> None:
        assert data_land.style_problem("battleBestTimes") is None

    def test_PascalCase_plain_passes(self) -> None:
        # data has 198 CamelCase names (e.g. ChatboxPalette); the function rule's lowerCamel-only
        # would drop them
        assert data_land.style_problem("ChatboxPalette") is None

    def test_BBS_acronym_start_passes(self) -> None:
        # BBSMessageCounts starts with an all-caps acronym; lowerCamel is `[a-z][A-Za-z0-9]*`
        # which would drop it. PascalCase `[A-Z][A-Za-z0-9]+` saves it.
        assert data_land.style_problem("BBSMessageCounts") is None

    def test_lower_prefix_with_underscore_passes(self) -> None:
        assert data_land.style_problem("chatbox_textSlots") is None

    def test_allowed_uppercase_prefix_passes(self) -> None:
        # eRFUState is in the allowed set; the tail's PascalCase is a `e<Struct>_<Field>` shape
        assert data_land.style_problem("eRFUState_CallbackData",
                                       frozenset({"eRFUState"})) is None

    def test_disallowed_uppercase_prefix_fails(self) -> None:
        # no allowed prefix; eBattle_xxx doesn't match the data conventions
        assert data_land.style_problem("eBattle_TrackedTags") is not None

    def test_more_than_one_underscore_fails(self) -> None:
        assert data_land.style_problem("battle_step_commitMoveAndHit") == "more than one underscore"

    def test_0x_literal_fails(self) -> None:
        assert data_land.style_problem("pSTWI_0x80") == "it carries a 0x literal"

    def test_trailing_address_run_fails(self) -> None:
        assert data_land.style_problem("pSTWI_2010CCC") is not None

    def test_maybe_fails(self) -> None:
        assert data_land.style_problem("maybeLoadSprite") == "it hedges (Maybe)"

    def test_empty_prefix_fails(self) -> None:
        # the function landing's rule says "the prefix '' is not lower-case"
        assert data_land.style_problem("_chatbox") is not None


# ── the digit-copied-from-address rule ─────────────────────────────────

class TestAddressDigitProblem:
    def test_full_address_copy(self) -> None:
        problem = data_land.address_digit_problem("loadFFC_203F5A4", "0x0203F5A4")
        assert problem is not None and "203F5A4" in problem and "0x0203F5A4" in problem

    def test_partial_address_copy(self) -> None:
        # FFC is a 3-char substring of the address
        problem = data_land.address_digit_problem("loadFFC", "0x080BFFC0")
        assert problem is not None and "FFC" in problem

    def test_no_digits_passes(self) -> None:
        assert data_land.address_digit_problem("battleBestTimes", "0x0203F5A4") is None

    def test_short_digit_run_passes(self) -> None:
        # a 2-character digit run does not count (the rule starts at 3+ chars)
        assert data_land.address_digit_problem("load80", "0x080BFFC0") is None


# ── the prefix/skeleton helpers ───────────────────────────────────────

class TestPrefixHelpers:
    def test_camel_prefix_in_use(self) -> None:
        # eBattle_xxx would be in use when any symbol/db name spells it `eBattle_`
        assert "eBattle" in data_land.prefixes_in_use(
            ["eBattle_runPhase", "chatbox_blitGlyph", "sub_1"])

    def test_camel_prefix_not_in_use_does_not_count(self) -> None:
        # a camelCase prefix no name uses stays out
        assert "writeChipParams" not in data_land.prefixes_in_use(["chatbox_blitGlyph"])

    def test_lowercase_prefix_does_not_need_allowance(self) -> None:
        # lower-case prefixes already match the rule on their own
        assert "chatbox" not in data_land.prefixes_in_use(["chatbox_x"])


class TestStyleFixed:
    def test_lowercamel_stays(self) -> None:
        # already valid: stay as-is
        assert data_land.style_fixed("chatboxBlitGlyph", frozenset()) == "chatboxBlitGlyph"

    def test_pascalcase_stays(self) -> None:
        # data allows PascalCase plain names; the function landing would lowercase them, the
        # data landing does not.
        assert data_land.style_fixed("ChatboxPalette", frozenset()) == "ChatboxPalette"

    def test_allowed_prefix_with_pascal_tail_stays(self) -> None:
        # eBattle_TrackedTags: prefix allowed, tail is PascalCase (e<Struct>_<Field>)
        assert (data_land.style_fixed("eBattle_TrackedTags", frozenset({"eBattle"}))
                == "eBattle_TrackedTags")

    def test_disallowed_prefix_folds(self) -> None:
        # writeChipParams_xxx would fold into one word (the prefix isn't a data one)
        assert data_land.style_fixed("writeChipParams_chip74", frozenset()) == "writeChipParamsChip74"

    def test_two_underscore_name_lowercases_the_tail(self) -> None:
        assert (data_land.style_fixed("ai_attackStep_CommitMoveAndHit", frozenset())
                == "ai_attackStepCommitMoveAndHit")

    def test_maybe_returns_none(self) -> None:
        assert data_land.style_fixed("maybe_loadSprite", frozenset()) is None

    def test_0x_literal_returns_none(self) -> None:
        assert data_land.style_fixed("pSTWI_0x80", frozenset()) is None


# ── the data-specific guards: offset expression & cport A_<old> ──────

class TestOffsetExpression:
    def test_no_offset_stays_clean(self) -> None:
        # When the offset pattern file cache was never set, every name returns no hits.
        # A test that drives the offset scanner follows below by patching _OFFSET_FILES.
        data_land._OFFSET_FILES = None
        assert data_land.offset_expression_problem("byte_203CA70") is None

    def test_offset_expression_is_detected(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        """A line like `.set OFFSET, byte_203CA74 - 0x203ca70` is detected; a bare `=byte_203F5A4`
        alone is not (no arithmetic operator, just a literal pool). The `byte_203F5A4 + 4` in the
        data file is an arithmetic expression and IS detected."""
        (tmp_path / "asm").mkdir()
        (tmp_path / "data").mkdir()
        (tmp_path / "asm" / "test.s").write_text(textwrap.dedent("""\
            battle_render:
                push {lr}
                ldr r0, off_801CA78 // =byte_203CA70
                ldr r1, [r0, #-4]  // byte_203CA74 - 0x203ca70
                .set OFFSET, byte_203CA74 - 0x203ca70
                .set BASE, byte_203F5A4
                pop {pc}
        """))
        (tmp_path / "data" / "render.s").write_text("byte_203F5A4 + 4\n")
        # patch the cache to point at our synthetic files
        data_land._OFFSET_FILES = (tmp_path / "asm" / "test.s", tmp_path / "data" / "render.s")
        try:
            # byte_203CA74 appears in two arithmetic sites
            problem = data_land.offset_expression_problem("byte_203CA74")
            assert problem is not None
            assert "needs manual review" in problem
            assert "test.s:" in problem
            assert "byte_203CA74 - 0x203ca70" not in problem         # file:line format
            # byte_203F5A4 alone is `byte_203F5A4` (the literal pool) with no operator: skip
            # But the data/render.s line `byte_203F5A4 + 4` IS an arithmetic expression and
            # IS detected.
            problem_f5a4 = data_land.offset_expression_problem("byte_203F5A4")
            assert problem_f5a4 is not None
            assert "render.s:1" in problem_f5a4
        finally:
            data_land._OFFSET_FILES = None

    def test_offset_expression_reports_first_hit_site(
        self, tmp_path: Path) -> None:
        (tmp_path / "asm").mkdir()
        (tmp_path / "asm" / "test.s").write_text(
            "\t.set OFFSET, byte_203CA74 - 0x203ca70\n")
        data_land._OFFSET_FILES = (tmp_path / "asm" / "test.s",)
        try:
            problem = data_land.offset_expression_problem("byte_203CA74")
            assert problem is not None
            assert "test.s:1" in problem
        finally:
            data_land._OFFSET_FILES = None


class TestPortA:
    def test_port_a_refs_counts_known_reference(self, tmp_path: Path,
                                                 monkeypatch: pytest.MonkeyPatch) -> None:
        """`A_<old>` references in cport/ are how many lines need the `A_` rewrite; the
        record workflow handles it (tools/rename_symbol.py recomp_files prefixes `A_`).
        We build a tiny cport/ folder and ask for the count of one name."""
        (tmp_path / "cport" / "src" / "game").mkdir(parents=True)
        (tmp_path / "cport" / "src" / "game" / "test.c").write_text(
            "A_byte_203F5A4;\n"
            "    R0 = A_byte_203F5A4 + 4;\n"
            "    LD32(A_byte_203F5A4) ; /* hits again */\n"
            "    no_match_here\n"
            "    A_byte_203F5A4_something\n"        # not a word match (longer)
        )
        # patch PortCaches via path.glob; point ROOT/port_identifiers at our tmp_path
        # the actual port_identifiers uses CWD's cport/ — patch via use_root-like behavior.
        # Simpler: patch _OFFSET_FILES to None and call _PORT_TOKEN over the file directly.
        # But the tool's port_a_refs uses Path(ROOT).glob, so the easiest path is to
        # construct a fresh root pointing at tmp_path.
        monkeypatch.setattr(data_land, "ROOT", str(tmp_path))
        # invalidate cache
        data_land._PORT_IDS = None
        try:
            count = data_land.port_a_refs("byte_203F5A4")
            assert count == 3           # three A_<name> word-matches in the file
        finally:
            data_land._PORT_IDS = None
            # restore the real ROOT so subsequent tests don't see the patched layout
            monkeypatch.undo()


# ── the candidate yaml ────────────────────────────────────────────────

class TestEntryOf:
    def test_data_kind_and_status(self) -> None:
        candidate = {"name": "byte_203F5A4", "address": "0x0203F5A4", "kind": "data",
                     "status": "probable",
                     "summary": "Proposed name: foo. The link packet.",
                     "region": "EWRAM", "size": 64,
                     "details": "Filled in by buildEnemyLinkPacket.",
                     "evidence": [{"method": "caller-analysis", "date": "2026-10-20", "detail": "ok",
                                   "refs": ["bn6f/asm/asm32.s:1234"],
                                   "reproduce": "grep -n -w byte_203F5A4 bn6f/ewram.s",
                                   "result": "proposed by minimax/MiniMax-M3, self_score 4; "
                                             "cross-checked by minimax/MiniMax-M3: agree"}],
                     "open_questions": ["bigger?"]}
        entry = data_land.entry_of(candidate, "investigate-data-20261020-192515")
        assert entry["kind"] == "data"
        assert entry["status"] == "probable"
        assert entry["name"] == "byte_203F5A4"          # the current placeholder (record renames it)
        assert entry["region"] == "EWRAM"
        assert entry["size"] == 64
        assert entry["address"] == "0x0203F5A4"

    def test_evidence_records_the_run_id(self) -> None:
        candidate = {"name": "byte_203F5A4", "address": "0x0203F5A4",
                     "summary": "x", "evidence": [
                         {"method": "caller-analysis", "date": "2026-10-20", "detail": "ok",
                          "refs": ["bn6f/asm/asm32.s:1"],
                          "reproduce": "grep -n -w byte_203F5A4 bn6f/ewram.s",
                          "result": "agree"}]}
        entry = data_land.entry_of(candidate, "investigate-data-20261020-192515")
        assert "wf investigate-data investigate-data-20261020-192515" in entry["evidence"][0]["result"]

    def test_rescue_appends_first_evidence(self) -> None:
        candidate = {"name": "byte_203F5A4", "address": "0x0203F5A4",
                     "summary": "x", "evidence": [
                         {"method": "caller-analysis", "date": "2026-10-20", "detail": "ok",
                          "refs": ["bn6f/asm/asm32.s:1"],
                          "reproduce": "grep -n -w byte_203F5A4 bn6f/ewram.s",
                          "result": "agree"}]}
        entry = data_land.entry_of(candidate, "run1",
                                    rescue="deduped by tools/investigate_data_land.py dedupe")
        assert ("deduped by tools/investigate_data_land.py dedupe"
                in entry["evidence"][0]["result"])


# ── the selection filtering ───────────────────────────────────────────

class TestSelect:
    def _row(self, ts_line: str) -> data_land.Row:
        return data_land.Row(ts_line)

    def _setup(self, run_dir: Path, olds: list[str]) -> None:
        for old in olds:
            (run_dir / f"{old}.yaml").write_text(
                "name: " + old + "\naddress: '0x02000000'\nkind: data\n"
                "status: probable\nregion: EWRAM\nsize: 4\n"
                "summary: A label.\n")
        # db_names sees the candidates folder for db_names(); also fake a symbols/db dir
        # by pointing DB at run_dir's parent (which is /tmp here).

    def _patch_db(self, monkeypatch: pytest.MonkeyPatch, db_dir: Path) -> None:
        monkeypatch.setattr(data_land, "DB", str(db_dir))

    def test_select_keeps_only_clean_proposed(
            self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        # names that don't collide with anything in a real bn6f/, so the test is independent
        # of where it's run.
        rows = [
            self._row("0x02010CCC\tdword_20100CC\tgoodName\tprobable\t4\tagree\t100\t0\t-"),
            self._row("0x02010CD4\tdword_20100D4\tmaybeX\tprobable\t4\tagree\t100\t0\t-"),
            self._row("0x02010CD8\tdword_20100D8\tbad__name\tprobable\t4\tagree\t100\t0\t-"),
            self._row("0x02010CE0\tdword_20100E0\tdupName\tprobable\t5\tagree\t100\t0\t-"),
            self._row("0x02010CE4\tdword_20100E4\tdupName\thypothesis\t4\tagree\t100\t0\t-"),
            self._row("0x02010CE8\tdword_20100E8\tportIdent\tprobable\t4\tagree\t100\t0\t-"),
            self._row("0x02010CF0\tdword_20100F0\tasm\thypothesis\t4\tnone\t100\t0\t-"),
        ]
        run_dir = tmp_path
        for old in (
                "dword_20100CC", "dword_20100D4", "dword_20100D8",
                "dword_20100E0", "dword_20100E4", "dword_20100E8"):
            (run_dir / f"{old}.yaml").write_text(
                "name: " + old + "\naddress: '0x02000000'\nkind: data\n"
                "status: probable\nregion: EWRAM\nsize: 4\n"
                "summary: A label.\n")
        # empty DB and no offset-expression files (a real worktree's bn6f/ could have labels
        # in expressions) so the test is independent of where it runs.
        db_dir = tmp_path / "fakedb"
        db_dir.mkdir(parents=True, exist_ok=True)
        monkeypatch.setattr(data_land, "DB", str(db_dir))
        monkeypatch.setattr(data_land, "_OFFSET_FILES", ())
        syms = {}
        port = frozenset({"portIdent"})
        chosen, skipped = data_land.select(rows, str(run_dir), syms, port, 30, frozenset())
        kept_old = [r.old for r, _ in chosen]
        kept_name = [r.proposed for r, _ in chosen]
        assert kept_old == ["dword_20100CC"]
        assert kept_name == ["goodName"]
        reasons = {old: reason for _, old, _, reason in skipped}
        # the cport-collide row was the only filter candidate for port_identifiers:
        assert reasons["dword_20100E8"] == (
                "the name is already an identifier in cport/ (grep -w): the rename would collide with it")
        # the maybe hedge and double underscore are naming style rejects:
        assert reasons["dword_20100D4"].startswith("naming style:")
        assert reasons["dword_20100D8"].startswith("naming style:")
        # the duplicate-name proposal (the probable row in the pair) is the dup reason:
        assert reasons["dword_20100E0"] == "proposed for 2 labels in the run"

    def test_select_drops_dup_names_across_rows(
            self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        rows = [
            self._row("0x02010CD0\tdword_20100F0\tdupName\tprobable\t5\tagree\t100\t0\t-"),
            self._row("0x02010CD4\tdword_20100F4\tdupName\thypothesis\t5\tagree\t100\t0\t-"),
        ]
        run_dir = tmp_path
        db_dir = tmp_path / "fakedb"
        db_dir.mkdir(parents=True, exist_ok=True)
        monkeypatch.setattr(data_land, "DB", str(db_dir))
        monkeypatch.setattr(data_land, "_OFFSET_FILES", ())
        # neither row has a yaml file, so they fall out as "no candidate yaml"
        syms = {}
        port = frozenset()
        chosen, skipped = data_land.select(rows, str(run_dir), syms, port, 30, frozenset())
        kept_old = [r.old for r, _ in chosen]
        # hypothesis is excluded by status; probable is excluded by missing yaml
        assert kept_old == []
        # the hypothesis row was filtered out by status (not in skipped either)
        assert "dword_20100F4" not in {old for _, old, _, _ in skipped}
