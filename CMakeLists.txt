cmake_minimum_required(VERSION 3.16)
project(myMinecraft)

set(CMAKE_CXX_STANDARD 20)

# Add all source files (main.cpp and Ball.cpp)
add_executable(myMinecraft
    main.cpp
)

# Tell CMake where to find SDL3 headers
include_directories("C:/libs/SDL3/x86_64-w64-mingw32/include")

# Tell CMake where to find SDL3 libraries
link_directories("C:/libs/SDL3/x86_64-w64-mingw32/lib")

include_directories("C:/Users/jacks/OpenCL/include")
link_directories("C:/Users/jacks/OpenCL/lib")

# Link SDL3 and OpenGL
target_link_libraries(myMinecraft SDL3 opengl32)
