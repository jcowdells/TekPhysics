#pragma once

#include <cglm/vec4.h>
#include "../tekgl.h"
#include "../core/exception.h"

struct TekGuiWindowDefaults {
    uint x_pos;
    uint y_pos;
    uint width;
    uint height;
    vec4 colour;
};

#define tekGuiLog(...) printf("[INFO] TekGui: "); printf(__VA_ARGS__)

exception tekGuiGetWindowDefaults(struct TekGuiWindowDefaults* defaults);