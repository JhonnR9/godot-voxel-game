#include "inventory_service.h"
#include <cmath>
#include <godot_cpp/core/class_db.hpp>
namespace godot {
InventoryService *InventoryService::singleton = nullptr;
bool InventoryService::register_item_type(int id, int stack_limit) {
	if (id <= 0 || stack_limit < 1)
		return false;
	if (limits.has(id))
		return limits[id] == stack_limit;
	limits.insert(id, stack_limit);
	return true;
}

bool InventoryService::register_inventory(const String &id, int capacity, bool copy_source) {
	if (id.is_empty() || capacity < 1 || capacity > 4096)
		return false;
	if (const Inventory *existing = inventories.getptr(id))
		return int(existing->slots.size()) == capacity && existing->copy_source == copy_source;
	Inventory inventory;
	inventory.slots.resize(capacity);
	inventory.copy_source = copy_source;
	inventories.insert(id, std::move(inventory));
	return true;
}

int InventoryService::get_capacity(const String &id) const {
	const Inventory *v = inventories.getptr(id);
	return v ? int(v->slots.size()) : 0;
}

int64_t InventoryService::get_revision(const String &id) const {
	const Inventory *v = inventories.getptr(id);
	return v ? v->revision : -1;
}

bool InventoryService::is_copy_source(const String &id) const {
	const Inventory *v = inventories.getptr(id);
	return v && v->copy_source;
}
bool InventoryService::valid_slot(const String &id, int index) const { return index >= 0 && index < get_capacity(id); }
Dictionary InventoryService::get_stack(const String &id, int index) const {
	Dictionary result;
	if (!valid_slot(id, index))
		return result;
	const Stack &stack = inventories[id].slots[index];
	if (stack.id) {
		result["id"]	 = stack.id;
		result["amount"] = stack.amount;
	}
	return result;
}
void InventoryService::changed(const String &id, int index) { emit_signal("inventory_changed", id, index); }
bool InventoryService::set_stack(const String &id, int index, int item_id, int amount) {
	if (!valid_slot(id, index))
		return false;
	if (amount < 0 || (amount > 0 && (!limits.has(item_id) || amount > limits[item_id])))
		return false;
	Inventory &inventory   = inventories[id];
	inventory.slots[index] = amount ? Stack{ item_id, amount } : Stack{};
	++inventory.revision;
	changed(id, index);
	return true;
}

bool InventoryService::add_items(const String &id, int item_id, int amount) {
	Inventory *inventory = inventories.getptr(id);
	if (!inventory || inventory->copy_source || !limits.has(item_id) || amount < 1)
		return false;
	const int limit	 = limits[item_id];
	int64_t capacity = 0;
	for (const Stack &stack : inventory->slots)
		if (!stack.id || stack.id == item_id)
			capacity += int64_t(limit) - stack.amount;
	if (capacity < amount)
		return false;
	int remaining = amount;
	for (int pass = 0; pass < 2; ++pass)
		for (Stack &stack : inventory->slots) {
			if (!remaining)
				break;
			if ((pass == 0 && stack.id != item_id) || (pass == 1 && stack.id != 0))
				continue;
			const int moved = MIN(remaining, limit - stack.amount);
			stack.id		= item_id;
			stack.amount += moved;
			remaining -= moved;
		}
	++inventory->revision;
	changed(id);
	return true;
}

String InventoryService::validate_transfer(const String &source, int from, const String &target, int to, int amount, int64_t revision) const {
	if (!valid_slot(source, from) || !valid_slot(target, to))
		return "invalid_slot";
	if (get_revision(source) != revision)
		return "stale_source";
	if (is_copy_source(target))
		return "read_only_target";
	const Stack &origin = inventories[source].slots[from], &destination = inventories[target].slots[to];
	if (!origin.id || amount < 1 || amount > origin.amount)
		return "invalid_amount";
	if (source == target && from == to)
		return String();
	if (destination.id == origin.id && destination.amount >= limits[origin.id])
		return "full";
	if (destination.id && destination.id != origin.id && (is_copy_source(source) || amount != origin.amount))
		return "incompatible";
	return String();
}

bool InventoryService::can_transfer(const String &source, int from, const String &target, int to, int amount, int64_t revision) const {
	return validate_transfer(source, from, target, to, amount, revision).is_empty();
}

Dictionary InventoryService::transfer(const String &source, int from, const String &target, int to, int amount, int64_t revision) {
	Dictionary result;
	const String error = validate_transfer(source, from, target, to, amount, revision);
	result["success"]  = error.is_empty();
	result["reason"]   = error;
	if (!error.is_empty())
		return result;
	if (source == target && from == to) {
		result["moved"] = 0;
		return result;
	}
	Inventory &src = inventories[source], &dst = inventories[target];
	Stack &origin = src.slots[from], &destination = dst.slots[to];
	int moved = amount;
	if (!destination.id || destination.id == origin.id) {
		moved		   = MIN(amount, limits[origin.id] - destination.amount);
		destination.id = origin.id;
		destination.amount += moved;
		if (!src.copy_source) {
			origin.amount -= moved;
			if (!origin.amount)
				origin = {};
		}
	} else {
		const Stack old = destination;
		destination		= origin;
		origin			= old;
	}
	if (!src.copy_source)
		++src.revision;
	if (source != target)
		++dst.revision;
	result["moved"] = moved;
	// All data is committed before notifying either observer.
	changed(source, from);
	changed(target, to);
	return result;
}

Array InventoryService::snapshot(const String &id) const {
	Array result;
	for (int i = 0; i < get_capacity(id); ++i) {
		Dictionary stack = get_stack(id, i);
		result.push_back(stack.is_empty() ? Variant() : Variant(stack));
	}
	return result;
}

bool InventoryService::restore(const String &id, const Array &slots) {
	Inventory *inventory = inventories.getptr(id);
	if (!inventory)
		return false;
	std::vector<Stack> restored(inventory->slots.size());
	for (int i = 0; i < MIN(int(restored.size()), slots.size()); ++i) {
		if (slots[i].get_type() != Variant::DICTIONARY)
			continue;
		Dictionary entry = slots[i];
		Variant raw_id = entry.get("id", 0), raw_amount = entry.get("amount", 0);
		if ((raw_id.get_type() != Variant::INT && raw_id.get_type() != Variant::FLOAT) || (raw_amount.get_type() != Variant::INT && raw_amount.get_type() != Variant::FLOAT))
			continue;
		double item_id = raw_id, amount = raw_amount;
		if (!std::isfinite(item_id) || !std::isfinite(amount) || item_id < 1 || item_id > INT32_MAX || amount < 1)
			continue;
		int type = int(item_id);
		if (!limits.has(type))
			continue;
		restored[i] = { type, int(MIN(amount, double(limits[type]))) };
	}
	inventory->slots = std::move(restored);
	++inventory->revision;
	changed(id);
	return true;
}

void InventoryService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_item_type", "id", "stack_limit"), &InventoryService::register_item_type);
	ClassDB::bind_method(D_METHOD("register_inventory", "id", "capacity", "copy_source"), &InventoryService::register_inventory, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("has_inventory", "id"), &InventoryService::has_inventory);
	ClassDB::bind_method(D_METHOD("get_capacity", "id"), &InventoryService::get_capacity);
	ClassDB::bind_method(D_METHOD("get_revision", "id"), &InventoryService::get_revision);
	ClassDB::bind_method(D_METHOD("is_copy_source", "id"), &InventoryService::is_copy_source);
	ClassDB::bind_method(D_METHOD("get_stack", "id", "index"), &InventoryService::get_stack);
	ClassDB::bind_method(D_METHOD("set_stack", "id", "index", "item_id", "amount"), &InventoryService::set_stack);
	ClassDB::bind_method(D_METHOD("add_items", "id", "item_id", "amount"), &InventoryService::add_items);
	ClassDB::bind_method(D_METHOD("can_transfer", "source", "from", "target", "to", "amount", "revision"), &InventoryService::can_transfer);
	ClassDB::bind_method(D_METHOD("transfer", "source", "from", "target", "to", "amount", "revision"), &InventoryService::transfer);
	ClassDB::bind_method(D_METHOD("snapshot", "id"), &InventoryService::snapshot);
	ClassDB::bind_method(D_METHOD("restore", "id", "slots"), &InventoryService::restore);
	ADD_SIGNAL(MethodInfo("inventory_changed", PropertyInfo(Variant::STRING, "id"), PropertyInfo(Variant::INT, "index")));
}
} //namespace godot
