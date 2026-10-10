extends SceneTree

# Shared test infrastructure; no concrete test inherits another test's run().
var failures := 0
var _biome_registry: Dictionary = {}
var _block_registry: Dictionary = {}

func check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		push_error(message)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	push_error("Test cases must implement run().")
	quit(1)

func read_registry(path: String) -> Dictionary:
	var parser := JSON.new()
	var error := parser.parse(FileAccess.get_file_as_string(path))
	if error != OK or not parser.data is Dictionary:
		check(false, "Cannot read registry %s: %s" % [path, parser.get_error_message()])
		quit(1)
		return {}
	return parser.data

func biome_registry() -> Dictionary:
	if _biome_registry.is_empty():
		_biome_registry = read_registry("res://data/biome_registry.json")
	return _biome_registry.duplicate(true)

func block_registry() -> Dictionary:
	if _block_registry.is_empty():
		_block_registry = read_registry("res://data/block_registry.generated.json")
	return _block_registry.duplicate(true)

func biome(name: String) -> Dictionary:
	for entry: Dictionary in biome_registry().get("biomes", []):
		if entry.name == name:
			return entry
	check(false, "Missing required biome profile: " + name)
	return {}

func biomes_of_kind(kind: String) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	for entry: Dictionary in biome_registry().get("biomes", []):
		if entry.get("selection", {}).get("kind", "land") == kind:
			result.append(entry)
	return result

func block_id(name: String) -> int:
	for entry: Dictionary in block_registry().get("blocks", []):
		if entry.name == name:
			return int(entry.id)
	check(false, "Missing required block: " + name)
	return -1

func blocks_with_flag(flag: String) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	for entry: Dictionary in block_registry().get("blocks", []):
		if entry.get("flags", []).has(flag):
			result.append(entry)
	return result

func profile_block_ids(profile: Dictionary, keys: Array = ["plants", "flowers"]) -> Array[int]:
	var result: Array[int] = []
	for key: String in keys:
		for entry: Dictionary in profile.get(key, []):
			var id := block_id(entry.block)
			if not result.has(id):
				result.append(id)
	return result

func test_seeds() -> Array[int]:
	var result: Array[int] = []
	var configured := OS.get_environment("TEST_SEEDS")
	for value in (configured if not configured.is_empty() else "42,1234,2026").split(","):
		result.append(int(value))
	return result

func test_seed() -> int:
	return test_seeds()[0]

func rarity_levels(configured: int) -> Array[int]:
	var result: Array[int] = [0, 1]
	for value in [configured, mini(10000, maxi(1, configured) * 2), mini(10000, maxi(1, configured) * 4)]:
		if not result.has(value):
			result.append(value)
	result.sort()
	return result

func wait_for_world(world: Node) -> bool:
	var deadline := Time.get_ticks_msec() + 20000
	while world.is_initial_loading() and Time.get_ticks_msec() < deadline:
		await process_frame
	check(not world.is_initial_loading(), "Terrain chunks did not load in time.")
	return not world.is_initial_loading()

func make_world(config_path: String, focus: Vector3, name: String, seed: Variant = null) -> Node:
	var world: Node = ClassDB.instantiate("VoxelAPI")
	world.set_biome_registry_path(config_path)
	root.add_child(world)
	world.set_render_settings({"render_distance": 4, "vertical_render_distance": 2})
	world.set_focus_position(focus)
	world.start_world(int(SaveService.create_world(test_seed() if seed == null else int(seed), name)))
	return world

func write_config(path: String, data: Dictionary) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	check(file != null, "Could not write test config.")
	if file:
		file.store_string(JSON.stringify(data))
		file.close()
