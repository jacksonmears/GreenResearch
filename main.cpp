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

// ---- Math helpers (column-major matrices) ----
struct Vec3 { float x,y,z; };
static Vec3 cross(const Vec3& a, const Vec3& b) {
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}
static float dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Vec3 normalize(const Vec3& v) {
    float L = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    return { v.x / L, v.y / L, v.z / L };
}

// produce a 4x4 perspective (column-major) matrix
std::array<float,16> perspective(float fovYRadians, float aspect, float nearP, float farP) {
    float f = 1.0f / std::tan(fovYRadians / 2.0f);
    std::array<float,16> m = {};
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (farP + nearP) / (nearP - farP);
    m[11] = -1.0f;
    m[14] = (2.0f * farP * nearP) / (nearP - farP);
    // m[15] = 0 set by {}
    return m;
}

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

    const int SCREEN_WIDTH = 1280;
    const int SCREEN_HEIGHT = 720;

    // request OpenGL 3.3 core
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("3D Random Points",
                                          SCREEN_WIDTH,
                                          SCREEN_HEIGHT,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

void updateMovement(std::vector<Particle*>& particles, bool middle) {
    for (Particle* p : particles) {
        p->y += 0.5f + 0.5f*middle;
    }
}

    // Create shader program
    GLuint program = createProgram(vertexShaderSrc, fragmentShaderSrc);

    // Generate 5 random 3D points with colors
    std::mt19937 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_real_distribution<float> posDist(-3.0f, 3.0f);
    std::uniform_real_distribution<float> zDist(-6.0f, -1.0f);
    std::uniform_real_distribution<float> colDist(0.2f, 1.0f);

    const int N = 5;
    std::vector<float> interleaved;
    interleaved.reserve(N * 6); // pos(3) + color(3)
    for (int i = 0; i < N; ++i) {
        float x = posDist(rng);
        float y = posDist(rng);
        float z = zDist(rng);
        float r = colDist(rng);
        float g = colDist(rng);
        float b = colDist(rng);
        interleaved.push_back(x);
        interleaved.push_back(y);
        interleaved.push_back(z);
        interleaved.push_back(r);
        interleaved.push_back(g);
        interleaved.push_back(b);
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

    return weightedScalar/planeCount;
}



int main(int argc, char** argv) {
    
    std::ifstream file("point_clouds/backyard_space.xyz"); 
    if (!file.is_open()) {
        std::cerr << "Failed to open file\n";
        return 1;
    }

    std::vector<Particle> particles;
    particles.reserve(5'000'000); // withouth this the particles vector is reblocked and pointers created in cellMap are invalid. a very awesome real life case of the reappointing of capacity and the real dangers of pointers and their safety!
    std::string line;
    float grid_resolution = Config::get().grid_resolution;
    std::unordered_map<size_t, std::vector<Particle*>> cellMap;
    std::unordered_map<size_t, SlopeResult> planes;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        float x, y, z;

        if (iss >> x >> y >> z) {
            size_t cell = fetch_cell(x, z);
            // auto [r,g,b] = hashToColor(cell);
            particles.emplace_back(x,y,z, 1,1,1, cell);
            cellMap[cell].push_back(&particles.back());
        }
        // optionally handle lines that don't have 3 floats
    }


    // for (auto [key, value] : cellMap) {
    //     // std::cout << cellMap[key].size() << "\n";
    //     planes[key] = fitPlane(value);
    //     SlopeResult& plane = planes[key];
    //     if (!plane.valid) continue;
    //     int slopePercent = static_cast<int>(std::clamp((std::sqrt(plane.a*plane.a + plane.b*plane.b) * 101.0f), 0.0f, 10.0f));
    //     // std::cout << slopePercent << "\n";
    //     ColorF color = slopeGradient[slopePercent];
    //     // std::vector<float> color(3);
    //     // if (slopePercent > 12) color = {1, 0,0};
    //     // else if (slopePercent >= 6) color = {0.5,0.5, 0};
    //     // else color = {0,0.5,1};
    //     for (Particle* p : cellMap[key]) {
    //         p->r = color.r;
    //         p->g = color.g;
    //         p->b = color.b;
    //     }
    // }




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
        // for(const auto& p : particles) {
        //     if (cellMap[p.grid_index].size() < 3'000) continue;
        //     glColor3f(p.r, p.g, p.b);
        //     glVertex3f(p.x, p.y, p.z);
        // }
        for(int i = 0; i < particles.size(); i += 20) {
            Particle& p = particles[i];
            if (cellMap[p.grid_index].size() < 3'000) continue;
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
