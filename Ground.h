#pragma once
#include <SDL3/SDL.h>
#include "Config.h"

class Ground {
public:
    double restitution, fc;   // existing: bounciness and friction
    double mu_k;              // new: kinetic (sliding) friction coefficient
    double Crr;               // new: rolling resistance coefficient
    double y;       
    const double height = 5.0;
    double angle = 0;
    double roughness; // don't know what the skibidi this is but physics youtuber fella likes it fr

    Ground(double restitution_, double y_, double fc_, double mu_k_ = 0.0, double Crr_ = 0.0) : restitution(restitution_), y(y_), fc(fc_), mu_k(mu_k_), Crr(Crr_) {}
    virtual ~Ground() = default;

    virtual void render(SDL_Renderer* renderer) = 0;
};

// Green ground
class Green : public Ground {
public:
    Green() : Ground(0.2, Config::get().SCREEN_HEIGHT - 5.0, 0.002, 0.002, 0.0001) {} // 5 px from bottom

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
    Fairway() : Ground(0.5, Config::get().SCREEN_HEIGHT - 5.0, 0.05, 0.05, 0.0002) {} // same height

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
    Cartpath() : Ground(0.9, Config::get().SCREEN_HEIGHT - 5.0, 0.001, 0.001, 0.00005) {} // same height dont forget to change back to 0.9

    void render(SDL_Renderer* renderer) override {
        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255); // gray
        SDL_FRect rect = {0.0f, static_cast<float>(y), 
                          static_cast<float>(Config::get().SCREEN_WIDTH), 
                          static_cast<float>(height)};
        SDL_RenderFillRect(renderer, &rect);
    }
};
