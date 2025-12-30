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

/**
 * Get the data for drawing a background box from a text input.
 * @param text_input The text input to get the data from.
 * @param box_data An empty box data struct to be filled with the data.
 */
static void tekGuiGetTextInputData(const TekGuiTextInput* text_input, TekGuiBoxData* box_data) {
    // minmax = the minimum and maximum extent of that direction
    // i = internal, this is the actual drawn area without borders
    glm_vec2_copy((vec2){(float)text_input->button.hitbox_x, (float)text_input->button.hitbox_x + (float)text_input->button.hitbox_width}, box_data->minmax_x);
    glm_vec2_copy((vec2){(float)text_input->button.hitbox_y, (float)text_input->button.hitbox_y + (float)text_input->button.hitbox_height}, box_data->minmax_y);
    glm_vec2_copy((vec2){(float)text_input->button.hitbox_x + (float)text_input->border_width, (float)text_input->button.hitbox_x + (float)(text_input->button.hitbox_width - text_input->border_width)}, box_data->minmax_ix);
    glm_vec2_copy((vec2){(float)text_input->button.hitbox_y + (float)text_input->border_width, (float)text_input->button.hitbox_y + (float)(text_input->button.hitbox_height - text_input->border_width)}, box_data->minmax_iy);
}

/**
 * Get the number of characters currently being displayed to the screen by a text input.
 * @param text_input The text input to get the length of.
 * @return The number of characters being displayed to the screen.
 */
static uint tekGuiGetTextInputTextLength(const TekGuiTextInput* text_input) {
    // if a max length has been found, return it
    if (text_input->text_max_length != -1) {
        return (uint)text_input->text_max_length;
    }

    // otherwise, not yet reached max, return current length.
    return text_input->text.length;
}

/**
 * Move the cursor backwards, and scroll the text if needed.
 * @param text_input The text input to decrement the cursor of.
 */
static void tekGuiTextInputDecrementCursor(TekGuiTextInput* text_input) {
    // if not already reached the start, decrement position
    if (text_input->cursor_index > 0)
        text_input->cursor_index--;
    else
        return;

    // move the text forwards if scrolling would move the cursor off the end
    if (text_input->text_start_index > 0 && text_input->cursor_index < text_input->text_start_index)
        text_input->text_start_index--;
}

/**
 * Move the cursor forwards by a single position, scrolling the text if needed.
 * @param text_input The text input to increment the cursor of.
 */
static void tekGuiTextInputIncrementCursor(TekGuiTextInput* text_input) {
    // only increment if there is space for the cursor to move into.
    if (text_input->cursor_index + 2 < text_input->text.length)
        text_input->cursor_index++;
    else
        return;

    // if max length not achieved, then last step can be skipped
    if (text_input->text_max_length <= -1)
        return;

    // if cursor would be going off the end, scroll all the text back to fit the next char on the end
    if ((int)text_input->cursor_index + 1 > text_input->text_start_index + text_input->text_max_length) {
        text_input->text_start_index++;
    }
}

/**
 * Create a new text input box background shape for a new text input.
 * @param text_input The text input to create the background for.
 * @param index The outputted index of the box. All the box meshes are stored in a single vertex buffer.
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiTextInputAddGLMesh(const TekGuiTextInput* text_input, uint* index) {
    // basic box background creation
    TekGuiBoxData box_data = {};
    tekGuiGetTextInputData(text_input, &box_data);
    tekChainThrow(tekGuiCreateBox(&box_data, index));
    return SUCCESS;
}

/**
 * Visually update the text input background area.
 * @param text_input The text input to update.
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiTextInputUpdateGLMesh(const TekGuiTextInput* text_input) {
    // basic box background
    TekGuiBoxData box_data = {};
    tekGuiGetTextInputData(text_input, &box_data);
    tekChainThrow(tekGuiUpdateBox(&box_data, text_input->mesh_index));
    return SUCCESS;
}

/**
 * Allocate a buffer of the visible text being displayed.
 * @param text_input The text to get a pointer of.
 * @param buffer A pointer to where the buffer should be allocated.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekGuiTextInputGetTextPtr(const TekGuiTextInput* text_input, char** buffer) {
    // mallocate sum memory
    const uint length = tekGuiGetTextInputTextLength(text_input);
    *buffer = (char*)malloc(length * sizeof(char));
    if (!*buffer)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for text buffer.");

    // if its possible that some text is cut off, then dont copy it all
    if (text_input->text_max_length != -1) {
        memcpy(*buffer, (char*)text_input->text.internal + text_input->text_start_index, length);
        (*buffer)[length - 1] = 0;
        return SUCCESS;
    }

    // if text not that long yet, then copy it straight over
    memcpy(*buffer, text_input->text.internal, length);
    return SUCCESS;
}

/**
 * Create the text mesh to be displayed to the user.
 * @param text_input The text input to create the text for.
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiTextInputCreateText(TekGuiTextInput* text_input) {
    char* text = 0;

    // create the text
    tekChainThrow(tekGuiTextInputGetTextPtr(text_input, &text));
    tekChainThrow(tekCreateText(text, text_input->text_height, &monospace_font, &text_input->tek_text));

    // if too long, recreate with less characters
    const float max_width = (float)text_input->button.hitbox_width - 2 * text_input->tek_text.height;
    if (text_input->text_max_length == -1 && text_input->tek_text.width >= max_width) {
        text_input->text_max_length = (int)text_input->text.length - 1;
        tekChainThrow(tekCreateText(text, text_input->text_height, &monospace_font, &text_input->tek_text));
    }

    // create version that includes the cursor
    const uint cursor_index = text_input->cursor_index - text_input->text_start_index;
    text[cursor_index] = CURSOR;
    tekChainThrow(tekCreateText(text, text_input->text_height, &monospace_font, &text_input->tek_text_cursor));

    free(text);
    return SUCCESS;
}

/**
 * Recreate the meshes needed to draw the text so that changes to the contents of the text input are reflected visually.
 * @param text_input The text input to recreate.
 * @throws OPENGL_EXCEPTION .
 */
static exception tekGuiTextInputRecreateText(TekGuiTextInput* text_input) {
    char* text = 0;

    // get text pointer.
    // points to where the text is started to be displayed.
    tekChainThrow(tekGuiTextInputGetTextPtr(text_input, &text));
    tekChainThrow(tekUpdateText(&text_input->tek_text, text, text_input->text_height));

    // check if the entire text can fit on or nah
    const float max_width = (float)text_input->button.hitbox_width - 2 * text_input->tek_text.height;
    if (text_input->text_max_length == -1 && text_input->tek_text.width >= max_width) {
        text_input->text_max_length = (int)text_input->text.length - 1;
        tekChainThrow(tekUpdateText(&text_input->tek_text, text, text_input->text_height));
    }

    // create another version of the text that includes the cursor icon.
    const uint cursor_index = text_input->cursor_index - text_input->text_start_index;
    text[cursor_index] = CURSOR;
    tekChainThrow(tekUpdateText(&text_input->tek_text_cursor, text, text_input->text_height));

    free(text);
    return SUCCESS;
}

/**
 * Add a character to the text input at the cursor position.
 * @param text_input The text input to add the character to.
 * @param codepoint The character to add to the text input.
 * @throws VECTOR_EXCEPTION .
 */
static exception tekGuiTextInputAdd(TekGuiTextInput* text_input, const char codepoint) {
    // add the character
    tekChainThrow(vectorInsertItem(&text_input->text, text_input->cursor_index, &codepoint));

    // move the cursor forwards
    tekGuiTextInputIncrementCursor(text_input);
    return SUCCESS;
}

/**
 * Remove a character from a text input gui.
 * @param text_input The text input to remove a character from.
 * @throws VECTOR_EXCEPTION .
 */
static exception tekGuiTextInputRemove(TekGuiTextInput* text_input) {
    if (text_input->text.length <= 1) return SUCCESS;
    if (text_input->cursor_index <= 0) return SUCCESS; // if cursor reached the start, cannot remove now

    // remove character before cursor index
    tekChainThrow(vectorRemoveItem(&text_input->text, text_input->cursor_index - 1, NULL));

    // shift cursor back
    if (text_input->cursor_index > 0) {
        text_input->cursor_index--;
        // also potentially shift the entire text back
        if (text_input->text_start_index > 0)
            text_input->text_start_index--;
    }
    return SUCCESS;
}

/**
 * Callback for listening for actual typed text, so caps lock gives capital letters and such.
 * @param codepoint The codepoint of the last typed character.
 */
static void tekGuiTextInputCharCallback(const uint codepoint) {
    if (!selected_input) return; // if no input selected, then ignore
    tekGuiTextInputAdd(selected_input, (char)codepoint); // otherwise write character to the selected input.
    tekGuiTextInputRecreateText(selected_input);
}

/**
 * Finish inputting on a text input, reset the text to initial position.
 * @param text_input The text input to finish inputting to. 
 * @throws EXCEPTION that the further callbacks could call.
 */
static exception tekGuiFinishTextInput(TekGuiTextInput* text_input) {
    // reset cursor position
    selected_input = 0;
    text_input->cursor_index = 0;
    text_input->text_start_index = 0;

    if (text_input->callback) // if there is a callback, call it
        tekChainThrow(text_input->callback(text_input, text_input->text.internal, text_input->text.length));

    // redraw text from new starting position
    tekChainThrow(tekGuiTextInputRecreateText(text_input));
    return SUCCESS;
}

/**
 * Callback for when a key is pressed to affect text inputs. Listens for enter key, arrow keys etc.
 * @param key The key code of the key being pressed.
 * @param scancode The scancode of the key.
 * @param action The action of the key e.g. press, release
 * @param mods The modifiers acting on the key.
 */
static void tekGuiTextInputKeyCallback(const int key, const int scancode, const int action, const int mods) {
    if (!selected_input) return;
    if (action != GLFW_RELEASE && action != GLFW_REPEAT) return;

    TekGuiTextInput* text_input = selected_input;

    switch (key) {
    case GLFW_KEY_ENTER:
        // on enter, finish writing
        tekGuiFinishTextInput(selected_input);
        break;
    case GLFW_KEY_BACKSPACE:
        // on backspace, delete a character
        tekGuiTextInputRemove(selected_input);
        break;
    case GLFW_KEY_LEFT:
        // on key left, move the cursor backwards
        tekGuiTextInputDecrementCursor(selected_input);
        break;
    case GLFW_KEY_RIGHT:
        // on key right, move the cursor forwards
        tekGuiTextInputIncrementCursor(selected_input);
        break;
    default:
        return;
    }

    tekGuiTextInputRecreateText(text_input);
}

/**
 * Callback for when the text input is clicked.
 * @param button The internal button of the text input.
 * @param callback_data The callback data e.g. mouse position. 
 */
static void tekGuiTextInputButtonCallback(TekGuiButton* button, TekGuiButtonCallbackData callback_data) {
    TekGuiTextInput* text_input = (TekGuiTextInput*)button->data; // get the text input stored in button data

    switch (callback_data.type) {
    case TEK_GUI_BUTTON_MOUSE_BUTTON_CALLBACK:
        // if not clicking then ignore
        if (callback_data.data.mouse_button.button != GLFW_MOUSE_BUTTON_LEFT || callback_data.data.mouse_button.action != GLFW_RELEASE)
            break;
        if (text_input == selected_input) // if this input is already selected, then dont re select it
            break;
        if (selected_input)
            tekGuiFinishTextInput(selected_input);
        selected_input = text_input;
        break;
    default:
        break;
    }
}

/**
 * Load text input related things that require opengl. Just loads fonts presently.
 * @throws FREETYPE_EXCEPTION if couldn't load the font.
 */
static exception tekGuiTextInputGLLoad() {
    // commenting every function !!
    tekChainThrow(tekCreateBitmapFont("../res/inconsolata.ttf", 0, 64, &monospace_font));
    return SUCCESS;
}

/**
 * Initialise some stuff for text inputs, just callbacks for keys and typing and loading.
 */
tek_init tekGuiTextInputInit() {
    // user interactivity
    tekAddCharCallback(tekGuiTextInputCharCallback);
    tekAddKeyCallback(tekGuiTextInputKeyCallback);

    // graphical stuff
    tekAddGLLoadFunc(tekGuiTextInputGLLoad);
}

/**
 * Create a new text input gui element.
 * @param text_input A pointer to an existing but empty text input to be filled with data. 
 * @throws OPENGL_EXCEPTION .
 */
exception tekGuiCreateTextInput(TekGuiTextInput* text_input) {
    // get defaults from the defaults file.
    struct TekGuiTextInputDefaults defaults = {};
    tekChainThrow(tekGuiGetTextInputDefaults(&defaults));

    // create the internal button for checking when selected by mouse
    tekChainThrow(tekGuiCreateButton(&text_input->button));
    tekGuiSetButtonPosition(&text_input->button, (int)defaults.x_pos, (int)defaults.y_pos);
    tekGuiSetButtonSize(&text_input->button, defaults.width, defaults.text_height * 5 / 4);
    text_input->button.callback = tekGuiTextInputButtonCallback;
    text_input->button.data = (void*)text_input;
    tekChainThrow(tekGuiTextInputAddGLMesh(text_input, &text_input->mesh_index));

    // create buffer for storing text
    tekChainThrow(vectorCreate(16, sizeof(char), &text_input->text));
    const char zero = 0;
    tekChainThrow(vectorAddItem(&text_input->text, &zero));
    tekChainThrow(vectorAddItem(&text_input->text, &zero));
    text_input->text_height = defaults.text_height;
    text_input->cursor_index = 0;
    text_input->text_max_length = -1;
    text_input->text_start_index = 0;
    tekChainThrow(tekGuiTextInputCreateText(text_input));

    // copy in defaults
    text_input->border_width = defaults.border_width;
    glm_vec4_copy(defaults.background_colour, text_input->background_colour);
    glm_vec4_copy(defaults.border_colour, text_input->border_colour);
    glm_vec4_copy(defaults.text_colour, text_input->text_colour);

    return SUCCESS;
}

/**
 * Draw a text input gui element to the screen.
 * @param text_input The text input to draw
 * @throws OPENGL_EXCEPTION .
 */
exception tekGuiDrawTextInput(const TekGuiTextInput* text_input) {
    // draw the outer box
    tekChainThrow(tekGuiDrawBox(text_input->mesh_index, text_input->background_colour, text_input->border_colour));
    // text positionm, slight offset from left edge
    const float x = (float)(text_input->button.hitbox_x + (int)(text_input->text_height / 2));
    const float y = (float)text_input->button.hitbox_y;

    // flickering cursor
    // basically get the current time
    // if in the first half of a second, show the cursor, otherwise dont show it
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

    return SUCCESS;
}

/**
 * Update the position of a text input gui element.
 * @param text_input The text input to update the position of.
 * @param x_pos The new x position of the text input.
 * @param y_pos The new y position of the text input.
 * @throws MEMORY_EXCEPTION .
 */
exception tekGuiSetTextInputPosition(TekGuiTextInput* text_input, int x_pos, int y_pos) {
    tekGuiSetButtonPosition(&text_input->button, x_pos, y_pos); // update internal button
    tekChainThrow(tekGuiTextInputUpdateGLMesh(text_input)); // update visually.

    return SUCCESS;
}

/**
 * Set the width and height of a text input gui element in pixels.
 * @param text_input The text input to change the size of.
 * @param width The new width of the text input.
 * @param height The new height of the text input.
 * @throws MEMORY_EXCEPTION .
 */
exception tekGuiSetTextInputSize(TekGuiTextInput* text_input, uint width, uint height) {
    tekGuiSetButtonSize(&text_input->button, width, height); // update internal button
    tekChainThrow(tekGuiTextInputUpdateGLMesh(text_input)); // update visuals

    return SUCCESS;
}

/**
 * Set the text being displayed by a text input.
 * @param text_input The text input to update with new text.
 * @param text The new text to be stored in the input.
 * @throws VECTOR_EXCEPTION .
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekGuiSetTextInputText(TekGuiTextInput* text_input, const char* text) {
    // clear out the old input and add two null characters at the end.
    // first null character is where the '|' will flicker
    // probably a better way to do it but idc
    text_input->cursor_index = 0;
    text_input->text_start_index = 0;
    vectorClear(&text_input->text);

    const char zero = 0;
    for (uint i = 0; i < 2; i++)
        tekChainThrow(vectorAddItem(&text_input->text, &zero));

    // if text is null, assume an empty string is wanted.
    if (text) {
        const uint len_text = strlen(text);
        for (uint i = 0; i < len_text; i++) {
            tekChainThrow(tekGuiTextInputAdd(text_input, text[i]));
            if (text_input->text_max_length == -1)
                tekChainThrow(tekGuiTextInputRecreateText(text_input));
        }
    }

    if (text_input != selected_input) {
        text_input->cursor_index = 0;
        text_input->text_start_index = 0;
    }

    tekChainThrow(tekGuiTextInputRecreateText(text_input));

    return SUCCESS;
}

/**
 * Delete a text input gui element, freeing any allocated memory.
 * @param text_input The text input to delete.
 */
void tekGuiDeleteTextInput(TekGuiTextInput* text_input) {
    tekGuiDeleteButton(&text_input->button); // delete internal button
    vectorDelete(&text_input->text); // delete written text.
}
