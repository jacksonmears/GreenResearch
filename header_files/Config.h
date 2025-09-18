#pragma once
#include <SDL3/SDL.h>

class Config {
public:
    float SCREEN_WIDTH = 800, SCREEN_HEIGHT = 650;
    float gravity = 0; // used to be 9.81
    float deltaTime = 1.0 / 500;
    float pixelsPerMeter = 100;
    const double PI = 3.141592653589793;
    const float R = SCREEN_HEIGHT / 100;
    const float mass = 0.005f; // originally 0.005f
    const int num_drops = 5000;
    const float collision_damping = 0.95;
    const float smoothing_radius = R*1.2;
    const float cell_size = smoothing_radius*2;
    const int normal_neighbors = 10;
    const int max_number_particles_per_cell = num_drops/200;
    float W_self = 4.0f / (PI * smoothing_radius * smoothing_radius);
    float air_damping = 1;
    // float target_density = mass * W_self * normal_neighbors;
    // float pressureMultiplier = 50.0f * mass / (smoothing_radius * smoothing_radius);



    const float target_density = 50; //orig 1.5
    const float pressureMultiplier = 0.005; //0.5, 5, 50 it don't matter this thang awesome


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
