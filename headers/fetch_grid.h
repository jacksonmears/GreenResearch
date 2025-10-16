#pragma once

#include <unordered_set>
#include <utility>
#include <cmath>
#include "Config.h"
#include "flat_hash_map.hpp"  // from https://github.com/skarupke/flat_hash_map
#include "calculate_slopes.h"

float grid_resolution = Config::get().grid_resolution;

inline std::pair<int,int> quantize(float x, float y) {
    int gx = static_cast<int>(std::floor(x / grid_resolution));
    int gy = static_cast<int>(std::floor(y / grid_resolution));
    return {gx, gy};
}


inline size_t hashCell(int gx, int gy) {
    uint64_t key = (uint64_t(uint32_t(gx)) << 32) ^ uint32_t(gy);
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return size_t(key);
}

inline size_t fetch_cell(float x, float y) {
    auto [gx, gy] = quantize(x, y);
    return hashCell(gx, gy);
}



inline std::vector<Cell*> getNeighbors(float x, float y, ska::flat_hash_map<size_t, Cell>& tt) {
    auto [gx, gy] = quantize(x, y);

    std::unordered_set<size_t> seen;
    std::vector<Cell*> neighbors;
    neighbors.reserve(9);

    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            size_t cell = hashCell(gx+dx, gy+dy);
            if (seen.insert(cell).second) { // insert returns true if new
                neighbors.push_back(&tt.find(cell)->second);
            }
        }
    }


    return neighbors;
}

