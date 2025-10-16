#pragma once

#include <vector>
#include <cmath>
#include <iostream>

struct Particle { 
    float x,y,z; 
    float r,g,b; 
    size_t grid_index; 
};

struct SlopeResult {
    float a, b, len, slopePercent, dx, dz, endX, endY, endZ, color, xBar, yBar, zBar;
    bool valid;
};

struct Cell {
    size_t start_index, end_index;
    SlopeResult plane;
};


// SlopeResult fitPlane(const std::vector<Particle*>& pts, float scale) {
//     size_t n = pts.size();
//     if (n < 3) return {0,0,0,0,0,0,0,0,0,0,0,0,0,false};

//     double Sx=0, Sy=0, Sz=0;
//     for (auto p : pts) { Sx += p->x; Sy += p->y; Sz += p->z; }
//     double xBar = Sx/n, yBar = Sy/n, zBar = Sz/n;

//     double Sxx=0, Szz=0, Sxz=0, Sxy=0, Szy=0;
//     for (auto p : pts) {
//         double X = p->x - xBar;
//         double Z = p->z - zBar;
//         double Y = p->y - yBar; // vertical
//         Sxx += X*X;
//         Szz += Z*Z;
//         Sxz += X*Z;
//         Sxy += X*Y;
//         Szy += Z*Y;
//     }

//     double det = Sxx*Szz - Sxz*Sxz;
//     if (std::abs(det) < 1e-12) return {0,0,0,0,0,0,0,0,0,0,0,0,0,false};

//     double a = (Sxy*Szz - Szy*Sxz)/det; // dy/dx
//     double b = (Szy*Sxx - Sxy*Sxz)/det; // dy/dz
//     double c = yBar - a*xBar - b*zBar;

//     // Correct perpendicular normal (pointing outward)
//     double nx = -a;
//     double ny = 1.0;
//     double nz = -b;
//     double norm = std::sqrt(nx*nx + ny*ny + nz*nz);
//     nx /= norm; ny /= norm; nz /= norm;

//     bool isPlaneValid = pts.size() > 3'000;




//     float len = std::sqrt(a*a + b*b);
//     float slopePercent = len * 100.0f;


//     float dx = -a/len;
//     float dz = -b/len;

//     float endX = xBar + dx * scale;
//     float endY = yBar; // keep it parallel to the surface
//     float endZ = zBar + dz * scale;

//     float color = std::min(slopePercent/100.0f, 1.0f);

//     return { static_cast<float>(a), static_cast<float>(b), len, slopePercent, dx, dz, endX, endY, endZ, color,
//              static_cast<float>(xBar), static_cast<float>(yBar), static_cast<float>(zBar),
//              isPlaneValid };
// }


SlopeResult fitPlane(std::vector<Particle>& particles, const size_t start_index, const size_t end_index, float scale) {
    size_t n = end_index-start_index;
    if (n < 3) return {0,0,0,0,0,0,0,0,0,0,0,0,0,false};

    double Sx=0, Sy=0, Sz=0;
    for (int p = start_index; p < end_index; ++p) {
        Sx += particles[p].x; Sy += particles[p].y; Sz += particles[p].z;
    }
    double xBar = Sx/n, yBar = Sy/n, zBar = Sz/n;

    double Sxx=0, Szz=0, Sxz=0, Sxy=0, Szy=0;
    for (int p = start_index; p < end_index; ++p) {
        double X = particles[p].x - xBar;
        double Z = particles[p].z - zBar;
        double Y = particles[p].y - yBar; // vertical
        Sxx += X*X;
        Szz += Z*Z;
        Sxz += X*Z;
        Sxy += X*Y;
        Szy += Z*Y;
    }

    double det = Sxx*Szz - Sxz*Sxz;
    if (std::abs(det) < 1e-12) return {0,0,0,0,0,0,0,0,0,0,0,0,0,false};

    double a = (Sxy*Szz - Szy*Sxz)/det; // dy/dx
    double b = (Szy*Sxx - Sxy*Sxz)/det; // dy/dz
    double c = yBar - a*xBar - b*zBar;

    // Correct perpendicular normal (pointing outward)
    double nx = -a;
    double ny = 1.0;
    double nz = -b;
    double norm = std::sqrt(nx*nx + ny*ny + nz*nz);
    nx /= norm; ny /= norm; nz /= norm;

    bool isPlaneValid = n > 3'000;

    float len = std::sqrt(a*a + b*b);
    float slopePercent = len * 100.0f;


    float dx = -a/len;
    float dz = -b/len;

    float endX = xBar + dx * scale;
    float endY = yBar; // keep it parallel to the surface
    float endZ = zBar + dz * scale;

    float color = std::min(slopePercent/100.0f, 1.0f);

    return { static_cast<float>(a), static_cast<float>(b), len, slopePercent, dx, dz, endX, endY, endZ, color,
             static_cast<float>(xBar), static_cast<float>(yBar), static_cast<float>(zBar),
             isPlaneValid };
}