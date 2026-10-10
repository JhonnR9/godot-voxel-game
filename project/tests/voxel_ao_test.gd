extends "res://tests/support/test_case.gd"

# TEST_REQUIRES_RENDERER
# Run with an active renderer (without --headless).
# Exercise the generated chunk mesh, including all face orientations and a
# three-axis chunk boundary. No renderer-dependent screen-space AO is needed.
func corner_visibility(world: Node, point: Vector3, normal: Vector3) -> float:
	for child in world.get_children():
		if not child is MeshInstance3D or child.mesh == null:
			continue
		var local: Vector3 = point - child.position
		if local.x < 0 or local.x > 32 or local.y < 0 or local.y > 64 or local.z < 0 or local.z > 32:
			continue
		for surface in child.mesh.get_surface_count():
			var arrays: Array = child.mesh.surface_get_arrays(surface)
			var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
			var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
			var colors: PackedColorArray = arrays[Mesh.ARRAY_COLOR]
			for i in vertices.size():
				if vertices[i].is_equal_approx(local) and normals[i].dot(normal) > 0.99:
					return colors[i].a
	return -1.0

func wait_corner(world: Node, point: Vector3, normal: Vector3, expected: float) -> void:
	var deadline := Time.get_ticks_msec() + 5000
	while Time.get_ticks_msec() < deadline:
		var actual := corner_visibility(world, point, normal)
		if absf(actual - expected) < 0.01:
			return
		await process_frame
	check(false, "AO at %s normal %s: expected %s, got %s" % [point, normal, expected, corner_visibility(world, point, normal)])

func run() -> void:
	if DisplayServer.get_name() == "headless":
		push_error("Voxel AO mesh inspection requires an active renderer; omit --headless.")
		quit(1)
		return
	var data: Dictionary = biome_registry()
	data.world.base_height = 32
	data.world.amplitude = 0
	data.world.sea_level = 0
	data.world.coast_start = -1
	data.biomes = [{
		"id": 51, "name": "ao_fixture", "selection": {"kind": "land"},
		"relief": {"anchor": 0, "ridge_amplitude": 0, "bias": 0},
		"materials": {"surface": "stone", "soil": "stone", "rock": "stone", "deep_rock": "deepslate"},
		"soil_depth": 3, "deep_rock_below_y": 24
	}]
	write_config("user://ao_fixture.json", data)
	var world := make_world("user://ao_fixture.json", Vector3(8, 48, 8), "Voxel AO fixture")
	if not await wait_for_world(world):
		quit(1)
		return
	var stone := block_id("stone")
	var directions := [Vector3.RIGHT, Vector3.LEFT, Vector3.UP, Vector3.DOWN, Vector3.BACK, Vector3.FORWARD]
	for i in directions.size():
		var normal: Vector3 = directions[i]
		var u := Vector3.FORWARD if normal.x != 0 or normal.y != 0 else Vector3.RIGHT
		var v := Vector3.RIGHT if normal.y != 0 else Vector3.UP
		# Positive tangent axes keep expected corner coordinates unambiguous.
		u = u.abs()
		var base := Vector3(4 + i * 4, 50, 4)
		world.set_block(base, stone)
		world.set_block(base + normal + u, stone)
		world.set_block(base + normal + v, stone)
		var face_origin := base + normal.max(Vector3.ZERO)
		await wait_corner(world, face_origin + u + v, normal, 0.0)
		await wait_corner(world, face_origin, normal, 1.0)
	# This corner needs a diagonal chunk across X, Y and Z simultaneously.
	var base := Vector3(31, 63, 31)
	var diagonal := Vector3(32, 64, 32)
	world.set_block(base, stone)
	world.set_block(diagonal, stone)
	await wait_corner(world, diagonal, Vector3.UP, 2.0 / 3.0)
	world.break_block(diagonal)
	await wait_corner(world, diagonal, Vector3.UP, 1.0)
	for transparent_name in ["water", "oak_leaves"]:
		world.set_block(diagonal, block_id(transparent_name))
		for frame in range(30): await process_frame
		await wait_corner(world, diagonal, Vector3.UP, 1.0)
		world.break_block(diagonal)
	# Isolated cubes remain unoccluded; large open planes retain greedy meshing.
	var floor_quads := 0
	for child in world.get_children():
		if not child is MeshInstance3D or child.mesh == null or child.position != Vector3.ZERO:
			continue
		var arrays: Array = child.mesh.surface_get_arrays(0)
		var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
		for i in range(0, vertices.size(), 4):
			if normals[i].y > 0.99 and is_equal_approx(vertices[i].y, 33.0):
				floor_quads += 1
	check(floor_quads == 1, "Unoccluded flat chunk should still merge to one top quad, got %s" % floor_quads)
	for frame in range(60): await process_frame
	print("Voxel AO tests: ", failures, " failures (six faces, diagonal chunk edits, transparent blocks, greedy merging).")
	quit(1 if failures else 0)
