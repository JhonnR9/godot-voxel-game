extends "res://tests/support/test_case.gd"

var session: Node:
	get: return root.get_node("InventorySession")

func run() -> void:
	if "--read" in OS.get_cmdline_user_args():
		var fixture: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("user://inventory_fixture.json"))
		session.load_world(int(fixture.world))
		var ids: Dictionary = session.get_player_ids()
		check(ids == fixture.ids, "New process recovers the same UUIDs from disk")
		check(InventoryService.get_capacity(ids.storage) == 27, "New process recovers real capacity")
		check(InventoryService.get_stack(ids.storage, 26).amount == 37, "New process recovers items without UI")
		check(InventoryService.get_capacity(fixture.chest) == 6, "New process registers non-player inventory from disk")
		check(InventoryService.get_stack(fixture.chest, 5).amount == 11, "Other persistent UUID also recovers its items")
	else:
		var world: int = SaveService.create_world(42, "Cross-process inventory test")
		session.load_world(world)
		var ids: Dictionary = session.get_player_ids()
		InventoryService.set_stack(ids.storage, 26, block_id("grass"), 37)
		var chest: String = session.create_uuid()
		session.register_inventory(chest, 6)
		InventoryService.set_stack(chest, 5, block_id("grass"), 11)
		check(session.save_world(world), "Service saves without player or grids")
		var file := FileAccess.open("user://inventory_fixture.json", FileAccess.WRITE)
		file.store_string(JSON.stringify({"world": str(world), "ids": ids, "chest": chest}))
		file.close()
	print("Inventory disk tests: ", failures, " failures.")
	quit(1 if failures else 0)
