#include "../src/biome_rarity.h"
#include "support/registry_fixture.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>

int main(int argc, char **argv) {
    const auto registry = test_registry::load(argc, argv);
    const int64_t seed = argc > 2 ? std::stoll(argv[2]) : 42;

    for (const auto &biome : registry.biomes) {
        std::set<int> levels{0, 1, biome.rarity, std::min(10000, std::max(1, biome.rarity) * 2), std::min(10000, std::max(1, biome.rarity) * 4)};
        std::map<int, int> counts;
        int total = 0, changed = 0;
        for (int z = -8192; z < 8192; z += 64)
            for (int x = -8192; x < 8192; x += 64) {
                ++total;
                assert(voxel::biome_presence(0, seed, biome.id, x, z) == 0);
                float previous = 1;
                for (int rarity : levels) {
                    const float value = voxel::biome_presence(rarity, seed, biome.id, x, z);
                    assert(value >= 0 && value <= 1);
                    assert(value == voxel::biome_presence(rarity, seed, biome.id, x, z));
                    if (rarity > 0) {
                        assert(value <= previous);
                        previous = value;
                    }
                    counts[rarity] += value >= 0.5f;
                }
                const int probe = std::min(10000, std::max(2, biome.rarity));
                const float value = voxel::biome_presence(probe, seed, biome.id, x, z);
                changed += (value >= 0.5f) != (voxel::biome_presence(probe, seed + 1, biome.id, x, z) >= 0.5f);
                assert(std::abs(value - voxel::biome_presence(probe, seed, biome.id, x + 1, z)) < 0.10f);
            }
        assert(counts[0] == 0 && counts[1] == total);

        int previous = total;
        for (int rarity : levels) if (rarity > 1) {
            assert(counts[rarity] <= previous);
            if (rarity <= 4) assert(counts[rarity] > 0 && counts[rarity] < total);
            previous = counts[rarity];
        }
        if (biome.rarity <= 4) assert(changed > 0);

        std::cout << biome.name << " (ID " << biome.id << ", configured rarity " << biome.rarity << "):";
        for (const auto &[rarity, count] : counts) std::cout << " " << rarity << "=" << count << "/" << total;
        std::cout << '\n';
    }
}
