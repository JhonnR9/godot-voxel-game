extends "res://tests/support/test_case.gd"

const Cycle = preload("res://scripts/directional_light_3d.gd")

func run() -> void:
	var scene := Node3D.new()
	var world_environment := WorldEnvironment.new()
	world_environment.name = "WorldEnvironment"
	var original := Environment.new()
	original.sky = Sky.new()
	original.sky.sky_material = ProceduralSkyMaterial.new()
	world_environment.environment = original
	scene.add_child(world_environment)
	var sun := DirectionalLight3D.new()
	sun.set_script(Cycle)
	scene.add_child(sun)
	root.add_child(scene)
	assert(is_equal_approx(sun.hora, 8.0))
	assert(world_environment.environment != original)
	var environment := world_environment.environment
	sun.set_hour(12.0)
	var noon_energy: float = sun.light_energy
	var noon_ambient: float = environment.ambient_light_energy
	var noon_sky: Color = environment.sky.sky_material.sky_top_color
	assert((-sun.global_basis.z).y < -0.99)
	sun.set_hour(0.0)
	assert(sun.light_energy == 0.0)
	assert(environment.ambient_light_energy < noon_ambient * 0.07)
	assert(environment.sky.sky_material.sky_top_color.get_luminance() < noon_sky.get_luminance() * 0.15)
	assert(sun._moon.light_energy > 0 and sun._moon.light_energy < 0.02)
	assert(not sun._moon.shadow_enabled)
	assert(noon_energy > 1.0)
	sun.set_hour(6.0)
	sun._process(780.0)
	assert(is_equal_approx(sun.hora, 18.0))
	sun._process(420.0)
	assert(is_equal_approx(sun.hora, 6.0))
	sun.set_hour(8.0)
	var previous_angle: float = sun.rotation_degrees.x
	sun._process(1.0 / 60.0)
	assert(sun.rotation_degrees.x < previous_angle)
	sun.set_hour(8.0)
	sun._process(1200.0 * 3)
	assert(is_equal_approx(sun.hora, 8.0))
	# Twilight is continuous on either side of the horizon, with no hard flash.
	sun.set_hour(17.99)
	var before: float = environment.ambient_light_energy
	sun.set_hour(18.01)
	assert(absf(environment.ambient_light_energy - before) < 0.02)
	scene.free()
	print("Day/night tests passed: starting hour, sun direction, darkness, durations, wrap, twilight.")
	quit()
