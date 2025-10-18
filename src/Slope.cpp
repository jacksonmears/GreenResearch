#include "../include/Slope.h"
#include "../include/Config.h"
#include "../include/Particle.h"
#include <cmath>
#include <algorithm>

namespace geometry {


Slope fitPlane(std::vector<particle::Particle>& particles,
                     const size_t start_index,
                     const size_t end_index,
                     float scale)
{
    size_t n = end_index - start_index;
    if (n < 3)
        return {0,0,0,0,0,0,0,0,0,0,0,0,0,false};

    double Sx=0, Sy=0, Sz=0;
    for (size_t i = start_index; i < end_index; ++i) {
        Sx += particles[i].x;
        Sy += particles[i].y;
        Sz += particles[i].z;
    }

    double xBar = Sx / n, yBar = Sy / n, zBar = Sz / n;

    double Sxx=0, Szz=0, Sxz=0, Sxy=0, Szy=0;
    for (size_t i = start_index; i < end_index; ++i) {
        double X = particles[i].x - xBar;
        double Z = particles[i].z - zBar;
        double Y = particles[i].y - yBar;
        Sxx += X*X;
        Szz += Z*Z;
        Sxz += X*Z;
        Sxy += X*Y;
        Szy += Z*Y;
    }

    double det = Sxx * Szz - Sxz * Sxz;
    if (std::abs(det) < 1e-12)
        return {0,0,0,0,0,0,0,0,0,0,0,0,0,false};

    double a = (Sxy*Szz - Szy*Sxz) / det; // dy/dx
    double b = (Szy*Sxx - Sxy*Sxz) / det; // dy/dz

    // normal vector
    double nx = -a;
    double ny = 1.0;
    double nz = -b;
    double norm = std::sqrt(nx*nx + ny*ny + nz*nz);
    nx /= norm; ny /= norm; nz /= norm;

    bool isPlaneValid = n > 3000;

    float len = std::sqrt(a*a + b*b);
    float slopePercent = len * 100.0f;

    float dx = -a / len;
    float dz = -b / len;

    float endX = xBar + dx * scale;
    float endY = yBar;
    float endZ = zBar + dz * scale;

    float color = std::min(slopePercent / 100.0f, 1.0f);

    return {
        static_cast<float>(a), static_cast<float>(b),
        len, slopePercent, dx, dz,
        endX, endY, endZ, color,
        static_cast<float>(xBar), static_cast<float>(yBar), static_cast<float>(zBar),
        isPlaneValid
    };
}


int calculateScalarLinear(int slopePercent, float distance) {
    const float maxDist = 2.75f;
    float weight = std::clamp(1.0f - distance / maxDist, 0.0f, 1.0f);
    return static_cast<int>(slopePercent * weight);
}


int calculateScalarPoly(int slopePercent, float distance) {
    const float maxDist = 2.0f;
    float t = std::clamp(distance / maxDist, 0.0f, 1.0f);

    // Example: cubic polynomial falloff (smooth start, faster decay)
    // weight = (1 - t)^3
    float weight = (1.0f - t) * (1.0f - t) * (1.0f - t);

    return static_cast<int>(slopePercent * weight);
}

int slopeNeighborsScalar(const ska::flat_hash_map<size_t, Cell>& cellMap, particle::Particle& p, std::vector<Cell*>& neighbors) {

    int weightedScalar = 0, planeCount = 0;
    for (Cell* c : neighbors) {
        Slope& plane = c->plane;
        if (!plane.valid) continue;
        ++planeCount;
        int slopePercent = std::sqrt(plane.a*plane.a + plane.b*plane.b) * 100.0f;
        float dx = plane.xBar - p.x;
        float dy = plane.yBar - p.y;
        float dz = plane.zBar - p.z;
        float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
        weightedScalar += calculateScalarLinear(slopePercent, distance);
    }

    return (planeCount) ? weightedScalar/planeCount : 0;
}


void fillCellPlane(std::vector<particle::Particle>& particles,ska::flat_hash_map<size_t, Cell>& cellMap) {
    for (auto [key, _] : cellMap) {
        Cell& c = cellMap[key];
        c.plane = fitPlane(particles, c.start_index, c.end_index, config::SCALE);
    }
}

void fillCellMap(std::vector<particle::Particle>& particles,ska::flat_hash_map<size_t, Cell>& cellMap) {
    particle::fillCellParticles(particles, cellMap);
    fillCellPlane(particles, cellMap);
}

} 
