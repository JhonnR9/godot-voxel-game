extends "res://tests/support/test_case.gd"

func lights(world: Node) -> Array[Node]:
	return world.find_children("*", "OmniLight3D", true, false)

func wait_lights(world: Node, expected: int) -> void:
	var deadline := Time.get_ticks_msec() + 10000
	while Time.get_ticks_msec() < deadline:
		if lights(world).size() == expected:
			return
		await process_frame
	check(false, "Expected %d torch lights, got %d" % [expected, lights(world).size()])

func run() -> void:
	var data: Dictionary = biome_registry()
	data.world.base_height = 32
	data.world.amplitude = 0
	data.world.sea_level = 0
	data.world.coast_start = -1
	data.biomes = [{"id": 51, "name": "torch_fixture", "selection": {"kind": "land"},
		"relief": {"anchor": 0, "ridge_amplitude": 0, "bias": 0},
		"materials": {"surface": "stone", "soil": "stone", "rock": "stone", "deep_rock": "deepslate"},
		"soil_depth": 3, "deep_rock_below_y": 24}]
	write_config("user://torch_fixture.json", data)
	var id := int(SaveService.create_world(42, "Torch test"))
	var world: Node = ClassDB.instantiate("VoxelAPI")
	world.set_biome_registry_path("user://torch_fixture.json")
	root.add_child(world)
	world.set_render_settings({"render_distance": 4, "vertical_render_distance": 2})
	world.set_focus_position(Vector3(8, 48, 8))
	world.start_world(id)
	if not await wait_for_world(world):
		quit(1)
		return
	var torch := block_id("torch")
	check(torch > 0, "Torch is registered")
	check(root.get_node("BlockIconCache").get_icon(torch) != null, "Torch inventory icon exists")
	var point := Vector3(8, 33, 8)
	world.set_block(point, torch)
	await wait_lights(world, 1)
	check(world.get_block_type_at(point) == torch, "Placed torch exists")
	if not lights(world).is_empty():
		var light: OmniLight3D = lights(world)[0]
		check(light.shadow_enabled and light.light_energy > 0, "Torch emits shadowed light")
		check(light.global_position.is_equal_approx(point + Vector3(0.5, 0.8, 0.5)), "Light follows block position")
		var selector: StaticBody3D = light.get_parent().get_node("ObjectSelection")
		check(selector.collision_layer == 2 and selector.collision_mask == 0, "Torch selection does not block player movement")
	# All crossed-object types share selection, independently of torch lights.
	var player_script = load("res://scripts/player.gd")
	check(player_script != null, "Player interaction script parses")
	var ui = load("res://scenes/inventory_ui.tscn").instantiate()
	root.add_child(ui)
	check(ui.collect_block(block_id("fern")), "Plant can be collected into inventory")
	ui.free()

	for entry: Dictionary in blocks_with_flag("crossed"):
		if entry.name == "torch": continue
		var name: String = entry.name
		var plant_point := Vector3(12, 48, 8)
		world.set_block(plant_point, block_id(name))
		var deadline := Time.get_ticks_msec() + 10000
		var hit: Dictionary = {}
		while Time.get_ticks_msec() < deadline:
			await physics_frame
			await process_frame
			var query := PhysicsRayQueryParameters3D.create(plant_point + Vector3(0.5, 0.5, 3), plant_point + Vector3(0.5, 0.5, -1), 2)
			hit = root.get_world_3d().direct_space_state.intersect_ray(query)
			if not hit.is_empty(): break
		check(not hit.is_empty(), name + " detected by selection ray")
		if not hit.is_empty():
			var body: StaticBody3D = hit.collider
			var positions: PackedVector3Array = body.get_meta("voxel_selection_positions")
			check(body.to_global(positions[hit.shape]).is_equal_approx(plant_point), "Selection maps to exact voxel")
		var movement_query := PhysicsRayQueryParameters3D.create(plant_point + Vector3(0.5, 0.5, 3), plant_point + Vector3(0.5, 0.5, -1), 1)
		check(root.get_world_3d().direct_space_state.intersect_ray(movement_query).is_empty(), "Plant does not obstruct movement")
		world.break_block(plant_point)
		deadline = Time.get_ticks_msec() + 10000
		while Time.get_ticks_msec() < deadline:
			await physics_frame
			await process_frame
			var query := PhysicsRayQueryParameters3D.create(plant_point + Vector3(0.5, 0.5, 3), plant_point + Vector3(0.5, 0.5, -1), 2)
			if root.get_world_3d().direct_space_state.intersect_ray(query).is_empty(): break
		check(world.get_block_type_at(plant_point) == 0, "Plant removed from world")
		check(Time.get_ticks_msec() < deadline, "Selection collider removed after breaking")
	var environment := WorldEnvironment.new()
	environment.environment = Environment.new()
	environment.environment.background_mode = Environment.BG_COLOR
	environment.environment.background_color = Color(0.015, 0.02, 0.03)
	environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.environment.ambient_light_color = Color(0.3, 0.35, 0.5)
	environment.environment.ambient_light_energy = 0.08
	root.add_child(environment)
	var camera := Camera3D.new()
	root.add_child(camera)
	camera.position = Vector3(10, 35, 12)
	camera.look_at(point + Vector3(0.5, 0.5, 0.5))
	if DisplayServer.get_name() != "headless":
		for frame in range(30): await process_frame
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png("/tmp/torch_preview.png")
	world.save_world()
	world.start_world(id)
	await wait_for_world(world)
	await wait_lights(world, 1)
	check(world.get_block_type_at(point) == torch, "Torch survives world reload")
	world.break_block(point)
	await wait_lights(world, 0)
	# Removing the last block of an otherwise empty chunk must clear its light too.
	point = Vector3(8, 48, 8)
	world.set_block(point, torch)
	await wait_lights(world, 1)
	world.break_block(point)
	await wait_lights(world, 0)
	world.free()
	camera.free()
	environment.free()
	print("Torch tests: ", failures, " failures (placement, icon, lighting, selection, reload, removal).")
	quit(1 if failures else 0)
