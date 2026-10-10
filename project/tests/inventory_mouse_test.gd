extends "res://tests/support/test_case.gd"

var manager: Node:
	get: return root.get_node("InventoryManager")

func click_at(point: Vector2) -> void:
	var motion := InputEventMouseMotion.new()
	motion.position = point
	motion.global_position = point
	root.push_input(motion, true)

	for pressed in [true, false]:
		var click := InputEventMouseButton.new()
		click.button_index = MOUSE_BUTTON_LEFT
		click.pressed = pressed
		click.position = point
		root.push_input(click, true)
		await process_frame

func gesture(target: Control, delta: Vector2) -> void:
	var point := target.global_position + target.size / 2
	var motion := InputEventMouseMotion.new()
	motion.position = point
	motion.global_position = point
	root.push_input(motion, true)
	var click := InputEventMouseButton.new()
	click.position = point
	click.button_index = MOUSE_BUTTON_LEFT
	click.pressed = true
	root.push_input(click, true)
	await process_frame
	motion.position = point + delta
	motion.global_position = point + delta
	motion.relative = delta
	root.push_input(motion, true)
	await process_frame
	click.position = point + delta
	click.pressed = false
	root.push_input(click, true)
	await process_frame

func run() -> void:
	root.size = Vector2i(1280, 720)
	root.content_scale_size = Vector2i.ZERO
	var player = CharacterBody3D.new()
	player.set_script(load("res://scripts/player.gd"))
	var head := Node3D.new()
	var camera := Camera3D.new()
	var shape := CollisionShape3D.new()
	shape.shape = CapsuleShape3D.new()
	player.add_child(head)
	head.add_child(camera)
	player.add_child(shape)
	player.head = head
	player.camera = camera
	player.collision_shape = shape
	root.add_child(player)
	player.set_physics_process(false)
	var ui = player.get_node("InventoryCanvas/InventoryUI")

	for frame in range(4): await process_frame
	check(not manager.is_mouse_unlocked(), "Starts controlling character")
	var key := InputEventKey.new()
	key.physical_keycode = KEY_F1
	key.pressed = true
	check(key.is_action_pressed("unlock_mouse") and not key.is_action_pressed("save"), "F1 has one dedicated action")
	root.push_input(key, true)
	await process_frame
	check(manager.is_mouse_unlocked(), "F1 unlocks mouse")
	check(not ui.inventory_panel.visible and not ui.creative_panel.visible, "Unlock does not force windows open")
	await click_at(ui.get_node("Launchers/Inventory").global_position + Vector2(24, 22))
	await click_at(ui.get_node("Launchers/Creative").global_position + Vector2(24, 22))
	check(ui.inventory_panel.visible and ui.creative_panel.visible, "Launcher icons open independent windows")
	var before: Vector2 = ui.creative_panel.position
	await gesture(ui.creative_panel.get_node("Margin/Content/TitleBar/Title"), Vector2(70, 40))
	check(ui.creative_panel.position.distance_to(before + Vector2(70, 40)) < 2, "Title bar moves window")
	var old_size: Vector2 = ui.creative_panel.size
	await gesture(ui.creative_panel.resize_handle, Vector2(60, 35))
	check(ui.creative_panel.size.distance_to(old_size + Vector2(60, 35)) < 2, "Corner resizes window")
	var rect: Rect2 = ui.creative_panel.get_rect()
	var initial_yaw: float = player.yaw
	var motion := InputEventMouseMotion.new()
	motion.relative = Vector2(25, 10)
	player._unhandled_input(motion)
	check(player.yaw == initial_yaw, "Free mouse never rotates camera")
	player.velocity = Vector3.ONE
	player._physics_process(0.016)
	check(player.velocity == Vector3.ZERO, "Free mouse stops character movement")
	root.push_input(key, true)
	await process_frame
	check(not manager.is_mouse_unlocked(), "Second F1 returns character control")
	check(ui.inventory_panel.visible and ui.creative_panel.visible, "Windows remain open while playing")
	check(ui.creative_panel.get_rect() == rect, "F1 retains chosen window placement")

	for control in ui.find_children("*", "Control", true, false):
		check(control.mouse_filter == Control.MOUSE_FILTER_IGNORE, "Captured mouse bypasses " + str(control.get_path()))
	player._unhandled_input(motion)
	check(player.yaw != initial_yaw, "Camera responds with inventory windows still open")
	Input.action_press("move_forward")
	player.current_mode = player.Mode.FLY
	player._physics_process(0.016)
	Input.action_release("move_forward")
	check(player.velocity.length() > 0, "Character moves with windows still open")
	manager.set_mouse_unlocked(true)
	ui.collect_block(block_id("fern"))
	var source: Control = ui.hotbar_grid.get_child(0)
	var point := source.global_position + source.size / 2
	var drag_motion := InputEventMouseMotion.new()
	drag_motion.position = point
	drag_motion.global_position = point
	root.push_input(drag_motion, true)
	var press := InputEventMouseButton.new()
	press.position = point
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	root.push_input(press, true)
	await process_frame
	drag_motion.position = point + Vector2(25, 0)
	drag_motion.global_position = drag_motion.position
	drag_motion.relative = Vector2(25, 0)
	drag_motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	root.push_input(drag_motion, true)
	await process_frame
	check(root.gui_is_dragging(), "Actual item drag starts")
	manager.set_mouse_unlocked(false)
	await process_frame
	check(ui.hotbar_grid.get_item_at(Vector2i.ZERO) != null, "F1 cancels drag without losing item")
	manager.set_mouse_unlocked(true)
	ui._load_window_layout()
	check(ui.creative_panel.get_rect() == rect, "Window placement is saved")
	if DisplayServer.get_name() != "headless":
		for frame in range(12): await process_frame
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png("/tmp/inventory_windows.png")
	player.free()
	await process_frame
	check(not manager.is_mouse_unlocked(), "Teardown clears interaction state")
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	print("Inventory mouse tests: ", failures, " failures.")
	quit(1 if failures else 0)
