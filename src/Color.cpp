#include "../include/Slope.h"
#include "../include/Particle.h"
#include "../include/Grid.h"
#include "../include/Color.h"


namespace color {


std::tuple<float, float, float> hashToColor(uint64_t h) {
    // scramble bits
    h ^= (h >> 23);
    h *= 0x2127599bf4325c37ULL;
    h ^= (h >> 47);

    // extract bytes
    uint8_t r = (h >>  0) & 0xFF;
    uint8_t g = (h >>  8) & 0xFF;
    uint8_t b = (h >> 16) & 0xFF;

    // normalize to [0,1] and avoid 0
    auto norm = [](uint8_t c) -> float {
        return c / 255.0f * 0.75f + 0.25f;  // scale to [0.1, 1.0] just so no grid cell is EVER completely black
    };

    return { norm(r), norm(g), norm(b) };
}


void applyColorGradient(std::vector<particle::Particle>& particles,ska::flat_hash_map<size_t, geometry::Cell>& cellMap) {
    for (auto [key, value] : cellMap) {
        geometry::Cell* c = &cellMap[key];
        std::vector<geometry::Cell*> neighbors = grid::getNeighbors(particles[cellMap[key].start_index].x, particles[cellMap[key].start_index].z, cellMap);

        for (int i = cellMap[key].start_index; i < (*c).end_index; ++i) {
            particle::Particle* p = &particles[i];
            int slopePercentWeightScalar = std::clamp(geometry::slopeNeighborsScalar(cellMap, *p, neighbors), 0, 10);
            color::ColorF color = color::slopeGradient[slopePercentWeightScalar];

            p->r = color.r;
            p->g = color.g;
            p->b = color.b;

        }
    }
}

}