#ifndef INVENTORY_SERVICE_H
#define INVENTORY_SERVICE_H
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <vector>
namespace godot {
// Logical registry only. The Godot binding exchanges values, never UI objects.

class InventoryService final : public Object {
	GDCLASS(InventoryService, Object)
	struct Stack {
		int id	   = 0;
		int amount = 0;
	};
	struct Inventory {
		std::vector<Stack> slots;
		bool copy_source = false;
		int64_t revision = 0;
	};
	static InventoryService *singleton;
	HashMap<String, Inventory> inventories;
	HashMap<int, int> limits;
	bool valid_slot(const String &id, int index) const;
	String validate_transfer(const String &source, int from, const String &target, int to, int amount, int64_t revision) const;
	void changed(const String &id, int index = -1);

protected:
	static void _bind_methods();

public:
	InventoryService() { singleton = this; }
	~InventoryService() {
		if (singleton == this)
			singleton = nullptr;
	}
	static InventoryService *get_singleton() { return singleton; }
	bool register_item_type(int id, int stack_limit);
	bool register_inventory(const String &id, int capacity, bool copy_source = false);
	bool has_inventory(const String &id) const { return inventories.has(id); }
	int get_capacity(const String &id) const;
	int64_t get_revision(const String &id) const;
	bool is_copy_source(const String &id) const;
	Dictionary get_stack(const String &id, int index) const;
	bool set_stack(const String &id, int index, int item_id, int amount);
	bool add_items(const String &id, int item_id, int amount);
	bool can_transfer(const String &source, int from, const String &target, int to, int amount, int64_t revision) const;
	Dictionary transfer(const String &source, int from, const String &target, int to, int amount, int64_t revision);
	Array snapshot(const String &id) const;
	bool restore(const String &id, const Array &slots);
};
} //namespace godot
#endif
