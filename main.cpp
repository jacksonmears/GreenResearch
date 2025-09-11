#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>
#include "Body.h"
#include <SDL3/SDL.h>
#include <vector>
#include <fstream>
#include <iostream>

std::string load_kernel(const char* filename) {
    std::ifstream file(filename);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("GPU Drops", Config::get().SCREEN_WIDTH, Config::get().SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

    Body body;
    body.fill_children(100'000); // tens of thousands of drops

    // --- Create circle texture once ---
    int diameter = 10;
    SDL_Texture* circle_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, diameter, diameter);
    SDL_SetRenderTarget(renderer, circle_texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    int r = diameter / 2;
    for (int w = 0; w < diameter; ++w)
        for (int h = 0; h < diameter; ++h)
            if ((w - r)*(w - r) + (h - r)*(h - r) <= r*r)
                SDL_RenderPoint(renderer, w, h);
    SDL_SetRenderTarget(renderer, nullptr);

    // --- OpenCL Setup ---
    cl_platform_id platform;
    clGetPlatformIDs(1, &platform, nullptr);

    cl_device_id device;
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

    cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, nullptr);
    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, 0, nullptr);

    std::string source = load_kernel("drop_kernel.cl");
    const char* src = source.c_str();
    size_t src_size = source.size();
    cl_program program = clCreateProgramWithSource(context, 1, &src, &src_size, nullptr);
    clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);
    cl_kernel kernel = clCreateKernel(program, "update_drops", nullptr);

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

    cl_mem x_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, x.data(), nullptr);
    cl_mem y_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, y.data(), nullptr);
    cl_mem Vn_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, Vn.data(), nullptr);
    cl_mem Vt_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, Vt.data(), nullptr);
    cl_mem R_buf = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, R.data(), nullptr);
    cl_mem restitution_buf = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, restitution.data(), nullptr);

    float dt = static_cast<float>(Config::get().deltaTime);
    float g = static_cast<float>(Config::get().gravity * Config::get().pixelsPerMeter);
    float mu_k = 0.5f;
    float screen_width = static_cast<float>(Config::get().SCREEN_WIDTH);
    float screen_height = static_cast<float>(Config::get().SCREEN_HEIGHT);

    clSetKernelArg(kernel, 0, sizeof(cl_mem), &x_buf);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &y_buf);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &Vn_buf);
    clSetKernelArg(kernel, 3, sizeof(cl_mem), &Vt_buf);
    clSetKernelArg(kernel, 4, sizeof(cl_mem), &R_buf);
    clSetKernelArg(kernel, 5, sizeof(cl_mem), &restitution_buf);
    clSetKernelArg(kernel, 6, sizeof(float), &dt);
    clSetKernelArg(kernel, 7, sizeof(float), &g);
    clSetKernelArg(kernel, 8, sizeof(float), &mu_k);
    clSetKernelArg(kernel, 9, sizeof(float), &screen_width);
    clSetKernelArg(kernel, 10, sizeof(float), &screen_height);

    // --- Main loop ---
    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT) done = true;

        // GPU physics update
        size_t global_work_size = N;
        clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global_work_size, nullptr, 0, nullptr, nullptr);
        clFinish(queue);

        // Copy back positions for rendering
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

    SDL_DestroyTexture(circle_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
