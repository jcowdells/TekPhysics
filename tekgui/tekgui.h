#pragma once

#include <cglm/vec4.h>
#include "../core/exception.h"
#include "../tekgl/font.h"

struct TekGuiWindowDefaults {
    uint x_pos;
    uint y_pos;
    uint width;
    uint height;
    uint title_width;
    uint border_width;
    vec4 background_colour;
    vec4 border_colour;
    vec4 title_colour;
    char* title;
};

struct TekGuiListWindowDefaults {
    uint text_size;
    vec4 text_colour;
    uint num_visible;
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

struct TekGuiTextInputDefaults {
    uint x_pos;
    uint y_pos;
    uint width;
    uint text_height;
    uint border_width;
    vec4 background_colour;
    vec4 border_colour;
    vec4 text_colour;
};

/**
 * Mostly unused logging function. Just prints [INFO] TekGui: before the message
 */
#define tekGuiLog(...) printf("[INFO] TekGui: "); printf(__VA_ARGS__)

exception tekGuiGetWindowDefaults(struct TekGuiWindowDefaults* defaults);
exception tekGuiGetListWindowDefaults(struct TekGuiListWindowDefaults* defaults);
exception tekGuiGetTextButtonDefaults(struct TekGuiTextButtonDefaults* defaults);
exception tekGuiGetTextInputDefaults(struct TekGuiTextInputDefaults* defaults);
exception tekGuiGetDefaultFont(TekBitmapFont** font);
