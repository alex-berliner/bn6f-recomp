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
decisions are plain functions of the whole run state.
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
    """After `after`, go to `go_to` if `when` holds. Transitions are tried in
    order; one without `when` is the fallback."""
    after: AnyStep
    go_to: AnyStep
    when: Callable[[Never], bool] | None = None


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
        """`tracked` runs show up in the Burr UI; an untracked one is only for drawing."""
        steps = {*(transition.after for transition in transitions), *(transition.go_to for transition in transitions)}
        builder = (
            ApplicationBuilder()
            .with_typing(PydanticTypingSystem(self.state_type))
            .with_state(start)  # pyright: ignore[reportUnknownMemberType]
            .with_actions(**{_name(step): self._burr_actions[step] for step in steps})  # pyright: ignore[reportUnknownMemberType]
            .with_transitions(*(self._burr_transition(transition) for transition in transitions))
            .with_entrypoint(_name(first))
            .with_identifiers(app_id=run_id)
        )
        if tracked:
            builder = builder.with_tracker("local", project=self.project)
        return TypedApplication(self.state_type, builder.build())

    def _state_fields(self, function: AnyStep, model: type[BaseModel]) -> list[str]:
        fields = list(model.model_fields)
        unknown = [field for field in fields if field not in self.state_type.model_fields]
        if unknown:
            raise TypeError(f"step {_name(function)}: {model.__name__} has fields {unknown} "
                            f"that {self.state_type.__name__} does not")
        return fields

    def _burr_transition(self, transition: Transition) -> tuple[str, str, Condition]:
        return (_name(transition.after), _name(transition.go_to),
                default if transition.when is None else self._condition(cast(Callable[[StateT], bool], transition.when)))

    def _condition(self, test: Callable[[StateT], bool]) -> Condition:
        state_type = self.state_type

        def resolve(state: State[BaseModel]) -> bool:
            return test(model_from_state(model=state_type, state=state))

        return Condition(keys=list(state_type.model_fields), resolver=resolve, name=_name(test))


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
