"""tools/layout.py, the layout of cport/src/game: a function converted on its own lives in the file of
the bn6f source that defines it (big sources in parts), in address order; inbox files are emptied into
those files; a part must be self-contained where it lands; symbols/db follows the moves."""
from pathlib import Path
from typing import cast

import yaml

import wf.repo  # noqa: F401  # pyright: ignore[reportUnusedImport]  # puts tools/ on the path
import layout  # tools/layout.py

# a fake bn6f: two sources, object.s from 0x08001000 and npc.s from 0x08002000, npc.s cut at 0x08002800
BN6F = layout.Bn6f(symbols={"objA": 0x08001100, "objB": 0x08001200, "npcA": 0x08002100, "npcB": 0x08002900,
                            "npcC": 0x08002904},
                   starts=(0x08001000, 0x08002000), sources=("object.s", "npc.s"))
TABLE = {"npc": (0x08002800,)}

FARM = ('/*\n * leaves_farm_1.c: small functions from the model farm, set 1.\n */\n'
        '#include "game.h"\n#include "runtime.h"\n\n'
        '/* ── npcB ── */\n#define SPEED 4\nNext npcB(void) {\n    R0 = SPEED;\n    RETURN_TO_CALLER();\n}\n\n'
        '/* ── objB ── */\nNext objB(void) {\n    RETURN_TO_CALLER();\n}\n\n'
        '/* ── objA ── */\n/* objA: the first one. */\nNext objA(void) {\n    RETURN_TO_CALLER();\n}\n')


def plan(files: dict[str, str]) -> layout.Plan:
    return layout.plan(files, b=BN6F, table=TABLE)


def test_functions_go_to_their_bn6f_source_in_address_order_and_the_inbox_goes() -> None:
    p = plan({"leaves_farm_1.c": FARM, "npc_update.c": "Next npcA(void) {\n}\n"})

    assert not p.problems
    assert p.files["leaves_farm_1.c"] is None
    assert "npc_update.c" not in p.files                       # a subsystem's own file stays as it is
    obj = p.files["object.c"]
    assert obj is not None and obj.index("/* ── objA ── */") < obj.index("/* ── objB ── */")
    assert obj.startswith(layout.header("object.c", BN6F, TABLE) + "\n")
    npc = p.files["npc_8002800.c"]                              # the second part of npc.s
    assert npc is not None and "#define SPEED 4\nNext npcB(void) {" in npc
    assert {(m.function, m.new) for m in p.moves} == {("npcB", "npc_8002800.c"), ("objB", "object.c"),
                                                      ("objA", "object.c")}


def test_laying_out_a_laid_out_tree_changes_nothing_and_keeps_each_part_as_it_was() -> None:
    first = plan({"leaves_farm_1.c": FARM})
    tree = {name: text for name, text in first.files.items() if text is not None}

    again = plan(tree)

    assert again.laid_out
    before = {owner: part.digest() for owner, part in first.parts.items()}
    after = {owner: part.digest() for name, text in tree.items() for owner, part in
             ((p.owner, p) for p in layout.parts_of(name, text)[1])}
    assert before == after


def test_a_file_without_part_lines_is_cut_at_its_functions_keeping_shared_definitions_together() -> None:
    misc = ('/* leaves_misc.c */\n#include "game.h"\n\n/* ── bn6f object.s ──────── */\n\n'
            '/* objA */\nNext objA(void) {\n}\n\n'
            'enum { FRAME = 8 };\nNext npcB(void) {\n    SP -= FRAME;\n}\n\n'
            'Next npcC(void) {\n    SP += FRAME;\n}\n')

    head, parts = layout.parts_of("leaves_misc.c", misc)

    assert head == '/* leaves_misc.c */\n#include "game.h"\n'
    assert [(p.owner, p.functions) for p in parts] == [("objA", ("objA",)), ("npcB", ("npcB", "npcC"))]
    assert parts[0].text == "/* ── objA ── */\n/* objA */\nNext objA(void) {\n}"


def test_a_macro_defined_before_a_part_that_takes_it_from_the_headers_stops_the_layout() -> None:
    defines = "/* ── objA ── */\n#define SPEED 4\nNext objA(void) {\n    R0 = SPEED;\n}"      # at 0x08001100
    takes = "/* ── objB ── */\nNext objB(void) {\n    R0 = SPEED;\n}"                       # at 0x08001200

    problems = plan({"inbox_1.c": f"x\n\n{defines}\n\n{takes}\n"}).problems

    assert any("objB uses SPEED, which the part of objA defines before it" in problem for problem in problems)


def test_a_part_before_one_that_defines_a_macro_still_sees_the_headers_one() -> None:
    takes = "/* ── objA ── */\nNext objA(void) {\n    R0 = SPEED;\n}"                       # at 0x08001100
    defines = "/* ── objB ── */\n#define SPEED 4\nNext objB(void) {\n    R0 = SPEED;\n}"      # at 0x08001200

    assert not plan({"inbox_1.c": f"x\n\n{defines}\n\n{takes}\n"}).problems


def test_two_parts_defining_one_enumerator_in_a_file_stop_the_layout_but_two_macros_do_not() -> None:
    a = "/* ── objA ── */\nenum { K = 1 };\n#define M 2\nNext objA(void) {\n    R0 = K + M;\n}"
    b = "/* ── objB ── */\nenum { K = 1 };\n#define M 2\nNext objB(void) {\n    R0 = K + M;\n}"

    problems = plan({"inbox_1.c": f"x\n\n{a}\n\n{b}\n"}).problems

    assert any("K is defined by the parts of objA, objB" in problem for problem in problems)
    assert not any(" M " in problem for problem in problems)


def test_moved_functions_get_their_new_file_in_symbols_db(tmp_path: Path) -> None:
    entry = tmp_path / "objA.yaml"
    entry.write_text("name: objA\nevidence:\n- method: readable-c\n  detail: farm\n  refs:\n"
                     "  - cport/src/game/leaves_farm_1.c\nreadable_c: cport/src/game/leaves_farm_1.c\n"
                     "stamp:\n  body: abc\n")

    _ = layout.update_db([layout.Move("objA", "leaves_farm_1.c", "object.c")], str(tmp_path), "a test")

    data = cast(dict[str, object], yaml.safe_load(entry.read_text()))
    assert data["readable_c"] == "cport/src/game/object.c"
    assert cast(list[dict[str, object]], data["evidence"])[0]["refs"] == ["cport/src/game/object.c"]
    change = cast(list[dict[str, str]], data["changes"])[0]
    assert change["what"].startswith("moved from cport/src/game/leaves_farm_1.c to cport/src/game/object.c")
    assert list(data) == ["name", "evidence", "changes", "readable_c", "stamp"]
