#pragma once

#include "button.h"
#include "../core/vector.h"
#include <cglm/vec4.h>

typedef struct TekGuiTextInput {
    TekGuiButton button;
    Vector text_data;
    vec4 background_colour;
    vec4 text_colour;
} TekGuiTextInput;

exception tekGuiCreateTextInput(TekGuiTextInput* input);
void tekGuiDeleteTextInput(const TekGuiTextInput* input);
