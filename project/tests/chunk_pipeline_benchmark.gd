extends "res://tests/support/test_case.gd"

func run() -> void:
	var rows: Array[Dictionary] = []

	for batch in [1, 2, 4, 8]:
		for trial in range(3):
			var world: Node = ClassDB.instantiate("VoxelAPI")
			root.add_child(world)
			world.set_render_settings({"render_distance": 4, "vertical_render_distance": 2})
			world.set_pipeline_settings({"batch_size": batch, "max_inflight": 8, "finalize_budget_ms": 2.0})
			world.set_focus_position(Vector3(8, 48, 8))
			var id := int(SaveService.create_world(12345, "Batch benchmark %d %d" % [batch, trial]))
			var start := Time.get_ticks_usec()
			world.start_world(id)
			var stats: Dictionary
			var initial_ms := -1.0
			while Time.get_ticks_usec() - start < 30000000:
				await process_frame
				if initial_ms < 0 and not world.is_initial_loading():
					initial_ms = (Time.get_ticks_usec() - start) / 1000.0
				stats = world.get_pipeline_stats()
				if initial_ms >= 0 and stats.models.pending == 0 and stats.models.inflight == 0 and stats.meshes.pending == 0 and stats.meshes.inflight == 0:
					break
			if stats.models.pending != 0 or stats.models.inflight != 0 or stats.meshes.pending != 0 or stats.meshes.inflight != 0 or initial_ms < 0:
				push_error("Benchmark did not drain: " + str(stats))
				world.free()
				quit(1)
				return
			rows.append({"batch": batch, "trial": trial, "initial_ms": initial_ms,
				"drain_ms": (Time.get_ticks_usec() - start) / 1000.0, "stats": stats})
			world.free()

	for frame in range(2): await process_frame
	print("CHUNK_BATCH_BENCHMARK=", JSON.stringify(rows))
	quit(0)
