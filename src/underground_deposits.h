#ifndef UNDERGROUND_DEPOSITS_H
#define UNDERGROUND_DEPOSITS_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace voxel {

enum class DepositKind { IRON, COAL, DIAMOND, DIRT };

struct UndergroundDeposit {
    DepositKind kind;
    double x, y, z, rx, ry, rz;
    bool contains(double px, double py, double pz) const {
        const double dx=(px-x)/rx, dy=(py-y)/ry, dz=(pz-z)/rz;
        return dx*dx+dy*dy+dz*dz <= 1.0;
    }
};

// Finite seeded deposits form compact veins and earthy pockets. A bounded
// neighbour halo makes each voxel independent of chunk generation order.
inline std::vector<UndergroundDeposit> underground_deposits(int64_t seed,
        double min_x, double min_y, double min_z, double max_x, double max_y, double max_z) {
    constexpr double halo=7.0;
    auto region=[](double p) { return static_cast<int>(std::floor(p/16.0)); };
    std::vector<UndergroundDeposit> result;
    for (int cz=region(min_z-halo);cz<=region(max_z+halo);++cz)
        for (int cy=region(min_y-halo);cy<=region(max_y+halo);++cy)
            for (int cx=region(min_x-halo);cx<=region(max_x+halo);++cx) {
                uint64_t state=static_cast<uint64_t>(seed) ^ UINT64_C(0xD18A4B65F0C23791) ^
                        (static_cast<uint64_t>(cx)*UINT64_C(0x632BE59BD9B4E019)) ^
                        (static_cast<uint64_t>(cy)*UINT64_C(0x9E3779B185EBCA87)) ^
                        (static_cast<uint64_t>(cz)*UINT64_C(0xC2B2AE3D27D4EB4F));
                auto random=[&]() {
                    state+=UINT64_C(0x9E3779B97F4A7C15);
                    uint64_t v=state;
                    v=(v^(v>>30))*UINT64_C(0xBF58476D1CE4E5B9);
                    v=(v^(v>>27))*UINT64_C(0x94D049BB133111EB);
                    v^=v>>31;
                    return (v>>11)*(1.0/9007199254740992.0);
                };
                const double roll=random();
                if (roll>=0.80) continue;
                const auto kind=roll<0.35 ? DepositKind::IRON : roll<0.64 ? DepositKind::COAL :
                        roll<0.68 ? DepositKind::DIAMOND : DepositKind::DIRT;
                const double x=cx*16.0+4+random()*8;
                const double y=cy*16.0+4+random()*8;
                const double z=cz*16.0+4+random()*8;
                if ((kind==DepositKind::IRON && y>64) ||
                    (kind==DepositKind::COAL && (y < -192 || y > 96)) ||
                    (kind==DepositKind::DIAMOND && y>-80) ||
                    (kind==DepositKind::DIRT && (y < -96 || y > 48))) continue;
                const double base=kind==DepositKind::DIAMOND ? 1.7 : kind==DepositKind::DIRT ? 4.6 : 2.8;
                const double variation=kind==DepositKind::DIRT ? 2.3 : 1.5;
                UndergroundDeposit deposit{kind,x,y,z,base+random()*variation,
                        (base+random()*variation)*0.72,base+random()*variation};
                if (deposit.x+deposit.rx < min_x || deposit.x-deposit.rx > max_x ||
                    deposit.y+deposit.ry < min_y || deposit.y-deposit.ry > max_y ||
                    deposit.z+deposit.rz < min_z || deposit.z-deposit.rz > max_z) continue;
                result.push_back(deposit);
            }
    return result;
}
} // namespace voxel
#endif
