#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

typedef void(*TekFramebufferCallback)(int framebuffer_width, int framebuffer_height);
typedef void(*TekDeleteFunc)(void);

exception tekInit(const char* window_name, int window_width, int window_height);
flag tekRunning();
exception tekUpdate();
exception tekDelete();

void tekGetWindowSize(int* window_width, int* window_height);
exception tekAddFramebufferCallback(TekFramebufferCallback callback);

exception tekRegisterDeleteFunc(TekDeleteFunc delete_func);

#define tek_init __attribute__((constructor)) void