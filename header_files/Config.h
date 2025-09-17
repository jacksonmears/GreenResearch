#pragma once
#include <SDL3/SDL.h>

class Config {
public:
    float SCREEN_WIDTH = 2400, SCREEN_HEIGHT = 1300;
    float gravity = 9.81;
    float deltaTime = 1.0 / 5000;
    float pixelsPerMeter = 100;
    const double PI = 3.141592653589793;
    const float R = SCREEN_HEIGHT / 100;
    const float mass = 0.005f; // originally 0.005f
    const int num_drops = 5000;
    const float collision_damping = 0.10;
    const float smoothing_radius = R*1.2;
    const float cell_size = smoothing_radius*1.2;
    const int normal_neighbors = 10;
    const int max_number_particles_per_cell = 50;
    float W_self = 4.0f / (PI * smoothing_radius * smoothing_radius);
    // float target_density = mass * W_self * normal_neighbors;
    // float pressureMultiplier = 50.0f * mass / (smoothing_radius * smoothing_radius);



    const float target_density = 3;
    const float pressureMultiplier = 50;


    // const float W_self = 6.0f / (PI * R * R);
    // float target_density = mass * W_self * 5.0f; // factor 2 ~ 2 neighbors (used to just be 3)


    // SDL_FColor blue = {0, 0, 255, 255};
    SDL_FColor blue = {0.0f, 0.0f, 1.0f, 0.3f}; // blue with 10% opacity


    static Config& get() {
        static Config instance;
        return instance;
    }

private:
    Config() = default; // private constructor
};
