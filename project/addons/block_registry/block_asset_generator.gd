@tool
extends RefCounted

const REGISTRY_PATH := "res://data/block_registry.json"
const TEXTURE_PATH: String = "res://textures/blocks/"
const ARRAY_PATH := "res://textures/block_array.tres"
const MAPPING_PATH := "res://textures/block_mapping.json"
const GENERATED_REGISTRY_PATH := "res://data/block_registry.generated.json"
const GENERATED_HEADER_PATH := "res://generated/block_registry.generated.h"

const FLAG_BITS := {
	"solid": 1 << 10,
	"transparent": 1 << 11,
	"emissive": 1 << 12,
	"waterlog": 1 << 13,
	"cutout": 1 << 14,
	"ocean": 1 << 15,
	"crossed": 1 << 16,
}
const CPP_KEYWORDS := ["alignas", "alignof", "and", "asm", "auto", "bool", "break", "case", "catch", "char", "class", "const", "constexpr", "continue", "default", "delete", "do", "double", "else", "enum", "explicit", "export", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long", "namespace", "new", "noexcept", "not", "nullptr", "operator", "or", "private", "protected", "public", "register", "return", "short", "signed", "sizeof", "static", "struct", "switch", "template", "this", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "while", "xor"]


func rebuild() -> bool:
	var registry_file := FileAccess.open(REGISTRY_PATH, FileAccess.READ)
	if registry_file == null:
		push_error("Could not open block registry: " + REGISTRY_PATH)
		return false
	var parsed: Variant = JSON.parse_string(registry_file.get_as_text())
	if not (parsed is Dictionary) or not (parsed.get("blocks", []) is Array):
		push_error("Block registry must contain a blocks array.")
		return false

	var blocks: Array = parsed.blocks.duplicate(true)
	blocks.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return int(a.get("id", -1)) < int(b.get("id", -1)))
	var validation := _validate_blocks(blocks)
	if not validation.is_empty():
		push_error(validation)
		return false

	var texture_names := _get_texture_names(blocks)
	var texture_images: Array[Image] = []
	var reference_size := Vector2i(-1, -1)

	for texture_name in texture_names:
		var path := _find_texture_path(texture_name)
		if path.is_empty():
			push_error("Missing registered block texture: " + texture_name)
			return false
		var image := Image.new()
		if image.load(path) != OK:
			push_error("Could not load image: " + path)
			return false
		image.clear_mipmaps()
		image.convert(Image.FORMAT_RGBA8)
		if reference_size == Vector2i(-1, -1):
			reference_size = image.get_size()
		elif image.get_size() != reference_size:
			push_error("All block textures must use the same dimensions. Mismatch: " + path)
			return false
		if image.generate_mipmaps() != OK:
			push_error("Could not generate block texture mipmaps: " + path)
			return false
		texture_images.append(image)

	if texture_images.is_empty():
		push_error("Register at least one texture before generating the block assets.")
		return false

	var texture_layers: Dictionary = {}

	for index in range(texture_names.size()):
		texture_layers[texture_names[index]] = index

	var generated_blocks: Array[Dictionary] = []

	for block: Dictionary in blocks:
		var textures: Dictionary = block.get("textures", {})
		var side := str(textures.get("side", ""))
		var top := str(textures.get("top", side))
		var bottom := str(textures.get("bottom", side))
		var face_layers: Dictionary = {}
		if not side.is_empty():
			face_layers["side"] = int(texture_layers[side])
		if not top.is_empty():
			face_layers["top"] = int(texture_layers[top])
		if not bottom.is_empty():
			face_layers["bottom"] = int(texture_layers[bottom])
		var flags: Array = block.get("flags", [])
		generated_blocks.append({
			"id": int(block.id),
			"name": str(block.name),
			"display_name": str(block.get("display_name", block.name)),
			"category": str(block.get("category", "misc")),
			"flags": flags,
			"flag_mask": _flag_mask(flags),
			"textures": textures,
			"texture_layers": face_layers,
			"tint": block.get("tint", [1.0, 1.0, 1.0, 1.0]),
		})

	var generated_registry := {
		"version": 1,
		"water_texture_layer": texture_images.size(),
		"texture_layers": texture_layers,
		"blocks": generated_blocks,
	}
	if not _write_json(MAPPING_PATH, texture_layers):
		return false
	if not _write_json(GENERATED_REGISTRY_PATH, generated_registry):
		return false
	if not _write_header(generated_blocks, texture_images.size(), texture_layers):
		return false
	return _write_texture_array(texture_images)


func _validate_blocks(blocks: Array) -> String:
	var ids := {}
	var names := {}
	var name_regex := RegEx.new()
	name_regex.compile("^[a-z][a-z0-9_]*$")

	for block: Dictionary in blocks:
		var id := int(block.get("id", -1))
		var name := str(block.get("name", ""))
		if id < 0 or id > 1023:
			return "Block ID must be between 0 and 1023: " + name
		if ids.has(id):
			return "Duplicate block ID: %d" % id
		if name_regex.search(name) == null:
			return "Block names must use lowercase letters, numbers, and underscores: " + name
		if CPP_KEYWORDS.has(name):
			return "Block names cannot be C++ keywords: " + name
		if names.has(name):
			return "Duplicate block name: " + name
		if id == 0 and name != "air":
			return "Block ID 0 is reserved for air."
		ids[id] = true
		names[name] = true
		for flag in block.get("flags", []):
			if not FLAG_BITS.has(str(flag)):
				return "Unknown flag '%s' on block '%s'." % [flag, name]
		var textures: Dictionary = block.get("textures", {})
		if id != 0 and not block.get("flags", []).has("transparent") and str(textures.get("side", "")).is_empty():
			return "Block '%s' needs a side texture key." % name
		var tint: Variant = block.get("tint", [1.0, 1.0, 1.0, 1.0])
		if not (tint is Array) or tint.size() != 4:
			return "Tint must contain four color values: " + name
		for texture_name in textures.values():
			if str(texture_name).is_empty():
				continue
			if _find_texture_path(str(texture_name)).is_empty():
				return "Texture '%s' referenced by '%s' was not found in %s" % [texture_name, name, TEXTURE_PATH]
	if not ids.has(0) or not names.has("air"):
		return "Block ID 0 named 'air' is required."
	return ""


func _get_texture_names(blocks: Array) -> Array[String]:
	var names: Array[String] = []

	for block: Dictionary in blocks:
		for texture_name in block.get("textures", {}).values():
			var key := str(texture_name)
			if not key.is_empty() and not names.has(key):
				names.append(key)
	names.sort()
	return names


func _find_texture_path(texture_name: String) -> String:
	for extension in ["png", "jpg"]:
		var path = TEXTURE_PATH + texture_name + "." + extension
		if FileAccess.file_exists(path):
			return path
	return ""


func _flag_mask(flags: Array) -> int:
	var result := 0

	for flag in flags:
		result |= int(FLAG_BITS.get(str(flag), 0))
	return result


func _write_json(path: String, value: Variant) -> bool:
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		push_error("Could not write generated file: " + path)
		return false
	file.store_string(JSON.stringify(value, "\t") + "\n")
	return true


func _write_header(blocks: Array[Dictionary], water_layer: int, texture_layers: Dictionary) -> bool:
	var output := "#ifndef BLOCK_REGISTRY_GENERATED_H\n#define BLOCK_REGISTRY_GENERATED_H\n\n#include <cstdint>\n#include <string_view>\n\nnamespace voxel {\nnamespace block_ids {\n"

	for block: Dictionary in blocks:
		output += "inline constexpr std::uint16_t %s = %d;\n" % [str(block.name), int(block.id)]
	output += "}\n\ninline constexpr std::uint32_t default_block_flags(std::uint16_t id) {\n\tswitch (id) {\n"

	for block: Dictionary in blocks:
		output += "\t\tcase block_ids::%s: return %du;\n" % [str(block.name), _flag_mask(block.get("flags", []))]
	output += "\t\tdefault: return 0u;\n\t}\n}\n\ninline constexpr std::uint16_t block_id_from_name(std::string_view name) {\n"

	for block: Dictionary in blocks:
		output += "\tif (name == \"%s\") return block_ids::%s;\n" % [str(block.name), str(block.name)]
	output += "\treturn 0xffff;\n}\n\ninline constexpr int texture_layer_from_name(std::string_view name) {\n"

	for texture_name: String in texture_layers:
		output += "\tif (name == %s) return %d;\n" % [JSON.stringify(texture_name), int(texture_layers[texture_name])]
	output += "\treturn -1;\n}\n\ninline constexpr int WATER_TEXTURE_LAYER = %d;\n}\n\n#endif\n" % water_layer
	var file := FileAccess.open(GENERATED_HEADER_PATH, FileAccess.WRITE)
	if file == null:
		push_error("Could not write generated C++ registry header.")
		return false
	file.store_string(output)
	return true


func _write_texture_array(images: Array[Image]) -> bool:
	var texture_array := Texture2DArray.new()
	var error := texture_array.create_from_images(images)
	if error != OK:
		push_error("Could not create the block Texture2DArray: %d" % error)
		return false
	var resource_text := "[gd_resource type=\"Texture2DArray\" format=4 uid=\"uid://bjx6v2kqe3jyk\"]\n\n"
	var image_refs := PackedStringArray()

	for index in range(images.size()):
		var image: Image = images[index]
		var subresource_id := "Image_layer_%d" % index
		resource_text += "[sub_resource type=\"Image\" id=\"%s\"]\n" % subresource_id
		resource_text += "data = {\n\"data\": PackedByteArray(\"%s\"),\n" % Marshalls.raw_to_base64(image.get_data())
		resource_text += "\"format\": \"RGBA8\",\n\"height\": %d,\n\"mipmaps\": %s,\n\"width\": %d\n}\n\n" % [image.get_height(), str(image.has_mipmaps()), image.get_width()]
		image_refs.append("SubResource(\"%s\")" % subresource_id)
	resource_text += "[resource]\n_images = Array[Image]([%s])\n" % ", ".join(image_refs)
	var resource_file := FileAccess.open(ARRAY_PATH, FileAccess.WRITE)
	if resource_file == null:
		push_error("Could not write the generated Texture2DArray.")
		return false
	resource_file.store_string(resource_text)
	return true
