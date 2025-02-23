#include "manager.h"

#include <stdlib.h>

#include "text.h"
#include "../tekgui/primitives.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

typedef struct TekFbCallbackNode {
    TekFramebufferCallback callback;
    struct TekFbCallbackNode* next;
} TekFbCallbackNode;

GLFWwindow* tek_window = 0;
int tek_window_width   = 0;
int tek_window_height  = 0;
TekFbCallbackNode* tek_framebuffer_callbacks = 0;

exception tekAddFramebufferCallback(const TekFramebufferCallback callback) {
    // allocate memory for a new callback item in list
    TekFbCallbackNode* node = (TekFbCallbackNode*)malloc(sizeof(TekFbCallbackNode));
    if (!node) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for framebuffer node.")

    // set callback and clear pointer to next item (there is no next item)
    node->callback = callback;
    node->next = 0;

    // if callbacks list doesn't exist, make this the first item
    // else, search the list to find the last item, and place it there
    if (!tek_framebuffer_callbacks) {
        tek_framebuffer_callbacks = node;
    } else {
        TekFbCallbackNode* search_node = tek_framebuffer_callbacks;
        while (search_node->next) search_node = search_node->next;
        search_node->next = node;
    }
    return SUCCESS;
}

void tekFreeFramebufferCallbacks() {
    // loop through each node of the list, freeing as we go
    TekFbCallbackNode* node = tek_framebuffer_callbacks;
    while (node) {
        // must make a copy of node, as once freed the data stored at that pointer may have changed
        TekFbCallbackNode* free_node = node;
        node = node->next;
        free(free_node);
    }
}

void tekManagerFramebufferCallback(GLFWwindow* window, const int width, const int height) {
    // update the size of the viewport
    glViewport(0, 0, width, height);

    // push the callback to all the nodes of the list
    TekFbCallbackNode* node = tek_framebuffer_callbacks;
    while (node) {
        node->callback(width, height);
        node = node->next;
    }
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
    glClear(GL_COLOR_BUFFER_BIT);

    return SUCCESS;
}

exception tekDelete() {
    // clean up memory allocate by callback functions
    tekFreeFramebufferCallbacks();

    // clean up TekGL things we initialised
    tekDeleteTextEngine();

    // destroy the glfw window and context
    glfwDestroyWindow(tek_window);
    glfwTerminate();
    return SUCCESS;
}

void tekGetWindowSize(int* window_width, int* window_height) {
    *window_width = tek_window_width;
    *window_height = tek_window_height;
}