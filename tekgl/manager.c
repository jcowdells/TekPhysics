#include "manager.h"

#include <stdlib.h>

#include "entity.h"
#include "text.h"
#include "../tekgui/primitives.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "../core/list.h"

static GLFWwindow* tek_window = 0;
static int tek_window_width   = 0;
static int tek_window_height  = 0;
static vec3 tek_window_colour = { 0.0f, 0.0f, 0.0f };

static List tek_fb_funcs = {0, 0};
static List tek_delete_funcs = {0, 0};
static List tek_gl_load_funcs = {0, 0};
static List tek_key_funcs = {0, 0};
static List tek_char_funcs = {0, 0};
static List tek_mmove_funcs = {0, 0};
static List tek_mbutton_funcs = {0, 0};
static List tek_mscroll_funcs = {0, 0};
static GLFWcursor* crosshair_cursor;

exception tekAddFramebufferCallback(const TekFramebufferCallback callback) {
    if (!tek_fb_funcs.length) {
        listCreate(&tek_fb_funcs);
    }

    tekChainThrow(listAddItem(&tek_fb_funcs, callback));
    return SUCCESS;
}

void tekManagerFramebufferCallback(GLFWwindow* window, const int width, const int height) {
    if (window != tek_window) return;

    tek_window_width = width;
    tek_window_height = height;

    // update the size of the viewport
    glViewport(0, 0, width, height);

    const ListItem* item = 0;
    foreach(item, (&tek_fb_funcs), {
        const TekFramebufferCallback callback = (TekFramebufferCallback)item->data;
        callback(width, height);
    });
}

exception tekAddDeleteFunc(const TekDeleteFunc delete_func) {
    if (!tek_delete_funcs.length)
        listCreate(&tek_delete_funcs);

    // add delete func to a list that we can iterate over on cleanup
    tekChainThrow(listAddItem(&tek_delete_funcs, delete_func));
    return SUCCESS;
}

exception tekAddGLLoadFunc(const TekGLLoadFunc gl_load_func) {
    if (!tek_gl_load_funcs.length)
        listCreate(&tek_gl_load_funcs);

    // add gl load func to a list that we can iterate over on cleanup
    tekChainThrow(listAddItem(&tek_gl_load_funcs, gl_load_func));
    return SUCCESS;
}

void tekManagerKeyCallback(GLFWwindow* window, const int key, const int scancode, const int action, const int mods) {
    if (window != tek_window) return;
    const ListItem* item = 0;
    foreach(item, (&tek_key_funcs), {
        const TekKeyCallback callback = (TekKeyCallback)item->data;
        callback(key, scancode, action, mods);
    });
}

static void tekManagerCharCallback(GLFWwindow* window, uint codepoint) {
    if (window != tek_window) return;
    const ListItem* item = 0;
    foreach(item, (&tek_char_funcs), {
        const TekCharCallback callback = (TekCharCallback)item->data;
        callback(codepoint);
    });
}

static void tekManagerMouseMoveCallback(GLFWwindow* window, const double x, const double y) {
    if (window != tek_window) return;
    const ListItem* item = 0;
    foreach(item, (&tek_mmove_funcs), {
        const TekMousePosCallback callback = (TekMousePosCallback)item->data;
        callback(x, y);
    });
}

static void tekManagerMouseButtonCallback(GLFWwindow* window, const int button, const int action, const int mods) {
    if (window != tek_window) return;

    const ListItem* item = 0;
    foreach(item, (&tek_mbutton_funcs), {
        const TekMouseButtonCallback callback = (TekMouseButtonCallback)item->data;
        callback(button, action, mods);
    });
}

static void tekManagerMouseScrollCallback(GLFWwindow* window, const double x_offset, const double y_offset) {
    if (window != tek_window) return;

    const ListItem* item = 0;
    foreach(item, (&tek_mscroll_funcs), {
        const TekMouseScrollCallback callback = (TekMouseScrollCallback)item->data;
        callback(x_offset, y_offset);
    });
}

exception tekAddKeyCallback(const TekKeyCallback callback) {
    if (!tek_key_funcs.length)
        listCreate(&tek_key_funcs);

    tekChainThrow(listAddItem(&tek_key_funcs, callback));
    return SUCCESS;
}

exception tekAddCharCallback(const TekCharCallback callback) {
    if (!tek_char_funcs.length)
       listCreate(&tek_char_funcs);

    tekChainThrow(listAddItem(&tek_char_funcs, callback));
    return SUCCESS;
}

exception tekAddMousePosCallback(const TekMousePosCallback callback) {
    if (!tek_mmove_funcs.length)
        listCreate(&tek_mmove_funcs);

    tekChainThrow(listAddItem(&tek_mmove_funcs, callback));
    return SUCCESS;
}

exception tekAddMouseButtonCallback(const TekMouseButtonCallback callback) {
    if (!tek_mbutton_funcs.length)
        listCreate(&tek_mbutton_funcs);

    tekChainThrow(listAddItem(&tek_mbutton_funcs, callback));
    return SUCCESS;
}

exception tekAddMouseScrollCallback(const TekMouseScrollCallback callback) {
    if (!tek_mscroll_funcs.length)
        listCreate(&tek_mscroll_funcs);

    tekChainThrow(listAddItem(&tek_mscroll_funcs, callback));
    return SUCCESS;
}

/**
 * Set the mode of the cursor. Should be one of two modes:
 * DEFAULT_CURSOR or CROSSHAIR_CURSOR, assumes default cursor if invalid cursor mode specified.
 * @param cursor_mode The cursor mode to use.
 */
void tekSetCursor(const flag cursor_mode) {
    switch (cursor_mode) {
    case CROSSHAIR_CURSOR: // if crosshair cursor, set the cursor to crosshair cursor
        glfwSetCursor(tek_window, crosshair_cursor);
        break;
    default: // NULL = default cursor.
        glfwSetCursor(tek_window, NULL);
    }
}

/**
 * Set the colour of the window background when nothing is drawn.
 * @param colour The colour to use.
 */
void tekSetWindowColour(vec3 colour) {
    glm_vec3_copy(colour, tek_window_colour); // copy into window colour variable.
}

/**
 * Set the draw mode of the window, should be one of:
 * DRAW_MODE_NORMAL - draw everything as expected.
 * DRAW_MODE_GUI - draw everything as gui.
 * This changes how depth is rendered, gui elements are all rendered at the same level.
 * Typical depth test will not work, so setting gui mode will force new things to appear above old things.
 * @param draw_mode 
 */
void tekSetDrawMode(const flag draw_mode) {
    if (draw_mode == DRAW_MODE_NORMAL) {
        glDepthFunc(GL_LESS); // obscure things which are behind
        return;
    }

    if (draw_mode == DRAW_MODE_GUI) {
        glDepthFunc(GL_ALWAYS); // always draw over things
    }
}

/**
 * Initialise the TekPhysics window.
 * @param window_name The title to be displayed for the window.
 * @param window_width The initial width of the window.
 * @param window_height The initial height of the window.
 * @throws GLFW_EXCEPTION if GLFW failed to initialise or create the window.
 * @throws GLAD_EXCEPTION if GLAD failed to load.
 */
exception tekInit(const char* window_name, const int window_width, const int window_height) {
    // initialise glfw
    if (!glfwInit()) tekThrow(GLFW_EXCEPTION, "GLFW failed to initialise.");

    // create a window with opengl and store some info about it
    tek_window = glfwCreateWindow(window_width, window_height, window_name, NULL, NULL);
    if (!tek_window) {
        glfwTerminate();
        tekThrow(GLFW_EXCEPTION, "GLFW failed to create window.");
    }

    // make this window as current context, so it will receive opengl draw calls
    glfwMakeContextCurrent(tek_window);

    // load glad, allows us to use opengl functions
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) tekThrow(GLAD_EXCEPTION, "GLAD failed to load loader.");

    // update framebuffer size and callbacks
    glfwGetFramebufferSize(tek_window, &tek_window_width, &tek_window_height);
    glfwSetFramebufferSizeCallback(tek_window, tekManagerFramebufferCallback);
    glfwSetKeyCallback(tek_window, tekManagerKeyCallback);
    glfwSetCharCallback(tek_window, tekManagerCharCallback);
    glfwSetCursorPosCallback(tek_window, tekManagerMouseMoveCallback);
    glfwSetMouseButtonCallback(tek_window, tekManagerMouseButtonCallback);
    glfwSetScrollCallback(tek_window, tekManagerMouseScrollCallback);

    crosshair_cursor = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);

    tekChainThrow(tekCreateFreeType());

    // run all the callbacks for when opengl loaded.
    const ListItem* item;
    foreach(item, (&tek_gl_load_funcs), {
        const TekGLLoadFunc gl_load_func = (TekGLLoadFunc)item->data;
        tekChainThrow(gl_load_func());
    });

    tekManagerFramebufferCallback(tek_window, window_width, window_height);

    // dont render stuff that is behind other objects.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // dont render stuff that is facing the wrong way
    glEnable(GL_CULL_FACE);

    // blend translucent colours nicely.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return SUCCESS;
}

/**
 * Return whether the window should remain open.
 * @return 1 if the window is still open, 0 if the X button has been pressed.
 */
flag tekRunning() {
    // return whether tekgl should continue running
    return glfwWindowShouldClose(tek_window) ? 0 : 1;
}

/**
 * Swap buffers and poll events. Should be called every frame / every loop of the main loop.
 * @throws NOTHING not sure why it's not a void function. 
 */
exception tekUpdate() {
    // swap buffers - clears front buffer and displays back buffer to screen
    glfwSwapBuffers(tek_window);

    // push any events that have occurred - key presses, close window etc.
    glfwPollEvents();

    // clear framebuffer for next draw call
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(tek_window_colour[0], tek_window_colour[1], tek_window_colour[2], 1.0f);

    return SUCCESS;
}

/**
 * Free memory allocated by supporting structures for the manager code, for example different callback function lists.
 */
void tekDelete() {
    // run all cleanup functions
    const ListItem* item = 0;
    foreach(item, (&tek_delete_funcs), {
        const TekDeleteFunc delete_func = (TekDeleteFunc)item->data;
        delete_func();
    });

    // clean up memory allocated by callback functions
    listDelete(&tek_fb_funcs);
    listDelete(&tek_delete_funcs);
    listDelete(&tek_gl_load_funcs);
    listDelete(&tek_key_funcs);
    listDelete(&tek_mmove_funcs);
    listDelete(&tek_mbutton_funcs);
    listDelete(&tek_mscroll_funcs);

    tekDeleteFreeType();

    glfwDestroyCursor(crosshair_cursor);

    // destroy the glfw window and context
    glfwDestroyWindow(tek_window);
    glfwTerminate();
}

/**
 * Get the current size of the window.
 * @param window_width The outputted width.
 * @param window_height The outputted height.
 */
void tekGetWindowSize(int* window_width, int* window_height) {
    // pointers :D
    *window_width = tek_window_width;
    *window_height = tek_window_height;
}

/**
 * Set the mouse mode of the window. Can be once of two types:
 * MOUSE_MODE_CAMERA - make the mouse disappear and unable to leave the screen.
 * MOUSE_MODE_NORMAL - make the mouse behave as expected.
 * If neither one is used, then normal mode is assumed.
 * @param mouse_mode The mouse mode as specified above
 */
void tekSetMouseMode(const flag mouse_mode) {
    // cannot change mouse mode unless window is focused
    glfwFocusWindow(tek_window);
    switch (mouse_mode) {
    case MOUSE_MODE_CAMERA:
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(tek_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE); // raw motion
        glfwSetInputMode(tek_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // hide cursor
        break;
    case MOUSE_MODE_NORMAL:
    default:
        glfwSetInputMode(tek_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE); // normal mouse movement
        glfwSetInputMode(tek_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // show cursor
    }
}