#pragma once

#include "../core/exception.h"
#include "../tekgl/mesh.h"
#include "../tekgl/texture.h"
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

typedef struct TekGuiImage {
    TekMesh mesh;
    uint texture_id;
} TekGuiImage;

exception tekGuiCreateLine(vec2 point_a, vec2 point_b, float thickness, vec4 color, TekGuiLine* line);
exception tekGuiDrawLine(const TekGuiLine* line);
void tekGuiDeleteLine(const TekGuiLine* line);

exception tekGuiCreateOval(vec2 point_a, vec2 point_b, float thickness, flag fill, vec4 color, TekGuiOval* oval);
exception tekGuiDrawOval(const TekGuiOval* oval);
void tekGuiDeleteOval(const TekGuiOval* oval);

exception tekGuiCreateImage(vec2 point_a, vec2 point_b, const char* texture_filename, TekGuiImage* image);
exception tekGuiDrawImage(const TekGuiImage* image);
void tekGuiDeleteImage(const TekGuiImage* image);