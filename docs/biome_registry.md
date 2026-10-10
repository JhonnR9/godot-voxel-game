# Biome and strata registry

`project/data/biome_registry.json` configures relief, materials, strata, trees, and vegetation. It includes mountains, plains, desert, ocean, river, beach, snow, and frozen coastal profiles.

`VoxelAPI` reads and validates the file once in `_ready()`. Each generation task receives a shared, immutable reference to the registry. Chunk generation performs no JSON/disk reads or configuration reloads. Restart the world/game after editing the file. To use another file, set `biome_registry_path` in the Inspector or before adding `VoxelAPI` to the tree.

An invalid configuration produces a message with the path and field/problem, and the game uses built-in default biomes. Validation without starting a world is available through `VoxelAPI.validate_biome_registry(dictionary)`, which returns `valid` and `error`. IDs and names must be unique; materials must exist in the block registry; ranges, frequencies, and limits are validated.

## Selection and transitions

The `climate` noise is converted to a weight between 0 and 1. Biomes of kind `land` partition this range using `selection.climate_min`/`climate_max`, without gaps or overlaps. The upper bound is exclusive except at 1. `relief.anchor` positions each relief profile on the same axis. Height is interpolated between the two neighboring anchors, avoiding steps between biomes. Anchors must be unique.

`selection.kind` also accepts `ocean`, `river`, and `beach`. These profiles are selected over the land biome according to height relative to sea level and the influence of the corresponding noise. `height_max_offset` and `min_influence` configure thresholds. `climate_min`/`climate_max` can restrict each coastal profile to certain climates. When multiple profiles of the same kind match, the highest `priority` wins; ties use file order.

Material selection uses smooth patches approximately eight blocks across, deterministic from the seed and world coordinates. A climate band of up to 0.10 on either side of boundaries mixes dirt/grass, sand, and rock without cutting into the relief. This also smooths `surface_overrides` boundaries.

Selection order is ocean, river, then beach. `dry_coast` keeps a shallow sandy shelf at the start of the coast and disables rivers in the desert. Coastal influence is interpolated between climates; the shelf gradually descends to the same ocean floor as wet coasts. Ocean and beach can replace desert, allowing water along the shore. `dry_coast_clamp` and `coast_blend_start/end` are accepted for compatibility but no longer control this transition. `surface_water` controls water filling independently of the biome ID/name.

Changes affect new chunks; previously saved chunks retain the earlier terrain.

Climate uses `climate_start: -0.9` and `climate_span: 2.0` to distribute noise across the three regions without saturating the extremes. In broad samples of seeds 42, 1234, and 2026, the temperate band covers approximately 56% of climate; snow and desert are close to 22% each, before coastal profiles. These values do not guarantee the same proportions in every local region. Plains grass, vegetation, and trees extend to climate 0.58; exposed dirt occupies the transition band near the desert.

The `world` section configures base height, sea level, amplitude, coastal transitions, riverbed depth, and the six noise resources. Seeds and their offsets remain deterministic per world. Final height is clamped to Y = -255 through 255, respecting bedrock and the world ceiling.

## Regional rarity

Each biome entry has `rarity`, an integer from 0 to 10000:

- `0`: disables the biome, including its relief profile.
- `1`: always available when climate/coastal conditions match.
- `2`, `4`, `8`, etc.: higher values make occurrences progressively rarer.

For example, `"rarity": 4` in the desert keeps the dry-climate requirement and additionally requires a region approved by rarity. This value is not an exact percentage of map area. Selection uses a smooth regional field with 256-block cells, calculated from the seed, biome ID, and world coordinates. There is no random draw per block/chunk or dependency on loading order.

If a land biome is rejected, the engine uses the land biome with `rarity: 1` whose relief anchor is closest to the current climate. At least one land biome must therefore retain `rarity: 1`; fallback uses the nearest guaranteed profile. Relief blends smoothly with this fallback at regional transitions. For ocean, river, and beach, selection skips rejected profiles and considers the next compatible profile.

A missing field is equivalent to `1`. Current entries start at `1` to preserve existing generation; change the value for biomes you want to make rare. Restart the game after editing. Changing the ID or seed changes the regional pattern.

## Materials and strata

Materials are block registry names, such as `grass`, `dirt`, `stone`, `sandstone`, and `deepslate`. For each column, precedence is:

1. Bedrock at the world base, Y = -256.
2. `materials.surface` at surface height.
3. `materials.soil` in the next `soil_depth` layers.
4. The first `strata` entry matching the depth and Y ranges.
5. `materials.deep_rock` below `deep_rock_below_y`.
6. `materials.rock` elsewhere.

`strata` accepts `block`, `min_depth`, `max_depth`, `min_y`, and `max_y`. Depth is the distance in blocks below that column's surface; Y is absolute height. Bounds are inclusive. Additional strata do not replace surface, soil, or bedrock. Example sandstone band below the soil:

```json
"strata": [
  {"block": "sandstone", "min_depth": 16, "max_depth": 24}
]
```

`surface_overrides` changes the surface and optionally `soil_depth` within a climate subrange. The first matching rule wins. The dirt band between plains and desert uses this mechanism.

Caves and deposits remain separate passes after strata filling. Deposits replace only stone/deepslate; a configured sandstone stratum does not receive these deposits. Tunnel and deposit rules remain in `src/cave_tunnels.h` and `src/underground_deposits.h`.

## Trees and plants

`trees` configures `max_per_chunk`, `min_height`, `max_height`, `crown_radius`, `trunk`, `leaves`, and an optional climate range. The maximum limits candidates per chunk column rather than guaranteeing an exact count: water and unsuitable biomes can reject candidates. Trees retain the five-layer crown of the current oak model with `shape: "oak"` (default). `shape: "palm"` uses a tall trunk with eight radial leaves that droop at the ends. Beach configures palms with `palm_log`/`palm_leaves`, heights from 7 to 10, and up to two candidates per chunk. Only dry columns above water receive trees; desert and ocean have no palm candidates. The original tree blocks are `oak_log`, `oak_leaves`, and `oak_wood`; IDs 8, 9, and 4 were preserved for saved worlds.

`shape: "pine"` generates overlapping branch skirts that taper to a tip. The `snow` biome covers climate `[0, 0.18)` and uses pines 8 to 12 blocks tall, with `pine_log` and `pine_leaves`. Mountains and plains share the temperate band `[0.18, 0.68)`. Relief and materials remain smoothed at transitions.

`surface_fill` configures filling below sea level: `water` (default) or a solid block. Cold regions use `ice`, a solid block with collision, throughout the volume that would normally receive water. The `frozen_ocean`, `frozen_river`, and `snowy_shore` profiles, with priority 10 and climate below 0.18, keep snowy banks and frozen water along coasts and rivers. Snowy shores receive pines; palms remain on warm beaches. The historical `surface_water` field still enables/disables filling, whether water or ice. `sample_terrain_column` also returns the `surface_fill` ID.

At the cold boundary, a climate band of 0.14 on each side mixes ice and water with spatial noise independent of the surface. Ice always starts at the bottom, but its top gradually descends in steps at the cold boundary. Water fills the space above those steps up to sea level. The query returns `solid_fill_height`, the maximum solid-ice height per column. This avoids both floating slabs and full-height vertical walls. The transition uses world coordinates and the seed, preserving continuity at chunk boundaries.

The old `plains` biome, with mountainous relief, is now called `mountains` and retains ID 0 and its relief/vegetation parameters. It occupies climate `[0.18, 0.38)`. The new `plains` (ID 9) occupies `[0.38, 0.68)`, uses low relief (`scale: 0.22`, `ridge_amplitude: 2`), and has at most one candidate tree per chunk. In the isolated relief test, heights range from 28 to 30; boundaries still interpolate with mountains, coasts, and desert.

Plains flowers use `patch_chance: 180` (18% of 24-block cells) and `cluster_radius: 3`, creating spaced circular groups. `patch_chance` accepts 0 to 1000; the default 1000 preserves other biomes. A zero `cluster_radius` disables clustering, while a positive value restricts vegetation to a deterministic circle within each cell; it must be less than half of `patch_size`. Plains have no tall grass or ferns in their plant list.

`vegetation` configures `patch_size`, `coverage_min`/`coverage_max`, and `flowers_min`/`flowers_max`. Coverage uses thousandths: 120 means 12% of eligible columns. Flowers are part of total coverage rather than added to it. `plants` and `flowers` are lists of `{block, weight, min_height, max_height}`; weights control relative frequency. Plants appear only above dry surfaces. Variable heights allow cacti or other stacked plants.

## Adding a biome

Duplicate a land entry, assign a new ID/name, and adjust neighboring entries' climate partition. For example: plains `[0, 0.35)`, forest `[0.35, 0.68)`, and desert `[0.68, 1]`, with anchors 0, 0.5, and 1. Then configure materials, soil, relief, trees, and plants. The generator needs no branches by ID. The system still uses one climate axis; independent temperature, humidity, or vertical biomes require extending selection.

In exports, include `data/*.json` in the filter for files not recognized as resources, alongside the resources/scenes used by the game. Alternative files outside that folder must also be included in the export filter.

## Queries and verification

`VoxelAPI.sample_terrain_column(Vector2i(x, z))` returns height, biome ID/name, climate weight, materials, soil depth, water level, and tree eligibility. Use it after `_ready()`; starting the world applies its seed. This query describes base terrain before tunnels, deposits, trees, and saved edits. `TerrainSampler` is shared by generation and queries inside/outside the chunk. Trees no longer use a different height formula at boundaries. Initial spawn also queries the actual terrain with the world's seed applied. Chunks at the ceiling/base do not wait for neighbors outside vertical bounds.

```sh
scons -j4
XDG_DATA_HOME=/tmp/godot-biome-check godot --headless --path project --script res://tests/biome_registry_test.gd --log-file /tmp/biome-check.log
python3 tests/run_tests.py --filter surface_vegetation
python3 tests/run_tests.py --filter rarity
# Tests derive rarity variants from the registry and log populations.
```

Tests check invalid configurations, a new biome with custom strata, heights and blocks at negative coordinates/boundaries, the five existing biomes, trees with configured crowns/materials across different generation orders, and spawn above tall custom relief.
