extends "res://tests/support/test_case.gd"

func run() -> void:
	var data: Dictionary = biome_registry()

	for b in data.biomes:
		if b.name == "beach":
			check(b.trees.shape == "palm" and block_id(b.trees.trunk) > 0, "Beach palm profile missing.")
		elif b.name == "desert" or b.name == "ocean":
			check(b.get("trees", {}).get("max_per_chunk", 0) == 0, "Palms configured outside beach.")
	var invalid := data.duplicate(true)

	for profile in invalid.biomes:
		if profile.name == "beach": profile.trees.shape = "unknown"
	check(not VoxelAPI.validate_biome_registry(invalid).valid, "Invalid tree shape accepted.")
	# Fully flatten the shoreline and remove rivers to exercise beach trees
	# across positive/negative chunk boundaries with different loading orders.
	data.world.amplitude = 0
	data.world.climate_start = -1
	data.world.climate_span = 0.001
	data.world.coast_start = 1
	data.world.dry_coast_start = 1
	data.world.coast_span = 0.001
	data.world.dry_coast_span = 0.001
	data.world.wet_coast_offset = 0

	for b in data.biomes:
		b.relief = b.get("relief", {})
		b.relief.ridge_amplitude = 0
		b.relief.bias = 0
		b.vegetation = {}
		if b.name == "river": b.rarity = 0
	write_config("user://palm_beach.json", data)
	var a := make_world("user://palm_beach.json", Vector3(0, 24, 0), "Palm beach A")
	var b := make_world("user://palm_beach.json", Vector3(16, 24, 16), "Palm beach B")
	if await wait_for_world(a) and await wait_for_world(b):
		var trunks := 0
		var leaves := 0
		var palm: Dictionary = biome("beach").trees
		var palm_log := block_id(palm.trunk)
		var palm_leaves := block_id(palm.leaves)
		for z in range(-16, 32):
			for x in range(-16, 32):
				var c: Dictionary = a.sample_terrain_column(Vector2i(x, z))
				check(c.biome_name == "beach", "Palm fixture did not select beach.")
				for y in range(int(data.world.sea_level) + 1, int(data.world.sea_level) + int(palm.max_height) + 5):
					var p := Vector3(x, y, z)
					var first := int(a.get_block_type_at(p))
					check(first == int(b.get_block_type_at(p)), "Palm changed across chunk loading order at %s" % p)
					if first == palm_log: trunks += 1
					if first == palm_leaves: leaves += 1
		if int(palm.max_per_chunk) > 0:
			check(trunks > 0 and leaves > 0, "Beach palms were not generated.")
		else:
			check(trunks == 0 and leaves == 0, "Disabled beach palms were generated.")

	for frame in range(60): await process_frame
	print("Palm tree checks finished: ", failures, " failures.")
	quit(1 if failures else 0)
