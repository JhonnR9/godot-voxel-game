extends Node

const REGISTRY_PATH := "res://data/block_registry.generated.json"
const CACHE_PATH := "user://cache/block_icons"
const ICON_SIZE := 64
# Bump whenever projection, shading, sampling or fallback art changes.
const GENERATOR_VERSION := "cube-reference-v1"

var _icons: Dictionary = {}
var _source_images: Dictionary = {}
var last_run := {"generated": 0, "loaded": 0}

func _ready() -> void:
	var file := FileAccess.open(REGISTRY_PATH, FileAccess.READ)
	if file == null:
		push_error("Cannot prepare block icons: missing block registry.")
		return
	var data: Variant = JSON.parse_string(file.get_as_text())
	if data is Dictionary and data.get("blocks") is Array:
		prepare(data.blocks)
	else:
		push_error("Cannot prepare block icons: invalid block registry.")

func get_icon(block_id: int) -> Texture2D:
	return _icons.get(block_id)

func _resource_hash(path: String) -> String:
	# Exported text resources may be converted into binary resources.
	var import_config := ConfigFile.new()
	if import_config.load(path + ".remap") == OK or import_config.load(path + ".import") == OK:
		path = str(import_config.get_value("remap", "path", path))
	return FileAccess.get_sha256(path)

func prepare(blocks: Array, cache_path: String = CACHE_PATH) -> Dictionary:
	_icons.clear()
	_source_images.clear()
	last_run = {"generated": 0, "loaded": 0}
	var writable := DirAccess.make_dir_recursive_absolute(cache_path) == OK
	if not writable:
		push_warning("Block icon cache is not writable; icons will stay in memory.")
	var manifest := ConfigFile.new()
	manifest.load(cache_path.path_join("manifest.cfg"))
	var source_hash := ""
	var texture_keys: Dictionary = {}

	for block: Dictionary in blocks:
		for key: String in block.get("textures", {}).values():
			texture_keys[key] = true
	var sorted_keys := texture_keys.keys()
	sorted_keys.sort()

	for key: String in sorted_keys:
		source_hash += key + _resource_hash(_texture_path(key))
	var updated := false

	for entry: Variant in blocks:
		if not entry is Dictionary or int(entry.get("id", 0)) == 0:
			continue
		var block: Dictionary = entry
		var id := int(block.id)
		var key := str(id)
		var signature := (GENERATOR_VERSION + source_hash + JSON.stringify(block)).sha256_text()
		var path := cache_path.path_join(key + ".png")
		var image := Image.new()
		var cached := false
		if manifest.get_value("icons", key, "") == signature and FileAccess.file_exists(path):
			cached = image.load(path) == OK and image.get_size() == Vector2i(ICON_SIZE, ICON_SIZE)
		if cached:
			last_run.loaded += 1
		else:
			image = _render_icon(block)
			last_run.generated += 1
			# Never commit the signature until its PNG has been saved successfully.
			if writable and image.save_png(path + ".tmp") == OK:
				if DirAccess.rename_absolute(path + ".tmp", path) == OK:
					manifest.set_value("icons", key, signature)
					updated = true
				else:
					push_warning("Could not commit cached block icon: " + path)
			elif writable:
				push_warning("Could not save cached block icon: " + path)
		_icons[id] = ImageTexture.create_from_image(image)
	if updated:
		var temporary := cache_path.path_join("manifest.cfg.tmp")
		if manifest.save(temporary) != OK or DirAccess.rename_absolute(temporary, cache_path.path_join("manifest.cfg")) != OK:
			push_warning("Could not save block icon cache manifest.")
	# Source pixels are only needed on cache misses.
	_source_images.clear()
	return last_run.duplicate()

func _texture_path(key: String) -> String:
	for extension: String in ["png", "jpg", "jpeg", "webp"]:
		var path := "res://textures/blocks/" + key + "." + extension
		if ResourceLoader.exists(path):
			return path
	return ""

func _face_image(block: Dictionary, face: String) -> Image:
	var textures: Dictionary = block.get("textures", {})
	var key := str(textures.get(face, textures.get("side", textures.get("top", ""))))
	if _source_images.has(key):
		return _source_images[key]
	var image: Image
	var path := _texture_path(key) if not key.is_empty() else ""
	if not path.is_empty():
		var texture := load(path) as Texture2D
		if texture != null:
			image = texture.get_image()
	if image == null or image.is_empty():
		# Water has a procedural shader instead of a registered texture.
		image = Image.create(1, 1, false, Image.FORMAT_RGBA8)
		image.fill(Color(0.16, 0.48, 0.85, 0.9) if str(block.get("name")) == "water" else Color.MAGENTA)
		if not key.is_empty():
			push_warning("Missing source texture for block icon: " + key)
	if image.is_compressed():
		image.decompress()
	_source_images[key] = image
	return image

func _render_icon(block: Dictionary) -> Image:
	var image := Image.create(ICON_SIZE, ICON_SIZE, false, Image.FORMAT_RGBA8)
	image.fill(Color.TRANSPARENT)
	var values: Array = block.get("tint", [1.0, 1.0, 1.0, 1.0])
	var tint := Color(float(values[0]), float(values[1]), float(values[2]), float(values[3]))
	var side := _face_image(block, "side")
	if block.get("flags", []).has("crossed"):
		# Flowers and grass are sprites in the world, so retain their silhouette.
		_paint_face(image, side, Vector2(8, 8), Vector2(48, 0), Vector2(0, 48), tint, 1.0)
		return image
	# Projection follows visual_bugs/images.png: upper back corner, left and
	# right shoulders, then the front corner. Bottom vertices are 29px lower.
	_paint_face(image, _face_image(block, "top"), Vector2(30, 5), Vector2(28, 12), Vector2(-23, 13), tint, 1.0)
	_paint_face(image, side, Vector2(7, 18), Vector2(28, 12), Vector2(0, 29), tint, 0.72)
	_paint_face(image, side, Vector2(35, 30), Vector2(23, -13), Vector2(0, 29), tint, 0.90)
	return image

func _paint_face(target: Image, source: Image, origin: Vector2, u_axis: Vector2, v_axis: Vector2, tint: Color, shade: float) -> void:
	var determinant := u_axis.cross(v_axis)

	for y in range(ICON_SIZE):
		for x in range(ICON_SIZE):
			var offset := Vector2(x + 0.5, y + 0.5) - origin
			var u := offset.cross(v_axis) / determinant
			var v := u_axis.cross(offset) / determinant
			if u < 0.0 or u >= 1.0 or v < 0.0 or v >= 1.0:
				continue
			var pixel := source.get_pixel(mini(int(u * source.get_width()), source.get_width() - 1), mini(int(v * source.get_height()), source.get_height() - 1))
			pixel *= tint
			pixel.r *= shade
			pixel.g *= shade
			pixel.b *= shade
			if pixel.a > 0.0:
				target.set_pixel(x, y, target.get_pixel(x, y).blend(pixel))
