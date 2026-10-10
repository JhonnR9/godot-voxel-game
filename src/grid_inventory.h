#ifndef GRID_INVENTORY_H
#define GRID_INVENTORY_H

#include "item_view.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label_settings.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <vector>

namespace godot {

class Label;

class Panel;

class TextureRect;

class InputEvent;

class GridInventory final : public Control {
	GDCLASS(GridInventory, Control)

	struct Slot {
		Rect2i rect;
		Ref<ItemView> item;
		TextureRect *icon  = nullptr;
		Panel *panel	   = nullptr;
		Label *count_label = nullptr;
		int column		   = 0;
		int row			   = 0;
	};

	static constexpr int64_t INVALID_KEY = -1;
	HashMap<int64_t, Slot> _cells;
	int _rows				  = 1;
	int _columns			  = 9;

	Size2i _slot_size		  = Size2i(64, 64);
	Size2i _slot_margin		  = Size2i(3, 3);
	Size2i _grid_padding	  = Size2i(6, 6);
	Point2i _selected_cell	  = Point2i(-1, -1);
	int64_t _hovered_key	  = INVALID_KEY;
	bool _show_item_count	  = true;
	bool _interaction_enabled = true;
	String _inventory_id;
	std::vector<Ref<ItemView>> _items;
	int _hidden_slot = -1;
	Dictionary _drag_payload;
	bool _drop_allowed = false;

	int _slot_index(const Point2i &cell) const;

	Ref<StyleBox> _background;
	Ref<StyleBox> _item_frame;
	Ref<StyleBox> _item_frame_hover;
	Ref<StyleBox> _item_frame_selected;
	Ref<LabelSettings> _count_label_settings;

	void _draw_background();
	void _generate_grid();
	void _clear_grid();
	Panel *_create_slot_panel(const Point2i &position);
	Label *_create_count_label();
	void _sync_slot(Slot &slot);
	void _apply_slot_style(Slot &slot);
	void _connect_style_signal(const Ref<StyleBox> &style);
	void _disconnect_style_signal(const Ref<StyleBox> &style);
	int64_t _make_key(int column, int row) const;
	Variant _make_drag_data(const Vector2 &position, const Point2i &cell);
	bool _accept_drop_data(const Vector2 &position, const Variant &data, const Point2i &cell);
	void _handle_drop_data(const Vector2 &position, const Variant &data, const Point2i &cell);
	void _on_slot_gui_input(InputEvent *event, const Point2i &cell);
	void _on_style_changed();
	void _on_label_settings_changed();
	void _on_slot_mouse_entered(const Point2i &cell);
	void _on_slot_mouse_exited(const Point2i &cell);

protected:
	static void _bind_methods();
	void _notification(int what);

public:
	void set_inventory_id(const String &uuid);
	String get_inventory_id() const { return _inventory_id; }
	int get_slot_count() const;
	Size2 _get_minimum_size() const override;
	Ref<ItemView> get_item_at(const Point2i &cell) const;
	void set_slot_count(int count);
	void set_item_view(int index, const Ref<ItemView> &item);
	void set_hidden_slot(int index);
	void set_drag_payload(const Dictionary &payload) { _drag_payload = payload; }
	void set_drop_allowed(bool allowed) { _drop_allowed = allowed; }
	void set_selected_cell(const Point2i &cell);
	Point2i get_selected_cell() const { return _selected_cell; }
	void set_show_item_count(bool enabled);
	bool is_show_item_count() const { return _show_item_count; }
	void set_interaction_enabled(bool enabled);
	bool is_interaction_enabled() const { return _interaction_enabled; }

	int get_rows() const { return _rows; }
	void set_rows(int value);
	int get_columns() const { return _columns; }
	void set_columns(int value);
	Size2i get_slot_size() const { return _slot_size; }
	void set_slot_size(const Size2i &value);
	Size2i get_slot_margin() const { return _slot_margin; }
	void set_slot_margin(const Size2i &value);
	Size2i get_grid_padding() const { return _grid_padding; }
	void set_grid_padding(const Size2i &value);

	Ref<StyleBox> get_background() const { return _background; }
	void set_background(const Ref<StyleBox> &value);
	Ref<StyleBox> get_item_frame() const { return _item_frame; }
	void set_item_frame(const Ref<StyleBox> &value);
	Ref<StyleBox> get_item_frame_hover() const { return _item_frame_hover; }
	void set_item_frame_hover(const Ref<StyleBox> &value);
	Ref<StyleBox> get_item_frame_selected() const { return _item_frame_selected; }
	void set_item_frame_selected(const Ref<StyleBox> &value);
	Ref<LabelSettings> get_count_label_settings() const { return _count_label_settings; }
	void set_count_label_settings(const Ref<LabelSettings> &value);
};

} // namespace godot

#endif // GRID_INVENTORY_H
