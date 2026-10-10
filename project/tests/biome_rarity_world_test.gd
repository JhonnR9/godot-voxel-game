extends "res://tests/support/test_case.gd"

func run() -> void:
	var data := biome_registry()
	var lands := biomes_of_kind("land")
	lands.sort_custom(func(a: Dictionary, b: Dictionary): return float(a.selection.get("climate_min", 0)) < float(b.selection.get("climate_min", 0)))
	check(lands.size() >= 2, "Rarity comparison requires two configured land profiles.")
	if lands.size() < 2:
		quit(1)
		return
	var target: Dictionary = lands.back()
	var fallback: Dictionary = lands.front()
	var levels := rarity_levels(int(target.get("rarity", 1)))
	var previous: Dictionary = {}
	var baseline := 0

	for rarity in levels:
		var config := data.duplicate(true)
		var preferred := target.duplicate(true)
		var guaranteed := fallback.duplicate(true)
		var boundary := float(target.selection.get("climate_min", 0.5))
		guaranteed.selection = {"kind": "land", "climate_min": 0, "climate_max": boundary}
		preferred.selection = {"kind": "land", "climate_min": boundary, "climate_max": 1}
		guaranteed.rarity = 1
		preferred.rarity = rarity
		# Controlled flat terrain isolates rarity without copying production palettes.
		config.biomes = [guaranteed, preferred]
		config.world.base_height = int(data.world.base_height)
		config.world.sea_level = min(0, int(data.world.base_height) - 1)
		config.world.amplitude = 0
		config.world.climate_start = -1
		config.world.climate_span = 0.001
		config.world.coast_start = -1
		config.world.dry_coast_start = -1
		config.world.coast_span = 0.001
		config.world.dry_coast_span = 0.001
		for entry: Dictionary in config.biomes:
			entry.relief.ridge_amplitude = 0
			entry.relief.bias = 0
			entry.trees = {}
			entry.vegetation = {}
		preferred.relief.bias = 128 if rarity == 0 else 0
		var path := "user://rarity_%s.json" % rarity
		write_config(path, config)
		var height := int(config.world.base_height)
		var world := make_world(path, Vector3(0, height + 6, 0), "Rarity %s" % rarity)
		if not await wait_for_world(world):
			world.free()
			continue
		var selected := {}
		var samples := 0
		for z in range(-4096, 4097, 64):
			for x in range(-4096, 4097, 64):
				var point := Vector2i(x, z)
				var column: Dictionary = world.sample_terrain_column(point)
				check(column == world.sample_terrain_column(point), "Rarity sampling is not deterministic.")
				check(column.height == height, "Disabled relief leaked into flat terrain.")
				check(column.biome_id in [int(preferred.id), int(guaranteed.id)], "Unexpected fallback ID.")
				if int(column.biome_id) == int(preferred.id): selected[point] = true
				samples += 1
		if rarity == 0: check(selected.is_empty(), "Disabled biome appeared.")
		elif rarity == 1:
			check(selected.size() == samples, "Guaranteed biome did not cover forced climate.")
			baseline = selected.size()
		else:
			check(selected.size() <= previous.size(), "Increasing rarity increased population.")
			for point in selected: check(previous.has(point), "Increasing rarity introduced a new region.")
			if rarity <= 4: check(selected.size() > 0 and selected.size() < baseline, "Moderate rarity did not produce sparse regions.")
		for z in [-17, -16, -1, 0, 15, 16, 17]:
			for x in [-17, -16, -1, 0, 15, 16, 17]:
				var column: Dictionary = world.sample_terrain_column(Vector2i(x, z))
				check(world.get_block_type_at(Vector3(x, column.height, z)) == column.surface_block, "Rarity sampler/generation disagreement.")
		print("Registry biome ", target.name, " (ID ", target.id, "), rarity ", rarity, ": ", selected.size(), "/", samples, " samples.")
		previous = selected
		world.free()
	print("Biome rarity tests: ", failures, " failures.")
	quit(1 if failures else 0)
