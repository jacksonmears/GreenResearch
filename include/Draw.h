#pragma once

#include "../external/flat_hash_map.hpp"
#include <vector>

namespace particle { struct Particle; }
namespace geometry { struct Cell; }

namespace draw {

void drawParticles(std::vector<particle::Particle>& particles,ska::flat_hash_map<size_t, geometry::Cell>& cellMap);

void drawArrows(std::vector<particle::Particle>& particles,ska::flat_hash_map<size_t, geometry::Cell>& cellMap);

}