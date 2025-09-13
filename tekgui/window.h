#pragma once

#include "../tekgl.h"
#include "../core/exception.h"
#include <cglm/vec4.h>
#include "../tekgl/mesh.h"
#include "button.h"

#define WINDOW_TYPE_EMPTY 0

typedef struct TekGuiWindow {
    flag type;
    void* data;
    int x_pos;
    int y_pos;
    uint width;
    uint height;
    uint title_width;
    uint border_width;
    vec4 background_colour;
    vec4 border_colour;
    uint mesh_index;
    char* title;
    TekGuiButton title_button;
    flag being_dragged;
    int x_delta;
    int y_delta;
} TekGuiWindow;

typedef struct TekGuiWindowData {
    vec2 minmax_x;
    vec2 minmax_y;
    vec2 minmax_ix;
    vec2 minmax_iy;
    vec4 background_colour;
    vec4 border_colour;
} TekGuiWindowData;

struct TekGuiWindowDefaults {
    uint x_pos;
    uint y_pos;
    uint width;
    uint height;
    uint title_width;
    uint border_width;
    vec4 background_colour;
    vec4 border_colour;
};

exception tekGuiCreateWindow(TekGuiWindow* window);
void tekGuiSetWindowPosition(TekGuiWindow* window, int x_pos, int y_pos);
void tekGuiSetWindowSize(TekGuiWindow* window, uint width, uint height);
void tekGuiSetWindowBackgroundColour(TekGuiWindow* window, vec4 colour);
void tekGuiSetWindowBorderColour(TekGuiWindow* window, vec4 colour);
void tekGuiDeleteWindow(TekGuiWindow* window);
exception tekGuiDrawWindow(const TekGuiWindow* window);
exception tekGuiDrawAllWindows();