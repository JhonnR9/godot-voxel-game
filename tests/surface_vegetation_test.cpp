#include "../src/surface_vegetation.h"
#include "support/registry_fixture.h"
#include <cassert>
#include <iostream>
#include <map>

int main(int argc, char **argv) {
    const auto registry = test_registry::load(argc, argv);
    const int64_t seed = argc > 2 ? std::stoll(argv[2]) : 42;

    for (const auto &[name, profile] : registry.profiles) {
        auto empty = profile;
        empty.coverage_min = empty.coverage_max = 0;
        std::map<uint16_t, test_registry::Plant> allowed;
        for (const auto &plant : profile.plants) allowed[plant.block] = plant;
        for (const auto &plant : profile.flowers) allowed[plant.block] = plant;

        for (int64_t current_seed : {seed, seed + 1, seed - 1}) {
            int occupied = 0;
            const int span = profile.patch_size * 32;
            for (int z = -span; z < span; ++z) for (int x = -span; x < span; ++x) {
                assert(voxel::sample_surface_plant(empty, current_seed, x, z).block == registry.ids.at("air"));
                const auto plant = voxel::sample_surface_plant(profile, current_seed, x, z);
                const auto again = voxel::sample_surface_plant(profile, current_seed, x, z);
                assert(plant.block == again.block && plant.height == again.height);
                if (plant.block == registry.ids.at("air")) continue;
                ++occupied;
                assert(allowed.count(plant.block));
                const auto &definition = allowed.at(plant.block);
                assert(plant.height >= definition.min_height && plant.height <= definition.max_height);
                assert(voxel::default_block_flags(plant.block) == registry.flags.at(plant.block));
            }
            const int total = 4 * span * span;
            if (profile.coverage_max == 0 || profile.patch_chance == 0 || allowed.empty()) assert(occupied == 0);
            if (profile.coverage_min > 0 && profile.patch_chance >= 100 && !allowed.empty()) assert(occupied > 0);
            // Upper bound follows configured coverage; allow sampling variance, not a frozen count.
            assert(double(occupied) / total <= profile.coverage_max / 1000.0 + 0.02);
            std::cout << name << " seed " << current_seed << ": " << occupied << "/" << total
                      << " plants; patch size " << profile.patch_size << ", chance " << profile.patch_chance << '\n';
        }

        // Guaranteed coverage isolates configured species heights from production density.
        for (const auto &[id, plant] : allowed) {
            auto heights = profile;
            heights.patch_chance = 1000;
            heights.cluster_radius = 0;
            heights.coverage_min = heights.coverage_max = 1000;
            heights.flowers_min = heights.flowers_max = 0;
            heights.plants = {plant};
            std::map<int, int> counts;
            for (int z = -32; z < 32; ++z) for (int x = -32; x < 32; ++x) {
                const auto sample = voxel::sample_surface_plant(heights, seed, x, z);
                assert(sample.block == id);
                assert(sample.height >= plant.min_height && sample.height <= plant.max_height);
                ++counts[sample.height];
            }
            for (int height = plant.min_height; height <= plant.max_height; ++height) assert(counts[height] > 0);
        }
    }
}
