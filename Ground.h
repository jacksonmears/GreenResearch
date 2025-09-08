#pragma once
#include <SDL3/SDL.h>
#include "Config.h"

class Ground {
public:
    double cor;     // coefficient of restitution (bounciness)
    double y;       // vertical position (top of ground)
    const double height = 5.0; // consistent height

    Ground(double cor_, double y_) : cor(cor_), y(y_) {}
    virtual ~Ground() = default;

    virtual void render(SDL_Renderer* renderer) = 0;
};

// Green ground
class Green : public Ground {
public:
    Green() : Ground(0.5, Config::get().SCREEN_HEIGHT - 5.0) {} // 5 px from bottom

    void render(SDL_Renderer* renderer) override {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // green
        SDL_FRect rect = {0.0f, static_cast<float>(y), 
                          static_cast<float>(Config::get().SCREEN_WIDTH), 
                          static_cast<float>(height)};
        SDL_RenderFillRect(renderer, &rect);
    }
};

// Fairway ground
class Fairway : public Ground {
public:
    Fairway() : Ground(0.7, Config::get().SCREEN_HEIGHT - 5.0) {} // same height

    void render(SDL_Renderer* renderer) override {
        SDL_SetRenderDrawColor(renderer, 160, 82, 45, 255); // brownish
        SDL_FRect rect = {0.0f, static_cast<float>(y), 
                          static_cast<float>(Config::get().SCREEN_WIDTH), 
                          static_cast<float>(height)};
        SDL_RenderFillRect(renderer, &rect);
    }
};

// Cartpath ground
class Cartpath : public Ground {
public:
    Cartpath() : Ground(1, Config::get().SCREEN_HEIGHT - 5.0) {} // same height

    void render(SDL_Renderer* renderer) override {
        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255); // gray
        SDL_FRect rect = {0.0f, static_cast<float>(y), 
                          static_cast<float>(Config::get().SCREEN_WIDTH), 
                          static_cast<float>(height)};
        SDL_RenderFillRect(renderer, &rect);
    }
};
