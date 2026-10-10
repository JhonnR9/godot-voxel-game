# Stone, emission, and caves

## Textures

Stone now uses `res://textures/blocks/stone.png` on all three faces, with a neutral grain matching the rock in ore textures. Deepslate and bedrock reuse this image with their existing tints. Coal uses the new `res://textures/blocks/coal_ore.png`, with small dark graphite inclusions. PNGs are 32 × 32 RGBA. The coal block uses ID 22; earlier IDs are preserved. The registry, atlas, C++ header, and icon cache are updated.

Both images were generated with the integrated `image_gen` tool, using iron and diamond as style references, and downscaled with nearest-neighbor filtering. Full prompts and final paths are in `project/art/underground_texture_prompts.json`.

## Emission

`chunk.gdshader` identifies only the iron and diamond layers. Two analytical masks in linear RGB select warm copper/tan pixels and cool cyan pixels, respectively. The surrounding gray rock does not emit. This technique requires no mask textures, extra atlas samples, or per-block lights. Initial intensities are 0.65 for iron and 1.25 for diamond, editable through the shader's `iron_emission` and `diamond_emission` uniforms.

The scene enables subtle glow (intensity 0.18, HDR threshold 1.1) for the brightest points. Emission makes ore visible in the dark; it is not a point light illuminating neighboring walls. The halo depends on the renderer's glow support. Lighting and the night cycle remain active.

Layer indices come from the generated C++ function `texture_layer_from_name()`; adding/reordering textures does not change which ore emits. `ChunkNode` also preserves its materials when receiving collisions and when reused by the pool.

Emission reference: [Godot spatial shader reference](https://docs.godotengine.org/en/stable/tutorials/shaders/shader_reference/spatial_shader.html).

## Generation

Tunnels are now approximately 7–10 blocks wide and 6–8 high, retaining curves, branches, ceilings, and bedrock. Underground deposits are compact pockets, deterministic from the seed and world coordinates. Iron is more common and coal appears across a broad depth range. Diamond remains rare and deep. Dirt forms small patches in walls, floors, and ceilings where a pocket intersects a tunnel. The geological layer replaces only solid rock, without closing passages or filling air.

Terrain is regenerated on load and receives saved edits afterward. A new world is the best way to see the distribution without interference from earlier mining. Ores and stone can be inspected immediately in the inventory.

## Verification

SCons compilation, Godot import, and the tests below passed. The integration test loads a real world in headless mode and confirms tunnels, iron, coal, and dirt in an underground region of 262,144 blocks. Mask tests confirm that only colored inclusions receive emission. Headless verification does not assess the final glow halo on a monitor with graphical rendering.

```sh
c++ -std=c++17 -O2 -Wall -Wextra -pedantic tests/underground_deposits_test.cpp -o /tmp/underground_deposits_test
/tmp/underground_deposits_test
c++ -std=c++17 -O2 -Wall -Wextra -pedantic tests/cave_tunnels_test.cpp -o /tmp/cave_tunnels_test
/tmp/cave_tunnels_test
XDG_DATA_HOME=/tmp/godot-underground-emission-tests godot --headless --path project --log-file /tmp/godot-emission-tests.log --script res://tests/ore_emission_test.gd
XDG_DATA_HOME=/tmp/godot-underground-world-tests godot --headless --path project --log-file /tmp/godot-underground-world.log --script res://tests/underground_world_test.gd
```

The world test creates a test save. Always use an isolated `XDG_DATA_HOME` directory, as in the commands above, to keep player saves separate.
