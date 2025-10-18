#pragma once

#include "text_button.h"
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
        struct {
            TekGuiTextButton button;
            const char* name;
        } button;
    } display;
} TekGuiOption;

typedef struct TekGuiOptionWindowCallbackData {
    flag type;
    const char* name;
} TekGuiOptionWindowCallbackData;

struct TekGuiOptionWindow;
typedef exception (*TekGuiOptionWindowCallback)(struct TekGuiOptionWindow* window, TekGuiOptionWindowCallbackData callback_data);

typedef struct TekGuiOptionWindow {
    TekGuiWindow window;
    TekGuiOption* option_display;
    uint len_options;
    HashTable option_data;
    TekGuiOptionWindowCallback callback;
} TekGuiOptionWindow;

exception tekGuiCreateOptionWindow(const char* options_yml, TekGuiOptionWindow* window);
void tekGuiDeleteOptionWindow(const TekGuiOptionWindow* window);