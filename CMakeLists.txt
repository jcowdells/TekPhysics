cmake_minimum_required(VERSION 3.30)
set(CMAKE_C_STANDARD 11)
project(TekPhysics C)             # Defines name of project
add_executable(
        TekPhysics main.c
        core/exception.h
        core/exception.c
        core/yml.c
        core/yml.h
        tekgl.h
        glad/include/glad/glad.h
        glad/src/glad.c
        tekgl/shader.c
        tekgl/shader.h
        core/file.c
        core/file.h
        tekgl/mesh.c
        tekgl/mesh.h
        stb/stb_image.h
        tekgl/texture.c
        tekgl/texture.h
        tekgl/font.c
        tekgl/font.h
        tekgl/text.c
        tekgl/text.h
        tekgl/manager.c
        tekgl/manager.h
#        tekgui/tekgui.c
#        tekgui/tekgui.h
#        tekgui/container.c
#        tekgui/container.h
#        tekgui/size.c
#        tekgui/size.h
        tekgui/primitives.c
        tekgui/primitives.h
        tekgl/material.c
        tekgl/material.h
        tekgl/camera.h
        tekgl/camera.c
        core/list.c
        core/list.h
        core/stack.c
        core/stack.h
        core/hashtable.h
        core/hashtable.c
        tekphys/body.c
        tekphys/body.h
        core/queue.c
        core/queue.h
        core/threadqueue.c
        core/threadqueue.h
        tekphys/engine.c
        tekphys/engine.h
        core/vector.c
        core/vector.h
        tekgl/entity.c
        tekgl/entity.h
)
find_package(OpenGL REQUIRED)     # Require OpenGL
include_directories(${OPENGL_INCLUDE_DIR})
target_link_libraries(TekPhysics ${OPENGL_LIBRARIES})
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)     # Remove unnecessary includes
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)    #
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) #
add_subdirectory(glfw)
add_subdirectory(cglm)
add_subdirectory(freetype)
target_link_libraries(TekPhysics glfw)       # Link GLFW
target_link_libraries(TekPhysics OpenGL::GL) # Link OpenGL
target_link_libraries(TekPhysics cglm)       # Link CGLM
target_link_libraries(TekPhysics freetype)
target_include_directories(
        TekPhysics PRIVATE
        glfw/include
        cglm/include
        glad/include
        freetype/include
)
