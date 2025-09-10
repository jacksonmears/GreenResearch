#pragma once
#include "Config.h"
#include <SDL3/SDL.h>
#include <iostream>


class Drop {
public:
    double R, x, y, mass, restitution, Vn = 0, Vt;
    double g = Config::get().gravity * Config::get().pixelsPerMeter;
    double mu_k = 0.5;
    SDL_Color color;


    Drop(double x_, double y_, double R_, double mass_, double restitution_, SDL_Color color_) : x(x_), y(y_), R(R_), mass(mass_), restitution(restitution_), color(color_) {}

    int fetchSign(double n) { return (n >= 0.0) - (n < 0.0); }

    void update() {
        double dt = Config::get().deltaTime;

        Vn += g * dt;
        y += Vn * dt;

        x += Vt * dt;


        if (x + R >= Config::get().SCREEN_WIDTH) {
            x = Config::get().SCREEN_WIDTH - R;

            Vt = -Vt * restitution;
        }
        if (x - R <= 0) {
            x = R;

            Vt = -Vt * restitution;
        }

        if (y + R >= Config::get().SCREEN_HEIGHT) {
            y = Config::get().SCREEN_HEIGHT - R;

            // apply friction if rolling/slipping don't care rn tbh
            double eps_threshold = 1e-1; 
            if (Vn <= eps_threshold) {
                Vt -= fetchSign(Vt)*g*mu_k*dt;
            }

            Vn = -Vn * restitution;
        }
    }


    void render(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        for (int w = 0; w < R * 2; ++w) {
            for (int h = 0; h < R * 2; ++h) {
                int dx = R - w;
                int dy = R - h;
                if (dx * dx + dy * dy <= R * R) {
                    SDL_RenderPoint(renderer, static_cast<int>(x) + dx, static_cast<int>(y) + dy);
                }
            }
        }
    }

    void print_coord() {
        std::cout << x << " " << y << std::endl;
    }


};