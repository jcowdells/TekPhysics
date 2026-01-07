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

/**
 * Return the largest of two numbers
 * @param a First number
 * @param b Second number
 * @return The largest number
 */
#define MAX(a, b) (a > b) ? a : b;

static List window_ptr_list;

static flag window_init = NOT_INITIALISED;
static flag window_gl_init = NOT_INITIALISED;

static struct TekGuiWindowDefaults window_defaults;
static GLFWcursor* move_cursor;

/**
 * Delete callback for window, freeing any supporting data structures.
 */
static void tekGuiWindowDelete() {
    // delete window list
    listDelete(&window_ptr_list);

    window_init = DE_INITIALISED;
    window_gl_init = DE_INITIALISED;
}

/**
 * Callback for when opengl loads. Loads window defaults and creates the cursor for when window is moving.
 * @throws YML_EXCEPTION if could not load defaults
 */
static exception tekGuiWindowGLLoad() {
    // load defaults
    tekChainThrow(tekGuiGetWindowDefaults(&window_defaults));

    // create the cursor
    move_cursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);

    window_gl_init = INITIALISED;
    return SUCCESS;
}

/**
 * Initialise some supporting data structures for the window code. Loads some callbacks.
 */
tek_init tekGuiWindowInit() {
    // load callbacks
    exception tek_exception = tekAddDeleteFunc(tekGuiWindowDelete);
    if (tek_exception) return;

    tek_exception = tekAddGLLoadFunc(tekGuiWindowGLLoad);
    if (tek_exception) return;

    // create window list
    listCreate(&window_ptr_list);

    window_init = INITIALISED;
}

/**
 * Get the data for a window box.
 * @param window The window to get the box data for.
 * @param box_data The outputted box data.
 */
static void tekGuiGetWindowData(const TekGuiWindow* window, TekGuiBoxData* box_data) {
    // minmax = minimum and maximum extent in that axis.
    // minmax_i = internal extents, excludes border.
    glm_vec2_copy((vec2){(float)window->x_pos - (float)window->border_width, (float)(window->width + window->x_pos + window->border_width)}, box_data->minmax_x);
    glm_vec2_copy((vec2){(float)window->y_pos - (float)window->title_width, (float)(window->height + window->y_pos + window->border_width)}, box_data->minmax_y);
    glm_vec2_copy((vec2){(float)window->x_pos, (float)(window->x_pos + window->width)}, box_data->minmax_ix);
    glm_vec2_copy((vec2){(float)window->y_pos, (float)(window->y_pos + window->height)}, box_data->minmax_iy);
}

/**
 * Create a box mesh for the window and add it to the list of boxes (all stored in one buffer)
 * @param window The window to create the box for.
 * @param index The outputted index of the box in the buffer of boxes.
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiWindowAddGLMesh(const TekGuiWindow* window, uint* index) {
    // get box data and create box
    TekGuiBoxData box_data = {};
    tekGuiGetWindowData(window, &box_data);
    tekChainThrow(tekGuiCreateBox(&box_data, index));
    return SUCCESS;
}

/**
 * Update the mesh of the background area of the window.
 * @param window The window to update.
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiWindowUpdateGLMesh(const TekGuiWindow* window) {
    // get box data and update the box.
    TekGuiBoxData box_data = {};
    tekGuiGetWindowData(window, &box_data);
    tekChainThrow(tekGuiUpdateBox(&box_data, window->mesh_index));
    return SUCCESS;
}

/**
 * Bring a window to the front so it is rendered above other windows.
 * @param window The window to bring to the front.
 * @throws LIST_EXCEPTION if could not move window in the window list.
 * @throws EXCEPTION whatever the select callback of the window threw.
 */
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

/**
 * Update the position of the title button hitbox according to the window position.
 * @param window The window to update the title button of.
 */
static void tekGuiWindowUpdateButtonHitbox(TekGuiWindow* window) {
    // dont think i need to cast to uint any more cuz i fixed the buttons but oh well
    window->title_button.hitbox_x = (uint)MAX(0, window->x_pos - (int)window_defaults.border_width);
    window->title_button.hitbox_y = (uint)MAX(0, window->y_pos - (int)window_defaults.title_width);
}

/**
 * Create the visual title text of a window.
 * @param window The window to create the title text of.
 * @throws FREETYPE_EXCEPTION .
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiWindowCreateTitleText(TekGuiWindow* window) {
    // hard coded ratio that looks nice
    const uint text_size = window->title_width * 4 / 5;

    // get font and create text
    TekBitmapFont* font;
    tekChainThrow(tekGuiGetDefaultFont(&font));
    tekChainThrow(tekCreateText(window->title, text_size, font, &window->title_text));
    return SUCCESS;
}

/**
 * Recreate the title text visually of the window.
 * @param window The window to recreate the title text of.
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiWindowRecreateTitleText(TekGuiWindow* window) {
    // hard coded ratio that looks nice
    const uint text_size = window->title_width * 4 / 5;
    tekChainThrow(tekUpdateText(&window->title_text, window->title, text_size));
    return SUCCESS;
}

/**
 * Callback for whenever the title area of a window is clicked. Handles dragging the window.
 * @param button_ptr Pointer to the button to register clicks to the title area.
 * @param callback_data The callback data such as which button was pressed.
 */
static void tekGuiWindowTitleButtonCallback(TekGuiButton* button_ptr, TekGuiButtonCallbackData callback_data) {
    // get the window from button data.
    TekGuiWindow* window = (TekGuiWindow*)button_ptr->data;
    switch (callback_data.type) {
    case TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK: // on clicks
        // if not a left click, ignore it.
        if (callback_data.data.mouse_button.button != GLFW_MOUSE_BUTTON_LEFT) break;

        // on press, begin dragging the window
        if (callback_data.data.mouse_button.action == GLFW_PRESS) {
            window->being_dragged = 1;
            // need a delta, cuz the mouse aint gonna be at the top left of the window always.
            window->x_delta = window->x_pos - (int)callback_data.mouse_x;
            window->y_delta = window->y_pos - (int)callback_data.mouse_y;
            tekSetCursor(CROSSHAIR_CURSOR); // make cursor look like its moving the window
            tekGuiBringWindowToFront(window);
        // on release, stop dragging the window
        } else if (callback_data.data.mouse_button.action == GLFW_RELEASE) {
            window->being_dragged = 0;
            tekSetCursor(DEFAULT_CURSOR); // reset the cursor
        }
        break;
    case TEK_GUI_BUTTON_MOUSE_LEAVE_CALLBACK: // if mouse goes out of the border, reset
        // kinda a bodge, but this is the best solution cuz my buttons dont register events outside of their region
        window->being_dragged = 0;
        tekSetCursor(DEFAULT_CURSOR);
        break;
    case TEK_GUI_BUTTON_MOUSE_TOUCHING_CALLBACK: // while mouse is moving
        if (window->being_dragged) { // if held while moving, move the window to the mouse
            window->being_dragged = 1;
            window->x_pos = (int)callback_data.mouse_x + window->x_delta;
            window->y_pos = (int)callback_data.mouse_y + window->y_delta;
            // moving window = needs to be redrawn
            tekGuiWindowUpdateButtonHitbox(window);
            tekGuiWindowUpdateGLMesh(window);
        }
        break;
    default:
        break;
    }
}

/**
 * Update the string buffer that contains the window title, this will allocate or reallocate a buffer as needed, and copy in the title string.
 * @param window The window to update the title buffer of.
 * @param title The new title to put in the buffer.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekGuiSetWindowTitleBuffer(TekGuiWindow* window, const char* title) {
    // free title if exists
    if (window->title)
        free(window->title);

    // mallocate buffer
    const uint len_title = strlen(title) + 1;
    window->title = (char*)malloc(len_title * sizeof(char));
    if (!window->title)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for title.");

    // copy title into buffer
    memcpy(window->title, title, len_title);
    return SUCCESS;
}

/**
 * Set the title of a window in the gui (not the main window). Title is copied, changing original string does not affect the title.
 * @param window The window to set the title of. 
 * @param title The new title of the window.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekGuiSetWindowTitle(TekGuiWindow* window, const char* title) {
    // copy title into buffer and redraw title
    tekChainThrow(tekGuiSetWindowTitleBuffer(window, title));
    tekChainThrow(tekGuiWindowRecreateTitleText(window));
    return SUCCESS;
}

/**
 * Create a new window and add it to the list of all windows.
 * @param window The new outputted window.
 * @throws FAILURE if opengl not initialised.
 * @throws OPENGL_EXCEPTION . 
 */
exception tekGuiCreateWindow(TekGuiWindow* window) {
    // check if opengl initialised + window intitialised
    if (!window_init) tekThrow(FAILURE, "Attempted to run function before initialised.");

    // assign all default values from defaults.yml
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

    // create button to detect when title bar clicked / dragged
    tekChainThrow(tekGuiCreateButton(&window->title_button));
    tekGuiWindowUpdateButtonHitbox(window);
    window->title_button.hitbox_width = window_defaults.width + 2 * window_defaults.border_width;
    window->title_button.hitbox_height = window_defaults.title_width;
    window->title_button.data = (void*)window;
    window->title_button.callback = tekGuiWindowTitleButtonCallback;

    // add background mesh to list
    tekChainThrow(tekGuiWindowAddGLMesh(window, &window->mesh_index));

    window->being_dragged = 0;
    window->x_delta = 0;
    window->y_delta = 0;

    tekChainThrow(listAddItem(&window_ptr_list, window));

    window->draw_callback = NULL;

    return SUCCESS;
}

/**
 * Draw a single window, and call any sub draw calls of the window.
 * @param window The window to draw.
 * @throws OPENGL_EXCEPTION .
 */
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

/**
 * Draw all the windows that have been created.
 * @throws OPENGL_EXCEPTION .
 */
exception tekGuiDrawAllWindows() {
    // loop through windows and draw each one
    const ListItem* item = 0;
    foreach(item, (&window_ptr_list), {
        const TekGuiWindow* window = (const TekGuiWindow*)item->data;
        tekChainThrow(tekGuiDrawWindow(window));
    });
    return SUCCESS;
}

/**
 * Set the position of a window in pixels.
 * @param window The window to set the position of.
 * @param x_pos The new x position of the window.
 * @param y_pos The new y position of the window.
 */
void tekGuiSetWindowPosition(TekGuiWindow* window, const int x_pos, const int y_pos) {
    // update position and redraw mesh
    window->x_pos = x_pos;
    window->y_pos = y_pos;
    tekGuiWindowUpdateButtonHitbox(window);
    tekGuiWindowUpdateGLMesh(window);
}

/**
 * Set the size of a window in pixels.
 * @param window The window to set the size of.
 * @param width The new width in pixels.
 * @param height The new height in pixels.
 */
void tekGuiSetWindowSize(TekGuiWindow* window, const uint width, const uint height) {
    // update size and redraw mesh and update button
    window->width = width;
    window->height = height;
    tekGuiWindowUpdateButtonHitbox(window);
    tekGuiWindowUpdateGLMesh(window);
}

/**
 * Set the background colour of a window.
 * @param window The window to set the background colour of.
 * @param colour The colour to set the background as.
 */
void tekGuiSetWindowBackgroundColour(TekGuiWindow* window, vec4 colour) {
    // set colour and update mesh
    glm_vec4_copy(colour, window->background_colour);
    tekGuiWindowUpdateGLMesh(window);
}

/**
 * Set the colour of the border of a window.
 * @param window The window to set the border colour of.
 * @param colour The colour to set the window border.
 */
void tekGuiSetWindowBorderColour(TekGuiWindow* window, vec4 colour) {
    // set colour and update mesh.
    glm_vec4_copy(colour, window->border_colour);
    tekGuiWindowUpdateGLMesh(window);
}

/**
 * Delete a window, freeing any allocated memory.
 * @param window The window to delete.
 */
void tekGuiDeleteWindow(TekGuiWindow* window) {
    // free everything we allocated
    tekGuiDeleteButton(&window->title_button);
    tekDeleteText(&window->title_text);
    free(window->title);
}
