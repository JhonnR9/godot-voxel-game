extends "res://tests/support/test_case.gd"

func run() -> void:
	var menu: Control = load("res://scenes/main_menu.tscn").instantiate()
	root.add_child(menu)
	current_scene = menu
	menu._on_settings_pressed()
	var panel: Control = menu.get_node("SettingsPanel")
	assert(panel.visible and not menu.get_node("HUD").visible)
	var tabs: TabContainer = panel.get_node("Center/Panel/Margin/Content/Categories")
	assert(tabs.get_tab_count() == 3)
	for index in range(tabs.get_tab_count()):
		tabs.current_tab = index
		await process_frame
		if DisplayServer.get_name() != "headless":
			await RenderingServer.frame_post_draw
			root.get_texture().get_image().save_png("/tmp/settings_category_%d.png" % index)
	panel._on_back_pressed()
	assert(not panel.visible and menu.get_node("HUD").visible)
	assert(menu.get_node("HUD/Buttons/Settings").has_focus())
	# Exercise the actual pause controller without loading a terrain world.
	var game := Node.new()
	var world: Node = ClassDB.instantiate("VoxelAPI")
	world.name = "VoxelAPI"
	game.add_child(world)
	var layer := CanvasLayer.new()
	game.add_child(layer)
	var pause_scene: PackedScene = load("res://scenes/VoxelAPI.tscn")
	var template := pause_scene.instantiate()
	var pause_menu := template.get_node("CanvasLayer/Pause").duplicate()
	layer.add_child(pause_menu)
	var pause_settings: Control = load("res://scenes/settings_panel.tscn").instantiate()
	layer.add_child(pause_settings)
	template.free()
	root.add_child(game)
	pause_menu.toggle_pause()
	pause_menu._on_settings_pressed()
	assert(paused and pause_settings.visible and not pause_menu.visible)
	var escape := InputEventAction.new()
	escape.action = "ui_cancel"
	escape.pressed = true
	Input.parse_input_event(escape)
	await process_frame
	assert(paused and not pause_settings.visible and pause_menu.visible)
	pause_menu._on_settings_pressed()
	pause_settings._on_back_pressed()
	assert(paused and pause_menu.visible)
	paused = false
	game.free()
	menu.free()
	print("Settings navigation passed: categories, caller, focus, Back, Escape, pause state.")
	quit()
