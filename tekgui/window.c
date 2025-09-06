#include "tekgui.h"
#include "window.h"

#include <stdio.h>
#include <string.h>
#include <cglm/vec2.h>

#include "../tekgl/manager.h"
#include "../core/vector.h"
#include "glad/glad.h"
#include "../tekgl/shader.h"

#define NOT_INITIALISED 0
#define INITIALISED 1
#define DE_INITIALISED 2

static Vector window_mesh_buffer;
static flag window_init = NOT_INITIALISED;

static uint window_vbo;
static uint window_vao;
static uint window_shader;
static flag window_gl_init = NOT_INITIALISED;

static void tekGuiWindowDelete() {
    vectorDelete(&window_mesh_buffer);

    window_init = DE_INITIALISED;
    window_gl_init = DE_INITIALISED;
}

static exception tekGuiWindowGLLoad() {
    glGenBuffers(1, &window_vbo);
    glGenVertexArrays(1, &window_vao);
    glBindVertexArray(window_vao);
    glBindBuffer(GL_ARRAY_BUFFER, window_vbo);

    const int layout[] = {
        2, 2, 4
    };
    const uint len_layout = sizeof(layout) / sizeof(int);

    // find the total size of each vertex
    int layout_size = 0;
    for (uint i = 0; i < len_layout; i++) layout_size += layout[i];

    // convert size to bytes
    layout_size *= sizeof(float);

    // create attrib pointer for each section of the layout
    int prev_layout = 0;
    for (uint i = 0; i < len_layout; i++) {
        if (layout[i] == 0)
            tekThrow(OPENGL_EXCEPTION, "Cannot have layout of size 0.");
        glVertexAttribPointer(i, layout[i], GL_FLOAT, GL_FALSE, layout_size, (void*)(prev_layout * sizeof(float)));
        glEnableVertexAttribArray(i);
        prev_layout += layout[i];
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    tekChainThrow(tekCreateShaderProgramVGF("../shader/window.glvs", "../shader/window.glgs", "../shader/window.glfs", &window_shader));

    window_gl_init = INITIALISED;
    return SUCCESS;
}

tek_init tekGuiWindowInit() {
    exception tek_exception = vectorCreate(1, sizeof(TekGuiWindowData), &window_mesh_buffer);
    if (tek_exception) return;

    tek_exception = tekAddDeleteFunc(tekGuiWindowDelete);
    if (tek_exception) return;

    tek_exception = tekAddGLLoadFunc(tekGuiWindowGLLoad);
    if (tek_exception) return;

    window_init = INITIALISED;
}

static exception tekGuiWindowAddGLMesh(const TekGuiWindowData* window_data, uint* index) {
    if (!window_init) tekThrow(FAILURE, "Attempted to run function before initialised.");
    if (!window_gl_init) tekThrow(OPENGL_EXCEPTION, "Attempted to run function before OpenGL initialised.");

    // length = index of the next item which will be added
    *index = window_mesh_buffer.length;

    // add new window data to buffer
    tekChainThrow(vectorAddItem(&window_mesh_buffer, window_data));

    // update buffers using glBufferData to add new item
    glBindVertexArray(window_vao);
    glBindBuffer(GL_ARRAY_BUFFER, window_vbo);
    glBufferData(GL_ARRAY_BUFFER, (long)(window_mesh_buffer.length * sizeof(TekGuiWindowData)), window_mesh_buffer.internal, GL_DYNAMIC_DRAW);

    // unbind buffers for cleanliness
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return SUCCESS;
}

static exception tekGuiWindowUpdateGLMesh(const uint index, TekGuiWindowData* window_data) {
    if (!window_init) tekThrow(FAILURE, "Attempted to run function before initialised.");
    if (!window_gl_init) tekThrow(OPENGL_EXCEPTION, "Attempted to run function before OpenGL initialised.");

    // update the vector containing window data
    tekChainThrow(vectorSetItem(&window_mesh_buffer, index, window_data));

    // update the vertex buffer using glSubBufferData to overwrite the old data without reallocating.
    glBindVertexArray(window_vao);
    glBindBuffer(GL_ARRAY_BUFFER, window_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, (long)(index * sizeof(TekGuiWindowData)), sizeof(TekGuiWindowData), window_data);

    // undbind buffers for cleanliness
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return SUCCESS;
}

exception tekGuiCreateWindow(TekGuiWindow* window) {
    struct TekGuiWindowDefaults defaults = {};
    tekChainThrow(tekGuiGetWindowDefaults(&defaults));

    window->type = WINDOW_TYPE_EMPTY;
    window->data = NULL;
    window->x_pos = defaults.x_pos;
    window->y_pos = defaults.y_pos;
    window->width = defaults.width;
    window->height = defaults.height;
    glm_vec4_copy(defaults.colour, window->background_colour);

    TekGuiWindowData window_data = {};
    tekGuiGetWindowData(window, &window_data);
    tekGuiWindowAddGLMesh(&window_data, &window->mesh_index);

    return SUCCESS;
}

exception tekGuiUpdateWindowMesh(const TekGuiWindow* window) {
    TekGuiWindowData* window_data_ptr;
    tekChainThrow(vectorGetItemPtr(&window_mesh_buffer, window->mesh_index, &window_data_ptr));
    tekGuiGetWindowData(window, window_data_ptr);
    return SUCCESS;
}

exception tekGuiDrawWindow(const TekGuiWindow* window) {
    tekBindShaderProgram(window_shader);

    int window_width, window_height;
    tekGetWindowSize(&window_width, &window_height);
    tekChainThrow(tekShaderUniformVec2(window_shader, "window_size", (vec2){(float)window_width, (float)window_height}));

    glBindVertexArray(window_vao);
    glDrawArrays(GL_POINTS, (int)window->mesh_index, 1);

    // unbind everything to keep clean.
    glBindVertexArray(0);
    tekBindShaderProgram(0);

    return SUCCESS;
}

void tekGuiDeleteWindow(TekGuiWindow* window) {

}

void tekGuiGetWindowData(const TekGuiWindow* window, TekGuiWindowData* window_data) {
    glm_vec2_copy((vec2){(float)window->x_pos, (float)(window->width + window->x_pos)}, window_data->minmax_x);
    glm_vec2_copy((vec2){(float)window->y_pos, (float)(window->height + window->y_pos)}, window_data->minmax_y);
    memcpy(window_data->colour, window->background_colour, sizeof(vec4));
}