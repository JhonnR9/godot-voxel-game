extends "res://tests/support/test_case.gd"

func run() -> void:
	var data: Dictionary = biome_registry()
	# Force a full ocean while retaining the real seeded climate field.
	data.world.coast_start = 1
	data.world.coast_span = 0.001
	data.world.dry_coast_start = 1
	data.world.dry_coast_span = 0.001

	for biome in data.biomes:
		biome.trees = {}
		biome.vegetation = {}
	write_config("user://ice_transition.json", data)
	var climate := FastNoiseLite.new()
	climate.noise_type = FastNoiseLite.TYPE_SIMPLEX
	climate.seed = test_seed() + 2
	climate.frequency = data.world.noises.climate.frequency
	climate.fractal_octaves = data.world.noises.climate.octaves
	var freezing_boundary := float(biome("frozen_ocean").selection.climate_max)
	var focus := Vector2i.ZERO
	var found := false

	for z in range(-4096, 4097, 32):
		for x in range(-4096, 4097, 32):
			var weight := smoothstep(0.0, 1.0, (climate.get_noise_2d(x, z) - data.world.climate_start) / data.world.climate_span)
			if absf(weight - freezing_boundary) < 0.002:
				focus = Vector2i(x, z)
				found = true
				break
		if found: break
	check(found, "No freezing boundary found.")
	var world := make_world("user://ice_transition.json", Vector3(focus.x, 24, focus.y), "Gradual ice transition")
	var ice := block_id("ice")
	var water := block_id("water")
	var ice_columns := 0
	var water_columns := 0
	var terraces := 0
	var heights := {}
	if await wait_for_world(world):
		for z in range(focus.y - 16, focus.y + 17):
			for x in range(focus.x - 16, focus.x + 17):
				var c: Dictionary = world.sample_terrain_column(Vector2i(x, z))
				check(c == world.sample_terrain_column(Vector2i(x, z)), "Ice transition is not deterministic.")
				if c.surface_fill == ice:
					ice_columns += 1
					if c.solid_fill_height < c.water_level - 1: terraces += 1
					heights[c.solid_fill_height] = true
				else: water_columns += 1
				var neighbor: Dictionary = world.sample_terrain_column(Vector2i(x + 1, z))
				check(abs(c.solid_fill_height - neighbor.solid_fill_height) <= 2, "Ice terrace has a steep neighboring wall.")
				for y in range(c.height + 1, c.water_level):
					var expected: int = water if y > c.solid_fill_height else c.surface_fill
					check(world.get_block_type_at(Vector3(x, y, z)) == expected, "Ice terrace has a gap, floating cap or missing water at %s" % Vector3i(x, y, z))
	check(ice_columns > 50 and terraces > 50, "Freezing boundary does not lower ice toward the bed.")
	check(heights.size() >= 3, "Ice edge does not form several terrace heights.")

	for frame in range(60): await process_frame
	print("Ice transition checks finished: ", failures, " failures; ", ice_columns, " ice / ", water_columns, " water columns.")
	quit(1 if failures else 0)
