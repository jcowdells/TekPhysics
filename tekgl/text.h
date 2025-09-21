#pragma once

#include <cglm/vec4.h>
#include "../core/exception.h"
#include "font.h"
#include "mesh.h"

typedef struct TekText {
    float width, height;
    TekMesh mesh;
    TekBitmapFont* font;
} TekText;

exception tekCreateText(const char* text, uint size, TekBitmapFont* font, TekText* tek_text);
exception tekDrawColouredText(const TekText* tek_text, float x, float y, const vec4 colour);
exception tekDrawText(const TekText* tek_text, float x, float y);
void tekDeleteText(const TekText* tek_text);