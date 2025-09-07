#pragma once
#include <SDL3/SDL.h>

class Ball {
public:
    int x, y;
    int radius;
    SDL_Color color;

    Ball(int x_, int y_, int r_, SDL_Color c_) : x(x_), y(y_), radius(r_), color(c_) {}

    void render(SDL_Renderer* renderer);
};
