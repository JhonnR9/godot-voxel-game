# GDScript layer review

Review of the local checkout on October 9, 2026, using Godot 4.7.2. Runtime scripts, asset-generation tools, related scenes, and relevant tests were read. The C++ extension was consulted to check inventory and persistence contracts; it was not subject to a complete review.

## What is already implemented

| Area | Current implementation | Assessment |
| --- | --- | --- |
| Character | Movement, jumping, flight, noclip, camera, block interaction, footsteps, and underwater effects | Broad functionality; `player.gd` combines movement, interaction, audio, and UI construction |
| Worlds and menus | Creation, selection, deletion, loading, pause, and procedural menu preview | Complete flow, with defects in seed input and list refresh |
| Inventory | Hotbar, creative catalog, movable windows, reflow, drag-and-drop, and selection | Good separation between presentation, controller, and authoritative C++ service |
| Inventory persistence | UUIDs, world isolation, migration, and recovery of old crafting items | Mature area with migration and item-conservation tests |
| Settings | Audio, resolution, fullscreen, AA, FSR, SSAO, distance, and fog | Separate services; audio already avoids writing on every slider movement |
| Lighting and loading | Day/night cycle and initial progress | Tests exist, but two lighting tests use an obsolete path |
| Content tools | Block Registry plugin, generation of C++ header, JSON, and texture array; icon cache | Good content-editing foundation; file publication and ID stability need review |
| Verification | Godot tests for inventory, menu, graphics, and generation; compilation CI | Useful coverage, but no unified runner or execution of these tests in CI |

## What needs review first

1. **Broken lighting tests:** `day_night_test.gd:3` and `world_time_graphics_test.gd:19` reference `res://scenes/directional_light_3d.gd`; the file is in `res://scripts/`.
2. **Seed ignored without Enter:** the scene connects only `text_submitted` for the seed. Typing and clicking Create retains the previous random seed.
3. **List after deleting the last world:** the early return in `world_selector.gd:16` occurs before removing old rows.
4. **Missing ambient audio:** the ocean MP3 is cast to `AudioStreamWAV`; the cast returns `null` and playback never starts.
5. **Distributed mouse state:** pause, loading, character, settings, and InventoryManager change the mouse. Pause captures it on return without updating `InventoryManager.mouse_unlocked`. The logical inconsistency was observed; its visual effect needs validation with rendering.
6. **Reusable block IDs:** removing the highest ID and adding another allows reuse even though saves persist numeric IDs. This compatibility risk was identified through code inspection without changing the real registry.
7. **Partially published generation:** the plugin writes the source before validation and the generator publishes outputs individually; failures can leave source and assets in different states.

## Changes to make development more comfortable

- Create one command to check dependencies, import the project, and run tests with isolated data, timeouts, and a report. Execute it in CI.
- Add editor tasks for extension builds, game execution, asset generation, and tests. Pin lint-tool versions and offer formatting for selected files only.
- Centralize block catalog reading/validation. InventorySession, InventoryUI, and BlockIconCache currently read it separately; the character still uses numeric IDs to choose sounds.
- Gradually extract audio/effects and block interaction from `player.gd`, keeping authoritative inventory logic in C++. Do this alongside fixes, preserving behavior and contracts.
- Use exported references or unique scene names at fragile points such as pause and settings, and type UI interfaces. Current long paths make hierarchy changes cumbersome.
- After fixes, add a debug panel with seed, coordinates, biome, chunk queue, finalization time, and player mode, using `get_pipeline_stats()`. The current HUD shows FPS/VSync but does not help diagnose the pipeline.
- Define game-mode behavior: creation offers Survival/Creative, but the selection is unused. Placing blocks also does not consume quantities, as documented in the README.

This review does not justify rewriting the entire layer in C++ or replacing inventory architecture. The most immediate gains come from fixing menu/input flows and shortening the validation cycle.

## Checks performed

Data and configuration were directed to separate directories under `/tmp/voxelgames-gd-review/`; personal saves were not used.

| Check | Result |
| --- | --- |
| `settings_navigation_test.gd` | Passed |
| `inventory_controller_test.gd` | Passed, 0 failures |
| `inventory_session_test.gd` | Passed, 0 failures |
| `inventory_mouse_test.gd` | Passed, 0 failures |
| `block_icon_cache_test.gd` | Passed |
| `day_night_test.gd` | Parse failure due to preload of a nonexistent file, exit 1 |
| `world_time_graphics_test.gd` | Failed to load the same file and call `set_hour`; process interrupted after failing to terminate |
| Seed reproduction | Field contained `12345`, internal variable retained a random seed |
| List reproduction | Two children before and after deleting the only world; expected only the placeholder |
| Runtime inspection of audio resource | `AudioStreamMP3`; cast to `AudioStreamWAV` returned `null` |
| Pause after unlocking the mouse | `InventoryManager.is_mouse_unlocked()` remained `true` after pausing/resuming |

Headless tests do not establish appearance, audible sound, GPU performance, or actual cursor capture. The entire suite was not run. The pause check confirms logical state; capture must be checked in a windowed game session.

## Prepared issues

The ten complete drafts are in [issues/gdscript/README.md](issues/gdscript/README.md), with priority, evidence, a proposal, and acceptance criteria. They are local files and have not been published on GitHub. Priorities suggest execution order rather than labels already present in the repository.

Issue templates also need an update: `.github/ISSUE_TEMPLATE/bug_report.yml` still describes the godot-cpp template and links to another repository; `config.yml` disables blank issues. This cleanup can accompany development-workflow improvements.
