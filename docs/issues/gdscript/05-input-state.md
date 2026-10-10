# Synchronize mouse state between pause, inventory, and loading

Suggested priority: P1. Type: bug / architecture.

## Problem and evidence

`pause.gd:26` captures the mouse directly on resume. `inventory_manager.gd:26` maintains separate state, and `player.gd:143` prevents movement while that state indicates an unlocked mouse. Player, LoadingScreen, and SettingsPanel also write to `Input.mouse_mode`.

Sequence to validate in a window: F1 to unlock, close inventory windows, Esc to pause, then resume. The code captures the cursor but leaves `InventoryManager.mouse_unlocked = true`. Headless reproduction confirmed that the manager state remains true; actual cursor capture was not validated with that backend.

## Proposal

Define one owner of input state, with explicit transitions for gameplay, GUI, pause, and loading. When pausing, save the previous state and restore it or enter gameplay with synchronized state. Cancel drag gestures when suspending interaction. Use InputMap actions for shortcuts still referencing keys directly.

## Acceptance criteria

- [ ] After resuming, manager state, grid interaction, and cursor behavior follow the same policy.
- [ ] F1 → Esc → resume works with and without open windows.
- [ ] Finishing loading and leaving settings do not incorrectly capture the cursor over another UI.
- [ ] An active drag is canceled without item loss/duplication.
- [ ] Tests cover logical transitions, and a windowed check covers mouse capture.
