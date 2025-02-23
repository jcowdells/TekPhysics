#pragma once

#include "../core/exception.h"
#include "../tekgl.h"

#define GUI_SIZE_PIXELS     0x00
#define GUI_SIZE_FRACTION_X 0x01
#define GUI_SIZE_FRACTION_Y 0x02

typedef struct TekGuiSize {
    flag type;
    float size;
} TekGuiSize;

exception tekGuiInitSizeManager();
float tekGuiSizeToPixels(TekGuiSize size);
float tekGuiSizeToFractionX(TekGuiSize size);
float tekGuiSizeToFractionY(TekGuiSize size);