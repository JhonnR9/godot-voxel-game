extends "res://tests/support/test_case.gd"

class TestPlayer extends Node:
	var inventory_open := false

var manager: Node:
	get: return root.get_node("InventoryManager")

func move_mouse(position: Vector2) -> void:
	var event := InputEventMouseMotion.new()
	event.position = position
	event.global_position = position
	root.push_input(event, true)
	await process_frame

func run() -> void:
	root.size = Vector2i(960, 640)
	root.content_scale_size = Vector2i.ZERO
	var player := TestPlayer.new()
	root.add_child(player)
	var ui = load("res://scenes/inventory_ui.tscn").instantiate()
	root.add_child(ui)
	manager.setup(player, ui)

	for frame in range(4): await process_frame

	for index in range(9):
		check(ui.hotbar_grid.get_item_at(Vector2i(index, 0)) == null, "Hotbar starts prefilled.")
	check(manager.get_selected_block_id() == 0, "Empty hotbar still selects a block.")
	check(ui.hotbar_grid.get_child_count() == 9, "Hotbar has extra slot-number labels.")
	manager.open_inventory()

	for frame in range(4): await process_frame
	check(manager.is_mouse_unlocked() and ui.inventory_panel.visible, "Player inventory did not open.")
	ui.creative_panel.size = Vector2(500, 220)
	ui.toggle_creative_window()
	await process_frame
	check(ui.creative_panel.visible and ui.inventory_panel.visible, "Independent windows cannot coexist.")
	var center: Vector2 = ui.creative_panel.global_position + ui.creative_panel.size / 2.0
	check(root.get_visible_rect().encloses(ui.creative_panel.get_global_rect()), "Creative panel is outside viewport.")
	var grid_center: float = ui.creative_grid.global_position.x + ui.creative_grid.size.x / 2.0
	var scroll_center: float = ui.creative_scroll.global_position.x + (ui.creative_scroll.size.x - ui.creative_scroll.get_v_scroll_bar().size.x) / 2.0
	check(ui.creative_grid.size.x <= ui.creative_scroll.size.x, "Creative content does not fit window width.")
	check(ui.creative_scroll.get_v_scroll_bar().visible, "Creative inventory does not show its scrollbar.")

	for index in range(ui.blocks.size()):
		var item = ui.creative_grid.get_item_at(Vector2i(index % ui.creative_grid.get_columns(), index / ui.creative_grid.get_columns()))
		check(item != null and item.get_id() == ui.blocks[index].id, "Creative block missing.")
	var slot: Control = ui.creative_grid.get_child(0)
	var item = ui.creative_grid.get_item_at(Vector2i.ZERO)
	check(slot.tooltip_text.contains(item.get_name()) and slot.tooltip_text.contains("ID: %s" % item.get_id()) and slot.tooltip_text.contains("Category: " + item.get_category()), "C++ tooltip metadata missing.")
	check(item.duplicate_item().get_category() == item.get_category(), "Dragged copy loses category.")
	# Wheel input over a slot must propagate to the enclosing ScrollContainer.
	await move_mouse(slot.global_position + slot.size / 2.0)
	var wheel := InputEventMouseButton.new()
	wheel.button_index = MOUSE_BUTTON_WHEEL_DOWN
	wheel.pressed = true
	wheel.position = slot.global_position + slot.size / 2.0
	root.push_input(wheel, true)

	for frame in range(4): await process_frame
	check(ui.creative_scroll.scroll_vertical > 0, "Wheel over an item did not scroll.")
	ui.creative_scroll.scroll_vertical = 10000

	for frame in range(4): await process_frame
	var last: Control = ui.creative_grid.get_child(ui.blocks.size() - 1)
	check(ui.creative_scroll.get_global_rect().encloses(last.get_global_rect()), "Last creative block is unreachable.")
	# Exercise actual drag forwarding to an empty hotbar slot.
	var copy = item.duplicate_item()
	var preview := Control.new()
	ui.creative_grid.drag_started.emit(0)
	slot.force_drag({"inventory_drag": ui.drag_controller.active.token}, preview)
	var target: Control = ui.hotbar_grid.get_child(0)
	var drop_position := target.global_position + target.size / 2.0
	await move_mouse(drop_position)
	var release := InputEventMouseButton.new()
	release.button_index = MOUSE_BUTTON_LEFT
	release.pressed = false
	release.position = drop_position
	root.push_input(release, true)

	for frame in range(4): await process_frame
	check(ui.hotbar_grid.get_item_at(Vector2i.ZERO) != null, "Creative drag did not fill hotbar.")
	check(manager.get_selected_block_id() == item.get_id(), "Selected block was not updated after drop.")
	check(ui.creative_grid.get_item_at(Vector2i.ZERO).get_id() == item.get_id(), "Creative source was consumed.")
	check(ui.hotbar_grid.get_item_at(Vector2i.ZERO).get_item_amount() == 99, "Creative should grant 99 items.")
	var count_label = ui.hotbar_grid.get_child(0).find_children("*", "Label", true, false)
	check(count_label.size() == 1 and count_label[0].text == "99", "Hotbar stack count missing.")
	check(ui.inventory_grid.get_child_count() == 27, "Normal inventory should have 27 slots.")
	# Merging into a nearly full stack preserves the remainder in the source.
	ui.inventory_panel.move_to_front()
	await process_frame
	var small = item.duplicate_item()
	small.set_item_amount(90)
	InventoryService.set_stack(ui.inventory_ids.storage, 0, item.get_id(), 90)
	var incoming = item.duplicate_item()
	incoming.set_item_amount(20)
	InventoryService.set_stack(ui.inventory_ids.hotbar, 0, item.get_id(), 20)
	ui.hotbar_grid.drag_started.emit(0)
	ui.hotbar_grid.get_child(0).force_drag({"inventory_drag": ui.drag_controller.active.token}, Control.new())
	var storage_slot: Control = ui.inventory_grid.get_child(0)
	var storage_position := storage_slot.global_position + storage_slot.size / 2.0
	await move_mouse(storage_position)
	release.position = storage_position
	root.push_input(release, true)

	for frame in range(4): await process_frame
	check(ui.inventory_grid.get_item_at(Vector2i.ZERO).get_item_amount() == 99, "Merged stack exceeds or misses limit.")
	check(ui.hotbar_grid.get_item_at(Vector2i.ZERO).get_item_amount() == 11, "Partial merge lost remaining items.")
	var extra = item.duplicate_item()
	extra.set_item_amount(200)
	check(extra.get_item_amount() == 200, "Presentation object should not enforce game stack rules.")
	small = item.duplicate_item()
	small.set_item_amount(90)
	InventoryService.set_stack(ui.inventory_ids.storage, 1, item.get_id(), 90)
	extra.set_item_amount(20)
	check(InventoryService.add_items(ui.inventory_ids.storage, item.get_id(), 20), "Stack insertion failed.")
	check(ui.inventory_grid.get_item_at(Vector2i(1, 0)).get_item_amount() == 99 and ui.inventory_grid.get_item_at(Vector2i(2, 0)).get_item_amount() == 11, "Insertion failed to split overflow.")
	check(not InventoryService.set_stack(ui.inventory_ids.storage, 0, item.get_id(), 119), "Full slot accepted overflowing items.")
	# A full inventory rejects insertion without modifying any stack.

	for index in range(27):
		InventoryService.set_stack(ui.inventory_ids.storage, index, item.get_id(), 99)
	check(not InventoryService.add_items(ui.inventory_ids.storage, item.get_id(), 20), "Full inventory accepted extra items.")

	for index in range(27):
		check(InventoryService.get_stack(ui.inventory_ids.storage, index).amount == 99, "Rejected insertion changed a stack.")
	check(count_label[0].z_index > 0, "Stack quantity renders behind item icon.")
	# Round-trip through the world's existing level.json section, preserving other fields.
	manager.select_hotbar_slot(3)
	var saved: Dictionary = ui.serialize_inventory()
	var world_id := int(SaveService.create_world(42, "Inventory persistence test"))
	check(SaveService.save_world_section(world_id, "player", {"position": [1, 2, 3], "inventory": saved}), "Inventory save failed.")
	ui.restore_inventory({})
	check(ui.hotbar_grid.get_item_at(Vector2i.ZERO) == null and ui.inventory_grid.get_item_at(Vector2i.ZERO) == null, "Legacy save retained previous world's items.")
	var player_data: Dictionary = SaveService.load_world_section(world_id, "player")
	ui.restore_inventory(player_data.inventory)
	check(ui.serialize_inventory() == saved, "Saved inventory did not restore exactly.")
	check(player_data.position.size() == 3 and Vector3(player_data.position[0], player_data.position[1], player_data.position[2]) == Vector3(1, 2, 3), "Inventory save lost player position.")
	check(ui.hotbar_grid.get_item_at(Vector2i.ZERO).get_icon() != null, "Loaded item icon missing.")
	ui.restore_inventory({"hotbar": [{"id": 999999, "amount": 99}, {"id": item.get_id(), "amount": 999}], "selected_slot": 900})
	check(ui.hotbar_grid.get_item_at(Vector2i.ZERO) == null, "Unknown saved item ID accepted.")
	check(ui.hotbar_grid.get_item_at(Vector2i(1, 0)).get_item_amount() == 99, "Loaded stack exceeds limit.")
	ui.restore_inventory(saved)
	# Smaller windows keep the panels on screen and retain manually added items.
	root.size = Vector2i(480, 360)

	for frame in range(6): await process_frame
	check(root.get_visible_rect().encloses(ui.creative_panel.get_global_rect()), "Creative panel overflows small viewport.")
	check(root.get_visible_rect().encloses(ui.hotbar_panel.get_global_rect()), "Hotbar overflows small viewport.")
	check(ui.creative_panel.size.x <= 480, "Creative window cannot shrink to viewport.")
	var retained = ui.hotbar_grid.get_item_at(Vector2i.ZERO)
	check(retained != null and retained.get_id() == item.get_id(), "Resizing lost hotbar item.")
	InventoryService.set_stack(ui.inventory_ids.hotbar, 0, 0, 0)
	check(ui.hotbar_grid.get_child(0).tooltip_text.is_empty(), "Empty slot retains item tooltip.")
	manager.close_inventory()
	check(not ui.inventory_panel.visible and not ui.creative_panel.visible, "Inventory did not close.")
	manager.set_mouse_unlocked(false)
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	ui.free()
	player.free()
	print("Inventory UI checks finished: ", failures, " failures.")
	quit(1 if failures else 0)
