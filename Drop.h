#pragma once
#include "Config.h"
#include <SDL3/SDL.h>

class Drop {
public:
    float R, x, y, mass, restitution, Vn = 0, Vt;
    SDL_FColor color;

    Drop(float x_, float y_, float R_, float mass_, float restitution_, SDL_FColor color_)
        : x(x_), y(y_), R(R_), mass(mass_), restitution(restitution_), color(color_) {}

    // Render a square using SDL_RenderGeometry
    void render(SDL_Renderer* renderer) {
        float halfSize = R;

        SDL_Vertex vertices[4];

        // Top-left
        vertices[0].position = { x - halfSize, y - halfSize };
        vertices[0].color = color;
        vertices[0].tex_coord = { 0.0f, 0.0f };

        // Top-right
        vertices[1].position = { x + halfSize, y - halfSize };
        vertices[1].color = color;
        vertices[1].tex_coord = { 0.0f, 0.0f }; // no texture needed

        // Bottom-right
        vertices[2].position = { x + halfSize, y + halfSize };
        vertices[2].color = color;
        vertices[2].tex_coord = { 0.0f, 0.0f };

        // Bottom-left
        vertices[3].position = { x - halfSize, y + halfSize };
        vertices[3].color = color;
        vertices[3].tex_coord = { 0.0f, 0.0f };

        // Two triangles: 0-1-2 and 0-2-3
        int indices[6] = { 0, 1, 2, 0, 2, 3 };

        // No texture needed, pass nullptr
        SDL_RenderGeometry(renderer, nullptr, vertices, 4, indices, 6);
    }
};
