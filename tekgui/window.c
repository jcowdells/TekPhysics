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
#include "box_manager.h"

#define NOT_INITIALISED 0
#define INITIALISED 1
#define DE_INITIALISED 2

#define MAX(a, b) (a > b) ? a : b;

static List window_ptr_list;

static flag window_init = NOT_INITIALISED;
static flag window_gl_init = NOT_INITIALISED;

static struct TekGuiWindowDefaults window_defaults;
static GLFWcursor* move_cursor;

static void tekGuiWindowDelete() {
    listDelete(&window_ptr_list);

    window_init = DE_INITIALISED;
    window_gl_init = DE_INITIALISED;
}

static exception tekGuiWindowGLLoad() {
    tekChainThrow(tekGuiGetWindowDefaults(&window_defaults));

    move_cursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);

    window_gl_init = INITIALISED;
    return SUCCESS;
}

tek_init tekGuiWindowInit() {
    exception tek_exception = tekAddDeleteFunc(tekGuiWindowDelete);
    if (tek_exception) return;

    tek_exception = tekAddGLLoadFunc(tekGuiWindowGLLoad);
    if (tek_exception) return;

    listCreate(&window_ptr_list);

    window_init = INITIALISED;
}

static void tekGuiGetWindowData(const TekGuiWindow* window, TekGuiBoxData* box_data) {
    glm_vec2_copy((vec2){(float)window->x_pos - (float)window->border_width, (float)(window->width + window->x_pos + window->border_width)}, box_data->minmax_x);
    glm_vec2_copy((vec2){(float)window->y_pos - (float)window->title_width, (float)(window->height + window->y_pos + window->border_width)}, box_data->minmax_y);
    glm_vec2_copy((vec2){(float)window->x_pos, (float)(window->x_pos + window->width)}, box_data->minmax_ix);
    glm_vec2_copy((vec2){(float)window->y_pos, (float)(window->y_pos + window->height)}, box_data->minmax_iy);
}

static exception tekGuiWindowAddGLMesh(const TekGuiWindow* window, uint* index) {
    TekGuiBoxData box_data = {};
    tekGuiGetWindowData(window, &box_data);
    tekChainThrow(tekGuiCreateBox(&box_data, index));
    return SUCCESS;
}

static exception tekGuiWindowUpdateGLMesh(const TekGuiWindow* window) {
    TekGuiBoxData box_data = {};
    tekGuiGetWindowData(window, &box_data);
    tekChainThrow(tekGuiUpdateBox(&box_data, window->mesh_index));
    return SUCCESS;
}

exception tekGuiBringWindowToFront(const TekGuiWindow* window) {
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

    // on window select, call back to other windows.
    if (window->select_callback)
        tekChainThrow(window->select_callback(window));

    return SUCCESS;
}

static void tekGuiWindowUpdateButtonHitbox(TekGuiWindow* window) {
    window->title_button.hitbox_x = (uint)MAX(0, window->x_pos - (int)window_defaults.border_width);
    window->title_button.hitbox_y = (uint)MAX(0, window->y_pos - (int)window_defaults.title_width);
}

static exception tekGuiWindowCreateTitleText(TekGuiWindow* window) {
    const uint text_size = window->title_width * 4 / 5;
    TekBitmapFont* font;
    tekChainThrow(tekGuiGetDefaultFont(&font));
    tekChainThrow(tekCreateText(window->title, text_size, font, &window->title_text));
    return SUCCESS;
}

static exception tekGuiWindowRecreateTitleText(TekGuiWindow* window) {
    const uint text_size = window->title_width * 4 / 5;
    tekChainThrow(tekUpdateText(&window->title_text, window->title, text_size));
    return SUCCESS;
}

static void tekGuiWindowTitleButtonCallback(TekGuiButton* button_ptr, TekGuiButtonCallbackData callback_data) {
    TekGuiWindow* window = (TekGuiWindow*)button_ptr->data;
    switch (callback_data.type) {
    case TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK:
        if (callback_data.data.mouse_button.button != GLFW_MOUSE_BUTTON_LEFT) break;
        if (callback_data.data.mouse_button.action == GLFW_PRESS) {
            window->being_dragged = 1;
            window->x_delta = window->x_pos - (int)callback_data.mouse_x;
            window->y_delta = window->y_pos - (int)callback_data.mouse_y;
            tekSetCursor(CROSSHAIR_CURSOR);
            tekGuiBringWindowToFront(window);
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
            window->being_dragged = 1;
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

static exception tekGuiSetWindowTitleBuffer(TekGuiWindow* window, const char* title) {
    if (window->title)
        free(window->title);
    const uint len_title = strlen(title) + 1;
    window->title = (char*)malloc(len_title * sizeof(char));
    if (!window->title)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for title.");
    memcpy(window->title, title, len_title);
    return SUCCESS;
}

exception tekGuiSetWindowTitle(TekGuiWindow* window, const char* title) {
    tekChainThrow(tekGuiSetWindowTitleBuffer(window, title));
    tekChainThrow(tekGuiWindowRecreateTitleText(window));
    return SUCCESS;
}

exception tekGuiCreateWindow(TekGuiWindow* window) {
    if (!window_init) tekThrow(FAILURE, "Attempted to run function before initialised.");

    window->type = WINDOW_TYPE_EMPTY;
    window->visible = 1;
    window->data = NULL;
    window->x_pos = (int)window_defaults.x_pos;
    window->y_pos = (int)window_defaults.y_pos;
    window->width = window_defaults.width;
    window->height = window_defaults.height;
    window->title_width = window_defaults.title_width;
    window->border_width = window_defaults.border_width;
    glm_vec4_copy(window_defaults.background_colour, window->background_colour);
    glm_vec4_copy(window_defaults.border_colour, window->border_colour);
    glm_vec4_copy(window_defaults.title_colour, window->title_colour);

    // create the window title char buffer and the corresponding text mesh.
    window->title = NULL;
    tekChainThrow(tekGuiSetWindowTitleBuffer(window, window_defaults.title));
    tekChainThrow(tekGuiWindowCreateTitleText(window));

    tekChainThrow(tekGuiCreateButton(&window->title_button));
    tekGuiWindowUpdateButtonHitbox(window);
    window->title_button.hitbox_width = window_defaults.width + 2 * window_defaults.border_width;
    window->title_button.hitbox_height = window_defaults.title_width;
    window->title_button.data = (void*)window;
    window->title_button.callback = tekGuiWindowTitleButtonCallback;

    tekChainThrow(tekGuiWindowAddGLMesh(window, &window->mesh_index));

    window->being_dragged = 0;
    window->x_delta = 0;
    window->y_delta = 0;

    tekChainThrow(listAddItem(&window_ptr_list, window));

    window->draw_callback = NULL;

    return SUCCESS;
}

exception tekGuiDrawWindow(const TekGuiWindow* window) {
    if (!window->visible) return SUCCESS;

    // draw background box
    tekChainThrow(tekGuiDrawBox(window->mesh_index, window->background_colour, window->border_colour));

    // draw title
    const float x = (float)(window->x_pos + (int)(window->width / 2)) - window->title_text.width / 2.0f;
    const float y = (float)(window->y_pos - (int)window->title_width);
    tekChainThrow(tekDrawColouredText(&window->title_text, x, y, window->title_colour));

    // callback for additional drawing the window would want to do
    if (window->draw_callback)
        tekChainThrow(window->draw_callback(window));

    return SUCCESS;
}

exception tekGuiDrawAllWindows() {
    const ListItem* item = 0;
    foreach(item, (&window_ptr_list), {
        const TekGuiWindow* window = (const TekGuiWindow*)item->data;
        tekChainThrow(tekGuiDrawWindow(window));
    });
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
