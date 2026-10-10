extends "res://tests/support/test_case.gd"

var session: Node:
	get: return root.get_node("InventorySession")
var manager: Node:
	get: return root.get_node("InventoryManager")

func begin(grid: GridInventory, controller: Node) -> Dictionary:
	grid.drag_started.emit(0)
	return {"inventory_drag": controller.active.token}

func run() -> void:
	root.size = Vector2i(1280, 720)
	root.content_scale_size = Vector2i.ZERO
	var ui = load("res://scenes/inventory_ui.tscn").instantiate()
	root.add_child(ui)
	var player := Node.new()
	root.add_child(player)
	manager.setup(player, ui)
	manager.open_inventory()

	for frame in range(4): await process_frame
	var source: String = ui.inventory_ids.hotbar
	var target: String = ui.inventory_ids.storage
	InventoryService.set_stack(source, 0, block_id("grass"), 23)
	InventoryService.set_stack(target, 0, block_id("grass"), 99)
	var original = ui.hotbar_grid.get_item_at(Vector2i.ZERO)
	var payload := begin(ui.hotbar_grid, ui.drag_controller)
	check(ui.hotbar_grid.get_child(0).tooltip_text.is_empty(), "Pending drag hides source presentation")
	check(InventoryService.get_stack(source, 0).amount == 23, "Pending drag keeps logical item and save snapshot")
	# Deliberately stale UI data must be replaced after rejected drop, including metadata.
	var stale := ItemView.new()
	stale.set_id(block_id("dirt"))
	stale.set_name("stale")
	stale.set_category("wrong")
	stale.set_item_amount(1)
	ui.hotbar_grid.set_item_view(0, stale)
	ui.inventory_grid.drop_requested.emit(payload, 0)
	var restored = ui.hotbar_grid.get_item_at(Vector2i.ZERO)
	check(ui.drag_controller.active.is_empty(), "Full target ends drag")
	check(restored.get_id() == original.get_id() and restored.get_name() == original.get_name() and restored.get_category() == original.get_category() and restored.get_item_amount() == 23 and restored.get_icon() == original.get_icon(), "Rejected drop synchronizes every ItemView field")
	check(ui.hotbar_grid.get_child(0).tooltip_text.contains(original.get_name()), "Rejected drop restores rendered tooltip")
	check(InventoryService.get_stack(target, 0).amount == 99, "Rejected drop cannot mutate target")
	payload = begin(ui.hotbar_grid, ui.drag_controller)
	ui.inventory_panel.hide()
	ui.inventory_grid.drop_requested.emit(payload, 1)
	check(InventoryService.get_stack(source, 0).amount == 23 and InventoryService.get_stack(target, 1).is_empty(), "Hidden window is not a valid destination")
	ui.inventory_panel.show()
	payload = begin(ui.hotbar_grid, ui.drag_controller)
	InventoryService.set_stack(source, 0, block_id("dirt"), 7)
	ui.inventory_grid.drop_requested.emit(payload, 1)
	check(InventoryService.get_stack(source, 0) == {"id": block_id("dirt"), "amount": 7}, "Concurrent source mutation cancels stale request")
	check(ui.hotbar_grid.get_item_at(Vector2i.ZERO).get_id() == block_id("dirt"), "Mutation cancellation refreshes replacement ItemView")
	payload = begin(ui.hotbar_grid, ui.drag_controller)
	ui.inventory_grid.drop_requested.emit(payload, 1)
	check(InventoryService.get_stack(source, 0).is_empty() and InventoryService.get_stack(target, 1).amount == 7, "Controller maps grid events to logical transfer")
	ui.inventory_grid.drop_requested.emit(payload, 2)
	check(InventoryService.get_stack(target, 2).is_empty(), "Consumed UI token cannot replay")
	var mirror := GridInventory.new()
	root.add_child(mirror)
	ui.drag_controller.bind_grid(mirror, target)
	InventoryService.set_stack(target, 0, block_id("grass"), 10)
	check(mirror.get_item_at(Vector2i.ZERO).get_item_amount() == 10 and ui.inventory_grid.get_item_at(Vector2i.ZERO).get_item_amount() == 10, "Notification updates multiple views of one UUID")
	var view_copy = mirror.get_item_at(Vector2i.ZERO)
	view_copy.set_item_amount(1)
	check(InventoryService.get_stack(target, 0).amount == 10, "ItemView mutation does not change model")
	begin(ui.inventory_grid, ui.drag_controller)
	ui.inventory_panel.hide()
	check(ui.drag_controller.active.is_empty(), "Closing source window cancels pending drag")
	mirror.free()
	ui.free()
	player.free()
	check(InventoryService.has_inventory(target) and InventoryService.get_stack(target, 1).amount == 7, "Destroying UI preserves model identity and contents")
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	print("Inventory controller tests: ", failures, " failures.")
	quit(1 if failures else 0)
