extends "res://tests/support/test_case.gd"

func _mask(color: Color, kind: String) -> float:
	var linear := color.srgb_to_linear()
	return smoothstep(0.045, 0.16, linear.r - linear.b) if kind == "iron_ore" else smoothstep(0.025, 0.15, minf(linear.g, linear.b) - linear.r)

func run() -> void:
	for kind: String in ["iron_ore", "diamond_ore"]:
		var texture := load("res://textures/blocks/" + kind + ".png") as Texture2D
		var image := texture.get_image()
		assert(image.get_width() > 0 and image.get_height() > 0)
		var total := image.get_width() * image.get_height()
		var emitting := 0
		var neutral := 0
		for y in range(image.get_height()):
			for x in range(image.get_width()):
				var color := image.get_pixel(x, y)
				var mask := _mask(color, kind)
				if mask > 0.05:
					emitting += 1
				if absf(color.r - color.g) < 0.015 and absf(color.g - color.b) < 0.015:
					neutral += 1
					assert(mask == 0.0)
		assert(emitting > total * 0.03 and emitting < total * 0.4)
		assert(neutral > total * 0.1)
		print(kind, ": ", emitting, "/", total, " inclusion pixels emit")
	# Host rock and coal must remain non-emissive under either analytical mask.
	for name: String in ["stone", "coal_ore"]:
		var image: Image = load("res://textures/blocks/" + name + ".png").get_image()
		var emitting := 0
		for y in range(image.get_height()):
			for x in range(image.get_width()):
				var color := image.get_pixel(x, y)
				if _mask(color, "iron_ore") > 0.05 or _mask(color, "diamond_ore") > 0.05:
					emitting += 1
		assert(emitting < image.get_width() * image.get_height() * 0.01)
	var registry: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://data/block_registry.generated.json"))
	# Additional block types may extend the registry without changing ore IDs.
	assert(block_id("coal_ore") > 0, "Coal ore must be registered.")
	for block: Dictionary in registry.blocks:
		for layer: int in block.get("texture_layers", {}).values():
			assert(layer >= 0 and layer < int(registry.water_texture_layer))
	print("Ore emission tests passed: colored inclusions, neutral rock, coal, atlas bounds.")
	quit()
