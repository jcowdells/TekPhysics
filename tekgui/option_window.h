#pragma once

#include "text_button.h"
#include "window.h"
#include "../core/hashtable.h"
#include "../core/vector.h"
#include "../tekgl/text.h"
#include "text_input.h"

#define TEK_UNKNOWN_INPUT  (-1)
#define TEK_LABEL          0
#define TEK_STRING_INPUT   1
#define TEK_NUMBER_INPUT   2
#define TEK_BOOLEAN_INPUT  3
#define TEK_VEC3_INPUT     4
#define TEK_VEC4_INPUT     5
#define TEK_BUTTON_INPUT   6

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
    void* data;
    TekGuiOptionWindowCallback callback;
} TekGuiOptionWindow;

exception tekGuiCreateOptionWindow(const char* options_yml, TekGuiOptionWindow* window);
exception tekGuiWriteStringOption(TekGuiOptionWindow* window, const char* key, const char* string, uint len_string);
exception tekGuiWriteNumberOption(TekGuiOptionWindow* window, const char* key, double number);
exception tekGuiWriteBooleanOption(TekGuiOptionWindow* window, const char* key, flag boolean);
exception tekGuiWriteVec3Option(TekGuiOptionWindow* window, const char* key, vec3 vector);
exception tekGuiWriteVec4Option(TekGuiOptionWindow* window, const char* key, vec4 vector);
exception tekGuiReadStringOption(TekGuiOptionWindow* window, const char* key, char** string);
exception tekGuiReadNumberOption(TekGuiOptionWindow* window, const char* key, double* number);
exception tekGuiReadBooleanOption(TekGuiOptionWindow* window, const char* key, flag* boolean);
exception tekGuiReadVec3Option(TekGuiOptionWindow* window, const char* key, vec3 vector);
exception tekGuiReadVec4Option(TekGuiOptionWindow* window, const char* key, vec4 vector);
void tekGuiDeleteOptionWindow(const TekGuiOptionWindow* window);