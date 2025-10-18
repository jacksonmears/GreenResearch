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

#include <vector>
#include <chrono>
#include <algorithm> 


// Window settings
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Rotation angles
float rotX = 20.0f;
float rotY = 30.0f;


void fillCellMap( std::vector<particle::Particle>& particles, ska::flat_hash_map<size_t, geometry::Cell>& cellMap) {
    cellMap[particles[0].grid_index].start_index = 0;
    particle::Particle* prev = &particles[0];

    for (int p = 1; p < particles.size(); ++p) {
        if (particles[p].grid_index != prev->grid_index) {
            cellMap[prev->grid_index].end_index = p-1;
            cellMap[particles[p].grid_index].start_index = p;
        }
        prev = &particles[p];
    }
    cellMap[particles[particles.size()-1].grid_index].end_index = particles.size()-1;
}



int main(int argc, char** argv) {



    static auto msStart = std::chrono::high_resolution_clock::now();
    
    const char* file = "data/south_space.xyz";

    std::vector<particle::Particle> particles;
    ska::flat_hash_map<size_t, geometry::Cell> cellMap;
    std::unordered_map<size_t, geometry::Slope> planes;

    parse::readXYZFast(file, particles);

    std::sort(particles.begin(), particles.end(), [](const particle::Particle& a, const particle::Particle& b) { return a.grid_index < b.grid_index; });

    fillCellMap(particles, cellMap);

    float scale = 0.5f; // arrow length

    for (auto [key, _] : cellMap) {
        geometry::Cell& c = cellMap[key];
        c.plane = geometry::fitPlane(particles, c.start_index, c.end_index, scale);
    }

    
    for (auto [key, value] : cellMap) {
        geometry::Cell* c = &cellMap[key];
        std::vector<geometry::Cell*> neighbors = grid::getNeighbors(particles[cellMap[key].start_index].x, particles[cellMap[key].start_index].z, cellMap);

        for (int i = cellMap[key].start_index; i < (*c).end_index; ++i) {
            particle::Particle* p = &particles[i];
            int slopePercentWeightScalar = std::clamp(geometry::slopeNeighborsScalar(cellMap, *p, neighbors), 0, 10);
            color::ColorF color = color::slopeGradient[slopePercentWeightScalar];

            p->r = color.r;
            p->g = color.g;
            p->b = color.b;

        }
    }


    auto lastTime = std::chrono::high_resolution_clock::now();
    int frames = 0;
    bool running = true; 
    SDL_Event event;
    bool mouseDown = false;
    int lastMouseX = 0, lastMouseY = 0;
    float cameraDistance = 5.0f;




    SDL_Window* window = SDL_CreateWindow("3D Particles",
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);

    glEnable(GL_DEPTH_TEST);
    glPointSize(5.0f); 


    auto msEnd = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(msEnd - msStart).count();

    std::cout << "Execution time:" << static_cast<double>(ms)/1000 << " seconds\n";

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT: 
                    running = false; 
                    break;
                case SDL_EVENT_KEY_DOWN: 
                    if (event.key.key == SDLK_ESCAPE) running = false; 
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouseDown = true;
                        lastMouseX = event.button.x;
                        lastMouseY = event.button.y;
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (event.button.button == SDL_BUTTON_LEFT) mouseDown = false;
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    if (mouseDown) {
                        int dx = event.motion.x - lastMouseX;
                        int dy = event.motion.y - lastMouseY;
                        rotY += dx * 0.5f;
                        rotX += dy * 0.5f;
                        lastMouseX = event.motion.x;
                        lastMouseY = event.motion.y;
                    }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    cameraDistance -= event.wheel.y * 0.5f; 
                    if (cameraDistance < 0.1f) cameraDistance = 0.1f; 
                    break;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Setup camera
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0, (double)SCREEN_WIDTH / SCREEN_HEIGHT, 0.1, 100.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(0,0,cameraDistance, 0,0,0, 0,1,0);

        glRotatef(rotX, 1, 0, 0);
        glRotatef(rotY, 0, 1, 0);

        // Draw particles
        glBegin(GL_POINTS);
        for(int i = 0; i < particles.size(); i += 50) {
            particle::Particle& p = particles[i];
            if (cellMap[p.grid_index].end_index-cellMap[p.grid_index].start_index < 5'000) continue;
            glColor3f(p.r, p.g, p.b);
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();




        glLineWidth(2.0f);
        glBegin(GL_LINES);
        for (auto& [key, cell] : cellMap) {
            geometry::Slope& plane = cellMap[key].plane;
            if (!plane.valid || plane.len < 1e-6f) continue;

            float yOffset = 0.25f;
            float arrowSize = 0.5f * scale;


            // color by slope magnitude
            glColor3f(plane.color, 0.0f, 1.0f - plane.color);

            // draw line segment
            glVertex3f(plane.xBar, plane.yBar + yOffset, plane.zBar);
            glVertex3f(plane.endX, plane.endY + yOffset, plane.endZ);

            // optional: small arrowhead (two small lines)
            glVertex3f(plane.endX, plane.endY + yOffset, plane.endZ);
            glVertex3f(
                plane.endX - plane.dx*arrowSize + plane.dz*arrowSize*0.5f, 
                plane.endY + yOffset, 
                plane.endZ - plane.dz*arrowSize - plane.dx*arrowSize*0.5f
            );

            glVertex3f(plane.endX, plane.endY + yOffset, plane.endZ);
            glVertex3f(
                plane.endX - plane.dx*arrowSize - plane.dz*arrowSize*0.5f, 
                plane.endY + yOffset, 
                plane.endZ - plane.dz*arrowSize + plane.dx*arrowSize*0.5f
            );
        }
        glEnd();




        SDL_GL_SwapWindow(window);

        ++frames;
        static auto fpsLast = std::chrono::high_resolution_clock::now();
        auto fpsNow = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(fpsNow - fpsLast).count();
        if (ms >= 1000) {
            // std::cout << "FPS: " << frames << "\n";
            frames = 0;
            fpsLast = fpsNow;
        }
    }

    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
