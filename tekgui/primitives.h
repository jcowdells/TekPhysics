#pragma once

#include "../core/exception.h"
#include "../tekgl/mesh.h"
#include <cglm/cglm.h>

typedef struct TekGuiLine {
    TekMesh mesh;
    vec4 color;
} TekGuiLine;

typedef struct TekGuiOval {
    TekMesh mesh;
    float inv_width;
    float inv_height;
    float min_dist;
    vec2 center;
    vec4 color;
} TekGuiOval;

exception tekInitPrimitives(); // TODO: delete this, i want to make material.h in /tekgl to do this more abstractly
exception tekCreateLine(vec2 point_a, vec2 point_b, float thickness, vec4 color, TekGuiLine* line);
exception tekDrawLine(const TekGuiLine* line);
void tekDeleteLine(const TekGuiLine* line);

exception tekCreateOval(vec2 point_a, vec2 point_b, float thickness, flag fill, vec4 color, TekGuiOval* oval);
exception tekDrawOval(const TekGuiOval* oval);
void tekDeleteOval(const TekGuiOval* oval);