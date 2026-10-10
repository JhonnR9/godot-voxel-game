extends Node

signal world_inventory_loaded(world_id: int)
const STACK_LIMIT := 99
var definitions: Dictionary = {}
var world_id := 0
var player_ids: Dictionary = {}
var owners: Dictionary = {}
var selected_slot := 0
var creative_id := ""
var recovery_pending := false
var recovering := false

func _ready() -> void:
	var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://data/block_registry.generated.json"))

	for block: Dictionary in data.get("blocks", []):
		var id := int(block.id)
		if id == 0: continue
		definitions[id] = {"id": id, "name": str(block.get("display_name", block.name)), "category": str(block.get("category", "misc"))}
		InventoryService.register_item_type(id, STACK_LIMIT)
	get_tree().node_added.connect(_node_added)
	InventoryService.inventory_changed.connect(_inventory_changed)

func _node_added(node: Node) -> void:
	if node is VoxelAPI:
		node.world_opened.connect(load_world)
		node.world_saving.connect(save_world)

func create_uuid() -> String:
	var bytes := Crypto.new().generate_random_bytes(16)
	bytes[6] = (bytes[6] & 15) | 64
	bytes[8] = (bytes[8] & 63) | 128
	var hex := bytes.hex_encode()
	return "%s-%s-%s-%s-%s" % [hex.substr(0, 8), hex.substr(8, 4), hex.substr(12, 4), hex.substr(16, 4), hex.substr(20, 12)]

func register_inventory(uuid: String, capacity: int) -> bool:
	if not InventoryService.register_inventory(uuid, capacity): return false
	owners[uuid] = world_id
	return true

func get_player_ids() -> Dictionary:
	for role in ["hotbar", "storage"]:
		if not player_ids.has(role):
			player_ids[role] = create_uuid()
			register_inventory(player_ids[role], 9 if role == "hotbar" else 27)
	return player_ids.duplicate()

func get_creative_inventory_id() -> String:
	if creative_id.is_empty():
		creative_id = create_uuid()
		InventoryService.register_inventory(creative_id, definitions.size(), true)
		var index := 0
		for id in definitions:
			InventoryService.set_stack(creative_id, index, id, STACK_LIMIT)
			index += 1
	return creative_id

func make_view(stack: Dictionary):
	if stack.is_empty() or not definitions.has(int(stack.get("id", 0))): return null
	var definition: Dictionary = definitions[int(stack.id)]
	var view := ItemView.new()
	view.set_id(int(stack.id))
	view.set_item_amount(int(stack.amount))
	view.set_name(definition.name)
	view.set_category(definition.category)
	view.set_icon(BlockIconCache.get_icon(int(stack.id)))
	return view

func serialize_player() -> Dictionary:
	var data := {"version": 3, "ids": get_player_ids(), "selected_slot": selected_slot}

	for role in player_ids: data[role] = InventoryService.snapshot(player_ids[role])
	return data

func restore_player(data: Dictionary) -> void:
	get_player_ids()

	for role in ["hotbar", "storage"]:
		var slots = data.get(role, [])
		InventoryService.restore(player_ids[role], slots if slots is Array else [])
	if data.get("craft", []) is Array and not data.get("craft", []).is_empty():
		if not player_ids.has("recovery"):
			player_ids.recovery = create_uuid()
			register_inventory(player_ids.recovery, maxi(4, data.craft.size()))
		InventoryService.restore(player_ids.recovery, data.craft)
	if player_ids.has("recovery") and data.get("recovery") is Array:
		InventoryService.restore(player_ids.recovery, data.recovery)
	selected_slot = clampi(int(data.get("selected_slot", 0)), 0, 8)
	recover_legacy_items()

func load_world(id: int) -> void:
	if id <= 0: return
	world_id = id
	player_ids = {}
	var saved: Dictionary = SaveService.load_world_section(id, "inventories")
	var player: Dictionary = SaveService.load_world_section(id, "player")
	var legacy = player.get("inventory", {})
	if not legacy is Dictionary: legacy = {}
	var ids = saved.get("ids", legacy.get("ids", {}))
	if not ids is Dictionary: ids = {}
	var records = saved.get("records", {})
	if not records is Dictionary: records = {}
	# Keep generic records, including an old crafting record, without requiring a UI.

	for uuid in records:
		var record = records[uuid]
		if not uuid is String or not record is Dictionary: continue
		if owners.has(uuid) and owners[uuid] != id: continue
		var capacity := int(record.get("capacity", 0))
		if capacity < 1 or capacity > 4096: continue
		if register_inventory(uuid, capacity):
			var slots = record.get("slots", [])
			InventoryService.restore(uuid, slots if slots is Array else [])
	var data: Dictionary = legacy.duplicate(true)

	for role in ["hotbar", "storage"]:
		var uuid: String = str(ids.get(role, ""))
		if uuid.is_empty() or (owners.has(uuid) and owners[uuid] != id) or player_ids.values().has(uuid): uuid = create_uuid()
		if not register_inventory(uuid, 9 if role == "hotbar" else 27):
			uuid = create_uuid()
			register_inventory(uuid, 9 if role == "hotbar" else 27)
		player_ids[role] = uuid
		var record = records.get(str(ids.get(role, "")), {})
		if record is Dictionary and record.has("slots"): data[role] = record.slots
		elif record is Array: data[role] = record
	var old_uuid: String = str(ids.get("recovery", ids.get("craft", "")))
	if not old_uuid.is_empty() and owners.get(old_uuid, -1) == id and InventoryService.has_inventory(old_uuid):
		player_ids.recovery = old_uuid
		data.erase("craft")
		data.recovery = InventoryService.snapshot(old_uuid)
	data.selected_slot = saved.get("selected_slot", legacy.get("selected_slot", 0))
	restore_player(data)
	save_world(id)
	world_inventory_loaded.emit(id)

func save_world(id: int) -> bool:
	if id <= 0 or id != world_id: return false
	var records := {}

	for uuid in owners:
		if owners[uuid] == id:
			records[uuid] = {"capacity": InventoryService.get_capacity(uuid), "slots": InventoryService.snapshot(uuid)}
	return SaveService.save_world_section(id, "inventories", {"version": 3, "ids": player_ids, "selected_slot": selected_slot, "records": records})

func _inventory_changed(uuid: String, _index: int) -> void:
	if recovering or recovery_pending or not player_ids.has("recovery"): return
	if uuid not in [player_ids.hotbar, player_ids.storage]: return
	recovery_pending = true
	call_deferred("recover_legacy_items")

func recover_legacy_items() -> void:
	recovery_pending = false
	if recovering or not player_ids.has("recovery"): return
	recovering = true
	var source: String = player_ids.recovery

	for index in range(InventoryService.get_capacity(source)):
		for destination in [player_ids.storage, player_ids.hotbar]:
			for target in range(InventoryService.get_capacity(destination)):
				var stack: Dictionary = InventoryService.get_stack(source, index)
				if stack.is_empty(): break
				var existing: Dictionary = InventoryService.get_stack(destination, target)
				if not existing.is_empty() and existing.id != stack.id: continue
				InventoryService.transfer(source, index, destination, target, stack.amount, InventoryService.get_revision(source))
	recovering = false
