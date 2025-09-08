#pragma once
#include <SDL3/SDL.h>



class Shape {
public:
    int width, height;
    double mass, x, y, acceleration, velocity;
    SDL_Color color;


    Shape(int width_, int height_, double mass_, double x_, double y_, double acceleration_, double velocity_, SDL_Color color_) 
        : width(width_), height(height_), mass(mass_), x(x_), y(y_), acceleration(acceleration_), velocity(velocity_), color(color_) {}

    virtual void render(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.g, color.a);

        SDL_FRect rect = {
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(width),
            static_cast<float>(height)
        };

        SDL_RenderFillRect(renderer, &rect);
    }

};



class Ball : public Shape {
public:
    Ball(int radius, double mass_, double x_, double y_, SDL_Color color_) 
        : Shape(radius * 2, radius * 2, mass_, x_, y_, 0, 0, color_), radius(radius) {}


    void render(SDL_Renderer* renderer) override {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        for (int w = 0; w < radius * 2; ++w) {
            for (int h = 0; h < radius * 2; ++h) {
                int dx = radius - w;
                int dy = radius - h;
                if (dx * dx + dy * dy <= radius * radius) {
                    SDL_RenderPoint(renderer, static_cast<int>(x) + dx, static_cast<int>(y) + dy);
                }
            }
        }
    }

private:
    int radius;
};
