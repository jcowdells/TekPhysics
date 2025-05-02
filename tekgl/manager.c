#include "manager.h"

#include <stdlib.h>

#include "entity.h"
#include "text.h"
#include "../tekgui/primitives.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "../core/list.h"

GLFWwindow* tek_window = 0;
int tek_window_width   = 0;
int tek_window_height  = 0;

List tek_fb_funcs = {0, 0};
List tek_delete_funcs = {0, 0};
List tek_key_funcs = {0, 0};

exception tekAddFramebufferCallback(const TekFramebufferCallback callback) {
    if (tek_fb_funcs.length == 0) listCreate(&tek_fb_funcs);
    tekChainThrow(listAddItem(&tek_fb_funcs, callback));
    return SUCCESS;
}

void tekManagerFramebufferCallback(const GLFWwindow* window, const int width, const int height) {
    if (window != tek_window) return;

    // update the size of the viewport
    glViewport(0, 0, width, height);

    const ListItem* item = 0;
    foreach(item, (&tek_fb_funcs), {
        const TekFramebufferCallback callback = (TekFramebufferCallback)item->data;
        callback(width, height);
    });
}

exception tekAddDeleteFunc(const TekDeleteFunc delete_func) {
    if (tek_delete_funcs.length == 0) listCreate(&tek_delete_funcs);

    // add delete func to a list that we can iterate over on cleanup
    tekChainThrow(listAddItem(&tek_delete_funcs, delete_func));
    return SUCCESS;
}

void tekManagerKeyCallback(const GLFWwindow* window, const int key, const int scancode, const int action, const int mods) {
    if (window != tek_window) return;
    const ListItem* item = 0;
    foreach(item, (&tek_key_funcs), {
        const TekKeyCallback callback = (TekKeyCallback)item->data;
        callback(key, scancode, action, mods);
    });
}

exception tekAddKeyCallback(const TekKeyCallback callback) {
    if (tek_key_funcs.length == 0) listCreate(&tek_key_funcs);

    tekChainThrow(listAddItem(&tek_key_funcs, callback));
    return SUCCESS;
}

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

    // init different areas of tekgl
    tekChainThrow(tekInitTextEngine());
    //tekGuiInitSizeManager();
    tekChainThrow(tekInitPrimitives());

    return SUCCESS;
}

flag tekRunning() {
    // return whether tekgl should continue running
    return glfwWindowShouldClose(tek_window) ? 0 : 1;
}

exception tekUpdate() {
    // swap buffers - clears front buffer and displays back buffer to screen
    glfwSwapBuffers(tek_window);

    // push any events that have occurred - key presses, close window etc.
    glfwPollEvents();

    // clear framebuffer for next draw call
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    return SUCCESS;
}

exception tekDelete() {
    // clean up memory allocate by callback functions
    listDelete(&tek_fb_funcs);

    // clean up TekGL things we initialised
    tekDeleteTextEngine();

    // run all cleanup functions
    const ListItem* item = 0;
    foreach(item, (&tek_delete_funcs), {
        const TekDeleteFunc delete_func = (TekDeleteFunc)item->data;
        delete_func();
    });
    listDelete(&tek_delete_funcs);

    listDelete(&tek_key_funcs);

    // destroy the glfw window and context
    glfwDestroyWindow(tek_window);
    glfwTerminate();
    return SUCCESS;
}

void tekGetWindowSize(int* window_width, int* window_height) {
    *window_width = tek_window_width;
    *window_height = tek_window_height;
}