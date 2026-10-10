# capture

Capture the original game's own calls of some functions (the inputs that
difftest and the CPU oracle check conversions on): give each function a
recorded route that executes it (as few routes as possible), replay those
routes on the original in parallel, save the first calls of each function.
The calls go into the snapshot store, build-snapshots/FUNCTION, next to what
earlier captures saved there: one file per call, named by its route, and a
replay is deterministic, so capturing on a route again writes the same files.

![capture](capture.png)

Runs are tracked in the Burr UI under project `recomp-capture`.

## Steps

### `assign_routes` (step)

Give each function a route that runs it: repeatedly take the route that runs the most still unassigned.

- reads: `functions: tuple[FunctionName, ...]`, `candidate_routes: tuple[Route, ...]`, `run_dir: Path`
- produces: `assignments: tuple[RouteAssignment, ...]`
- then: `capture_calls_on_routes` when `some_route_runs_them`; `write_report` when `no_route_runs_them`

### `capture_calls_on_routes` (step)

Replay each assigned route on the original game (in parallel) and save the first calls of its functions.

- reads: `snapshots: SnapshotDir`, `assignments: tuple[RouteAssignment, ...]`, `calls_per_function: int`, `run_dir: Path`
- produces: `captured: tuple[CapturedCalls, ...]`
- then: `write_report`

### `write_report` (step)

Say where the calls are and which functions were never called.

- reads: `functions: tuple[FunctionName, ...]`, `snapshots: SnapshotDir`, `assignments: tuple[RouteAssignment, ...]`, `captured: tuple[CapturedCalls, ...]`
- produces: `report: CaptureReport`
- then: the run ends
