#pragma once

#include "window.h"
#include "../core/list.h"
#include "../core/hashtable.h"
#include <cglm/vec4.h>

struct TekGuiListWindow;

typedef void (*TekGuiListWindowCallback)(struct TekGuiListWindow* window);

typedef struct TekGuiListWindow {
    TekGuiWindow window;
    List* text_list;
    HashTable text_lookup;
    uint text_size;
    vec4 text_colour;
    uint num_visible;
    int draw_index;
    int hover_index;
    int select_index;
    TekGuiButton button;
    void* data;
    TekGuiListWindowCallback callback;
} TekGuiListWindow;

exception tekGuiCreateListWindow(TekGuiListWindow* window, List* text_list);
void tekGuiDeleteListWindow(TekGuiListWindow* window);