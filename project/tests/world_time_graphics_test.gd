extends "res://tests/support/test_case.gd"

const GraphicsOptions = preload("res://scripts/graphics_settings.gd")

func run() -> void:
	var graphics = root.get_node("DisplaySettings").graphics
	var scene := Node3D.new()
	var environment_node := WorldEnvironment.new()
	environment_node.name = "WorldEnvironment"
	environment_node.environment = Environment.new()
	environment_node.environment.sky = Sky.new()
	environment_node.environment.sky.sky_material = ProceduralSkyMaterial.new()
	scene.add_child(environment_node)
	var sun := DirectionalLight3D.new()
	sun.name = "DirectionalLight3D"
	sun.set_script(load("res://scripts/directional_light_3d.gd"))
	scene.add_child(sun)
	var world: Node3D = ClassDB.instantiate("VoxelAPI")
	scene.add_child(world)
	root.add_child(scene)
	current_scene = scene
	world.set_focus_position(Vector3(0, 40, 0))
	var world_id := int(SaveService.create_world(7301, "Clock persistence test"))
	world.start_world(world_id)
	sun.set_hour(22.75)
	world.save_world()
	var saved: Dictionary = SaveService.load_world_section(world_id, "world")
	assert(absf(float(saved.get("hour", -1)) - 22.75) < 0.001)
	var other_id := int(SaveService.create_world(7302, "Clock reset test"))
	world.start_world(other_id)
	assert(absf(sun.hora - 8.0) < 0.001)
	world.start_world(world_id)
	assert(absf(sun.hora - 22.75) < 0.001)
	world.set_process(false)
	var settings_panel: Control = load("res://scenes/settings_panel.tscn").instantiate()
	root.add_child(settings_panel)
	assert(settings_panel.antialiasing.item_count >= 3)
	assert(settings_panel.upscaling.item_count >= 1)
	assert(settings_panel.ssao.disabled == not graphics.supports_ssao())
	graphics.set_aa_mode(GraphicsOptions.AA_FXAA)
	assert(root.screen_space_aa == (Viewport.SCREEN_SPACE_AA_FXAA if graphics.supports_fxaa() else Viewport.SCREEN_SPACE_AA_DISABLED))
	graphics.set_upscale_mode(GraphicsOptions.UPSCALE_FSR1_QUALITY)
	if graphics.supports_fsr1():
		assert(root.scaling_3d_mode == Viewport.SCALING_3D_MODE_FSR)
		assert(absf(root.scaling_3d_scale - 0.77) < 0.001)
	graphics.set_ssao_enabled(true)
	assert(environment_node.environment.ssao_enabled == graphics.supports_ssao())
	if graphics.supports_fsr2():
		assert(settings_panel.upscaling.item_count == 7)
		graphics.set_upscale_mode(GraphicsOptions.UPSCALE_FSR2_QUALITY)
		assert(root.scaling_3d_mode == Viewport.SCALING_3D_MODE_FSR2)
		assert(absf(root.scaling_3d_scale - 0.77) < 0.001)
		graphics._load_settings()
		assert(graphics.upscale_mode == GraphicsOptions.UPSCALE_FSR2_QUALITY)
	graphics.set_aa_mode(GraphicsOptions.AA_OFF)
	graphics.set_upscale_mode(GraphicsOptions.UPSCALE_NATIVE)
	graphics.set_ssao_enabled(false)
	if DisplayServer.get_name() != "headless":
		settings_panel.show()
		await process_frame
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png("/tmp/world_time_graphics_menu.png")
	sun.set_hour(3.5)
	scene.queue_free()
	await process_frame
	var exit_saved: Dictionary = SaveService.load_world_section(world_id, "world")
	assert(absf(float(exit_saved.get("hour", -1)) - 3.5) < 0.001)
	print("World time and graphics settings passed: save, world switch, scene exit, UI, FXAA, FSR, SSAO.")
	quit()
