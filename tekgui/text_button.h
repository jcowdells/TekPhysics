#pragma once

#include "button.h"
#include <cglm/vec4.h>
#include "../tekgl/text.h"

struct TekGuiTextButton;

typedef void(*TekGuiTextButtonCallback)(struct TekGuiTextButton* button, TekGuiButtonCallbackData callback_data);

typedef struct TekGuiTextButton {
    TekGuiButton button;
    char* text;
    TekText tek_text;
    uint border_width;
    uint text_height;
    vec4 background_colour;
    vec4 selected_colour;
    vec4 border_colour;
    uint mesh_index;
    flag hovered;
    TekGuiTextButtonCallback callback;
} TekGuiTextButton;

typedef struct TekGuiTextButtonData {
    vec2 minmax_x;
    vec2 minmax_y;
    vec2 minmax_ix;
    vec2 minmax_iy;
} TekGuiTextButtonData;

exception tekGuiCreateTextButton(const char* text, TekGuiTextButton* button);
exception tekGuiSetTextButtonPosition(TekGuiTextButton* button, uint x_pos, uint y_pos);
exception tekGuiSetTextButtonSize(TekGuiTextButton* button, uint width, uint height);
exception tekGuiSetTextButtonText(TekGuiTextButton* button, const char* text);
exception tekGuiDrawTextButton(const TekGuiTextButton* button);
void tekGuiDeleteTextButton(const TekGuiTextButton* button);