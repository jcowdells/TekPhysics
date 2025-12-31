#include "box_manager.h"

#include <stdio.h>

#include "../tekgl/manager.h"
#include "../tekgl/shader.h"
#include "../core/vector.h"
#include <glad/glad.h>

#define NOT_INITIALISED 0
#define INITIALISED     1
#define DE_INITIALISED  2

static flag box_init = NOT_INITIALISED;
static flag box_gl_init = NOT_INITIALISED;

static uint box_shader = 0;
static uint box_vbo = 0;
static uint box_vao = 0;

static Vector box_mesh_buffer;

/**
 * Delete callback for box code. Free supporting memory.
 */
static void tekGuiBoxDelete() {
    // delete opengl shenanigans
    glDeleteVertexArrays(1, &box_vao);
    glDeleteBuffers(1, &box_vbo);
    tekDeleteShaderProgram(box_shader);

    // delete vector
    vectorDelete(&box_mesh_buffer);

    box_init = DE_INITIALISED;
    box_gl_init = DE_INITIALISED;
}

/**
 * Callback for when opengl loads. Set up the vertex buffer + element buffer + vertex array
 * @throws FAILURE if not initialised.
 * @throws OPENGL_EXCEPTION .
 * @throws SHADER_EXCEPTION .
 */
static exception tekGuiBoxGLLoad() {
    if (box_init != INITIALISED)
        tekThrow(FAILURE, "TekGuiBoxManager was not initialised fully.");

    // create vertex array to keep track of other buffers
    glGenVertexArrays(1, &box_vao);
    glBindVertexArray(box_vao);

    // create vertex buffer to store vertex data
    glGenBuffers(1, &box_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, box_vbo);

    // define layout - 4 x 2 component vectors
    const int layout[] = {
        2, 2, 2, 2
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

    // unbind for cleanliness
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    tekChainThrow(tekCreateShaderProgramVGF("../shader/button.glvs", "../shader/window.glgs", "../shader/window.glfs", &box_shader));

    box_gl_init = INITIALISED;
    return SUCCESS;
}

/**
 * Initialisation function for gui boxes. Set some callbacks and create supporting data structures.
 */
tek_init tekGuiBoxInit() {
    // callbacks
    exception tek_exception = tekAddGLLoadFunc(tekGuiBoxGLLoad);
    if (tek_exception) return;

    tek_exception = tekAddDeleteFunc(tekGuiBoxDelete);
    if (tek_exception) return;

    // create box buffer
    tek_exception = vectorCreate(1, sizeof(TekGuiBoxData), &box_mesh_buffer);
    if (tek_exception) return;

    box_init = INITIALISED;
}

/**
 * Create a new box to be added to the box buffer.
 * @param box_data A pointer to the box data to add to the buffer.
 * @param index The outputted index of where the box data was added to the buffer.
 * @throws FAILURE if box not initialised.
 * @throws OPENGL_EXCEPTION if opengl not initialised.
 */
exception tekGuiCreateBox(const TekGuiBoxData* box_data, uint* index) {
    if (!box_init) tekThrow(FAILURE, "Attempted to run function before initialised.");
    if (!box_gl_init) tekThrow(OPENGL_EXCEPTION, "Attempted to run function before OpenGL initialised.");

    // length = index of the next item which will be added
    *index = box_mesh_buffer.length;

    // add new text button data to buffer
    tekChainThrow(vectorAddItem(&box_mesh_buffer, box_data));

    // update buffers using glBufferData to add new item
    glBindVertexArray(box_vao);
    glBindBuffer(GL_ARRAY_BUFFER, box_vbo);
    glBufferData(GL_ARRAY_BUFFER, (long)(box_mesh_buffer.length * sizeof(TekGuiBoxData)), box_mesh_buffer.internal, GL_DYNAMIC_DRAW);

    // unbind buffers for cleanliness
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return SUCCESS;
}

/**
 * Update a box in the box buffer with new coordinates. Will overwrite the old mesh data.
 * @param box_data A pointer to the new box data to overwrite into the box.
 * @param index The index to write at in the box buffer.
 * @throws FAILURE if not initialised.
 * @throws OPENGL_EXCEPTION if opengl not initialised.
 */
exception tekGuiUpdateBox(const TekGuiBoxData* box_data, const uint index) {
    if (!box_init) tekThrow(FAILURE, "Attempted to run function before initialised.");
    if (!box_gl_init) tekThrow(OPENGL_EXCEPTION, "Attempted to run function before OpenGL initialised.");

    // update the vector containing text button data
    tekChainThrow(vectorSetItem(&box_mesh_buffer, index, box_data));

    // update the vertex buffer using glSubBufferData to overwrite the old data without reallocating.
    glBindVertexArray(box_vao);
    glBindBuffer(GL_ARRAY_BUFFER, box_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, (long)(index * sizeof(TekGuiBoxData)), sizeof(TekGuiBoxData), box_data);

    // unbind buffers for cleanliness
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return SUCCESS;
}

/**
 * Draw a generic box to the screen, with a background and border colour. The box to draw is specified by
 * the index in the buffer containing all boxes, this is given by the \ref tekGuiCreateBox method.
 * @param index The index of the box to draw.
 * @param background_colour The colour to fill the background with (rgba).
 * @param border_colour The colour to fill the border with (rgba).
 * @throws OPENGL_EXCEPTION if index out of range.
 * @throws SHADER_EXCEPTION .
 */
exception tekGuiDrawBox(const uint index, const vec4 background_colour, const vec4 border_colour) {
    if (index >= box_mesh_buffer.length)
        tekThrow(OPENGL_EXCEPTION, "Attempted to draw index out of range.");

    TekGuiBoxData* box_data;
    vectorGetItemPtr(&box_mesh_buffer, index, &box_data);

    // retrieve window size
    int window_width, window_height;
    tekGetWindowSize(&window_width, &window_height);

    // bind shader and the necessary uniforms
    tekBindShaderProgram(box_shader);
    tekChainThrow(tekShaderUniformVec2(box_shader, "window_size", (vec2){(float)window_width, (float)window_height}));
    tekChainThrow(tekShaderUniformVec4(box_shader, "bg_colour", background_colour));
    tekChainThrow(tekShaderUniformVec4(box_shader, "bd_colour", border_colour));

    // draw the box specified by the index.
    glBindVertexArray(box_vao);
    glDrawArrays(GL_POINTS, (int)index, 1);

    // unbind everything for cleanliness.
    glBindVertexArray(0);
    tekBindShaderProgram(0);

    return SUCCESS;
}