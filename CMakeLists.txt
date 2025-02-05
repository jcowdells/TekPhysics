cmake_minimum_required(VERSION 3.30)
set(CMAKE_C_STANDARD 11)
project(TekPhysics C)             # Defines name of project
add_executable(TekPhysics main.c) # Tell CMake this is an executable file
find_package(OpenGL REQUIRED)     # Require OpenGL
include_directories(${OPENGL_INCLUDE_DIR})
target_link_libraries(TekPhysics ${OPENGL_LIBRARIES})
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)     # Remove unnecessary includes
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)    #
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) #
add_subdirectory(glfw)
target_link_libraries(TekPhysics glfw)       # Link GLFW
target_link_libraries(TekPhysics OpenGL::GL) # Link OpenGL
target_link_libraries(TekPhysics cglm) # Link OpenGL
target_include_directories(
        TekPhysics PRIVATE
        glfw/include
        cglm/include
)
