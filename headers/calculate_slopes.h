#include <vector>
#include <cmath>
#include <iostream>

struct Particle { 
    float x,y,z; 
    float r,g,b; 
    size_t grid_index; 
};

struct SlopeResult {
    float a; // dz/dx
    float b; // dz/dy
    float c; // plane offset
    float nx, ny, nz; // unit normal
    float cx, cy, cz;
    bool valid;
};




SlopeResult fitPlane(const std::vector<Particle*>& pts) {
    size_t n = pts.size();
    if (n < 3) return {0,0,0,0,0,0,0,0,0,false};

    double Sx=0, Sy=0, Sz=0;
    for (auto p : pts) { Sx += p->x; Sy += p->y; Sz += p->z; }
    double xBar = Sx/n, yBar = Sy/n, zBar = Sz/n;

    double Sxx=0, Szz=0, Sxz=0, Sxy=0, Szy=0;
    for (auto p : pts) {
        double X = p->x - xBar;
        double Z = p->z - zBar;
        double Y = p->y - yBar; // vertical
        Sxx += X*X;
        Szz += Z*Z;
        Sxz += X*Z;
        Sxy += X*Y;
        Szy += Z*Y;
    }

    double det = Sxx*Szz - Sxz*Sxz;
    if (std::abs(det) < 1e-12) return {0,0,0,0,0,0,0,0,0,false};

    double a = (Sxy*Szz - Szy*Sxz)/det; // dy/dx
    double b = (Szy*Sxx - Sxy*Sxz)/det; // dy/dz
    double c = yBar - a*xBar - b*zBar;

    // Correct perpendicular normal (pointing outward)
    double nx = -a;
    double ny = 1.0;
    double nz = -b;
    double norm = std::sqrt(nx*nx + ny*ny + nz*nz);
    nx /= norm; ny /= norm; nz /= norm;

    return { static_cast<float>(a), static_cast<float>(b), static_cast<float>(c),
             static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz),
             static_cast<float>(xBar), static_cast<float>(yBar), static_cast<float>(zBar),
             true };
}


