#pragma once

#include <vector>
#include <cstddef>  
#include "../external/flat_hash_map.hpp"
#include "Particle.h"


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


/**
 * @brief Fits a local plane to a set of particles in the given range.
 * 
 * @param particles   reference to the full particle vector
 * @param start_index starting index of the cell's points
 * @param end_index   ending index (exclusive)
 * @param scale       arrow length scaling
 * @return SlopeResult  structure containing slope info
 */

Slope fitPlane(std::vector<particle::Particle>& particles, size_t start_index, size_t end_index, float scale);

int calculateScalarLinear(int slopePercent, float distance);

int calculateScalarPoly(int slopePercent, float distance);

int slopeNeighborsScalar(const ska::flat_hash_map<size_t, Cell>& cellMap, particle::Particle& p, std::vector<Cell*>& neighbors);

} // namespace geometry
