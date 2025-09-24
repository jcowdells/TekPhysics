#include "text_input.h"

#include "../tekgl/manager.h"
#include "box_manager.h"
#include "tekgui.h"
#include "glad/glad.h"

#define NOT_INITIALISED 0
#define INITIALISED     1
#define DE_INITIALISED  2

void tekGuiGetTextInputData(const TekGuiTextInput* text_input, TekGuiBoxData* box_data) {
    glm_vec2_copy((vec2){(float)text_input->button.hitbox_x, (float)(text_input->button.hitbox_x + text_input->button.hitbox_width)}, box_data->minmax_x);
    glm_vec2_copy((vec2){(float)text_input->button.hitbox_y, (float)(text_input->button.hitbox_y + text_input->button.hitbox_height)}, box_data->minmax_y);
    glm_vec2_copy((vec2){(float)(text_input->button.hitbox_x + text_input->border_width), (float)(text_input->button.hitbox_x + text_input->button.hitbox_width - text_input->border_width)}, box_data->minmax_ix);
    glm_vec2_copy((vec2){(float)(text_input->button.hitbox_y + text_input->border_width), (float)(text_input->button.hitbox_y + text_input->button.hitbox_height - text_input->border_width)}, box_data->minmax_iy);
}

static exception tekGuiTextInputAddGLMesh(const TekGuiTextInput* text_input, uint* index) {
    TekGuiBoxData box_data = {};
    tekGuiGetTextInputData(text_input, &box_data);
    tekChainThrow(tekGuiCreateBox(&box_data, index));
    return SUCCESS;
}

static exception tekGuiTextInputUpdateGLMesh(const TekGuiTextInput* text_input) {
    TekGuiBoxData box_data = {};
    tekGuiGetTextInputData(text_input, &box_data);
    tekChainThrow(tekGuiUpdateBox(&box_data, text_input->mesh_index));
    return SUCCESS;
}

exception tekGuiCreateTextInput(TekGuiTextInput* text_input) {
    struct TekGuiTextInputDefaults defaults = {};
    tekChainThrow(tekGuiGetTextInputDefaults(&defaults));

    tekChainThrow(tekGuiCreateButton(&text_input->button));
    tekGuiSetButtonPosition(&text_input->button, defaults.x_pos, defaults.y_pos);
    tekGuiSetButtonSize(&text_input->button, defaults.width, defaults.height);

    tekChainThrow(tekGuiTextInputAddGLMesh(text_input, &text_input->mesh_index));

    tekChainThrow(vectorCreate(16, sizeof(char), &text_input->text_data))

    text_input->border_width = defaults.border_width;
    glm_vec4_copy(defaults.background_colour, text_input->background_colour);
    glm_vec4_copy(defaults.border_colour, text_input->border_colour);
    glm_vec4_copy(defaults.text_colour, text_input->text_colour);

    return SUCCESS;
}

exception tekGuiDrawTextInput(const TekGuiTextInput* text_input) {
    glDepthFunc(GL_ALWAYS);
    tekGuiDrawBox(text_input->mesh_index, text_input->background_colour, text_input->border_colour);
    glDepthFunc(GL_LESS);

    return SUCCESS;
}

void tekGuiDeleteTextInput(TekGuiTextInput* text_input) {
    tekGuiDeleteButton(&text_input->button);
    vectorDelete(&text_input->text_data);
}