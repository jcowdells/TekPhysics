#pragma once

#include "../tekgl.h"

#define hasFlag(f, m) (f & m) == m

#define CONTAINER_PTR 0x00

typedef struct TekGuiBbox {
    float min_x, min_y, max_x, max_y;
} TekGuiBbox;

struct TekGuiObject;

typedef float (*TekGuiGetSizeFunc)(struct TekGuiObject* gui_object);
typedef void (*TekGuiGetBboxFunc)(struct TekGuiObject* gui_object, TekGuiBbox* bbox);

typedef struct TekGuiInterface {
    flag type;
    TekGuiGetSizeFunc tekGuiGetWidth;
    TekGuiGetSizeFunc tekGuiGetHeight;
    TekGuiGetSizeFunc tekGuiGetUsableWidth;
    TekGuiGetSizeFunc tekGuiGetUsableHeight;
    TekGuiGetBboxFunc tekGuiGetBbox;
} TekGuiInterface;

typedef struct TekGuiObject {
    const TekGuiInterface* interface;
    struct TekGuiObject* parent;
    void* ptr;
} TekGuiObject;