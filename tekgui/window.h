#pragma once

#include "../tekgl.h"
#include "../core/exception.h"
#include <cglm/vec4.h>
#include "../tekgl/mesh.h"
#include "button.h"
#include "../tekgl/text.h"

#define WINDOW_TYPE_EMPTY 0

struct TekGuiWindow;

typedef exception(*TekGuiWindowDrawCallback)(struct TekGuiWindow* window);

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
    vec4 title_colour;
    uint mesh_index;
    char* title;
    TekText title_text;
    TekGuiButton title_button;
    flag being_dragged;
    int x_delta;
    int y_delta;
    TekGuiWindowDrawCallback draw_callback;
} TekGuiWindow;

exception tekGuiCreateWindow(TekGuiWindow* window);
void tekGuiSetWindowPosition(TekGuiWindow* window, int x_pos, int y_pos);
void tekGuiSetWindowSize(TekGuiWindow* window, uint width, uint height);
exception tekGuiSetWindowTitle(TekGuiWindow* window, const char* title);
void tekGuiSetWindowBackgroundColour(TekGuiWindow* window, vec4 colour);
void tekGuiSetWindowBorderColour(TekGuiWindow* window, vec4 colour);
void tekGuiDeleteWindow(TekGuiWindow* window);
exception tekGuiDrawWindow(const TekGuiWindow* window);
exception tekGuiDrawAllWindows();