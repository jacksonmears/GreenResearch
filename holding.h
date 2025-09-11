
#define CL_TARGET_OPENCL_VERSION 200
#include "Body.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "Config.h"
#include <thread>
#include <algorithm>
#include <functional>
#include "Thread_Pool.h"
#include <CL/cl.h>

void update_children_range(std::vector<Drop>& children, size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
        children[i].update();
    }
}



int main(int argc, char* argv[]) {

    SDL_Window *window;                    // Declare a pointer
    bool done = false;

    double screen_width = Config::get().SCREEN_WIDTH;
    double screen_height = Config::get().SCREEN_HEIGHT;
    double gravity = Config::get().gravity;
    double VELO_CHANGE = 5;

    SDL_Init(SDL_INIT_VIDEO);              // Initialize SDL3

    // Create an application window with the following settings:
    window = SDL_CreateWindow(
        "An SDL3 window",                  // window title
        screen_width,                               // width, in pixels
        screen_height,                               // height, in pixels
        SDL_WINDOW_OPENGL                  // flags - see below
    );


    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Body body;
    body.fill_children(40);

    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) {
        num_threads = 20; 
    }
    size_t chunk_size = (body.children.size() + num_threads - 1) / num_threads;

    ThreadPool pool(num_threads);




    // Check that the window was successfully created
    if (window == NULL) {
        // In the case that the window could not be made...
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s\n", SDL_GetError());
        return 1;
    }

    while (!done) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                done = true;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                // switch (event.key.key) {
                //     case SDLK_LEFT:  drop1.Vt -= VELO_CHANGE; break;
                //     case SDLK_RIGHT:  drop2.Vt += VELO_CHANGE; break;
                // }
            }
        }

        // Do game logic, present a frame, etc.


        for (size_t t = 0; t < num_threads; ++t) {
            size_t start = t * chunk_size;
            size_t end = std::min(start + chunk_size, body.children.size());
            if (start < end) {
                pool.enqueue([&, start, end]() {
                    update_children_range(body.children, start, end);
                });
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);


        for (auto& child : body.children) {
            child.render(renderer);
        }

        
        SDL_RenderPresent(renderer);
    }

    // Close and destroy the window
    SDL_DestroyWindow(window);

    // Clean up
    SDL_Quit();
    return 0;
}