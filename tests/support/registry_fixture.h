#ifndef TEST_REGISTRY_FIXTURE_H
#define TEST_REGISTRY_FIXTURE_H

#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace test_registry {

struct Plant {
	uint16_t block;
	int weight, min_height, max_height;
};

struct Profile {
	int patch_size = 12, patch_chance = 1000, cluster_radius = 0;
	int coverage_min = 0, coverage_max = 0, flowers_min = 0, flowers_max = 0;
	std::vector<Plant> plants, flowers;
};

struct Biome {
	uint16_t id;
	std::string name;
	int rarity;
	float climate_max;
	std::string surface_fill;
};

struct Registry {
	int sea_level = 0, wet_coast_offset = 0, dry_coast_offset = 0;
	std::vector<Biome> biomes;
	std::map<std::string, Profile> profiles;
	std::map<uint16_t, uint32_t> flags;
	std::map<std::string, uint16_t> ids;
	explicit Registry(const char *path) {
		std::ifstream file(path);
		if (!file) throw std::runtime_error("Cannot open registry fixture; use python3 tests/run_tests.py");
		std::string line;
		while (std::getline(file, line)) {
			std::istringstream row(line);
			std::string kind, name;
			row >> kind;
			if (kind == "world") row >> sea_level >> wet_coast_offset >> dry_coast_offset;
			else if (kind == "block") {
				uint16_t id;
				uint32_t mask;
				row >> id >> name >> mask;
				ids[name] = id;
				flags[id] = mask;
			} else if (kind == "biome") {
				Biome b;
				row >> b.id >> b.name >> b.rarity >> b.climate_max >> b.surface_fill;
				biomes.push_back(b);
			} else if (kind == "profile") {
				row >> name;
				auto &p = profiles[name];
				row >> p.patch_size >> p.patch_chance >> p.cluster_radius >> p.coverage_min >> p.coverage_max >> p.flowers_min >> p.flowers_max;
			} else if (kind == "plant") {
				std::string role;
				Plant plant;
				row >> name >> role >> plant.block >> plant.weight >> plant.min_height >> plant.max_height;
				(role == "flowers" ? profiles.at(name).flowers : profiles.at(name).plants).push_back(plant);
			} else throw std::runtime_error("Unknown registry fixture row: " + kind);
			if (!row) throw std::runtime_error("Malformed registry fixture row: " + line);
		}
		if (biomes.empty() || ids.empty()) throw std::runtime_error("Empty registry fixture");
	}
};
inline Registry load(int argc, char **argv) {
	if (argc < 2) throw std::runtime_error("Registry fixture required; use python3 tests/run_tests.py");
	return Registry(argv[1]);
}
} // namespace test_registry
#endif
