#include "../src/cave_tunnels.h"
#include <cassert>
#include <iostream>
#include <random>

bool hollow(const std::vector<voxel::CaveSegment> &segments, voxel::CavePoint p) {
    for (const auto &s : segments)
        if (voxel::CaveTunnels::distance_squared(s,p) <= s.radius*s.radius) return true;
    return false;
}
int main() {
    using voxel::CaveTunnels;
    const voxel::CaveSegment tunnel{{0,0,0},{12,0,0},3.2};
    assert(hollow({tunnel},{6,2,0}));
    assert(!hollow({tunnel},{6,3,0}));
    assert(hollow({tunnel},{0,0,0}));
    assert(!hollow({tunnel},{20,0,0}));

    std::mt19937 rng(7319);
    for (int64_t seed : {0,1,42,-19,987654}) {
        const auto whole=CaveTunnels::for_bounds(seed,{-96,-64,-96},{96,64,96});
        const auto again=CaveTunnels::for_bounds(seed,{-96,-64,-96},{96,64,96});
        assert(whole.size()==again.size());

        for (size_t i=0;i<whole.size();++i) {
            const auto &a=whole[i]; const auto &b=again[i];
            assert(a.a.x==b.a.x && a.a.y==b.a.y && a.a.z==b.a.z);
            assert(a.b.x==b.b.x && a.b.y==b.b.y && a.b.z==b.b.z && a.radius==b.radius);
            assert(a.radius>=3 && a.radius<=CaveTunnels::MAX_RADIUS);
            assert(hollow({a},a.a) && hollow({a},a.b));
        }

        int count=0;
        for (int i=0;i<6000;++i) {
            const int x=static_cast<int>(rng()%192)-96;
            const int y=static_cast<int>(rng()%128)-64;
            const int z=static_cast<int>(rng()%192)-96;
            const voxel::CavePoint p{x+0.5,y+0.5,z+0.5};
            const voxel::CavePoint lo{std::floor(x/16.0)*16,std::floor(y/16.0)*16,std::floor(z/16.0)*16};
            const auto local=CaveTunnels::for_bounds(seed,lo,{lo.x+16,lo.y+16,lo.z+16});
            const bool carved=hollow(whole,p);
            // Neighbour region lookup and clipping must give identical terrain
            // independently of chunk boundaries (including negative coordinates).
            assert(carved==hollow(local,p));
            count+=carved;
        }
        const double fraction=count/6000.0;
        assert(fraction>0.003 && fraction<0.15);
        std::cout << "seed " << seed << ": " << fraction*100 << "% hollow samples\n";
    }

    // Check precisely around shared chunk and region boundaries.
    const auto broad=CaveTunnels::for_bounds(42,{-112,-80,-112},{112,80,112});
    for (int x : {-97,-96,-95,-17,-16,-15,-1,0,1,15,16,17,95,96,97})
        for (int y=-64;y<=64;y+=8) for (int z=-96;z<=96;z+=8) {
            const voxel::CavePoint p{x+0.5,y+0.5,z+0.5};
            const auto local=CaveTunnels::for_bounds(42,p,p);
            assert(hollow(broad,p)==hollow(local,p));
        }
}
