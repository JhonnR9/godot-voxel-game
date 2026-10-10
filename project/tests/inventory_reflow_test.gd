extends "res://tests/support/test_case.gd"

var manager: Node:
	get: return root.get_node("InventoryManager")

func settle() -> void:
	for frame in range(5): await process_frame

func run() -> void:
	root.size = Vector2i(1280, 720)
	root.content_scale_size = Vector2i.ZERO
	var ui = load("res://scenes/inventory_ui.tscn").instantiate()
	var player := Node.new()
	root.add_child(player)
	root.add_child(ui)
	manager.setup(player, ui)
	manager.open_inventory()
	ui.toggle_creative_window()
	await settle()
	var item = ui._make_block_item(ui._block_by_id(1))
	item.set_item_amount(17)
	InventoryService.set_stack(ui.inventory_grid.get_inventory_id(), 8, block_id("grass"), 17)
	var before: Dictionary = ui.serialize_inventory()
	var creative: Array = InventoryService.snapshot(ui.creative_grid.get_inventory_id())
	ui.inventory_panel.size = Vector2(170, 320)
	ui.creative_panel.size = Vector2(160, 320)
	await settle()
	check(ui.inventory_grid.get_columns() == 2 and ui.creative_grid.get_columns() == 2, "Narrow windows reflow to two columns")
	check(ui.inventory_grid.get_item_at(Vector2i(0, 4)).get_item_amount() == 17, "Items move visually with stable slot identity")
	check(ui.inventory_grid.get_child_count() == 27, "Last row never adds real slots")
	check(ui.serialize_inventory() == before, "Narrow layout never changes saved inventory")
	check(ui.hotbar_grid.get_columns() == 9, "Hotbar layout remains fixed")
	if DisplayServer.get_name() != "headless":
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png("/tmp/inventory_narrow.png")
	ui.inventory_grid.drag_started.emit(8)
	var payload := {"inventory_drag": ui.drag_controller.active.token}
	ui.inventory_panel.size = Vector2(740, 360)
	ui.creative_panel.size = Vector2(680, 360)
	await settle()
	check(ui.inventory_grid.get_columns() > 2 and ui.creative_grid.get_columns() > 2, "Widening windows adds columns in real time")
	check(InventoryService.snapshot(ui.creative_grid.get_inventory_id()) == creative, "Creative catalog keeps original order")
	check(ui.serialize_inventory() == before, "Reflow during drag preserves all saved items")
	ui.hotbar_grid.drop_requested.emit(payload, 0)
	check(InventoryService.get_stack(ui.inventory_ids.storage, 8).is_empty(), "Drag uses stable index across column changes")
	check(ui.hotbar_grid.get_item_at(Vector2i.ZERO).get_item_amount() == 17, "Reflow transfer loses no items")
	if DisplayServer.get_name() != "headless":
		for frame in range(30): await process_frame
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png("/tmp/inventory_wide.png")
	var uuid: String = ui.inventory_grid.get_inventory_id()
	ui.inventory_panel.hide()
	check(InventoryService.has_inventory(uuid), "Closing window keeps registration")
	ui.free()
	player.free()
	check(InventoryService.has_inventory(uuid), "Destroying view removes no inventory model")
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	print("Inventory reflow tests: ", failures, " failures.")
	quit(1 if failures else 0)
