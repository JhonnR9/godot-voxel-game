#include "biome_registry.h"
#include "biome_rarity.h"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <set>

namespace godot {
namespace {
struct Reader {
	String &error;

	void fail(const String &message) {
		if (error.is_empty())
			error = message;
	}

	double number(const Dictionary &d, const char *key, double fallback, double lo, double hi) {
		Variant v = d.get(key, fallback);
		if (v.get_type() != Variant::INT && v.get_type() != Variant::FLOAT) {
			fail(String(key) + ": expected a number");
			return fallback;
		}
		double n = v;
		if (!std::isfinite(n) || n < lo || n > hi) {
			fail(String(key) + ": number outside allowed range");
			return fallback;
		}
		return n;
	}

	int integer(const Dictionary &d, const char *key, int fallback, int lo, int hi) {
		double n = number(d, key, fallback, lo, hi);
		if (std::floor(n) != n)
			fail(String(key) + ": expected an integer");
		return static_cast<int>(n);
	}

	Dictionary object(const Dictionary &d, const char *key) {
		Variant v = d.get(key, Dictionary());
		if (v.get_type() != Variant::DICTIONARY) {
			fail(String(key) + ": expected an object");
			return {};
		}
		return v;
	}

	std::vector<Variant> array(const Dictionary &d, const char *key) {
		Variant v = d.get(key, Array());
		if (v.get_type() != Variant::ARRAY) {
			fail(String(key) + ": expected an array");
			return {};
		}
		Array values = v;
		std::vector<Variant> result;
		if (values.size() > 256) {
			fail(String(key) + ": maximum 256 entries");
			return {};
		}
		result.reserve(values.size());
		for (int i = 0; i < values.size(); ++i)
			result.push_back(values[i]);
		return result;
	}

	String string(const Dictionary &d, const char *key, const String &fallback) {
		Variant v = d.get(key, fallback);
		if (v.get_type() != Variant::STRING) {
			fail(String(key) + ": expected a string");
			return fallback;
		}
		return v;
	}

	bool boolean(const Dictionary &d, const char *key, bool fallback) {
		Variant v = d.get(key, fallback);
		if (v.get_type() != Variant::BOOL) {
			fail(String(key) + ": expected a boolean");
			return fallback;
		}
		return v;
	}

	uint16_t block(const Dictionary &d, const char *key, const char *fallback) {
		const String name = string(d, key, fallback);
		const uint16_t id = voxel::block_id_from_name(name.utf8().get_data());
		if (id == 0xffff || id == 0) {
			fail(String(key) + ": unknown or empty block '" + name + "'");
			return voxel::block_ids::stone;
		}
		return id;
	}

	void climate(const Dictionary &d, float &lo, float &hi) {
		lo = number(d, "climate_min", lo, 0, 1);
		hi = number(d, "climate_max", hi, 0, 1);
		if (lo >= hi)
			fail("climate_min must be lower than climate_max");
	}

	std::vector<WeightedPlant> plants(const Dictionary &d, const char *key) {
		std::vector<WeightedPlant> result;
		for (const Variant &v : array(d, key)) {
			if (v.get_type() != Variant::DICTIONARY) {
				fail("Plant entry must be an object");
				continue;
			}
			Dictionary p = v;
			WeightedPlant plant;
			plant.block		 = block(p, "block", "air");
			plant.weight	 = integer(p, "weight", 1, 1, 10000);
			plant.min_height = integer(p, "min_height", 1, 1, 8);
			plant.max_height = integer(p, "max_height", plant.min_height, plant.min_height, 8);
			if (!voxel::is_collidable(voxel::make_block(plant.block)) &&
				!voxel::has_flag(voxel::default_block_flags(plant.block), voxel::BLOCK_FLAG_CROSSED))
				fail("Vegetation blocks must be solid or crossed plants");
			result.push_back(plant);
		}
		return result;
	}
};

bool matches(float climate, float lo, float hi) {
	return climate >= lo && (climate < hi || (hi == 1.0f && climate == 1.0f));
}
} //namespace

const BiomeDefinition &BiomeRegistry::fallback_land(float climate) const {
	const BiomeDefinition *best = nullptr;
	for (const auto &b : biomes)
		if (b.kind == BiomeKind::LAND && b.rarity == 1 &&
			(!best || std::abs(b.height_anchor - climate) < std::abs(best->height_anchor - climate)))
			best = &b;
	return *best; // Validation requires at least one always available land biome.
}

const BiomeDefinition &BiomeRegistry::land_at(float climate, int64_t seed, int32_t x, int32_t z) const {
	for (const auto &b : biomes)
		if (b.kind == BiomeKind::LAND && matches(climate, b.climate_min, b.climate_max) &&
			voxel::biome_presence(b.rarity, seed, b.id, x, z) >= 0.5f)
			return b;
	return fallback_land(climate);
}

const BiomeDefinition *BiomeRegistry::overlay_at(BiomeKind kind, float climate, int64_t seed, int32_t x, int32_t z) const {
	const BiomeDefinition *best = nullptr;
	for (const auto &b : biomes)
		if (b.kind == kind && matches(climate, b.climate_min, b.climate_max) &&
			voxel::biome_presence(b.rarity, seed, b.id, x, z) >= 0.5f && (!best || b.priority > best->priority))
			best = &b;
	return best;
}

void BiomeRegistry::relief_pair(float climate, const BiomeDefinition *&a, const BiomeDefinition *&b, float &blend) const {
	a = b = nullptr;
	for (const auto &entry : biomes)
		if (entry.kind == BiomeKind::LAND) {
			if (entry.height_anchor <= climate && (!a || entry.height_anchor > a->height_anchor))
				a = &entry;
			if (entry.height_anchor >= climate && (!b || entry.height_anchor < b->height_anchor))
				b = &entry;
		}
	if (!a)
		a = b;
	if (!b)
		b = a;
	blend = a == b ? 0.0f : (climate - a->height_anchor) / (b->height_anchor - a->height_anchor);
}

int BiomeRegistry::max_tree_candidates() const {
	int limit = 0;
	for (const auto &b : biomes)
		if (b.rarity != 0)
			limit = std::max(limit, b.trees.max_per_chunk);
	return limit;
}

std::shared_ptr<const BiomeRegistry> BiomeRegistry::from_dictionary(const Dictionary &data, String &error) {
	error = String();
	Reader r{ error };
	if (r.integer(data, "version", 0, 0, 65535) != 1)
		r.fail("Unsupported biome registry version (expected 1)");
	auto registry		   = std::make_shared<BiomeRegistry>();
	auto &w				   = registry->world;
	const Dictionary world = r.object(data, "world");
	w.base_height		   = r.integer(world, "base_height", 24, WORLD_BEDROCK_Y + 16, 239);
	w.sea_level			   = r.integer(world, "sea_level", 24, WORLD_BEDROCK_Y + 16, 239);
	w.amplitude			   = r.number(world, "amplitude", 9, 0, 128);
	w.climate_start		   = r.number(world, "climate_start", -0.90, -1, 1);
	w.climate_span		   = r.number(world, "climate_span", 2.0, 0.001, 2);
	w.coast_start		   = r.number(world, "coast_start", 0.25, -1, 1);
	w.coast_span		   = r.number(world, "coast_span", 0.80, 0.001, 2);
	w.dry_coast_start	   = r.number(world, "dry_coast_start", 0.05, -1, 1);
	w.dry_coast_span	   = r.number(world, "dry_coast_span", 0.65, 0.001, 2);
	w.coast_blend_start	   = r.number(world, "coast_blend_start", 0.34, 0, 1);
	w.coast_blend_end	   = r.number(world, "coast_blend_end", 0.68, 0, 1);
	if (w.coast_blend_start >= w.coast_blend_end)
		r.fail("Invalid coast blend interval");

	w.wet_coast_offset		= r.integer(world, "wet_coast_offset", -8, -64, 0);
	w.dry_coast_offset		= r.integer(world, "dry_coast_offset", 1, 0, 64);
	w.dry_coast_clamp		= r.number(world, "dry_coast_clamp", 0.85, 0, 1);
	w.river_width			= r.number(world, "river_width", 0.07, 0.001, 1);
	w.river_bed_offset		= r.integer(world, "river_bed_offset", -2, -64, -1);

	const Dictionary noises = r.object(world, "noises");
	auto noise				= [&](const char *name, TerrainNoiseProfile &profile) {
		Dictionary d	  = r.object(noises, name);
		profile.frequency = r.number(d, "frequency", profile.frequency, 0.000001, 1);
		profile.octaves	  = r.integer(d, "octaves", profile.octaves, 1, 8);
	};
	noise("terrain", w.terrain);
	noise("climate", w.climate);
	noise("mountain", w.mountain);
	noise("dune", w.dune);
	noise("ocean", w.ocean);
	noise("river", w.river);

	std::set<int> ids;
	std::set<std::string> names;
	for (const Variant &value : r.array(data, "biomes")) {
		if (value.get_type() != Variant::DICTIONARY) {
			r.fail("Biome entry must be an object");
			continue;
		}
		const Dictionary d = value;
		BiomeDefinition b;
		b.rarity = r.integer(d, "rarity", 1, 0, 10000);
		b.id	 = r.integer(d, "id", -1, 0, 65535);
		b.name	 = r.string(d, "name", "").utf8().get_data();
		if (b.name.empty() || !names.insert(b.name).second)
			r.fail("Biome names must be non-empty and unique");
		if (!ids.insert(b.id).second)
			r.fail("Duplicate biome ID");
		const Dictionary selection = r.object(d, "selection");
		const String kind		   = r.string(selection, "kind", "land");
		if (kind == "land")
			b.kind = BiomeKind::LAND;
		else if (kind == "ocean")
			b.kind = BiomeKind::OCEAN;
		else if (kind == "river")
			b.kind = BiomeKind::RIVER;
		else if (kind == "beach")
			b.kind = BiomeKind::BEACH;
		else
			r.fail("Unknown biome selection kind: " + kind);
		r.climate(selection, b.climate_min, b.climate_max);
		b.priority				  = r.integer(selection, "priority", 0, -1000, 1000);
		b.selection_height_offset = r.integer(selection, "height_max_offset", 2, -64, 64);
		b.selection_influence	  = r.number(selection, "min_influence", 0.5, 0, 1);

		const Dictionary relief	  = r.object(d, "relief");
		b.height_anchor			  = r.number(relief, "anchor", 0, 0, 1);
		b.height_scale			  = r.number(relief, "scale", 2.4, 0, 16);
		b.ridge_amplitude		  = r.number(relief, "ridge_amplitude", 19, 0, 128);
		b.ridge_power			  = r.number(relief, "ridge_power", 2, 0.1, 8);
		b.height_bias			  = r.number(relief, "bias", -5, -128, 128);
		const String relief_noise = r.string(relief, "noise", "mountain");
		if (relief_noise == "mountain")
			b.relief_noise = ReliefNoise::MOUNTAIN;
		else if (relief_noise == "dune")
			b.relief_noise = ReliefNoise::DUNE;
		else if (relief_noise == "terrain")
			b.relief_noise = ReliefNoise::TERRAIN;
		else
			r.fail("Unknown relief noise: " + relief_noise);

		const Dictionary materials = r.object(d, "materials");
		b.surface				   = r.block(materials, "surface", "grass");
		b.soil					   = r.block(materials, "soil", "dirt");
		b.rock					   = r.block(materials, "rock", "stone");
		b.deep_rock				   = r.block(materials, "deep_rock", "deepslate");
		for (uint16_t id : { b.surface, b.soil, b.rock, b.deep_rock })
			if (!voxel::is_collidable(voxel::make_block(id)))
				r.fail("Terrain materials must be collidable blocks");
		b.soil_depth		= r.integer(d, "soil_depth", 15, 0, 128);
		b.deep_rock_below_y = r.integer(d, "deep_rock_below_y", -32, WORLD_BEDROCK_Y, 255);
		b.dry_coast			= r.boolean(d, "dry_coast", false);
		b.surface_water		= r.boolean(d, "surface_water", true);
		b.surface_fill = r.block(d, "surface_fill", "water");
		if (b.surface_fill != voxel::block_ids::water && !voxel::is_collidable(voxel::make_block(b.surface_fill)))
			r.fail("surface_fill must be water or a solid block");

		for (const Variant &v : r.array(d, "surface_overrides")) {
			if (v.get_type() != Variant::DICTIONARY) {
				r.fail("Surface override must be an object");
				continue;
			}
			Dictionary o = v;
			SurfaceOverride entry;
			r.climate(o, entry.climate_min, entry.climate_max);
			entry.surface = r.block(o, "surface", "dirt");
			if (!voxel::is_collidable(voxel::make_block(entry.surface)))
				r.fail("Surface override must be solid");
			entry.soil_depth = r.integer(o, "soil_depth", -1, -1, 128);
			b.surface_overrides.push_back(entry);
		}

		for (const Variant &v : r.array(d, "strata")) {
			if (v.get_type() != Variant::DICTIONARY) {
				r.fail("Stratum must be an object");
				continue;
			}
			Dictionary layer = v;
			TerrainStratum entry;
			entry.block = r.block(layer, "block", "stone");
			if (!voxel::is_collidable(voxel::make_block(entry.block)))
				r.fail("Stratum must be solid");
			entry.min_depth = r.integer(layer, "min_depth", 1, 1, 65535);
			entry.max_depth = r.integer(layer, "max_depth", 65535, entry.min_depth, 65535);
			entry.min_y		= r.integer(layer, "min_y", WORLD_BEDROCK_Y + 1, WORLD_BEDROCK_Y + 1, 255);
			entry.max_y		= r.integer(layer, "max_y", 255, entry.min_y, 255);
			b.strata.push_back(entry);
		}

		const Dictionary trees = r.object(d, "trees");
		const String tree_shape = r.string(trees, "shape", "oak");
		if (tree_shape == "oak")
			b.trees.shape = TreeProfile::Shape::OAK;
		else if (tree_shape == "palm")
			b.trees.shape = TreeProfile::Shape::PALM;
		else if (tree_shape == "pine")
			b.trees.shape = TreeProfile::Shape::PINE;
		else
			r.fail("Unknown tree shape: " + tree_shape);
		b.trees.max_per_chunk  = r.integer(trees, "max_per_chunk", 0, 0, 16);
		b.trees.min_height	   = r.integer(trees, "min_height", 5, 2, 24);
		b.trees.max_height	   = r.integer(trees, "max_height", std::max(7, b.trees.min_height), b.trees.min_height, 24);
		b.trees.crown_radius   = r.integer(trees, "crown_radius", 2, 1, 6);
		b.trees.trunk		   = r.block(trees, "trunk", "oak_log");
		b.trees.leaves		   = r.block(trees, "leaves", "oak_leaves");
		if (!voxel::is_collidable(voxel::make_block(b.trees.trunk)) || !voxel::is_collidable(voxel::make_block(b.trees.leaves)))
			r.fail("Tree trunk and leaves must be solid blocks");
		r.climate(trees, b.trees.climate_min, b.trees.climate_max);

		const Dictionary vegetation = r.object(d, "vegetation");
		b.vegetation.patch_size		= r.integer(vegetation, "patch_size", 12, 1, 256);
		b.vegetation.patch_chance = r.integer(vegetation, "patch_chance", 1000, 0, 1000);
		b.vegetation.cluster_radius = r.integer(vegetation, "cluster_radius", 0, 0, (b.vegetation.patch_size - 1) / 2);
		b.vegetation.coverage_min	= r.integer(vegetation, "coverage_min", 0, 0, 1000);
		b.vegetation.coverage_max	= r.integer(vegetation, "coverage_max", b.vegetation.coverage_min, b.vegetation.coverage_min, 1000);
		b.vegetation.flowers_min	= r.integer(vegetation, "flowers_min", 0, 0, b.vegetation.coverage_min);
		b.vegetation.flowers_max	= r.integer(vegetation, "flowers_max", b.vegetation.flowers_min, b.vegetation.flowers_min, b.vegetation.coverage_min);
		r.climate(vegetation, b.vegetation.climate_min, b.vegetation.climate_max);
		b.vegetation.plants	 = r.plants(vegetation, "plants");
		b.vegetation.flowers = r.plants(vegetation, "flowers");
		if (b.vegetation.coverage_max > 0 && b.vegetation.plants.empty())
			r.fail("Vegetation coverage requires plants");
		if (b.vegetation.flowers_max > 0 && b.vegetation.flowers.empty())
			r.fail("Flower coverage requires flowers");

		registry->biomes.push_back(std::move(b));
	}

	std::vector<const BiomeDefinition *> lands;
	for (const auto &b : registry->biomes)
		if (b.kind == BiomeKind::LAND)
			lands.push_back(&b);
	std::sort(lands.begin(), lands.end(), [](auto a, auto b) { return a->climate_min < b->climate_min; });

	float end = 0;
	std::set<float> anchors;
	for (const auto *b : lands) {
		if (b->climate_min != end)
			r.fail("Land climate intervals must cover [0,1] without gaps or overlaps");
		end = b->climate_max;
		if (!anchors.insert(b->height_anchor).second)
			r.fail("Land relief anchors must be unique");
	}

	bool guaranteed_land = false;
	for (const auto *b : lands)
		if (b->rarity == 1)
			guaranteed_land = true;
	if (!guaranteed_land)
		r.fail("At least one land biome must have rarity 1 for fallback terrain");
	if (lands.empty() || end != 1.0f)
		r.fail("Land climate intervals must cover [0,1]");

	if (!error.is_empty())
		return nullptr;
	return registry;
}

std::shared_ptr<const BiomeRegistry> BiomeRegistry::load(const String &path) {
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);

	String error;
	std::shared_ptr<const BiomeRegistry> result;

	if (file.is_null())
		error = "Cannot open biome registry: " + path;
	else {
		Variant parsed = JSON::parse_string(file->get_as_text());

		if (parsed.get_type() != Variant::DICTIONARY)
			error = "Biome registry must be a JSON object";
		else
			result = from_dictionary(parsed, error);
	}

	if (!result) {
		ERR_PRINT("Invalid biome registry '" + path + "': " + error + ". Using built-in defaults.");
		return defaults();
	}

	return result;
}

std::shared_ptr<const BiomeRegistry> BiomeRegistry::defaults() {
	static const auto fallback = []() {
		auto r = std::make_shared<BiomeRegistry>();
		BiomeDefinition mountains;
		mountains.name		   = "mountains";
		mountains.climate_min = 0.18f;
		mountains.height_anchor = 0.26f;
		mountains.climate_max = 0.38f;
		mountains.surface_overrides.push_back({ 0.58f, 1.0f, voxel::block_ids::dirt, 7 });
		mountains.trees.max_per_chunk	   = 4;
		mountains.trees.climate_max	   = 0.58f;
		mountains.vegetation.coverage_min = 85;
		mountains.vegetation.coverage_max = 159;
		mountains.vegetation.flowers_min  = 12;
		mountains.vegetation.flowers_max  = 20;
		mountains.vegetation.climate_max  = 0.58f;
		mountains.vegetation.plants	   = { { voxel::block_ids::short_grass, 62 }, { voxel::block_ids::tall_grass, 28 }, { voxel::block_ids::fern, 10 } };
		mountains.vegetation.flowers	   = { { voxel::block_ids::flower, 1 }, { voxel::block_ids::daisy, 1 }, { voxel::block_ids::cornflower, 1 }, { voxel::block_ids::poppy, 1 } };
		r->biomes.push_back(mountains);
		BiomeDefinition desert;
		desert.id			   = 1;
		desert.name			   = "desert";
		desert.climate_min	   = 0.68f;
		desert.height_anchor   = 1;
		desert.relief_noise	   = ReliefNoise::DUNE;
		desert.height_scale	   = 0.48f;
		desert.ridge_amplitude = 7;
		desert.ridge_power	   = 1;
		desert.height_bias	   = -2.5f;
		desert.surface = desert.soil   = voxel::block_ids::sand;
		desert.rock					   = voxel::block_ids::sandstone;
		desert.soil_depth			   = 4;
		desert.dry_coast			   = true;
		desert.surface_water		   = false;
		desert.vegetation.climate_min  = 0.97f;
		desert.vegetation.coverage_min = desert.vegetation.coverage_max = 6;
		desert.vegetation.plants										= { { voxel::block_ids::cactus, 1, 1, 3 } };
		r->biomes.push_back(desert);
		for (int i = 2; i <= 4; ++i) {
			BiomeDefinition b;
			b.id	  = i;
			b.name	  = i == 2 ? "ocean" : i == 3 ? "river"
												  : "beach";
			b.kind	  = i == 2 ? BiomeKind::OCEAN : i == 3 ? BiomeKind::RIVER
														   : BiomeKind::BEACH;
			b.surface = b.soil		  = voxel::block_ids::sand;
			b.soil_depth			  = i == 2 ? 5 : i == 3 ? 2
															: 3;
			b.selection_height_offset = i == 2 ? -4 : 2;
			b.selection_influence	  = i == 2 ? 0.0f : i == 4 ? 0.05f
															   : 0.5f;
			if (i == 4) {
				b.trees.shape = TreeProfile::Shape::PALM;
				b.trees.max_per_chunk = 2;
				b.trees.min_height = 7;
				b.trees.max_height = 10;
				b.trees.crown_radius = 4;
				b.trees.trunk = voxel::block_ids::palm_log;
				b.trees.leaves = voxel::block_ids::palm_leaves;
			}
			r->biomes.push_back(b);
		}
		BiomeDefinition snow;
		snow.id = 5;
		snow.name = "snow";
		snow.climate_max = 0.18f;
		snow.height_scale = 1.6f;
		snow.ridge_amplitude = 22;
		snow.height_bias = -3;
		snow.surface = voxel::block_ids::snow;
		snow.soil_depth = 6;
		snow.surface_fill = voxel::block_ids::ice;
		snow.trees.shape = TreeProfile::Shape::PINE;
		snow.trees.max_per_chunk = 4;
		snow.trees.min_height = 8;
		snow.trees.max_height = 12;
		snow.trees.crown_radius = 3;
		snow.trees.trunk = voxel::block_ids::pine_log;
		snow.trees.leaves = voxel::block_ids::pine_leaves;
		r->biomes.push_back(snow);
		for (int i = 2; i <= 4; ++i) {
			BiomeDefinition frozen = r->biomes[i];
			frozen.id = i + 4;
			frozen.name = i == 2 ? "frozen_ocean" : i == 3 ? "frozen_river" : "snowy_shore";
			frozen.climate_max = snow.climate_max;
			frozen.priority = 10;
			frozen.surface = voxel::block_ids::snow;
			frozen.surface_fill = voxel::block_ids::ice;
			frozen.trees = i == 4 ? snow.trees : TreeProfile{};
			if (i == 4) frozen.trees.max_per_chunk = 2;
			r->biomes.push_back(frozen);
		}
		BiomeDefinition plains;
		plains.id = 9;
		plains.name = "plains";
		plains.climate_min = 0.38f;
		plains.climate_max = 0.68f;
		plains.height_anchor = 0.48f;
		plains.relief_noise = ReliefNoise::TERRAIN;
		plains.height_scale = 0.22f;
		plains.ridge_amplitude = 2;
		plains.ridge_power = 1;
		plains.height_bias = 4;
		plains.soil_depth = 6;
		plains.surface_overrides.push_back({ 0.62f, 1.0f, voxel::block_ids::dirt, 4 });
		plains.trees.max_per_chunk = 1;
		plains.trees.min_height = 4;
		plains.trees.max_height = 6;
		plains.trees.climate_max = 0.58f;
		plains.vegetation.patch_size = 24;
		plains.vegetation.patch_chance = 180;
		plains.vegetation.cluster_radius = 3;
		plains.vegetation.coverage_min = 600;
		plains.vegetation.coverage_max = 850;
		plains.vegetation.climate_max = 0.62f;
		plains.vegetation.plants = mountains.vegetation.flowers;
		r->biomes.push_back(plains);
		return r;
	}();
	return fallback;
}
} // namespace godot
