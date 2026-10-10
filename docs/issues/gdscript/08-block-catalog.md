# Centralize block metadata and catalog validation in GDScript

Suggested priority: P2. Type: architecture / maintenance.

## Problem and evidence

InventorySession, InventoryUI, and BlockIconCache read the same generated JSON separately. `inventory_session.gd:15` assigns the parse directly to Dictionary without the validation found in the other readers. `inventory_ui.gd:201–215` searches blocks linearly; `player.gd:230–238` associates footstep materials with literal IDs.

Adding or changing content requires synchronizing consumers and makes missing/invalid registry diagnosis harder.

## Proposal

Create a shared catalog with schema/version validation, ID/name lookup, and an ordered list. Consumers use the same snapshot; the icon cache remains responsible for images. Move sound classification into metadata or resolve names through the catalog. Define one failure message and avoid continuing with a partially initialized inventory.

## Acceptance criteria

- [ ] Session/UI/cache receive consistent metadata without duplicate parsers.
- [ ] Missing files, invalid JSON, and incompatible fields have clear diagnostics and controlled failure behavior.
- [ ] Material IDs are not scattered as literals in the character script.
- [ ] New blocks appear in the catalog and receive views/icons without editing UI lists.
- [ ] Existing inventory/cache tests still pass; fixtures cover invalid input.
