extends "res://tests/support/test_case.gd"

func wait_idle(world: Node) -> bool:
	var deadline := Time.get_ticks_msec() + 30000
	while Time.get_ticks_msec() < deadline:
		await process_frame
		var stats: Dictionary = world.get_pipeline_stats()
		check(stats.models.inflight <= stats.models.max_inflight, "Model inflight limit exceeded")
		check(stats.meshes.inflight <= stats.meshes.max_inflight, "Mesh inflight limit exceeded")
		if stats.models.pending == 0 and stats.models.inflight == 0 and stats.meshes.pending == 0 and stats.meshes.inflight == 0:
			return true
	check(false, "Pipeline did not drain: " + str(world.get_pipeline_stats()))
	return false

func torch_lights(world: Node) -> Array[Node]:
	return world.find_children("*", "OmniLight3D", true, false)

func run() -> void:
	var data: Dictionary = biome_registry()
	data.world.base_height = 32
	data.world.amplitude = 0
	data.world.sea_level = 0
	data.world.coast_start = -1
	data.biomes = [{"id": 51, "name": "pipeline_fixture", "selection": {"kind": "land"},
		"relief": {"anchor": 0, "ridge_amplitude": 0, "bias": 0},
		"materials": {"surface": "stone", "soil": "stone", "rock": "stone", "deep_rock": "deepslate"},
		"soil_depth": 3, "deep_rock_below_y": 24}]
	write_config("user://pipeline_fixture.json", data)
	var world: Node = ClassDB.instantiate("VoxelAPI")
	world.set_biome_registry_path("user://pipeline_fixture.json")
	root.add_child(world)
	world.set_render_settings({"render_distance": 4, "vertical_render_distance": 2})
	world.set_pipeline_settings({"batch_size": 2, "max_inflight": 8, "finalize_budget_ms": 1.0})
	world.set_focus_position(Vector3(8, 48, 8))
	var id := int(SaveService.create_world(42, "Pipeline regression"))
	world.start_world(id)
	await wait_for_world(world)
	await wait_idle(world)
	var stats: Dictionary = world.get_pipeline_stats()
	check(stats.models.peak_ready <= 8 and stats.meshes.peak_ready <= 8, "Ready queues exceed inflight bound")
	check(stats.models.tasks_submitted < stats.models.chunks_completed, "Batching did not amortize model submissions")
	check(stats.loaded_models > 245, "Visible chunks lack a generated data halo")
	var before: int = stats.meshes.tasks_submitted

	for frame in range(30): await process_frame
	check(world.get_pipeline_stats().meshes.tasks_submitted == before, "Idle world keeps rebuilding meshes")

	# A corner edit invalidates the face and AO neighbours. Repeated updates must
	# retain the last desired version while a previous snapshot is being meshed.
	var point := Vector3(31, 33, 31)
	var torch := block_id("torch")

	for iteration in range(12):
		world.set_block(point, torch if iteration % 2 == 0 else 0)
		await process_frame
	world.set_block(point, torch)
	await wait_idle(world)
	check(torch_lights(world).size() == 1, "Newest corner mesh lost its torch")
	if not torch_lights(world).is_empty():
		var light: Node = torch_lights(world)[0]
		var light_id: int = light.get_instance_id()
		var node_id: int = light.get_parent().get_instance_id()
		world.set_block(Vector3(30, 33, 31), block_id("stone"))
		await wait_idle(world)
		check(torch_lights(world).size() == 1, "Unrelated rebuild lost the torch")
		if not torch_lights(world).is_empty():
			check(torch_lights(world)[0].get_instance_id() == light_id, "Unchanged light was recreated")
			check(torch_lights(world)[0].get_parent().get_instance_id() == node_id, "Rebuild replaced its chunk node")

	world.start_world(id)

	for frame in range(3): await process_frame
	world.set_focus_position(Vector3(512, 48, 0))
	await process_frame
	world.set_focus_position(Vector3(8, 48, 8))
	await process_frame
	await wait_for_world(world)
	await wait_idle(world)
	check(world.get_block_type_at(point) == torch and torch_lights(world).size() == 1, "Reload lost saved edits")
	# Switch worlds while new streaming jobs are pending; no previous-world data
	# or lights may survive. Then destroy a world with pending jobs.
	world.set_focus_position(Vector3(512, 48, 0))
	await process_frame
	world.set_focus_position(Vector3(8, 48, 8))
	world.start_world(int(SaveService.create_world(99, "Pipeline replacement")))
	await wait_for_world(world)
	await wait_idle(world)
	check(world.get_block_type_at(point) == 0 and torch_lights(world).is_empty(), "Previous world contaminated its replacement")
	print("Pipeline stats: ", JSON.stringify(world.get_pipeline_stats()))
	world.set_focus_position(Vector3(-512, 48, 0))
	await process_frame
	world.free()
	print("Chunk pipeline tests: ", failures, " failures (limits, batching, halo, idle, latest revision, reuse, reload, pending teardown).")
	quit(1 if failures else 0)
