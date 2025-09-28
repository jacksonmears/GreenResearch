#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>
#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <random>
#include <chrono>
#include <array>
#include <cmath>

// ---- Shader sources ----
const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
uniform mat4 uMVP;
uniform float uPointSize;
out vec3 vColor;
void main() {
    vColor = inColor;
    gl_Position = uMVP * vec4(inPos, 1.0);
    gl_PointSize = uPointSize; // allow vertex shader to set point size
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
in vec3 vColor;
out vec4 outColor;
void main() {
    // simple circular point (soft edge)
    float r = length(gl_PointCoord - vec2(0.5));
    if (r > 0.5) discard;
    outColor = vec4(vColor, 1.0);
}
)";

// ---- Utility: compile / link ----
GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string infoLog(len, '\0');
        glGetShaderInfoLog(shader, len, nullptr, &infoLog[0]);
        std::cerr << "Shader compile error: " << infoLog << std::endl;
    }
    return shader;
}

GLuint createProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string infoLog(len, '\0');
        glGetProgramInfoLog(program, len, nullptr, &infoLog[0]);
        std::cerr << "Program link error: " << infoLog << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

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

// lookAt matrix (column-major)
std::array<float,16> lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = normalize({ center.x - eye.x, center.y - eye.y, center.z - eye.z });
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);

    std::array<float,16> m = {};
    // column 0
    m[0] = s.x; m[1] = u.x; m[2] = -f.x; m[3] = 0.0f;
    // column 1
    m[4] = s.y; m[5] = u.y; m[6] = -f.y; m[7] = 0.0f;
    // column 2
    m[8] = s.z; m[9] = u.z; m[10] = -f.z; m[11] = 0.0f;
    // column 3
    m[12] = -dot(s, eye); m[13] = -dot(u, eye); m[14] = dot(f, eye); m[15] = 1.0f;
    return m;
}

std::array<float,16> multiply(const std::array<float,16>& A, const std::array<float,16>& B) {
    std::array<float,16> R = {};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) {
                // column-major indexing: A[k*4 + r] * B[c*4 + k]
                s += A[k*4 + r] * B[c*4 + k];
            }
            R[c*4 + r] = s;
        }
    }
    return R;
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
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

    // init GLEW
    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    // glewInit can produce a GL_INVALID_ENUM harmlessly; clear it
    glGetError();
    if (glewErr != GLEW_OK) {
        std::cerr << "glewInit failed: " << glewGetErrorString(glewErr) << std::endl;
        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
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

    // Setup VAO/VBO
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, interleaved.size() * sizeof(float), interleaved.data(), GL_STATIC_DRAW);

    // position (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    // color (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);

    // GL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE); // allow program to set point size

    bool running = true;
    SDL_Event event;

    float pointSize = 24.0f; // visible size in pixels

    // camera parameters
    Vec3 eye { 0.0f, 0.0f, 2.0f };
    Vec3 center { 0.0f, 0.0f, -2.0f };
    Vec3 up { 0.0f, 1.0f, 0.0f };

    auto lastTime = std::chrono::high_resolution_clock::now();
    int frames = 0;
    int width, height;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.button.button == SDLK_ESCAPE) running = false;
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                // update viewport
                SDL_GetWindowSize(window, &width, &height);
                glViewport(0, 0, width, height);
            }
        }

        // simple orbit camera (optional): rotate around Y slowly
        auto now = std::chrono::high_resolution_clock::now();
        float t = std::chrono::duration<float>(now - lastTime).count();
        // we'll keep camera static for clarity; remove comments below to orbit:
        // float angle = t * 0.3f;
        // eye.x = 6.0f * std::sin(angle);
        // eye.z = 6.0f * std::cos(angle);

        // prepare matrices
        float aspect = (height == 0) ? 1.0f : (float)width / (float)height;
        SDL_GetWindowSize(window, &width, &height);
        auto proj = perspective(45.0f * (3.14159265358979323846f/180.0f), aspect, 0.1f, 100.0f);
        auto view = lookAt(eye, center, up);
        auto mvp = multiply(proj, view);

        // render
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);
        // set uniforms
        GLint locMVP = glGetUniformLocation(program, "uMVP");
        glUniformMatrix4fv(locMVP, 1, GL_FALSE, mvp.data());
        GLint locPoint = glGetUniformLocation(program, "uPointSize");
        glUniform1f(locPoint, pointSize);

        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, N);
        glBindVertexArray(0);
        glUseProgram(0);

        SDL_GL_SwapWindow(window);

        // simple FPS print
        ++frames;
        static auto fpsLast = std::chrono::high_resolution_clock::now();
        auto fpsNow = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(fpsNow - fpsLast).count();
        if (ms >= 1000) {
            std::cout << "FPS: " << frames << std::endl;
            frames = 0;
            fpsLast = fpsNow;
        }
    }

    // cleanup
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);

    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
