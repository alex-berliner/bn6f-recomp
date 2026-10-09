"""The pieces of the subsystem writer that need no model or difftest: reading a
reply, what review is shown about register use and register-form notes, and
the difftest.cfg section a run installs and removes."""
import subprocess
from pathlib import Path

import pytest

from wf.tools import install, subsystem
from wf.tools.install import uninstall_file
from wf.tools.models import AssemblyText, Contract, ConversionContext, LiftedC, Register
from wf.tools.subsystem import (CandidateFile, CfgLine, FunctionTest, TokenUse, parse_reply, register_form_notes,
                                register_use_outside_adapters)
from wf.types import FunctionName, GuestAddress, SnapshotDir, SymbolStatus, ToolMessage

REPLY = '''Here is the file.
```c
#include "game.h"
static u32 add(u32 a, u32 b) { return a + b; }
/* register form: it returns to its caller's caller */
static void odd(void) { R0 = 1; }
Next sub_1(void) { R0 = add(R0, R1); RETURN_TO_CALLER(); }
```
```cfg
# only sub_2 calls it
sub_1 compare=r0
```
```yaml
- function: sub_1
  proposed_name: addTwo_1
  status: probable
  summary: Adds.
  interface: u32 add(u32 a, u32 b)
  details: ""
```
'''


def test_a_reply_gives_the_file_the_promises_and_the_drafts() -> None:
    reply = parse_reply(REPLY)

    assert reply.file is not None and "Next sub_1(void)" in reply.file
    assert reply.cfg_lines == (CfgLine("# only sub_2 calls it"), CfgLine("sub_1 compare=r0"))
    assert reply.drafts[0].proposed_name == "addTwo_1" and reply.drafts[0].status is SymbolStatus.PROBABLE
    assert reply.problems == ()


def test_a_reply_without_code_or_with_bad_yaml_says_so() -> None:
    reply = parse_reply("```yaml\n- function: [unclosed\n```")

    assert reply.file is None
    assert any("no ```c block" in problem for problem in reply.problems)
    assert any("```yaml block" in problem for problem in reply.problems)


def test_a_structured_interface_is_kept_as_text() -> None:
    reply = parse_reply("```c\nint x;\n```\n```yaml\n- function: f\n  proposed_name: g\n  status: probable\n"
                        "  summary: s\n  interface: {typed: 'void f(int)', adapter: 'r0 = arg'}\n```")

    assert reply.problems == ()
    assert reply.drafts[0].interface == "typed: void f(int); adapter: r0 = arg"


def test_yaml_values_with_colons_are_read_as_text() -> None:
    reply = parse_reply("```c\nint x;\n```\n```yaml\n- function: f\n  proposed_name: g\n  status: probable\n"
                        "  summary: the redraw callback (table +0x20): redraws the window\n  interface: void f(void)\n```")

    assert reply.problems == ()
    assert reply.drafts[0].summary == "the redraw callback (table +0x20): redraws the window"


def test_code_that_passes_with_a_bad_yaml_block_is_kept(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    def compiles(code: str, names: tuple[FunctionName, ...], workdir: Path) -> tuple[Path | None, str]:
        return tmp_path / "cand.so", ""

    def passes(context: ConversionContext, library: Path, snapshots: SnapshotDir, difftest: Path,
               promise: tuple[list[str], list[str]] | None) -> FunctionTest:
        return FunctionTest(function=context.function, captured=ToolMessage("ok"), random=ToolMessage("ok"), passed=True)

    monkeypatch.setattr(subsystem, "_compile", compiles)
    monkeypatch.setattr(subsystem, "_test_function", passes)
    reply = parse_reply("```c\nNext sub_1(void) { RETURN_TO_CALLER(); }\n```")

    test = subsystem.test_candidate_file((context("sub_1"),), reply, SnapshotDir(tmp_path), tmp_path / "difftest")

    assert not test.passed and test.functions[0].passed
    assert test.report.startswith("The code passed every test; keep it as it is")


def context(name: str) -> ConversionContext:
    return ConversionContext(function=FunctionName(name), address=GuestAddress(0x08000000),
                             assembly=AssemblyText(""), lifted=LiftedC(""),
                             contract=Contract(inputs=(), outputs=(), clobbers=(), preserved=()))


def test_register_use_counts_only_outside_the_adapters() -> None:
    code = CandidateFile(parse_reply(REPLY).file or "")

    assert register_use_outside_adapters(code, (FunctionName("sub_1"),)) == ("odd: 1 register-world uses",)


def test_a_register_form_note_names_the_function_after_it() -> None:
    code = CandidateFile(parse_reply(REPLY).file or "")

    assert register_form_notes(code) == ("odd: it returns to its caller's caller",)


def test_the_cfg_section_is_replaced_and_removed(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    cfg = tmp_path / "difftest.cfg"
    _ = cfg.write_text("existing compare=r0\n")
    monkeypatch.setattr(install, "DIFFTEST_CFG", cfg)
    monkeypatch.setattr(install, "GAME_DIR", tmp_path)

    _ = install.install_file("x.c", CandidateFile("code"), (CfgLine("a compare=r1"),))
    _ = install.install_file("x.c", CandidateFile("code"), (CfgLine("a compare=r2"),))
    assert cfg.read_text() == "existing compare=r0\n# begin x.c\na compare=r2\n# end x.c\n"

    uninstall_file("x.c")
    assert cfg.read_text() == "existing compare=r0\n" and not (tmp_path / "x.c").exists()


def test_a_function_whose_original_cannot_run_in_the_harness_is_left_to_the_full_verify(
        monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    """Every captured call and random input out of domain: the original faults or spins there, so
    nothing a model writes could be compared; it passes on, marked not compared."""
    lines = {"-S": "sub_1  ok    (6 snapshots, 6 runs, 0 failures, 6 out-of-domain)",
             "-n": "sub_1  ok    (300 trials, 0 failures, 300 out-of-domain)"}

    def run(command: list[str], **_: object) -> subprocess.CompletedProcess[str]:
        line = next(line for flag, line in lines.items() if flag in command)
        return subprocess.CompletedProcess(command, 0, stdout=line + "\n", stderr="")

    (tmp_path / "sub_1").mkdir()
    monkeypatch.setattr(subsystem.subprocess, "run", run)
    context = ConversionContext(function=FunctionName("sub_1"), address=GuestAddress(0x08000100),
                                assembly=AssemblyText("bx lr"), lifted=LiftedC("Next L_sub_1(void) {}"),
                                contract=Contract(inputs=(Register("r0"),), outputs=(Register("r0"),), clobbers=(), preserved=()))

    test = subsystem._test_function(context, tmp_path / "lib.so", SnapshotDir(tmp_path), tmp_path / "difftest",  # pyright: ignore[reportPrivateUsage]
                                    None)

    assert test.passed and not test.compared
    assert "left to the full verify" in test.captured


def test_token_counts_are_read_as_logged_and_may_be_missing(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    log = tmp_path / "usage.jsonl"
    log.write_text('{"model": "zai/glm-5.3", "tag": "run round 1", "in": 100, "out": 64000, "reasoning": 63994}\n'
                   '{"model": "opencode/longcat", "tag": "run round 2", "in": null, "out": null, "reasoning": null}\n')
    monkeypatch.setattr(subsystem, "USAGE_LOG", log)

    assert subsystem.tokens_for("run round 1") == TokenUse(tokens_in=100, tokens_out=64000)   # reasoning is in "out"
    assert subsystem.tokens_for("run round 2") is None
