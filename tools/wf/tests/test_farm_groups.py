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


def test_a_part_that_does_not_fit_the_laid_out_files_is_dropped_with_why() -> None:
    found = failing("layout: second: cport/src/game/object.c: second uses K, which the part of x defines")

    assert found == {FunctionName("second"): "does not fit the laid-out files: cport/src/game/object.c: second "
                                             "uses K, which the part of x defines"}


def test_a_group_keeps_its_own_definitions_apart_from_the_file_it_goes_to() -> None:
    from wf.tools.drafts import _names_kept_apart  # pyright: ignore[reportPrivateUsage]
    first = "#define SPEED 4\nstatic void set_flag(void) {}\nNext first(void) { R0 = SPEED; set_flag(); }"
    second = "#define SPEED 4\nNext second(void) { R0 = SPEED; }"

    kept = dict(_names_kept_apart([(FunctionName("first"), first), (FunctionName("second"), second)],
                                  {FunctionName("first"): frozenset({"set_flag", "count"})}))

    assert "static void set_flag_first(void)" in kept[FunctionName("first")]       # its file has one
    assert "#define SPEED_first 4" in kept[FunctionName("first")]       # the group's second part uses SPEED
    assert "#define SPEED_second 4" in kept[FunctionName("second")]


def test_a_group_member_reading_a_header_macro_another_redefines_keeps_the_headers() -> None:
    from wf.tools.drafts import _names_kept_apart  # pyright: ignore[reportPrivateUsage]
    defines = "#define oFlags 4\nNext first(void) { R0 = LD8(R5 + oFlags); }"
    reads = "Next second(void) { R0 = LD8(R5 + oFlags); }"                  # oFlags from the headers

    kept = dict(_names_kept_apart([(FunctionName("first"), defines), (FunctionName("second"), reads)], {}))

    assert "#define oFlags_first 4" in kept[FunctionName("first")] and "R5 + oFlags_first" in kept[FunctionName("first")]
    assert kept[FunctionName("second")] == reads
