#pragma once

#include "window.h"
#include "../core/list.h"
#include "../core/hashtable.h"
#include <cglm/vec4.h>

typedef struct TekGuiListWindow {
    TekGuiWindow window;
    List* text_list;
    HashTable text_lookup;
    uint text_size;
    vec4 text_colour;
    uint num_visible;
} TekGuiListWindow;

exception tekGuiCreateListWindow(TekGuiListWindow* window, List* text_list);
void tekGuiDeleteListWindow(TekGuiListWindow* window);