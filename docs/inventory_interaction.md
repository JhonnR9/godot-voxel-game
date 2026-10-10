# Inventory and mouse control

F1 (`unlock_mouse`) switches between character control and a free mouse. With the mouse unlocked, movement, camera control, and block interaction are suspended; pressing F1 again returns control without closing windows. The world-save shortcut moves to F5 to avoid a conflict.

The closed chest in the upper-left corner opens or closes the player inventory. The creative button beside it controls a separate window. Both can remain open simultaneously. The hotbar retains its behavior and appearance.

Drag a window by its title bar; drag its lower-right corner to resize it. Position and size are saved in the local `inventory_windows` settings. Panels stay within the screen, retain scrolling for items that do not fit, and preserve items when the viewport is resized.

The GDScript `InventoryManager` keeps mouse-control state separate from window visibility. `is_inventory_open()` checks visibility; `is_mouse_unlocked()` determines whether the player can receive commands. The UI ignores mouse events while the mouse is captured, including internal scrolling controls; canceling interaction stops drags without consuming items.

`floating_inventory_panel.gd` handles dragging, resizing, and screen bounds. `inventory_ui.gd` handles content, independent windows, and layout persistence. F1 is processed in the player's `_input`, before the GUI, and does not repeat while held.

Validation: `inventory_mouse_test.gd` checks F1, launcher clicks, moving and resizing, persistence, event propagation, and character control with windows open. `inventory_ui_test.gd` covers drag-and-drop, stacking, a full inventory, saving, and viewport resizing.
