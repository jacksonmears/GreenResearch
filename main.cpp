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
#include <algorithm>
#include "headers/Config.h"
#include "headers/fetch_grid.h"
#include "headers/calculate_slopes.h"

// Window settings
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Rotation angles
float rotX = 20.0f;
float rotY = 30.0f;



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


struct ColorF {
    float r, g, b;
};

constexpr std::array<ColorF, 11> slopeGradient = {{
    {1.0f, 1.0f, 1.0f},   // greenish
    {0.0f, 1.0f, 0.502f},   // greenish
    {0.0f, 1.0f, 0.0f},     // green
    {0.251f, 1.0f, 0.0f},
    {0.502f, 1.0f, 0.0f},
    {0.749f, 1.0f, 0.0f},
    {1.0f, 1.0f, 0.0f},     // yellow
    {1.0f, 0.749f, 0.0f},
    {1.0f, 0.502f, 0.0f},
    {1.0f, 0.251f, 0.0f},
    {1.0f, 0.0f, 0.0f}      // red
}};







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



inline int calculateScalarLinear(int slopePercent, float distance) {
    const float maxDist = 2.75f;
    float weight = std::clamp(1.0f - distance / maxDist, 0.0f, 1.0f);
    return static_cast<int>(slopePercent * weight);
}


inline int calculateScalarPoly(int slopePercent, float distance) {
    const float maxDist = 2.0f;
    float t = std::clamp(distance / maxDist, 0.0f, 1.0f);

    // Example: cubic polynomial falloff (smooth start, faster decay)
    // weight = (1 - t)^3
    float weight = (1.0f - t) * (1.0f - t) * (1.0f - t);

    return static_cast<int>(slopePercent * weight);
}




int slopeNeighborsScalar(std::unordered_map<size_t, SlopeResult>& planes, Particle& p, std::vector<size_t>& neighbors) {
    int weightedScalar = 0, planeCount = 0;
    for (size_t cell : neighbors) {
        SlopeResult& plane = planes[cell];
        if (!plane.valid) continue;
        ++planeCount;
        int slopePercent = std::sqrt(plane.a*plane.a + plane.b*plane.b) * 100.0f;
        float dx = plane.cx - p.x;
        float dy = plane.cy - p.y;
        float dz = plane.cz - p.z;
        float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
        weightedScalar += calculateScalarLinear(slopePercent, distance);
    }

    return (planeCount) ? weightedScalar/planeCount : 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::pair<float, size_t> parseFloat4Decimal(const char* s) {
    const char* start = s;

    while (*s == ' ') ++s;

    int sign = 1;
    if (*s == '-') { sign = -1; ++s; }

    int intPart = 0;
    while (*s >= '0' && *s <= '9') {
        intPart = intPart * 10 + (*s - '0');
        ++s;
    }

    ++s; // skip decimal point

    int fracPart = 0;
    while (*s >= '0' && *s <= '9') {
        fracPart = fracPart * 10 + (*s - '0');
        ++s;
    }

    float value = sign * (intPart + fracPart * 0.0001f);
    size_t consumed = s - start;

    return {value, consumed};
}



size_t getSizePCD(const char* file) {
    FILE* fp = fopen(file, "r");
    if (!fp) return 0;

    size_t count = 0;
    const size_t BUF_SIZE = 1 << 20; // 1 MB
    char* buf = new char[BUF_SIZE];
    while (size_t n = fread(buf, 1, sizeof(buf), fp)) {
        for (size_t i = 0; i < n; ++i)
            if (buf[i] == '\n') ++count;
    }
    fclose(fp);
    return count;
}


void readXYZFast(const char* file, std::vector<Particle>& particles, std::unordered_map<size_t, std::vector<Particle*>>& cellMap) {

    size_t particleCount = getSizePCD(file);

    FILE* fp = fopen(file, "r");
    if (!fp) return;

    particles.reserve(particleCount); 

    const size_t BUF_SIZE = 1 << 20; // 1 MB
    char* buf = new char[BUF_SIZE];
    size_t bufEnd = 0, bufPos = 0;

    char line[40]; // each line is ~40 char
    size_t linePos = 0;

    while (true) {
        if (bufPos == bufEnd) {
            bufEnd = fread(buf, 1, sizeof(buf), fp);
            if (bufEnd == 0) break; // EOF
            bufPos = 0;
        }

        char c = buf[bufPos++];
        line[linePos++] = c;

        if (c == '\n') {
            // Parse line
            line[linePos] = '\0';
            auto [x, offset1] = parseFloat4Decimal(line);
            auto [y, offset2] = parseFloat4Decimal(line + offset1);
            auto [z, _] = parseFloat4Decimal(line + offset1 + offset2);
            linePos = 0;
            size_t cell = fetch_cell(x, z);
            particles.emplace_back(x, y, z, 1, 1, 1, cell);
            cellMap[cell].push_back(&particles.back());
        }
    }

    fclose(fp);
}





int main(int argc, char** argv) {


    static auto msStart = std::chrono::high_resolution_clock::now();
    
    const char* file = "point_clouds/south_space.xyz";

    std::vector<Particle> particles;
    std::unordered_map<size_t, std::vector<Particle*>> cellMap;
    std::unordered_map<size_t, SlopeResult> planes;

    readXYZFast(file, particles, cellMap);

    for (auto [key, value] : cellMap) {
        planes[key] = fitPlane(value);
    }


    for (auto [key, value] : cellMap) {
        std::vector<size_t> neighbors = getNeighbors(value[0]->x, value[0]->z);

        for (Particle* p : cellMap[key]) {
            int slopePercentWeightScalar = std::clamp(slopeNeighborsScalar(planes, *p, neighbors), 0, 10);
            ColorF color = slopeGradient[slopePercentWeightScalar];
            p->r = color.r;
            p->g = color.g;
            p->b = color.b;
        }
    }


    auto lastTime = std::chrono::high_resolution_clock::now();
    int frames = 0;
    bool running = true; // turned off temporarily while we attempt to create a more efficient solution (memory and time complexity wise!)
    SDL_Event event;
    bool mouseDown = false;
    int lastMouseX = 0, lastMouseY = 0;
    float cameraDistance = 5.0f;



    // std::vector<std::pair<size_t, bool>> movedHashes; 
    // movedHashes.reserve(20);
    // auto it = cellMap.begin();
    // updateMovement(it->second, 1);


    auto msEnd = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(msEnd - msStart).count();

    std::cout << static_cast<double>(ms)/1000 << std::endl;





////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


    SDL_Window* window = SDL_CreateWindow("3D Particles",
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);

    glEnable(GL_DEPTH_TEST);
    glPointSize(5.0f); // visible particle size



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
        for(int i = 0; i < particles.size(); i += 20) {
            Particle& p = particles[i];
            if (cellMap[p.grid_index].size() < 5'000) continue;
            glColor3f(p.r, p.g, p.b);
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();




        //straight lines from slope perpendicular to surface. still a very good visualization
        // glLineWidth(2.0f);
        // glBegin(GL_LINES);
        // for (SlopeResult& plane : planes) {
        //     if (!plane.valid) continue;
        //     float slopePercent = std::sqrt(plane.a*plane.a + plane.b*plane.b) * 100.0f;
        //     std::cout << "Cell centroid (" << plane.cx << ", " << plane.cy << ", " << plane.cz << ") "
        //             << "Slope: " << slopePercent << "%\n";

        //     float scale = 0.5f; // length of normal
        //     glColor3f(1.0f, 0.0f, 0.0f);

        //     glVertex3f(plane.cx, plane.cy, plane.cz); // start at centroid
        //     glVertex3f(plane.cx + plane.nx*scale,
        //             plane.cy + plane.ny*scale,
        //             plane.cz + plane.nz*scale); // tip of normal
        // }
        // glEnd();

        glLineWidth(2.0f);
        glBegin(GL_LINES);
        for (auto& [key, plane] : planes) {
            if (!plane.valid) continue;

            // compute downhill direction
            float dx = -plane.a;
            float dz = -plane.b;
            float len = std::sqrt(dx*dx + dz*dz);
            float slopePercent = std::sqrt(plane.a*plane.a + plane.b*plane.b) * 100.0f;
            if (len < 1e-6f) continue; // flat cell, skip

            dx /= len; 
            dz /= len;

            float scale = 0.5f; // arrow length
            float startX = plane.cx;
            float startY = plane.cy;
            float startZ = plane.cz;
            float endX = startX + dx * scale;
            float endY = startY; // keep it parallel to the surface
            float endZ = startZ + dz * scale;

            // color by slope magnitude
            float color = std::min(slopePercent/100.0f, 1.0f);
            glColor3f(color, 0.0f, 1.0f - color);

            float yOffset = 0.25f;
            // draw line segment
            glVertex3f(startX, startY + yOffset, startZ);
            glVertex3f(endX, endY + yOffset, endZ);

            // optional: small arrowhead (two small lines)
            float arrowSize = 0.5f * scale;
            glVertex3f(endX, endY + yOffset, endZ);
            glVertex3f(
                endX - dx*arrowSize + dz*arrowSize*0.5f, 
                endY + yOffset, 
                endZ - dz*arrowSize - dx*arrowSize*0.5f
            );

            glVertex3f(endX, endY + yOffset, endZ);
            glVertex3f(
                endX - dx*arrowSize - dz*arrowSize*0.5f, 
                endY + yOffset, 
                endZ - dz*arrowSize + dx*arrowSize*0.5f
            );

            // optional: print slope
            // std::cout << "Slope at (" << plane.cx << ", " << plane.cy << ", " << plane.cz
            //         << ") = " << slopePercent << "%" << std::endl;
        }
        glEnd();




        SDL_GL_SwapWindow(window);

        // simple FPS print
        ++frames;
        static auto fpsLast = std::chrono::high_resolution_clock::now();
        auto fpsNow = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(fpsNow - fpsLast).count();
        if (ms >= 1000) {
            // // Always revert, even the very first time
            // for (auto hash : movedHashes) {
            //     auto itCell = cellMap.find(hash.first);
            //     if (itCell != cellMap.end() && !itCell->second.empty()) {
            //         revertMovement(itCell->second, hash.second);
            //     }
            // }
            // movedHashes.clear();


            // // move iterator to next cell
            // ++it;
            // if (it == cellMap.end()) {
            //     it = cellMap.begin(); // loop back to first cell
            // }

            // // update the new current cell
            // if (it != cellMap.end()) {
            //     std::vector<size_t> neighbors = getNeighbors((it->second)[0]->x, (it->second)[0]->z);
            //     for (auto neighbor : neighbors) {
            //         if (cellMap.find(neighbor) != cellMap.end() && !it->second.empty()) {
            //             bool middle = neighbor == it->first;
            //             movedHashes.emplace_back(neighbor, middle);
            //             updateMovement(cellMap[neighbor], middle);
            //         }
            //     }
            // }

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
