#ifndef CAVE_TUNNELS_H
#define CAVE_TUNNELS_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace voxel {

struct CavePoint { double x, y, z; };

struct CaveSegment { CavePoint a, b; double radius; };

// Stateless regional worms: world coordinates and seed completely determine
// geometry. No noise thresholds, chunk-local RNG, or shared worker state.

class CaveTunnels {
    uint64_t state;
    explicit CaveTunnels(uint64_t seed) : state(seed) {}
    double random() {
        state += UINT64_C(0x9e3779b97f4a7c15);
        uint64_t value = state;
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        value ^= value >> 31;
        return (value >> 11) * (1.0 / 9007199254740992.0);
    }
    static int region(double value, int size) { return static_cast<int>(std::floor(value / size)); }

public:
    static constexpr double MAX_RADIUS = 8.0;
    static constexpr double VERTICAL_SCALE = 0.85;
    static double distance_squared(const CaveSegment &s, CavePoint p) {
        const CavePoint d{s.b.x-s.a.x, (s.b.y-s.a.y)/VERTICAL_SCALE, s.b.z-s.a.z};
        const CavePoint v{p.x-s.a.x, (p.y-s.a.y)/VERTICAL_SCALE, p.z-s.a.z};
        const double length = d.x*d.x + d.y*d.y + d.z*d.z;
        const double t = length > 0 ? std::clamp((v.x*d.x+v.y*d.y+v.z*d.z)/length, 0.0, 1.0) : 0;
        const double x=v.x-t*d.x, y=v.y-t*d.y, z=v.z-t*d.z;
        return x*x+y*y+z*z;
    }

    static std::vector<CaveSegment> for_bounds(int64_t seed, CavePoint lo, CavePoint hi) {
        std::vector<CaveSegment> result;
        // Centers remain inside their region; only the radius can cross it.
        for (int rz=region(lo.z-MAX_RADIUS,96); rz<=region(hi.z+MAX_RADIUS,96); ++rz)
            for (int ry=region(lo.y-MAX_RADIUS,64); ry<=region(hi.y+MAX_RADIUS,64); ++ry)
                for (int rx=region(lo.x-MAX_RADIUS,96); rx<=region(hi.x+MAX_RADIUS,96); ++rx) {
                    CaveTunnels rng{static_cast<uint64_t>(seed) ^
                        (static_cast<uint64_t>(rx)*UINT64_C(0x632be59bd9b4e019)) ^
                        (static_cast<uint64_t>(ry)*UINT64_C(0x9e3779b185ebca87)) ^
                        (static_cast<uint64_t>(rz)*UINT64_C(0xc2b2ae3d27d4eb4f))};
                    if (rng.random() < 0.18) continue;
                    const CavePoint origin{rx*96.0,ry*64.0,rz*96.0};
                    CavePoint p{origin.x+16+rng.random()*64,origin.y+12+rng.random()*40,origin.z+16+rng.random()*64};
                    double angle=rng.random()*6.283185307179586;
                    double slope=(rng.random()-0.5)*0.6;
                    const double radius=3.6+rng.random()*1.4;
                    const bool branch=rng.random()<0.4;
                    CavePoint fork{};
                    double fork_angle=0;
                    auto append = [&](CavePoint a, CavePoint b, double r) {
                        if (std::max(a.x,b.x)+r < lo.x || std::min(a.x,b.x)-r > hi.x ||
                            std::max(a.y,b.y)+r < lo.y || std::min(a.y,b.y)-r > hi.y ||
                            std::max(a.z,b.z)+r < lo.z || std::min(a.z,b.z)-r > hi.z) return;
                        result.push_back({a,b,r});
                    };
                    auto walk = [&](int steps, double base_radius, bool main) {
                        for (int i=0;i<steps;++i) {
                            angle+=(rng.random()-0.5)*0.55;
                            slope=std::clamp(slope*0.8+(rng.random()-0.5)*0.16,-0.35,0.35);
                            CavePoint next{p.x+std::cos(angle)*6,p.y+slope*6,p.z+std::sin(angle)*6};
                            // Reflect smoothly at regional bounds. Paths never leave
                            // these bounds, so neighbouring chunks need a small halo.
                            if (next.x<origin.x+8 || next.x>origin.x+88) { angle=3.141592653589793-angle; next.x=p.x+std::cos(angle)*6; }
                            if (next.z<origin.z+8 || next.z>origin.z+88) { angle=-angle; next.z=p.z+std::sin(angle)*6; }
                            if (next.y<origin.y+8 || next.y>origin.y+56) { slope=-slope; next.y=p.y+slope*6; }
                            append(p,next,base_radius);
                            p=next;
                            if (main && i==10) { fork=p; fork_angle=angle; }
                            if (main && i==16 && rng.random()<0.55)
                                append(p,p,6.5+rng.random()*1.5);
                        }
                    };
                    walk(24,radius,true);
                    if (branch) { p=fork; angle=fork_angle+1.2; walk(10,3.4,false); }
                }
        return result;
    }
};
} // namespace voxel
#endif
