#include "size.h"

#include "../tekgl/manager.h"

flag initialised = 0;
float width, height;

void tekGuiSizeFramebufferCallback(const int window_width, const int window_height) {
    width = (float)window_width;
    height = (float)window_height;
}

exception tekGuiInitSizeManager() {
    if (initialised) return SUCCESS;
    initialised = 1;
    tekChainThrow(tekAddFramebufferCallback(tekGuiSizeFramebufferCallback));
    int window_width, window_height;
    tekGetWindowSize(&window_width, &window_height);
    width = (float)window_width;
    height = (float)window_height;
    return SUCCESS;
}

float tekGuiSizeToPixels(const TekGuiSize size) {
    switch (size.type) {
    case GUI_SIZE_FRACTION_X:
        return size.size * width;
    case GUI_SIZE_FRACTION_Y:
        return size.size * height;
    case GUI_SIZE_PIXELS:
        return size.size;
    default:
        return 0.0f;
    }
}

float tekGuiSizeToFractionX(const TekGuiSize size) {
    switch (size.type) {
    case GUI_SIZE_FRACTION_X:
        return size.size;
    case GUI_SIZE_FRACTION_Y:
        return size.size * height / width;
    case GUI_SIZE_PIXELS:
        return size.size / width;
    default:
        return 0.0f;
    }
}

float tekGuiSizeToFractionY(const TekGuiSize size) {
    switch (size.type) {
    case GUI_SIZE_FRACTION_X:
        return size.size * width / height;
    case GUI_SIZE_FRACTION_Y:
        return size.size;
    case GUI_SIZE_PIXELS:
        return size.size / height;
    default:
        return 0.0f;
    }
}