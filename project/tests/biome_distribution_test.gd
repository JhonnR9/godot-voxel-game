extends "res://tests/support/test_case.gd"

func run() -> void:
	var data := biome_registry()
	var profiles := {}
	var palettes := {}
	for entry: Dictionary in data.biomes:
		profiles[int(entry.id)] = entry
		var allowed: Array[int] = [block_id(entry.materials.surface)]
		for override: Dictionary in entry.get("surface_overrides", []):
			allowed.append(block_id(override.surface))
		palettes[int(entry.id)] = allowed
	for seed in test_seeds():
		var world := make_world("res://data/biome_registry.json", Vector3(0, int(data.world.base_height) + 6, 0), "Registry distribution %s" % seed, seed)
		if not await wait_for_world(world):
			world.free()
			continue
		var counts := {}
		for entry: Dictionary in data.biomes: counts[entry.name] = 0
		var total := 0
		for z in range(-8192, 8193, 128):
			for x in range(-8192, 8193, 128):
				var column: Dictionary = world.sample_terrain_column(Vector2i(x, z))
				check(profiles.has(int(column.biome_id)), "Sample selected an unregistered biome ID.")
				if not profiles.has(int(column.biome_id)): continue
				var profile: Dictionary = profiles[int(column.biome_id)]
				check(column.biome_name == profile.name, "Biome ID/name mapping differs from registry.")
				check(int(profile.get("rarity", 1)) > 0, "Disabled biome was selected.")
				check(palettes[int(column.biome_id)].has(int(column.surface_block)), "Surface is outside registered palette.")
				var selection: Dictionary = profile.selection
				# Palette transition patches can perturb climate by up to 0.10.
				check(float(column.climate) >= float(selection.get("climate_min", 0)) - 0.10 - 0.0001 and float(column.climate) <= float(selection.get("climate_max", 1)) + 0.10 + 0.0001, "Selected biome lies outside its configured climate transition.")
				counts[profile.name] += 1
				total += 1
		check(total > 0, "No distribution samples were collected.")
		for entry: Dictionary in data.biomes:
			if int(entry.get("rarity", 1)) == 0: check(counts[entry.name] == 0, "Disabled biome appeared in distribution.")
		# Repeat representative samples to assert stability without frozen population totals.
		for point in [Vector2i.ZERO, Vector2i(-4096, 4096), Vector2i(8192, -8192)]:
			check(world.sample_terrain_column(point) == world.sample_terrain_column(point), "Distribution sampling is not deterministic.")
		print("Seed ", seed, ": ", total, " samples; configured biome counts ", counts)
		world.free()
	print("Biome distribution tests: ", failures, " failures.")
	quit(1 if failures else 0)
