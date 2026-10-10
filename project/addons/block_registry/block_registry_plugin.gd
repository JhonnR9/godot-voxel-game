@tool
extends EditorPlugin

const REGISTRY_PATH := "res://data/block_registry.json"
const GENERATOR_PATH := "res://addons/block_registry/block_asset_generator.gd"
const FLAG_NAMES := ["solid", "transparent", "cutout", "emissive", "waterlog", "ocean", "crossed"]

var _dock: VBoxContainer
var _block_list: ItemList
var _id_field: SpinBox
var _name_field: LineEdit
var _display_name_field: LineEdit
var _category_field: LineEdit
var _tint_field: ColorPickerButton
var _texture_fields: Dictionary = {}
var _flag_fields: Dictionary = {}
var _selected_index := -1
var _blocks: Array = []
var _status: Label


func _enter_tree() -> void:
	_build_dock()
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, _dock)
	_load_registry()


func _exit_tree() -> void:
	if _dock != null:
		remove_control_from_docks(_dock)
		_dock.queue_free()


func _build_dock() -> void:
	_dock = VBoxContainer.new()
	_dock.name = "BlockRegistry"
	_dock.custom_minimum_size = Vector2(340, 420)
	var title := Label.new()
	title.text = "Block Registry"
	title.add_theme_font_size_override("font_size", 18)
	_dock.add_child(title)
	var split := HSplitContainer.new()
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_dock.add_child(split)
	_block_list = ItemList.new()
	_block_list.custom_minimum_size = Vector2(125, 0)
	_block_list.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_block_list.item_selected.connect(_on_block_selected)
	split.add_child(_block_list)

	var form_scroll := ScrollContainer.new()
	form_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	form_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	split.add_child(form_scroll)
	var form := VBoxContainer.new()
	form.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	form_scroll.add_child(form)

	_id_field = SpinBox.new()
	_id_field.min_value = 0
	_id_field.max_value = 1023
	_id_field.step = 1
	_add_labeled_control(form, "Block ID", _id_field)
	_name_field = LineEdit.new()
	_add_labeled_control(form, "Name (lowercase_key)", _name_field)
	_display_name_field = LineEdit.new()
	_add_labeled_control(form, "Display name", _display_name_field)
	_category_field = LineEdit.new()
	_add_labeled_control(form, "Category", _category_field)
	_tint_field = ColorPickerButton.new()
	_add_labeled_control(form, "Tint", _tint_field)
	_add_labeled_label(form, "Texture keys (without extension)")

	for face in ["side", "top", "bottom"]:
		var field := LineEdit.new()
		field.placeholder_text = "optional"
		_texture_fields[face] = field
		_add_labeled_control(form, face.capitalize(), field)
	_add_labeled_label(form, "Flags")
	var flags_grid := GridContainer.new()
	flags_grid.columns = 2
	form.add_child(flags_grid)

	for flag in FLAG_NAMES:
		var checkbox := CheckBox.new()
		checkbox.text = flag
		_flag_fields[flag] = checkbox
		flags_grid.add_child(checkbox)

	var buttons := HBoxContainer.new()
	_dock.add_child(buttons)
	var add_button := Button.new()
	add_button.text = "Add"
	add_button.pressed.connect(_add_block)
	buttons.add_child(add_button)
	var remove_button := Button.new()
	remove_button.text = "Remove"
	remove_button.pressed.connect(_remove_block)
	buttons.add_child(remove_button)
	var generate_button := Button.new()
	generate_button.text = "Save and Generate"
	generate_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	generate_button.pressed.connect(_save_and_generate)
	_dock.add_child(generate_button)
	_status = Label.new()
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_dock.add_child(_status)


func _add_labeled_control(parent: VBoxContainer, title: String, control: Control) -> void:
	var label := Label.new()
	label.text = title
	parent.add_child(label)
	control.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	parent.add_child(control)


func _add_labeled_label(parent: VBoxContainer, title: String) -> void:
	var label := Label.new()
	label.text = title
	label.add_theme_font_size_override("font_size", 14)
	parent.add_child(label)


func _load_registry() -> void:
	var file := FileAccess.open(REGISTRY_PATH, FileAccess.READ)
	if file == null:
		_status.text = "Could not open " + REGISTRY_PATH
		return
	var data: Variant = JSON.parse_string(file.get_as_text())
	if not (data is Dictionary) or not (data.get("blocks", []) is Array):
		_status.text = "The registry file is invalid."
		return
	_blocks = data.blocks.duplicate(true)
	_refresh_list()
	if not _blocks.is_empty():
		_block_list.select(0)
		_on_block_selected(0)


func _refresh_list() -> void:
	_block_list.clear()

	for block: Dictionary in _blocks:
		_block_list.add_item("%03d  %s" % [int(block.get("id", 0)), str(block.get("name", "unnamed"))])


func _on_block_selected(index: int) -> void:
	if _selected_index >= 0 and _selected_index < _blocks.size():
		_commit_fields()
	_selected_index = index
	if index < 0 or index >= _blocks.size():
		return
	var block: Dictionary = _blocks[index]
	_id_field.value = int(block.get("id", 0))
	_name_field.text = str(block.get("name", ""))
	_display_name_field.text = str(block.get("display_name", ""))
	_category_field.text = str(block.get("category", "misc"))
	var tint: Array = block.get("tint", [1.0, 1.0, 1.0, 1.0])
	if tint.size() == 4:
		_tint_field.color = Color(float(tint[0]), float(tint[1]), float(tint[2]), float(tint[3]))
	var textures: Dictionary = block.get("textures", {})

	for face in _texture_fields:
		_texture_fields[face].text = str(textures.get(face, ""))
	var flags: Array = block.get("flags", [])

	for flag in _flag_fields:
		_flag_fields[flag].button_pressed = flags.has(flag)


func _commit_fields() -> void:
	if _selected_index < 0 or _selected_index >= _blocks.size():
		return
	var block: Dictionary = _blocks[_selected_index]
	block.id = int(_id_field.value)
	block.name = _name_field.text.strip_edges()
	block.display_name = _display_name_field.text.strip_edges()
	block.category = _category_field.text.strip_edges()
	var tint := _tint_field.color
	block.tint = [tint.r, tint.g, tint.b, tint.a]
	var textures := {}

	for face in _texture_fields:
		var value := str(_texture_fields[face].text).strip_edges()
		if not value.is_empty():
			textures[face] = value
	block.textures = textures
	var flags: Array[String] = []

	for flag in _flag_fields:
		if _flag_fields[flag].button_pressed:
			flags.append(flag)
	block.flags = flags
	_blocks[_selected_index] = block
	_block_list.set_item_text(_selected_index, "%03d  %s" % [int(block.id), str(block.name)])


func _add_block() -> void:
	_commit_fields()
	var next_id := 0

	for block: Dictionary in _blocks:
		next_id = maxi(next_id, int(block.get("id", -1)) + 1)
	if next_id > 1023:
		_status.text = "No free block IDs remain (maximum is 1023)."
		return
	_blocks.append({
		"id": next_id,
		"name": "new_block_%d" % next_id,
		"display_name": "New Block %d" % next_id,
		"category": "misc",
		"textures": {},
		"flags": ["solid"],
		"tint": [1.0, 1.0, 1.0, 1.0],
	})
	_refresh_list()
	var new_index := _blocks.size() - 1
	_selected_index = -1
	_block_list.select(new_index)
	_on_block_selected(new_index)
	_status.text = "Added a block. Assign at least one texture before generating."


func _remove_block() -> void:
	_commit_fields()
	if _selected_index < 0 or _selected_index >= _blocks.size():
		return
	if int(_blocks[_selected_index].get("id", -1)) == 0:
		_status.text = "Block ID 0 is reserved for air."
		return
	_blocks.remove_at(_selected_index)
	_selected_index = -1
	_refresh_list()
	if not _blocks.is_empty():
		_block_list.select(0)
		_on_block_selected(0)
	_status.text = "Block removed. Save and Generate to apply changes."


func _save_and_generate() -> void:
	_commit_fields()
	var selected_id := -1
	if _selected_index >= 0 and _selected_index < _blocks.size():
		selected_id = int(_blocks[_selected_index].get("id", -1))
	_blocks.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return int(a.get("id", -1)) < int(b.get("id", -1)))
	var source := {"version": 1, "blocks": _blocks}
	var file := FileAccess.open(REGISTRY_PATH, FileAccess.WRITE)
	if file == null:
		_status.text = "Could not save " + REGISTRY_PATH
		return
	file.store_string(JSON.stringify(source, "\t") + "\n")
	var generator_script: Script = load(GENERATOR_PATH)
	if generator_script == null:
		_status.text = "Could not load the asset generator."
		return
	var generator = generator_script.new()
	if not generator.rebuild():
		_status.text = "Generation failed. Check the Output panel for details."
		return
	get_editor_interface().get_resource_filesystem().scan()
	_selected_index = -1
	_refresh_list()

	for index in range(_blocks.size()):
		if int(_blocks[index].get("id", -1)) == selected_id:
			_block_list.select(index)
			_on_block_selected(index)
			break
	_status.text = "Registry saved. Texture array, metadata, and C++ IDs regenerated."
