#pragma once

#include "../core/exception.h"
#include "../tekgl/mesh.h"
#include <cglm/cglm.h>

typedef struct TekGuiLine {
    TekMesh mesh;
    vec4 color;
} TekGuiLine;

exception tekInitPrimitives(); // TODO: delete this, i want to make material.h in /tekgl to do this more abstractly
exception tekCreateLine(vec2 point_a, vec2 point_b, float thickness, vec4 color, TekGuiLine* line);
exception tekDrawLine(const TekGuiLine* line);
void tekDeleteLine(const TekGuiLine* line);