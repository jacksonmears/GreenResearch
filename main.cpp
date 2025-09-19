#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>
#include <GL/glew.h>
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



const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec4 inColor;
out vec4 fragColor;
void main() {
    fragColor = inColor;
    gl_PointSize = 5.0; // size of particle
    gl_Position = vec4(inPos * 2.0 - 1.0, 0.0, 1.0); // normalize to [-1,1]
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
in vec4 fragColor;
out vec4 outColor;
void main() {
    outColor = fragColor;
}
)";


GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
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
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program link error: " << infoLog << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}




int main() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("GPU Drops",
                                          Config::get().SCREEN_WIDTH,
                                          Config::get().SCREEN_HEIGHT,
                                          SDL_WINDOW_OPENGL);
    // SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    // SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);


    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    glewInit(); // initialize GLEW

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


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

    GLuint glProgram = createProgram(vertexShaderSrc, fragmentShaderSrc);

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
    std::vector<GLParticle> gpu_particles(N);
    for (size_t i = 0; i < N; ++i) {
        xR[i] = body.children[i].x;
        yR[i] = body.children[i].y;
    }









    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLParticle) * N, nullptr, GL_DYNAMIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLParticle), (void*)0);
    glEnableVertexAttribArray(0);

    // color attribute
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GLParticle), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);










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
    cl_mem gpu_particles_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(GLParticle)*N, gpu_particles.data(), nullptr);


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
    clSetKernelArg(update_particles_kernel, 16, sizeof(int), &leftMouseDown);
    clSetKernelArg(update_particles_kernel, 17, sizeof(int), &mouseX);
    clSetKernelArg(update_particles_kernel, 18, sizeof(int), &mouseY);




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
    


    clSetKernelArg(render_particles_kernel, 0, sizeof(cl_mem), &xR_buf);
    clSetKernelArg(render_particles_kernel, 1, sizeof(cl_mem), &yR_buf);
    clSetKernelArg(render_particles_kernel, 2, sizeof(cl_mem), &Vn_buf);
    clSetKernelArg(render_particles_kernel, 3, sizeof(cl_mem), &Vt_buf);
    clSetKernelArg(render_particles_kernel, 4, sizeof(cl_mem), &gpu_particles_buf);
    clSetKernelArg(render_particles_kernel, 5, sizeof(int), &N);
    clSetKernelArg(render_particles_kernel, 6, sizeof(float), &screen_width);
    clSetKernelArg(render_particles_kernel, 7, sizeof(float), &screen_height);




    long long frame_count = 0l, total_fram_count = 0l;
    auto start_time = std::chrono::high_resolution_clock::now();
    auto last_fps_time = start_time;
    
    // --- Main loop ---
    bool done = false;

    while (!done) {
        ++frame_count;
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
        


        float zero_float = 0.0f;
        int zero_int = 0;
        int minus_one = -1;

        clEnqueueFillBuffer(queue, pressureForceX_buf, &zero_float, sizeof(float), 0, sizeof(float) * N, 0, nullptr, nullptr);
        clEnqueueFillBuffer(queue, pressureForceY_buf, &zero_float, sizeof(float), 0, sizeof(float) * N, 0, nullptr, nullptr);

        clEnqueueFillBuffer(queue, cell_counts_buf, &zero_int, sizeof(int), 0, sizeof(int) * number_of_cells, 0, nullptr, nullptr);
        clEnqueueFillBuffer(queue, cell_particles_buf, &minus_one, sizeof(int), 0, sizeof(int) * max_particles * number_of_cells, 0, nullptr, nullptr);





        clEnqueueNDRangeKernel(queue, update_pred_pos_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);


        clEnqueueNDRangeKernel(queue, build_grid_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);


        clEnqueueNDRangeKernel(queue, calculate_densities_kernel, 1, nullptr, &number_of_cells_size_t, nullptr, 0, nullptr, nullptr);

        
        clEnqueueNDRangeKernel(queue, calculate_pressures_kernel, 1, nullptr, &number_of_cells_size_t, nullptr, 0, nullptr, nullptr);


        clEnqueueNDRangeKernel(queue, update_pressures_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);


        clEnqueueNDRangeKernel(queue, update_particles_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);

        clEnqueueNDRangeKernel(queue, render_particles_kernel, 1, nullptr, &N, nullptr, 0, nullptr, nullptr);
        clFinish(queue);

        clEnqueueReadBuffer(queue, gpu_particles_buf, CL_TRUE, 0, sizeof(GLParticle)*N, gpu_particles.data(), 0, nullptr, nullptr);


        // // Read back just once for rendering
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLParticle)*N, gpu_particles.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);


        // --- DRAW ---
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(glProgram);
        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, N);
        SDL_GL_SwapWindow(window);


        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();

        // Every 1 second, print FPS and reset counter
        if (elapsed >= 1000) {
            std::cout << "FPS: " << frame_count << std::endl;
            total_fram_count += frame_count;
            frame_count = 0;
            last_fps_time = now;
        }

    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    double average_fps = total_fram_count / (total_ms / 1000.0);
    std::cout << "average FPS: " << average_fps << std::endl;


    // SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}