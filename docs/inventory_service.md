# Inventories: logical core and presentation

## Responsibilities

- `src/inventory_service.*`: a generic C++ singleton with registration by ID, capacity, `{id, amount}` slots, per-type limits, and revisions. It has no knowledge of nodes, grids, ItemView, blocks, the player, worlds, the mouse, recipes, or files. It uses Godot value types and signals to interface with GDScript and stores no node pointers.
- `inventory_session.gd`: the autoload that knows about the game. It registers item types, creates UUIDs, defines the hotbar/storage/creative catalog, produces ItemViews, and saves/loads world records through SaveService.
- `inventory_drag_controller.gd`: associates grids with UUIDs using weak references, tracks dragging, and calls the core. It observes notifications and rebuilds representations.
- `GridInventory`: presents ItemView snapshots and reports GUI events. Columns, icons, counters, tooltips, and previews are visual. It does not query the service or execute inventory rules.
- `inventory_manager.gd`: the autoload for mouse/F1 control and hotbar selection. Selection queries logical data independently of presentation.

## Registration and binding

```gdscript
var uuid := InventorySession.create_uuid()
InventorySession.register_inventory(uuid, 27)
drag_controller.bind_grid(grid, uuid)
```

Registration in the core is purely logical. The GDScript adapter associates persistent records with the current world. Two grids can represent the same UUID with different layouts. Closing or destroying a grid does not remove its registration or items.

For records without game persistence, use `InventoryService.register_inventory(id, capacity, copy_source)` directly. The core accepts a stable caller-provided ID and types registered with `register_item_type(item_id, stack_limit)`. It does not require UUID generation or a universal limit of 99; that configuration belongs to the game.

## Drag flow

1. The grid emits `drag_started(index)` with a stable linear index.
2. The controller queries the associated UUID and stores the source, amount, and revision. It gives the GUI only a transient token. The source representation is hidden; its data remains in the core and save snapshots.
3. Godot's native drag-and-drop forwarding identifies the grid/cell under the mouse. The grid emits `drop_hovered(payload, index)` and `drop_requested(payload, index)`.
4. The controller checks the token, visibility, and interaction state of the views. It requests `transfer(source, from, target, to, amount, revision)` from the core.
5. The core revalidates indices, amount, revision, and compatibility. It commits all changes before emitting `inventory_changed(uuid, index)`. The `can_transfer` preview never replaces this final validation.
6. The controller reads committed data and updates all bound grids. Index -1 indicates a multiple-slot update.

Stacking can move only what fits, leaving the remainder at the source. Swapping different types requires the full stack. Sources configured for copying supply inventories without consuming their contents and reject incoming items. `add_items` is an all-or-nothing insertion: insufficient capacity changes no slots.

An invalid drop, full destination, F1, closing the source, or ending a gesture without a destination cancels transient state. Refreshing restores the ID, amount, name, category, icon, and tooltip from current data. A source mutation during dragging invalidates the previous revision. The controller consumes the token and prevents reuse after completion/cancellation.

The core executes these operations synchronously on the main thread. Atomicity here means validation precedes mutation and observers receive signals only after both inventories have been updated; the API does not provide concurrent worker access.

## Columns and capacity

Windows calculate columns from usable width, with a minimum of two and a preserved slot size. Updates are grouped per frame. Changing columns does not change capacity, order, or identity: slot 8 moves from (8, 0) to (0, 4) with two columns. A capacity of 27 does not create extra slots in the last row. The hotbar retains nine columns.

## Persistence and crafting removal

VoxelAPI emits `world_opened(id)` and `world_saving(id)`. InventorySession connects these signals and loads/saves even without a player or UI. The menu preview does not open or save persistent inventories.

The world's `inventories` section stores version 3, role IDs, selection, and UUID records with capacity and slots. IDs are retained on subsequent loads. The creative catalog is rebuilt without being persisted as a consumable inventory. Records remain in memory during the session.

The crafting interface and rules have been removed. Older saves with ingredients in craft slots are migrated to a logical recovery record. Items return to storage/hotbar as soon as space becomes available; while everything is full, they remain saved in that record. An old craft UUID is preserved as the recovery UUID. New worlds do not create a craft inventory.

## Verification

- `inventory_service_test.gd`: core without UI, configurable limits, snapshots, partial transfers, swaps, stale revisions, failures without mutation, and complete data in the first notification.
- `inventory_controller_test.gd`: rejected drops and ItemView resynchronization, hidden destinations, mutation invalidation, consumed tokens, multiple views, and independent lifetimes.
- `inventory_ui_test.gd` and `inventory_mouse_test.gd`: real GUI event forwarding, F1, mouse, icons, counters, scrolling, and windows.
- `inventory_reflow_test.gd`: column changes during dragging, fixed capacity, order, and model retention after UI destruction.
- `inventory_session_test.gd`: crafting migration with a full inventory, later recovery, fixed UUIDs, and world isolation.
- `inventory_disk_test.gd`: writing and reading in separate processes, with an additional inventory without UI.
