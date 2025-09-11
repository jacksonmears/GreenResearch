#pragma once
#include "Drop.h"
#include "Config.h"
#include <vector>
#include <cstdlib>

class Body {
public:
    std::vector<Drop> children;

    void fill_children(int n) {
        for (int i = 0; i < n; ++i) {
            float x = static_cast<float>(rand() % static_cast<int>(Config::get().SCREEN_WIDTH));
            float y = static_cast<float>(rand() % static_cast<int>(Config::get().SCREEN_HEIGHT));
            children.emplace_back(x, y, 0.5f, 1.0f, 0.5f, Config::get().blue);
        }
    }
};
