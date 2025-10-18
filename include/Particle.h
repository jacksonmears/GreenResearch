#pragma once

#include <iostream>
#include <vector>
#include "../external/flat_hash_map.hpp"

namespace geometry { struct Cell; }

namespace particle {

struct Particle { 
    float x,y,z; 
    float r,g,b; 
    size_t grid_index; 
};


inline void sortParticles(std::vector<Particle>& particles) {
    std::sort(particles.begin(), particles.end(), [](const particle::Particle& a, const particle::Particle& b) { return a.grid_index < b.grid_index; });
}


void fillCellParticles(std::vector<Particle>& particles, ska::flat_hash_map<size_t, geometry::Cell>& cellMap);

}