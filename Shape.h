#pragma once
#include <SDL3/SDL.h>
#include <cmath>
#include "Config.h"
#include "Ground.h"

class Shape {
public:
    int width, height;
    double mass, x, y, cor;  // coefficient of restitution
    double accelerationY = Config::get().gravity, accelerationX = 0;
    double velocityY = 0, velocityX = 0;
    double frictionGround = 0.15; // horizontal friction on ground
    double frictionWall   = 0.95; // vertical friction on wall
    SDL_Color color;

    Shape(int width_, int height_, double mass_, double x_, double y_, double cor_, SDL_Color color_) 
        : width(width_), height(height_), mass(mass_), x(x_), y(y_), cor(cor_), color(color_) {}

    virtual void render(SDL_Renderer* renderer) = 0;

    void update(double deltaTime, Ground* ground) {
        // Gravity acceleration (mass included implicitly)
        double accelY = accelerationY;
        double accelX = accelerationX; // can be 0 unless a force is applied

        // Update velocities
        velocityY += accelY * deltaTime;
        velocityX += accelX * deltaTime;

        // Update positions
        y += velocityY * deltaTime;
        x += velocityX * deltaTime;

        // --- Wall collisions ---
        if (x - width <= 0) {
            x = width;
            velocityX = -velocityX * cor;     // bounce horizontally
            velocityY *= frictionWall;       // vertical energy loss on wall hit
        } 
        else if (x + width >= Config::get().SCREEN_WIDTH) {
            x = Config::get().SCREEN_WIDTH - height;
            velocityX = -velocityX * cor;
            velocityY *= frictionWall;
        }

        // Stop tiny horizontal velocity
        if (std::abs(velocityX) < 0.001) velocityX = 0;

        // --- Ground collision ---
        if (y + height >= ground->y) {
            y = ground->y - height;

            double effective_cor = cor * ground->cor;
            velocityY = -velocityY * effective_cor;

            // Apply horizontal friction while on the ground
            velocityX *= frictionGround;

            if (std::abs(velocityY) < 0.001) velocityY = 0;
        }
    }
};

class Ball : public Shape {
public:
    int radius;

    Ball(int radius_, double mass_, double x_, double y_, double cor_, SDL_Color color_) 
        : Shape(radius_ * 2, radius_ * 2, mass_, x_, y_, cor_, color_), radius(radius_) {}

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
};
