#include "primitives.h"

#include "../tekgl/manager.h"
#include "../tekgl/shader.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

mat4 projection;
uint line_shader_program = 0;
float pwidth, pheight;

void tekPrimitiveFramebufferCallback(const int window_width, const int window_height) {
    pwidth = (float)window_width;
    pheight = (float)window_height;
    glm_ortho(0.0f, (float)pwidth, 0.0f, (float)pheight, -1.0f, 1.0f, projection);
}

exception tekInitPrimitives() {
    tekChainThrow(tekCreateShaderProgramVF("../shader/line_vertex.glvs", "../shader/line_fragment.glfs", &line_shader_program));
    int window_width, window_height;
    tekChainThrow(tekAddFramebufferCallback(tekPrimitiveFramebufferCallback));
    tekGetWindowSize(&window_width, &window_height);
    pwidth = (float)window_width;
    pheight = (float)window_height;
    glm_ortho(0.0f, (float)pwidth, 0.0f, (float)pheight, -1.0f, 1.0f, projection);
    return SUCCESS;
}

exception tekCreateLine(vec2 point_a, vec2 point_b, const float thickness, vec4 color, TekGuiLine* line) {
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

    for (int i = 0; i < 8; i += 2) {
        printf("%f %f\n", vertices[i], vertices[i+1]);
    }

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

exception tekDrawLine(const TekGuiLine* line) {
    tekBindShaderProgram(line_shader_program);
    tekChainThrow(tekShaderUniformMat4(line_shader_program, "projection", projection));
    tekChainThrow(tekShaderUniformVec4(line_shader_program, "line_color", line->color));
    tekDrawMesh(&line->mesh);
    return SUCCESS;
}

void tekDeleteLine(const TekGuiLine* line) {
    tekDeleteMesh(&line->mesh);
}