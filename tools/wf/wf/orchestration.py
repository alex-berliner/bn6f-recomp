"""The orchestration map: what the orchestrator (Claude) does and what the
workflows do, as one page with a tooltip on every box, arrow and workflow
name, and as a PNG.

The workflow side comes from the graphs themselves: each step, what it reads,
waits for and produces, its decisions, the child runs it starts and whether it
asks an outside model. So that part of the map cannot disagree with the code.
The orchestrator's side (its judgement, and the work no workflow does yet) is
described here in ORCHESTRATOR, with how it starts runs (STARTS), what comes
back to it (HANDBACKS) and which pause points it answers (ANSWERS). A pause
point that nobody here answers still gets a box of its own, so a new one shows
up on the map.

  cd tools/wf && .venv/bin/python -m wf.orchestration [OUT_DIR]   # default docs/: orchestration.html and .png
"""
import inspect
import json
import re
import subprocess
import sys
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from datetime import datetime
from enum import Enum
from pathlib import Path

from pydantic import BaseModel, ConfigDict, TypeAdapter

from wf.burr_typed import AnyStepOrPause, Transition, step_models
from wf.graphs.convert import DEFAULT_MODELS, ConvertState
from wf.registry import WORKFLOWS, WorkflowEntry
from wf.repo import REPO
from wf.routes import ALL_ROUTES

PAGE_TEMPLATE = Path(__file__).with_name("orchestration_page.html")


# ── the orchestrator's side ────────────────────────────────────────────

class ActionKind(Enum):
    JUDGEMENT = "judgement"                      # the orchestrator's by design
    ANSWERS_PAUSE = "answers a pause point"
    BY_HAND = "by hand: not in a workflow yet"


@dataclass(frozen=True)
class OrchestratorAction:
    key: str
    title: str
    kind: ActionKind
    what: str
    gets: str
    gives: str
    commands: tuple[str, ...]
    documented: str


@dataclass(frozen=True)
class Start:
    """The orchestrator starts a run of a workflow from the command line."""
    action: str
    workflow: str
    command: str
    why: str


@dataclass(frozen=True)
class Handback:
    """What a finished run hands back to the orchestrator."""
    workflow: str
    action: str
    label: str
    what: str


@dataclass(frozen=True)
class Answer:
    """Which orchestrator action answers a pause point."""
    workflow: str
    step: str
    action: str


ORCHESTRATOR: tuple[OrchestratorAction, ...] = (
    OrchestratorAction(
        "pick_subsystem", "Pick a subsystem", ActionKind.JUDGEMENT,
        "Claude chooses the next group of lifted bn6f functions to make readable: functions that work together "
        "(callers and callees), taken from code the recorded gameplay routes execute. Hot code comes first, because "
        "readable code there is also a speedup. Claude describes the group in a few words, names the new C file "
        "under cport/src/game, and starts a convert run for it.",
        "Coverage profiles (tools/coverage.py over tools/traces/coverage/*.prof), the lifted C, and the outcomes "
        "of earlier convert runs.",
        "A convert run: the subsystem's description, the new file's name, its functions, and whether it is battle "
        "code.",
        ("python -m wf convert --subsystem WHAT --file NAME.c [--battle-code] FUNCTION...",),
        "docs/PROCESS.md §1 (goal, coverage) and §4"),
    OrchestratorAction(
        "pick_leaves", "Pick leaf functions\nfor the farm", ActionKind.JUDGEMENT,
        "Claude chooses small, self-contained lifted bn6f functions (leaves) that the recorded gameplay routes "
        "execute, for cheap outside models to convert one at a time. For these, the cost per converted function "
        "matters most. Claude first starts a capture run for the chosen functions, then a farm batch over the "
        "captured calls, with one model per provider.",
        "Coverage profiles, earlier farm results (tools/convert/scorecard.py), and each provider's remaining quota.",
        "A capture run for the chosen functions, then a farm batch over its snapshot directory.",
        ("python -m wf capture --function NAME... [--calls N]",
         "python -m wf farm --model PROVIDER/MODEL... --snapshots DIR FUNCTION..."),
        "docs/PROCESS.md §4"),
    OrchestratorAction(
        "review_subsystem", "Review a converted\nsubsystem", ActionKind.ANSWERS_PAUSE,
        "Claude answers the convert workflow's review pause point. A convert run stops there once a strong outside "
        "model's new C file for a subsystem has passed its quick test and the full verify. `wf waiting RUN_ID` "
        "prints the review sheet: the file, the register contracts it promises, its symbols/db drafts, the test "
        "evidence and every attempt. Claude checks register use outside the thin adapter functions and each "
        "/* register form: why */ note, return targets, table dispatch, second entry points, and which code paths "
        "the captured calls ran. Claude then approves, requests changes (the comments go back to the model), or "
        "rejects.",
        "The review sheet (a ReviewRequest), printed by `wf waiting RUN_ID`.",
        "A Review: approve, request changes or reject, with comments and the attribution lines for the commits.",
        ("python -m wf waiting RUN_ID", "python -m wf answer RUN_ID review.json"),
        "tools/wf/wf/graphs/convert.py (REVIEW_CHECKLIST is printed on the review sheet)"),
    OrchestratorAction(
        "integrate_farm", "Integrate farm\ncandidates", ActionKind.JUDGEMENT,
        "Farm runs leave passing versions of single functions in tools/convert/accepted/, each with a drafted "
        "symbols/db entry, without committing them. Claude groups them (by bn6f source file, until their "
        "subsystems are converted) and starts a convert run per group with --farm-model: the group becomes one "
        "file that gets the quick test, the full verify and the same review pause as a model-written subsystem, "
        "then is committed and recorded. A failed group goes to the strong models for repair.",
        "The accepted farm candidates and their drafts in tools/convert/accepted/, and a snapshot directory with "
        "their captured calls.",
        "A convert run per group, its first candidate built from the farm's accepted files.",
        ("python -m wf convert --farm-model PROVIDER/MODEL --snapshots DIR --subsystem WHAT --file NAME.c "
         "FUNCTION...",),
        "docs/PROCESS.md §4"),
    OrchestratorAction(
        "verify_tree", "Verify the tree\nand triage", ActionKind.JUDGEMENT,
        "Claude runs the verify workflow on the main checkout after changes made outside a convert run (hand "
        "edits, renames, tooling changes) and triages the VerifyReport. A CPU-oracle difference is compared with "
        "the known harness artifacts in tools/oracle_triage.md, and `tools/oracle.py --lifted` runs the lifted "
        "originals to tell a new difference from one that a callee already had.",
        "A VerifyReport: the verdict, failed functions, CPU-oracle differences, functions never compared, failed "
        "routes and the battle sweep.",
        "A judgement on the checkout: a fix, or a known harness artifact.",
        ("python -m wf verify [--function NAME]... [--snapshots DIR] [--battle-code]",),
        "docs/PROCESS.md §3; tools/oracle_triage.md (read by Claude, not yet an automatic classifier)"),
    OrchestratorAction(
        "record_other", "Document other\ndiscoveries", ActionKind.JUDGEMENT,
        "When Claude works out what a bn6f symbol means outside a convert run (a convert run writes its own "
        "entries), Claude writes its symbols/db entry: meaning, status, evidence and the exact commands that "
        "reproduce it. Claude then starts a record run to rename the symbol and document it in the bn6f "
        "disassembly.",
        "What was learnt about each symbol, with its evidence.",
        "symbols/db entries, the renames for confirmed or probable meanings, and a commit message.",
        ("python -m wf record --symbol NAME... [--rename OLD NEW]... --message-file FILE",),
        "docs/PROCESS.md §6; symbols/db/README.md"),
    OrchestratorAction(
        "watch", "Watch and steer runs", ActionKind.JUDGEMENT,
        "Claude keeps every workflow run moving. `wf status` lists all runs and where each one stopped, "
        "`wf waiting` shows what a paused run needs, and `wf retry` carries a failed or interrupted run on from "
        "the step where it stopped. Every run is also shown live in the Burr UI on port 7241.",
        "Run states from the Burr tracking logs in ~/.burr.",
        "Answers to pause points, and retries.",
        ("python -m wf status", "python -m wf waiting RUN_ID", "python -m wf retry RUN_ID",
         "tools/wf/.venv/bin/burr --no-open --no-copy-demo_data --host 0.0.0.0 --port 7241"),
        "tools/wf/wf/__main__.py"),
    OrchestratorAction(
        "cost", "Track model cost", ActionKind.BY_HAND,
        "Claude measures what each outside model costs per converted function. Claude snapshots each provider's "
        "remaining quota before and after a farm or convert run and counts the functions each model converted. "
        "Unused quota expires at each reset, so the aim is the most converted functions per reset period. API keys "
        "are never printed. This is not yet part of any workflow.",
        "Each provider's quota (Hyper resets daily; Z.ai, MiniMax and OpenCode Go weekly) and the token log "
        "tools/convert/usage.jsonl.",
        "The cost per converted function for each model.",
        ("python3 tools/convert/quota.py OUT.json", "python3 tools/convert/scorecard.py [FUNC...]"),
        "docs/PROCESS.md §4 (cost)"),
    OrchestratorAction(
        "sweeps", "Run the other sweeps", ActionKind.BY_HAND,
        "Claude runs the lockstep sweeps that no workflow runs yet. They compare the port with the original game "
        "beyond the recorded routes: 108 maps, every battle chip, and random play.",
        "A built port ROM.",
        "Lockstep results for maps, chips and random play.",
        ("python3 tools/mapsweep.py, then tools/tracesweep.sh", "python3 tools/chipsweep.py", "tools/fuzzsweep.sh"),
        "docs/PROCESS.md §3"),
    OrchestratorAction(
        "recordings", "Ask the user\nfor recordings", ActionKind.BY_HAND,
        "New gameplay routes come from the user playing the game, never from guessed inputs. Claude asks for a "
        "recording of the part of the game a route should cover. tools/record.sh records the inputs and the "
        "game's frame counter from a start save. Verify replays the routes in lockstep, and capture uses them to "
        "find calls of functions.",
        "Which part of the game the new route should cover.",
        "A new route, tools/traces/NAME.trace, logged in tools/traces/SESSIONS.md.",
        ("tools/record.sh NAME START.sav", "tools/check_recording.sh NAME"),
        "docs/PROCESS.md §3 (new recordings)"),
)

STARTS: tuple[Start, ...] = (
    Start("pick_subsystem", "convert", "wf convert",
          "Claude starts a convert run for the chosen group of functions. A strong outside model writes the "
          "readable C file; the run tests it, verifies it and waits for Claude's review."),
    Start("pick_leaves", "capture", "wf capture",
          "Claude starts a capture run to save the original game's calls of the chosen leaf functions. The farm "
          "tests candidates on these calls."),
    Start("pick_leaves", "farm-batch", "wf farm",
          "Claude starts a farm batch over the chosen leaf functions and their captured calls: one farm run per "
          "function and model."),
    Start("pick_leaves", "drafts", "wf drafts",
          "Claude starts a drafts run for accepted farm candidates without a symbols/db entry; `--drafter` "
          "picks the model that writes them (MiniMax-M3, on its own plan, rather than the farm model's credit)."),
    Start("integrate_farm", "convert", "wf convert --farm-model",
          "Claude sends a group of accepted farm candidates through a convert run as its first candidate file, "
          "reusing their captured calls. The group then gets the same verify, review, commit and symbol recording "
          "as a model-written file."),
    Start("verify_tree", "verify", "wf verify", "Claude starts a verify run on the main checkout."),
    Start("record_other", "record", "wf record", "Claude starts a record run for the symbols/db entries it wrote."),
)

HANDBACKS: tuple[Handback, ...] = (
    Handback("capture", "pick_leaves", "snapshots\nfor wf farm",
             "A finished capture run hands back a CaptureReport: the snapshot directory with the original game's "
             "calls, and which functions no route called. `wf farm --snapshots` takes that directory."),
    Handback("farm", "integrate_farm", "accepted\ncandidates",
             "A farm run whose candidate passed leaves it in tools/convert/accepted/ (its entry comes from a "
             "drafts run)."),
    Handback("drafts", "integrate_farm", "entries",
             "A drafts run hands back how many entries it wrote, and the candidates that got none."),
    Handback("farm-batch", "cost", "tallies\nper model",
             "A finished farm batch hands back how many functions each model converted."),
    Handback("verify", "verify_tree", "VerifyReport",
             "A finished verify run hands back its VerifyReport: one verdict, and every result behind it."),
    Handback("convert", "pick_subsystem", "outcome",
             "A finished convert run hands back its outcome: committed and recorded, rejected at review, every "
             "model out of attempts, nothing to convert, or the host difftest did not build."),
    Handback("record", "record_other", "RecordReport",
             "A finished record run hands back its RecordReport: the bn6f commits, the verify result after renames "
             "and the main commit, or the problems that stopped it."),
)

ANSWERS: tuple[Answer, ...] = (
    Answer("convert", "review_conversion", "review_subsystem"),
)

COMMAND_FOR_WORKFLOW = {"farm-batch": "farm"}   # `wf farm` starts a farm-batch run

# Left to right on the map, in the order work goes through the process: leaf functions are captured and
# farmed, farm output and subsystems are converted, conversions are verified, and findings are recorded.
# Claude's actions sit in the same order, each above the workflow it works with; watching runs applies to all.
WORKFLOW_ORDER = ("capture", "farm-batch", "farm", "drafts", "convert", "verify", "record")
ACTION_ORDER = ("recordings", "pick_leaves", "cost", "integrate_farm", "pick_subsystem", "review_subsystem",
                "verify_tree", "sweeps", "record_other", "watch")
CLAUDE_LANE = ("Orchestrator: Claude",
               "Claude chooses the work, starts runs, answers their pause points, and does what no workflow does "
               "yet. It does not write the conversions itself: outside models do, and Claude reviews them.")
WORKFLOW_LANE = ("Burr workflows: tools/wf",
                 "Small typed Burr graphs. Each step is a Python function from a frozen pydantic input to an output, "
                 "and a pause point waits for an answer from outside (wf answer). Every run is tracked for the Burr "
                 "UI (port 7241); wf status lists them all.")


# ── text that stands on its own ────────────────────────────────────────
# Each card is read alone, so these name what they act on ("the subsystem's
# new C file", not "the file") and are whole sentences. The graphs'
# docstrings are written to be read in place; they are shown only for a step
# that has no entry here, and the generator names those steps.
# {routes}, {route_count}, {convert_models} and {convert_rounds} come from the code.

WORKFLOW_SUMMARIES: dict[str, str] = {
    "verify":
        "Checks that the port still behaves exactly like the original game after a change to the main checkout. "
        "It lifts bn6f with the hand-written readable functions in place and builds the port ROM and the host "
        "difftest. It compares the named readable functions with their lifted originals, first on random inputs "
        "and then on the original game's own captured calls, and runs them on the CPU oracle against the real "
        "game. It replays the {route_count} recorded routes in lockstep against the original, and the 116 recorded "
        "battles when battle code changed. Each check runs only if the earlier ones passed. The result is a "
        "VerifyReport with one verdict.",
    "capture":
        "Saves real inputs for testing conversions. Given some lifted bn6f functions, it picks a recorded "
        "gameplay route that executes them, replays the route on the original game in mGBA, and saves the "
        "machine state at the first calls of each function (40 by default) into a snapshot directory. The host "
        "difftest and the CPU oracle start from those saved calls to compare a readable version with the original.",
    "farm":
        "One cheap outside model tries to convert one lifted bn6f function into readable C. The model sees the "
        "function's assembly, lifted C and register contract. Its reply is tested against the lifted original on "
        "the captured calls and on random inputs; a failure goes back to the same model with the test output, "
        "for up to 3 rounds by default. A passing version is saved in tools/convert/accepted/, and the same "
        "model drafts its symbols/db entry beside it. Nothing is committed.",
    "farm-batch":
        "Runs the farm over many functions and models at once; `wf farm` starts it. It builds the host difftest "
        "once from the current sources, starts one farm run per function and model in parallel, skips pairs "
        "that passed or ran out of rounds in an earlier batch, and counts how many functions each model converted.",
    "drafts":
        "Catches up on documentation for small functions: for every accepted farm candidate of a model that has "
        "no drafted symbols/db entry, the same cheap model reads the function's assembly, register contract and "
        "verified C and drafts the entry (name, status, summary, interface, details, open questions) beside the "
        "candidate. The convert workflow takes these drafts into review and the symbol database.",
    "convert":
        "Turns a subsystem (a group of lifted bn6f functions that call each other) into one new readable C file "
        "under cport/src/game. Strong outside models write the file; Claude reviews it. The run captures the "
        "original game's calls of the functions and asks {convert_models} in that order, {convert_rounds} "
        "attempts each, for the file. It quick-tests every function, installs the file and runs the full verify, "
        "then stops for Claude's review. Approved work is committed, documented in symbols/db and recorded in "
        "the bn6f disassembly.",
    "record":
        "Writes what has been learnt about bn6f symbols into the project's permanent record. It checks the "
        "symbols/db entries (one YAML file per symbol: meaning, status, evidence, reproduce commands). It renames "
        "symbols in the bn6f disassembly where the meaning is confirmed or probable, and the ROM must stay "
        "byte-identical. It writes each entry as a `// symdb:` comment at the symbol's definition and regenerates "
        "symbols/SYMBOLS.md and symbols/symdb.sqlite. It re-verifies the port if anything was renamed, and "
        "commits locally.",
}

STEP_SUMMARIES: dict[tuple[str, str], str] = {
    ("verify", "note_sources"):
        "Every verify run starts by recording which git commit of the main checkout it verifies, and whether "
        "that checkout has uncommitted changes, so its VerifyReport says exactly what it covered.",
    ("verify", "lift_and_build_everything"):
        "A verify run lifts the bn6f disassembly to C with the hand-written readable functions (cport/src/game) "
        "in place of their lifted versions. It builds the port ROM in cport/build-wf and the host difftest "
        "program in cport/build-wf-host. If either build fails, the run ends with the failure in its report.",
    ("verify", "difftest_random_inputs"):
        "For each readable function named when the verify run started, this step runs the readable version and "
        "its lifted original from the same random CPU and memory state (300 trials by default). It compares "
        "memory, the return target, the stack pointer, and the registers and flags in the function's contract. "
        "Functions that follow pointers into game structures are often out of domain on random memory; the "
        "report lists them as never compared.",
    ("verify", "difftest_captured_calls"):
        "A verify run makes the same comparison as the random difftest, but starting from the original game's "
        "own calls of each named function, saved earlier by a capture run. It is skipped when the run was given "
        "no snapshot directory. Any failure ends the run.",
    ("verify", "run_cpu_oracle"):
        "A verify run runs each named readable function in the port and in the original game (mGBA with the "
        "real BIOS) from the same entry states, built from real game memory (24 states per function by default), "
        "and compares the results. Any difference ends the run with the verdict that the CPU oracle saw "
        "differences: either a known harness artifact (tools/oracle_triage.md) or a real bug.",
    ("verify", "lockstep_routes"):
        "A verify run replays each recorded gameplay route ({routes}) on the original game and on the port side "
        "by side. It compares palette, VRAM and OAM at every checkpoint, and save memory at the end. This catches "
        "behaviour changes anywhere in the game, not only in the named functions.",
    ("verify", "sweep_battles"):
        "Only when battle code changed, a verify run replays the 116 recorded battles in lockstep against the "
        "original game (tools/battlesweep.sh).",
    ("verify", "write_report"):
        "Every verify run ends by summing up the build, the difftests, the CPU oracle, each route and the battle "
        "sweep into a VerifyReport. Its verdict is one of: passed; passed, but some functions were never "
        "compared; the CPU oracle saw differences; failed; or build failed.",
    ("capture", "assign_routes"):
        "A capture run gives each function it was asked to capture a recorded gameplay route whose coverage "
        "profile (tools/traces/coverage) executes it, using as few routes as possible: it repeatedly takes the "
        "route that runs the most functions still unassigned. If no route executes any of them, the run ends "
        "with nothing captured.",
    ("capture", "capture_calls_on_routes"):
        "A capture run replays each assigned route on the original game in mGBA (tools/convert/capture.sh), all "
        "routes in parallel, and saves the CPU and memory state at the first calls of each function assigned to "
        "the route (40 by default) into the run's snapshot directory.",
    ("capture", "write_report"):
        "Every capture run ends with a CaptureReport: where the snapshot directory is, which route each function "
        "was captured on, how many calls were saved for each, and which functions no route called.",
    ("drafts", "find_undrafted_candidates"):
        "A drafts run lists the model's accepted farm candidates in tools/convert/accepted/ that have no drafted "
        "symbols/db entry beside them (of the functions it was given, or all).",
    ("drafts", "draft_each_candidate"):
        "A drafts run asks the model for each candidate's entry, several at a time (16 by default), and saves "
        "each as NAME.MODEL.yaml beside the candidate. An unusable reply is asked for once more.",
    ("drafts", "summarise_drafts"):
        "A drafts run ends with how many entries it wrote and which candidates got none, with the reason.",
    ("farm", "gather_context"):
        "A farm run (one lifted bn6f function, one outside model) starts by collecting what the model needs: "
        "the function's assembly from bn6f, its lifted C, its register contract from difftest.cfg, and its "
        "captured calls. If there is nothing to convert, the run ends.",
    ("farm", "ask_model_for_c"):
        "A farm run sends its outside model a prompt asking for a readable C version of the function being "
        "farmed. After a failed test, the prompt also shows the model its previous code and the test failure. "
        "The tokens used are logged to tools/convert/usage.jsonl.",
    ("farm", "test_reply"):
        "A farm run compiles the model's C into the host difftest and compares it with the lifted original on "
        "the captured calls and on random inputs, the same acceptance test tools/convert/farm.py uses. The round "
        "is logged to tools/convert/log. A failure goes back to the model while rounds are left (3 by default).",
    ("farm", "accept_candidate"):
        "When the model's C passes, the farm run saves it in tools/convert/accepted/ for Claude's review. "
        "Nothing is installed or committed.",
    ("farm", "give_up"):
        "A farm run that produced no passing version records why: there was nothing to convert, the model's "
        "provider failed, every round failed its test, or the reply had no code (a runaway: the same prompt is "
        "not sent again; another model gets the function later).",
    ("farm-batch", "build_difftest"):
        "A farm batch (started by `wf farm`) builds the host difftest once from the current sources; every farm "
        "run in the batch uses it. Building it fresh matters: a stale difftest once accepted candidates that "
        "returned to the wrong place.",
    ("farm-batch", "convert_each_pair"):
        "A farm batch starts one farm run for every pair of function and model it was given, several at a time "
        "(4 by default). It skips pairs that already passed, or ran out of rounds, in an earlier batch.",
    ("farm-batch", "summarise"):
        "A farm batch ends by counting how many of its functions each model converted. "
        "tools/convert/scorecard.py adds tokens and time.",
    ("convert", "capture_original_calls"):
        "A convert run starts a child capture run that saves the original game's calls of every function in the "
        "subsystem being converted; the tests start from these calls. It is skipped when the run was given an "
        "existing snapshot directory (--snapshots).",
    ("convert", "gather_context"):
        "A convert run collects each subsystem function's assembly, lifted C and register contract for the "
        "model's prompt, and builds the host difftest for quick tests. The run ends here if cport/src/game "
        "already has a file with the requested name, or if a function has no lifted code.",
    ("convert", "ask_model_for_file_step"):
        "A convert run asks a strong outside model to write the subsystem's whole new C file "
        "(cport/src/game/NAME.c), with a symbols/db draft for each function. The models are tried in order "
        "({convert_models}), {convert_rounds} attempts each, and later attempts include what failed or the "
        "reviewer's comments. With --candidate, a ready file such as reviewed farm output is tried first.",
    ("convert", "check_registers"):
        "A convert run checks that the model's new C file keeps guest registers (R0 to R12, flags, GUEST_CALL) to "
        "thin Next adapters, using tools/abi/lint.py, before any test runs. A typed function may touch registers "
        "only with a `register form` note saying why C cannot work there, such as a call to code outside the "
        "file that has no typed interface yet. A file that fails goes back to the model with each offending "
        "function and line while attempts are left; a given farm file that fails ends the run.",
    ("convert", "quick_test"):
        "A convert run checks the model's new C file before it touches the source tree. The file must have the "
        "expected shape and a symbols/db draft for each function, and it must compile. Each function is then "
        "compared with its lifted original on the captured calls and on random inputs, without rebuilding the "
        "ROM. A failure goes back to the model while attempts are left.",
    ("convert", "install_and_verify"):
        "A convert run copies the model's file into cport/src/game, with its difftest.cfg lines, and starts a "
        "child verify run with every check. If verification fails, the file is taken out of the tree again and "
        "the failure goes back to the model.",
    ("convert", "review_conversion"):
        "A convert run stops here and waits for Claude to review the subsystem's new C file. Claude sees its "
        "symbols/db drafts, the test results, the verify report and every attempt. The answer is approve, "
        "request changes (the comments go back to the model), or reject.",
    ("convert", "commit_conversion"):
        "After Claude approves, a convert run commits the subsystem's new C file and its difftest.cfg lines to "
        "the main checkout's local git, never pushed. The message names the model, the attempt, the verify run "
        "and the review.",
    ("convert", "write_symbol_entries"):
        "After the commit, a convert run writes a symbols/db entry for each converted function. Each entry joins "
        "the model's draft with this run's evidence: tests, verify run, commit. The step also picks the renames "
        "whose meaning is confirmed or probable.",
    ("convert", "record_symbols"):
        "A convert run ends by starting a child record run for the subsystem's functions. That run checks the "
        "new entries, renames the symbols in bn6f, writes the comments, re-verifies the port and commits.",
    ("convert", "give_up"):
        "A convert run that ends without a commit takes the model's file out of cport/src/game again if it was "
        "installed, and says why: nothing to convert, the host difftest did not build, every model ran out of "
        "attempts, or Claude rejected it.",
    ("record", "check_entries"):
        "A record run (for symbols named with `wf record`, or by a convert run) runs tools/symdb.py check on the "
        "symbol database. It checks that every named symbol has an entry, and that every requested rename has a "
        "confirmed or probable meaning. Problems end the run with nothing changed.",
    ("record", "rename_in_bn6f"):
        "A record run renames the requested symbols with tools/rename_symbol.py: in the bn6f disassembly sources "
        "(rebuilt, checked byte-identical, and committed in bn6f), in the port's hand-written code, and in the "
        "regenerated lifted C.",
    ("record", "refresh_pc_port_symbols_after_renames"):
        "After renames, a record run refreshes the PC port's symbol overlay from the renamed bn6f "
        "(tools/import_symbols.sh).",
    ("record", "write_comments_into_bn6f"):
        "A record run writes each symbols/db entry as a `// symdb:` comment block at the symbol's definition in "
        "the bn6f disassembly (tools/symdoc.py), checks that the ROM is still byte-identical, and commits in bn6f.",
    ("record", "render_symbols"):
        "A record run regenerates symbols/SYMBOLS.md (the readable symbol list) and symbols/symdb.sqlite from "
        "the entries in symbols/db.",
    ("record", "reverify_port"):
        "Only after renames, a record run starts a child verify run that replays every recorded route. Renames "
        "change the port's layout, not its behaviour; this proves it.",
    ("record", "commit_records"):
        "A record run commits the symbols/db entries, the bn6f submodule pointer and any renamed port code to "
        "the main checkout's local git, never pushed.",
    ("record", "write_report"):
        "Every record run ends with a RecordReport: the bn6f commits, the verify result after renames and the "
        "main commit, or the problems that stopped it.",
}

FIELD_GLOSSARY: dict[str, str] = {
    "run_dir": "this run's folder for tool logs, under build-wf-runs/",
    "functions": "the bn6f function names this run works on",
    "function": "the bn6f function name this run works on",
    "snapshots": "the directory of the original game's captured calls, made by a capture run",
    "routes": "the recorded gameplay routes to replay (tools/traces)",
    "candidate_routes": "the recorded gameplay routes to choose from",
    "route": "the recorded gameplay route chosen for capturing",
    "coverage": "how many of the functions each route executes",
    "captured": "how many calls were saved for each function",
    "calls_per_function": "how many calls to save for each function",
    "build": "the built port ROM and host difftest, or why the build failed",
    "difftest": "the host difftest program the tests run in",
    "difftests": "one random-input difftest result per function",
    "captured_difftests": "one captured-call difftest result per function, or why none ran",
    "oracle": "one CPU-oracle result per function, or why none ran",
    "locksteps": "one lockstep result per recorded route",
    "battle_sweep": "the 116-battle lockstep sweep's result, or why it did not run",
    "sources": "the commit verified, and whether the checkout had uncommitted changes",
    "model": "an outside model, written provider/model (e.g. hyper/glm-5.3-flash)",
    "models": "outside models, written provider/model, in the order they are tried",
    "attempts": "every model reply so far, with its register check, tests, verify run and review",
    "context": "the function's assembly, lifted C and register contract",
    "contexts": "each function's assembly, lifted C and register contract",
    "subsystem": "what the functions do together, in a few words",
    "file_name": "the new C file's name under cport/src/game",
    "capture": "the child capture run's report, or why it was skipped",
    "checks": "for each function: the quick test, verify and review evidence for symbols/db",
    "entries": "the symbols/db files written",
    "record": "the child record run's report",
    "verification": "a verify run's report",
    "report": "this run's final report",
    "symbols": "the bn6f symbols to record",
    "renames": "pairs of old and new symbol names",
    "problems": "what the database check or the rename checks found",
    "renamed_in_bn6f": "the bn6f commit with the renames",
    "comments_in_bn6f": "the bn6f commit with the `// symdb:` comments, if anything changed",
    "commit": "a local git commit in the main checkout",
    "commit_message": "the message for the main checkout's commit",
    "pairs": "one result per function and model",
    "tallies": "how many functions each model converted",
    "rounds": "how many times a model may try",
    "jobs": "how many farm runs go at once",
    "trials": "random inputs per function",
    "oracle_states": "entry states per function on the CPU oracle",
    "battle_code": "whether the change touches battle code, which adds the battle sweep",
    "accepted": "where the passing candidate was saved",
    "outcome": "how the run ended",
    "candidate": "a ready C file to try before asking a model",
    "decision": "approve, request changes, or reject",
    "comments": "what the reviewer wants changed, or notes for the commit message",
}


def _facts() -> dict[str, str]:
    route_names = [route.name for route in ALL_ROUTES]
    rounds = ConvertState.model_fields["rounds"].get_default(call_default_factory=False)  # pyright: ignore[reportAny]
    return {"routes": ", ".join(route_names), "route_count": str(len(route_names)),
            "convert_models": ", ".join(DEFAULT_MODELS), "convert_rounds": f"{rounds}"}

# ── what the code says about the workflows ─────────────────────────────

class PathKind(Enum):
    MAIN = "main"            # the first transition a step lists
    ALTERNATIVE = "alternative"
    LOOP = "loop"            # back to an earlier step


@dataclass(frozen=True)
class FieldInfo:
    name: str
    type_name: str
    note: str


@dataclass(frozen=True)
class ModelInfo:
    name: str
    doc: str
    fields: tuple[FieldInfo, ...]


@dataclass(frozen=True)
class NextStep:
    step: str
    decision: str | None
    why: str
    path: PathKind


@dataclass(frozen=True)
class StepInfo:
    workflow: str
    name: str
    doc: str
    reads: tuple[FieldInfo, ...]
    waits_for: ModelInfo | None
    produces: tuple[FieldInfo, ...]
    next_steps: tuple[NextStep, ...]
    children: tuple[str, ...]       # workflows it starts as child runs
    asks_a_model: bool
    is_end: bool
    source: str


@dataclass(frozen=True)
class WorkflowInfo:
    name: str
    project: str
    doc: str
    command: str | None
    steps: tuple[StepInfo, ...]
    source: str


def collect_workflows() -> tuple[WorkflowInfo, ...]:
    runners = _child_runners()
    usage = _command_usage()
    return tuple(_workflow_info(entry, runners, usage) for entry in WORKFLOWS)


def _workflow_info(entry: WorkflowEntry, runners: dict[str, str], usage: dict[str, str]) -> WorkflowInfo:
    graph = entry.graph()
    order = _steps_in_order(graph.first, graph.transitions)
    loops = _loops(graph.first, graph.transitions)
    steps: list[StepInfo] = []
    for step in order:
        models = step_models(step)
        source = inspect.getsource(step)
        body = source.split(":\n", 1)[-1]
        steps.append(StepInfo(
            workflow=entry.name, name=step.__name__, doc=inspect.getdoc(step) or "",
            reads=_fields(models.input),
            waits_for=_model_info(models.answer) if models.answer is not None else None,
            produces=_fields(models.output),
            next_steps=_next_steps(step, graph.transitions, loops),
            children=tuple(dict.fromkeys(workflow for runner, workflow in runners.items()
                                         if re.search(rf"\b{runner}\(", body))),
            asks_a_model=re.search(r"\bask_model\w*\(", body) is not None,
            is_end=step in graph.ends,
            source=_source_of(step)))
    command = usage.get(COMMAND_FOR_WORKFLOW.get(entry.name, entry.name))
    return WorkflowInfo(entry.name, entry.project, inspect.getdoc(entry.module) or "", command, tuple(steps),
                        _relative(Path(inspect.getfile(entry.module))))


def _child_runners() -> dict[str, str]:
    """Function names that start a run of a workflow: its graph function, and run_* wrappers around it."""
    runners: dict[str, str] = {}
    for entry in WORKFLOWS:
        graph_function = entry.graph.__name__
        runners[graph_function] = entry.name
        for name, member in inspect.getmembers(entry.module, inspect.isfunction):
            if name.startswith("run_") and re.search(rf"\b{graph_function}\(\)", inspect.getsource(member)):
                runners[name] = entry.name
    return runners


def _command_usage() -> dict[str, str]:
    """`python -m wf NAME ...` lines from the command line's own help text."""
    text = (Path(__file__).with_name("__main__.py")).read_text()
    usage: dict[str, str] = {}
    last = ""
    for line in text.split('"""', 2)[1].splitlines():
        match = re.match(r"\s*python -m wf (\w+)\s.*", line)
        if match:
            last = match.group(1)
            usage[last] = " ".join(match.group(0).split("#")[0].split())
        elif last and line.startswith("    ") and line.strip().startswith("["):
            usage[last] += " " + " ".join(line.split())
        else:
            last = ""
    return usage


def _steps_in_order(first: AnyStepOrPause, transitions: Sequence[Transition]) -> list[AnyStepOrPause]:
    order = [first]
    for transition in transitions:
        for step in (transition.after, transition.go_to):
            if step not in order:
                order.append(step)
    return order


def _next_steps(step: AnyStepOrPause, transitions: Sequence[Transition],
                loops: list[Transition]) -> tuple[NextStep, ...]:
    mine = [transition for transition in transitions if transition.after is step]
    result: list[NextStep] = []
    for index, transition in enumerate(mine):
        if transition in loops:
            path = PathKind.LOOP
        else:
            path = PathKind.MAIN if index == 0 else PathKind.ALTERNATIVE
        when = transition.when
        result.append(NextStep(transition.go_to.__name__, when.__name__ if when else None,
                               (inspect.getdoc(when) or "") if when else "", path))
    return tuple(result)


def _loops(first: AnyStepOrPause, transitions: Sequence[Transition]) -> list[Transition]:
    """The transitions that go back to a step the run has already been through on its way here: the back
    edges of a depth-first walk from the first step, taking each step's transitions in the order listed."""
    loops: list[Transition] = []
    done: list[AnyStepOrPause] = []
    on_the_way: list[AnyStepOrPause] = []

    def walk(step: AnyStepOrPause) -> None:
        on_the_way.append(step)
        for transition in transitions:
            if transition.after is not step:
                continue
            if transition.go_to in on_the_way:
                loops.append(transition)
            elif transition.go_to not in done:
                walk(transition.go_to)
        on_the_way.pop()
        done.append(step)

    walk(first)
    return loops


def _model_info(model: type[BaseModel]) -> ModelInfo:
    own_doc = vars(model).get("__doc__")
    return ModelInfo(model.__name__, own_doc if isinstance(own_doc, str) else "", _fields(model))


def _fields(model: type[BaseModel]) -> tuple[FieldInfo, ...]:
    # the model's own comment first; then the glossary; then a comment on another model in the module, which
    # may describe the field in that model's sense (a run's state: "reuse a capture instead of capturing")
    notes = {**_module_notes(model), **FIELD_GLOSSARY, **_field_notes(model)}
    return tuple(FieldInfo(name, _type_name(field.annotation), notes.get(name, ""))
                 for name, field in model.model_fields.items())


def _module_notes(model: type[BaseModel]) -> dict[str, str]:
    """Field comments on every model in the same module (a run's state usually comments its fields)."""
    notes: dict[str, str] = {}
    for _, member in inspect.getmembers(sys.modules[model.__module__], inspect.isclass):
        if issubclass(member, BaseModel) and member.__module__ == model.__module__:
            notes.update(_field_notes(member))
    return notes


def _field_notes(model: type[BaseModel]) -> dict[str, str]:
    """The `# comment` after each field in the model's source."""
    notes: dict[str, str] = {}
    for line in inspect.getsource(model).splitlines():
        match = re.match(r"\s+(\w+)\s*:[^#]*#\s*(.+)$", line)
        if match:
            notes[match.group(1)] = match.group(2).strip()
    return notes


def _type_name(annotation: object) -> str:
    text = annotation.__name__ if isinstance(annotation, type) else str(annotation)
    text = re.sub(r"\b(?:wf|pathlib|typing)(?:\.\w+)*\.(?=[A-Za-z_])", "", text)
    if isinstance(annotation, type) and issubclass(annotation, Enum):
        values = " | ".join(member.name.lower().replace("_", " ") for member in annotation)
        text += f" ({values})"
    return text


def _source_of(function: Callable[..., object]) -> str:  # pyright: ignore[reportExplicitAny]
    line = inspect.getsourcelines(function)[1]
    return f"{_relative(Path(inspect.getfile(function)))}:{line}"


def _relative(path: Path) -> str:
    try:
        return str(path.resolve().relative_to(REPO.resolve()))
    except ValueError:
        return str(path)


# ── tooltips ───────────────────────────────────────────────────────────

class Row(BaseModel):
    model_config = ConfigDict(frozen=True)
    code: str = ""
    text: str = ""


class Section(BaseModel):
    model_config = ConfigDict(frozen=True)
    heading: str
    rows: tuple[Row, ...]


class Tip(BaseModel):
    model_config = ConfigDict(frozen=True)
    chip: str
    tone: str                  # the chip's colour, a CSS class
    title: str
    lead: str
    sections: tuple[Section, ...] = ()
    joins: tuple[str, ...] = ()  # an edge's two ends, highlighted with it


def _field_rows(fields: tuple[FieldInfo, ...]) -> tuple[Row, ...]:
    return tuple(Row(code=f"{field.name}: {field.type_name}", text=field.note) for field in fields) \
        or (Row(text="nothing"),)


def step_tip(step: StepInfo, answered_by: OrchestratorAction | None) -> Tip:
    kind = "Pause point" if step.waits_for else "Step"
    tone = "pause" if step.waits_for else ("model" if step.asks_a_model else "step")
    sections = [Section(heading="Reads", rows=_field_rows(step.reads))]
    if step.waits_for:
        rows = (Row(code=step.waits_for.name, text=step.waits_for.doc), *_field_rows(step.waits_for.fields))
        sections.append(Section(heading="Waits for", rows=rows))
    sections.append(Section(heading="Produces", rows=_field_rows(step.produces)))
    then = tuple(Row(code=f"→ {next_step.step}",
                     text=(f"when {next_step.decision.replace('_', ' ')}" if next_step.decision else "always")
                     + (f". {next_step.why}" if next_step.why else ""))
                 for next_step in step.next_steps) or (Row(text="the run ends"),)
    sections.append(Section(heading="Then", rows=then))
    if step.children:
        sections.append(Section(heading="Starts", rows=tuple(Row(code=child, text="a child run, shown under this "
                                                                 "step in the Burr UI") for child in step.children)))
    if step.asks_a_model:
        sections.append(Section(heading="Outside model", rows=(Row(text="This step sends a prompt to an outside "
                                                                    "model and logs its tokens to "
                                                                    "tools/convert/usage.jsonl."),)))
    if answered_by:
        sections.append(Section(heading="Answered by", rows=(Row(code=answered_by.title.replace("\n", " "),
                                                                 text="the orchestrator, with wf answer"),)))
    sections.append(Section(heading="Source", rows=(Row(code=step.source),)))
    return Tip(chip=f"{kind} · {step.workflow}", tone=tone, title=step.name, lead=step_summary(step),
               sections=tuple(sections))


def step_summary(step: StepInfo) -> str:
    summary = STEP_SUMMARIES.get((step.workflow, step.name))
    if summary is None:
        return f"A step of the {step.workflow} workflow. In the code: {step.doc}"
    return summary.format(**_facts())


def workflow_tip(info: WorkflowInfo, started_by: list[str], parents: list[str]) -> Tip:
    how = [Row(code=info.command)] if info.command else []
    how += [Row(text=f"by the orchestrator: {action}") for action in started_by]
    how += [Row(text=f"as a child run of {parent}") for parent in parents]
    steps = tuple(Row(code=step.name, text=("pause point" if step.waits_for else "")
                      + (" · ends the run" if step.is_end else "")) for step in info.steps)
    sections = (Section(heading="Started", rows=tuple(how)),
                Section(heading="Steps", rows=steps),
                Section(heading="Burr UI project", rows=(Row(code=info.project),)),
                Section(heading="Source", rows=(Row(code=info.source),)))
    summary = WORKFLOW_SUMMARIES.get(info.name)
    return Tip(chip="Workflow", tone="workflow", title=info.name,
               lead=summary.format(**_facts()) if summary else info.doc, sections=sections)


def action_tip(action: OrchestratorAction, starts: list[str], receives: list[str], answers: list[str]) -> Tip:
    sections = [Section(heading="Gets", rows=(Row(text=action.gets),)),
                Section(heading="Gives", rows=(Row(text=action.gives),)),
                Section(heading="Commands", rows=tuple(Row(code=command) for command in action.commands))]
    if starts:
        sections.append(Section(heading="Starts", rows=tuple(Row(code=workflow) for workflow in starts)))
    if answers:
        sections.append(Section(heading="Answers", rows=tuple(Row(code=pause) for pause in answers)))
    if receives:
        sections.append(Section(heading="Hears back from", rows=tuple(Row(code=workflow) for workflow in receives)))
    sections.append(Section(heading="Written down in", rows=(Row(text=action.documented),)))
    tone = {ActionKind.JUDGEMENT: "claude", ActionKind.ANSWERS_PAUSE: "pause", ActionKind.BY_HAND: "byhand"}
    return Tip(chip=f"Orchestrator · {action.kind.value}", tone=tone[action.kind],
               title=action.title.replace("\n", " "), lead=action.what, sections=tuple(sections))


# ── the diagram ────────────────────────────────────────────────────────

class EdgeKind(Enum):
    START = "start"
    WAIT = "wait"
    ANSWER = "answer"
    CHILD = "child"
    RESULT = "result"
    MAIN = "main"
    ALTERNATIVE = "alternative"
    LOOP = "loop"


@dataclass(frozen=True)
class Edge:
    tail: str
    head: str
    kind: EdgeKind
    label: str
    tip: Tip
    lhead: str | None = None
    back: bool = False          # drawn from tail to head, arrow at the tail
    constraint: bool = True


@dataclass(frozen=True)
class Diagram:
    dot: str
    tips: dict[str, Tip]


def slug(workflow: str) -> str:
    return workflow.replace("-", "_")


def step_id(workflow: str, step: str) -> str:
    return f"{slug(workflow)}__{step}"


def cluster_name(workflow: str) -> str:
    return f"cluster_wf_{slug(workflow)}"


def build_diagram(workflows: tuple[WorkflowInfo, ...], legend: bool) -> Diagram:
    by_name = {info.name: info for info in workflows}
    actions = {action.key: action for action in ORCHESTRATOR}
    answers = {(answer.workflow, answer.step): actions[answer.action] for answer in ANSWERS}
    _check_references(by_name, actions)
    tips: dict[str, Tip] = {}
    edges: list[Edge] = []
    extra_actions: list[OrchestratorAction] = []

    for info in workflows:
        for step in info.steps:
            if step.waits_for and (info.name, step.name) not in answers:
                orphan = OrchestratorAction(f"answer_{slug(info.name)}_{step.name}", f"Answer\n{step.name}",
                                            ActionKind.ANSWERS_PAUSE, f"Nothing in wf/orchestration.py says who "
                                            f"answers {info.name}.{step.name} yet.", step.waits_for.doc,
                                            step.waits_for.name, ("python -m wf answer RUN_ID ANSWER.json",),
                                            step.source)
                extra_actions.append(orphan)
                answers[(info.name, step.name)] = orphan
            for next_step in step.next_steps:
                kind = {PathKind.MAIN: EdgeKind.MAIN, PathKind.ALTERNATIVE: EdgeKind.ALTERNATIVE,
                        PathKind.LOOP: EdgeKind.LOOP}[next_step.path]
                why = (f"After {step.name}, a {info.name} run goes on to {next_step.step}"
                       + (f" when {next_step.decision.replace('_', ' ')}." if next_step.decision
                          else ": it is the only way on.")
                       + (f" {next_step.why}" if next_step.why else ""))
                edges.append(Edge(step_id(info.name, step.name), step_id(info.name, next_step.step), kind,
                                  (next_step.decision or "").replace("_", " "),
                                  Tip(chip=f"Transition · {info.name}", tone=kind.value,
                                      title=f"{step.name} → {next_step.step}", lead=why,
                                      sections=(Section(heading="Decision", rows=(Row(code=next_step.decision or "none: "
                                                                                      "the step's only way on"),)),)),
                                  constraint=kind is not EdgeKind.LOOP))
            for child in step.children:
                first = by_name[child].steps[0].name
                edges.append(Edge(step_id(info.name, step.name), step_id(child, first), EdgeKind.CHILD, "child run",
                                  Tip(chip="Child run", tone="child", title=f"{info.name}.{step.name} starts {child}",
                                      lead=f"The {info.name} workflow's {step.name} step starts a {child} run and "
                                           f"waits for it to finish. The Burr UI shows the {child} run as a child "
                                           "of that step.",
                                      sections=(Section(heading="Source", rows=(Row(code=step.source),)),)),
                                  lhead=cluster_name(child), constraint=False))

    all_actions = (*ORCHESTRATOR, *extra_actions)
    for start in STARTS:
        info = by_name[start.workflow]
        edges.append(Edge(f"claude__{start.action}", step_id(info.name, info.steps[0].name), EdgeKind.START,
                          start.command, Tip(chip="Starts a run", tone="start", title=info.command or start.command,
                                             lead=start.why), lhead=cluster_name(info.name)))
    for (workflow, step_name), action in answers.items():
        step = next(step for step in by_name[workflow].steps if step.name == step_name)
        assert step.waits_for is not None
        edges.append(Edge(f"claude__{action.key}", step_id(workflow, step_name), EdgeKind.WAIT,
                          f"waits: {_input_name(workflow, step_name)}",
                          Tip(chip="Pause point", tone="pause", title=f"{step_name} waits for the orchestrator",
                              lead=f"A {workflow} run stops at {step_name} and waits for the orchestrator. "
                                   f"`wf waiting RUN_ID` shows what the run was given (a "
                                   f"{_input_name(workflow, step_name)}); the run goes on when it gets an answer.",
                              sections=(Section(heading="Given", rows=_field_rows(step.reads)),)),
                          back=True, constraint=False))  # constraining it as well trips dot 2.43 ("trouble in init_rank")
        edges.append(Edge(f"claude__{action.key}", step_id(workflow, step_name), EdgeKind.ANSWER,
                          f"wf answer: {step.waits_for.name}",
                          Tip(chip="Answer", tone="answer", title="wf answer RUN_ID ANSWER.json",
                              lead=step.waits_for.doc or f"A {step.waits_for.name}.",
                              sections=(Section(heading=step.waits_for.name, rows=_field_rows(step.waits_for.fields)),)),
                          constraint=False))  # lets the row keep ACTION_ORDER

    # results last: drawn on top of the other arrows
    for handback in HANDBACKS:
        info = by_name[handback.workflow]
        edges.append(Edge(f"claude__{handback.action}", step_id(info.name, info.steps[0].name), EdgeKind.RESULT,
                          handback.label, Tip(chip=f"Result · {info.name}", tone="result",
                                              title=handback.label.replace("\n", " "), lead=handback.what),
                          lhead=cluster_name(info.name), back=True))
    for index, edge in enumerate(edges):
        tips[f"edge_{index}"] = edge.tip.model_copy(update={"joins": (edge.tail, edge.head)})
    starts_of = {action.key: [start.workflow for start in STARTS if start.action == action.key] for action in all_actions}
    hears_of = {action.key: [back.workflow for back in HANDBACKS if back.action == action.key] for action in all_actions}
    answers_of = {action.key: [f"{workflow}.{step}" for (workflow, step), answering in answers.items()
                               if answering.key == action.key] for action in all_actions}
    for action in all_actions:
        tips[f"claude__{action.key}"] = action_tip(action, starts_of[action.key], hears_of[action.key],
                                                   answers_of[action.key])
    for info in workflows:
        started_by = [actions[start.action].title.replace("\n", " ") for start in STARTS if start.workflow == info.name]
        parents = [f"{other.name}.{step.name}" for other in workflows for step in other.steps if info.name in step.children]
        tips[cluster_name(info.name)] = workflow_tip(info, started_by, parents)
        for step in info.steps:
            tips[step_id(info.name, step.name)] = step_tip(step, answers.get((info.name, step.name)))
    tips["cluster_claude"] = Tip(chip="Lane", tone="claude", title=CLAUDE_LANE[0], lead=CLAUDE_LANE[1])
    tips["cluster_workflows"] = Tip(chip="Lane", tone="workflow", title=WORKFLOW_LANE[0], lead=WORKFLOW_LANE[1])
    return Diagram(_dot(workflows, all_actions, edges, legend), tips)


def _input_name(workflow: str, step: str) -> str:
    entry = next(entry for entry in WORKFLOWS if entry.name == workflow)
    graph = entry.graph()
    function = next(candidate for candidate in _steps_in_order(graph.first, graph.transitions)
                    if candidate.__name__ == step)
    return step_models(function).input.__name__


def _check_references(workflows: dict[str, WorkflowInfo], actions: dict[str, OrchestratorAction]) -> None:
    for start in STARTS:
        assert start.workflow in workflows and start.action in actions, start
    for handback in HANDBACKS:
        assert handback.workflow in workflows and handback.action in actions, handback
    for answer in ANSWERS:
        assert answer.action in actions, answer
        steps = workflows[answer.workflow].steps
        assert any(step.name == answer.step and step.waits_for for step in steps), \
            f"{answer.workflow}.{answer.step} is not a pause point any more"


def _dot(workflows: tuple[WorkflowInfo, ...], actions: Sequence[OrchestratorAction], edges: list[Edge],
         legend: bool) -> str:
    actions = sorted(actions, key=lambda action: _position(ACTION_ORDER, action.key))
    workflows = tuple(sorted(workflows, key=lambda info: _position(WORKFLOW_ORDER, info.name)))
    lines = ["digraph orchestration {",
             '  rankdir=TB; compound=true; nodesep=0.25; ranksep=0.42; fontname="Helvetica";',
             "  mclimit=0.01;  // keep the declared order: full crossing reduction reshuffles the columns",
             '  outputorder="nodesfirst";  // every arrow is drawn over every box, in the order declared',
             '  bgcolor="transparent";' if not legend else '  bgcolor="white"; labelloc=t; fontsize=18;',
             '  node [fontname="Helvetica", fontsize=11, shape=box, style="rounded,filled", fillcolor="#c4e0eb", '
             'color="#2c3e4c", penwidth=1];',
             '  edge [fontname="Helvetica", fontsize=9, color="#333333", arrowsize=0.7];']
    if legend:
        lines.append(f"  label={_html('Orchestrator (Claude) and the Burr workflows', bold=True)};")
    lines += ["  subgraph cluster_claude {",
              f'    id="cluster_claude"; class="lane lane-claude"; label={_html(CLAUDE_LANE[0], bold=True)}; '
              'fontsize=14; style="rounded,filled"; fillcolor="#fbf3e2"; color="#c99a2e";']
    for action in actions:
        style = {ActionKind.JUDGEMENT: ('"rounded,filled"', "#f6dca0", "claude"),
                 ActionKind.ANSWERS_PAUSE: ('"rounded,filled"', "#f2b950", "claude pause"),
                 ActionKind.BY_HAND: ('"rounded,filled,dashed"', "#f6dca0", "claude byhand")}[action.kind]
        lines.append(f'    claude__{action.key} [id="claude__{action.key}", class="{style[2]}", '
                     f'label={_quote(action.title)}, style={style[0]}, fillcolor="{style[1]}", color="#8a6a22"'
                     + (", penwidth=2" if action.kind is ActionKind.ANSWERS_PAUSE else "") + "];")
    keys = " ".join(f"claude__{action.key};" for action in actions)
    lines += [f"    {{rank=same; {keys}}}",
              "    " + " -> ".join(f"claude__{action.key}" for action in actions) + " [style=invis];",
              "  }",
              "  subgraph cluster_workflows {",
              f'    id="cluster_workflows"; class="lane lane-wf"; label={_html(WORKFLOW_LANE[0], bold=True)}; '
              'fontsize=14; style="rounded,filled"; fillcolor="#e8f1f5"; color="#6f9fb2";']
    for info in reversed(workflows):  # dot 2.43 places sibling clusters right to left in declaration order
        lines += [f"    subgraph {cluster_name(info.name)} {{",
                  f'      id="{cluster_name(info.name)}"; class="workflow"; label={_html(info.name, bold=True)}; '
                  'fontsize=14; style="rounded,filled"; fillcolor="white"; color="#6f9fb2";']
        for step in info.steps:
            label = step.name
            classes = ["step"]
            attributes = ""
            if step.waits_for:
                label += f"\n⏸ waits for {step.waits_for.name}"
                classes.append("pause")
                attributes = ', fillcolor="#f2b950", penwidth=2'
            elif step.asks_a_model:
                classes.append("model")
                attributes = ', fillcolor="#c6e5c8"'
            if step.is_end:
                result = next((field.type_name for field in step.produces
                               if re.fullmatch(r"[A-Z]\w+", field.type_name) and field.type_name != "Path"), None)
                if result:
                    label += f"\n⇒ {result}"
            identifier = step_id(info.name, step.name)
            lines.append(f'      {identifier} [id="{identifier}", class="{" ".join(classes)}", '
                         f"label={_quote(label)}{attributes}];")
        lines.append("    }")
    lines.append("  }")
    colours = {EdgeKind.START: ("#18202b", "solid", "1.6"), EdgeKind.WAIT: ("#d07a00", "bold", "2.2"),
               EdgeKind.ANSWER: ("#d07a00", "bold", "2.2"), EdgeKind.CHILD: ("#2367a0", "dashed", "1.4"),
               EdgeKind.RESULT: ("#22804a", "solid", "1.4"), EdgeKind.MAIN: ("#333333", "solid", "1"),
               EdgeKind.ALTERNATIVE: ("#8d97a3", "solid", "1"), EdgeKind.LOOP: ("#b8382b", "solid", "1")}
    for index, edge in enumerate(edges):
        colour, style, width = colours[edge.kind]
        attributes = [f'id="edge_{index}"', f'class="{edge.kind.value}"', f'color="{colour}"', f'fontcolor="{colour}"',
                      f'style="{style}"', f"penwidth={width}", f"label={_quote(edge.label)}"]
        if edge.kind in (EdgeKind.ALTERNATIVE, EdgeKind.LOOP, EdgeKind.MAIN):
            attributes.append("fontsize=8")
        if edge.lhead:
            attributes.append(f'lhead="{edge.lhead}"')
        if edge.back:
            attributes.append("dir=back")
        if not edge.constraint:
            attributes.append("constraint=false")
        lines.append(f"  {edge.tail} -> {edge.head} [{', '.join(attributes)}];")
    if legend:
        lines += _legend()
    lines.append("}")
    return "\n".join(lines) + "\n"


def _position(order: tuple[str, ...], name: str) -> int:
    return order.index(name) if name in order else len(order)


def _legend() -> list[str]:
    rows = [("#18202b", "━━▶", "the orchestrator starts a run (python -m wf …)"),
            ("#d07a00", "━━▶", "a pause point waits for the orchestrator's answer"),
            ("#2367a0", "╍╍▶", "a step starts a child run"),
            ("#22804a", "━━▶", "a result handed back to the orchestrator"),
            ("#b8382b", "━━▶", "back to an earlier step"),
            ("#8d97a3", "━━▶", "another way on (failure, nothing to do)")]
    cells = "".join(f'<tr><td align="left"><font color="{colour}"><b>{arrow}</b></font></td>'
                    f'<td align="left">{text}</td></tr>' for colour, arrow, text in rows)
    cells += ('<tr><td align="left" bgcolor="#f6dca0">dashed</td><td align="left">by hand: not in a workflow yet</td></tr>'
              '<tr><td align="left" bgcolor="#c6e5c8">green</td><td align="left">asks an outside model</td></tr>')
    return [f'  legend [shape=plain, style="", label=<<table border="1" color="#aaaaaa" cellspacing="3" '
            f'cellpadding="3"><tr><td colspan="2" align="left"><b>Legend</b></td></tr>{cells}</table>>];',
            "  {rank=sink; legend}"]


def _quote(text: str) -> str:
    return json.dumps(text, ensure_ascii=False)  # "\n" stays the two characters dot reads as a line break


def _html(text: str, bold: bool = False) -> str:
    escaped = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    return f"<<b>{escaped}</b>>" if bold else f"<{escaped}>"


# ── the page ───────────────────────────────────────────────────────────

def render(dot: str, format_name: str, *options: str) -> bytes:
    done = subprocess.run(["dot", f"-T{format_name}", *options], input=dot.encode(), capture_output=True, check=True)
    return done.stdout


def inline_svg(svg: str) -> str:
    """The SVG as the page embeds it: no XML prologue, no <title>s (the page has its own tooltips)."""
    svg = svg[svg.index("<svg"):]
    svg = re.sub(r"<title>.*?</title>", "", svg, flags=re.S)
    svg = re.sub(r"<!--.*?-->", "", svg, flags=re.S)
    # the results handed back are drawn last, over every other arrow (Graphviz orders edges its own way)
    results = [match.group(0) for match in re.finditer(r'<g id="edge_\d+" class="edge result">.*?</g>\n', svg, flags=re.S)]
    for group in results:
        svg = svg.replace(group, "", 1)
    closing = svg.rindex("</g>")
    svg = svg[:closing] + "".join(results) + svg[closing:]
    width = re.search(r'width="([\d.]+)pt"', svg)
    height = re.search(r'height="([\d.]+)pt"', svg)
    size = f'data-width="{width.group(1) if width else 0}" data-height="{height.group(1) if height else 0}"'
    return re.sub(r'<svg width="[\d.]+pt" height="[\d.]+pt"', f'<svg id="map" {size} role="img" '
                  'aria-label="The orchestrator and the Burr workflows"', svg, count=1)


def stamp() -> str:
    commit = subprocess.run(["git", "-C", str(REPO), "log", "-1", "--format=%h %s"], capture_output=True, text=True,
                            check=True).stdout.strip()
    dirty = subprocess.run(["git", "-C", str(REPO), "status", "--porcelain", "--untracked-files=no", "--", "tools/wf"],
                           capture_output=True, text=True, check=True).stdout.strip()
    when = datetime.now().strftime("%Y-%m-%d %H:%M")
    return f"{when} from {commit}" + (" (with uncommitted changes in tools/wf)" if dirty else "")


def _check_order(svg: str, prefix: str, wanted: list[str]) -> None:
    """Warn when the layout does not show these groups left to right in this order (by their labels' x)."""
    found: list[tuple[float, str]] = []
    for match in re.finditer(rf'<g id="({prefix}[a-z_]+)" class="[^"]*">(.*?)</g>', svg, flags=re.S):
        label = re.search(r'<text[^>]*x="([-\d.]+)"', match.group(2))
        if label:
            found.append((float(label.group(1)), match.group(1)))
    shown = [name for _, name in sorted(found) if name in wanted]
    expected = [name for name in wanted if name in shown]
    if shown != expected:
        print(f"the map does not show {prefix}* in order: {', '.join(shown)}", file=sys.stderr)


def write_site(out_dir: Path) -> tuple[Path, Path]:
    workflows = collect_workflows()
    steps = {(info.name, step.name) for info in workflows for step in info.steps}
    for missing in sorted(steps - STEP_SUMMARIES.keys()):
        print(f"no standalone description for {missing[0]}.{missing[1]}: its card shows the docstring",
              file=sys.stderr)
    for stale in sorted(STEP_SUMMARIES.keys() - steps):
        print(f"STEP_SUMMARIES has {stale[0]}.{stale[1]}, which is not a step any more", file=sys.stderr)
    page_diagram = build_diagram(workflows, legend=False)
    raw_svg = render(page_diagram.dot, "svg").decode()
    _check_order(raw_svg, "cluster_wf_", [cluster_name(name) for name in WORKFLOW_ORDER])
    _check_order(raw_svg, "claude__", [f"claude__{key}" for key in ACTION_ORDER])
    svg = inline_svg(raw_svg)
    tips = TypeAdapter(dict[str, Tip]).dump_json(page_diagram.tips).decode().replace("</", "<\\/")
    page = (PAGE_TEMPLATE.read_text().replace("{{SVG}}", svg).replace("{{TIPS}}", tips)
            .replace("{{STAMP}}", stamp()))
    out_dir.mkdir(parents=True, exist_ok=True)
    html_path = out_dir / "orchestration.html"
    png_path = out_dir / "orchestration.png"
    html_path.write_text(page)
    png_path.write_bytes(render(build_diagram(workflows, legend=True).dot, "png", "-Gdpi=90"))
    return html_path, png_path


if __name__ == "__main__":
    for written in write_site(Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "docs"):
        print(written)
