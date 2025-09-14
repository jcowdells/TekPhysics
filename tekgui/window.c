#include "tekgui.h"
#include "window.h"

#include <stdio.h>
#include <string.h>
#include <cglm/vec2.h>
#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include "../tekgl/manager.h"
#include "../core/vector.h"
#include "../core/yml.h"
#include "../tekgl/shader.h"
#include "../core/list.h"

#define NOT_INITIALISED 0
#define INITIALISED 1
#define DE_INITIALISED 2

#define MAX(a, b) (a > b) ? a : b;

static Vector window_mesh_buffer;
static List window_ptr_list;
static flag window_init = NOT_INITIALISED;

static uint window_vbo;
static uint window_vao;
static uint window_shader;
static flag window_gl_init = NOT_INITIALISED;

static struct TekGuiWindowDefaults window_defaults;
static GLFWcursor* move_cursor;

static void tekGuiWindowDelete() {
    vectorDelete(&window_mesh_buffer);

    window_init = DE_INITIALISED;
    window_gl_init = DE_INITIALISED;
}

static exception tekGuiWindowGLLoad() {
    tekChainThrow(tekGuiGetWindowDefaults(&window_defaults));

    glGenBuffers(1, &window_vbo);
    glGenVertexArrays(1, &window_vao);
    glBindVertexArray(window_vao);
    glBindBuffer(GL_ARRAY_BUFFER, window_vbo);

    const int layout[] = {
        2, 2, 2, 2, 4, 4
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

    move_cursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);

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

    listCreate(&window_ptr_list);

    window_init = INITIALISED;
}

static void tekGuiGetWindowData(const TekGuiWindow* window, TekGuiWindowData* window_data) {
    glm_vec2_copy((vec2){(float)window->x_pos - (float)window->border_width, (float)(window->width + window->x_pos + window->border_width)}, window_data->minmax_x);
    glm_vec2_copy((vec2){(float)window->y_pos - (float)window->title_width, (float)(window->height + window->y_pos + window->border_width)}, window_data->minmax_y);
    glm_vec2_copy((vec2){(float)window->x_pos, (float)(window->x_pos + window->width)}, window_data->minmax_ix);
    glm_vec2_copy((vec2){(float)window->y_pos, (float)(window->y_pos + window->height)}, window_data->minmax_iy);
    memcpy(window_data->background_colour, window->background_colour, sizeof(vec4));
    memcpy(window_data->border_colour, window->border_colour, sizeof(vec4));
}

static exception tekGuiWindowAddGLMesh(const TekGuiWindow* window, uint* index) {
    if (!window_init) tekThrow(FAILURE, "Attempted to run function before initialised.");
    if (!window_gl_init) tekThrow(OPENGL_EXCEPTION, "Attempted to run function before OpenGL initialised.");

    TekGuiWindowData window_data = {};
    tekGuiGetWindowData(window, &window_data);

    // length = index of the next item which will be added
    *index = window_mesh_buffer.length;

    // add new window data to buffer
    tekChainThrow(vectorAddItem(&window_mesh_buffer, &window_data));

    // update buffers using glBufferData to add new item
    glBindVertexArray(window_vao);
    glBindBuffer(GL_ARRAY_BUFFER, window_vbo);
    glBufferData(GL_ARRAY_BUFFER, (long)(window_mesh_buffer.length * sizeof(TekGuiWindowData)), window_mesh_buffer.internal, GL_DYNAMIC_DRAW);

    // unbind buffers for cleanliness
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return SUCCESS;
}

static exception tekGuiWindowUpdateGLMesh(const TekGuiWindow* window) {
    if (!window_init) tekThrow(FAILURE, "Attempted to run function before initialised.");
    if (!window_gl_init) tekThrow(OPENGL_EXCEPTION, "Attempted to run function before OpenGL initialised.");

    TekGuiWindowData window_data;
    tekGuiGetWindowData(window, &window_data);

    // update the vector containing window data
    tekChainThrow(vectorSetItem(&window_mesh_buffer, window->mesh_index, &window_data));

    // update the vertex buffer using glSubBufferData to overwrite the old data without reallocating.
    glBindVertexArray(window_vao);
    glBindBuffer(GL_ARRAY_BUFFER, window_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, (long)(window->mesh_index * sizeof(TekGuiWindowData)), sizeof(TekGuiWindowData), &window_data);

    // unbind buffers for cleanliness
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return SUCCESS;
}

static exception tekGuiBringWindowToFront(const TekGuiWindow* window) {
    const ListItem* item = 0;
    uint index = 0;
    foreach(item, (&window_ptr_list), {
        if (item->data == window) break;
        index++;
    });

    // change the order of the window pointer list
    tekChainThrow(listMoveItem(&window_ptr_list, index, window_ptr_list.length - 1));

    // make sure that the window buttons are also brought to the front.
    tekChainThrow(tekGuiBringButtonToFront(&window->title_button));

    return SUCCESS;
}

static void tekGuiWindowUpdateButtonHitbox(TekGuiWindow* window) {
    window->title_button.hitbox_x = (uint)MAX(0, window->x_pos - (int)window_defaults.border_width);
    window->title_button.hitbox_y = (uint)MAX(0, window->y_pos - (int)window_defaults.title_width);
}

void tekGuiWindowTitleButtonCallback(TekGuiButton* button_ptr, TekGuiButtonCallbackData callback_data) {
    TekGuiWindow* window = (TekGuiWindow*)button_ptr->data;
    switch (callback_data.type) {
    case TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK:
        if (callback_data.data.mouse_button.button != GLFW_MOUSE_BUTTON_LEFT) break;
        if (callback_data.data.mouse_button.action == GLFW_PRESS) {
            printf("Press\n");
            window->being_dragged = 1;
            window->x_delta = window->x_pos - (int)callback_data.mouse_x;
            window->y_delta = window->y_pos - (int)callback_data.mouse_y;
            tekSetCursor(CROSSHAIR_CURSOR);
            tekGuiBringWindowToFront(window);
            printf("Window mesh index: %u\n", window->mesh_index);
        } else if (callback_data.data.mouse_button.action == GLFW_RELEASE) {
            printf("Release\n");
            window->being_dragged = 0;
            tekSetCursor(DEFAULT_CURSOR);
        }
        break;
    case TEK_GUI_BUTTON_MOUSE_LEAVE_CALLBACK:
        printf("Leave\n");
        window->being_dragged = 0;
        tekSetCursor(DEFAULT_CURSOR);
        break;
    case TEK_GUI_BUTTON_MOUSE_TOUCHING_CALLBACK:
        if (window->being_dragged) {
            printf("Drag\n");
            window->x_pos = (int)callback_data.mouse_x + window->x_delta;
            window->y_pos = (int)callback_data.mouse_y + window->y_delta;
            tekGuiWindowUpdateButtonHitbox(window);
            tekGuiWindowUpdateGLMesh(window);
        }
        break;
    default:
        break;
    }
}

exception tekGuiCreateWindow(TekGuiWindow* window) {
    if (!window_init) tekThrow(FAILURE, "Attempted to run function before initialised.");

    window->type = WINDOW_TYPE_EMPTY;
    window->data = NULL;
    window->x_pos = (int)window_defaults.x_pos;
    window->y_pos = (int)window_defaults.y_pos;
    window->width = window_defaults.width;
    window->height = window_defaults.height;
    window->title_width = window_defaults.title_width;
    window->border_width = window_defaults.border_width;
    glm_vec4_copy(window_defaults.background_colour, window->background_colour);
    glm_vec4_copy(window_defaults.border_colour, window->border_colour);

    tekGuiCreateButton(&window->title_button);
    tekGuiWindowUpdateButtonHitbox(window);
    window->title_button.hitbox_width = window_defaults.width + 2 * window_defaults.border_width;
    window->title_button.hitbox_height = window_defaults.title_width;
    window->title_button.data = (void*)window;
    window->title_button.callback = tekGuiWindowTitleButtonCallback;

    tekGuiWindowAddGLMesh(window, &window->mesh_index);

    window->being_dragged = 0;
    window->x_delta = 0;
    window->y_delta = 0;

    listAddItem(&window_ptr_list, window);

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

exception tekGuiDrawAllWindows() {
    glDepthFunc(GL_ALWAYS);
    const ListItem* item = 0;
    foreach(item, (&window_ptr_list), {
        const TekGuiWindow* window = (const TekGuiWindow*)item->data;
        tekChainThrow(tekGuiDrawWindow(window));
    });
    glDepthFunc(GL_LESS);

    return SUCCESS;
}

void tekGuiSetWindowPosition(TekGuiWindow* window, const int x_pos, const int y_pos) {
    window->x_pos = x_pos;
    window->y_pos = y_pos;
    tekGuiWindowUpdateButtonHitbox(window);
    tekGuiWindowUpdateGLMesh(window);
}

void tekGuiSetWindowSize(TekGuiWindow* window, const uint width, const uint height) {
    window->width = width;
    window->height = height;
    tekGuiWindowUpdateButtonHitbox(window);
    tekGuiWindowUpdateGLMesh(window);
}

void tekGuiSetWindowBackgroundColour(TekGuiWindow* window, vec4 colour) {
    glm_vec4_copy(colour, window->background_colour);
    tekGuiWindowUpdateGLMesh(window);
}

void tekGuiSetWindowBorderColour(TekGuiWindow* window, vec4 colour) {
    glm_vec4_copy(colour, window->border_colour);
    tekGuiWindowUpdateGLMesh(window);
}

void tekGuiDeleteWindow(TekGuiWindow* window) {

}
