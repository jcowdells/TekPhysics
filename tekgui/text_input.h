#pragma once

#include "button.h"
#include "../core/vector.h"
#include <cglm/vec4.h>

struct TekGuiTextInput;

typedef void(*TekGuiTextInputCallback)(struct TekGuiTextInput* text_input, const char* text, uint len_text);

typedef struct TekGuiTextInput {
    TekGuiButton button;
    Vector text_data;
    vec4 background_colour;
    vec4 border_colour;
    vec4 text_colour;
    uint border_width;
    uint mesh_index;
    TekGuiTextInputCallback callback;
    void* data;
} TekGuiTextInput;

exception tekGuiCreateTextInput(TekGuiTextInput* text_input);
exception tekGuiDrawTextInput(const TekGuiTextInput* text_input);
void tekGuiDeleteTextInput(TekGuiTextInput* text_input);
