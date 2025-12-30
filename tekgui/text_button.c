#include "text_button.h"

#include <cglm/vec2.h>

#include "tekgui.h"
#include "../core/vector.h"

#include "../tekgl/manager.h"
#include "../tekgl/shader.h"
#include "glad/glad.h"

/**
 * Get the dat needed to draw a box for the button.
 * @param button The text button to get the box data for.
 * @param box_data The outputted box data 
 */
static void tekGuiGetTextButtonData(const TekGuiTextButton* button, TekGuiBoxData* box_data) {
    // minmax = minimum and maximum extents in an axis
    // minmax_i = internal box, excluding border
    glm_vec2_copy((vec2){(float)button->button.hitbox_x, (float)(button->button.hitbox_x + button->button.hitbox_width)}, box_data->minmax_x);
    glm_vec2_copy((vec2){(float)button->button.hitbox_y, (float)(button->button.hitbox_y + button->button.hitbox_height)}, box_data->minmax_y);
    glm_vec2_copy((vec2){(float)(button->button.hitbox_x + button->border_width), (float)(button->button.hitbox_x + button->button.hitbox_width - button->border_width)}, box_data->minmax_ix);
    glm_vec2_copy((vec2){(float)(button->button.hitbox_y + button->border_width), (float)(button->button.hitbox_y + button->button.hitbox_height - button->border_width)}, box_data->minmax_iy);
}

/**
 * Add box mesh to the list of boxes to be drawn. All box backgrounds are stored in the same vertex buffer
 * So each box usage needs to be registered to find the index of the box in this buffer.
 * @param text_button The text button to add a mesh for.
 * @param index The outputted index of the box.
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiTextButtonAddGLMesh(const TekGuiTextButton* text_button, uint* index) {
    // get box data and create new box and add to list of boxes.
    TekGuiBoxData box_data = {};
    tekGuiGetTextButtonData(text_button, &box_data);
    tekChainThrow(tekGuiCreateBox(&box_data, index));
    return SUCCESS;
}

/**
 * Update the background box mesh of the text button.
 * @param text_button The text button to update the mesh for.
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiTextButtonUpdateGLMesh(const TekGuiTextButton* text_button) {
    // get box data and update
    TekGuiBoxData box_data = {};
    tekGuiGetTextButtonData(text_button, &box_data);
    tekChainThrow(tekGuiUpdateBox(&box_data, text_button->mesh_index));
    return SUCCESS;
}

/**
 * Create some text to be displayed on the text button.
 * @param button The button to create the text for.
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiTextButtonCreateText(TekGuiTextButton* button) {
    // get the default font.
    TekBitmapFont* font;
    tekChainThrow(tekGuiGetDefaultFont(&font));

    // create text with button data
    tekChainThrow(tekCreateText(button->text, button->text_height, font, &button->tek_text));
    return SUCCESS;
}

/**
 * Recreate the displayed text on a text button.
 * @param button The button to recreate the text for.
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiTextButtonRecreateText(TekGuiTextButton* button) {
    // delete old text and make a new one
    tekDeleteText(&button->tek_text);
    tekChainThrow(tekGuiTextButtonCreateText(button));
    return SUCCESS;
}

/**
 * Callback for text buttons. Passes on the callback to the text button and update hovered status.
 * @param button The base button that received the callback.
 * @param callback_data The callback data associated with the button press.
 */
static void tekGuiTextButtonCallback(TekGuiButton* button, TekGuiButtonCallbackData callback_data) {
    // get text button from button data.
    TekGuiTextButton* text_button = (TekGuiTextButton*)button->data;

    // update hovered status
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

    // pass callback onto the text button callback
    if (text_button->callback) text_button->callback(text_button, callback_data);
}

/**
 * Create a new text button which is a button that has text.
 * @param text The text on the button.
 * @param button The outputted button.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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
    button->callback = NULL;
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

/**
 * Set the pixel position of a text button on the window.
 * @param button The button to set the position of.
 * @param x_pos The x position to set the button to.
 * @param y_pos The y position to set the button to.
 * @throws OPENGL_EXCEPTION .
 */
exception tekGuiSetTextButtonPosition(TekGuiTextButton* button, const uint x_pos, const uint y_pos) {
    // set position and recreate mesh
    tekGuiSetButtonPosition(&button->button, x_pos, y_pos);
    tekChainThrow(tekGuiTextButtonUpdateGLMesh(button));
    return SUCCESS;
}

/**
 * Set the size of a text button in pixels.
 * @param button The button to update the size of.
 * @param width The new width of the button in pixels. 
 * @param height The new height of the button in pixels.
 * @throws OPENGL_EXCEPTION .
 */
exception tekGuiSetTextButtonSize(TekGuiTextButton* button, const uint width, const uint height) {
    // update button size and recreate mesh
    tekGuiSetButtonSize(&button->button, width, height);
    tekChainThrow(tekGuiTextButtonUpdateGLMesh(button));
    tekChainThrow(tekGuiTextButtonRecreateText(button));
    return SUCCESS;
}

/**
 * Update the text that is displayed on a text button.
 * @param button The button to update the text for.
 * @param text The new text to display.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
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

/**
 * Draw a text button to the screen, drawing the background and text. Also shade the button if hovered.
 * @param button The button to draw to the screen.
 * @throws OPENGL_EXCEPTION .
 */
exception tekGuiDrawTextButton(const TekGuiTextButton* button) {
    // get colour based on if hovered or not
    vec4 background_colour;
    if (button->hovered) {
        memcpy(background_colour, button->selected_colour, sizeof(vec4));
    } else {
        memcpy(background_colour, button->background_colour, sizeof(vec4));
    }

    // draw the background of the button
    tekChainThrow(tekGuiDrawBox(button->mesh_index, background_colour, button->border_colour));

    // draw the text + slight offset to center the text.
    const float x = (float)button->button.hitbox_x + ((float)button->button.hitbox_width - button->tek_text.width) * 0.5f;
    const float y = (float)button->button.hitbox_y + ((float)button->button.hitbox_height - button->tek_text.height) * 0.5f;
    tekChainThrow(tekDrawText(&button->tek_text, x, y));

    return SUCCESS;
}

/**
 * Delete a text button, freeing any memory allocated.
 * @param button 
 */
void tekGuiDeleteTextButton(const TekGuiTextButton* button) {
    // free displayed text
    if (button->text)
        free(button->text);

    // delete button and text gui elements
    tekGuiDeleteButton(&button->button);
    tekDeleteText(&button->tek_text);
}
