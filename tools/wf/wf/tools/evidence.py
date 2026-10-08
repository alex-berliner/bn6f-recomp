"""symbols/db entries for converted functions: the writing model's draft
(meaning, interface, proposed name) plus evidence the workflow fills in from
its own results (difftest, CPU oracle, lockstep, coverage), with the commands
that reproduce them. Format: symbols/db/README.md."""
import re
from datetime import date
from pathlib import Path

import yaml

from wf.repo import REPO
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


def write_entries(drafts: tuple[SymbolDraft, ...], checks: tuple[FunctionChecks, ...],
                  provenance: Provenance) -> tuple[Path, ...]:
    by_function = {check.function: check for check in checks}
    written: list[Path] = []
    for draft in drafts:
        check = by_function[draft.function]
        path = DATABASE / f"{draft.function}.yaml"
        with open(path, "w") as out:
            yaml.safe_dump(_entry(draft, check, provenance, [d.function for d in drafts]), out,
                           sort_keys=False, width=110, allow_unicode=True)
        written.append(path)
    return tuple(written)


def renames(drafts: tuple[SymbolDraft, ...], checks: tuple[FunctionChecks, ...]) -> tuple[Rename, ...]:
    """Proposed names for meanings that are confirmed or probable (hypotheses get comments only)."""
    by_function = {check.function: check for check in checks}
    return tuple(Rename(old=SymbolName(draft.function), new=draft.proposed_name) for draft in drafts
                 if draft.proposed_name != draft.function
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
           subsystem_functions: list[FunctionName]) -> dict[str, object]:
    today = date.today().isoformat()
    verify = (f"cd tools/wf && .venv/bin/python -m wf capture --function {draft.function}\n"
              f".venv/bin/python -m wf verify --function {draft.function} --snapshots SNAPSHOT_DIR")
    evidence: list[dict[str, object]] = [
        {"method": "readable-c", "date": today,
         "detail": f"Written by {provenance.model} through the convert workflow ({provenance.attempts} attempt(s), "
                   f"run {provenance.convert_run}); reviewed by Claude before the commit.",
         "refs": [str(provenance.file.relative_to(REPO))],
         "reproduce": f"cd tools/wf && .venv/bin/python -m wf convert --subsystem '{provenance.subsystem}' "
                      f"--file {provenance.file.name} {' '.join(subsystem_functions)}"},
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
    return {
        "name": draft.function, "address": f"0x{check.address:08X}", "kind": "function",
        "status": status(draft, check).value, "summary": draft.summary, "interface": draft.interface,
        "details": details, "evidence": evidence,
        "changes": [{"date": today, "what": f"readable C replacement in {provenance.file.relative_to(REPO)}",
                     "where": f"recomp {provenance.commit[:7]}", "tool": f"tools/wf convert ({provenance.model})"}],
        "readable_c": str(provenance.file.relative_to(REPO)), "coverage": routes,
        "related": [name for name in subsystem_functions if name != draft.function],
        "open_questions": open_questions, "sources": [f"tools/wf run {provenance.convert_run}"],
    }


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
