#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

typedef void(*TekFramebufferCallback)(int framebuffer_width, int framebuffer_height);

void tekSetMousePositionCallback(void* callback);
void tekSetMouseButtonCallback(void* callback);

exception tekInit(const char* window_name, int window_width, int window_height);
flag tekRunning();
exception tekUpdate();
exception tekDelete();

void tekGetWindowSize(int* window_width, int* window_height);
exception tekAddFramebufferCallback(TekFramebufferCallback callback);