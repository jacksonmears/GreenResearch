#pragma once
#include <SDL3/SDL.h>

class Config {
public:
    float SCREEN_WIDTH = 800, SCREEN_HEIGHT = 650;
    float grid_resolution = 1.0f;


    static Config& get() {
        static Config instance;
        return instance;
    }

private:
    Config() = default; 
};
