#include "../external/flat_hash_map.hpp"
#include "../include/Slope.h"
#include "../include/Particle.h"

namespace particle {


void fillCellParticles(std::vector<Particle>& particles, ska::flat_hash_map<size_t, geometry::Cell>& cellMap) {
    cellMap[particles[0].grid_index].start_index = 0;
    Particle* prev = &particles[0];

    for (int p = 1; p < particles.size(); ++p) {
        if (particles[p].grid_index != prev->grid_index) {
            cellMap[prev->grid_index].end_index = p-1;
            cellMap[particles[p].grid_index].start_index = p;
        }
        prev = &particles[p];
    }
    cellMap[particles[particles.size()-1].grid_index].end_index = particles.size()-1;
}

}