# Running and extending the test suite

From the repository root, run:

```sh
python3 tests/run_tests.py
```

The runner discovers `tests/*_test.py`, `tests/*_test.cpp`, and `project/tests/*_test.gd`. It checks executable availability, imports the Godot project, compiles native tests with assertions enabled, and runs each test in its own process. Output streams to the terminal as the test executes. Each run has a fresh temporary directory containing logs, a registry snapshot, isolated save/settings directories, and `report.json`. The directory is printed at startup and completion and remains available for inspection.

Prerequisites are Python 3, a C++17 compiler compatible with the compiler flags used here, Godot compatible with this project, and a built GDExtension. If the extension is missing or outdated, build it first with `scons -j6 target=template_debug`. The runner does not rebuild the extension or overwrite block assets. Linux is the validated environment. Windows user-data isolation uses `APPDATA`/`LOCALAPPDATA`; compiler and renderer availability must be configured. Godot execution on macOS is currently rejected because its user-data isolation is not implemented.

## Selection and output

```sh
# Show discovered functional tests.
python3 tests/run_tests.py --list

# Select a suite or a substring of test names.
python3 tests/run_tests.py --suite godot
python3 tests/run_tests.py --suite native
python3 tests/run_tests.py --filter rarity

# Include the Godot and C++ benchmarks.
python3 tests/run_tests.py --include-benchmarks

# Supply sampling seeds or executable locations.
python3 tests/run_tests.py --seeds 73,2026,-17
python3 tests/run_tests.py --godot /path/to/godot --cxx /path/to/c++

# Retain artifacts in a chosen NEW directory.
python3 tests/run_tests.py --output /tmp/voxelgames-test-run
```

Benchmarks are separate from the default functional suite because their timing results serve a different purpose. `--include-benchmarks` runs both kinds through the same entry point. New matching test files are discovered automatically; helpers belong in a support directory and do not match the discovery pattern.

`inventory_disk_test.gd` is executed in write and read phases using separate processes but the same isolated directory. The next test receives a different directory. New tests requiring multiple processes should register their phases explicitly in the runner rather than relying on state from another test.

## Failures and renderer requirements

The terminal shows `PASS`, `FAIL`, `TIMEOUT`, or `SKIP`, followed by a summary. A nonzero process exit, a Godot `ERROR`/`SCRIPT ERROR`, or a reported nonzero failure count marks a test as failed. This catches scripts that log errors but exit with code zero. The default per-process timeout is 90 seconds and can be changed with `--timeout`; stalled processes are terminated, including their process group on Linux. The runner continues after individual test failures. Setup/import failures stop execution before the suite starts.

`voxel_ao_test.gd` inspects real mesh arrays and requires an active renderer. Its source declares `TEST_REQUIRES_RENDERER`. By default, the runner uses OpenGL Compatibility for that test when a display is available and runs other Godot tests headlessly. Without a display, it explicitly reports the AO test as skipped. A skip is never reported as a pass.

```sh
# Headless-only run; renderer tests appear as SKIP.
python3 tests/run_tests.py --render-tests off

# Require every selected test to execute; skipped tests make the run fail.
python3 tests/run_tests.py --require-all

# In Linux CI with Xvfb and OpenGL support installed:
xvfb-run -a python3 tests/run_tests.py --render-tests on --require-all
```

`--skip-import` is available for an already imported workspace. It should not replace importing a fresh checkout. Exit code 0 means all executed tests passed, with any skips listed explicitly; exit code 1 indicates failure/timeout or a skip under `--require-all`. Invalid command-line arguments return 2; interruption returns 130.

## Registry-driven tests

All Godot tests share [test_case.gd](../project/tests/support/test_case.gd), which provides assertions, world-loading helpers, cached registry reads, block lookup by name, biome lookup by name/kind, vegetation membership, rarity variants, and sampling seeds. Tests no longer inherit the concrete biome registry test.

- Runtime block IDs come from `block_registry.generated.json`, not fixed numeric IDs or JSON array positions. The source and generated ID mappings are checked before execution; regenerate assets and rebuild the extension if they disagree.
- Biome distribution iterates the configured entries and checks registered identities, allowed surface palettes, climate-transition bounds, disabled biomes, and determinism. Counts are logged for diagnosis without exact expected populations or fixed climate percentages.
- Every configured biome profile is exercised in an isolated generation fixture, including profiles too rare or narrow to appear in the broad scan.
- Rarity tests include the configured value, disabled/guaranteed endpoints, and higher values derived from the configured rarity. They check deterministic selection, nested regions, and non-increasing population instead of fixed counts such as 8,395 samples.
- Plains, snow, palms, freezing transitions, icon-cache repairs, inventory item identities, and underground block queries resolve relevant IDs or parameters from registry data.
- Native rarity and vegetation tests consume a snapshot exported from those same JSON registries. Species IDs, weights, height ranges, patch settings, flags, biome IDs, and rarity values follow the current data. Coast/freezing tests use registered sea level, offsets, and freezing boundary.

Controlled fixtures still use explicit values where they define the behavior under test: a flat world, an invalid range, a full inventory, disabled/guaranteed rarity, or a chunk boundary. These are intentional test inputs rather than assumptions about production tuning. Named feature tests still require their feature profiles, such as snow or palms; removing a feature is a coverage decision, not merely a numeric tuning change.

New Godot tests should extend `res://tests/support/test_case.gd`, implement `run()`, use `check(...)` or assertions, and terminate with `quit(1 if failures else 0)`. Use `biome_registry()`, `block_id(name)`, and profile fields for production values. Mutate deep copies and write fixtures only to `user://`, which the runner isolates. Keep all documentation and diagnostic messages in English.
