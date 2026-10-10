# Restore lighting tests and execute the Godot suite in CI

Suggested priority: P1. Type: bug / tests.

## Problem and evidence

`project/tests/day_night_test.gd:3` and `world_time_graphics_test.gd:19` load `res://scenes/directional_light_3d.gd`, but the script is in `res://scripts/directional_light_3d.gd`.

With Godot 4.7.2, the first fails during parsing and returns 1. The second fails during loading and the `set_hour` call, remaining running until interrupted. `.github/workflows/ci.yml` compiles the extension but does not execute Godot tests.

## Proposal

Correct the paths and create a runner that imports the project, executes tests with isolated data/configuration directories, enforces per-process timeouts, and aggregates results. Detect script/assertion errors as well as exit codes. Separate benchmarks from functional tests; include both phases of `inventory_disk_test.gd`. Run the functional suite in a Linux job with an engine and extension compatible in precision and version.

## Acceptance criteria

- [ ] Both lighting tests terminate without errors and return 0.
- [ ] One command runs the functional suite and reports passes, failures, and timeouts.
- [ ] A script error or stalled test produces a failure result.
- [ ] The runner does not use personal saves/settings.
- [ ] Pull requests execute the job and expose failure logs.

## Implementation progress

The lighting paths have been fixed, and `tests/run_tests.py` now provides automatic discovery, isolated data, live logs, timeout/error handling, disk-test phases, and optional benchmarks. See [Testing guide](../../testing.md). CI integration remains pending; this issue is not fully complete.
