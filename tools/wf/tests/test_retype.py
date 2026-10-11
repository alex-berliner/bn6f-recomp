"""The retype queue: an old-prompt install raises the lint baseline by exactly its own register-level code
(tools/abi/lint.py on the laid-out file) and queues each function that has any, with its keys; a function
already in the typed form (a thin adapter over a typed function) is not queued."""
import json
from pathlib import Path

import pytest

from wf.tools import retype
from wf.tools.retype import PENDING, fill_install_commit, queue_install, read_queue
from wf.types import CommitHash, FunctionName, ModelSpec

ASSEMBLED = """/* ── sub_8001000 ── */
static u32 read_flag_sub_8001000(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return R0;
}

Next sub_8001000(void) {
    if (LD8(R0) == 0)
        R0 = read_flag_sub_8001000(R1);
    RETURN_TO_CALLER();
}

/* ── sub_8001100 ── */
static u32 twice(u32 x) { return x * 2; }

Next sub_8001100(void) {
    R0 = twice(R0);
    RETURN_TO_CALLER();
}
"""


def test_an_install_queues_its_register_code_and_raises_the_baseline_by_it(monkeypatch: pytest.MonkeyPatch,
                                                                          tmp_path: Path) -> None:
    baseline, queue = tmp_path / "lint_baseline.json", tmp_path / "retype_queue.tsv"
    _ = baseline.write_text(json.dumps({"comment": "c", "functions": {"Other": {"regs": 2}}}))
    monkeypatch.setattr(retype, "BASELINE", baseline)
    monkeypatch.setattr(retype, "QUEUE", queue)
    laid_out = tmp_path / "asm00_0.c"
    _ = laid_out.write_text('#include "game.h"\n' + ASSEMBLED)
    functions = (FunctionName("sub_8001000"), FunctionName("sub_8001100"))

    rows = queue_install(functions, ASSEMBLED, (laid_out,), ModelSpec("minimax/MiniMax-M3+old"), "6505961c4798",
                         "convert-1")

    keys = {row.function: row.lint_keys for row in rows}
    assert keys == {"sub_8001000": ("asm00_0.c:read_flag_sub_8001000", "sub_8001000")}
    written = retype.BaselineFile.model_validate_json(baseline.read_text()).functions
    assert set(written) == {"Other", "asm00_0.c:read_flag_sub_8001000", "sub_8001000"}
    assert [(row.function, row.install_commit) for row in read_queue(queue)] == [("sub_8001000", PENDING)]

    assert fill_install_commit(functions, CommitHash("0123456789abcdef"))
    assert [row.install_commit for row in read_queue(queue)] == ["0123456789"]


def test_register_form_hazards_name_their_functions(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(retype, "_next_functions", lambda: frozenset({"sub_803A7EC", "TestEventFlag"}))
    code = """/* ── sub_80498F8 ── */
Next sub_80498F8(void) {
    LR = 0x804990C | 1u;   /* tail call */
    GUEST_CALL_ADDR(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8001000 ── */
Next sub_8001000(void) {
    u32 saved_lr = LR;
    sub_803A7EC();
    GUEST_CALL(TestEventFlag);
    LR = saved_lr;
    RETURN_TO_CALLER();
}

/* ── sub_8001100 ── */
Next sub_8001100(void) {
    GUEST_CALL(sub_803A7EC);   /* sub_803A7EC() in a comment is fine */
    RETURN_TO_CALLER();
}
"""
    hazards = retype.register_form_hazards(code)

    assert set(hazards) == {"sub_80498F8", "sub_8001000"}
    assert "sets LR" in hazards[FunctionName("sub_80498F8")]
    assert "sub_803A7EC as a C function" in hazards[FunctionName("sub_8001000")]
