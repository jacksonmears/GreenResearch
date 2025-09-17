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
    std::string kernel_functions_src = load_kernel("kernel_functions.cl");
    std::string build_grid_src = load_kernel("build_grid.cl");
    std::string update_particles_src = load_kernel("update_particles.cl");
    std::string calculate_densities_src = load_kernel("calculate_densities.cl");
    std::string calculate_pressures_src = load_kernel("calculate_pressures.cl");

    // Concatenate sources into a single program
    std::string full_source = kernel_functions_src + "\n" + build_grid_src + "\n" + update_particles_src + "\n" + calculate_densities_src + "\n" + calculate_pressures_src;
    const char* src = full_source.c_str();
    size_t src_size = full_source.size();

    cl_program program = clCreateProgramWithSource(context, 1, &src, &src_size, nullptr);
    clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);

    cl_int build_status;
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_STATUS,
                        sizeof(build_status), &build_status, nullptr);

    if (build_status != CL_SUCCESS) {
        size_t log_size = 0;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                            0, nullptr, &log_size);
        std::vector<char> log(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                            log_size, log.data(), nullptr);
        std::cerr << "Build log:\n" << log.data() << std::endl;
    }


    // --- Create kernels ---
    cl_kernel build_grid_kernel = clCreateKernel(program, "build_grid", nullptr);
    cl_kernel update_particles_kernel = clCreateKernel(program, "update_particles", nullptr);
    cl_kernel calculate_densities_kernel = clCreateKernel(program, "calculate_densities", nullptr);
    cl_kernel calculate_pressures_kernel = clCreateKernel(program, "calculate_pressures", nullptr);


    size_t N = body.children.size();
    float dt = Config::get().deltaTime;
    float g = Config::get().gravity * Config::get().pixelsPerMeter;
    float mu_k = 0.5f;
    float screen_width = Config::get().SCREEN_WIDTH;
    float screen_height = Config::get().SCREEN_HEIGHT;
    float R = Config::get().R;
    float collision_damping = Config::get().collision_damping;
    float cell_size = Config::get().cell_size;
    int grid_width = screen_width / cell_size;
    int grid_height = screen_height / cell_size;
    int number_of_cells = grid_height * grid_width;
    size_t number_of_cells_size_t = number_of_cells;
    int max_particles = Config::get().max_number_particles_per_cell;
    float smoothing_radius = Config::get().smoothing_radius;
    float PI = Config::get().PI;
    float mass = Config::get().mass;
    float target_density = Config::get().target_density;
    float pressureMultiplier = Config::get().pressureMultiplier;

    std::vector<float> x(N), y(N), Vn(N, 0), Vt(N, 0), pressureForceX(N, 0), pressureForceY(N, 0), pressureAccelerationX(N, 0), pressureAccelerationY(N, 0), densities(N, 0);
    std::vector<int> cell_particles(number_of_cells * max_particles, -1);
    std::vector<int> cell_counts(number_of_cells, 0);
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
    cl_mem cell_particles_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(int)*number_of_cells*max_particles, cell_particles.data(), nullptr);
    cl_mem cell_counts_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(int)*number_of_cells, cell_counts.data(), nullptr);
    cl_mem densities_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, densities.data(), nullptr);


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



    clSetKernelArg(build_grid_kernel, 0, sizeof(cl_mem), &x_buf);
    clSetKernelArg(build_grid_kernel, 1, sizeof(cl_mem), &y_buf);
    clSetKernelArg(build_grid_kernel, 2, sizeof(float), &cell_size);
    clSetKernelArg(build_grid_kernel, 3, sizeof(cl_mem), &cell_particles_buf);
    clSetKernelArg(build_grid_kernel, 4, sizeof(cl_mem), &cell_counts_buf);
    clSetKernelArg(build_grid_kernel, 5, sizeof(int), &grid_width);
    clSetKernelArg(build_grid_kernel, 6, sizeof(int), &grid_height);
    clSetKernelArg(build_grid_kernel, 7, sizeof(int), &max_particles);



    clSetKernelArg(calculate_densities_kernel, 0, sizeof(cl_mem), &x_buf);
    clSetKernelArg(calculate_densities_kernel, 1, sizeof(cl_mem), &y_buf);
    clSetKernelArg(calculate_densities_kernel, 2, sizeof(cl_mem), &cell_particles_buf);
    clSetKernelArg(calculate_densities_kernel, 3, sizeof(cl_mem), &cell_counts_buf);
    clSetKernelArg(calculate_densities_kernel, 4, sizeof(cl_mem), &densities_buf);
    clSetKernelArg(calculate_densities_kernel, 5, sizeof(int), &max_particles);
    clSetKernelArg(calculate_densities_kernel, 6, sizeof(int), &grid_width);
    clSetKernelArg(calculate_densities_kernel, 7, sizeof(int), &grid_height);
    clSetKernelArg(calculate_densities_kernel, 8, sizeof(float), &smoothing_radius);
    clSetKernelArg(calculate_densities_kernel, 9, sizeof(float), &PI);
    clSetKernelArg(calculate_densities_kernel, 10, sizeof(float), &mass);


    clSetKernelArg(calculate_pressures_kernel, 0, sizeof(cl_mem), &cell_particles_buf);
    clSetKernelArg(calculate_pressures_kernel, 1, sizeof(cl_mem), &cell_counts_buf);
    clSetKernelArg(calculate_pressures_kernel, 2, sizeof(cl_mem), &x_buf);
    clSetKernelArg(calculate_pressures_kernel, 3, sizeof(cl_mem), &y_buf);
    clSetKernelArg(calculate_pressures_kernel, 4, sizeof(cl_mem), &densities_buf);
    clSetKernelArg(calculate_pressures_kernel, 5, sizeof(cl_mem), &pressureForceX_buf);
    clSetKernelArg(calculate_pressures_kernel, 6, sizeof(cl_mem), &pressureForceY_buf);
    clSetKernelArg(calculate_pressures_kernel, 7, sizeof(cl_mem), &pressureAccelerationX_buf);
    clSetKernelArg(calculate_pressures_kernel, 8, sizeof(cl_mem), &pressureAccelerationY_buf);
    clSetKernelArg(calculate_pressures_kernel, 9, sizeof(float), &target_density);
    clSetKernelArg(calculate_pressures_kernel, 10, sizeof(float), &pressureMultiplier);
    clSetKernelArg(calculate_pressures_kernel, 11, sizeof(float), &smoothing_radius);
    clSetKernelArg(calculate_pressures_kernel, 12, sizeof(float), &PI);
    clSetKernelArg(calculate_pressures_kernel, 13, sizeof(float), &mass);
    clSetKernelArg(calculate_pressures_kernel, 14, sizeof(int), &grid_width);
    clSetKernelArg(calculate_pressures_kernel, 15, sizeof(int), &grid_height);
    clSetKernelArg(calculate_pressures_kernel, 16, sizeof(int), &max_particles);
    clSetKernelArg(calculate_pressures_kernel, 17, sizeof(float), &collision_damping);

    // --- Main loop ---
    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT) done = true;


        
        // reset vectors 
        std::fill(densities.begin(), densities.end(), 0);
        std::fill(pressureForceX.begin(), pressureForceX.end(), 0);
        std::fill(pressureForceY.begin(), pressureForceY.end(), 0);
        std::fill(pressureAccelerationX.begin(), pressureAccelerationX.end(), 0);
        std::fill(pressureAccelerationY.begin(), pressureAccelerationY.end(), 0);
        std::fill(cell_counts.begin(), cell_counts.end(), 0);
        std::fill(cell_particles.begin(), cell_particles.end(), -1);



        // read updated values back into GPU
        clEnqueueWriteBuffer(queue, x_buf, CL_TRUE, 0, sizeof(float)*N, x.data(), 0, nullptr, nullptr);
        clEnqueueWriteBuffer(queue, y_buf, CL_TRUE, 0, sizeof(float)*N, y.data(), 0, nullptr, nullptr);
        clEnqueueWriteBuffer(queue, cell_particles_buf, CL_TRUE, 0, sizeof(int)*max_particles * number_of_cells, cell_particles.data(), 0, nullptr, nullptr);
        clEnqueueWriteBuffer(queue, cell_counts_buf, CL_TRUE, 0, sizeof(int)*number_of_cells, cell_counts.data(), 0, nullptr, nullptr);


        // run first kernel to place particles in cells for more efficient updates
        clEnqueueNDRangeKernel(queue, build_grid_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        clFinish(queue);

        // read new particles locations in cells
        clEnqueueReadBuffer(queue, cell_particles_buf, CL_TRUE, 0, sizeof(int)*max_particles * number_of_cells, cell_particles.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, cell_counts_buf, CL_TRUE, 0, sizeof(int)*number_of_cells, cell_counts.data(), 0, nullptr, nullptr);



        // run second kernel to calculate densities
        clEnqueueNDRangeKernel(queue, calculate_densities_kernel, 1, nullptr, &number_of_cells_size_t, nullptr, 0, nullptr, nullptr);
        clFinish(queue);

        // read new density values
        clEnqueueReadBuffer(queue, densities_buf, CL_TRUE, 0, sizeof(float)*N, densities.data(), 0, nullptr, nullptr);


        // use new density values to find new pressure values
        clEnqueueNDRangeKernel(queue, calculate_pressures_kernel, 1, nullptr, &number_of_cells_size_t, nullptr, 0, nullptr, nullptr);
        clFinish(queue);

        clEnqueueWriteBuffer(queue, pressureAccelerationX_buf, CL_TRUE, 0, sizeof(float)*N, pressureAccelerationX.data(), 0, nullptr, nullptr);
        clEnqueueWriteBuffer(queue, pressureAccelerationY_buf, CL_TRUE, 0, sizeof(float)*N, pressureAccelerationY.data(), 0, nullptr, nullptr);



        // run last kernel applying new pressure values to particles
        clEnqueueNDRangeKernel(queue, update_particles_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        clFinish(queue);



        // read new position and velocity values for each particle
        clEnqueueReadBuffer(queue, x_buf, CL_TRUE, 0, sizeof(float)*N, x.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, y_buf, CL_TRUE, 0, sizeof(float)*N, y.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, Vn_buf, CL_TRUE, 0, sizeof(float)*N, Vn.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, Vt_buf, CL_TRUE, 0, sizeof(float)*N, Vt.data(), 0, nullptr, nullptr);// reset vectors 
        std::fill(densities.begin(), densities.end(), 0);
        std::fill(pressureForceX.begin(), pressureForceX.end(), 0);
        std::fill(pressureForceY.begin(), pressureForceY.end(), 0);
        std::fill(pressureAccelerationX.begin(), pressureAccelerationX.end(), 0);
        std::fill(pressureAccelerationY.begin(), pressureAccelerationY.end(), 0);
        std::fill(cell_counts.begin(), cell_counts.end(), 0);
        std::fill(cell_particles.begin(), cell_particles.end(), -1);



        // read updated values back into GPU
        clEnqueueWriteBuffer(queue, x_buf, CL_TRUE, 0, sizeof(float)*N, x.data(), 0, nullptr, nullptr);
        clEnqueueWriteBuffer(queue, y_buf, CL_TRUE, 0, sizeof(float)*N, y.data(), 0, nullptr, nullptr);
        clEnqueueWriteBuffer(queue, cell_particles_buf, CL_TRUE, 0, sizeof(int)*max_particles * number_of_cells, cell_particles.data(), 0, nullptr, nullptr);
        clEnqueueWriteBuffer(queue, cell_counts_buf, CL_TRUE, 0, sizeof(int)*number_of_cells, cell_counts.data(), 0, nullptr, nullptr);


        // run first kernel to place particles in cells for more efficient updates
        clEnqueueNDRangeKernel(queue, build_grid_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        clFinish(queue);

        // read new particles locations in cells
        clEnqueueReadBuffer(queue, cell_particles_buf, CL_TRUE, 0, sizeof(int)*max_particles * number_of_cells, cell_particles.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, cell_counts_buf, CL_TRUE, 0, sizeof(int)*number_of_cells, cell_counts.data(), 0, nullptr, nullptr);



        // run second kernel to calculate densities
        clEnqueueNDRangeKernel(queue, calculate_densities_kernel, 1, nullptr, &number_of_cells_size_t, nullptr, 0, nullptr, nullptr);
        clFinish(queue);

        
        clEnqueueNDRangeKernel(queue, calculate_pressures_kernel, 1, nullptr, &number_of_cells_size_t, nullptr, 0, nullptr, nullptr);
        clFinish(queue);



        clEnqueueReadBuffer(queue, pressureForceX_buf, CL_TRUE, 0, sizeof(float)*N, pressureForceX.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, pressureForceY_buf, CL_TRUE, 0, sizeof(float)*N, pressureForceY.data(), 0, nullptr, nullptr);

        for (int i = 0; i < N; ++i) {
            pressureAccelerationX[i] = pressureForceX[i] / densities[i];
            pressureAccelerationY[i] = pressureForceY[i] / densities[i];
            std::cout << pressureForceX[i] << " " << pressureForceY[i] << " " << pressureAccelerationX[i] << " " << pressureAccelerationY[i] << std::endl;
        }

        clEnqueueReadBuffer(queue, pressureAccelerationX_buf, CL_TRUE, 0, sizeof(float)*N, pressureAccelerationX.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, pressureAccelerationY_buf, CL_TRUE, 0, sizeof(float)*N, pressureAccelerationY.data(), 0, nullptr, nullptr);

        // run last kernel applying new pressure values to particles
        clEnqueueNDRangeKernel(queue, update_particles_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        clFinish(queue);

        




        // read new position and velocity values for each particle
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
            // if (cell_particles[i]>0) std::cout << cell_particles[i] << std::endl;  
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


    // for (int i = 0; i < grid_height; ++i) {
    //     std::cout << i << ": ";
    //     for (int j = 0; j < grid_width; ++j) {
    //         int cellindex = i * grid_width + j;
    //         for (int x = 0; x < max_particles; ++x) {
    //             std::cout << cell_particles[cellindex * max_particles + x] << " ";
    //         }std::cout << "    ";
    //     }std::cout << "\n";
    // }


    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}