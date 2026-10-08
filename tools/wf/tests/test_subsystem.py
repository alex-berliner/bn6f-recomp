"""The pieces of the subsystem writer that need no model or difftest: reading a
reply, what review is shown about register use and register-form notes, and
the difftest.cfg section a run installs and removes."""
from pathlib import Path

import pytest

from wf.tools import install
from wf.tools.install import uninstall_file
from wf.tools.subsystem import CandidateFile, CfgLine, parse_reply, register_form_notes, register_use_outside_adapters
from wf.types import FunctionName, SymbolStatus

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
