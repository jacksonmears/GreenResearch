#pragma once
#include "Drop.h"
#include "Config.h"
#include <vector>
#include <cstdlib>
#include <cmath> 
#include <iostream>

class Body {
public:
    std::vector<Drop> children;


    // void fill_children(int n) {
    //     float radius = Config::get().R;
    //     float screen_width = static_cast<float>(Config::get().SCREEN_WIDTH);
    //     float screen_height = static_cast<float>(Config::get().SCREEN_HEIGHT);

        // float x_min = 0.25f * screen_width + radius;
        // float x_max = 0.75f * screen_width - radius;
        // float y_min = 0.25f * screen_height + radius;
        // float y_max = 0.75f * screen_height - radius;

    //     for (int i = 0; i < n; ++i) {
    //         float x = x_min + static_cast<float>(rand()) / (RAND_MAX + 1.0f) * (x_max - x_min);
    //         float y = y_min + static_cast<float>(rand()) / (RAND_MAX + 1.0f) * (y_max - y_min);

    //         children.emplace_back(x, y, Config::get().blue);
    //     }
    // }

    void fill_children(int n) {
        float radius = Config::get().R;
        float screen_width = Config::get().SCREEN_WIDTH;
        float screen_height = Config::get().SCREEN_HEIGHT;

        // float x_min = radius;
        // float x_max = screen_width - radius;
        // float y_min = radius;
        // float y_max = screen_height - radius;

        float x_min = 0.25f * screen_width + radius;
        float x_max = 0.75f * screen_width - radius;
        float y_min = 0.25f * screen_height + radius;
        float y_max = 0.75f * screen_height - radius;


        for (int i = 0; i < n; ++i) {
            float x = x_min + static_cast<float>(rand()) / (RAND_MAX + 1.0f) * (x_max - x_min);
            float y = y_min + static_cast<float>(rand()) / (RAND_MAX + 1.0f) * (y_max - y_min);

            children.emplace_back(x, y, Config::get().blue);
        }
    }

};
