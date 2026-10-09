"""The recorded routes lockstep replays, with how many game frames each covers
(docs/PROCESS.md §3, "Recorded routes")."""
from wf.repo import TRACES
from wf.types import FrameCount, Route, RouteName


def _route(name: str, length: int) -> Route:
    return Route(name=RouteName(name), trace=TRACES / f"{name}.trace", length=FrameCount(length))


ALL_ROUTES: tuple[Route, ...] = (
    _route("battles1", 20111),
    _route("battle", 20000),
    _route("menus2", 23042),
    _route("story1", 9700),
    _route("story2", 33546),
    _route("story3", 17576),
    _route("story4", 33703),
    _route("story5", 52712),
    _route("story6", 36237),
    _route("story7", 12601),
    _route("story8", 22494),
    _route("title_mash_a", 8000),
    _route("wander", 8000),
)


def route_named(name: RouteName) -> Route:
    for route in ALL_ROUTES:
        if route.name == name:
            return route
    raise ValueError(f"no recorded route named {name!r}")
