#include "text_button.h"

#include <cglm/vec2.h>

#include "tekgui.h"
#include "../core/vector.h"

#include "../tekgl/manager.h"
#include "../tekgl/shader.h"
#include "glad/glad.h"

static void tekGuiGetTextButtonData(const TekGuiTextButton* button, TekGuiBoxData* box_data) {
    glm_vec2_copy((vec2){(float)button->button.hitbox_x, (float)(button->button.hitbox_x + button->button.hitbox_width)}, box_data->minmax_x);
    glm_vec2_copy((vec2){(float)button->button.hitbox_y, (float)(button->button.hitbox_y + button->button.hitbox_height)}, box_data->minmax_y);
    glm_vec2_copy((vec2){(float)(button->button.hitbox_x + button->border_width), (float)(button->button.hitbox_x + button->button.hitbox_width - button->border_width)}, box_data->minmax_ix);
    glm_vec2_copy((vec2){(float)(button->button.hitbox_y + button->border_width), (float)(button->button.hitbox_y + button->button.hitbox_height - button->border_width)}, box_data->minmax_iy);
}

static exception tekGuiTextButtonAddGLMesh(const TekGuiTextButton* text_button, uint* index) {
    TekGuiBoxData box_data = {};
    tekGuiGetTextButtonData(text_button, &box_data);
    tekChainThrow(tekGuiCreateBox(&box_data, index));
    return SUCCESS;
}

static exception tekGuiTextButtonUpdateGLMesh(const TekGuiTextButton* text_button) {
    TekGuiBoxData box_data = {};
    tekGuiGetTextButtonData(text_button, &box_data);
    tekChainThrow(tekGuiUpdateBox(&box_data, text_button->mesh_index));
    return SUCCESS;
}

static exception tekGuiTextButtonCreateText(TekGuiTextButton* button) {
    tekCreateText(button->text, button->text_height, tekGuiGetDefaultFont(), &button->tek_text);
    return SUCCESS;
}

static exception tekGuiTextButtonRecreateText(TekGuiTextButton* button) {
    tekDeleteText(&button->tek_text);
    tekChainThrow(tekGuiTextButtonCreateText(button));
    return SUCCESS;
}

static void tekGuiTextButtonCallback(TekGuiButton* button, TekGuiButtonCallbackData callback_data) {
    TekGuiTextButton* text_button = (TekGuiTextButton*)button->data;

    switch (callback_data.type) {
    case TEK_GUI_BUTTON_MOUSE_ENTER_CALLBACK:
        text_button->hovered = 1;
        break;
    case TEK_GUI_BUTTON_MOUSE_LEAVE_CALLBACK:
        text_button->hovered = 0;
        break;
    default:
        break;
    }

    if (text_button->callback) text_button->callback(text_button, callback_data);
}

exception tekGuiCreateTextButton(const char* text, TekGuiTextButton* button) {
    // collect default values for a text button e.g. size and colours.
    struct TekGuiTextButtonDefaults defaults = {};
    tekChainThrow(tekGuiGetTextButtonDefaults(&defaults));

    // create the internal button that will receive events etc.
    tekChainThrow(tekGuiCreateButton(&button->button));
    tekGuiSetButtonPosition(&button->button, defaults.x_pos, defaults.y_pos);
    tekGuiSetButtonSize(&button->button, defaults.width, defaults.height);
    button->button.callback = tekGuiTextButtonCallback;
    button->border_width = defaults.border_width;
    button->text_height = defaults.text_height;
    button->hovered = 0;
    button->button.data = button;
    glm_vec4_copy(defaults.background_colour, button->background_colour);
    glm_vec4_copy(defaults.selected_colour, button->selected_colour);
    glm_vec4_copy(defaults.border_colour, button->border_colour);

    // fill up text buffer with the text to be displayed.
    const uint len_text = strlen(text) + 1;
    button->text = (char*)malloc(len_text * sizeof(char));
    if (!button->text)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for text buffer.");
    memcpy(button->text, text, len_text);

    // generate the tek_text struct.
    tekChainThrow(tekGuiTextButtonCreateText(button));

    // generate the mesh to display the button.
    tekChainThrow(tekGuiTextButtonAddGLMesh(button, &button->mesh_index));

    return SUCCESS;
}

exception tekGuiSetTextButtonPosition(TekGuiTextButton* button, const uint x_pos, const uint y_pos) {
    tekGuiSetButtonPosition(&button->button, x_pos, y_pos);
    tekChainThrow(tekGuiTextButtonUpdateGLMesh(button));
    return SUCCESS;
}

exception tekGuiSetTextButtonSize(TekGuiTextButton* button, const uint width, const uint height) {
    tekGuiSetButtonSize(&button->button, width, height);
    tekChainThrow(tekGuiTextButtonUpdateGLMesh(button));
    tekChainThrow(tekGuiTextButtonRecreateText(button));

    return SUCCESS;
}

exception tekGuiSetTextButtonText(TekGuiTextButton* button, const char* text) {
    // free the pointer to the text buffer if it exists.
    if (button->text)
        free(button->text);

    // create new buffer for the new text to be stored in.
    const uint len_text = strlen(text) + 1;
    button->text = (char*)malloc(len_text * sizeof(char));
    if (!button->text)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for text buffer.");

    // copy over the new text.
    memcpy(button->text, text, len_text);

    // rebuild tek_text struct.
    tekGuiTextButtonRecreateText(button);

    return SUCCESS;
}

exception tekGuiDrawTextButton(const TekGuiTextButton* button) {
    vec4 background_colour;
    if (button->hovered) {
        memcpy(background_colour, button->selected_colour, sizeof(vec4));
    } else {
        memcpy(background_colour, button->background_colour, sizeof(vec4));
    }

    glDepthFunc(GL_ALWAYS);
    tekChainThrow(tekGuiDrawBox(button->mesh_index, background_colour, button->border_colour));

    const float x = (float)button->button.hitbox_x + ((float)button->button.hitbox_width - button->tek_text.width) * 0.5f;
    const float y = (float)button->button.hitbox_y + ((float)button->button.hitbox_height - button->tek_text.height) * 0.5f;
    tekChainThrow(tekDrawText(&button->tek_text, x, y));

    glDepthFunc(GL_LESS);

    return SUCCESS;
}

void tekGuiDeleteTextButton(const TekGuiTextButton* button) {
    if (button->text)
        free(button->text);

    tekGuiDeleteButton(&button->button);
    tekDeleteText(&button->tek_text);
}