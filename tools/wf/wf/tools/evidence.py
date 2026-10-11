"""symbols/db entries for converted functions: the writing model's draft
(meaning, interface, proposed name) plus evidence the workflow fills in from
its own results (difftest, CPU oracle, lockstep, coverage), with the commands
that reproduce them. Format: symbols/db/README.md."""
import re
import shlex
from collections.abc import Sequence
from datetime import date
from pathlib import Path

import yaml

from wf.repo import REPO
import funcindex  # tools/funcindex.py (wf.repo puts tools/ on the path): direct callers and callees
import gamepaths  # tools/gamepaths.py: where the ABI export is
from wf.tools.subsystem import SymbolDraft
from wf.types import (CapturedDifftestResult, CommitHash, DifftestResult, FunctionName, GuestAddress, ModelSpec,
                      OracleResult, Rename, Result, SymbolName, SymbolStatus)

DATABASE = REPO / "symbols/db"
PROFILES = REPO / "tools/traces/coverage"


class FunctionChecks(Result):
    """What the verify run found for one function."""
    function: FunctionName
    address: GuestAddress
    random: DifftestResult | None
    captured: CapturedDifftestResult | None
    oracle: OracleResult | None
    routes_identical: int
    routes_checked: int


class Provenance(Result):
    """Who wrote the code and which runs checked it."""
    subsystem: str
    file: Path
    model: ModelSpec
    attempts: int
    convert_run: str
    verify_run: str
    commit: CommitHash
    farm_group: bool = False     # the model farm's candidates for unrelated small functions, put together
    placed: dict[FunctionName, Path] = {}   # a farm group: the laid-out file each function went to (tools/layout.py)


def write_entries(drafts: tuple[SymbolDraft, ...], checks: tuple[FunctionChecks, ...],
                  provenance: Provenance) -> tuple[Path, ...]:
    by_function = {check.function: check for check in checks}
    neighbours = call_neighbours() if provenance.farm_group else {}
    written: list[Path] = []
    for draft in drafts:
        check = by_function[draft.function]
        others = [d.function for d in drafts if d.function != draft.function]
        related = (funcindex.related_by_calls(check.address, [(f, by_function[f].address) for f in others], neighbours)
                   if provenance.farm_group else others)
        path = DATABASE / f"{draft.function}.yaml"
        with open(path, "w") as out:
            yaml.safe_dump(_entry(draft, check, provenance, [d.function for d in drafts], related), out,
                           sort_keys=False, width=110, allow_unicode=True)
        written.append(path)
    return tuple(written)


def call_neighbours() -> dict[int, frozenset[int]]:
    """Direct callers and callees by address, from the lifter's ABI export (none when it is not built)."""
    graph = Path(gamepaths.ABI_GRAPH)
    return funcindex.call_neighbours(str(graph)) if graph.exists() else {}


# a name that says nothing yet: sub_8030808, loc_803083E, or a bare module and address (chatbox_80411BC)
PLACEHOLDER = re.compile(r"^(?:[a-z]+)_[0-9A-Fa-f]{6,8}$")


def renames(drafts: tuple[SymbolDraft, ...], checks: tuple[FunctionChecks, ...]) -> tuple[Rename, ...]:
    """Proposed names for meanings that are confirmed or probable (hypotheses get comments only), and only
    for placeholder names: a name someone chose (TestCutsceneFlag) stays; the entry says what was learnt."""
    by_function = {check.function: check for check in checks}
    return tuple(Rename(old=SymbolName(draft.function), new=draft.proposed_name) for draft in drafts
                 if draft.proposed_name != draft.function and PLACEHOLDER.match(draft.function)
                 and status(draft, by_function[draft.function]) is not SymbolStatus.HYPOTHESIS)


def status(draft: SymbolDraft, check: FunctionChecks) -> SymbolStatus:
    """README: confirmed = read from the code and checked against the original; a readable
    replacement compared on real calls (captured difftest or the oracle) is that check."""
    compared = (check.captured is not None and check.captured.passed) or \
               (check.oracle is not None and check.oracle.compared > 0 and check.oracle.differed == 0)
    if draft.status is SymbolStatus.PROBABLE and compared:
        return SymbolStatus.CONFIRMED
    return draft.status


def _entry(draft: SymbolDraft, check: FunctionChecks, provenance: Provenance,
           subsystem_functions: list[FunctionName], related: Sequence[str]) -> dict[str, object]:
    """README: a farm group's readable-c line converts its own function (the group is the run) and its
    related list names only the members it calls or that call it; a subsystem's names them all."""
    today = date.today().isoformat()
    readable_c = str(provenance.placed.get(draft.function, provenance.file).relative_to(REPO))
    verify = (f"./wf capture --function {draft.function}\n"
              f"./wf verify --function {draft.function} --snapshots build-snapshots")
    evidence: list[dict[str, object]] = [
        {"method": "readable-c", "date": today,
         "detail": f"Written by {provenance.model} through the convert workflow ({provenance.attempts} attempt(s), "
                   f"run {provenance.convert_run}); reviewed by Claude before the commit.",
         "refs": [readable_c],
         "reproduce": f"./wf convert --subsystem {shlex.quote(provenance.subsystem)} --file {provenance.file.name} "
                      + " ".join([draft.function] if provenance.farm_group else subsystem_functions)},
    ]
    if check.captured is not None or check.random is not None:
        evidence.append({"method": "difftest", "date": today,
                         "detail": "Readable vs lifted on the original's captured calls and on random inputs.",
                         "reproduce": verify, "result": _difftest_result(check) + f" (verify run {provenance.verify_run})"})
    if check.oracle is not None:
        evidence.append({"method": "cpu-oracle", "date": today,
                         "detail": "The original (mGBA, real BIOS) and the readable version from the same real states.",
                         "reproduce": verify,
                         "result": f"{check.oracle.compared} compared, {check.oracle.differed} differed, "
                                   f"{check.oracle.out_of_domain} out of domain (verify run {provenance.verify_run})"})
    evidence.append({"method": "lockstep", "date": today,
                     "detail": "Palette, VRAM and OAM identical to the original at every checkpoint.",
                     "reproduce": verify,
                     "result": f"{check.routes_identical}/{check.routes_checked} routes identical "
                               f"(verify run {provenance.verify_run})"})
    routes = covering_routes(check.address)
    if routes:
        evidence.append({"method": "coverage", "date": today, "detail": "Executed by: " + ", ".join(routes) + ".",
                         "reproduce": f"grep -l '^{check.address:08x} ' tools/traces/coverage/*.prof"})
    open_questions = list(draft.open_questions)
    details = draft.details
    if draft.register_form:
        details += f"\nRegister form kept (C call semantics could not work): {draft.register_form}"
        open_questions.append(f"Calling-convention work: register form kept because {draft.register_form}")
    entry: dict[str, object] = {
        "name": draft.function, "address": f"0x{check.address:08X}", "kind": "function",
        "status": status(draft, check).value, "summary": draft.summary, "interface": draft.interface,
        "details": details, "evidence": evidence,
        "changes": [{"date": today, "what": f"readable C replacement in {readable_c}",
                     "where": f"recomp {provenance.commit[:7]}", "tool": f"tools/wf convert ({provenance.model})"}],
        "readable_c": readable_c, "coverage": routes,
        "related": list(related),
        "open_questions": open_questions,
        "sources": [f"tools/wf run {provenance.convert_run}", f"recomp {provenance.commit[:7]}"],
    }
    if not related:
        del entry["related"]
    return entry


def _difftest_result(check: FunctionChecks) -> str:
    parts: list[str] = []
    if check.captured is not None:
        parts.append(f"captured calls: {check.captured.runs} runs, {check.captured.failures} failures, "
                     f"{check.captured.out_of_domain} out of domain")
    if check.random is not None:
        parts.append(f"random inputs: {check.random.trials} trials, {check.random.failures} failures, "
                     f"{check.random.out_of_domain} out of domain")
    return "; ".join(parts)


def covering_routes(address: GuestAddress) -> list[str]:
    pattern = re.compile(rf"^{address:08x} ", re.M)
    return sorted(profile.stem for profile in PROFILES.glob("*.prof") if pattern.search(profile.read_text()))
