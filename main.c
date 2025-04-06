#include <stdio.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/mat4.h>
#include "core/exception.h"
#include "tekgl/mesh.h"
#include "tekgl/shader.h"
#include "tekgl/texture.h"
#include "tekgl/font.h"
#include "tekgl/text.h"

#include <cglm/cam.h>

#include "core/list.h"
#include "tekgl/manager.h"
#include "tekgui/primitives.h"
#include "core/yml.h"
#include "tekgl/material.h"

#include "tekgl/camera.h"

#include <time.h>
#include <math.h>

#define printException(x) tekLog(x)

float width, height;
mat4 perp_projection;

void tekMainFramebufferCallback(const int fb_width, const int fb_height) {
    width = (float)fb_width;
    height = (float)fb_height;
    glm_perspective(1.2f, width / height, 0.1f, 100.0f, perp_projection);
}

int render() {
    tekChainThrow(tekInit("TekPhysics", 640, 480));
    width = 640.0f;
    height = 480.0f;

    tekAddFramebufferCallback(tekMainFramebufferCallback);

    TekBitmapFont verdana;
    printException(tekCreateFreeType());

    tekChainThrow(tekCreateBitmapFont("../res/verdana.ttf", 0, 64, &verdana));

    TekText text;
    tekChainThrow(tekCreateText("TekPhysics Alpha v1.0", 20, &verdana, &text));

    tekDeleteFreeType();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    TekMesh walls = {};

    const float wall_vertices[] = {
        0.0f, 6.0f, -3.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 6.0f,  3.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f,  3.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -3.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -3.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f,  3.0f, 0.0f, 1.0f, 0.0f,
        12.0f, 0.0f,  3.0f, 0.0f, 1.0f, 0.0f,
        12.0f, 0.0f, -3.0f, 0.0f, 1.0f, 0.0f
    };

    const uint wall_indices[] = {
        0, 1, 2,
        0, 2, 3,
        4, 5, 6,
        4, 6, 7
    };

    const uint wall_layout[] = {
        3, 3
    };

    tekChainThrow(tekCreateMesh(wall_vertices, 48, wall_indices, 12, wall_layout, 2, &walls));

    TekMesh cube = {};

    const float cube_vertices[] = {
        -1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,  0.0f, 1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,  0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,  0.0f, 0.0f, 1.0f,
         1.0f,  1.0f,  1.0f,  0.0f, 0.0f, 1.0f,
         1.0f, -1.0f,  1.0f,  0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,  0.0f, 0.0f, 1.0f
    };

    const uint cube_indices[] = {
        0, 1, 2,
        0, 2, 3,
        4, 5, 6,
        4, 6, 7,
        8, 9, 10,
        8, 10, 11
    };

    const uint cube_layout[] = {
        3, 3
    };

    tekCreateMesh(cube_vertices, 72, cube_indices, 36, cube_layout, 2, &cube);

    vec3 camera_position = {5.0f, 3.0f, 5.0f};
    vec3 light_color = {0.4f, 0.4f, 0.4f};
    vec3 light_position = {4.0f, 12.0f, 0.0f};

    uint shader_program;
    tekChainThrow(tekCreateShaderProgramVF("../shader/vertex.glvs", "../shader/fragment.glfs", &shader_program));

    tekBindShaderProgram(shader_program);

    tekShaderUniformVec3(shader_program, "light_color", light_color);
    tekShaderUniformVec3(shader_program, "light_position", light_position);
    tekShaderUniformVec3(shader_program, "camera_pos", camera_position);

    mat4 wall_model;
    mat4 view;

    glm_mat4_identity(wall_model);

    vec3 center = {0.0f, 0.0f, 0.0f};
    vec3 up = {0.0f, 1.0f, 0.0f};
    glm_lookat(camera_position, center, up, view);

    glm_perspective(1.2f, width / height, 0.1f, 100.0f, perp_projection);

    tekShaderUniformMat4(shader_program, "view", view);

    vec3 cube_position = {3.0f, 1.0f, 0.0f};
    mat4 cube_model;

    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    long time_ns = t.tv_sec * 1000000000 + t.tv_nsec;
    double delta = 0.0;
    double multiplier = -1.0;

    while (tekRunning()) {
        clock_gettime(CLOCK_REALTIME, &t);
        long delta_ns = (t.tv_sec * 1000000000 + t.tv_nsec) - time_ns;
        delta = ((double)delta_ns) / 1000000000.0;
        time_ns = t.tv_sec * 1000000000 + t.tv_nsec;

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);


        tekBindShaderProgram(shader_program);
        tekShaderUniformMat4(shader_program, "projection", perp_projection);

        tekShaderUniformMat4(shader_program, "model", wall_model);
        tekDrawMesh(&walls);

        cube_position[0] += delta * multiplier;
        if (cube_position[0] < 1.0f) {
            cube_position[0] = 1.0f;
            multiplier = 1.0;
        }

        glm_translate_make(cube_model, cube_position);
        tekShaderUniformMat4(shader_program, "model", cube_model);
        tekDrawMesh(&cube);

        printException(tekDrawText(&text, width - 185.0f, 5.0f));
        tekUpdate();
    }

    tekDeleteShaderProgram(shader_program);
    tekDeleteMesh(&walls);
    tekDeleteTextEngine();
    tekDeleteText(&text);
    tekDelete();
    return SUCCESS;
}

int main(void) {
    tekInitExceptions();
    tekLog(render());
    tekCloseExceptions();
}
