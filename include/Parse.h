#pragma once

#include <cstddef>
#include <vector>

namespace particle { struct Particle; }

namespace parse {

size_t getSizePCD(const char* file);

float parseFloat4Decimal(char*& data);

void readXYZFast(const char* file, std::vector<particle::Particle>& particles);


}