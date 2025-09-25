#include "text_input.h"

#include "../tekgl/manager.h"
#include "box_manager.h"
#include "tekgui.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <time.h>

#define NOT_INITIALISED 0
#define INITIALISED     1
#define DE_INITIALISED  2

#define CURSOR '|'

static TekGuiTextInput* selected_input = 0;
static TekBitmapFont monospace_font = {};

static void tekGuiGetTextInputData(const TekGuiTextInput* text_input, TekGuiBoxData* box_data) {
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

static exception tekGuiTextInputCreateText(TekGuiTextInput* text_input) {
    tekChainThrow(tekCreateText(text_input->text.internal, text_input->text_height, &monospace_font, &text_input->tek_text));

    char temp;
    tekChainThrow(vectorGetItem(&text_input->text, text_input->cursor_index, &temp));
    const char cursor = CURSOR;
    tekChainThrow(vectorSetItem(&text_input->text, text_input->cursor_index, &cursor));
    tekChainThrow(tekCreateText(text_input->text.internal, text_input->text_height, &monospace_font, &text_input->tek_text_cursor));
    tekChainThrow(vectorSetItem(&text_input->text, text_input->cursor_index, &temp));

    return SUCCESS;
}

static exception tekGuiTextInputRecreateText(TekGuiTextInput* text_input) {
    tekChainThrow(tekUpdateText(&text_input->tek_text, text_input->text.internal, text_input->text_height));

    char temp;
    tekChainThrow(vectorGetItem(&text_input->text, text_input->cursor_index, &temp));
    const char cursor = CURSOR;
    tekChainThrow(vectorSetItem(&text_input->text, text_input->cursor_index, &cursor));
    tekChainThrow(tekUpdateText(&text_input->tek_text_cursor, text_input->text.internal, text_input->text_height));
    tekChainThrow(vectorSetItem(&text_input->text, text_input->cursor_index, &temp));

    return SUCCESS;
}

static exception tekGuiTextInputAdd(TekGuiTextInput* text_input, const char codepoint) {
    tekChainThrow(vectorInsertItem(&text_input->text, text_input->cursor_index, &codepoint));
    text_input->cursor_index++;
    return SUCCESS;
}

static exception tekGuiTextInputRemove(TekGuiTextInput* text_input) {
    if (text_input->text.length <= 1) return SUCCESS;
    if (text_input->cursor_index <= 0) return SUCCESS;
    tekChainThrow(vectorRemoveItem(&text_input->text, text_input->cursor_index - 1, NULL));
    text_input->cursor_index--;
    return SUCCESS;
}

static void tekGuiTextInputCharCallback(const uint codepoint) {
    if (!selected_input) return;
    printf("Char: %c\n", codepoint);
    tekGuiTextInputAdd(selected_input, (char)codepoint);
    printf("%d\n", tekGuiTextInputRecreateText(selected_input));
}

static void tekGuiTextInputKeyCallback(const int key, const int scancode, const int action, const int mods) {
    if (!selected_input) return;
    if (action != GLFW_RELEASE && action != GLFW_REPEAT) return;

    TekGuiTextInput* text_input = selected_input;

    switch (key) {
    case GLFW_KEY_ENTER:
        selected_input = 0;
        break;
    case GLFW_KEY_BACKSPACE:
        tekGuiTextInputRemove(selected_input);
        break;
    case GLFW_KEY_LEFT:
        if (selected_input->cursor_index > 0)
            selected_input->cursor_index--;
        break;
    case GLFW_KEY_RIGHT:
        if (selected_input->cursor_index + 2 < selected_input->text.length)
            selected_input->cursor_index++;
        break;
    default:
        return;
    }

    tekGuiTextInputRecreateText(text_input);
}

static void tekGuiTextInputButtonCallback(TekGuiButton* button, TekGuiButtonCallbackData callback_data) {
    printf("Callbakc\n");
    TekGuiTextInput* text_input = (TekGuiTextInput*)button->data;

    switch (callback_data.type) {
    case TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK:
        if (callback_data.data.mouse_button.button != GLFW_MOUSE_BUTTON_LEFT || callback_data.data.mouse_button.action != GLFW_RELEASE)
            break;
        selected_input = text_input;
        printf("Selected input: %p\n", selected_input);
        break;
    default:
        break;
    }
}

static exception tekGuiTextInputGLLoad() {
    tekChainThrow(tekCreateBitmapFont("../res/inconsolata.ttf", 0, 64, &monospace_font));
    return SUCCESS;
}

tek_init tekGuiTextInputInit() {
    tekAddCharCallback(tekGuiTextInputCharCallback);
    tekAddKeyCallback(tekGuiTextInputKeyCallback);
    tekAddGLLoadFunc(tekGuiTextInputGLLoad);
}

exception tekGuiCreateTextInput(TekGuiTextInput* text_input) {
    struct TekGuiTextInputDefaults defaults = {};
    tekChainThrow(tekGuiGetTextInputDefaults(&defaults));

    tekChainThrow(tekGuiCreateButton(&text_input->button));
    tekGuiSetButtonPosition(&text_input->button, defaults.x_pos, defaults.y_pos);
    tekGuiSetButtonSize(&text_input->button, defaults.width, defaults.text_height * 5 / 4);
    text_input->button.callback = tekGuiTextInputButtonCallback;
    text_input->button.data = (void*)text_input;

    tekChainThrow(tekGuiTextInputAddGLMesh(text_input, &text_input->mesh_index));

    tekChainThrow(vectorCreate(16, sizeof(char), &text_input->text));
    const char zero = 0;
    tekChainThrow(vectorAddItem(&text_input->text, &zero));
    tekChainThrow(vectorAddItem(&text_input->text, &zero));
    text_input->text_height = defaults.text_height;
    text_input->cursor_index = 0;
    tekChainThrow(tekGuiTextInputCreateText(text_input));

    text_input->border_width = defaults.border_width;
    glm_vec4_copy(defaults.background_colour, text_input->background_colour);
    glm_vec4_copy(defaults.border_colour, text_input->border_colour);
    glm_vec4_copy(defaults.text_colour, text_input->text_colour);

    return SUCCESS;
}

exception tekGuiDrawTextInput(const TekGuiTextInput* text_input) {
    glDepthFunc(GL_ALWAYS);
    tekChainThrow(tekGuiDrawBox(text_input->mesh_index, text_input->background_colour, text_input->border_colour));
    const float x = (float)(text_input->button.hitbox_x + (int)(text_input->text_height / 2));
    const float y = (float)text_input->button.hitbox_y;

    flag display_cursor = 0;
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    if (text_input == selected_input && time.tv_nsec % 1000000000 > 500000000) {
        display_cursor = 1;
    }

    if (display_cursor) {
        tekChainThrow(tekDrawColouredText(&text_input->tek_text_cursor, x, y, text_input->text_colour));
    } else {
        tekChainThrow(tekDrawColouredText(&text_input->tek_text, x, y, text_input->text_colour));
    }
    glDepthFunc(GL_LESS);

    return SUCCESS;
}

void tekGuiDeleteTextInput(TekGuiTextInput* text_input) {
    tekGuiDeleteButton(&text_input->button);
    vectorDelete(&text_input->text);
}
