#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

#define TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK   0
#define TEK_GUI_BUTTON_MOUSE_ENTER_CALLBACK    1
#define TEK_GUI_BUTTON_MOUSE_LEAVE_CALLBACK    2
#define TEK_GUI_BUTTON_MOUSE_TOUCHING_CALLBACK 3

struct TekGuiButton;

typedef struct TekGuiButtonCallbackData {
    flag type;
    uint mouse_x;
    uint mouse_y;
    union {
        struct {
            int button;
            int action;
            int mods;
        } mouse_button;
    } data;
} TekGuiButtonCallbackData;

typedef void(*TekGuiButtonCallback)(struct TekGuiButton* button_ptr, const TekGuiButtonCallbackData callback_data);

typedef struct TekGuiButton {
    uint hitbox_x;
    uint hitbox_y;
    uint hitbox_width;
    uint hitbox_height;
    void* data;
    TekGuiButtonCallback callback;
} TekGuiButton;

exception tekGuiCreateButton(TekGuiButton* button);
void tekGuiSetButtonPosition(TekGuiButton* button, uint x, uint y);
void tekGuiSetButtonSize(TekGuiButton* button, uint width, uint height);
exception tekGuiDeleteButton(const TekGuiButton* button);
