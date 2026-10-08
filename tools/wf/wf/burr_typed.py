"""The one place that touches Burr's partly untyped API.

A step is a plain function from its own input model to its own output model;
that signature is its whole contract:

    class LockstepInput(BaseModel):
        build: BuiltRom
        routes: tuple[Route, ...]

    class LockstepOutput(BaseModel):
        locksteps: tuple[LockstepResult, ...]

    @workflow.step
    def lockstep_routes(given: LockstepInput) -> LockstepOutput: ...

The step reads the run-state fields named like its input model's fields and
writes those named like its output model's. Pydantic validates the input
before the step runs (a BuildFailure cannot reach a step that needs a
BuiltRom) and the output after. The Burr UI shows the output as the step's
Result, and the step's own source in its Code tab.

Steps are wired with `Transition`s that name the step functions themselves;
decisions are plain functions of the whole run state. A step with one
transition may leave out `when`; a step with several must give each a `when`,
and after the step runs exactly one of them must hold, or the run stops with
a TransitionError naming the step and the decisions involved.
"""
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Generic, Never, TypeVar, cast, get_type_hints

from burr.core import Application, ApplicationBuilder, Condition, State, default, serde
from burr.core.action import FunctionBasedAction
from burr.integrations.pydantic import PydanticTypingSystem, model_from_state  # pyright: ignore[reportUnknownVariableType]
from pydantic import BaseModel

StateT = TypeVar("StateT", bound=BaseModel)
InputT = TypeVar("InputT", bound=BaseModel)
OutputT = TypeVar("OutputT", bound=BaseModel)

AnyStep = Callable[[Never], BaseModel]  # a step with any input and output model


@serde.serialize.register(tuple)
def _serialize_tuple(value: tuple[object, ...], **options: object) -> list[object]:
    """Burr's tracker knows lists but writes tuples as their repr; our state
    uses tuples, so show them item by item in the UI like lists."""
    return [serde.serialize(item, **options) for item in value]  # pyright: ignore[reportUnknownMemberType]


@dataclass(frozen=True)
class Transition:
    """After `after`, go to `go_to` when `when` holds (always, if `when` is left out)."""
    after: AnyStep
    go_to: AnyStep
    when: Callable[[Never], bool] | None = None


class TransitionError(Exception):
    """The transitions out of a step are missing a decision, or did not pick exactly one way on."""


class Workflow(Generic[StateT]):
    def __init__(self, state_type: type[StateT], project: str) -> None:
        self.state_type = state_type
        self.project = project
        self._burr_actions: dict[AnyStep, FunctionBasedAction] = {}

    def step(self, function: Callable[[InputT], OutputT]) -> Callable[[InputT], OutputT]:
        """Register a step; its input and output models say what it reads and writes."""
        input_type, output_type = _step_models(function)
        reads = self._state_fields(function, input_type)
        writes = self._state_fields(function, output_type)

        def run(state: State[BaseModel]) -> tuple[dict[str, object], State[BaseModel]]:
            given = input_type.model_validate({field: state[field] for field in reads})  # pyright: ignore[reportAny]
            produced = function(given)
            written: dict[str, object] = {field: getattr(produced, field) for field in writes}  # pyright: ignore[reportAny]
            return written, state.update(**written)

        self._burr_actions[function] = FunctionBasedAction(run, reads, writes, originating_fn=function)
        return function

    def application(self, start: StateT, run_id: str, first: AnyStep,
                    transitions: Sequence[Transition], tracked: bool = True) -> "TypedApplication[StateT]":
        """`tracked` runs show up in the Burr UI; an untracked one is only for drawing or tests."""
        steps = {*(transition.after for transition in transitions), *(transition.go_to for transition in transitions)}
        builder = (
            ApplicationBuilder()
            .with_typing(PydanticTypingSystem(self.state_type))
            .with_state(start)  # pyright: ignore[reportUnknownMemberType]
            .with_actions(**{_name(step): self._burr_actions[step] for step in steps})  # pyright: ignore[reportUnknownMemberType]
            .with_transitions(*self._burr_transitions(transitions))
            .with_entrypoint(_name(first))
            .with_identifiers(app_id=run_id)
        )
        if tracked:
            builder = builder.with_tracker("local", project=self.project)
        return TypedApplication(self.state_type, builder.build())

    def _burr_transitions(self, transitions: Sequence[Transition]) -> list[tuple[str, str, Condition]]:
        burr_transitions: list[tuple[str, str, Condition]] = []
        for transition in transitions:
            siblings = [other for other in transitions if other.after is transition.after]
            if len(siblings) > 1 and any(other.when is None for other in siblings):
                raise TransitionError(f"{_name(transition.after)} has several transitions; each needs a `when`")
            if transition.when is None:
                condition = default
            else:
                condition = self._condition(transition, siblings)
            burr_transitions.append((_name(transition.after), _name(transition.go_to), condition))
        return burr_transitions

    def _condition(self, transition: Transition, siblings: list[Transition]) -> Condition:
        """Holds when this transition's decision does, after checking that exactly one sibling's does."""
        state_type = self.state_type
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

    def _state_fields(self, function: AnyStep, model: type[BaseModel]) -> list[str]:
        fields = list(model.model_fields)
        unknown = [field for field in fields if field not in self.state_type.model_fields]
        if unknown:
            raise TypeError(f"step {_name(function)}: {model.__name__} has fields {unknown} "
                            f"that {self.state_type.__name__} does not")
        return fields


class TypedApplication(Generic[StateT]):
    def __init__(self, state_type: type[StateT], application: Application[StateT]) -> None:
        self.state_type = state_type
        self.application = application

    def run_until_after(self, last: AnyStep) -> StateT:
        _, _, final_state = self.application.run(halt_after=[_name(last)])  # pyright: ignore[reportUnknownMemberType, reportUnknownVariableType]
        return model_from_state(model=self.state_type, state=final_state)

    def draw(self, path_without_extension: Path) -> None:
        self.application.visualize(output_file_path=str(path_without_extension), format="png",  # pyright: ignore[reportUnknownMemberType]
                                   include_conditions=True, include_state=True)


def _step_models(function: Callable[[InputT], OutputT]) -> tuple[type[InputT], type[OutputT]]:
    """The input and output models a step's signature names."""
    hints = cast(dict[str, object], get_type_hints(function))
    output_type = hints.pop("return", None)
    if len(hints) != 1 or not _is_model(output_type):
        raise TypeError(f"step {_name(function)} must take one pydantic model and return one")
    input_type = next(iter(hints.values()))
    if not _is_model(input_type):
        raise TypeError(f"step {_name(function)} must take one pydantic model and return one")
    return cast(type[InputT], input_type), cast(type[OutputT], output_type)


def _is_model(hint: object) -> bool:
    return isinstance(hint, type) and issubclass(hint, BaseModel)


def _name(function: Callable[[Never], object]) -> str:
    return function.__name__
