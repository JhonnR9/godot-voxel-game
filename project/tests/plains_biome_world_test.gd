extends "res://tests/support/test_case.gd"

func run() -> void:
	var data: Dictionary = biome_registry()
	var plains: Dictionary
	var mountains: Dictionary

	for biome in data.biomes:
		if biome.name == "plains": plains = biome.duplicate(true)
		if biome.name == "mountains": mountains = biome
	check(not mountains.is_empty(), "Configured mountain profile missing.")
	check(not plains.is_empty(), "Configured plains profile missing.")
	var invalid := data.duplicate(true)

	for profile in invalid.biomes:
		if profile.name == "plains":
			profile.vegetation.cluster_radius = int(profile.vegetation.patch_size)
	check(not VoxelAPI.validate_biome_registry(invalid).valid, "Oversized flower cluster accepted.")
	# Isolate the real plains profile to measure its relief and decorations.
	plains.selection = {"kind": "land"}
	plains.rarity = 1
	data.biomes = [plains]
	data.world.coast_start = -1
	data.world.coast_span = 0.001
	write_config("user://flat_plains.json", data)
	var world := make_world("user://flat_plains.json", Vector3(0, int(data.world.base_height) + 6, 0), "Sparse flat plains")
	await wait_for_world(world)
	var low := int(world.get_pipeline_stats().world_max_y)
	var high := int(world.get_pipeline_stats().world_min_y)

	for z in range(-2048, 2049, 32):
		for x in range(-2048, 2049, 32):
			var c: Dictionary = world.sample_terrain_column(Vector2i(x, z))
			low = mini(low, c.height)
			high = maxi(high, c.height)
	var relief: Dictionary = plains.relief
	var bound := ceili(2.0 * float(data.world.amplitude) * float(relief.get("scale", 1)) + float(relief.get("ridge_amplitude", 0))) + 2
	check(high - low <= bound, "Relief exceeds configured amplitude/scale: %s..%s, bound %s" % [low, high, bound])
	var vegetation: Dictionary = plains.get("vegetation", {})
	var allowed := profile_block_ids(vegetation)
	var decorations := blocks_with_flag("crossed")
	var patch_size := int(vegetation.get("patch_size", 12))
	var radius := int(vegetation.get("cluster_radius", 0))
	var flower_count := 0
	var occupied_patches := {}

	for z in range(-48, 48):
		for x in range(-48, 48):
			var c: Dictionary = world.sample_terrain_column(Vector2i(x, z))
			var block := int(world.get_block_type_at(Vector3(x, c.height + 1, z)))
			for decoration: Dictionary in decorations:
				if block == int(decoration.id): check(allowed.has(block), "Generated decoration is not in configured plains profile.")
			if allowed.has(block):
				flower_count += 1
				occupied_patches[Vector2i(floori(float(x) / patch_size), floori(float(z) / patch_size))] = true
	if int(vegetation.get("coverage_max", 0)) == 0 or int(vegetation.get("patch_chance", 1000)) == 0:
		check(flower_count == 0, "Disabled vegetation was generated.")
	elif radius > 0:
		var max_per_patch := (2 * radius + 1) * (2 * radius + 1)
		check(flower_count <= occupied_patches.size() * max_per_patch, "Cluster population exceeds configured radius.")
	print("Plains checks finished: ", failures, " failures; height ", low, "..", high, "; ", flower_count, " flowers in ", occupied_patches.size(), " clusters.")

	for frame in range(60): await process_frame
	quit(1 if failures else 0)
