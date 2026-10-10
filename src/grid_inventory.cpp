#include "grid_inventory.h"

#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/label_settings.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/texture_rect.hpp>

namespace godot {

int64_t GridInventory::_make_key(const int column, const int row) const {
	const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(column)) << 32) |
			static_cast<uint32_t>(row);
	return static_cast<int64_t>(key);
}

void GridInventory::_draw_background() {
	if (_background.is_valid())
		_background->draw(get_canvas_item(), Rect2(Point2(), get_size()));
}

Panel *GridInventory::_create_slot_panel(const Point2i &position) {
	Panel *panel = memnew(Panel);
	panel->set_position(position);
	panel->set_size(_slot_size);
	panel->set_mouse_filter(_interaction_enabled ? MOUSE_FILTER_STOP : MOUSE_FILTER_IGNORE);
	panel->set_force_pass_scroll_events(true);
	const Point2i cell((position.x - _grid_padding.x) / (_slot_size.x + _slot_margin.x),
					   (position.y - _grid_padding.y) / (_slot_size.y + _slot_margin.y));
	panel->connect("mouse_entered", callable_mp(this, &GridInventory::_on_slot_mouse_entered).bind(cell));
	panel->connect("mouse_exited", callable_mp(this, &GridInventory::_on_slot_mouse_exited).bind(cell));
	panel->connect("gui_input", callable_mp(this, &GridInventory::_on_slot_gui_input).bind(cell));
	panel->set_drag_forwarding(
			callable_mp(this, &GridInventory::_make_drag_data).bind(cell),
			callable_mp(this, &GridInventory::_accept_drop_data).bind(cell),
			callable_mp(this, &GridInventory::_handle_drop_data).bind(cell));
	return panel;
}

Label *GridInventory::_create_count_label() {
	Label *label = memnew(Label);
	label->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_BOTTOM);
	label->set_offset(SIDE_RIGHT, -3.0);
	label->set_offset(SIDE_BOTTOM, -1.0);
	label->set_mouse_filter(MOUSE_FILTER_IGNORE);
	label->set_z_index(1);
	label->add_theme_constant_override("outline_size", 4);
	label->add_theme_color_override("font_outline_color", Color(0, 0, 0, 1));
	if (_count_label_settings.is_valid())
		label->set_label_settings(_count_label_settings);
	return label;
}

void GridInventory::_sync_slot(Slot &slot) {
	const int index = slot.row * _columns + slot.column;
	slot.item		= index == _hidden_slot || index >= int(_items.size()) ? Ref<ItemView>() : _items[index];
	if (slot.item.is_null()) {
		if (slot.icon) {
			slot.icon->hide();
			slot.icon->queue_free();
			slot.icon = nullptr;
		}

		if (slot.count_label)
			slot.count_label->set_text(String());
		if (slot.panel)
			slot.panel->set_tooltip_text(String());
		return;
	}
	if (slot.item->get_icon().is_valid()) {
		if (!slot.icon) {
			slot.icon = memnew(TextureRect);
			slot.icon->set_mouse_filter(MOUSE_FILTER_IGNORE);
			slot.icon->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
			slot.icon->set_offset(SIDE_LEFT, 4.0);
			slot.icon->set_offset(SIDE_TOP, 4.0);
			slot.icon->set_offset(SIDE_RIGHT, -4.0);
			slot.icon->set_offset(SIDE_BOTTOM, -4.0);
			slot.icon->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
			slot.icon->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
			slot.panel->add_child(slot.icon);
		}
		slot.icon->set_texture(slot.item->get_icon());
	} else if (slot.icon) {
		slot.icon->hide();
		slot.icon->queue_free();
		slot.icon = nullptr;
	}
	if (slot.count_label) {
		slot.count_label->set_text(_show_item_count && slot.item->get_item_amount() > 1
										   ? String::num_int64(slot.item->get_item_amount())
										   : String());
	}
	if (slot.panel) {
		String tooltip = slot.item->get_name() + "\nID: " + String::num_int64(slot.item->get_id()) +
				"\nCategory: " + (slot.item->get_category().is_empty() ? String("misc") : slot.item->get_category());
		if (!slot.item->get_hint_description().is_empty())
			tooltip += "\n" + slot.item->get_hint_description();
		slot.panel->set_tooltip_text(tooltip);
	}
}

void GridInventory::_apply_slot_style(Slot &slot) {
	if (!slot.panel)
		return;
	const int64_t key = _make_key(slot.column, slot.row);
	Ref<StyleBox> style;
	if (key == _hovered_key && _item_frame_hover.is_valid()) {
		style = _item_frame_hover;
	} else if (Point2i(slot.column, slot.row) == _selected_cell && _item_frame_selected.is_valid()) {
		style = _item_frame_selected;
	} else {
		style = _item_frame;
	}
	if (style.is_valid())
		slot.panel->add_theme_stylebox_override("panel", style);
	else
		slot.panel->remove_theme_stylebox_override("panel");
}

void GridInventory::_generate_grid() {
	_clear_grid();
	_rows = MAX(1, (get_slot_count() + _columns - 1) / _columns);
	if (_slot_size.x < 1 || _slot_size.y < 1 || _rows < 1 || _columns < 1) {
		update_minimum_size();
		queue_redraw();
		return;
	}
	for (int row = 0; row < _rows; ++row) {
		for (int column = 0; column < _columns; ++column) {
			if (row * _columns + column >= get_slot_count())
				break;
			const Point2i position(_grid_padding.x + column * (_slot_size.x + _slot_margin.x),
								   _grid_padding.y + row * (_slot_size.y + _slot_margin.y));
			Slot slot;
			slot.rect		 = Rect2i(position, _slot_size);
			slot.column		 = column;
			slot.row		 = row;
			slot.panel		 = _create_slot_panel(position);
			slot.count_label = _create_count_label();
			slot.panel->add_child(slot.count_label);
			add_child(slot.panel);
			_apply_slot_style(slot);
			_sync_slot(slot);
			_cells.insert(_make_key(column, row), slot);
		}
	}
	_hovered_key = INVALID_KEY;
	update_minimum_size();
	queue_redraw();
}

void GridInventory::_clear_grid() {
	for (KeyValue<int64_t, Slot> &entry : _cells) {
		Slot &slot = entry.value;
		if (slot.panel) {
			const Point2i cell(slot.column, slot.row);
			Callable entered   = callable_mp(this, &GridInventory::_on_slot_mouse_entered).bind(cell);
			Callable exited	   = callable_mp(this, &GridInventory::_on_slot_mouse_exited).bind(cell);
			Callable gui_input = callable_mp(this, &GridInventory::_on_slot_gui_input).bind(cell);
			if (slot.panel->is_connected("mouse_entered", entered))
				slot.panel->disconnect("mouse_entered", entered);
			if (slot.panel->is_connected("mouse_exited", exited))
				slot.panel->disconnect("mouse_exited", exited);
			if (slot.panel->is_connected("gui_input", gui_input))
				slot.panel->disconnect("gui_input", gui_input);
			remove_child(slot.panel);
			slot.panel->queue_free();
		}
	}
	_cells.clear();
}

void GridInventory::_connect_style_signal(const Ref<StyleBox> &style) {
	const Callable changed = callable_mp(this, &GridInventory::_on_style_changed);
	if (style.is_valid() && !style->is_connected("changed", changed))
		style->connect("changed", changed);
}

void GridInventory::_disconnect_style_signal(const Ref<StyleBox> &style) {
	const Callable changed = callable_mp(this, &GridInventory::_on_style_changed);
	if (style.is_valid() && style->is_connected("changed", changed))
		style->disconnect("changed", changed);
}

Variant GridInventory::_make_drag_data(const Vector2 &, const Point2i &cell) {
	if (!_interaction_enabled)
		return Variant();
	const Ref<ItemView> item = get_item_at(cell);
	if (item.is_null() || item->get_icon().is_null())
		return Variant();
	_drag_payload.clear();
	emit_signal("drag_started", _slot_index(cell));
	if (_drag_payload.is_empty())
		return Variant();
	TextureRect *preview = memnew(TextureRect);
	preview->set_texture(item->get_icon());
	preview->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	preview->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	preview->set_custom_minimum_size(Vector2(_slot_size));
	set_drag_preview(preview);
	return _drag_payload;
}

bool GridInventory::_accept_drop_data(const Vector2 &, const Variant &data, const Point2i &cell) {
	_drop_allowed = false;
	if (_interaction_enabled && data.get_type() == Variant::DICTIONARY)
		emit_signal("drop_hovered", data, _slot_index(cell));
	return _drop_allowed;
}

void GridInventory::_handle_drop_data(const Vector2 &, const Variant &data, const Point2i &cell) {
	if (_interaction_enabled && data.get_type() == Variant::DICTIONARY)
		emit_signal("drop_requested", data, _slot_index(cell));
}

void GridInventory::_on_slot_gui_input(InputEvent *event, const Point2i &cell) {
	if (!_interaction_enabled)
		return;
	auto *mouse_event = Object::cast_to<InputEventMouseButton>(event);
	if (!mouse_event || !mouse_event->is_pressed())
		return;
	const int64_t key = _make_key(cell.x, cell.y);
	if (const Slot *slot = _cells.getptr(key)) {
		emit_signal("slot_clicked", Vector2i(slot->column, slot->row), mouse_event->get_button_index());
	}
}

void GridInventory::_on_style_changed() { queue_redraw(); }

void GridInventory::_on_label_settings_changed() {
	for (KeyValue<int64_t, Slot> &entry : _cells) {
		if (entry.value.count_label && _count_label_settings.is_valid()) {
			entry.value.count_label->set_label_settings(_count_label_settings);
		}
	}
}

void GridInventory::_on_slot_mouse_entered(const Point2i &cell) {
	const int64_t previous = _hovered_key;
	_hovered_key		   = _make_key(cell.x, cell.y);
	if (Slot *slot = _cells.getptr(previous))
		_apply_slot_style(*slot);
	if (Slot *slot = _cells.getptr(_hovered_key))
		_apply_slot_style(*slot);
}

void GridInventory::_on_slot_mouse_exited(const Point2i &cell) {
	if (_hovered_key != _make_key(cell.x, cell.y))
		return;
	const int64_t previous = _hovered_key;
	_hovered_key		   = INVALID_KEY;
	if (Slot *slot = _cells.getptr(previous))
		_apply_slot_style(*slot);
}

Size2 GridInventory::_get_minimum_size() const {
	return Size2(_grid_padding.x * 2 + _columns * _slot_size.x + MAX(0, _columns - 1) * _slot_margin.x,
				 _grid_padding.y * 2 + _rows * _slot_size.y + MAX(0, _rows - 1) * _slot_margin.y);
}

int GridInventory::_slot_index(const Point2i &cell) const {
	if (cell.x < 0 || cell.x >= _columns || cell.y < 0 || cell.y >= _rows)
		return -1;
	return cell.y * _columns + cell.x;
}
int GridInventory::get_slot_count() const { return int(_items.size()); }
void GridInventory::set_inventory_id(const String &uuid) { _inventory_id = uuid; }
void GridInventory::set_slot_count(int count) {
	ERR_FAIL_COND(count < 0 || count > 4096);
	if (count == int(_items.size()))
		return;
	_items.resize(count);
	_generate_grid();
}

void GridInventory::set_item_view(int index, const Ref<ItemView> &item) {
	if (index < 0 || index >= int(_items.size()))
		return;
	_items[index] = item.is_valid() ? item->duplicate_item() : Ref<ItemView>();
	const Vector2i cell(index % _columns, index / _columns);
	if (Slot *slot = _cells.getptr(_make_key(cell.x, cell.y)))
		_sync_slot(*slot);
	emit_signal("item_changed", cell, _items[index]);
}

void GridInventory::set_hidden_slot(int index) {
	int previous = _hidden_slot;
	_hidden_slot = index;
	for (int i : { previous, index })
		if (i >= 0 && i < int(_items.size()))
			if (Slot *slot = _cells.getptr(_make_key(i % _columns, i / _columns)))
				_sync_slot(*slot);
}

Ref<ItemView> GridInventory::get_item_at(const Point2i &cell) const {
	const int index = _slot_index(cell);
	return index >= 0 && index < int(_items.size()) && _items[index].is_valid() ? _items[index]->duplicate_item() : Ref<ItemView>();
}

void GridInventory::set_selected_cell(const Point2i &cell) {
	if (cell.x < -1 || cell.x >= _columns || cell.y < -1 || cell.y >= _rows)
		return;
	const Point2i previous = _selected_cell;
	_selected_cell		   = cell;
	if (Slot *slot = _cells.getptr(_make_key(previous.x, previous.y)))
		_apply_slot_style(*slot);
	if (Slot *slot = _cells.getptr(_make_key(cell.x, cell.y)))
		_apply_slot_style(*slot);
}

void GridInventory::set_show_item_count(const bool enabled) {
	_show_item_count = enabled;
	for (KeyValue<int64_t, Slot> &entry : _cells)
		_sync_slot(entry.value);
}

void GridInventory::set_interaction_enabled(const bool enabled) {
	_interaction_enabled = enabled;
	for (KeyValue<int64_t, Slot> &entry : _cells) {
		if (entry.value.panel)
			entry.value.panel->set_mouse_filter(enabled ? MOUSE_FILTER_STOP : MOUSE_FILTER_IGNORE);
	}
}

void GridInventory::set_rows(const int value) {
	_rows = MAX(1, value);
	_generate_grid();
}

void GridInventory::set_columns(const int value) {
	const int columns = MAX(1, value);
	if (columns == _columns)
		return;
	const int selected_index = _selected_cell.y * _columns + _selected_cell.x;
	_columns				 = columns;
	if (_selected_cell.x >= 0 && _selected_cell.y >= 0)
		_selected_cell = Point2i(selected_index % _columns, selected_index / _columns);
	_generate_grid();
}

void GridInventory::set_slot_size(const Size2i &value) {
	_slot_size = value;
	_generate_grid();
}

void GridInventory::set_slot_margin(const Size2i &value) {
	_slot_margin = value;
	_generate_grid();
}

void GridInventory::set_grid_padding(const Size2i &value) {
	_grid_padding = value;
	_generate_grid();
}

void GridInventory::set_background(const Ref<StyleBox> &value) {
	_disconnect_style_signal(_background);
	_background = value;
	_connect_style_signal(_background);
	queue_redraw();
}

void GridInventory::set_item_frame(const Ref<StyleBox> &value) {
	_disconnect_style_signal(_item_frame);
	_item_frame = value;
	_connect_style_signal(_item_frame);
	for (KeyValue<int64_t, Slot> &entry : _cells)
		_apply_slot_style(entry.value);
}

void GridInventory::set_item_frame_hover(const Ref<StyleBox> &value) {
	_disconnect_style_signal(_item_frame_hover);
	_item_frame_hover = value;
	_connect_style_signal(_item_frame_hover);
	for (KeyValue<int64_t, Slot> &entry : _cells)
		_apply_slot_style(entry.value);
}

void GridInventory::set_item_frame_selected(const Ref<StyleBox> &value) {
	_disconnect_style_signal(_item_frame_selected);
	_item_frame_selected = value;
	_connect_style_signal(_item_frame_selected);
	for (KeyValue<int64_t, Slot> &entry : _cells)
		_apply_slot_style(entry.value);
}

void GridInventory::set_count_label_settings(const Ref<LabelSettings> &value) {
	if (_count_label_settings.is_valid()) {
		Callable changed = callable_mp(this, &GridInventory::_on_label_settings_changed);
		if (_count_label_settings->is_connected("changed", changed))
			_count_label_settings->disconnect("changed", changed);
	}
	_count_label_settings = value;
	if (_count_label_settings.is_valid())
		_count_label_settings->connect("changed", callable_mp(this, &GridInventory::_on_label_settings_changed));
	for (KeyValue<int64_t, Slot> &entry : _cells) {
		if (entry.value.count_label && _count_label_settings.is_valid())
			entry.value.count_label->set_label_settings(_count_label_settings);
	}
}

void GridInventory::_notification(const int what) {
	switch (what) {
		case NOTIFICATION_DRAW:
			_draw_background();
			break;
		case NOTIFICATION_ENTER_TREE:
			if (_cells.is_empty())
				_generate_grid();
			_connect_style_signal(_background);
			_connect_style_signal(_item_frame);
			_connect_style_signal(_item_frame_hover);
			_connect_style_signal(_item_frame_selected);
			break;
		case NOTIFICATION_EXIT_TREE: {
			_disconnect_style_signal(_background);
			_disconnect_style_signal(_item_frame);
			_disconnect_style_signal(_item_frame_hover);
			_disconnect_style_signal(_item_frame_selected);
			break;
		}
		case NOTIFICATION_DRAG_END:
			emit_signal("drag_finished");
			break;
	}
}

void GridInventory::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_slot_count", "count"), &GridInventory::set_slot_count);
	ClassDB::bind_method(D_METHOD("set_item_view", "index", "item"), &GridInventory::set_item_view);
	ClassDB::bind_method(D_METHOD("set_hidden_slot", "index"), &GridInventory::set_hidden_slot);
	ClassDB::bind_method(D_METHOD("set_drag_payload", "payload"), &GridInventory::set_drag_payload);
	ClassDB::bind_method(D_METHOD("set_drop_allowed", "allowed"), &GridInventory::set_drop_allowed);
	ADD_SIGNAL(MethodInfo("drag_started", PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("drop_hovered", PropertyInfo(Variant::DICTIONARY, "payload"), PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("drop_requested", PropertyInfo(Variant::DICTIONARY, "payload"), PropertyInfo(Variant::INT, "index")));
	ADD_SIGNAL(MethodInfo("drag_finished"));
	ClassDB::bind_method(D_METHOD("set_inventory_id", "uuid"), &GridInventory::set_inventory_id);
	ClassDB::bind_method(D_METHOD("get_inventory_id"), &GridInventory::get_inventory_id);
	ClassDB::bind_method(D_METHOD("get_slot_count"), &GridInventory::get_slot_count);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "inventory_id"), "set_inventory_id", "get_inventory_id");
	ClassDB::bind_method(D_METHOD("get_item_at", "cell"), &GridInventory::get_item_at);
	ClassDB::bind_method(D_METHOD("set_selected_cell", "cell"), &GridInventory::set_selected_cell);
	ClassDB::bind_method(D_METHOD("get_selected_cell"), &GridInventory::get_selected_cell);
	ClassDB::bind_method(D_METHOD("set_show_item_count", "enabled"), &GridInventory::set_show_item_count);
	ClassDB::bind_method(D_METHOD("is_show_item_count"), &GridInventory::is_show_item_count);
	ClassDB::bind_method(D_METHOD("set_interaction_enabled", "enabled"), &GridInventory::set_interaction_enabled);
	ClassDB::bind_method(D_METHOD("is_interaction_enabled"), &GridInventory::is_interaction_enabled);
	ClassDB::bind_method(D_METHOD("get_rows"), &GridInventory::get_rows);
	ClassDB::bind_method(D_METHOD("set_rows", "value"), &GridInventory::set_rows);
	ClassDB::bind_method(D_METHOD("get_columns"), &GridInventory::get_columns);
	ClassDB::bind_method(D_METHOD("set_columns", "value"), &GridInventory::set_columns);
	ClassDB::bind_method(D_METHOD("get_slot_size"), &GridInventory::get_slot_size);
	ClassDB::bind_method(D_METHOD("set_slot_size", "value"), &GridInventory::set_slot_size);
	ClassDB::bind_method(D_METHOD("get_slot_margin"), &GridInventory::get_slot_margin);
	ClassDB::bind_method(D_METHOD("set_slot_margin", "value"), &GridInventory::set_slot_margin);
	ClassDB::bind_method(D_METHOD("get_grid_padding"), &GridInventory::get_grid_padding);
	ClassDB::bind_method(D_METHOD("set_grid_padding", "value"), &GridInventory::set_grid_padding);
	ClassDB::bind_method(D_METHOD("get_background"), &GridInventory::get_background);
	ClassDB::bind_method(D_METHOD("set_background", "value"), &GridInventory::set_background);
	ClassDB::bind_method(D_METHOD("get_item_frame"), &GridInventory::get_item_frame);
	ClassDB::bind_method(D_METHOD("set_item_frame", "value"), &GridInventory::set_item_frame);
	ClassDB::bind_method(D_METHOD("get_item_frame_hover"), &GridInventory::get_item_frame_hover);
	ClassDB::bind_method(D_METHOD("set_item_frame_hover", "value"), &GridInventory::set_item_frame_hover);
	ClassDB::bind_method(D_METHOD("get_item_frame_selected"), &GridInventory::get_item_frame_selected);
	ClassDB::bind_method(D_METHOD("set_item_frame_selected", "value"), &GridInventory::set_item_frame_selected);
	ClassDB::bind_method(D_METHOD("get_count_label_settings"), &GridInventory::get_count_label_settings);
	ClassDB::bind_method(D_METHOD("set_count_label_settings", "value"), &GridInventory::set_count_label_settings);

	ADD_SIGNAL(MethodInfo("slot_clicked", PropertyInfo(Variant::VECTOR2I, "cell"), PropertyInfo(Variant::INT, "button_index")));
	ADD_SIGNAL(MethodInfo("item_changed", PropertyInfo(Variant::VECTOR2I, "cell"), PropertyInfo(Variant::OBJECT, "item", PROPERTY_HINT_RESOURCE_TYPE, "ItemView")));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "rows", PROPERTY_HINT_RANGE, "1,20,1"), "set_rows", "get_rows");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "columns", PROPERTY_HINT_RANGE, "1,20,1"), "set_columns", "get_columns");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "slot_size"), "set_slot_size", "get_slot_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "slot_margin"), "set_slot_margin", "get_slot_margin");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "grid_padding"), "set_grid_padding", "get_grid_padding");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_item_count"), "set_show_item_count", "is_show_item_count");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "interaction_enabled"), "set_interaction_enabled", "is_interaction_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "background", PROPERTY_HINT_RESOURCE_TYPE, "StyleBox"), "set_background", "get_background");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "item_frame", PROPERTY_HINT_RESOURCE_TYPE, "StyleBox"), "set_item_frame", "get_item_frame");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "item_frame_hover", PROPERTY_HINT_RESOURCE_TYPE, "StyleBox"), "set_item_frame_hover", "get_item_frame_hover");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "item_frame_selected", PROPERTY_HINT_RESOURCE_TYPE, "StyleBox"), "set_item_frame_selected", "get_item_frame_selected");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "count_label_settings", PROPERTY_HINT_RESOURCE_TYPE, "LabelSettings"), "set_count_label_settings", "get_count_label_settings");
}

} // namespace godot
