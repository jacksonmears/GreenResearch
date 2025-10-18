#pragma once

#include <array>
#include <tuple>
#include <cstdint>
#include <vector>
#include "../external/flat_hash_map.hpp" 

namespace particle { struct Particle; }
namespace geometry { struct Cell;}

namespace color {

struct ColorF {
    float r, g, b;
};

constexpr std::array<ColorF, 11> slopeGradient = {{
    {1.0f, 1.0f, 1.0f},   // greenish
    {0.0f, 1.0f, 0.502f},   // greenish
    {0.0f, 1.0f, 0.0f},     // green
    {0.251f, 1.0f, 0.0f},
    {0.502f, 1.0f, 0.0f},
    {0.749f, 1.0f, 0.0f},
    {1.0f, 1.0f, 0.0f},     // yellow
    {1.0f, 0.749f, 0.0f},
    {1.0f, 0.502f, 0.0f},
    {1.0f, 0.251f, 0.0f},
    {1.0f, 0.0f, 0.0f}      // red
}};





std::tuple<float, float, float> hashToColor(uint64_t h);

void applyColorGradient(std::vector<particle::Particle>& particles,ska::flat_hash_map<size_t, geometry::Cell>& cellMap);

}
