"""The one place that touches Burr's partly untyped API.

A graph declares its steps and decisions as plain typed functions over its
pydantic state, and wires them with `Transition`s that name the functions
themselves:

    workflow = Workflow[VerifyField, VerifyState](VerifyState, project="recomp-verify")

    @workflow.step(reads=["run_dir"], writes=["build"])
    def build(state: VerifyState) -> VerifyState: ...

    Transition(after=build, go_to=lockstep_routes, when=build_succeeded)

`reads`/`writes` take the graph's Literal of state field names, so a typo is a
type error; Burr also checks them against the model when the module loads.
"""
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Generic, TypeVar, cast

from burr.core import Application, ApplicationBuilder, Condition, State, default
from burr.core.action import action
from burr.integrations.pydantic import PydanticTypingSystem, model_from_state  # pyright: ignore[reportUnknownVariableType]
from pydantic import BaseModel

StateT = TypeVar("StateT", bound=BaseModel)
FieldT = TypeVar("FieldT", bound=str)


@dataclass(frozen=True)
class Transition(Generic[StateT]):
    """After `after`, go to `go_to` if `when` holds. Transitions are tried in
    order; one without `when` is the fallback."""
    after: Callable[[StateT], StateT]
    go_to: Callable[[StateT], StateT]
    when: Callable[[StateT], bool] | None = None


class Workflow(Generic[FieldT, StateT]):
    def __init__(self, state_type: type[StateT], project: str) -> None:
        self.state_type = state_type
        self.project = project

    def step(self, reads: Sequence[FieldT], writes: Sequence[FieldT]
             ) -> Callable[[Callable[[StateT], StateT]], Callable[[StateT], StateT]]:
        """A step reads some state fields and sets others (assigns them and returns the state)."""
        register_with_burr = action.pydantic(reads=list(reads), writes=list(writes))  # pyright: ignore[reportUnknownMemberType, reportUnknownVariableType]
        return cast(Callable[[Callable[[StateT], StateT]], Callable[[StateT], StateT]], register_with_burr)

    def application(self, start: StateT, run_id: str, first: Callable[[StateT], StateT],
                    transitions: Sequence[Transition[StateT]]) -> "TypedApplication[StateT]":
        steps = {transition.after.__name__: transition.after for transition in transitions}
        steps |= {transition.go_to.__name__: transition.go_to for transition in transitions}
        builder = (
            ApplicationBuilder()
            .with_typing(PydanticTypingSystem(self.state_type))
            .with_state(start)  # pyright: ignore[reportUnknownMemberType]
            .with_actions(**steps)  # pyright: ignore[reportUnknownMemberType]
            .with_transitions(*(self._burr_transition(transition) for transition in transitions))
            .with_entrypoint(first.__name__)
            .with_identifiers(app_id=run_id)
            .with_tracker("local", project=self.project)
        )
        return TypedApplication(self.state_type, builder.build())

    def _burr_transition(self, transition: Transition[StateT]) -> tuple[str, str, Condition]:
        return (transition.after.__name__, transition.go_to.__name__,
                default if transition.when is None else self._condition(transition.when))

    def _condition(self, test: Callable[[StateT], bool]) -> Condition:
        state_type = self.state_type

        def resolve(state: State[BaseModel]) -> bool:
            return test(model_from_state(model=state_type, state=state))

        return Condition(keys=list(state_type.model_fields), resolver=resolve, name=test.__name__)


class TypedApplication(Generic[StateT]):
    def __init__(self, state_type: type[StateT], application: Application[StateT]) -> None:
        self.state_type = state_type
        self.application = application

    def run_until_after(self, last: Callable[[StateT], StateT]) -> StateT:
        _, _, final_state = self.application.run(halt_after=[last.__name__])  # pyright: ignore[reportUnknownMemberType, reportUnknownVariableType]
        return model_from_state(model=self.state_type, state=final_state)

    def draw(self, path_without_extension: Path) -> None:
        self.application.visualize(output_file_path=str(path_without_extension), format="png",  # pyright: ignore[reportUnknownMemberType]
                                   include_conditions=True, include_state=True)
