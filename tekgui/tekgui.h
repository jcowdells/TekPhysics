#pragma once

#include <cglm/vec4.h>
#include "../core/exception.h"

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

struct TekGuiTextButtonDefaults {
    uint x_pos;
    uint y_pos;
    uint width;
    uint height;
    uint border_width;
    uint text_height;
    vec4 background_colour;
    vec4 selected_colour;
    vec4 border_colour;
};

#define tekGuiLog(...) printf("[INFO] TekGui: "); printf(__VA_ARGS__)

exception tekGuiGetWindowDefaults(struct TekGuiWindowDefaults* defaults);
exception tekGuiGetTextButtonDefaults(struct TekGuiTextButtonDefaults* defaults);