# Add editor tasks and reproducible checks for GDScript development

Suggested priority: P2. Type: development.

## Problem and evidence

The README has useful manual commands, but `.vscode` contains only settings and C++/Python recommendations. There are no tasks for the GDScript/build/test cycle or lint configuration. Scripts vary in typing and formatting; UI depends on long paths such as those in `settings_panel.gd`.

Issue templates still reference godot-cpp-template and disable blank issues.

## Proposal

Add reusable commands and tasks for debug builds, importing, running, asset generation, and the GD-01 test runner. Check engine/library compatibility and explain that recompiling C++ requires restarting Godot (`reloadable = false`). Configure a formatter/linter with pinned versions and gradual adoption, plus a GDScript extension recommendation. Update templates for this game and include an enhancement template.

## Acceptance criteria

- [ ] A fresh checkout has instructions and commands to prepare, run, and check the project.
- [ ] Tasks reuse scripts/commands that work outside VS Code.
- [ ] Tests use isolated data, and prerequisite failures have actionable messages.
- [ ] Lint/formatting versions and commands are documented without reformatting the whole repository in this delivery.
- [ ] Bug/enhancement templates reference this project.
