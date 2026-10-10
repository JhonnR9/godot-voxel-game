#include "terrain_sampler.h"
#include "biome_rarity.h"
#include "terrain_transition.h"
#include <algorithm>
#include <godot_cpp/core/math.hpp>
namespace godot {
namespace {
float smooth(float t) {
	t = Math::clamp(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}
float noise(const Ref<FastNoiseLite> &n, int x, int z, float fallback = 0) { return n.is_valid() ? n->get_noise_2d(x, z) : fallback; }
bool matches(float t, float lo, float hi) { return t >= lo && (t < hi || (hi == 1 && t == 1)); }
} //namespace
std::shared_ptr<const std::vector<ColumnGenerationData>> TerrainColumnCache::get(const Vector3i &pos, const TerrainSettings &settings) {
    const uint64_t key = (uint64_t(uint32_t(pos.x)) << 32) | uint32_t(pos.z);
    {
        std::lock_guard lock(mutex);
        auto found = entries.find(key);
        if (found != entries.end()) return found->second;
    }
    // Noise and biome sampling stay outside the shared lock.
    auto sampled = std::make_shared<std::vector<ColumnGenerationData>>(Chunk::SIZE_X * Chunk::SIZE_Z);
    for (int z = 0; z < Chunk::SIZE_Z; ++z)
        for (int x = 0; x < Chunk::SIZE_X; ++x)
            (*sampled)[z * Chunk::SIZE_X + x] = TerrainSampler::sample(settings,
                pos.x * Chunk::SIZE_X + x, pos.z * Chunk::SIZE_Z + z);
    std::lock_guard lock(mutex);
    auto found = entries.find(key);
    if (found != entries.end()) return found->second;
    while (entries.size() >= 256) { entries.erase(order.front()); order.pop_front(); }
    order.push_back(key);
    entries.emplace(key, sampled);
    return sampled;
}

ColumnGenerationData TerrainSampler::sample(const TerrainSettings &s, int32_t x, int32_t z) {
	const BiomeRegistry &registry = s.biome_registry ? *s.biome_registry : *BiomeRegistry::defaults();
	const auto &w				  = registry.world;
	ColumnGenerationData c;
	const float terrain = noise(s.terrain_noise, x, z), mountain = noise(s.mountain_noise, x, z, terrain), dune = noise(s.dune_noise, x, z);
	c.climate_weight = smooth((noise(s.biome_noise, x, z) - w.climate_start) / w.climate_span);
	c.ocean_weight	 = noise(s.ocean_noise, x, z) + terrain * 0.16f;
	c.river_weight	 = noise(s.river_noise, x, z, 1);
	// Only the palette is perturbed: relief continues to follow smooth climate.
	const float material_climate = Math::clamp(c.climate_weight +
		(voxel::transition_patch(s.world_seed, x, z) - 0.5f) * 0.20f, 0.0f, 1.0f);
	const auto &land = registry.land_at(material_climate, s.world_seed, x, z);
	const BiomeDefinition *a, *b;
	float blend;
	registry.relief_pair(c.climate_weight, a, b, blend);
	auto relief = [&](const BiomeDefinition &p) {
		const float v = p.relief_noise == ReliefNoise::DUNE ? dune : p.relief_noise == ReliefNoise::TERRAIN ? terrain
																											: mountain;
		return Math::pow(1.0f - Math::abs(v), p.ridge_power) * p.ridge_amplitude + p.height_bias;
	};
	const auto &fallback = registry.fallback_land(c.climate_weight);
	auto availability	 = [&](const BiomeDefinition &p) { return voxel::biome_presence(p.rarity, s.world_seed, p.id, x, z); };
	const float pa = availability(*a), pb = availability(*b);
	const float ra	  = pa == 1 ? relief(*a) : Math::lerp(relief(fallback), relief(*a), pa);
	const float rb	  = pb == 1 ? relief(*b) : Math::lerp(relief(fallback), relief(*b), pb);
	const float sa	  = pa == 1 ? a->height_scale : Math::lerp(fallback.height_scale, a->height_scale, pa);
	const float sb	  = pb == 1 ? b->height_scale : Math::lerp(fallback.height_scale, b->height_scale, pb);
	c.height_offset	  = Math::round(Math::lerp(ra, rb, blend));
	c.height_scale	  = Math::lerp(sa, sb, blend);
	c.water_level	  = s.water_level;
	c.surface_height  = s.terrain_base_height + c.height_offset + Math::round(terrain * s.terrain_amplitude * c.height_scale);
	const auto &coast_a = registry.land_at(Math::clamp(c.climate_weight - 0.10f, 0.0f, 1.0f), s.world_seed, x, z);
	const auto &coast_b = registry.land_at(Math::clamp(c.climate_weight + 0.10f, 0.0f, 1.0f), s.world_seed, x, z);
	const float coast_mix = smooth((c.climate_weight - coast_b.climate_min + 0.10f) / 0.20f);
	const float dry = Math::lerp(float(coast_a.dry_coast), float(coast_b.dry_coast), coast_mix);
	const float ocean = smooth((Math::lerp(w.coast_start, w.dry_coast_start, dry) - c.ocean_weight) /
		Math::lerp(w.coast_span, w.dry_coast_span, dry));
	const float coast = voxel::coast_height(float(c.water_level + w.wet_coast_offset),
		float(c.water_level + w.dry_coast_offset), dry, ocean);
	c.surface_height  = Math::round(Math::lerp(float(c.surface_height), coast, ocean));
	const BiomeDefinition *chosen = &land;
	{
		const auto *o	  = registry.overlay_at(BiomeKind::OCEAN, material_climate, s.world_seed, x, z);
		const auto *r	  = registry.overlay_at(BiomeKind::RIVER, material_climate, s.world_seed, x, z);
		const auto *beach = registry.overlay_at(BiomeKind::BEACH, material_climate, s.world_seed, x, z);
		if (o && c.surface_height <= c.water_level + o->selection_height_offset && ocean >= o->selection_influence)
			chosen = o;
		else if (!land.dry_coast && r && c.surface_height <= c.water_level + r->selection_height_offset) {
			const float influence = smooth(1.0f - Math::abs(c.river_weight) / w.river_width);
			const int bed		  = std::min(c.surface_height, c.water_level + w.river_bed_offset);
			c.surface_height	  = Math::round(Math::lerp(float(c.surface_height), float(bed), influence));
			if (influence > r->selection_influence && c.surface_height <= c.water_level + w.river_bed_offset)
				chosen = r;
		}

		if (chosen == &land && beach && ocean > beach->selection_influence && c.surface_height <= c.water_level + beach->selection_height_offset)
			chosen = beach;
	}
	c.surface_height	= std::clamp(c.surface_height, WORLD_BEDROCK_Y + 1, WORLD_MAX_CHUNK_Y * Chunk::SIZE_Y + Chunk::MAX_Y);
	c.definition		= chosen;
	c.biome_id			= chosen->id;
	c.surface_block		= voxel::make_block(chosen->surface);
	c.subsurface_block	= voxel::make_block(chosen->soil);
	c.stone_block		= voxel::make_block(chosen->rock);
	c.deep_stone_block	= voxel::make_block(chosen->deep_rock);
	c.subsurface_depth	= chosen->soil_depth;
	c.deep_rock_below_y = chosen->deep_rock_below_y;
	c.surface_water		= chosen->surface_water;
	c.surface_fill = chosen->surface_fill;
	c.solid_fill_height = c.surface_fill == voxel::block_ids::water ? c.surface_height : c.water_level - 1;
	// Freeze independently of the ground palette so the shoreline develops
	// melting terraces anchored to the bed rather than a hard biome boundary.
	constexpr float freezing_band = 0.14f;
	auto fill_profile = [&](float climate) -> const BiomeDefinition * {
		climate = Math::clamp(climate, 0.0f, 1.0f);
		return chosen->kind == BiomeKind::LAND ? &registry.land_at(climate, s.world_seed, x, z)
			: registry.overlay_at(chosen->kind, climate, s.world_seed, x, z);
	};
	const auto *cold = fill_profile(c.climate_weight - freezing_band);
	const auto *warm = fill_profile(c.climate_weight + freezing_band);
	if (c.surface_water && cold && warm && cold->surface_fill == voxel::block_ids::ice &&
		warm->surface_fill == voxel::block_ids::water) {
		const float freeze = voxel::freezing_weight(c.climate_weight, cold->climate_max, freezing_band);
		const float patch = voxel::transition_patch(s.world_seed ^ 0x1CE, x, z);
		c.solid_fill_height = voxel::freezing_height(freeze, patch, c.surface_height, c.water_level);
		c.surface_fill = c.solid_fill_height > c.surface_height ? voxel::block_ids::ice : voxel::block_ids::water;
	}
	for (const auto &o : chosen->surface_overrides)
		if (matches(material_climate, o.climate_min, o.climate_max)) {
			c.surface_block = voxel::make_block(o.surface);
			if (o.soil_depth >= 0)
				c.subsurface_depth = o.soil_depth;
			break;
		}
	if (chosen == &land) {
		auto soil_depth = [&](const BiomeDefinition &p) {
			for (const auto &o : p.surface_overrides)
				if (matches(material_climate, o.climate_min, o.climate_max))
					return o.soil_depth >= 0 ? o.soil_depth : p.soil_depth;
			return p.soil_depth;
		};
		c.subsurface_depth = Math::round(Math::lerp(float(soil_depth(coast_a)), float(soil_depth(coast_b)), coast_mix));
		// Sand cover varies with the dunes instead of exposing sandstone at
		// exactly the same depth everywhere along the dry terrain boundary.
		if (chosen->soil == voxel::block_ids::sand && chosen->rock == voxel::block_ids::sandstone && c.subsurface_depth > 0)
			c.subsurface_depth = std::max(1, c.subsurface_depth + int(Math::round(dune * 2.0f)));
	}
	c.trees_allowed = chosen->trees.max_per_chunk > 0 && matches(c.climate_weight, chosen->trees.climate_min, chosen->trees.climate_max);
	return c;
}

voxel::Block TerrainSampler::block_at(const ColumnGenerationData &c, int32_t y) {
	if (y == WORLD_BEDROCK_Y)
		return voxel::make_block(voxel::block_ids::bedrock);
	if (y > c.surface_height)
		return 0;
	const int depth = c.surface_height - y;
	if (depth == 0)
		return c.surface_block;
	if (depth <= c.subsurface_depth)
		return c.subsurface_block;
	if (c.definition)
		for (const auto &layer : c.definition->strata)
			if (depth >= layer.min_depth && depth <= layer.max_depth && y >= layer.min_y && y <= layer.max_y)
				return voxel::make_block(layer.block);
	return y < c.deep_rock_below_y ? c.deep_stone_block : c.stone_block;
}
} //namespace godot
