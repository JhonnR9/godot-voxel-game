extends "res://tests/support/test_case.gd"

var session: Node:
	get: return root.get_node("InventorySession")

func run() -> void:
	var world: int = SaveService.create_world(42, "Legacy inventory migration")
	var full_hotbar: Array = []
	var full_storage: Array = []
	for index in range(9): full_hotbar.append({"id": block_id("grass"), "amount": 99})
	for index in range(27): full_storage.append({"id": block_id("grass"), "amount": 99})
	SaveService.save_world_section(world, "player", {"position": [1, 2, 3], "inventory": {"hotbar": full_hotbar, "storage": full_storage, "craft": [{"id": block_id("dirt"), "amount": 7}], "selected_slot": 3}})
	session.load_world(world)
	var ids: Dictionary = session.get_player_ids()
	check(ids.has("recovery") and not ids.has("craft"), "Legacy craft becomes logical recovery storage")
	check(InventoryService.get_stack(ids.recovery, 0) == {"id": block_id("dirt"), "amount": 7}, "Full inventory preserves former crafting ingredients")
	check(session.selected_slot == 3, "Migration restores selected hotbar slot")
	var position: Array = SaveService.load_world_section(world, "player").position
	check(Vector3(position[0], position[1], position[2]) == Vector3(1, 2, 3), "Migration preserves player fields")
	check(session.save_world(world), "Generic records save without any UI")
	session.load_world(world)
	check(session.get_player_ids() == ids, "Migrated UUIDs remain fixed after reload")
	check(InventoryService.get_stack(ids.recovery, 0).amount == 7, "Reload does not duplicate or lose recovery items")
	InventoryService.set_stack(ids.storage, 5, 0, 0)
	await process_frame
	check(InventoryService.get_stack(ids.storage, 5) == {"id": block_id("dirt"), "amount": 7} and InventoryService.get_stack(ids.recovery, 0).is_empty(), "Freed slot automatically receives legacy ingredients")
	check(session.save_world(world), "Recovered state persists")
	session.load_world(world)
	check(InventoryService.get_stack(ids.storage, 5).amount == 7 and InventoryService.get_stack(ids.recovery, 0).is_empty(), "Reload cannot recover old ingredients twice")
	# Already UUID-based legacy saves preserve their crafting UUID as recovery.
	var other: int = SaveService.create_world(43, "UUID craft migration")
	var old_ids := {"hotbar": session.create_uuid(), "storage": session.create_uuid(), "craft": session.create_uuid()}
	var records := {}
	records[old_ids.hotbar] = {"capacity": 9, "slots": full_hotbar}
	records[old_ids.storage] = {"capacity": 27, "slots": full_storage}
	records[old_ids.craft] = {"capacity": 4, "slots": [{"id": block_id("dirt"), "amount": 13}]}
	SaveService.save_world_section(other, "inventories", {"version": 2, "ids": old_ids, "records": records})
	session.load_world(other)
	var other_ids: Dictionary = session.get_player_ids()
	check(other_ids.hotbar == old_ids.hotbar and other_ids.recovery == old_ids.craft, "Existing persistent UUIDs survive craft removal")
	check(InventoryService.get_stack(other_ids.recovery, 0).amount == 13, "UUID record restores crafting contents independently of UI")
	session.load_world(world)
	check(session.get_player_ids() == ids and InventoryService.get_stack(ids.storage, 5).amount == 7, "Switching worlds preserves independent inventories")
	var fresh: int = SaveService.create_world(44, "Empty inventory world")
	session.load_world(fresh)
	var fresh_ids: Dictionary = session.get_player_ids()
	check(not fresh_ids.has("recovery") and not fresh_ids.has("craft"), "New world has no crafting inventory")
	check(InventoryService.get_stack(fresh_ids.hotbar, 0).is_empty(), "New world never inherits old items")
	# Real VoxelAPI lifecycle loads/saves the logical session without a player or grid.
	var api := VoxelAPI.new()
	root.add_child(api)
	api.set_render_settings({"render_distance": 4, "vertical_render_distance": 2})
	api.set_focus_position(Vector3(8, 48, 8))
	api.start_world(world)
	check(session.world_id == world and session.get_player_ids() == ids, "World-open signal loads inventory without UI")
	InventoryService.set_stack(ids.hotbar, 0, block_id("dirt"), 31)
	api.save_world()
	var persisted: Dictionary = SaveService.load_world_section(world, "inventories")
	check(int(persisted.records[ids.hotbar].slots[0].amount) == 31, "World-save signal persists inventory without player")
	api.free()
	print("Inventory session tests: ", failures, " failures.")
	quit(1 if failures else 0)
