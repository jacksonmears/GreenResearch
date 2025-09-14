#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>
#include "Body.h"
#include <SDL3/SDL.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <numeric>

std::string load_kernel(const char* filename) {
    std::ifstream file(filename);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
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
    if (dst >= radius) return 0;

    float volume  = (Config::get().PI * std::pow(radius, 4)) / 6;
    return fabs((radius - dst) * (radius - dst) / volume);
}

float smoothingKernelDerivative(float dst) {
    float smoothing_radius = Config::get().smoothing_radius;
    float PI = Config::get().PI;

    if (dst >= smoothing_radius) return 0;

    float scale = -12 / (pow(smoothing_radius, 4) * PI);
    return (dst - smoothing_radius) * scale; 
}


void calculateDensities(Body& body, std::vector<float>& densities, int N) {

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            float oY = offsetY(body, i, j), oX = offsetX(body, i, j);
            float dst = sqrt(oY*oY + oX*oX);
            float influence = smoothingKernel(dst);
            densities[i] += Config::get().mass * influence;
        }
    }
}


float convertDensityToPressure(float density) {
    float densityError = density - Config::get().target_density;
    float pressure = densityError * Config::get().pressureMultiplier;
    return pressure;
}


float getRandomDir() {
    return -1.0f + 2.0f * (rand() / (float)RAND_MAX);
}


float calculateSharedPressure(float densityA, float densityB) {
    float pressureA = convertDensityToPressure(densityA);
    float pressureB = convertDensityToPressure(densityB);
    return (pressureA + pressureB) / 2;
}


void calculatePressureForce(Body& body, int N, std::vector<float>& densities, std::vector<float>& pressureForceX, std::vector<float>& pressureForceY, std::vector<float>& pressureAccelerationX, std::vector<float>& pressureAccelerationY) {

    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            float oY = -offsetY(body, i, j), oX = -offsetX(body, i, j);
            float dst = sqrt(oY*oY + oX*oX);
            dst = std::fmax(dst, 1e-5);

            std::pair<float, float> dir = {oX / dst, oY / dst};
            if (dir.first == 0) dir.first = getRandomDir();
            if (dir.second == 0) dir.second = getRandomDir();

            float slope = smoothingKernelDerivative(dst);
            float sharedPressure = calculateSharedPressure(densities[i], densities[j]);
            float mass = Config::get().mass;

            float Fx = sharedPressure * dir.first * slope * mass;
            float Fy = sharedPressure * dir.second * slope * mass;

            // Apply equal and opposite forces
            pressureForceX[i] += Fx / densities[i];
            pressureForceY[i] += Fy / densities[i];
            pressureForceX[j] -= Fx / densities[j];
            pressureForceY[j] -= Fy / densities[j];

            pressureAccelerationX[i] = pressureForceX[i] / densities[i];
            pressureAccelerationY[i] = pressureForceY[i] / densities[i];
            pressureAccelerationX[j] = pressureForceX[j] / densities[j];
            pressureAccelerationY[j] = pressureForceY[j] / densities[j];
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
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

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
    std::vector<float> x(N), y(N), Vn(N, 0), Vt(N, 0), pressureForceX(N, 0), pressureForceY(N, 0), pressureAccelerationX(N, 0), pressureAccelerationY(N, 0), densities(N, 0);
    for (size_t i = 0; i < N; ++i) {
        x[i] = body.children[i].x;
        y[i] = body.children[i].y;
    }


    // --- Buffers ---
    cl_mem x_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, x.data(), nullptr);
    cl_mem y_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, y.data(), nullptr);
    cl_mem Vn_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, Vn.data(), nullptr);
    cl_mem Vt_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, Vt.data(), nullptr);
    cl_mem pressureForceX_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, pressureForceX.data(), nullptr);
    cl_mem pressureForceY_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, pressureForceY.data(), nullptr);
    cl_mem pressureAccelerationX_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, pressureAccelerationX.data(), nullptr);
    cl_mem pressureAccelerationY_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, pressureAccelerationY.data(), nullptr);

    float dt = Config::get().deltaTime;
    float g = Config::get().gravity * Config::get().pixelsPerMeter;
    float mu_k = 0.5f;
    float screen_width = Config::get().SCREEN_WIDTH;
    float screen_height = Config::get().SCREEN_HEIGHT;
    float R = Config::get().R;
    float collision_damping = Config::get().collision_damping;

    // --- Set update_particles kernel args ---
    clSetKernelArg(update_particles_kernel, 0, sizeof(cl_mem), &x_buf);
    clSetKernelArg(update_particles_kernel, 1, sizeof(cl_mem), &y_buf);
    clSetKernelArg(update_particles_kernel, 2, sizeof(cl_mem), &Vn_buf);
    clSetKernelArg(update_particles_kernel, 3, sizeof(cl_mem), &Vt_buf);
    clSetKernelArg(update_particles_kernel, 4, sizeof(float), &R);
    clSetKernelArg(update_particles_kernel, 5, sizeof(float), &dt);
    clSetKernelArg(update_particles_kernel, 6, sizeof(float), &g);
    clSetKernelArg(update_particles_kernel, 7, sizeof(float), &screen_width);
    clSetKernelArg(update_particles_kernel, 8, sizeof(float), &screen_height);
    clSetKernelArg(update_particles_kernel, 9, sizeof(int), &N);
    clSetKernelArg(update_particles_kernel, 10, sizeof(float), &collision_damping);
    clSetKernelArg(update_particles_kernel, 11, sizeof(cl_mem), &pressureForceX_buf);
    clSetKernelArg(update_particles_kernel, 12, sizeof(cl_mem), &pressureForceY_buf);
    clSetKernelArg(update_particles_kernel, 13, sizeof(cl_mem), &pressureAccelerationX_buf);
    clSetKernelArg(update_particles_kernel, 14, sizeof(cl_mem), &pressureAccelerationY_buf);


    // --- Main loop ---
    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT) done = true;




        // 1️⃣ Calculate densities and pressure forces on CPU
        std::fill(densities.begin(), densities.end(), 0);
        std::fill(pressureForceX.begin(), pressureForceX.end(), 0);
        std::fill(pressureForceY.begin(), pressureForceY.end(), 0);
        std::fill(pressureAccelerationX.begin(), pressureAccelerationX.end(), 0);
        std::fill(pressureAccelerationY.begin(), pressureAccelerationY.end(), 0);
        calculateDensities(body, densities, N);
        calculatePressureForce(body, N, densities, pressureForceX, pressureForceY, pressureAccelerationX, pressureAccelerationY);

        // 2️⃣ Upload the updated accelerations to the GPU
        clEnqueueWriteBuffer(queue, pressureAccelerationX_buf, CL_TRUE, 0, sizeof(float)*N, pressureAccelerationX.data(), 0, nullptr, nullptr);
        clEnqueueWriteBuffer(queue, pressureAccelerationY_buf, CL_TRUE, 0, sizeof(float)*N, pressureAccelerationY.data(), 0, nullptr, nullptr);

        // 3️⃣ Run your kernels (build grid + update particles)
        clEnqueueNDRangeKernel(queue, build_grid_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        clEnqueueNDRangeKernel(queue, update_particles_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        clFinish(queue);

        // 4️⃣ Read back positions & velocities (and optionally forces if you need them)
        clEnqueueReadBuffer(queue, x_buf, CL_TRUE, 0, sizeof(float)*N, x.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, y_buf, CL_TRUE, 0, sizeof(float)*N, y.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, Vn_buf, CL_TRUE, 0, sizeof(float)*N, Vn.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, Vt_buf, CL_TRUE, 0, sizeof(float)*N, Vt.data(), 0, nullptr, nullptr);



        // Update Body objects
        for (size_t i = 0; i < N; ++i) {
            Drop* particle = &body.children[i];
            // std::cout << pressureAccelerationX[i] << " " << pressureAccelerationY[i] << std::endl;
            // if (pressureForceX[i]) std::cout <<  pressureForceX[i] << " " << densities[i] << " " << pressureForceX[i] / densities[i] * dt << std::endl;
            particle->x = x[i];
            particle->y = y[i];
            particle->Vn = Vn[i];
            particle->Vt = Vt[i];
            // particle->pressureForceX = pressureForceX[i];
            // particle->pressureForceY = pressureForceY[i];
            // particle->pressureAccelerationX = pressureAccelerationX[i];
            // particle->pressureAccelerationY = pressureAccelerationY[i];
            // float t = densities[i] / mx;
            // body.children[i].color = {t, 0.0f, 1.0f - t, 1.0f};
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