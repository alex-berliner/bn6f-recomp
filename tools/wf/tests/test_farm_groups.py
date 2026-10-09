"""Which functions a farm group drops, from what the quick test reports about
the assembled file: the part holding a stray definition (not the function it
defines), and the part a compile error or its macro note points into."""
from wf.graphs.convert import Attempt, failing_functions
from wf.graphs.verify import Verdict, VerifyReport
from wf.tools.drafts import SECTION
from wf.tools.subsystem import CandidateFile, FileReply, FileTest
from wf.types import CommitHash, FunctionName, ModelSpec, RouteName, SourceVersion, ToolMessage

FILE = "\n".join([
    SECTION.format(function="first"),       # line 1
    "Next first(void) { RETURN_TO_CALLER(); }",
    SECTION.format(function="second"),      # line 3
    "Next second(void) { RETURN_TO_CALLER(); }",
    "Next first(void)",                     # line 5: a stray copy, brace a line down
    "{",
    "}",
    SECTION.format(function="third"),       # line 8
    "Next third(void) { R0 = A_table; RETURN_TO_CALLER(); }",
]) + "\n"


def failing(report: str) -> dict[FunctionName, str]:
    reply = FileReply(file=CandidateFile(FILE), cfg_lines=(), drafts=(), problems=(), seconds=0.0)
    test = FileTest(passed=False, report=ToolMessage(report), functions=(), register_use_outside_adapters=(),
                    register_form_notes=())
    return failing_functions(Attempt(number=1, model=ModelSpec("hyper/cheap"), given=True, reply=reply, test=test))


def test_a_definition_twice_drops_the_part_holding_the_copy() -> None:
    assert list(failing("`Next first(void)` must be defined exactly once (found 2)")) == ["second"]


def test_a_definition_outside_the_group_drops_the_part_holding_it() -> None:
    assert list(failing("defines functions outside the subsystem: first")) == ["second"]


def test_an_error_inside_a_header_drops_the_part_its_note_points_into() -> None:
    report = ("compile errors:\ninclude/bn6f_syms.h:2811:35: error: makes pointer from integer\n"
              "line 9:25: note: in expansion of macro 'A_table'")
    assert list(failing(report)) == ["third"]


def test_a_route_difference_no_check_names_sets_the_later_half_aside() -> None:
    reply = FileReply(file=CandidateFile(FILE), cfg_lines=(), drafts=(), problems=(), seconds=0.0)
    report = VerifyReport(sources=SourceVersion(commit=CommitHash("c0ffee"), has_uncommitted_changes=True),
                          verdict=Verdict.FAILED, build_failure=None, failed_functions=(), oracle_differences=(),
                          never_compared=(), routes_checked=(RouteName("story2"),), failed_routes=(RouteName("story2"),),
                          battle_sweep=None)
    attempt = Attempt(number=1, model=ModelSpec("hyper/cheap"), given=True, reply=reply, verification=report)

    assert list(failing_functions(attempt)) == ["second", "third"]
