"""The one place that touches Burr's partly untyped API.

A step is a plain function from its own input model to its own output model;
that signature is its whole contract:

    @workflow.step
    def lockstep_routes(given: LockstepInput) -> LockstepOutput: ...

The step reads the run-state fields named like its input model's fields and
writes those named like its output model's. Pydantic validates the input
before the step runs (a BuildFailure cannot reach a step that needs a
BuiltRom) and the output after. The Burr UI shows the output as the step's
Result, and the step's own source in its Code tab.

A pause point is a step that also needs an answer from outside the workflow
(from Claude or the user); the run stops before it and shows what it needs:

    @workflow.pause_point
    def write_readable_c(given: CodeRequest, answer: WrittenCode) -> CodeOutput: ...

Steps are wired into a `Graph` with `Transition`s that name the step
functions themselves; decisions are plain functions of the whole run state.
A step with one transition may leave out `when`; a step with several must
give each a `when`, and after the step runs exactly one must hold, or the
run stops with a TransitionError naming the step and the decisions involved.

Runs are tracked under ~/.burr/<project>/<run id> (WF_BURR_DIR overrides
~/.burr), which is what the Burr UI shows and what `resume` and
`final_state` read back. A run started from inside another run's step is
shown in the UI as that step's child.
"""
import os
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Generic, Never, Protocol, TypeVar, cast, get_type_hints

from burr.core import Application, ApplicationBuilder, ApplicationContext, Condition, State, default, serde
from burr.core.action import FunctionBasedAction
from burr.integrations.pydantic import PydanticTypingSystem, model_from_state  # pyright: ignore[reportUnknownVariableType]
from burr.integrations.serde.pydantic import set_allowlist
from burr.tracking.client import LocalTrackingClient
from pydantic import BaseModel

StateT = TypeVar("StateT", bound=BaseModel)
StateT_co = TypeVar("StateT_co", bound=BaseModel, covariant=True)
InputT = TypeVar("InputT", bound=BaseModel)
AnswerT = TypeVar("AnswerT", bound=BaseModel)
OutputT = TypeVar("OutputT", bound=BaseModel)

AnyStep = Callable[[Never], BaseModel]                       # a step with any input and output model
AnyPausePoint = Callable[[Never, Never], BaseModel]          # a pause point with any input, answer and output
AnyStepOrPause = AnyStep | AnyPausePoint


# Saved runs are read back by rebuilding pydantic classes named in the log; only ours may be built.
set_allowlist(["wf"])


@serde.serialize.register(tuple)
def _serialize_tuple(value: tuple[object, ...], **options: object) -> list[object]:
    """Burr's tracker knows lists but writes tuples as their repr; our state
    uses tuples, so show them item by item in the UI like lists."""
    return [serde.serialize(item, **options) for item in value]  # pyright: ignore[reportUnknownMemberType]


def burr_dir() -> Path:
    return Path(os.environ.get("WF_BURR_DIR", Path.home() / ".burr"))


def current_run_id() -> str:
    """The id of the run whose step is running now (for naming the runs it starts)."""
    context = ApplicationContext.get()
    if context is None:
        raise RuntimeError("current_run_id() called outside a running step")
    return context.app_id


# ── wiring ─────────────────────────────────────────────────────────────

@dataclass(frozen=True)
class Transition:
    """After `after`, go to `go_to` when `when` holds (always, if `when` is left out)."""
    after: AnyStepOrPause
    go_to: AnyStepOrPause
    when: Callable[[Never], bool] | None = None


class TransitionError(Exception):
    """The transitions out of a step are missing a decision, or did not pick exactly one way on."""


# ── what a run comes back with ─────────────────────────────────────────

@dataclass(frozen=True)
class Finished(Generic[StateT_co]):
    run_id: str
    state: StateT_co


@dataclass(frozen=True)
class Paused:
    """The run stopped before a pause point; `request` is that step's input,
    `answer_type` what `resume` must be given."""
    run_id: str
    step: str
    request: BaseModel
    answer_type: type[BaseModel]


# ── workflows ──────────────────────────────────────────────────────────

class Workflow(Generic[StateT]):
    def __init__(self, state_type: type[StateT], project: str) -> None:
        self.state_type = state_type
        self.project = project
        self.burr_actions: dict[AnyStepOrPause, FunctionBasedAction] = {}
        self.pause_points: dict[str, tuple[type[BaseModel], type[BaseModel]]] = {}  # name -> (input, answer)

    def step(self, function: Callable[[InputT], OutputT]) -> Callable[[InputT], OutputT]:
        """Register a step; its input and output models say what it reads and writes."""
        input_type, output_type = _step_models(function)
        reads = self._state_fields(function, input_type)
        writes = self._state_fields(function, output_type)

        def call(given: BaseModel, answer: BaseModel | None) -> BaseModel:
            return function(cast(InputT, given))

        action = _burr_action(call, input_type, None, reads, writes, function)
        self.burr_actions[function] = action
        return function

    def pause_point(self, function: Callable[[InputT, AnswerT], OutputT]) -> Callable[[InputT, AnswerT], OutputT]:
        """Register a step that waits for an answer from outside the workflow."""
        input_type, answer_type, output_type = _pause_point_models(function)
        reads = self._state_fields(function, input_type)
        writes = self._state_fields(function, output_type)

        def call(given: BaseModel, answer: BaseModel | None) -> BaseModel:
            return function(cast(InputT, given), cast(AnswerT, answer))

        action = _burr_action(call, input_type, answer_type, reads, writes, function)
        self.burr_actions[function] = action
        self.pause_points[_name(function)] = (input_type, answer_type)
        return function

    def graph(self, first: AnyStepOrPause, ends: Sequence[AnyStepOrPause],
              transitions: Sequence[Transition]) -> "Graph[StateT]":
        """A run starts at `first` and finishes after any of `ends`."""
        _check_decisions(transitions)
        return Graph(self, first, ends, transitions)

    def _state_fields(self, function: Callable[..., BaseModel], model: type[BaseModel]) -> list[str]:  # pyright: ignore[reportExplicitAny]
        fields = list(model.model_fields)
        unknown = [field for field in fields if field not in self.state_type.model_fields]
        if unknown:
            raise TypeError(f"step {function.__name__}: {model.__name__} has fields {unknown} "
                            f"that {self.state_type.__name__} does not")
        return fields


class Graph(Generic[StateT]):
    def __init__(self, workflow: Workflow[StateT], first: AnyStepOrPause, ends: Sequence[AnyStepOrPause],
                 transitions: Sequence[Transition]) -> None:
        self.workflow = workflow
        self.first = first
        self.ends = ends
        self.transitions = transitions

    def start(self, start: StateT, run_id: str, tracked: bool = True) -> Finished[StateT] | Paused:
        """Run from the first step until an end step finishes or a pause point is reached."""
        builder = self._builder(run_id, tracked).with_state(start)  # pyright: ignore[reportUnknownMemberType]
        return self._run(builder.build())

    def waiting(self, run_id: str) -> Paused | None:
        """What a saved run is waiting for, if it stopped at a pause point."""
        application = self._load(run_id)
        next_step = application.get_next_action()
        if next_step is None or next_step.name not in self.workflow.pause_points:
            return None
        input_type, answer_type = self.workflow.pause_points[next_step.name]
        state = application.state
        request = input_type.model_validate({field: state[field] for field in input_type.model_fields})  # pyright: ignore[reportAny]
        return Paused(run_id=run_id, step=next_step.name, request=request, answer_type=answer_type)

    def resume(self, run_id: str, answer: BaseModel) -> Finished[StateT] | Paused:
        """Give a paused run its answer and carry on."""
        application = self._load(run_id)
        waiting = application.get_next_action()
        if waiting is None or waiting.name not in self.workflow.pause_points:
            raise ValueError(f"run {run_id} is not waiting at a pause point")
        _, answer_type = self.workflow.pause_points[waiting.name]
        if not isinstance(answer, answer_type):
            raise TypeError(f"{waiting.name} needs a {answer_type.__name__}, not a {type(answer).__name__}")
        application.step(inputs={"answer": answer})  # pyright: ignore[reportUnknownMemberType]
        return self._run(application)

    def _load(self, run_id: str) -> Application[StateT]:
        """A saved run, rebuilt from its tracking log, ready to carry on where it stopped."""
        saved_runs = LocalTrackingClient(self.workflow.project, storage_dir=str(burr_dir()))
        builder = self._builder(run_id, tracked=True, resuming=True).initialize_from(  # pyright: ignore[reportUnknownMemberType]
            saved_runs, resume_at_next_action=True, default_state={}, default_entrypoint=_name(self.first))
        return builder.build()

    def final_state(self, run_id: str) -> StateT:
        """The state a finished run ended with, read back from its tracking log."""
        tracker = LocalTrackingClient(self.workflow.project, storage_dir=str(burr_dir()))
        loaded = tracker.load(partition_key="", app_id=run_id)  # pyright: ignore[reportUnknownMemberType]
        if loaded is None:
            raise ValueError(f"no run {run_id} in project {self.workflow.project}")
        return model_from_state(model=self.workflow.state_type, state=loaded["state"])

    def draw(self, path_without_extension: Path) -> None:
        empty: State[BaseModel] = State({})
        application = self._builder("diagram", tracked=False).with_state(empty).build()  # pyright: ignore[reportUnknownMemberType]
        application.visualize(output_file_path=str(path_without_extension), format="png",  # pyright: ignore[reportUnknownMemberType]
                              include_conditions=True, include_state=True)

    def _builder(self, run_id: str, tracked: bool, resuming: bool = False) -> ApplicationBuilder[StateT]:
        steps = {*(transition.after for transition in self.transitions),
                 *(transition.go_to for transition in self.transitions)}
        builder = (
            ApplicationBuilder()
            .with_typing(PydanticTypingSystem(self.workflow.state_type))
            .with_actions(**{_name(step): self.workflow.burr_actions[step] for step in steps})  # pyright: ignore[reportUnknownMemberType]
            .with_transitions(*self._burr_transitions())
            .with_identifiers(app_id=run_id)
        )
        if not resuming:
            builder = builder.with_entrypoint(_name(self.first))
        if tracked:
            builder = builder.with_tracker("local", project=self.workflow.project,
                                           params={"storage_dir": str(burr_dir())})
        parent = ApplicationContext.get()  # set while a step of another run is running
        if parent is not None and parent.sequence_id is not None:
            builder = builder.with_spawning_parent(parent.app_id, parent.sequence_id)
        return builder

    def _run(self, application: Application[StateT]) -> Finished[StateT] | Paused:
        pauses = list(self.workflow.pause_points)
        _, _, state = application.run(halt_after=[_name(end) for end in self.ends], halt_before=pauses)  # pyright: ignore[reportUnknownMemberType, reportUnknownVariableType]
        run_id = application.uid
        waiting = application.get_next_action()
        if waiting is not None and waiting.name in self.workflow.pause_points:
            input_type, answer_type = self.workflow.pause_points[waiting.name]
            request = input_type.model_validate({field: state[field] for field in input_type.model_fields})  # pyright: ignore[reportAny]
            return Paused(run_id=run_id, step=waiting.name, request=request, answer_type=answer_type)
        return Finished(run_id=run_id, state=model_from_state(model=self.workflow.state_type, state=state))

    def _burr_transitions(self) -> list[tuple[str, str, Condition]]:
        burr_transitions: list[tuple[str, str, Condition]] = []
        for transition in self.transitions:
            siblings = [other for other in self.transitions if other.after is transition.after]
            condition = default if transition.when is None else self._condition(transition, siblings)
            burr_transitions.append((_name(transition.after), _name(transition.go_to), condition))
        return burr_transitions

    def _condition(self, transition: Transition, siblings: list[Transition]) -> Condition:
        """Holds when this transition's decision does, after checking that exactly one sibling's does."""
        state_type = self.workflow.state_type
        own_test = cast(Callable[[StateT], bool], transition.when)
        sibling_tests = [cast(Callable[[StateT], bool], sibling.when) for sibling in siblings]

        def resolve(state: State[BaseModel]) -> bool:
            typed_state = model_from_state(model=state_type, state=state)
            holding = [test for test in sibling_tests if test(typed_state)]
            if len(holding) != 1:
                decisions = ", ".join(f"{_name(sibling.when)} -> {_name(sibling.go_to)}"
                                      for sibling in siblings if sibling.when is not None)
                raise TransitionError(f"after {_name(transition.after)}, {len(holding)} of these held "
                                      f"(exactly one must): {decisions}")
            return holding[0] is own_test

        return Condition(keys=list(state_type.model_fields), resolver=resolve, name=_name(own_test))


class GraphView(Protocol):
    """What tools that list, resume or draw any workflow need of a Graph, whatever its state type."""
    @property
    def first(self) -> AnyStepOrPause: ...
    @property
    def ends(self) -> Sequence[AnyStepOrPause]: ...
    @property
    def transitions(self) -> Sequence[Transition]: ...
    def waiting(self, run_id: str) -> Paused | None: ...
    def resume(self, run_id: str, answer: BaseModel) -> Finished[BaseModel] | Paused: ...
    def draw(self, path_without_extension: Path) -> None: ...


@dataclass(frozen=True)
class StepModels:
    """The models a step or pause point's signature names."""
    input: type[BaseModel]
    answer: type[BaseModel] | None
    output: type[BaseModel]


def step_models(function: AnyStepOrPause) -> StepModels:
    parameters, output_type = _signature_models(function)
    return StepModels(input=parameters[0], answer=parameters[1] if len(parameters) > 1 else None, output=output_type)


# ── helpers ────────────────────────────────────────────────────────────

def _burr_action(call: Callable[[BaseModel, BaseModel | None], BaseModel], input_type: type[BaseModel],
                 answer_type: type[BaseModel] | None, reads: list[str], writes: list[str],
                 shown: Callable[..., BaseModel]) -> FunctionBasedAction:  # pyright: ignore[reportExplicitAny]
    """A Burr action that validates the step's input (and answer), runs it, and
    writes its output back; the UI shows `shown`'s source as its code.
    (Burr hands the state to the parameter named `state` and runtime inputs,
    such as `answer`, by name.)"""

    def run_step(state: State[BaseModel]) -> tuple[dict[str, object], State[BaseModel]]:
        given = input_type.model_validate({field: state[field] for field in reads})  # pyright: ignore[reportAny]
        written = _written(call(given, None), writes)
        return written, state.update(**written)

    def run_pause_point(state: State[BaseModel], answer: BaseModel) -> tuple[dict[str, object], State[BaseModel]]:
        assert answer_type is not None
        given = input_type.model_validate({field: state[field] for field in reads})  # pyright: ignore[reportAny]
        written = _written(call(given, answer_type.model_validate(answer)), writes)
        return written, state.update(**written)

    run = run_step if answer_type is None else run_pause_point
    return FunctionBasedAction(run, reads, writes, originating_fn=shown)


def _written(produced: BaseModel, writes: list[str]) -> dict[str, object]:
    return {field: getattr(produced, field) for field in writes}  # pyright: ignore[reportAny]


def _check_decisions(transitions: Sequence[Transition]) -> None:
    for transition in transitions:
        siblings = [other for other in transitions if other.after is transition.after]
        if len(siblings) > 1 and any(other.when is None for other in siblings):
            raise TransitionError(f"{_name(transition.after)} has several transitions; each needs a `when`")


def _step_models(function: Callable[[InputT], OutputT]) -> tuple[type[InputT], type[OutputT]]:
    """The input and output models a step's signature names."""
    parameters, output_type = _signature_models(function)
    if len(parameters) != 1:
        raise TypeError(f"step {_name(function)} must take one pydantic model (its input)")
    return cast(type[InputT], parameters[0]), cast(type[OutputT], output_type)


def _pause_point_models(function: Callable[[InputT, AnswerT], OutputT]
                        ) -> tuple[type[InputT], type[AnswerT], type[OutputT]]:
    """The input, answer and output models a pause point's signature names."""
    parameters, output_type = _signature_models(function)
    if len(parameters) != 2:
        raise TypeError(f"pause point {function.__name__} must take two pydantic models (input, answer)")
    return cast(type[InputT], parameters[0]), cast(type[AnswerT], parameters[1]), cast(type[OutputT], output_type)


def _signature_models(function: Callable[..., object]) -> tuple[list[type[BaseModel]], type[BaseModel]]:  # pyright: ignore[reportExplicitAny]
    hints = cast(dict[str, object], get_type_hints(function))
    output_type = hints.pop("return", None)
    parameters = list(hints.values())
    if not all(_is_model(hint) for hint in [*parameters, output_type]):
        raise TypeError(f"{function.__name__}: every parameter and the return value must be pydantic models")
    return cast(list[type[BaseModel]], parameters), cast(type[BaseModel], output_type)


def _is_model(hint: object) -> bool:
    return isinstance(hint, type) and issubclass(hint, BaseModel)


def _name(function: Callable[..., object]) -> str:  # pyright: ignore[reportExplicitAny]
    return function.__name__
