#pragma once
#include <SDL3/SDL.h>

class Config {
public:
    float SCREEN_WIDTH = 700, SCREEN_HEIGHT = 500;
    float gravity = 9.81;
    float deltaTime = 1.0 / 500;
    float pixelsPerMeter = 100;
    float massScale = 1.0;
    const double PI = 3.141592653589793;
    const float radius = 0.5, cell_size = radius*4;
    const float grid_width = ceil(SCREEN_WIDTH / cell_size);
    const float grid_height = ceil(SCREEN_HEIGHT / cell_size);
    const float num_cells = grid_height * grid_width;
    const float mass = 1;
    const float restitution = 0.5;
    const int num_drops = 25'000;

    SDL_FColor blue = {0, 0, 255, 255};

    static Config& get() {
        static Config instance;
        return instance;
    }

private:
    Config() = default; // private constructor
};
