#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>
#include "header_files/Body.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>   // SDL’s OpenGL header
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <chrono>
#include <array>



struct GLParticle {
    float x, y;
    float r, g, b, a;
};



std::string load_kernel(const char* filename) {
    std::ifstream file(filename);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}


// SDL_FColor interpolateColor(float Vn, float Vt) {
//     float speed = std::sqrt(Vn*Vn + Vt*Vt);

//     // choose a max speed where the gradient saturates
//     float maxSpeed = 2000.0f;
//     float t = std::clamp(speed / maxSpeed, 0.0f, 1.0f);

//     // Define gradient stops (dark blue → red)
//     static const std::array<SDL_FColor, 15> stops = {{
//         {0.0f, 0.0f, 0.2f, 1.0f},  // dark blue
//         {0.0f, 0.0f, 1.0f, 1.0f},  // blue
//         {0.4f, 0.6f, 1.0f, 1.0f},  // light blue
//         {0.4f, 1.0f, 0.6f, 1.0f},  // light green
//         {0.0f, 1.0f, 0.0f, 1.0f},  // green
//         {0.6f, 1.0f, 0.4f, 1.0f},  // light green again
//         {1.0f, 1.0f, 0.4f, 1.0f},  // light yellow
//         {1.0f, 1.0f, 0.0f, 1.0f},  // yellow
//         {0.8f, 0.7f, 0.0f, 1.0f},  // dark yellow
//         {1.0f, 0.6f, 0.2f, 1.0f},  // light orange
//         {1.0f, 0.5f, 0.0f, 1.0f},  // orange
//         {0.8f, 0.3f, 0.0f, 1.0f},  // dark orange
//         {1.0f, 0.3f, 0.3f, 1.0f},  // light red
//         {1.0f, 0.0f, 0.0f, 1.0f},  // red
//         {0.6f, 0.0f, 0.0f, 1.0f}   // dark red
//     }};

//     // Scale t into the range of stops
//     float scaled = t * (stops.size() - 1);
//     int idx = static_cast<int>(scaled);
//     float frac = scaled - idx;

//     if (idx >= stops.size() - 1)
//         return stops.back();

//     const auto& c1 = stops[idx];
//     const auto& c2 = stops[idx + 1];

//     // Linear interpolation between c1 and c2
//     SDL_FColor result;
//     result.r = c1.r + frac * (c2.r - c1.r);
//     result.g = c1.g + frac * (c2.g - c1.g);
//     result.b = c1.b + frac * (c2.b - c1.b);
//     result.a = 1.0f;

//     return result;
// }



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
    std::string kernel_functions_src = load_kernel("cl_kernels/kernel_functions.cl");
    std::string update_pred_pos_src = load_kernel("cl_kernels/update_pred_pos.cl");
    std::string build_grid_src = load_kernel("cl_kernels/build_grid.cl");
    std::string update_particles_src = load_kernel("cl_kernels/update_particles.cl");
    std::string calculate_densities_src = load_kernel("cl_kernels/calculate_densities.cl");
    std::string calculate_pressures_src = load_kernel("cl_kernels/calculate_pressures.cl");
    std::string update_pressures_src = load_kernel("cl_kernels/update_pressures.cl");
    std::string render_particles_src = load_kernel("cl_kernels/render_particles.cl");


    // Concatenate sources into a single program
    std::string full_source = kernel_functions_src + "\n" + update_pred_pos_src + "\n" + build_grid_src  + "\n" + calculate_densities_src + "\n" + calculate_pressures_src + "\n" + update_pressures_src + "\n" + update_particles_src + "\n" + render_particles_src;
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
    cl_kernel update_pred_pos_kernel = clCreateKernel(program, "update_pred_pos", nullptr);
    cl_kernel build_grid_kernel = clCreateKernel(program, "build_grid", nullptr);
    cl_kernel calculate_densities_kernel = clCreateKernel(program, "calculate_densities", nullptr);
    cl_kernel calculate_pressures_kernel = clCreateKernel(program, "calculate_pressures", nullptr);
    cl_kernel update_pressures_kernel = clCreateKernel(program, "update_pressures", nullptr);
    cl_kernel update_particles_kernel = clCreateKernel(program, "update_particles", nullptr);
    cl_kernel render_particles_kernel = clCreateKernel(program, "render_particles", nullptr);






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
    float air_damping = Config::get().air_damping;
    int leftMouseDown = 0;
    int mouseX = 0, mouseY = 0;

    std::vector<float> x(N), y(N), Vn(N, 0), Vt(N, 0), pressureForceX(N, 0), pressureForceY(N, 0), pressureAccelerationX(N, 0), pressureAccelerationY(N, 0), densities(N, 0), xR(N, 0), yR(N, 0);
    std::vector<int> cell_particles(number_of_cells * max_particles, -1);
    std::vector<int> cell_counts(number_of_cells, 0);
    for (size_t i = 0; i < N; ++i) {
        xR[i] = body.children[i].x;
        yR[i] = body.children[i].y;
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
    cl_mem xR_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, xR.data(), nullptr);
    cl_mem yR_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, yR.data(), nullptr);


    // --- Set update_particles kernel args ---
    clSetKernelArg(update_particles_kernel, 0, sizeof(cl_mem), &xR_buf);
    clSetKernelArg(update_particles_kernel, 1, sizeof(cl_mem), &yR_buf);
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
    clSetKernelArg(update_particles_kernel, 15, sizeof(float), &air_damping);
    clSetKernelArg(update_particles_kernel, 19, sizeof(int), &N);



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
    clSetKernelArg(calculate_densities_kernel, 11, sizeof(float), &target_density);


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
    
    


    clSetKernelArg(update_pressures_kernel, 0, sizeof(cl_mem), &pressureForceX_buf);
    clSetKernelArg(update_pressures_kernel, 1, sizeof(cl_mem), &pressureForceY_buf);
    clSetKernelArg(update_pressures_kernel, 2, sizeof(cl_mem), &densities_buf);
    clSetKernelArg(update_pressures_kernel, 3, sizeof(cl_mem), &pressureAccelerationX_buf);
    clSetKernelArg(update_pressures_kernel, 4, sizeof(cl_mem), &pressureAccelerationY_buf);



    clSetKernelArg(update_pred_pos_kernel, 0, sizeof(cl_mem), &x_buf);
    clSetKernelArg(update_pred_pos_kernel, 1, sizeof(cl_mem), &y_buf);
    clSetKernelArg(update_pred_pos_kernel, 2, sizeof(cl_mem), &xR_buf);
    clSetKernelArg(update_pred_pos_kernel, 3, sizeof(cl_mem), &yR_buf);
    clSetKernelArg(update_pred_pos_kernel, 4, sizeof(cl_mem), &Vn_buf);
    clSetKernelArg(update_pred_pos_kernel, 5, sizeof(cl_mem), &Vt_buf);
    clSetKernelArg(update_pred_pos_kernel, 6, sizeof(float), &dt);
    clSetKernelArg(update_pred_pos_kernel, 7, sizeof(float), &g);
    


    // clSetKernelArg(render_particles_kernel, 0, sizeof(cl_mem), &xR_buf);
    // clSetKernelArg(render_particles_kernel, 1, sizeof(cl_mem), &yR_buf);
    // clSetKernelArg(render_particles_kernel, 2, sizeof(cl_mem), &Vn_buf);
    // clSetKernelArg(render_particles_kernel, 3, sizeof(cl_mem), &Vt_buf);
    // clSetKernelArg(render_particles_kernel, 4, sizeof(cl_mem), &particles_buf);
    // clSetKernelArg(render_particles_kernel, 5, sizeof(int), &N);




    long long frame_count = 0l, total_fram_count;
    auto start_time = std::chrono::high_resolution_clock::now();
    auto last_fps_time = start_time;
    
    // --- Main loop ---
    bool done = false;

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)){
            if (event.type == SDL_EVENT_QUIT) {
                done = true;
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    leftMouseDown = 1;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    leftMouseDown = 0;
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                mouseX = event.motion.x;
                mouseY = event.motion.y;
            }
        }

        clSetKernelArg(update_particles_kernel, 16, sizeof(int), &leftMouseDown);
        clSetKernelArg(update_particles_kernel, 17, sizeof(int), &mouseX);
        clSetKernelArg(update_particles_kernel, 18, sizeof(int), &mouseY);
        

        ++frame_count;


        // std::fill(pressureForceX.begin(), pressureForceX.end(), 0);
        // std::fill(pressureForceY.begin(), pressureForceY.end(), 0);
        // std::fill(cell_counts.begin(), cell_counts.end(), 0);
        // std::fill(cell_particles.begin(), cell_particles.end(), -1);


        float zero_float = 0.0f;
        int zero_int = 0;
        int minus_one = -1;

        // Fill float buffers
        clEnqueueFillBuffer(queue, pressureForceX_buf, &zero_float, sizeof(float), 0, sizeof(float) * N, 0, nullptr, nullptr);
        clEnqueueFillBuffer(queue, pressureForceY_buf, &zero_float, sizeof(float), 0, sizeof(float) * N, 0, nullptr, nullptr);

        // Fill int buffers
        clEnqueueFillBuffer(queue, cell_counts_buf, &zero_int, sizeof(int), 0, sizeof(int) * number_of_cells, 0, nullptr, nullptr);
        clEnqueueFillBuffer(queue, cell_particles_buf, &minus_one, sizeof(int), 0, sizeof(int) * max_particles * number_of_cells, 0, nullptr, nullptr);


        // read updated values back into GPU
        // clEnqueueWriteBuffer(queue, cell_particles_buf, CL_TRUE, 0, sizeof(int)*max_particles * number_of_cells, cell_particles.data(), 0, nullptr, nullptr);
        // clEnqueueWriteBuffer(queue, cell_counts_buf, CL_TRUE, 0, sizeof(int)*number_of_cells, cell_counts.data(), 0, nullptr, nullptr);
        // clEnqueueWriteBuffer(queue, pressureForceX_buf, CL_TRUE, 0, sizeof(float)*N, pressureForceX.data(), 0, nullptr, nullptr);
        // clEnqueueWriteBuffer(queue, pressureForceY_buf, CL_TRUE, 0, sizeof(float)*N, pressureForceY.data(), 0, nullptr, nullptr);

        
        // run kernel to update x and y via predicted positions
        clEnqueueNDRangeKernel(queue, update_pred_pos_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        // clFinish(queue);



        // run first kernel to place particles in cells for more efficient updates
        clEnqueueNDRangeKernel(queue, build_grid_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        // clFinish(queue);



        // // read new particles locations in cells
        // clEnqueueReadBuffer(queue, cell_particles_buf, CL_TRUE, 0, sizeof(int)*max_particles * number_of_cells, cell_particles.data(), 0, nullptr, nullptr);
        // clEnqueueReadBuffer(queue, cell_counts_buf, CL_TRUE, 0, sizeof(int)*number_of_cells, cell_counts.data(), 0, nullptr, nullptr);
        
        // for (int i = 0; i < cell_counts[0]; ++i) {
        //     std::cout << cell_particles[i] << " ";
        // } if (cell_counts[0]) std::cout << "\n";



        // run second kernel to calculate densities
        clEnqueueNDRangeKernel(queue, calculate_densities_kernel, 1, nullptr, &number_of_cells_size_t, nullptr, 0, nullptr, nullptr);
        // clFinish(queue);


        
        clEnqueueNDRangeKernel(queue, calculate_pressures_kernel, 1, nullptr, &number_of_cells_size_t, nullptr, 0, nullptr, nullptr);
        // clFinish(queue);



        // clEnqueueReadBuffer(queue, pressureForceX_buf, CL_TRUE, 0, sizeof(float)*N, pressureForceX.data(), 0, nullptr, nullptr);
        // clEnqueueReadBuffer(queue, pressureForceY_buf, CL_TRUE, 0, sizeof(float)*N, pressureForceY.data(), 0, nullptr, nullptr);


        clEnqueueNDRangeKernel(queue, update_pressures_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        // clFinish(queue);


        // clEnqueueReadBuffer(queue, pressureAccelerationX_buf, CL_TRUE, 0, sizeof(float)*N, pressureAccelerationX.data(), 0, nullptr, nullptr);
        // mx = std::max(mx, *std::max_element(pressureAccelerationX.begin(), pressureAccelerationX.end()));
        // mn = std::min(mn, *std::min_element(pressureAccelerationX.begin(), pressureAccelerationX.end()));

        // clEnqueueWriteBuffer(queue, pressureAccelerationX_buf, CL_TRUE, 0, sizeof(float)*N, pressureAccelerationX.data(), 0, nullptr, nullptr);
        // clEnqueueWriteBuffer(queue, pressureAccelerationY_buf, CL_TRUE, 0, sizeof(float)*N, pressureAccelerationY.data(), 0, nullptr, nullptr);

        // run last kernel applying new pressure values to particles
        clEnqueueNDRangeKernel(queue, update_particles_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        // clFinish(queue);


        // clEnqueueNDRangeKernel(queue, render_particles_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        // clFinish(queue);

        // // Read back just once for rendering
        // clEnqueueReadBuffer(queue, particles_buf, CL_TRUE, 0, sizeof(GLParticle) * N, gpu_particles.data(), 0, nullptr, nullptr);



        // read new position for each particle to render
        clEnqueueReadBuffer(queue, xR_buf, CL_TRUE, 0, sizeof(float)*N, xR.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, yR_buf, CL_TRUE, 0, sizeof(float)*N, yR.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, Vn_buf, CL_TRUE, 0, sizeof(float)*N, Vn.data(), 0, nullptr, nullptr);
        clEnqueueReadBuffer(queue, Vt_buf, CL_TRUE, 0, sizeof(float)*N, Vt.data(), 0, nullptr, nullptr);


        // Update Body objects
        for (size_t i = 0; i < N; ++i) {
            Drop* particle = &body.children[i];
            particle->x = xR[i];
            particle->y = yR[i];
            // particle->color = interpolateColor(Vn[i], Vt[i]);
            // particle->Vn = Vn[i];
            // particle->Vt = Vt[i];
        }

        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();

        // Every 1 second, print FPS and reset counter
        if (elapsed >= 1000) {
            std::cout << "FPS: " << frame_count << std::endl;
            total_fram_count += frame_count;
            frame_count = 0;
            last_fps_time = now;
        }

        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        for (auto& child : body.children)
            child.render(renderer);
        SDL_RenderPresent(renderer);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    double average_fps = total_fram_count / (total_ms / 1000.0);
    std::cout << "average FPS: " << average_fps << std::endl;


    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}