#pragma once

#include <vector>
#include <cstddef>  
#include "../external/flat_hash_map.hpp"

namespace particle { struct Particle; }

namespace geometry {


struct Slope {
    float a, b;               // plane coefficients
    float len;                // sqrt(a^2 + b^2)
    float slopePercent;
    float dx, dz;             // direction components
    float endX, endY, endZ;   // end of slope arrow
    float color;
    float xBar, yBar, zBar;   // centroid
    bool  valid;
};

struct Cell {
    size_t start_index, end_index;
    Slope plane;
};

Slope fitPlane(std::vector<particle::Particle>& particles, size_t start_index, size_t end_index, float scale);

int calculateScalarLinear(int slopePercent, float distance);

int calculateScalarPoly(int slopePercent, float distance);

int slopeNeighborsScalar(const ska::flat_hash_map<size_t, Cell>& cellMap, particle::Particle& p, std::vector<Cell*>& neighbors);

void fillCellPlane(std::vector<particle::Particle>& particles,ska::flat_hash_map<size_t, Cell>& cellMap);

void fillCellMap(std::vector<particle::Particle>& particles,ska::flat_hash_map<size_t, Cell>& cellMap);

} 
