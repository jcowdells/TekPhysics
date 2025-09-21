#include "text_button.h"

#include <cglm/vec2.h>

#include "tekgui.h"
#include "../core/vector.h"

#include "../tekgl/manager.h"
#include "../tekgl/shader.h"
#include "glad/glad.h"

#define NOT_INITIALISED 0
#define INITIALISED 1
#define DE_INITIALISED 2

static flag text_button_init = NOT_INITIALISED;
static flag text_button_gl_init = NOT_INITIALISED;

static uint text_button_shader;
static uint text_button_vao;
static uint text_button_vbo;
static TekBitmapFont text_button_font;

static Vector text_button_mesh_buffer;

static exception tekGuiTextButtonGLLoad() {
    glGenBuffers(1, &text_button_vbo);
    glGenVertexArrays(1, &text_button_vao);
    glBindVertexArray(text_button_vao);
    glBindBuffer(GL_ARRAY_BUFFER, text_button_vbo);

    const int layout[] = {
        2, 2, 2, 2
    };
    const uint len_layout = sizeof(layout) / sizeof(int);

    // find the total size of each vertex
    int layout_size = 0;
    for (uint i = 0; i < len_layout; i++) layout_size += layout[i];

    // convert size to bytes
    layout_size *= sizeof(float);

    // create attrib pointer for each section of the layout
    int prev_layout = 0;
    for (uint i = 0; i < len_layout; i++) {
        if (layout[i] == 0)
            tekThrow(OPENGL_EXCEPTION, "Cannot have layout of size 0.");
        glVertexAttribPointer(i, layout[i], GL_FLOAT, GL_FALSE, layout_size, (void*)(prev_layout * sizeof(float)));
        glEnableVertexAttribArray(i);
        prev_layout += layout[i];
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    tekChainThrow(tekCreateShaderProgramVGF("../shader/text_button.glvs", "../shader/window.glgs", "../shader/window.glfs", &text_button_shader));
    tekChainThrow(tekCreateBitmapFont("../res/verdana.ttf", 0, 64, &text_button_font));

    text_button_gl_init = INITIALISED;
    return SUCCESS;
}

tek_init tekGuiTextButtonInit() {
    exception tek_exception = tekAddGLLoadFunc(tekGuiTextButtonGLLoad);
    if (tek_exception) return;

    tek_exception = vectorCreate(1, sizeof(TekGuiTextButtonData), &text_button_mesh_buffer);
    if (tek_exception) return;

    text_button_init = INITIALISED;
}

static void tekGuiGetTextButtonData(const TekGuiTextButton* button, TekGuiTextButtonData* button_data) {
    glm_vec2_copy((vec2){(float)button->button.hitbox_x, (float)(button->button.hitbox_x + button->button.hitbox_width)}, button_data->minmax_x);
    glm_vec2_copy((vec2){(float)button->button.hitbox_y, (float)(button->button.hitbox_y + button->button.hitbox_height)}, button_data->minmax_y);
    glm_vec2_copy((vec2){(float)(button->button.hitbox_x + button->border_width), (float)(button->button.hitbox_x + button->button.hitbox_width - button->border_width)}, button_data->minmax_ix);
    glm_vec2_copy((vec2){(float)(button->button.hitbox_y + button->border_width), (float)(button->button.hitbox_y + button->button.hitbox_height - button->border_width)}, button_data->minmax_iy);
}

static exception tekGuiTextButtonAddGLMesh(const TekGuiTextButton* text_button, uint* index) {
    if (!text_button_init) tekThrow(FAILURE, "Attempted to run function before initialised.");
    if (!text_button_gl_init) tekThrow(OPENGL_EXCEPTION, "Attempted to run function before OpenGL initialised.");

    TekGuiTextButtonData text_button_data = {};
    tekGuiGetTextButtonData(text_button, &text_button_data);

    // length = index of the next item which will be added
    *index = text_button_mesh_buffer.length;

    // add new text button data to buffer
    tekChainThrow(vectorAddItem(&text_button_mesh_buffer, &text_button_data));

    // update buffers using glBufferData to add new item
    glBindVertexArray(text_button_vao);
    glBindBuffer(GL_ARRAY_BUFFER, text_button_vbo);
    glBufferData(GL_ARRAY_BUFFER, (long)(text_button_mesh_buffer.length * sizeof(TekGuiTextButtonData)), text_button_mesh_buffer.internal, GL_DYNAMIC_DRAW);

    // unbind buffers for cleanliness
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return SUCCESS;
}

static exception tekGuiTextButtonUpdateGLMesh(const TekGuiTextButton* text_button) {
    if (!text_button_init) tekThrow(FAILURE, "Attempted to run function before initialised.");
    if (!text_button_gl_init) tekThrow(OPENGL_EXCEPTION, "Attempted to run function before OpenGL initialised.");

    TekGuiTextButtonData text_button_data;
    tekGuiGetTextButtonData(text_button, &text_button_data);

    // update the vector containing text button data
    tekChainThrow(vectorSetItem(&text_button_mesh_buffer, text_button->mesh_index, &text_button_data));

    // update the vertex buffer using glSubBufferData to overwrite the old data without reallocating.
    glBindVertexArray(text_button_vao);
    glBindBuffer(GL_ARRAY_BUFFER, text_button_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, (long)(text_button->mesh_index * sizeof(TekGuiTextButtonData)), sizeof(TekGuiTextButtonData), &text_button_data);

    // unbind buffers for cleanliness
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

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
    tekBindShaderProgram(text_button_shader);

    int window_width, window_height;
    tekGetWindowSize(&window_width, &window_height);
    tekChainThrow(tekShaderUniformVec2(text_button_shader, "window_size", (vec2){(float)window_width, (float)window_height}));
    if (button->hovered) {
        tekChainThrow(tekShaderUniformVec4(text_button_shader, "bg_colour", button->selected_colour));
    } else {
        tekChainThrow(tekShaderUniformVec4(text_button_shader, "bg_colour", button->background_colour));
    }
    tekChainThrow(tekShaderUniformVec4(text_button_shader, "bd_colour", button->border_colour));

    glDepthFunc(GL_ALWAYS);
    glBindVertexArray(text_button_vao);
    glDrawArrays(GL_POINTS, (int)button->mesh_index, 1);

    // unbind everything to keep clean.
    glBindVertexArray(0);
    tekBindShaderProgram(0);

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