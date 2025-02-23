#pragma once

#include "../core/exception.h"
#include "font.h"
#include "mesh.h"

typedef struct TekText {
    double x, y;
    TekMesh mesh;
    TekBitmapFont* font;
} TekText;

exception tekInitTextEngine();
void tekDeleteTextEngine();
exception tekCreateText(const char* text, uint size, TekBitmapFont* font, TekText* tek_text);
exception tekDrawText(const TekText* tek_text, float x, float y);
void tekDeleteText(const TekText* tek_text);