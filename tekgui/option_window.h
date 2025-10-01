#pragma once

#include "window.h"
#include "../core/hashtable.h"
#include "../core/vector.h"
#include "../tekgl/text.h"
#include "text_input.h"

typedef struct TekGuiOptionData {
    flag type;
    union {
        char* string;
        double number;
        flag boolean;
        vec3 vector_3;
        vec4 vector_4;
    } data;
} TekGuiOptionData;

typedef struct TekGuiOption {
    flag type;
    uint height;
    union {
        TekText label;
        struct {
            TekGuiTextInput text_input;
            const char* name;
            uint index;
        } input;
    } display;
} TekGuiOption;

typedef struct TekGuiOptionWindow {
    TekGuiWindow window;
    Vector option_display;
    HashTable option_data;
} TekGuiOptionWindow;

exception tekGuiCreateOptionWindow(const char* options_yml, TekGuiOptionWindow* window);
