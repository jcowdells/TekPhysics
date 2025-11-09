#include "primitives.h"

#include <cglm/cam.h>
#include <cglm/mat4.h>
#include <cglm/vec2.h>

#include "../tekgl/entity.h"
#include "../tekgl/manager.h"
#include "../tekgl/shader.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

mat4 projection;
uint line_shader_program = 0;
uint oval_shader_program = 0;
uint image_shader_program = 0;

void tekPrimitiveFramebufferCallback(const int window_width, const int window_height) {
    glm_ortho(0.0f, (float)window_width, (float)window_height, 0.0f, -1.0f, 1.0f, projection);
}

static exception tekGuiCreateRectangularMesh(vec2 point_a, vec2 point_b, TekMesh* mesh) {
    const float vertices[] = {
        point_a[0], point_a[1],
        point_a[0], point_b[1],
        point_b[0], point_b[1],
        point_b[0], point_a[1]
    };

    const uint indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    const int layout[] = {
        2
    };

    tekChainThrow(tekCreateMesh(vertices, 8, indices, 6, layout, 1, mesh));
    return SUCCESS;
}

static exception tekGuiCreateImageMesh(const float width, const float height, TekMesh* mesh) {
    const float vertices[] = {
        0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, height, 0.0f, 0.0f,
        width, height, 1.0f, 0.0f,
        width, 0.0f, 1.0f, 1.0f
    };

    const uint indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    const int layout[] = {
        2, 2
    };

    tekChainThrow(tekCreateMesh(vertices, 16, indices, 6, layout, 2, mesh));
    return SUCCESS;
}

static void tekDeletePrimitives() {
    tekDeleteShaderProgram(line_shader_program);
    tekDeleteShaderProgram(oval_shader_program);
    tekDeleteShaderProgram(image_shader_program);
}

static exception tekGLLoadPrimitives() {
    tekChainThrow(tekCreateShaderProgramVF("../shader/line_vertex.glvs", "../shader/line_fragment.glfs", &line_shader_program));
    tekChainThrow(tekCreateShaderProgramVF("../shader/oval_vertex.glvs", "../shader/oval_fragment.glfs", &oval_shader_program));
    tekChainThrow(tekCreateShaderProgramVF("../shader/image_vertex.glvs", "../shader/image_fragment.glfs", &image_shader_program));
    int window_width, window_height;
    tekChainThrow(tekAddFramebufferCallback(tekPrimitiveFramebufferCallback));
    tekGetWindowSize(&window_width, &window_height);
    glm_ortho(0.0f, (float)window_width, 0.0f, (float)window_height, -1.0f, 1.0f, projection);
    return SUCCESS;
}

tek_init tekInitPrimitives() {
    tekAddGLLoadFunc(tekGLLoadPrimitives);
    tekAddDeleteFunc(tekDeletePrimitives);
}

exception tekGuiCreateLine(vec2 point_a, vec2 point_b, const float thickness, vec4 color, TekGuiLine* line) {
    // find the direction that the line is pointing, and normalise it
    vec2 mod;
    glm_vec2_sub(point_b, point_a, mod);
    glm_vec2_normalize(mod);

    // get the perpendicular vector to the direction
    const float swap = mod[0];
    mod[0] = mod[1];
    mod[1] = -swap;

    // make vector be same length as half the thickness
    // adding on one side, subtracting on other side makes up whole thickness again
    mod[0] *= thickness;
    mod[1] *= thickness;

    // create mesh data for the line
    const float vertices[] = {
        point_a[0] - mod[0], point_a[1] - mod[1],
        point_a[0] + mod[0], point_a[1] + mod[1],
        point_b[0] + mod[0], point_b[1] + mod[1],
        point_b[0] - mod[0], point_b[1] - mod[1]
    };

    const uint indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    const int layout[] = {
        2
    };

    // create mesh and set color of line
    tekChainThrow(tekCreateMesh(vertices, 8, indices, 6, layout, 1, &line->mesh));
    glm_vec4_copy(color, line->color);

    return SUCCESS;
}

exception tekGuiDrawLine(const TekGuiLine* line) {
    tekBindShaderProgram(line_shader_program);
    tekChainThrow(tekShaderUniformMat4(line_shader_program, "projection", projection));
    tekChainThrow(tekShaderUniformVec4(line_shader_program, "line_color", line->color));
    tekDrawMesh(&line->mesh);
    return SUCCESS;
}

void tekGuiDeleteLine(const TekGuiLine* line) {
    tekDeleteMesh(&line->mesh);
}

exception tekGuiCreateOval(vec2 point_a, vec2 point_b, const float thickness, const flag fill, vec4 color, TekGuiOval* oval) {
    tekChainThrow(tekGuiCreateRectangularMesh(point_a, point_b, &oval->mesh));

    oval->center[0] = (point_a[0] + point_b[0]) * 0.5f;
    oval->center[1] = (point_a[1] + point_b[1]) * 0.5f;

    oval->inv_width = 2.0f / (point_b[0] - point_a[0]);
    oval->inv_height = 2.0f / (point_b[1] - point_a[1]);

    if (fill) {
        oval->min_dist = 0.0f;
    } else {
        const float radius = (point_b[0] - point_a[0]) * 0.5f;
        const float fill_dist = radius - thickness;
        oval->min_dist = oval->inv_width * fill_dist;
    }

    glm_vec4_copy(color, oval->color);

    return SUCCESS;
}

exception tekGuiDrawOval(const TekGuiOval* oval) {
    tekBindShaderProgram(oval_shader_program);
    tekChainThrow(tekShaderUniformMat4(oval_shader_program, "projection", projection));
    tekChainThrow(tekShaderUniformFloat(oval_shader_program, "inv_width", oval->inv_width));
    tekChainThrow(tekShaderUniformFloat(oval_shader_program, "inv_height", oval->inv_height));
    tekChainThrow(tekShaderUniformFloat(oval_shader_program, "min_dist", oval->min_dist));
    tekChainThrow(tekShaderUniformVec2(oval_shader_program, "center", oval->center));
    tekChainThrow(tekShaderUniformVec4(oval_shader_program, "oval_color", oval->color));
    tekDrawMesh(&oval->mesh);
    return SUCCESS;
}

void tekGuiDeleteOval(const TekGuiOval* oval) {
    tekDeleteMesh(&oval->mesh);
}

exception tekGuiCreateImage(const float width, const float height, const char* texture_filename, TekGuiImage* image) {
    tekChainThrow(tekGuiCreateImageMesh(width, height, &image->mesh));
    tekChainThrowThen(tekCreateTexture(texture_filename, &image->texture_id), {
        tekDeleteMesh(&image->mesh);
    });

    return SUCCESS;
}

exception tekGuiDrawImage(const TekGuiImage* image, const float x, const float y) {
    tekBindShaderProgram(image_shader_program);
    tekBindTexture(image->texture_id, 0);
    tekChainThrow(tekShaderUniformMat4(image_shader_program, "projection", projection));
    tekChainThrow(tekShaderUniformInt(image_shader_program, "texture_sampler", 0));
    tekChainThrow(tekShaderUniformVec2(image_shader_program, "start_position", (vec2){x, y}));
    tekDrawMesh(&image->mesh);
    return SUCCESS;
}

void tekGuiDeleteImage(const TekGuiImage* image) {
    tekDeleteMesh(&image->mesh);
}