#pragma once

#include "../tekgl.h"
#include "../core/exception.h"
#include <cglm/vec4.h>

#define WINDOW_TYPE_EMPTY 0

typedef struct TekGuiWindow {
    flag type;
    void* data;
    uint x_pos;
    uint y_pos;
    uint width;
    uint height;
    vec4 background_colour;
} TekGuiWindow;

typedef struct TekGuiWindowData {
    vec2 position;
    vec2 size;
    vec4 colour;
} TekGuiWindowData;

exception tekGuiCreateWindow(TekGuiWindow* window);
void tekGuiDeleteWindow(TekGuiWindow* window);

void tekGuiGetWindowData(const TekGuiWindow* window, TekGuiWindowData* window_data);