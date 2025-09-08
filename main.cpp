#include "Shape.h"
#include "Ground.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "Config.h"



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

    SDL_Color red = {255, 0, 0, 255};
    Ball ball(20, 45, screen_height / 2.0, 0, 0.95, red); // start at top middle (dont forget to change back to 0.8)

    Ground* activeGround = new Cartpath();


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
                switch (event.key.key) {
                    // case SDLK_UP:    ball.y -= speed; break;
                    // case SDLK_DOWN:  ball.y += speed; break;
                    case SDLK_LEFT:  ball.velocityX -= VELO_CHANGE; break;
                    case SDLK_RIGHT: ball.velocityX += VELO_CHANGE; break;
                }
            }
        }

        // Do game logic, present a frame, etc.

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw a red circle
        double deltaTime = 1.0 / 5000.0; // assuming 60 FPS
        ball.update(deltaTime, activeGround);
        ball.render(renderer);
        activeGround->render(renderer);

        SDL_RenderPresent(renderer);
    }

    // Close and destroy the window
    SDL_DestroyWindow(window);

    // Clean up
    SDL_Quit();
    return 0;
}