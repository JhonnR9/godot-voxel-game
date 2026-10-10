# Voxel object selection and collection

Objects with `BLOCK_FLAG_CROSSED` (or the registry's default flag) automatically receive selectors during mesh construction. This includes torches, flowers, and grasses. New crossed objects follow the same path.

`ChunkMeshBuilder` produces local object positions; the asynchronous result carries this data to `ChunkNode`. `set_selection_positions` manages a `StaticBody3D` named `ObjectSelection` per chunk, with one shape owner per object and a single shared box. There are no collision nodes per plant or dependencies on light nodes.

Layer 1 represents terrain collision; layer 2 represents object selection, with a zero mask. The player queries both for interaction while keeping movement on layer 1. Boxes occupy the entire voxel to make aiming easier, including transparent parts of the textures.

The raycast's `shape` index corresponds to the body's `voxel_selection_positions` list. The player converts the local origin to world coordinates to break exactly the hit voxel, including in distant chunks or negative coordinates. When placing another block, the player adds the normal to that voxel's origin.

Collecting selectable objects adds one unit to the hotbar or inventory before removing them. If both are full, the object remains in the world. Solid blocks retain the existing breaking behavior.

Identical positions preserve selectors during mesh updates. Removal, unloading, and chunk reuse clear the shapes; the same cleanup occurs when chunks become empty.

Validation: `torch_test.gd` checks selection of all seven plants, voxel mapping, layer separation, removal after breaking, collection, torches, and persistence. Inventory and pipeline tests also pass in headless mode.
