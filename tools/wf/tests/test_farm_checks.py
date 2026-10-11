"""The typed prompt's tool pieces in tools/convert/farm.py (prompt version typed-2): the register-form
notes the tool writes, the report of every problem and how it is read back, the ranking of attempts,
and the callees' generated wrappers (tools/abi/gen.py). No compiler or difftest runs here."""
import sys
from collections.abc import Callable
from types import ModuleType
from typing import cast

from wf.repo import REPO
from wf.tools.models import FARM

sys.path.insert(0, str(REPO / "tools/abi"))
import gen  # noqa: E402  # pyright: ignore[reportMissingImports]

note_register_forms = cast(Callable[[str], str], FARM.note_register_forms)  # pyright: ignore[reportAny]
report_sections = cast(Callable[[str], dict[str, str]], FARM.report_sections)  # pyright: ignore[reportAny]
first_failure = cast(Callable[[str], tuple[str | None, str]], FARM.first_failure)  # pyright: ignore[reportAny]
rank = cast(Callable[[bool, str], tuple[int, float, int]], FARM.rank)  # pyright: ignore[reportAny]
verdict = cast(Callable[[bool, str], str], FARM.verdict)  # pyright: ignore[reportAny]

COMPILE = "compile errors:\nline 4: error: 'x' undeclared\n    x = 1;"
REGISTERS = ("guest registers used where the conventions do not allow them:\n"
             "- helper (line 2, a typed function): regs on lines 3\nMove the logic ...")
CAPTURED = ("captured calls: f  FAIL  (10 snapshots, 10 runs, 3 failures, 0 out-of-domain)\n"
            "f snap_0.bin 0: guest memory differs")
RANDOM = ("captured calls: f  ok  (10 snapshots, 10 runs, 0 failures, 0 out-of-domain)\n"
          "random inputs: f  FAIL  (300 trials, 9 failures, 0 out-of-domain)")
BEHAVES = ("captured calls: f  ok  (10 snapshots, 10 runs, 0 failures, 0 out-of-domain)\n"
           "random inputs: f  ok  (300 trials, 0 failures, 12 out-of-domain)")


def test_the_tool_notes_a_call_helper_on_its_own_line() -> None:
    code = ("static bool flag_set(u32 flag)\n{\n    R0 = flag;\n    GUEST_CALL(TestEventFlag);\n"
            "    return !ZERO_FLAG();\n}\n")
    noted = note_register_forms(code)
    assert noted.count("\n") == code.count("\n")          # the report's line numbers stay the candidate's
    assert "{ /* register form: calls TestEventFlag, which has no typed C interface yet (noted by the tool) */" in noted


def test_the_tool_does_not_explain_logic_in_registers() -> None:
    code = "static u32 pick(u32 a) {\n    if (a) { R0 = a; GUEST_CALL(Foo); }\n    return R0;\n}\n"
    assert note_register_forms(code) == code


def test_a_note_in_other_words_is_put_in_the_lints_words() -> None:
    code = "/* Register-form helper: calls Foo */\nstatic void f(void) { GUEST_CALL(Foo); }\n"
    assert note_register_forms(code).startswith("/* register form: calls Foo */")
    mention = "/* this was register form, now typed */\nstatic u32 g(u32 a) { return a; }\n"
    assert note_register_forms(mention) == mention


def test_every_problem_is_reported_and_read_back() -> None:
    report = "\n\n".join((COMPILE, REGISTERS))
    assert set(report_sections(report)) == {"compile", "registers"}
    assert first_failure(report)[0] == "registers"        # where typed-1's test stopped: pool.py's verdicts keep
    assert first_failure(BEHAVES)[0] is None
    assert first_failure("\n\n".join((RANDOM, REGISTERS)))[0] == "registers"


def test_attempts_rank_as_review_3_says() -> None:
    ranked = sorted([("compile", rank(False, COMPILE)), ("captured", rank(False, CAPTURED)),
                     ("random", rank(False, RANDOM)), ("behaves", rank(False, "\n\n".join((BEHAVES, REGISTERS)))),
                     ("passed", rank(True, BEHAVES)), ("entry", rank(False, "the code must define `Next f(void)`"))],
                    key=lambda item: item[1])
    assert [name for name, _ in ranked] == ["passed", "behaves", "random", "captured", "compile", "entry"]
    assert verdict(False, CAPTURED) == "differs on 3 of 10 captured calls"
    assert verdict(False, "\n\n".join((BEHAVES, REGISTERS))) == "behaves the same; fails the register check"


def test_a_wrapper_is_generated_from_the_lifted_contract() -> None:
    make = cast(Callable[[str, list[str], list[str], str], object], cast(ModuleType, gen).contract_wrapper)  # pyright: ignore[reportAny]
    one = make("Foo", ["r0", "r1", "r10", "lr"], ["r0"], "generated wrapper")
    assert cast(str, getattr(one, "declaration")).startswith("u32 call_Foo(u32 r0, u32 r1);")
    flags = make("TestEventFlag", ["r0", "r10", "lr"], ["nz", "c", "v", "r0", "r1", "r3"], "generated wrapper")
    definition = cast(str, getattr(flags, "definition"))
    assert "typedef struct { u32 r0, r1, r3; bool z, n, c, v; } call_TestEventFlag_result;" in definition
    assert "/* register form: generated wrapper */" in definition and "R10" not in definition
    none = make("Bar", [], ["c"], "generated wrapper")
    assert cast(str, getattr(none, "declaration")).startswith("call_Bar_result call_Bar(void);")
