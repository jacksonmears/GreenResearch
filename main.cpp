#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>
#include "Body.h"
#include <SDL3/SDL.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

std::string load_kernel(const char* filename) {
    std::ifstream file(filename);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}


void drawCircle(SDL_Renderer* renderer, SDL_FPoint center, float radius, SDL_FColor col) {
    const int steps = 16;
    for (int i = 0; i < steps; ++i) {
        float angle = 2.0f * Config::get().PI * i / steps;
        float x = center.x + radius * cos(angle);
        float y = center.y + radius * sin(angle);
        SDL_FRect dot = { x, y, 1.0f, 1.0f };
        SDL_SetRenderDrawColorFloat(renderer, col.r, col.g, col.b, col.a);
        SDL_RenderFillRect(renderer, &dot);
    }
}


float offsetX(const Body& body, int i , int j) {
    float dx = body.children[i].x - body.children[j].x;

    return dx;
}


float offsetY(const Body& body, int i , int j) {
    float dy = body.children[i].y - body.children[j].y;

    return dy;
}


float smoothingKernel(float dst) {
    float radius = Config::get().smoothing_radius;
    float volume  = Config::get().PI * std::pow(radius, 8) / 4;
    float value = std::max(0.0f, radius*radius - dst*dst);

    return value * value * value / volume; // divide by volume eventually dumby
}

float smoothingKernelDerivative(float dst) {
    float radius = Config::get().smoothing_radius;
    if (dst >= radius) return 0;
    float f = radius*radius - dst*dst;
    float scale = -24 / (Config::get().PI * pow(radius, 8));

    return scale * dst * f * f;
}


std::vector<float> calculateDensities(const Body& body) {
    size_t N = body.children.size();
    std::vector<float> densities(N, 0.0f);

    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) if (i != j) {
            float oY = offsetY(body, i, j), oX = offsetX(body, i, j);
            float dst = sqrt(oY*oY + oX*oX);
            float influence = smoothingKernel(dst);
            densities[i] += Config::get().mass * influence;
        }
    }

    return densities;
}


float convertDensityToPressure(float density) {

    float density_error = density - Config::get().target_density;
    float pressure = density_error * Config::get().pressure_multiplier;
    return pressure;
}


void calculatePressureForce(const Body& body, const std::vector<float>& densities, std::vector<float>& pressureForceX, std::vector<float>& pressureForceY) {
    size_t N = body.children.size();

    // Make sure vectors are correctly sized
    pressureForceX.assign(N, 0.0f);
    pressureForceY.assign(N, 0.0f);

    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
            if (i == j) continue;

            float oX = offsetX(body, i, j);
            float oY = offsetY(body, i, j);
            float dst = std::sqrt(oX*oX + oY*oY);

            // Avoid division by zero
            float dirX = dst > 0.0f ? oX / dst : 0.0f;
            float dirY = dst > 0.0f ? oY / dst : 0.0f;

            float slope = smoothingKernelDerivative(dst);
            float pressure = convertDensityToPressure(densities[j]);
            float mass = body.children[i].mass;
            float density = densities[j];

            pressureForceX[i] += pressure * dirX * slope * mass / density;
            pressureForceY[i] += pressure * dirY * slope * mass / density;
        }
    }
}


SDL_FColor interpolateColor(float Vn, float Vt) {
    float speed = std::sqrt(Vn*Vn + Vt*Vt);

    // choose a max speed where the gradient saturates
    float maxSpeed = 750.0f;  // completely arbitrary but for the current setup the absolute max a particle has reached is 1000.0f
    float t = std::clamp(speed / maxSpeed, 0.0f, 1.0f);

    // Smooth gradient from blue → purple → red
    float r = t;
    float g = 0.0f;
    float b = 1.0f - t;

    return {r, g, b, 1.0f}; // SDL_FColor expects floats [0..1]
}


int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("GPU Drops",
                                          Config::get().SCREEN_WIDTH,
                                          Config::get().SCREEN_HEIGHT,
                                          SDL_WINDOW_OPENGL);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

    Body body;
    body.fill_children(Config::get().num_drops); // testing with 1000 drops

    // --- OpenCL Setup ---
    cl_platform_id platform;
    clGetPlatformIDs(1, &platform, nullptr);

    cl_device_id device;
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

    cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, nullptr);
    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, 0, nullptr);

    // --- Load kernels separately ---
    std::string build_grid_src = load_kernel("build_grid.cl");
    std::string update_particles_src = load_kernel("update_particles.cl");

    // Concatenate sources into a single program
    std::string full_source = build_grid_src + "\n" + update_particles_src;
    const char* src = full_source.c_str();
    size_t src_size = full_source.size();

    cl_program program = clCreateProgramWithSource(context, 1, &src, &src_size, nullptr);
    clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);

    // --- Create kernels ---
    cl_kernel build_grid_kernel = clCreateKernel(program, "build_grid", nullptr);
    cl_kernel update_particles_kernel = clCreateKernel(program, "update_particles", nullptr);


    size_t N = body.children.size();
    std::vector<float> x(N), y(N), Vn(N), Vt(N), R(N), restitution(N);
    for (size_t i = 0; i < N; ++i) {
        x[i] = body.children[i].x;
        y[i] = body.children[i].y;
        Vn[i] = body.children[i].Vn;
        Vt[i] = body.children[i].Vt;
        R[i] = body.children[i].R;
        restitution[i] = body.children[i].restitution;
    }

    const int num_cells = static_cast<int>(Config::get().num_cells);
    std::vector<int> head(num_cells, -1);
    std::vector<int> next(N, -1);
    std::vector<int> killed(N, 0);

    // --- Buffers ---
    cl_mem x_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, x.data(), nullptr);
    cl_mem y_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, y.data(), nullptr);
    cl_mem Vn_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, Vn.data(), nullptr);
    cl_mem Vt_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, Vt.data(), nullptr);
    cl_mem R_buf = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, R.data(), nullptr);
    cl_mem restitution_buf = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, restitution.data(), nullptr);
    cl_mem head_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(int)*num_cells, head.data(), nullptr);
    cl_mem next_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(int)*N, next.data(), nullptr);
    cl_mem killed_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(int)*N, killed.data(), nullptr);

    float cell_size = Config::get().cell_size;
    int grid_width_int = static_cast<int>(Config::get().grid_width);
    int grid_height_int = static_cast<int>(Config::get().grid_height);
    float dt = Config::get().deltaTime;
    float g = Config::get().gravity * Config::get().pixelsPerMeter;
    float mu_k = 0.5f;
    float screen_width = Config::get().SCREEN_WIDTH;
    float screen_height = Config::get().SCREEN_HEIGHT;

    // --- Set build_grid kernel args ---
    clSetKernelArg(build_grid_kernel, 0, sizeof(cl_mem), &x_buf);
    clSetKernelArg(build_grid_kernel, 1, sizeof(cl_mem), &y_buf);
    clSetKernelArg(build_grid_kernel, 2, sizeof(float), &cell_size);
    clSetKernelArg(build_grid_kernel, 3, sizeof(int), &grid_width_int);
    clSetKernelArg(build_grid_kernel, 4, sizeof(int), &grid_height_int);
    clSetKernelArg(build_grid_kernel, 5, sizeof(int), &N);
    clSetKernelArg(build_grid_kernel, 6, sizeof(cl_mem), &head_buf);
    clSetKernelArg(build_grid_kernel, 7, sizeof(cl_mem), &next_buf);

    // --- Set update_particles kernel args ---
    clSetKernelArg(update_particles_kernel, 0, sizeof(cl_mem), &x_buf);
    clSetKernelArg(update_particles_kernel, 1, sizeof(cl_mem), &y_buf);
    clSetKernelArg(update_particles_kernel, 2, sizeof(cl_mem), &Vn_buf);
    clSetKernelArg(update_particles_kernel, 3, sizeof(cl_mem), &Vt_buf);
    clSetKernelArg(update_particles_kernel, 4, sizeof(cl_mem), &R_buf);
    clSetKernelArg(update_particles_kernel, 5, sizeof(float), &dt);
    clSetKernelArg(update_particles_kernel, 6, sizeof(float), &g);
    clSetKernelArg(update_particles_kernel, 7, sizeof(float), &screen_width);
    clSetKernelArg(update_particles_kernel, 8, sizeof(float), &screen_height);
    clSetKernelArg(update_particles_kernel, 9, sizeof(float), &cell_size);
    clSetKernelArg(update_particles_kernel, 10, sizeof(int), &grid_width_int);
    clSetKernelArg(update_particles_kernel, 11, sizeof(int), &grid_height_int);
    clSetKernelArg(update_particles_kernel, 12, sizeof(int), &N);
    clSetKernelArg(update_particles_kernel, 13, sizeof(cl_mem), &head_buf);
    clSetKernelArg(update_particles_kernel, 14, sizeof(cl_mem), &next_buf);
    clSetKernelArg(update_particles_kernel, 15, sizeof(cl_mem), &killed_buf);

    // --- Main loop ---
    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT) done = true;


        // Reset head buffer
        std::fill(head.begin(), head.end(), -1);
        clEnqueueWriteBuffer(queue, head_buf, CL_TRUE, 0, sizeof(int)*num_cells, head.data(), 0, nullptr, nullptr);

        size_t global_work_size = N;

        // 1️⃣ Build the grid
        clEnqueueNDRangeKernel(queue, build_grid_kernel, 1, nullptr, &global_work_size, nullptr, 0, nullptr, nullptr);

        // 2️⃣ Update particles
        clEnqueueNDRangeKernel(queue, update_particles_kernel, 1, nullptr, &global_work_size, nullptr, 0, nullptr, nullptr);
        clFinish(queue);

        // Read back updated positions
        clEnqueueReadBuffer(queue, x_buf, CL_TRUE, 0, sizeof(float)*N, x.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, y_buf, CL_TRUE, 0, sizeof(float)*N, y.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, Vn_buf, CL_TRUE, 0, sizeof(float)*N, Vn.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, Vt_buf, CL_TRUE, 0, sizeof(float)*N, Vt.data(), 0, nullptr, nullptr);

        std::vector<float> densities = calculateDensities(body);

        float mx = *std::max_element(densities.begin(), densities.end());

        for (auto& p : body.children) {
            float radius = Config::get().smoothing_radius;

            SDL_FPoint center = {p.x, p.y};
            SDL_FColor col = {1.0f, 0.0f, 0.0f, 0.2f}; // red aura, semi-transparent
            drawCircle(renderer, center, radius, col);
        }

        // Update Body objects
        for (size_t i = 0; i < N; ++i) {
            body.children[i].x = x[i];
            body.children[i].y = y[i];
            body.children[i].Vn = Vn[i];
            body.children[i].Vt = Vt[i];
            // body.children[i].color = interpolateColor(Vn[i], Vt[i]);
            float t = densities[i] / mx;
            body.children[i].color = {t, 0.0f, 1.0f - t, 1.0f};
        }



        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        for (auto& child : body.children)
            child.render(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
