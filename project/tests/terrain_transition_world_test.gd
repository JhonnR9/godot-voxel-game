extends "res://tests/support/test_case.gd"

func run() -> void:
	var data: Dictionary = biome_registry()
	# Force dry climate and full coastal influence. The old desert clamp kept
	# this above sea level and skipped ocean selection altogether.
	var coast := data.duplicate(true)
	coast.world.climate_start = -1
	coast.world.climate_span = 0.001
	coast.world.dry_coast_start = 1
	coast.world.dry_coast_span = 0.001
	for b in coast.biomes:
		b.trees = {}
		b.vegetation = {}
	write_config("user://transition_coast.json", coast)
	var world := make_world("user://transition_coast.json", Vector3(0, 24, 0), "Desert ocean transition test")
	if await wait_for_world(world):
		for z in [-17, -16, -1, 0, 15, 16, 17]:
			for x in [-17, -16, -1, 0, 15, 16, 17]:
				var c: Dictionary = world.sample_terrain_column(Vector2i(x, z))
				check(c.biome_name == "ocean", "Dry coast failed to join the ocean.")
				check(c.height == c.water_level + int(coast.world.wet_coast_offset) and c.surface_water, "Dry ocean depth/water incorrect.")
				check(world.get_block_type_at(Vector3(x, c.height + 1, z)) == block_id("water"), "Dry coast ocean was not filled with water.")
	var inland := data.duplicate(true)
	inland.world.coast_start = -1
	inland.world.dry_coast_start = -1
	inland.world.coast_span = 0.001
	inland.world.dry_coast_span = 0.001
	inland.world.sea_level = 0
	var dry := biome("desert")
	var temperate := biome("mountains")
	var dry_boundary := float(dry.selection.climate_min)
	inland.biomes = [temperate, dry]
	inland.biomes[0].selection.climate_min = 0
	inland.biomes[0].selection.climate_max = dry_boundary
	for b in inland.biomes:
		b.trees = {}
		b.vegetation = {}
	write_config("user://transition_inland.json", inland)
	var land := make_world("user://transition_inland.json", Vector3(0, 32, 0), "Material transition test")
	await wait_for_world(land)
	var mixed := {"sand_before": false, "dirt_after": false, "grass_after": false, "dirt_before": false}
	var grass := block_id(temperate.materials.surface)
	var dirt := block_id(temperate.surface_overrides[0].surface)
	var grass_boundary: float = inland.biomes[0].surface_overrides[0].climate_min
	for z in range(-2048, 2049, 16):
		for x in range(-2048, 2049, 16):
			var c: Dictionary = land.sample_terrain_column(Vector2i(x, z))
			if c.climate < dry_boundary and c.biome_name == "desert": mixed.sand_before = true
			if c.climate >= dry_boundary and c.biome_name == "mountains": mixed.dirt_after = true
			if c.climate >= grass_boundary and c.surface_block == grass: mixed.grass_after = true
			if c.climate < grass_boundary and c.surface_block == dirt: mixed.dirt_before = true
			check(c == land.sample_terrain_column(Vector2i(x, z)), "Material transition sampling is not deterministic.")
	for key in mixed:
		check(mixed[key], "Missing material mixture across climate boundary: " + key)
	for frame in range(60):
		await process_frame
	print("Terrain transition world checks finished: ", failures, " failures.")
	quit(1 if failures else 0)
