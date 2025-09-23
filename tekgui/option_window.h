#pragma once

#include "window.h"
#include "../core/hashtable.h"

typedef struct TekGuiOptionData {
    flag type;
    union {
        char* string;
        double number;
        flag boolean;
    } data;
} TekGuiOptionData;

typedef struct TekGuiOption {
    flag type;
    uint height;
    TekText label;
    TekGuiOptionData data;
} TekGuiOption;

typedef struct TekGuiOptionWindow {
    TekGuiWindow window;
    HashTable options;
} TekGuiOptionWindow;

exception tekGuiCreateOptionWindow(const char* options_yml, TekGuiOptionWindow* window);

