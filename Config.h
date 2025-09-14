#pragma once
#include <SDL3/SDL.h>

class Config {
public:
    float SCREEN_WIDTH = 700, SCREEN_HEIGHT = 500;
    float gravity = 9.81;
    float deltaTime = 1.0 / 5000;
    float pixelsPerMeter = 100;
    const double PI = 3.141592653589793;
    const float R = 20;
    const float mass = 0.00005;
    const int num_drops = 400;
    const float collision_damping = 0.75;
    const float smoothing_radius = R*1.5;
    const float target_density = 3;
    const float pressureMultiplier = 500;

    // SDL_FColor blue = {0, 0, 255, 255};
    SDL_FColor blue = {0.0f, 0.0f, 1.0f, 0.3f}; // blue with 10% opacity


    static Config& get() {
        static Config instance;
        return instance;
    }

private:
    Config() = default; // private constructor
};
