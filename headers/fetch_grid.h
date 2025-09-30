
#include <utility>
#include <cmath>
#include "Config.h"

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
