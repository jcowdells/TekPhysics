#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

#define tek_init __attribute__((constructor)) static void

#define MOUSE_MODE_NORMAL 0
#define MOUSE_MODE_CAMERA 1

#define DEFAULT_CURSOR   0
#define CROSSHAIR_CURSOR 1

typedef void(*TekFramebufferCallback)(int framebuffer_width, int framebuffer_height);
typedef void(*TekDeleteFunc)(void);
typedef exception(*TekGLLoadFunc)(void);
typedef void(*TekKeyCallback)(int key, int scancode, int action, int mods);
typedef void(*TekMousePosCallback)(double x, double y);
typedef void(*TekMouseButtonCallback)(int button, int action, int mods);

exception tekInit(const char* window_name, int window_width, int window_height);
flag tekRunning();
exception tekUpdate();
void tekDelete();

void tekGetWindowSize(int* window_width, int* window_height);

void tekSetMouseMode(flag mouse_mode);
void tekSetCursor(flag cursor_mode);

exception tekAddFramebufferCallback(TekFramebufferCallback callback);
exception tekAddKeyCallback(TekKeyCallback callback);
exception tekAddDeleteFunc(TekDeleteFunc delete_func);
exception tekAddGLLoadFunc(TekGLLoadFunc gl_load_func);
exception tekAddMousePosCallback(TekMousePosCallback callback);
exception tekAddMouseButtonCallback(TekMouseButtonCallback callback);
