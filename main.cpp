#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>
#include "Body.h"
#include <SDL3/SDL.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>

std::string load_kernel(const char* filename) {
    std::ifstream file(filename);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("GPU Drops",
                                          Config::get().SCREEN_WIDTH,
                                          Config::get().SCREEN_HEIGHT,
                                          SDL_WINDOW_OPENGL);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

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

        // Update Body objects
        for (size_t i = 0; i < N; ++i) {
            body.children[i].x = x[i];
            body.children[i].y = y[i];
            body.children[i].Vn = Vn[i];
            body.children[i].Vt = Vt[i];
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
