"""`wf runtime-evidence`: the per-symbol evidence that running the original game produces
about the names `wf investigate` proposed from assembly alone. Selects the functions that have
both a `wf investigate` symbols/db entry and captured calls, replays each in mGBA from K entry
states, summarises what it actually did, has a model judge the name against those facts, and
writes a NAME.yaml per function into build/runtime/RUN_ID/. Nothing under symbols/db or bn6f
is touched; the orchestrator (`wf record`) applies the evidence after review.

  python -m wf runtime-evidence [--count N] [--only NAME...] [--states K=8] [--jobs J]
                                 [--model PROVIDER/MODEL=minimax/MiniMax-M3]
  python -m wf runtime-evidence [--only NAME]   # re-judge the cached facts with the model

The graph mirrors `wf investigate`: a typed Burr state (RunState), small typed steps, pydantic
results, NewTypes for names and addresses, and `wf docs` regenerates docs/workflows from it."""
import threading
import time
from collections.abc import Callable
from pathlib import Path
from typing import cast

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.repo import REPO
from wf.tools import runtime_evidence as rt
from wf.tools.runtime_evidence import (DEFAULT_COUNT, DEFAULT_JOBS, DEFAULT_MODEL, DEFAULT_STATES,
                                       EvidenceYAML, Probe, Report, RuntimeFacts, Verdict,
                                       ask_judge, evidence_yaml, probe_work_dir,
                                       run_original, select_runtime_targets, summarise,
                                       write_per_function, write_report)
from wf.types import FunctionName, GuestAddress, ModelSpec, Result, ToolMessage

PROJECT = "recomp-runtime-evidence"


def runtime_evidence_graph() -> Graph["RuntimeEvidenceState"]:
    return workflow.graph(
        first=select_the_targets,
        ends=[write_the_evidence_files],
        transitions=[
            Transition(after=select_the_targets, go_to=gather_the_facts, when=some_selected),
            Transition(after=select_the_targets, go_to=write_the_evidence_files, when=none_selected),
            Transition(after=gather_the_facts, go_to=judge_the_names),
            Transition(after=judge_the_names, go_to=write_the_evidence_files),
        ],
    )


def run_runtime_evidence(start: "RuntimeEvidenceState", run_id: str,
                         tracked: bool = True) -> "Report":
    outcome = runtime_evidence_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"runtime-evidence run {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class RuntimeEvidenceState(BaseModel):
    run_dir: Path                                # build/runtime/RUN_ID
    count: int = DEFAULT_COUNT
    only: tuple[FunctionName, ...] = ()
    states: int = DEFAULT_STATES
    jobs: int = DEFAULT_JOBS
    model: ModelSpec = DEFAULT_MODEL
    targets: tuple[FunctionName, ...] = ()
    addresses: dict[FunctionName, GuestAddress] = {}
    summaries: dict[FunctionName, ToolMessage] = {}
    interfaces: dict[FunctionName, ToolMessage] = {}
    notes: tuple[ToolMessage, ...] = ()           # selection-time diagnostic messages
    facts: dict[FunctionName, RuntimeFacts] = {}
    verdicts: dict[FunctionName, Verdict] = {}
    evidence: dict[FunctionName, EvidenceYAML] = {}
    report: Report | None = None


workflow = Workflow[RuntimeEvidenceState](RuntimeEvidenceState, project=PROJECT)


# ── selection step ────────────────────────────────────────────────────

class SelectInput(Result):
    count: int
    only: tuple[FunctionName, ...]
    states: int
    run_dir: Path


class SelectOutput(Result):
    targets: tuple[FunctionName, ...]
    addresses: dict[FunctionName, GuestAddress]
    summaries: dict[FunctionName, ToolMessage]
    interfaces: dict[FunctionName, ToolMessage]
    notes: tuple[ToolMessage, ...]


@workflow.step
def select_the_targets(given: SelectInput) -> SelectOutput:
    """The `wf investigate` entries that have captured calls and no prior runtime evidence
    (newest first by the snapshots' mtime; --only overrides the count limit)."""
    from wf.graphs import runtime_evidence as _self_module
    targets = _self_module.select_runtime_targets(given.count, given.only, given.states, given.run_dir)
    addresses = _self_module._addresses_of(targets)
    summaries, interfaces = _self_module._summaries_and_interfaces(targets)
    notes: list[ToolMessage] = []
    if given.only:
        for name in given.only:
            if name not in targets:
                notes.append(ToolMessage(f"{name}: not selected (no captured calls, no `wf "
                                          "investigate` entry, or already has a runtime item)"))
    return SelectOutput(targets=targets, addresses=addresses, summaries=summaries,
                        interfaces=interfaces, notes=tuple(notes))


# ── gathering step (the heavy lifting) ────────────────────────────────

class GatherInput(Result):
    targets: tuple[FunctionName, ...]
    addresses: dict[FunctionName, GuestAddress]
    states: int
    jobs: int
    run_dir: Path
    notes: tuple[ToolMessage, ...] = ()
    notes: tuple[ToolMessage, ...] = ()


class GatherOutput(Result):
    facts: dict[FunctionName, RuntimeFacts]
    notes: tuple[ToolMessage, ...]


@workflow.step
def gather_the_facts(given: GatherInput) -> GatherOutput:
    """For every selected function: replay K entry states through the original in mGBA, fold
    the per-state Probes into a RuntimeFacts block. mgba_run runs are serialised via a single
    subprocess (one function at a time, K=states entry snapshots per function) so the run
    stays predictable and the per-function probe/ subdirectory is simple to inspect."""
    from wf.graphs import runtime_evidence as _self_module
    facts: dict[FunctionName, RuntimeFacts] = {}
    notes: list[ToolMessage] = list(given.notes)
    for name in given.targets:
        address = given.addresses[name]
        try:
            probes = _self_module.run_original(name, given.states, current_run_id(),
                                               _self_module.probe_work_dir(given.run_dir, name))
        except FileNotFoundError as error:
            notes.append(ToolMessage(f"{name}: {error}"))
            continue
        facts[name] = _self_module.summarise(name, address, probes)
        if not any(probe.returned for probe in probes):
            notes.append(ToolMessage(f"{name}: no probe returned within the step budget"))
    return GatherOutput(facts=facts, notes=tuple(notes))

# The Notes type alias keeps the steps' combined-notes rule in one place.
Notes = tuple[ToolMessage, ...]


# ── judging step (model call) ─────────────────────────────────────────

class JudgeInput(Result):
    facts: dict[FunctionName, RuntimeFacts]
    summaries: dict[FunctionName, ToolMessage]
    interfaces: dict[FunctionName, ToolMessage]
    model: ModelSpec
    run_dir: Path
    notes: tuple[ToolMessage, ...] = ()


class JudgeOutput(Result):
    verdicts: dict[FunctionName, Verdict]
    notes: tuple[ToolMessage, ...]


@workflow.step
def judge_the_names(given: JudgeInput) -> JudgeOutput:
    """Ask the model whether each function's summary + interface matches what the RuntimeFacts
    show; the verdict is cached per function (resumable across runs, since the model might
    time out partway)."""
    from wf.graphs import runtime_evidence as _self_module
    run_id = current_run_id()
    verdicts: dict[FunctionName, Verdict] = {}
    notes: list[ToolMessage] = list(given.notes)
    for name, facts in given.facts.items():
        summary = given.summaries.get(name, ToolMessage("(no summary in the symbols/db entry)"))
        interface = given.interfaces.get(name)
        verdict = _self_module.ask_judge(given.model, facts, summary, interface, run_id)
        verdicts[name] = verdict
        if verdict.problem:
            notes.append(ToolMessage(f"{name}: {verdict.problem}"))
    return JudgeOutput(verdicts=verdicts, notes=tuple(notes))


# ── write step (YAML per function + report.tsv + contradictions.tsv) ──

class WriteInput(Result):
    targets: tuple[FunctionName, ...]
    addresses: dict[FunctionName, GuestAddress]
    summaries: dict[FunctionName, ToolMessage]
    interfaces: dict[FunctionName, ToolMessage]
    facts: dict[FunctionName, RuntimeFacts]
    verdicts: dict[FunctionName, Verdict]
    run_dir: Path
    notes: tuple[ToolMessage, ...]


class WriteOutput(Result):
    report: Report


@workflow.step
def write_the_evidence_files(given: WriteInput) -> WriteOutput:
    """One NAME.yaml per function (mergeable into the symbols/db entry's `evidence` list by
    `wf record`), a report.tsv (every function's verdict row), and a contradictions.tsv (the
    rows the orchestrator should look at first)."""
    from wf.graphs import runtime_evidence as _self_module
    given.run_dir.mkdir(parents=True, exist_ok=True)       # the run may have selected 0 functions
    evidence: dict[FunctionName, EvidenceYAML] = {}
    notes = list(given.notes)
    for name in given.targets:
        facts = given.facts.get(name)
        verdict = given.verdicts.get(name)
        if facts is None or verdict is None:
            continue
        evidence[name] = _self_module.evidence_yaml(name, given.addresses[name], given.summaries[name],
                                                     _symbol_name_for(name), facts, verdict,
                                                     current_run_id(), given.run_dir)
        _self_module.write_per_function(evidence[name], given.run_dir)
    report_path = given.run_dir / "report.tsv"
    contradictions_path = given.run_dir / "contradictions.tsv"
    rows, contradictions = [], []
    for name in given.targets:
        facts = given.facts.get(name)
        verdict = given.verdicts.get(name)
        if facts is None or verdict is None:
            rows.append(f"{name}\t{given.addresses[name]:#010x}\t-\t-\t-\t(no probe ran)")
            continue
        rows.append(f"{name}\t{facts.address:#010x}\t{facts.probes}\t{facts.returned}\t"
                    f"{verdict.verdict}\t{(verdict.reason or '').replace(chr(10), ' ')[:200]}")
        if verdict.verdict == "contradicts":
            contradictions.append(f"{name}\t{facts.address:#010x}\t{verdict.reason}")
    report_path.write_text("function\taddress\tprobes\treturned\tverdict\treason\n"
                           + "\n".join(rows) + "\n")
    if contradictions:
        contradictions_path.write_text("function\taddress\treason\n" + "\n".join(contradictions) + "\n")
    elif contradictions_path.exists():
        contradictions_path.unlink()
    tokens = sum(verdict.tokens for verdict in given.verdicts.values())
    consistent = sum(1 for verdict in given.verdicts.values() if verdict.verdict == "consistent")
    contradicts = sum(1 for verdict in given.verdicts.values() if verdict.verdict == "contradicts")
    unknown = sum(1 for verdict in given.verdicts.values() if verdict.verdict == "unknown")
    not_run = len(given.targets) - len(given.facts)
    selected = len(given.targets)
    gathered = sum(1 for facts in given.facts.values() if facts.returned > 0)
    problems = tuple(notes)
    return WriteOutput(report=Report(selected=selected, gathered=gathered, consistent=consistent,
                                     contradicts=contradicts, unknown=unknown, not_run=not_run,
                                     tokens=tokens, problems=problems, stopped=None))


# ── decisions ──────────────────────────────────────────────────────────

def some_selected(state: RuntimeEvidenceState) -> bool:
    return bool(state.targets)


def none_selected(state: RuntimeEvidenceState) -> bool:
    return not state.targets


# ── helpers ────────────────────────────────────────────────────────────

def _symbol_name_for(name: FunctionName) -> str:
    """The function's symbols/db name (it is the function's current bn6f name; the entry's
    `name` is the same string for evidence that lives in the entry). Returns Name(name)."""
    from wf.types import SymbolName
    return SymbolName(name)


def _addresses_of(targets: tuple[FunctionName, ...]) -> dict[FunctionName, GuestAddress]:
    """The bn6f address of every target (for the evidence entries' `address:` and the judge
    prompt's header line). Looks up the address from the symbols/db entry's `address:` field
    (a hex string). The function's symbols/db file is the source of truth, even if bn6f
    renamed the function out from under us: the human applies the evidence to whichever name
    the function has today via `wf record`."""
    import yaml
    out: dict[FunctionName, GuestAddress] = {}
    for name in targets:
        path = REPO / "symbols/db" / f"{name}.yaml"
        if not path.exists():
            continue
        try:
            loaded = yaml.safe_load(path.read_text())
        except yaml.YAMLError:
            continue
        if isinstance(loaded, dict):
            address = loaded.get("address")
            if isinstance(address, str):
                try:
                    out[FunctionName(name)] = GuestAddress(int(address, 16))
                except ValueError:
                    pass
    return out


def _summaries_and_interfaces(targets: tuple[FunctionName, ...]) \
        -> tuple[dict[FunctionName, ToolMessage], dict[FunctionName, ToolMessage]]:
    """The symbols/db entry's one-line summary and interface lines (what the judge sees
    beside the RuntimeFacts, so it knows what the entry currently claims)."""
    import yaml
    summaries: dict[FunctionName, ToolMessage] = {}
    interfaces: dict[FunctionName, ToolMessage] = {}
    for name in targets:
        path = REPO / "symbols/db" / f"{name}.yaml"
        if not path.exists():
            continue
        try:
            loaded = yaml.safe_load(path.read_text())
        except yaml.YAMLError:
            continue
        if not isinstance(loaded, dict):
            continue
        summary = loaded.get("summary")
        if isinstance(summary, str):
            summaries[FunctionName(name)] = ToolMessage(" ".join(summary.split()))
        interface = loaded.get("interface")
        if isinstance(interface, str):
            interfaces[FunctionName(name)] = ToolMessage(" ".join(interface.split()))
    return summaries, interfaces
