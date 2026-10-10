extends "res://tests/support/test_case.gd"

func run() -> void:
	var data: Dictionary = biome_registry()
	check(VoxelAPI.validate_biome_registry(data).valid, "Snow registry rejected.")
	var invalid := data.duplicate(true)
	invalid.biomes[0].surface_fill = blocks_with_flag("crossed")[0].name
	check(not VoxelAPI.validate_biome_registry(invalid).valid, "Non-solid surface fill accepted.")
	var snow: Dictionary

	for biome in data.biomes:
		if biome.name == "snow": snow = biome.duplicate(true)
	check(snow.trees.shape == "pine" and snow.surface_fill == "ice", "Snow pine/ice profile missing.")
	# Flat snow forest exercises conical pine crowns and scheduling at borders.
	var forest := data.duplicate(true)
	forest.biomes = [snow]
	forest.biomes[0].selection = {"kind": "land"}
	forest.biomes[0].rarity = 1
	forest.biomes[0].relief = {"anchor": 0, "ridge_amplitude": 0, "bias": 0}
	var tree_height := floori(float(snow.trees.min_height + snow.trees.max_height) / 2.0)
	forest.biomes[0].trees.min_height = tree_height
	forest.biomes[0].trees.max_height = tree_height
	forest.world.base_height = 32
	forest.world.amplitude = 0
	forest.world.coast_start = -1
	forest.world.coast_span = 0.001
	write_config("user://snow_forest.json", forest)
	var a := make_world("user://snow_forest.json", Vector3(0, 32, 0), "Snow pine forest A")
	var b := make_world("user://snow_forest.json", Vector3(16, 32, 16), "Snow pine forest B")
	var pine_log := block_id(snow.trees.trunk)
	var pine_leaves := block_id(snow.trees.leaves)
	if await wait_for_world(a) and await wait_for_world(b):
		var trunks := 0
		var leaves := 0
		var crown_layers := {}
		for z in range(-16, 32):
			for x in range(-16, 32):
				var column: Dictionary = a.sample_terrain_column(Vector2i(x, z))
				check(column.height == 32 and column.surface_block == block_id(snow.materials.surface), "Snow surface incorrect.")
				for y in range(33, 32 + tree_height + 4):
					var p := Vector3(x, y, z)
					var first := int(a.get_block_type_at(p))
					check(first == int(b.get_block_type_at(p)), "Pine changed with chunk loading order at %s" % p)
					if first == pine_log: trunks += 1
					if first == pine_leaves:
						leaves += 1
						crown_layers[y] = int(crown_layers.get(y, 0)) + 1
		if int(snow.trees.get("max_per_chunk", 0)) > 0:
			check(trunks > 0 and leaves > 0, "Configured snow pines missing.")
			var layers := crown_layers.keys()
			layers.sort()
			if layers.size() >= 2:
				check(crown_layers[layers.front()] > crown_layers[layers.back()], "Pine crown did not taper to a tip.")
			else: check(false, "Pine crown has no vertical extent.")
		else: check(trunks == 0 and leaves == 0, "Disabled snow trees were generated.")
	# Full coastal influence must select a frozen ocean in cold climate and
	# ordinary water in warm climate, using actual generated blocks.

	for cold in [true, false]:
		var coast := data.duplicate(true)
		coast.world.climate_start = 1 if cold else -1
		coast.world.climate_span = 0.001
		coast.world.coast_start = 1
		coast.world.coast_span = 0.001
		coast.world.dry_coast_start = 1
		coast.world.dry_coast_span = 0.001
		for biome in coast.biomes:
			biome.trees = {}
			biome.vegetation = {}
		var path := "user://snow_coast_%s.json" % cold
		write_config(path, coast)
		var world := make_world(path, Vector3(0, 24, 0), "Frozen ocean %s" % cold)
		if await wait_for_world(world):
			var fill := block_id("ice" if cold else "water")
			for z in [-17, -16, -1, 0, 15, 16, 17]:
				for x in [-17, -16, -1, 0, 15, 16, 17]:
					var column: Dictionary = world.sample_terrain_column(Vector2i(x, z))
					check(column.biome_name == ("frozen_ocean" if cold else "ocean"), "Incorrect coastal climate selection.")
					check(column.surface_fill == fill, "Incorrect coastal fill palette.")
					for y in range(column.height + 1, column.water_level):
						var p := Vector3(x, y, z)
						check(world.get_block_type_at(p) == fill, "Generated ice/water disagrees at %s" % p)
						check(world.is_water_at(p) == not cold, "Solid ice was treated as swimming water.")

	for frame in range(60): await process_frame
	print("Snow biome checks finished: ", failures, " failures.")
	quit(1 if failures else 0)
