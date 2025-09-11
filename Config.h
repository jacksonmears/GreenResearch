#pragma once
#include <SDL3/SDL.h>

class Config {
public:
    double SCREEN_WIDTH = 700, SCREEN_HEIGHT = 500;
    double gravity = 9.81;
    double deltaTime = 1.0 / 500;
    double pixelsPerMeter = 100;
    double massScale = 1.0;
    const double PI = 3.141592653589793;
    SDL_FColor blue = {0, 0, 255, 255};

    static Config& get() {
        static Config instance;
        return instance;
    }

private:
    Config() = default; // private constructor
};
