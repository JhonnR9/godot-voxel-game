extends "res://tests/support/test_case.gd"

func run() -> void:
	var data: Dictionary = biome_registry()
	check(VoxelAPI.validate_biome_registry(data).valid, "Default registry rejected.")
	var land_index := -1
	var tree_index := -1
	var vegetation_index := -1

	for index in range(data.biomes.size()):
		var entry: Dictionary = data.biomes[index]
		if entry.selection.kind == "land" and land_index < 0: land_index = index
		if int(entry.get("trees", {}).get("max_per_chunk", 0)) > 0: tree_index = index
		if int(entry.get("vegetation", {}).get("coverage_max", 0)) > 0: vegetation_index = index

	for change in ["duplicate", "gap", "block", "strata", "version", "noise", "trees", "plants", "rarity", "fractional_rarity", "fallback"]:
		var invalid := data.duplicate(true)
		match change:
			"duplicate": invalid.biomes[1].id = invalid.biomes[0].id
			"gap": invalid.biomes[land_index].selection.climate_max = (float(data.biomes[land_index].selection.get("climate_min", 0)) + float(data.biomes[land_index].selection.get("climate_max", 1))) / 2.0
			"block": invalid.biomes[0].materials.surface = "missing_block"
			"strata": invalid.biomes[0].strata = [{"block": "stone", "min_depth": 8, "max_depth": 2}]
			"version": invalid.version = int(data.version) + 1
			"noise": invalid.world.noises.terrain.frequency = 0
			"trees": invalid.biomes[tree_index].trees.min_height = int(invalid.biomes[tree_index].trees.get("max_height", 0)) + 1
			"plants": invalid.biomes[vegetation_index].vegetation.plants = []
			"rarity": invalid.biomes[0].rarity = -1
			"fractional_rarity": invalid.biomes[0].rarity = 1.5
			"fallback":
				for b in invalid.biomes: b.rarity = 2
		var result: Dictionary = VoxelAPI.validate_biome_registry(invalid)
		check(not result.valid and not str(result.error).is_empty(), "Invalid registry accepted: " + change)

	# A new biome and depth-dependent sandstone layer require only configuration.
	var custom := data.duplicate(true)
	custom.world.base_height = 32
	custom.world.sea_level = 0
	custom.world.amplitude = 0
	custom.world.coast_start = -1
	custom.biomes = [{
		"id": 51, "name": "test_forest", "selection": {"kind": "land"},
		"relief": {"anchor": 0, "ridge_amplitude": 0, "bias": 0},
		"materials": {"surface": "dirt", "soil": "sand", "rock": "stone", "deep_rock": "deepslate"},
		"soil_depth": 3, "deep_rock_below_y": 24,
		"strata": [{"block": "sandstone", "min_depth": 4, "max_depth": 8}]
	}]
	check(VoxelAPI.validate_biome_registry(custom).valid, "Custom biome rejected.")
	write_config("user://test_biome_registry.json", custom)
	var world := make_world("user://test_biome_registry.json", Vector3(0, 32, 0), "Configured terrain test")
	if await wait_for_world(world):
		for z in [-17, -16, -1, 0, 15, 16, 17]:
			for x in [-17, -16, -1, 0, 15, 16, 17]:
				var c: Dictionary = world.sample_terrain_column(Vector2i(x, z))
				check(c.height == 32 and c.biome_id == 51 and c.biome_name == "test_forest", "Custom biome sampling disagrees.")
				check(c.water_level == 0 and not c.trees_allowed, "Custom sea level/tree profile ignored.")
				for pair in [[33, "air"], [32, "dirt"], [31, "sand"], [29, "sand"], [28, "sandstone"], [24, "sandstone"], [23, "deepslate"]]:
					var expected := block_id(str(pair[1]))
					check(world.get_block_type_at(Vector3(x, pair[0], z)) == expected, "Layer mismatch at %s: expected %s" % [Vector3(x, pair[0], z), pair[1]])

	# Remove decorations only in this fixture, then compare the real generated
	# terrain to the public sampler at chunk edges and negative coordinates.
	var plain := data.duplicate(true)

	for b in plain.biomes:
		b.trees = {}
		b.vegetation = {}
	write_config("user://test_default_terrain.json", plain)
	var defaults := make_world("user://test_default_terrain.json", Vector3(0, 32, 0), "Terrain sampler agreement test")
	if await wait_for_world(defaults):
		for z in range(-24, 25, 3):
			for x in range(-24, 25, 3):
				var c: Dictionary = defaults.sample_terrain_column(Vector2i(x, z))
				check(defaults.get_block_type_at(Vector3(x, c.height, z)) == c.surface_block, "Sampler and surface disagree at %s" % Vector2i(x, z))
				var above := int(defaults.get_block_type_at(Vector3(x, c.height + 1, z)))
				var expect_water: bool = c.surface_water and c.height + 1 < c.water_level
				var expected_fill: int = c.surface_fill
				if c.height + 1 > c.solid_fill_height: expected_fill = block_id("water")
				check(above == (expected_fill if expect_water else block_id("air")), "Sampler and surface fill/air disagree.")
	# Exercise all existing palettes across broad climate/coast regions.
	var found: Dictionary = {}

	for z in range(-4096, 4097, 128):
		for x in range(-4096, 4097, 128):
			var c: Dictionary = defaults.sample_terrain_column(Vector2i(x, z))
			found[c.biome_name] = true

	for entry: Dictionary in data.biomes:
		var name: String = entry.name
		if int(entry.get("rarity", 1)) == 0:
			check(not found.has(name), "Disabled biome selected: " + name)
		# Rare or narrow profiles may legitimately be absent from a finite scan.
		# Exercise every registered profile separately below instead of requiring golden counts.
	print("Configured biome coverage: ", found.keys())

	for entry: Dictionary in data.biomes:
		var isolated := data.duplicate(true)
		var profile := entry.duplicate(true)
		profile.selection = {"kind": "land"}
		profile.rarity = 1
		profile.relief = {"anchor": 0, "ridge_amplitude": 0, "bias": 0}
		profile.trees = {}
		profile.vegetation = {}
		isolated.biomes = [profile]
		isolated.world.amplitude = 0
		isolated.world.coast_start = -1
		isolated.world.dry_coast_start = -1
		isolated.world.coast_span = 0.001
		isolated.world.dry_coast_span = 0.001
		isolated.world.sea_level = min(0, int(isolated.world.base_height) - 1)
		write_config("user://isolated_profile.json", isolated)
		var profile_world := make_world("user://isolated_profile.json", Vector3(0, int(isolated.world.base_height) + 6, 0), "Registry profile " + str(entry.name))
		if await wait_for_world(profile_world):
			for point in [Vector2i.ZERO, Vector2i(-17, 16), Vector2i(17, -16)]:
				var column: Dictionary = profile_world.sample_terrain_column(point)
				check(int(column.biome_id) == int(entry.id) and column.biome_name == entry.name, "Configured profile identity was not preserved.")
				check(profile_world.get_block_type_at(Vector3(point.x, column.height, point.y)) == column.surface_block, "Configured profile surface disagrees with generation.")
		profile_world.free()

	# Different initial focus changes chunk scheduling/cache population order.
	# Border trees must still produce exactly the same blocks in both worlds.
	var trees := custom.duplicate(true)
	trees.biomes[0].trees = {"max_per_chunk": 4, "trunk": "sandstone", "leaves": "oak_leaves", "min_height": 5, "max_height": 7, "crown_radius": 3}
	write_config("user://test_tree_profiles.json", trees)
	var tree_a := make_world("user://test_tree_profiles.json", Vector3(0, 32, 0), "Tree boundary A")
	var tree_b := make_world("user://test_tree_profiles.json", Vector3(32, 32, 32), "Tree boundary B")
	if await wait_for_world(tree_a) and await wait_for_world(tree_b):
		var trunks := 0
		var leaves := 0
		var sandstone := block_id("sandstone")
		var leaf := block_id("oak_leaves")
		for z in range(-32, 64):
			for x in range(-32, 64):
				for y in range(33, 46):
					var pos := Vector3(x, y, z)
					var a := int(tree_a.get_block_type_at(pos))
					var b := int(tree_b.get_block_type_at(pos))
					check(a == b, "Tree generation depends on chunk scheduling at %s" % pos)
					if a == sandstone: trunks += 1
					if a == leaf: leaves += 1
		check(trunks > 50 and leaves > 200, "Configured tree materials/crown were not generated.")
	var high := custom.duplicate(true)
	high.world.base_height = 239
	high.biomes[0].relief.bias = 64
	write_config("user://test_spawn_biome.json", high)
	var spawn_world: Node = ClassDB.instantiate("VoxelAPI")
	spawn_world.set_biome_registry_path("user://test_spawn_biome.json")
	root.add_child(spawn_world)
	spawn_world.set_render_settings({"render_distance": 4, "vertical_render_distance": 2})
	var player := Node3D.new()
	spawn_world.add_child(player)
	player.global_position = Vector3(-17.5, 0, 16.3)
	spawn_world.set_focus_node(player)
	spawn_world.start_world(int(SaveService.create_world(42, "Configured spawn height test")))
	var spawn_column: Dictionary = spawn_world.sample_terrain_column(Vector2i(-18, 16))
	check(spawn_column.height == mini(303, int(spawn_world.get_pipeline_stats().world_max_y)), "Configured terrain exceeded world height limits.")
	check(player.global_position.y == spawn_column.height + 6, "Player spawned inside configured high terrain.")
	await wait_for_world(spawn_world)
	# Detach the test focus before Godot tears down the child nodes.
	spawn_world.set_focus_node(null)
	spawn_world.set_focus_position(player.global_position)
	world.set_focus_position(Vector3(0, -1008, 0))
	world.start_world(int(SaveService.create_world(42, "Bedrock loading boundary test")))
	if await wait_for_world(world):
		check(world.get_block_type_at(Vector3(0, int(world.get_pipeline_stats().world_min_y), 0)) == block_id("bedrock"), "Bedrock boundary failed to load.")

	for frame in range(60):
		await process_frame
	print("Biome registry checks finished: ", failures, " failures.")
	quit(1 if failures else 0)
