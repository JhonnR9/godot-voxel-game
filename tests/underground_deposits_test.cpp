#include "../src/underground_deposits.h"
#include <array>
#include <cassert>
#include <iostream>
#include <random>

int kind_at(const std::vector<voxel::UndergroundDeposit> &deposits, double x,double y,double z) {
    for (const auto &d : deposits) if (d.contains(x,y,z)) return static_cast<int>(d.kind);
    return -1;
}

int main() {
    std::mt19937 random(82);

    for (int64_t seed : {0,42,-19,123456}) {
        const auto large=voxel::underground_deposits(seed,-128,-224,-128,128,32,128);
        const auto again=voxel::underground_deposits(seed,-128,-224,-128,128,32,128);
        assert(large.size()==again.size());

        for (size_t i=0;i<large.size();++i) {
            const auto &d=large[i];
            assert(d.x==again[i].x && d.y==again[i].y && d.z==again[i].z && d.kind==again[i].kind);
            assert(d.rx>0 && d.rx<=7 && d.ry>0 && d.ry<=7 && d.rz>0 && d.rz<=7);
            assert(d.contains(d.x,d.y,d.z));
            assert(!d.contains(d.x+d.rx+0.01,d.y,d.z));
            if (d.kind==voxel::DepositKind::DIAMOND) assert(d.y<=-80);
            if (d.kind==voxel::DepositKind::DIRT) assert(d.y>=-96 && d.y<=48);
        }

        std::array<int,4> counts{};
        for (int i=0;i<24000;++i) {
            const int x=static_cast<int>(random()%256)-128;
            const int y=static_cast<int>(random()%256)-224;
            const int z=static_cast<int>(random()%256)-128;
            const double ox=std::floor(x/16.0)*16, oy=std::floor(y/16.0)*16, oz=std::floor(z/16.0)*16;
            const auto local=voxel::underground_deposits(seed,ox,oy,oz,ox+16,oy+16,oz+16);
            const int selected=kind_at(large,x+0.5,y+0.5,z+0.5);
            assert(selected==kind_at(local,x+0.5,y+0.5,z+0.5));
            if(selected>=0) ++counts[selected];
        }
        assert(counts[0]>150 && counts[1]>100 && counts[2]>3 && counts[3]>25);
        assert(counts[2]<counts[0]/4 && counts[2]<counts[1]/4);
        assert(counts[0]+counts[1]+counts[2]+counts[3]<2400);
        std::cout << "seed " << seed << ": iron " << counts[0] << ", coal " << counts[1]
                  << ", diamond " << counts[2] << ", dirt " << counts[3] << " / 24000 samples\n";
    }
}
