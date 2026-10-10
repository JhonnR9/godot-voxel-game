extends "res://tests/support/test_case.gd"

var observed_atomic := false

func observe_transfer(uuid: String, _index: int) -> void:
	if uuid == "source":
		observed_atomic = InventoryService.get_stack("source", 0).amount == 21 and InventoryService.get_stack("target", 0).amount == 99

func run() -> void:
	check(InventoryService.register_item_type(900001, 250), "Core accepts an item type unrelated to blocks")
	check(InventoryService.register_inventory("source", 3), "Logical registration needs no nodes or UUID generator")
	check(InventoryService.register_inventory("target", 2), "Target registers independently")
	check(not InventoryService.register_inventory("source", 4), "Duplicate registration cannot resize storage")
	check(not InventoryService.set_stack("source", 0, 900001, 251), "Core enforces supplied stack limit")
	InventoryService.set_stack("source", 0, block_id("grass"), 30)
	InventoryService.set_stack("target", 0, block_id("grass"), 90)
	var snapshot: Dictionary = InventoryService.get_stack("source", 0)
	snapshot.amount = 1
	check(InventoryService.get_stack("source", 0).amount == 30, "Value snapshots cannot mutate model")
	var revision: int = InventoryService.get_revision("source")
	InventoryService.inventory_changed.connect(observe_transfer)
	var result: Dictionary = InventoryService.transfer("source", 0, "target", 0, 30, revision)
	InventoryService.inventory_changed.disconnect(observe_transfer)
	check(result.success and result.moved == 9 and observed_atomic, "Both endpoints commit before first notification")
	check(not InventoryService.transfer("source", 0, "target", 1, 30, revision).success, "Stale request cannot replay")
	var before: Array = InventoryService.snapshot("source")
	check(not InventoryService.transfer("source", 0, "target", 0, 21, InventoryService.get_revision("source")).success, "Full target rejects drop")
	check(InventoryService.snapshot("source") == before, "Rejected transfer keeps source intact")
	InventoryService.set_stack("target", 1, block_id("dirt"), 5)
	check(not InventoryService.transfer("source", 0, "target", 1, 10, InventoryService.get_revision("source")).success, "Partial incompatible swap is rejected")
	check(InventoryService.transfer("source", 0, "target", 1, 21, InventoryService.get_revision("source")).success, "Whole stacks swap")
	check(InventoryService.get_stack("source", 0) == {"id": block_id("dirt"), "amount": 5} and InventoryService.get_stack("target", 1) == {"id": block_id("grass"), "amount": 21}, "Swap conserves items")
	InventoryService.set_stack("source", 2, 900001, 200)
	check(InventoryService.transfer("source", 2, "source", 1, 25, InventoryService.get_revision("source")).success, "Core accepts explicit partial quantity within same inventory")
	check(InventoryService.get_stack("source", 2).amount == 175 and InventoryService.get_stack("source", 1).amount == 25, "Generic limits and partial quantities are respected")
	before = InventoryService.snapshot("target")
	check(not InventoryService.add_items("target", block_id("grass"), 99), "All-or-nothing insertion rejects insufficient capacity")
	check(InventoryService.snapshot("target") == before, "Failed insertion leaves all slots unchanged")
	InventoryService.register_inventory("catalog", 1, true)
	InventoryService.set_stack("catalog", 0, block_id("grass"), 99)
	InventoryService.set_stack("target", 0, 0, 0)
	check(InventoryService.transfer("catalog", 0, "target", 0, 99, InventoryService.get_revision("catalog")).success, "Copy source can supply a stack")
	check(InventoryService.get_stack("catalog", 0).amount == 99, "Copy source remains intact")
	check(not InventoryService.can_transfer("target", 0, "catalog", 0, 99, InventoryService.get_revision("target")), "Copy source rejects incoming items")
	InventoryService.restore("source", [{"id": 900001, "amount": 1000}, {"id": 999999, "amount": 5}, {"id": block_id("grass"), "amount": -1}])
	check(InventoryService.snapshot("source") == [{"id": 900001, "amount": 250}, null, null], "Restore validates item definitions and quantities")
	print("Inventory service tests: ", failures, " failures.")
	quit(1 if failures else 0)
