#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

typedef struct TekGuiButton {
    uint hitbox_x;
    uint hitbox_y;
    uint hitbox_width;
    uint hitbox_height;
} TekGuiButton;

typedef void(*TekGuiButtonCallback)(TekGuiButton* button, int button, int action, int mods);
