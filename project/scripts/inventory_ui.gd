extends Control

const SLOT_SIZE := 56
const SLOT_GAP := 4
const CREATIVE_COLUMNS := 8
const BLOCK_REGISTRY_PATH := "res://data/block_registry.generated.json"
var blocks: Array[Dictionary] = []
var mouse_unlocked := false
var layout_initialized := false
var inventory_ids: Dictionary = {}
var reflow_queued := false
var drag_controller: Node
@onready var inventory_panel: PanelContainer = $InventoryPanel

@onready var creative_panel: PanelContainer = $CreativePanel
@onready var creative_scroll: ScrollContainer = $CreativePanel/Margin/Content/CreativeScroll
@onready var creative_grid = $CreativePanel/Margin/Content/CreativeScroll/Center/CreativeGrid
@onready var inventory_scroll: ScrollContainer = $InventoryPanel/Margin/Content/InventoryScroll
@onready var inventory_grid = $InventoryPanel/Margin/Content/InventoryScroll/Center/InventoryGrid
@onready var creative_tab: Button = $Launchers/Creative
@onready var hint: Label = $InventoryPanel/Margin/Content/Hint
@onready var hotbar_panel: PanelContainer = $HotbarPanel
@onready var hotbar_grid = $HotbarPanel/Margin/HotbarGrid

func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	creative_panel.add_theme_stylebox_override("panel", _make_panel_style())
	hotbar_panel.add_theme_stylebox_override("panel", _make_panel_style())
	creative_panel.hide()
	inventory_panel.hide()
	inventory_panel.add_theme_stylebox_override("panel", _make_panel_style())
	_load_blocks()
	drag_controller = preload("res://scripts/inventory_drag_controller.gd").new()
	add_child(drag_controller)
	inventory_ids = InventorySession.get_player_ids()

	var creative_rows := maxi(2, ceili(float(blocks.size()) / CREATIVE_COLUMNS))
	_configure_grid(creative_grid, creative_rows, CREATIVE_COLUMNS, Vector2i(50, 50), InventorySession.get_creative_inventory_id())
	creative_grid.set_show_item_count(false)
	creative_grid.set_interaction_enabled(false)

	_configure_grid(hotbar_grid, 1, 9, Vector2i(SLOT_SIZE, SLOT_SIZE), inventory_ids.hotbar)
	hotbar_grid.set_show_item_count(true)
	hotbar_grid.set_interaction_enabled(false)
	_configure_grid(inventory_grid, 3, 9, Vector2i(SLOT_SIZE, SLOT_SIZE), inventory_ids.storage)
	inventory_grid.set_show_item_count(true)
	inventory_grid.set_interaction_enabled(false)
	creative_tab.pressed.connect(toggle_creative_window)
	$Launchers/Inventory.pressed.connect(toggle_inventory_window)
	creative_panel.layout_changed.connect(_save_window_layout)
	inventory_panel.layout_changed.connect(_save_window_layout)
	inventory_scroll.show()
	InventorySession.world_inventory_loaded.connect(_bind_player_inventories)
	creative_scroll.resized.connect(_queue_reflow)
	inventory_scroll.resized.connect(_queue_reflow)
	creative_panel.resized.connect(_queue_reflow)
	inventory_panel.resized.connect(_queue_reflow)
	resized.connect(_update_layout)
	_update_layout()
	_load_window_layout()
	set_mouse_unlocked(false)
	_queue_reflow()

func toggle_inventory_window() -> void:
	if not mouse_unlocked: return
	inventory_panel.visible = not inventory_panel.visible
	if inventory_panel.visible: inventory_panel.move_to_front()

func toggle_creative_window() -> void:
	if not mouse_unlocked: return
	creative_panel.visible = not creative_panel.visible
	if creative_panel.visible: creative_panel.move_to_front()

func set_mouse_unlocked(unlocked: bool) -> void:
	mouse_unlocked = unlocked
	if not unlocked:
		get_viewport().gui_cancel_drag()
		drag_controller.cancel_drag()
		creative_panel.finish_gesture()
		inventory_panel.finish_gesture()

	for panel in [creative_panel, inventory_panel]: panel.interaction_enabled = unlocked
	_set_mouse_filters(self, unlocked)

	for grid in [creative_grid, inventory_grid, hotbar_grid]:
		grid.set_interaction_enabled(unlocked)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	$Launchers/MouseHint.text = "F1 · Control character" if unlocked else "F1 · Unlock mouse"
	if not unlocked:
		var focused := get_viewport().gui_get_focus_owner()
		if focused: focused.release_focus()

func _set_mouse_filters(node: Node, enabled: bool) -> void:
	if node is Control:
		if not node.has_meta("free_mouse_filter"):
			node.set_meta("free_mouse_filter", node.mouse_filter)
		node.mouse_filter = node.get_meta("free_mouse_filter") if enabled else Control.MOUSE_FILTER_IGNORE

	for child in node.get_children(true): _set_mouse_filters(child, enabled)

func _save_window_layout() -> void:
	var data := {}

	for panel in [creative_panel, inventory_panel]:
		data[str(panel.name)] = [panel.position.x, panel.position.y, panel.size.x, panel.size.y]
	SaveService.save_user_settings("inventory_windows", data)

func _load_window_layout() -> void:
	var data: Dictionary = SaveService.load_user_settings("inventory_windows")

	for panel in [creative_panel, inventory_panel]:
		var rect = data.get(str(panel.name), [])
		if rect is Array and rect.size() == 4:
			panel.size = Vector2(float(rect[2]), float(rect[3]))
			panel.position = Vector2(float(rect[0]), float(rect[1]))
		panel.clamp_to_screen()

func _bind_player_inventories(_world_id: int = 0) -> void:
	inventory_ids = InventorySession.get_player_ids()
	drag_controller.bind_grid(hotbar_grid, inventory_ids.hotbar)
	drag_controller.bind_grid(inventory_grid, inventory_ids.storage)
	InventoryManager.select_hotbar_slot(InventorySession.selected_slot)
	_queue_reflow()

func _queue_reflow() -> void:
	if reflow_queued: return
	reflow_queued = true
	call_deferred("_reflow_windows")

func _reflow_windows() -> void:
	reflow_queued = false

	for pair in [[creative_grid, creative_scroll], [inventory_grid, inventory_scroll]]:
		var grid = pair[0]
		var scroll: ScrollContainer = pair[1]
		# Reserve a scrollbar width consistently to avoid column oscillation.
		var width := maxf(0, scroll.size.x - 18.0)
		var slot: Vector2i = grid.get_slot_size()
		var columns := maxi(2, floori((width - 8 + SLOT_GAP) / float(slot.x + SLOT_GAP)))
		columns = mini(columns, grid.get_slot_count())
		if columns != grid.get_columns(): grid.set_columns(columns)
	_set_mouse_filters(self, mouse_unlocked)

	for grid in [creative_grid, inventory_grid, hotbar_grid]: grid.set_interaction_enabled(mouse_unlocked)

func _resize_slots(grid: Control, side: int) -> void:
	if grid.get_slot_size() != Vector2i(side, side): grid.set_slot_size(Vector2i(side, side))

func _update_layout() -> void:
	var available_width := maxf(240.0, size.x - 32.0)
	var hotbar_side := clampi(floori((available_width - 36.0 - 8 * SLOT_GAP) / 9.0), 20, SLOT_SIZE)
	_resize_slots(hotbar_grid, hotbar_side)
	var hotbar_size: Vector2 = hotbar_grid.get_combined_minimum_size() + Vector2(20, 18)
	hotbar_panel.offset_left = -hotbar_size.x / 2.0
	hotbar_panel.offset_right = hotbar_size.x / 2.0
	hotbar_panel.offset_top = -hotbar_size.y - 16.0
	hotbar_panel.offset_bottom = -16.0
	if not layout_initialized:
		creative_panel.size = Vector2(470, 320).min(size)
		inventory_panel.size = Vector2(580, 320).min(size)
		creative_panel.position = Vector2(16, 80)
		inventory_panel.position = Vector2(maxf(16, size.x - 596), 80)
		layout_initialized = true

	for panel in [creative_panel, inventory_panel]: panel.clamp_to_screen()
	set_mouse_unlocked(mouse_unlocked)

func _load_blocks() -> void:
	var file := FileAccess.open(BLOCK_REGISTRY_PATH, FileAccess.READ)
	if file == null:
		push_error("Generated block registry is missing. Run Block Registry > Save and Generate.")
		return
	var data: Variant = JSON.parse_string(file.get_as_text())
	if not (data is Dictionary) or not (data.get("blocks", []) is Array):
		push_error("Generated block registry metadata is invalid.")
		return

	for block: Dictionary in data.blocks:
		if int(block.get("id", 0)) == 0:
			continue
		blocks.append({
			"id": int(block.id),
			"name": str(block.get("display_name", block.name)),
			"category": str(block.get("category", "misc")),
			"flags": block.get("flags", []),
		})

func _configure_grid(grid: Control, rows: int, columns: int, slot_size: Vector2i, uuid: String) -> void:
	grid.set_rows(rows)
	grid.set_columns(columns)
	grid.set_slot_size(slot_size)
	grid.set_slot_margin(Vector2i(SLOT_GAP, SLOT_GAP))
	grid.set_grid_padding(Vector2i(4, 4))
	drag_controller.bind_grid(grid, uuid)
	grid.set_item_frame(_make_slot_style(false))
	grid.set_item_frame_hover(_make_slot_style(false, true))
	grid.set_item_frame_selected(_make_slot_style(true))

func _make_block_item(block: Dictionary):
	var item = ClassDB.instantiate("ItemView")
	item.set_id(int(block.id))
	item.set_name(str(block.name))
	item.set_category(str(block.category))
	item.set_item_amount(99)
	item.set_icon(BlockIconCache.get_icon(int(block.id)))
	return item

func collect_block(block_id: int) -> bool:
	var block := _block_by_id(block_id)
	if block.is_empty():
		return false
	return InventoryService.add_items(inventory_ids.hotbar, block_id, 1) or InventoryService.add_items(inventory_ids.storage, block_id, 1)

func _block_by_id(block_id: int) -> Dictionary:
	for block in blocks:
		if int(block.id) == block_id:
			return block
	return {}

func _make_panel_style() -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.055, 0.065, 0.085, 0.95)
	style.border_color = Color(0.68, 0.72, 0.8, 0.95)
	style.set_border_width_all(2)
	style.set_corner_radius_all(5)
	return style

func _make_slot_style(selected: bool, hovered: bool = false) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.13, 0.15, 0.19, 0.98) if not selected else Color(0.24, 0.22, 0.15, 0.99)
	style.border_color = Color(0.78, 0.8, 0.84, 0.95) if not selected else Color(1.0, 0.82, 0.32, 1.0)
	if hovered:
		style.border_color = Color(1.0, 0.92, 0.62, 1.0)
	style.set_border_width_all(2 if selected else 1)
	style.set_corner_radius_all(3)
	return style

func serialize_inventory() -> Dictionary:
	return InventorySession.serialize_player()

func restore_inventory(data: Dictionary) -> void:
	InventorySession.restore_player(data)
	_bind_player_inventories()
