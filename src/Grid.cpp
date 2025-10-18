#include "../include/Grid.h"
#include "../include/Slope.h"
#include <cmath>
#include <unordered_set>

namespace grid {


float grid_resolution = 1.0f; 


std::pair<int,int> quantize(float x, float y) {
    int gx = static_cast<int>(std::floor(x / grid::grid_resolution));
    int gy = static_cast<int>(std::floor(y / grid::grid_resolution));
    return {gx, gy};
}


size_t hashCell(int gx, int gy) {
    uint64_t key = (uint64_t(uint32_t(gx)) << 32) ^ uint32_t(gy);
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return size_t(key);
}

size_t fetch_cell(float x, float y) {
    auto [gx, gy] = quantize(x, y);
    return hashCell(gx, gy);
}

std::vector<geometry::Cell*> getNeighbors(float x, float y, ska::flat_hash_map<size_t, geometry::Cell>& cellMap) {
    auto [gx, gy] = quantize(x, y);

    std::unordered_set<size_t> seen;
    std::vector<geometry::Cell*> neighbors;
    neighbors.reserve(9);

    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            size_t cell = hashCell(gx+dx, gy+dy);
            if (seen.insert(cell).second) { // insert returns true if new
                neighbors.push_back(&cellMap.find(cell)->second);
            }
        }
    }


    return neighbors;
}

} 
