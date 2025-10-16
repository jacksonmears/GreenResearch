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
#include <map>
#include <random>
#include <chrono>
#include <algorithm>
#include "headers/Config.h"
#include "headers/fetch_grid.h"
#include "headers/calculate_slopes.h"
#include <filesystem>
#include <cstring>      // for memchr
#include <windows.h>
#include "flat_hash_map.hpp"  // from https://github.com/skarupke/flat_hash_map

// Window settings
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Rotation angles
float rotX = 20.0f;
float rotY = 30.0f;



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




int slopeNeighborsScalar(const ska::flat_hash_map<size_t, Cell>& tt, Particle& p, std::vector<Cell*>& neighbors) {

    int weightedScalar = 0, planeCount = 0;
    for (Cell* cell : neighbors) {
        SlopeResult& plane = cell->plane;
        if (!plane.valid) continue;
        ++planeCount;
        int slopePercent = std::sqrt(plane.a*plane.a + plane.b*plane.b) * 100.0f;
        float dx = plane.xBar - p.x;
        float dy = plane.yBar - p.y;
        float dz = plane.zBar - p.z;
        float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
        weightedScalar += calculateScalarLinear(slopePercent, distance);
    }

    return (planeCount) ? weightedScalar/planeCount : 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



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



float parseFloat4Decimal(char*& data) {
    while (*data == ' ') ++data;

    int sign = 1;
    if (*data == '-') { sign = -1; ++data; }

    int intPart = 0;
    while (*data >= '0' && *data <= '9') {
        intPart = intPart * 10 + (*data - '0');
        ++data;
    }

    ++data; // skip decimal point

    int fracPart = 0;
    while (*data >= '0' && *data <= '9') {
        fracPart = fracPart * 10 + (*data - '0');
        ++data;
    }

    while (*data == '\n' || *data == '\r' || *data == ' ') ++data;

    float value = sign * (intPart + fracPart * 0.0001f);

    return value;
}


void readXYZFast(const char* file, std::vector<Particle>& particles) {

    size_t particleCount = getSizePCD(file);
    particles.reserve(particleCount);


    HANDLE hFile = CreateFileA(
        file, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open file\n";
        return;
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) {
        std::cerr << "Failed to create file mapping\n";
        CloseHandle(hFile);
        return;
    }

    char* data = (char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!data) {
        std::cerr << "Failed to map view of file\n";
        CloseHandle(hMap);
        CloseHandle(hFile);
        return;
    }

    int iterator = 0;
    while (true) {
        ++iterator;
        if (*data == '\0') break;
        float values[3];
        for (int i = 0; i <= 2; ++i) {
            values[i] = parseFloat4Decimal(data);
        }
        // if (iterator%400000 == 0) std::cout << values[0] << " " << values[1] << " " <<  values[2] << "\n";

        size_t cell = fetch_cell(values[0], values[2]);
        particles.emplace_back(values[0], values[1], values[2], 1, 1, 1, cell);
    }


    UnmapViewOfFile(data);
    CloseHandle(hMap);
    CloseHandle(hFile);
}



int main(int argc, char** argv) {



    static auto msStart = std::chrono::high_resolution_clock::now();
    
    const char* file = "point_clouds/south_space.xyz";

    std::vector<Particle> particles;


    ska::flat_hash_map<size_t, std::vector<Particle*>> cellMap;
    std::unordered_map<size_t, SlopeResult> planes;

    // readXYZFast(file, particles, cellMap);
    readXYZFast(file, particles);

    std::sort(particles.begin(), particles.end(),
          [](const Particle& a, const Particle& b) {
              return a.grid_index < b.grid_index;  
          });


    ska::flat_hash_map<size_t, Cell> tt;
    tt[particles[0].grid_index].start_index = 0;
    Particle* prev = &particles[0];
    for (int p = 1; p < particles.size(); ++p) {
        if (particles[p].grid_index != prev->grid_index) {
            tt[prev->grid_index].end_index = p-1;
            tt[particles[p].grid_index].start_index = p;
        }
        prev = &particles[p];
    }
    tt[particles[particles.size()-1].grid_index].end_index = particles.size()-1;


    // for (Particle& p : particles) {
    //     cellMap[p.grid_index].push_back(&p);
    // }



    float scale = 0.5f; // arrow length

    for (auto [key, _] : tt) {
        Cell& c = tt[key];
        c.plane = fitPlane(particles, c.start_index, c.end_index, scale);
    }

    
    for (auto [key, value] : tt) {
        Cell* c = &tt[key];
        std::vector<Cell*> neighbors = getNeighbors(particles[tt[key].start_index].x, particles[tt[key].start_index].z, tt);

        for (int i = tt[key].start_index; i < (*c).end_index; ++i) {
            Particle* p = &particles[i];
            int slopePercentWeightScalar = std::clamp(slopeNeighborsScalar(tt, *p, neighbors), 0, 10);
            ColorF color = slopeGradient[slopePercentWeightScalar];

            // int slopePercentWeightScalar = std::clamp(static_cast<int>((*c).plane.slopePercent), 0, 10); 
            // ColorF color = slopeGradient[slopePercentWeightScalar];
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





    // size_t mx = 0;
    // for (auto [key, value] : cellMap) {
    //     mx = std::max(mx, value.size());
    // }

    // std::cout << mx << "\n";




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



    SDL_Window* window = SDL_CreateWindow("3D Particles",
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);

    glEnable(GL_DEPTH_TEST);
    glPointSize(5.0f); // visible particle size


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
        for(int i = 0; i < particles.size(); i += 50) {
            Particle& p = particles[i];
            if (tt[p.grid_index].end_index-tt[p.grid_index].start_index < 5'000) continue;
            glColor3f(p.r, p.g, p.b);
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();




        glLineWidth(2.0f);
        glBegin(GL_LINES);
        for (auto& [key, cell] : tt) {
            SlopeResult& plane = tt[key].plane;
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
