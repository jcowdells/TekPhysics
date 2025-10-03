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

static uint tekGuiGetTextInputTextLength(const TekGuiTextInput* text_input) {
    if (text_input->text_max_length != -1) {
        return (uint)text_input->text_max_length;
    }
    return text_input->text.length;
}

static void tekGuiTextInputDecrementCursor(TekGuiTextInput* text_input) {
    if (text_input->cursor_index > 0)
        text_input->cursor_index--;
    else
        return;
    if (text_input->text_start_index > 0 && text_input->cursor_index < text_input->text_start_index)
        text_input->text_start_index--;
}

static void tekGuiTextInputIncrementCursor(TekGuiTextInput* text_input) {
    if (text_input->cursor_index + 2 < text_input->text.length)
        text_input->cursor_index++;
    else
        return;
    if (text_input->text_max_length <= -1)
        return;
    if ((int)text_input->cursor_index + 1 > text_input->text_start_index + text_input->text_max_length) {
        text_input->text_start_index++;
    }
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

static exception tekGuiTextInputGetTextPtr(const TekGuiTextInput* text_input, char** buffer) {
    const uint length = tekGuiGetTextInputTextLength(text_input);
    *buffer = (char*)malloc(length * sizeof(char));
    if (!*buffer)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for text buffer.");

    if (text_input->text_max_length != -1) {
        memcpy(*buffer, (char*)text_input->text.internal + text_input->text_start_index, length);
        (*buffer)[length - 1] = 0;
        return SUCCESS;
    }

    memcpy(*buffer, text_input->text.internal, length);
    return SUCCESS;
}

static exception tekGuiTextInputCreateText(TekGuiTextInput* text_input) {
    char* text = 0;

    tekChainThrow(tekGuiTextInputGetTextPtr(text_input, &text));
    tekChainThrow(tekCreateText(text, text_input->text_height, &monospace_font, &text_input->tek_text));

    const float max_width = (float)text_input->button.hitbox_width - 2 * text_input->tek_text.height;
    if (text_input->text_max_length == -1 && text_input->tek_text.width >= max_width) {
        text_input->text_max_length = (int)text_input->text.length - 1;
        tekChainThrow(tekCreateText(text, text_input->text_height, &monospace_font, &text_input->tek_text));
    }

    const uint cursor_index = text_input->cursor_index - text_input->text_start_index;
    text[cursor_index] = CURSOR;
    tekChainThrow(tekCreateText(text, text_input->text_height, &monospace_font, &text_input->tek_text_cursor));

    free(text);
    return SUCCESS;
}

static exception tekGuiTextInputRecreateText(TekGuiTextInput* text_input) {
    char* text = 0;

    tekChainThrow(tekGuiTextInputGetTextPtr(text_input, &text));
    tekChainThrow(tekUpdateText(&text_input->tek_text, text, text_input->text_height));

    const float max_width = (float)text_input->button.hitbox_width - 2 * text_input->tek_text.height;
    if (text_input->text_max_length == -1 && text_input->tek_text.width >= max_width) {
        text_input->text_max_length = (int)text_input->text.length - 1;
        tekChainThrow(tekUpdateText(&text_input->tek_text, text, text_input->text_height));
    }

    const uint cursor_index = text_input->cursor_index - text_input->text_start_index;
    text[cursor_index] = CURSOR;
    tekChainThrow(tekUpdateText(&text_input->tek_text_cursor, text, text_input->text_height));

    free(text);
    return SUCCESS;
}

static exception tekGuiTextInputAdd(TekGuiTextInput* text_input, const char codepoint) {
    tekChainThrow(vectorInsertItem(&text_input->text, text_input->cursor_index, &codepoint));
    tekGuiTextInputIncrementCursor(text_input);
    return SUCCESS;
}

static exception tekGuiTextInputRemove(TekGuiTextInput* text_input) {
    if (text_input->text.length <= 1) return SUCCESS;
    if (text_input->cursor_index <= 0) return SUCCESS;
    tekChainThrow(vectorRemoveItem(&text_input->text, text_input->cursor_index - 1, NULL));
    if (text_input->cursor_index > 0) {
        text_input->cursor_index--;
        if (text_input->text_start_index > 0)
            text_input->text_start_index--;
    }
    return SUCCESS;
}

static void tekGuiTextInputCharCallback(const uint codepoint) {
    if (!selected_input) return;
    tekGuiTextInputAdd(selected_input, (char)codepoint);
    tekGuiTextInputRecreateText(selected_input);
}

static exception tekGuiFinishTextInput(TekGuiTextInput* text_input) {
    selected_input = 0;
    text_input->cursor_index = 0;
    text_input->text_start_index = 0;
    if (text_input->callback)
        text_input->callback(text_input, text_input->text.internal, text_input->text.length);
    tekChainThrow(tekGuiTextInputRecreateText(text_input));
    return SUCCESS;
}

static void tekGuiTextInputKeyCallback(const int key, const int scancode, const int action, const int mods) {
    if (!selected_input) return;
    if (action != GLFW_RELEASE && action != GLFW_REPEAT) return;

    TekGuiTextInput* text_input = selected_input;

    switch (key) {
    case GLFW_KEY_ENTER:
        tekGuiFinishTextInput(selected_input);
        break;
    case GLFW_KEY_BACKSPACE:
        tekGuiTextInputRemove(selected_input);
        break;
    case GLFW_KEY_LEFT:
        tekGuiTextInputDecrementCursor(selected_input);
        break;
    case GLFW_KEY_RIGHT:
        tekGuiTextInputIncrementCursor(selected_input);
        break;
    default:
        return;
    }

    tekGuiTextInputRecreateText(text_input);
}

static void tekGuiTextInputButtonCallback(TekGuiButton* button, TekGuiButtonCallbackData callback_data) {
    TekGuiTextInput* text_input = (TekGuiTextInput*)button->data;

    switch (callback_data.type) {
    case TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK:
        if (callback_data.data.mouse_button.button != GLFW_MOUSE_BUTTON_LEFT || callback_data.data.mouse_button.action != GLFW_RELEASE)
            break;
        if (text_input == selected_input)
            break;
        if (selected_input)
            tekGuiFinishTextInput(selected_input);
        selected_input = text_input;
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
    text_input->text_max_length = -1;
    text_input->text_start_index = 0;
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
    if (text_input == selected_input && time.tv_nsec % BILLION > BILLION / 2) {
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

exception tekGuiSetTextInputPosition(TekGuiTextInput* text_input, int x_pos, int y_pos) {
    tekGuiSetButtonPosition(&text_input->button, x_pos, y_pos);
    tekChainThrow(tekGuiTextInputUpdateGLMesh(text_input));

    return SUCCESS;
}

exception tekGuiSetTextInputSize(TekGuiTextInput* text_input, uint width, uint height) {
    tekGuiSetButtonSize(&text_input->button, width, height);
    tekChainThrow(tekGuiTextInputUpdateGLMesh(text_input));

    return SUCCESS;
}

exception tekGuiSetTextInputText(TekGuiTextInput* text_input, const char* text) {
    text_input->cursor_index = 0;
    text_input->text_start_index = 0;
    vectorClear(&text_input->text);

    const char zero = 0;
    for (uint i = 0; i < 2; i++)
        tekChainThrow(vectorAddItem(&text_input->text, &zero));

    const uint len_text = strlen(text);
    for (uint i = 0; i < len_text; i++) {
        tekChainThrow(tekGuiTextInputAdd(text_input, text[i]));
        if (text_input->text_max_length == -1)
            tekChainThrow(tekGuiTextInputRecreateText(text_input));
    }

    tekChainThrow(tekGuiTextInputRecreateText(text_input));

    if (text_input != selected_input) {
        text_input->cursor_index = 0;
        text_input->text_start_index = 0;
    }

    return SUCCESS;
}

void tekGuiDeleteTextInput(TekGuiTextInput* text_input) {
    tekGuiDeleteButton(&text_input->button);
    vectorDelete(&text_input->text);
}
