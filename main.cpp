#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>
#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <random>
#include <chrono>
#include "headers/Config.h"
#include "headers/fetch_grid.h"

// Window settings
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Rotation angles
float rotX = 20.0f;
float rotY = 30.0f;

// Particle structure
struct Particle {
    float x, y, z;
    float r, g, b;
    size_t grid_index;
};


// inline std::tuple<float, float, float> hashToColor(uint64_t h) {
//     // scramble bits
//     h ^= (h >> 23);
//     h *= 0x2127599bf4325c37ULL;
//     h ^= (h >> 47);

//     // extract bytes
//     uint8_t r = (h >>  0) & 0xFF;
//     uint8_t g = (h >>  8) & 0xFF;
//     uint8_t b = (h >> 16) & 0xFF;

//     // normalize to [0,1] for OpenGL / shaders
//     return { r/255.0f, g/255.0f, b/255.0f };
// }
inline std::tuple<float, float, float> hashToColor(uint64_t h) {
    // scramble bits
    h ^= (h >> 23);
    h *= 0x2127599bf4325c37ULL;
    h ^= (h >> 47);

    // extract bytes
    uint8_t r = (h >>  0) & 0xFF;
    uint8_t g = (h >>  8) & 0xFF;
    uint8_t b = (h >> 16) & 0xFF;

    // normalize to [0,1] and avoid 0
    auto norm = [](uint8_t c) -> float {
        return c / 255.0f * 0.75f + 0.25f;  // scale to [0.1, 1.0] just so no grid cell is EVER completely black
    };

    return { norm(r), norm(g), norm(b) };
}



void revertMovement(std::vector<Particle*>& particles, bool middle) {
    for (Particle* p : particles) {
        p->y -= (0.5f + 0.5f*middle);
    }
}



void updateMovement(std::vector<Particle*>& particles, bool middle) {
    for (Particle* p : particles) {
        p->y += 0.5f + 0.5f*middle;
    }
}



int main(int argc, char** argv) {
    
    std::ifstream file("point_clouds/backyard_space.xyz"); // your file
    if (!file.is_open()) {
        std::cerr << "Failed to open file\n";
        return 1;
    }

    std::vector<Particle> particles;
    particles.reserve(1'500'000); // withouth this the particles vector is reblocked and pointers created in cellMap are invalid. a very awesome real life case of the reappointing of capacity and the real dangers of pointers and their safety!
    std::string line;
    float grid_resolution = Config::get().grid_resolution;
    std::unordered_map<size_t, std::vector<Particle*>> cellMap;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        float x, y, z;

        if (iss >> x >> y >> z) {
            size_t cell = fetch_cell(x, z);
            auto [r,g,b] = hashToColor(cell);
            particles.emplace_back(x,y,z, r,g,b, cell);
            cellMap[cell].push_back(&particles.back());
        }
        // optionally handle lines that don't have 3 floats
    }

    // auto it = cellMap.begin();
    // while (it != cellMap.end()) {
    //     std::cout << it->second.size() << "\n";
    //     it++;
    // }

    // for (int i = 0; i < particles.size(); ++i) {
    //     if (i%10000 == 0) {
    //         std::cout << particles[i].grid_index << "\n";
    //     }
    // }

    SDL_Window* window = SDL_CreateWindow("3D Particles",
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);

    glEnable(GL_DEPTH_TEST);
    glPointSize(5.0f); // visible particle size

    // auto particles = generateParticles(1000); // start with 10

    auto lastTime = std::chrono::high_resolution_clock::now();
    int frames = 0;
    bool running = true;
    SDL_Event event;
    bool mouseDown = false;
    int lastMouseX = 0, lastMouseY = 0;
    float cameraDistance = 5.0f;



    std::vector<std::pair<size_t, bool>> movedHashes; 
    movedHashes.reserve(20);
    auto it = cellMap.begin();
    // updateMovement(it->second, 1);

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
                    cameraDistance -= event.wheel.y * 0.5f; // scroll up → zoom in, down → zoom out
                    if (cameraDistance < 0.1f) cameraDistance = 0.1f; // prevent going too close
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
        for(const auto& p : particles) {
            glColor3f(p.r, p.g, p.b);
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();

        SDL_GL_SwapWindow(window);

        // simple FPS print
        ++frames;
        static auto fpsLast = std::chrono::high_resolution_clock::now();
        auto fpsNow = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(fpsNow - fpsLast).count();
        if (ms >= 1000) {
            // Always revert, even the very first time
            for (auto hash : movedHashes) {
                auto itCell = cellMap.find(hash.first);
                if (itCell != cellMap.end() && !itCell->second.empty()) {
                    revertMovement(itCell->second, hash.second);
                }
            }
            movedHashes.clear();


            // move iterator to next cell
            ++it;
            if (it == cellMap.end()) {
                it = cellMap.begin(); // loop back to first cell
            }

            // update the new current cell
            if (it != cellMap.end()) {
                std::vector<size_t> neighbors = getNeighbors((it->second)[0]->x, (it->second)[0]->z);
                for (auto neighbor : neighbors) {
                    if (cellMap.find(neighbor) != cellMap.end() && !it->second.empty()) {
                        bool middle = neighbor == it->first;
                        movedHashes.emplace_back(neighbor, middle);
                        updateMovement(cellMap[neighbor], middle);
                    }
                }
            }

            std::cout << "FPS: " << frames << std::endl;
            frames = 0;
            fpsLast = fpsNow;
        }
    }

    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
