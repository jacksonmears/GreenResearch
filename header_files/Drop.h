#pragma once
#include "Config.h"
#include <SDL3/SDL.h>
#include <cmath>

class Drop {
public:
    float x, y;
    SDL_FColor color;

    Drop(float x_, float y_, SDL_FColor color_)
        : x(x_), y(y_), color(color_) {}

    void render(SDL_Renderer* renderer) {

        // --------------- DON'T DELETE THIS IS FOR CICLE WITH OPACITY --------------------------
        // const int segments = 10; // more segments = smoother circle
        // float R = Config::get().R;

        // // +1 for center
        // SDL_Vertex vertices[segments + 1];

        // // Center vertex
        // vertices[0].position = { x, y };
        // vertices[0].color = color;
        // vertices[0].tex_coord = {0.0f, 0.0f};

        // // Perimeter vertices
        // for (int i = 0; i < segments; ++i) {
        //     float angle = 2.0f * Config::get().PI * i / segments;
        //     float px = x + R * std::cosf(angle);
        //     float py = y + R * std::sinf(angle);
        //     vertices[i + 1].position = { px, py };
        //     vertices[i + 1].color = color;
        //     vertices[i + 1].tex_coord = {0.0f, 0.0f};
        // }

        // // Indices for triangle fan
        // int indices[segments * 3];
        // for (int i = 0; i < segments; ++i) {
        //     indices[i * 3 + 0] = 0;             // center
        //     indices[i * 3 + 1] = i + 1;         // current perimeter
        //     indices[i * 3 + 2] = (i + 1) % segments + 1; // next perimeter
        // }
        // SDL_RenderGeometry(renderer, nullptr, vertices, segments + 1, indices, segments * 3);



        

        float R = Config::get().R; // half-width of the square
        SDL_Vertex vertices[4];

        // Define corners of the square
        vertices[0].position = { x - R, y - R }; // top-left
        vertices[1].position = { x + R, y - R }; // top-right
        vertices[2].position = { x + R, y + R }; // bottom-right
        vertices[3].position = { x - R, y + R }; // bottom-left

        // Set color
        for (int i = 0; i < 4; ++i) {
            vertices[i].color = color;
            vertices[i].tex_coord = {0.0f, 0.0f};
        }

        // Indices for two triangles
        int indices[6] = {
            0, 1, 2, // first triangle
            0, 2, 3  // second triangle
        };
        SDL_RenderGeometry(renderer, nullptr, vertices, 4, indices, 6);


    }

};
