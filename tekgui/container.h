#pragma once

#include "../core/exception.h"
#include "../tekgl.h"

#include "tekgui.h"
#include "size.h"

#define ALIGN_X_LEFT   0b000001
#define ALIGN_X_CENTER 0b000010
#define ALIGN_X_RIGHT  0b000100
#define ALIGN_Y_TOP    0b001000
#define ALIGN_Y_CENTER 0b010000
#define ALIGN_Y_BOTTOM 0b100000

#define FILL_X_EXPAND 0b0001
#define FILL_X_SHRINK 0b0010
#define FILL_Y_EXPAND 0b0100
#define FILL_Y_SHRINK 0b1000

#define ARRANGE_HORIZONTAL 0
#define ARRANGE_VERTICAL   1

exception tekGuiCreateContainer(TekGuiSize padding, TekGuiSize margin, flag alignment, flag arrangement, flag fill, TekGuiObject* container);