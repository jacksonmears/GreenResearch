cmake_minimum_required(VERSION 3.16)
project(myMinecraft)

set(CMAKE_CXX_STANDARD 20)

# Add all source files (main.cpp and Ball.cpp)
add_executable(myMinecraft
    main.cpp
)

include_directories("C:/libs/SDL3/x86_64-w64-mingw32/include")
link_directories("C:/libs/SDL3/x86_64-w64-mingw32/lib")


include_directories("C:/Users/jacks/OpenCL/include")
link_directories("C:/Users/jacks/OpenCL/lib")


include_directories("D:/Apps/glew-2.1.0/include")
link_directories("D:/Apps/glew-2.1.0/lib/Release/x64")

# Link SDL3 and OpenGL
target_link_libraries(myMinecraft GLEW SDL3 opengl32)

