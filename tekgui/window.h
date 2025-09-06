#pragma once

#include "../tekgl.h"
#include "../core/exception.h"
#include <cglm/vec4.h>
#include "../tekgl/mesh.h"

#define WINDOW_TYPE_EMPTY 0

typedef struct TekGuiWindow {
    flag type;
    void* data;
    uint x_pos;
    uint y_pos;
    uint width;
    uint height;
    uint title_width;
    uint border_width;
    vec4 background_colour;
    vec4 border_colour;
    uint mesh_index;
} TekGuiWindow;

typedef struct TekGuiWindowData {
    vec2 minmax_x;
    vec2 minmax_y;
    vec2 minmax_ix;
    vec2 minmax_iy;
    vec4 background_colour;
    vec4 border_colour;
} TekGuiWindowData;

exception tekGuiCreateWindow(TekGuiWindow* window);
void tekGuiDeleteWindow(TekGuiWindow* window);

void tekGuiGetWindowData(const TekGuiWindow* window, TekGuiWindowData* window_data);
exception tekGuiDrawWindow(const TekGuiWindow* window);