#define CL_TARGET_OPENCL_VERSION 200

#include <CL/cl.h>
#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "../external/flat_hash_map.hpp"
#include "../include/Slope.h"
#include "../include/Particle.h"
#include "../include/Parse.h"
#include "../include/Grid.h"
#include "../include/Color.h"
#include "../include/Config.h"
#include "../include/Draw.h"
#include "../include/Camera.h"
#include "../include/keyboardInput.h"

#include <vector>
#include <chrono>
#include <algorithm> 



void initWindow() {
    
}



int main(int argc, char** argv) {

    static auto msStart = std::chrono::high_resolution_clock::now();
    
    const char* file = "data/street_space.xyz";

    std::vector<particle::Particle> particles;
    ska::flat_hash_map<size_t, geometry::Cell> cellMap;
    std::unordered_map<size_t, geometry::Slope> planes;

    parse::readXYZFast(file, particles);

    particle::sortParticles(particles);

    geometry::fillCellMap(particles, cellMap);

    color::applyColorGradient(particles, cellMap);



    auto lastTime = std::chrono::high_resolution_clock::now();
    int frames = 0;
    bool running = true; 
    SDL_Event event;
    bool mouseDown = false;
    int lastMouseX = 0, lastMouseY = 0;
    float cameraDistance = 5.0f;
    float rot_x = 20.0f;
    float rot_y = 30.0f;



    SDL_Window* window = SDL_CreateWindow("3D Particles",
        config::SCREEN_WIDTH, config::SCREEN_HEIGHT, SDL_WINDOW_OPENGL);

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);

    glEnable(GL_DEPTH_TEST);
    glPointSize(5.0f); 


    auto msEnd = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(msEnd - msStart).count();

    std::cout << "Execution time:" << static_cast<double>(ms)/1000 << " seconds\n";

    while (running) {
        while (SDL_PollEvent(&event)) {
            input::handleEvents(event, running, mouseDown, lastMouseX, lastMouseY, rot_x, rot_y, cameraDistance);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        camera::setCamera(rot_x, rot_y, cameraDistance);

        draw::drawParticles(particles, cellMap);

        draw::drawArrows(particles, cellMap);

        SDL_GL_SwapWindow(window);

        // ++frames;
        // static auto fpsLast = std::chrono::high_resolution_clock::now();
        // auto fpsNow = std::chrono::high_resolution_clock::now();
        // auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(fpsNow - fpsLast).count();
        // if (ms >= 1000) {
        //     // std::cout << "FPS: " << frames << "\n";
        //     frames = 0;
        //     fpsLast = fpsNow;
        // }
    }

    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
