#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

#include <cglm/vec2.h>

typedef struct TekGuiBoxData {
    vec2 minmax_x;
    vec2 minmax_y;
    vec2 minmax_ix;
    vec2 minmax_iy;
} TekGuiBoxData;

exception tekGuiCreateBox(const TekGuiBoxData* box_data, uint* index);
exception tekGuiDrawBox(uint index, const vec4 background_colour, const vec4 border_colour);
exception tekGuiUpdateBox(const TekGuiBoxData* box_data, uint index);