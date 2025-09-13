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

#define NOT_INITIALISED 0
#define INITIALISED 1
#define DE_INITIALISED 2

#define MAX(a, b) (a > b) ? a : b;

static Vector window_mesh_buffer;
static flag window_init = NOT_INITIALISED;

static uint window_vbo;
static uint window_vao;
static uint window_shader;
static flag window_gl_init = NOT_INITIALISED;

static YmlFile options_yml;
static struct TekGuiWindowDefaults window_defaults;
static GLFWcursor* move_cursor;

static exception tekGuiGetWindowDefaults(struct TekGuiWindowDefaults* defaults) {
    if (window_init != INITIALISED) tekThrow(FAILURE, "TekGui not initialised.");

    YmlData* yml_data;

    const exception tek_exception = ymlGet(&options_yml, &yml_data, "window_defaults");
    if (tek_exception == YML_EXCEPTION) {
        tekGuiLog("Missing window defaults section in 'options.yml'.");
        defaults->x_pos = 0;
        defaults->y_pos = 0;
        defaults->width = 0;
        defaults->height = 0;
        defaults->title_width = 0;
        defaults->border_width = 0;
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->background_colour);
        glm_vec4_copy((vec4){1.0f, 1.0f, 1.0f, 1.0f}, defaults->border_colour);
        return SUCCESS;
    }

    long yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "x_pos"));
    ymlDataToInteger(yml_data, &yml_integer);
    defaults->x_pos = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "y_pos"));
    ymlDataToInteger(yml_data, &yml_integer);
    defaults->y_pos = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "width"));
    ymlDataToInteger(yml_data, &yml_integer);
    defaults->width = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "height"));
    ymlDataToInteger(yml_data, &yml_integer);
    defaults->height = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "title_width"));
    ymlDataToInteger(yml_data, &yml_integer);
    defaults->title_width = (uint)yml_integer;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "border_width"));
    ymlDataToInteger(yml_data, &yml_integer);
    defaults->border_width = (uint)yml_integer;

    double yml_float;
    vec4 yml_colour;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "background_colour", "r"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[0] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "background_colour", "g"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[1] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "background_colour", "b"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[2] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "background_colour", "a"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[3] = (float)yml_float;

    glm_vec4_copy(yml_colour, defaults->background_colour);

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "border_colour", "r"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[0] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "border_colour", "g"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[1] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "border_colour", "b"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[2] = (float)yml_float;

    tekChainThrow(ymlGet(&options_yml, &yml_data, "window_defaults", "border_colour", "a"));
    ymlDataToFloat(yml_data, &yml_float);
    yml_colour[3] = (float)yml_float;

    glm_vec4_copy(yml_colour, defaults->border_colour);

    return SUCCESS;
}

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

    tek_exception = ymlReadFile("../tekgui/options.yml", &options_yml);
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

static void tekGuiGetWindowData(const TekGuiWindow* window, TekGuiWindowData* window_data) {
    glm_vec2_copy((vec2){(float)window->x_pos - (float)window->border_width, (float)(window->width + window->x_pos + window->border_width)}, window_data->minmax_x);
    glm_vec2_copy((vec2){(float)window->y_pos - (float)window->title_width, (float)(window->height + window->y_pos + window->border_width)}, window_data->minmax_y);
    glm_vec2_copy((vec2){(float)window->x_pos, (float)(window->x_pos + window->width)}, window_data->minmax_ix);
    glm_vec2_copy((vec2){(float)window->y_pos, (float)(window->y_pos + window->height)}, window_data->minmax_iy);
    memcpy(window_data->background_colour, window->background_colour, sizeof(vec4));
    memcpy(window_data->border_colour, window->border_colour, sizeof(vec4));
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
            window->being_dragged = 1;
            window->x_delta = window->x_pos - (int)callback_data.mouse_x;
            window->y_delta = window->y_pos - (int)callback_data.mouse_y;
            tekSetCursor(CROSSHAIR_CURSOR);
        } else if (callback_data.data.mouse_button.action == GLFW_RELEASE) {
            window->being_dragged = 0;
            tekSetCursor(DEFAULT_CURSOR);
        }
        break;
    case TEK_GUI_BUTTON_MOUSE_LEAVE_CALLBACK:
        window->being_dragged = 0;
        tekSetCursor(DEFAULT_CURSOR);
        break;
    case TEK_GUI_BUTTON_MOUSE_TOUCHING_CALLBACK:
        if (window->being_dragged) {
            window->x_pos = (int)callback_data.mouse_x + window->x_delta;
            window->y_pos = (int)callback_data.mouse_y + window->y_delta;
            TekGuiWindowData window_data;
            tekGuiGetWindowData(window, &window_data);
            tekGuiWindowUpdateButtonHitbox(window);
            tekGuiWindowUpdateGLMesh(window->mesh_index, &window_data);
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

    TekGuiWindowData window_data = {};
    tekGuiGetWindowData(window, &window_data);
    tekGuiWindowAddGLMesh(&window_data, &window->mesh_index);

    window->being_dragged = 0;
    window->x_delta = 0;
    window->y_delta = 0;

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
